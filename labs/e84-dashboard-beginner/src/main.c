/*
 * Copyright (c) 2026 Infineon Technologies AG,
 * or an affiliate of Infineon Technologies AG.
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * DevCon 2026 PSOC Edge lab - Device Dashboard, cheat tier.
 *
 * Three on-board LEDs dimmed over hardware PWM from LVGL sliders, plus a live
 * tilt readout from the on-board BMI270. Nothing on this screen is simulated:
 * touch drives real hardware, and the meter follows a real sensor.
 *
 * The three lab touchpoints all meet here:
 *   - devicetree: the pwm-led0..2 aliases and accel0 resolved below only
 *     exist because of boards/kit_pse84_eval_pse846gps2dbzc4a_m55.overlay
 *   - modules:    LVGL arrives as a west module; tilt_filter is a local one
 *   - code:       the PWM, sensor and LVGL API calls in this file
 */

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/display.h>
#include <zephyr/drivers/pwm.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/kernel.h>

#include <lvgl.h>
#include <math.h>
#include <stdio.h>

#ifdef CONFIG_TILT_FILTER
#include <tilt_filter.h>
#endif
#ifdef CONFIG_BEEPER
#include <beeper.h>
#endif

#include "ui.h"

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(e84_dashboard);

/*
 * This guard exists only in the lab skeletons.
 *
 * Without it, an unfinished Step 1 produces around a hundred lines of
 * devicetree macro expansion errors - all of them real, none of them
 * saying what is actually wrong. The #error below says it in one line,
 * and the dummy definition in this branch keeps the rest of the file
 * quiet so that line is the only thing you have to read.
 */
#if !DT_NODE_EXISTS(DT_ALIAS(pwm_led0))

#error "Step 1 is not finished. The pwm-led0 alias does not exist yet, \
so there is no PWM channel for this file to reference. Go back to \
boards/kit_pse84_eval_pse846gps2dbzc4a_m55.overlay and complete TODO 1a \
and TODO 1b, then build again."

static const struct pwm_dt_spec led_pwm[UI_LED_COUNT];

#else

/*
 * Second half of the same guard, and a reminder that devicetree alone is
 * never enough. The aliases above now exist, but a node being enabled in
 * devicetree does not build the driver that binds to it - that is what
 * CONFIG_PWM does. Without it this file compiles perfectly well and then
 * fails at link time with an undefined reference to a generated device
 * symbol, which names no file and no line.
 */
#if !defined(CONFIG_PWM)
#error "Step 1 is not finished. The LED nodes are enabled in devicetree \
now, but CONFIG_PWM is still off, so the PWM driver is not being built \
and nothing can bind to those nodes. Complete TODO 1c in prj.conf, then \
build again."
#endif

/*
 * PWM_DT_SPEC_GET() captures the device pointer, channel, period and flags
 * from the devicetree at build time. If one of these aliases is missing the
 * failure is a compile error, not a runtime surprise - which is exactly what
 * is wanted for something attendees are editing by hand.
 */
static const struct pwm_dt_spec led_pwm[UI_LED_COUNT] = {
	PWM_DT_SPEC_GET(DT_ALIAS(pwm_led0)),
	PWM_DT_SPEC_GET(DT_ALIAS(pwm_led1)),
	PWM_DT_SPEC_GET(DT_ALIAS(pwm_led2)),
};

#endif

static const struct device *const imu = DEVICE_DT_GET(DT_ALIAS(accel0));

/*
 * How often LVGL is serviced, and how often the IMU is read. These used to be
 * the same thing: every pass repainted the whole screen, so the display
 * controller set the pace and the sensor was simply read once per frame. Now
 * that the UI only repaints when a value changes, an idle pass costs
 * microseconds, and the two rates are free to differ.
 */
#define UI_TICK_MS       10
#define IMU_PERIOD_MS    50
#define REPORT_PERIOD_MS 5000U

/* Starting brightness per channel - chosen so the panel looks alive at boot. */
static const uint8_t initial_percent[UI_LED_COUNT] = { 70, 45, 20 };

#ifdef CONFIG_TILT_FILTER
static struct tilt_filter pitch_filter;
static struct tilt_filter roll_filter;
#endif

/*
 * Resting orientation, captured at startup and subtracted from every reading.
 *
 * Without this the meter reads whatever angle the EVK happens to be sitting
 * at - on this desk, -52 degrees of roll, because the board does not lie flat
 * once the panel is mounted on its stand-offs. Every attendee's bench is
 * different, so an absolute reading makes the demo behave differently on
 * every table. Zeroing at boot makes "flat" mean "however you left it", which
 * is what the attendee expects when they pick the board up and tip it.
 *
 * Subtracting the resting pitch and roll is not enough, though, and the
 * difference is visible rather than academic. Pitch is measured against true
 * vertical, so once the board already leans, a given physical rotation lands
 * only partly in the pitch plane: at this board's ~54 degrees of resting
 * roll, tipping it 20 degrees moves the reading about 10.5, and drags roll
 * 1.6 degrees along with it. The meter under-reads by roughly 40% and the
 * two axes bleed into each other.
 *
 * Angles do not compose by subtraction. Rotations do. So capture the resting
 * gravity vector instead of the resting angles, build the rotation that takes
 * it to +Z, and apply that to every later sample before deriving the angles.
 * The attendee then gets full sensitivity and independent axes whatever angle
 * the board is propped at, which matters because in a room full of EVKs no
 * two will be propped alike.
 */
static float ref_rot[3][3] = {
	{ 1.0f, 0.0f, 0.0f },
	{ 0.0f, 1.0f, 0.0f },
	{ 0.0f, 0.0f, 1.0f },
};

static int led_set_percent(uint8_t index, uint8_t percent)
{
	/*
	 * TODO 1d: drive this LED at `percent` brightness.
	 *
	 * led_pwm[index] is a struct pwm_dt_spec, filled in from the overlay
	 * you edited in Step 1. It carries the device, the channel, the period
	 * devicetree declared, and the polarity flags.
	 *
	 * Use pwm_set_pulse_dt(). It takes the spec and a pulse width in the
	 * same units as the period, so a percentage means scaling the period
	 * from the spec rather than hardcoding a number here - the overlay is
	 * allowed to change the period and this should still be correct.
	 *
	 * Watch the arithmetic: period * 100 overflows a uint32_t for any
	 * period longer than about 43 ms.
	 *
	 * Return what pwm_set_pulse_dt() returns; the caller logs failures.
	 *
	 * https://docs.zephyrproject.org/latest/hardware/peripherals/pwm.html
	 */
	ARG_UNUSED(index);
	ARG_UNUSED(percent);

	return 0; /* Replace this. */
	/*
	 * TODO 1d - ANSWER.
	 *
	 * Copy everything between the #if 0 and the #endif, paste it
	 * here, then delete this whole block.
	 * Then delete the two ARG_UNUSED lines and the `return 0;` just above,
	 * which only exists so the skeleton compiles.
	 */
#if 0 /* ==== copy from the next line ==== */

	/*
	 * Pulse width is a fraction of the period that devicetree declared, so
	 * the arithmetic stays correct if the period is later changed in the
	 * overlay. uint64_t because period_ns * 100 overflows 32 bits for any
	 * period above ~43 ms.
	 */
	uint32_t pulse = (uint32_t)(((uint64_t)led_pwm[index].period * percent) / 100U);

	return pwm_set_pulse_dt(&led_pwm[index], pulse);

#endif /* ==== stop copying at the line above ==== */

}

static void on_brightness(uint8_t index, uint8_t percent)
{
	int ret = led_set_percent(index, percent);

	if (ret < 0) {
		LOG_ERR("LED%u set %u%% failed (%d)", index, percent, ret);
	}
}

/*
 * Convert a gravity vector into pitch/roll in millidegrees.
 *
 * With the board flat, gravity sits almost entirely on Z. Tipping it moves
 * that component onto X or Y, and the arctangent of the ratio is the angle.
 * atan2f() rather than atanf() so the sign of both operands is respected and
 * the result covers the full +/-180 range instead of folding at 90.
 *
 * Which axis is which is a property of how the BMI270 is placed on this PCB,
 * not of how the board sits on the desk, and on this board it is not the
 * mapping you would guess:
 *
 *   sensor Y -> runs up and down the panel -> tipping the board forward and
 *               back moves gravity along it, so Y gives PITCH
 *   sensor X -> runs left and right across the panel -> tipping the board
 *               left and right moves gravity along it, so X gives ROLL
 *
 * The first version of this function had those two swapped, which is only
 * detectable by picking the board up and tilting it - every static reading
 * looks perfectly plausible. Signs are chosen so that tilting the far edge
 * up reads positive pitch and dropping the right side reads positive roll,
 * which is what someone holding the board expects to see.
 */
static void accel_to_angles(const float v[3], int32_t *pitch_mdeg, int32_t *roll_mdeg)
{
	const float rad_to_mdeg = 57295.779513f; /* (180/pi) * 1000 */

	*pitch_mdeg = (int32_t)(atan2f(-v[1], v[2]) * rad_to_mdeg);
	*roll_mdeg = (int32_t)(atan2f(v[0], sqrtf(v[1] * v[1] + v[2] * v[2])) * rad_to_mdeg);
}

/*
 * Build the rotation that takes the measured resting gravity vector onto +Z,
 * i.e. that makes "however the board is propped right now" mean level.
 *
 * This is Rodrigues' rotation formula for the specific case of rotating a
 * unit vector a onto b = (0,0,1). The axis is v = a x b = (ay, -ax, 0) and
 * the cosine of the angle is c = a.z, which collapses the general form
 * R = I + K + K^2/(1+c) into the closed form below.
 *
 * The 1/(1+c) term is singular when c = -1, i.e. the board was face-down at
 * boot. There is no unique rotation in that case (any axis in the XY plane
 * works), so pick one: a half turn about X.
 */
static void build_ref_rot(float ax, float ay, float az)
{
	float k;

	if (az < -0.999f) {
		ref_rot[0][0] = 1.0f;  ref_rot[0][1] = 0.0f;  ref_rot[0][2] = 0.0f;
		ref_rot[1][0] = 0.0f;  ref_rot[1][1] = -1.0f; ref_rot[1][2] = 0.0f;
		ref_rot[2][0] = 0.0f;  ref_rot[2][1] = 0.0f;  ref_rot[2][2] = -1.0f;
		return;
	}

	k = 1.0f / (1.0f + az);

	ref_rot[0][0] = 1.0f - ax * ax * k;
	ref_rot[0][1] = -ax * ay * k;
	ref_rot[0][2] = -ax;

	ref_rot[1][0] = -ax * ay * k;
	ref_rot[1][1] = 1.0f - ay * ay * k;
	ref_rot[1][2] = -ay;

	ref_rot[2][0] = ax;
	ref_rot[2][1] = ay;
	ref_rot[2][2] = 1.0f - (ax * ax + ay * ay) * k;
}

static void apply_ref_rot(const float in[3], float out[3])
{
	for (int r = 0; r < 3; r++) {
		out[r] = ref_rot[r][0] * in[0] + ref_rot[r][1] * in[1] +
			 ref_rot[r][2] * in[2];
	}
}

/*
 * Bring the accelerometer out of suspend.
 *
 * The BMI270 powers up with both the accelerometer and gyroscope disabled.
 * device_is_ready() still returns true and sensor_sample_fetch() still returns
 * 0 in that state - the driver reads the data registers successfully, they
 * just contain zeroes. That is the worst kind of failure to debug from a UI,
 * because a perfectly level reading is exactly what a board sitting on a desk
 * is supposed to produce.
 *
 * Sampling frequency must be set *last*: in this driver it is the write that
 * also selects the power mode, so anything configured after it is applied to a
 * running sensor.
 *
 * Only the accelerometer is configured. The dashboard derives tilt from
 * gravity alone, and leaving the gyroscope suspended saves a few hundred uA.
 */
static int imu_configure(void)
{
	struct sensor_value full_scale = { .val1 = 2, .val2 = 0 };   /* +/- 2 g */
	struct sensor_value oversampling = { .val1 = 1, .val2 = 0 }; /* normal mode */
	struct sensor_value sampling_freq = { .val1 = 100, .val2 = 0 }; /* Hz */
	int ret;

	ret = sensor_attr_set(imu, SENSOR_CHAN_ACCEL_XYZ, SENSOR_ATTR_FULL_SCALE, &full_scale);
	if (ret == 0) {
		ret = sensor_attr_set(imu, SENSOR_CHAN_ACCEL_XYZ, SENSOR_ATTR_OVERSAMPLING,
				      &oversampling);
	}
	if (ret == 0) {
		ret = sensor_attr_set(imu, SENSOR_CHAN_ACCEL_XYZ,
				      SENSOR_ATTR_SAMPLING_FREQUENCY, &sampling_freq);
	}

	if (ret != 0) {
		LOG_ERR("IMU configuration failed (%d)", ret);
	}

	return ret;
}

/* Read one raw gravity sample into g units. Returns 0 on success. */
static int imu_read_accel(float v[3])
{
	struct sensor_value accel[3];
	int ret;

	/*
	 * SENSOR_CHAN_ALL, not SENSOR_CHAN_ACCEL_XYZ. The API allows a driver
	 * to support per-channel fetches, but bmi270_sample_fetch() reads
	 * accel, gyro and temperature in one 12-byte burst and rejects any
	 * narrower request with -ENOTSUP. Asking for the narrower channel here
	 * fails every single poll.
	 *
	 * The *read* below is per-channel - that part the driver does support.
	 * Only the fetch is all-or-nothing.
	 */
	ret = sensor_sample_fetch(imu);
	if (ret != 0) {
		return ret;
	}

	ret = sensor_channel_get(imu, SENSOR_CHAN_ACCEL_XYZ, accel);
	if (ret != 0) {
		return ret;
	}

	for (int i = 0; i < 3; i++) {
		v[i] = (float)sensor_value_to_double(&accel[i]);
	}

	return 0;
}

/* Read one sample and convert it. Returns 0 on success. */
static int imu_read_angles(int32_t *pitch_mdeg, int32_t *roll_mdeg)
{
	float raw[3];
	float level[3];
	int ret = imu_read_accel(raw);

	if (ret != 0) {
		return ret;
	}

	/*
	 * Rotate into the frame captured at boot before deriving angles, so
	 * the readings are measured from where the board was resting rather
	 * than from true horizontal.
	 */
	apply_ref_rot(raw, level);
	accel_to_angles(level, pitch_mdeg, roll_mdeg);

	return 0;
}

/*
 * Average a short burst of samples to establish the resting orientation.
 *
 * Note this averages the gravity *vector* and then derives a rotation from
 * it, rather than averaging the pitch and roll angles and subtracting them
 * later. Subtracting angles looks equivalent and is not: pitch is measured
 * against true vertical, so on a board already propped at 54 degrees a 20
 * degree tip only registers as about 10.5, and it disturbs the roll reading
 * on the way past. Rotating the frame keeps both axes at full sensitivity
 * and independent of each other, whatever angle the board is propped at -
 * which matters in a room where no two boards will be propped alike.
 */
static void imu_capture_zero(void)
{
	const int discard = 12;
	const int samples = 16;
	float sum[3] = { 0.0f, 0.0f, 0.0f };
	float norm;
	int taken = 0;

	/*
	 * Discard the first readings rather than averaging them in.
	 *
	 * The accelerometer does not deliver its final value the instant it
	 * leaves suspend - the output ramps towards the true angle over the
	 * first few hundred milliseconds. Averaging across that ramp captured a
	 * zero reference ~9.8 degrees away from the resting angle on this
	 * board, so the meter settled at a constant non-zero reading after
	 * boot, which looks exactly like a broken calibration.
	 */
	for (int i = 0; i < discard; i++) {
		float v[3];

		(void)imu_read_accel(v);
		k_sleep(K_MSEC(25));
	}

	for (int i = 0; i < samples; i++) {
		float v[3];

		if (imu_read_accel(v) == 0) {
			sum[0] += v[0];
			sum[1] += v[1];
			sum[2] += v[2];
			taken++;
		}
		k_sleep(K_MSEC(15));
	}

	if (taken == 0) {
		LOG_WRN("tilt zero reference: no samples, using board axes as level");
		return;
	}

	/*
	 * Normalise before building the rotation. Rodrigues' formula is only a
	 * rotation for a unit vector, and this one is nowhere near unit length
	 * anyway - sensor_value_to_double() returns m/s^2, so a board at rest
	 * reads about 9.81, not 1. Feeding that in unnormalised would quietly
	 * scale every later reading. The floor below is well under 1 g in
	 * either unit, so it only catches a dead or free-falling sensor.
	 */
	norm = sqrtf(sum[0] * sum[0] + sum[1] * sum[1] + sum[2] * sum[2]);
	if (norm < 0.1f) {
		LOG_WRN("tilt zero reference: |g| too small to trust, staying level");
		return;
	}

	build_ref_rot(sum[0] / norm, sum[1] / norm, sum[2] / norm);

	/* Printed as a unit vector in thousandths: 1000 is full gravity. */
	LOG_INF("tilt zero reference: g = (%d, %d, %d)/1000 over %d samples",
		(int)(sum[0] / norm * 1000.0f), (int)(sum[1] / norm * 1000.0f),
		(int)(sum[2] / norm * 1000.0f), taken);
}

int main(void)
{
	const struct device *display_dev;
	bool imu_ok;
	bool imu_warned = false;
	uint32_t tick = 0;
	uint32_t next_imu_ms = 0;
	uint32_t next_report_ms = REPORT_PERIOD_MS;
	int ret;

	/*
	 * Per-build stamp, logged before anything else runs. OpenOCD can report
	 * a successful program + verify on this chip family while leaving the
	 * previous image running, so a clean programmer log is not evidence
	 * that a flash landed. A compile timestamp can be cross-checked against
	 * the ELF's mtime, which makes it the one value that identifies the
	 * running image. Requires a pristine build (west build -p always).
	 */
	LOG_INF("e84-dashboard build stamp: " __DATE__ " " __TIME__);

	display_dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_display));
	if (!device_is_ready(display_dev)) {
		LOG_ERR("display device not ready");
		return 0;
	}

	for (uint8_t i = 0; i < UI_LED_COUNT; i++) {
		if (!pwm_is_ready_dt(&led_pwm[i])) {
			LOG_ERR("PWM channel %u not ready", i);
			return 0;
		}
		ret = led_set_percent(i, initial_percent[i]);
		if (ret < 0) {
			LOG_ERR("LED%u initial set failed (%d)", i, ret);
			return 0;
		}
	}
	LOG_INF("all %d PWM channels ready", UI_LED_COUNT);

	/*
	 * A missing IMU is not fatal. The LED half of the dashboard is the part
	 * attendees build, and losing the tilt meter to one flaky board should
	 * not cost someone the whole exercise - the status line says so instead.
	 */
	imu_ok = device_is_ready(imu);
	if (!imu_ok) {
		LOG_WRN("IMU %s not ready - tilt meter disabled", imu->name);
	} else {
		imu_ok = (imu_configure() == 0);
		if (imu_ok) {
			imu_capture_zero();
		}
	}

#ifdef CONFIG_TILT_FILTER
	tilt_filter_reset(&pitch_filter);
	tilt_filter_reset(&roll_filter);
#endif

	/*
	 * Audio is also non-fatal. It is the last thing brought up and the
	 * first thing to lose: a board with a codec that does not answer on
	 * i2c0 still runs the whole dashboard, just without the tick.
	 */
#ifdef CONFIG_BEEPER
	/*
	 * TODO 3b: bring the beeper module up.
	 *
	 * modules/beeper exposes beeper_init() - see its header. It returns a
	 * negative errno if the codec did not answer on i2c0.
	 *
	 * That must not be fatal. A board whose codec is unreachable should
	 * still run the whole dashboard, just without the tick, so log a
	 * warning and carry on rather than returning.
	 */
	/*
	 * TODO 3b - ANSWER.
	 *
	 * Copy everything between the #if 0 and the #endif, paste it
	 * here, then delete this whole block.
	 * Paste it directly below this block.
	 */
#if 0 /* ==== copy from the next line ==== */

	ret = beeper_init();
	if (ret < 0) {
		LOG_WRN("beeper init failed (%d) - touch feedback disabled", ret);
	}

#endif /* ==== stop copying at the line above ==== */

#endif

	ui_init(on_brightness, initial_percent);

	/*
	 * LVGL will not move a slider while the finger is down until it has
	 * travelled scroll_limit pixels from the press point - see the
	 * check_drag branch of update_knob_pos() in lv_slider.c. The gate is
	 * skipped on release, so below that distance a press looks completely
	 * dead and then snaps to the finger the moment it lifts.
	 *
	 * The limit exists so a slider does not swallow a scroll gesture that
	 * was meant for a scrollable parent. Nothing in this UI scrolls - the
	 * active screen and every container have LV_OBJ_FLAG_SCROLLABLE
	 * cleared - so the default 10 px buys nothing here and only costs
	 * responsiveness.
	 *
	 * Zero rather than one: at one, a finger held perfectly still gives an
	 * offset of zero, which is still below the limit, so the knob does not
	 * move until release. Zero removes the gate entirely and gives
	 * tap-to-set, which is what a brightness control should do. It is only
	 * safe because update_knob_pos() also gives up when the input device
	 * has latched onto a scroll object, and nothing here can become one.
	 *
	 * The default is a hardcoded #define in lv_indev.c with no Kconfig
	 * symbol behind it, so it has to be set at runtime.
	 */
	for (lv_indev_t *indev = lv_indev_get_next(NULL); indev != NULL;
	     indev = lv_indev_get_next(indev)) {
		if (lv_indev_get_type(indev) == LV_INDEV_TYPE_POINTER) {
			lv_indev_set_scroll_limit(indev, 0);
		}
	}

	/*
	 * The first flush is discarded by the infineon_dc driver, so it is
	 * issued before blanking is turned off to avoid showing a partial
	 * frame.
	 */
	lv_timer_handler();

	ret = display_blanking_off(display_dev);
	if (ret < 0 && ret != -ENOSYS) {
		LOG_ERR("blanking off failed (%d)", ret);
		return 0;
	}

	/*
	 * The Waveshare panel's backlight is driven over I2C (register 0x86),
	 * independent of the MIPI DSI video link. Without this the frames are
	 * correct but the screen stays dark.
	 */
	(void)display_set_brightness(display_dev, 255);

	ui_set_status(imu_ok ? "LVGL + hardware PWM + BMI270" : "IMU unavailable - LEDs only");

	while (1) {
		uint32_t now = k_uptime_get_32();

		/*
		 * The sensor and the UI run on separate cadences now that the
		 * screen only repaints on change. LVGL is serviced every
		 * UI_TICK_MS so a touch is picked up within one tick of the
		 * FocalTech driver reporting it, while the BMI270 keeps the
		 * much slower poll it actually needs - the tilt meter shows
		 * tenths of a degree, and reading it a hundred times a second
		 * would only add traffic to the I2C bus the display panel
		 * shares.
		 */
		if (imu_ok && (int32_t)(now - next_imu_ms) >= 0) {
			int32_t pitch, roll;

			next_imu_ms = now + IMU_PERIOD_MS;

			ret = imu_read_angles(&pitch, &roll);
			if (ret == 0) {
				/*
				 * No offset subtraction here: imu_read_angles()
				 * already rotated the sample into the frame
				 * captured at boot, so these are angles from
				 * rest.
				 */

				/*
				 * Raw readings jitter by a degree or two even
				 * with the board sitting still, which makes the
				 * meter twitch constantly. The local tilt_filter
				 * module smooths them; build with
				 * CONFIG_TILT_FILTER=n to see the difference.
				 */
#ifdef CONFIG_TILT_FILTER
				pitch = tilt_filter_update(&pitch_filter, pitch);
				roll = tilt_filter_update(&roll_filter, roll);
#endif
				ui_set_tilt(pitch, roll);

				/*
				 * Periodic readout. Not for the attendee's
				 * benefit - it is the only way to confirm the
				 * sensor path from a terminal when nobody is
				 * looking at the panel.
				 */
				if (++tick % 100 == 0) {
					LOG_INF("pitch %d mdeg  roll %d mdeg", pitch, roll);
				}
			} else if (!imu_warned) {
				/*
				 * Logged once, not every poll. In immediate log
				 * mode a per-iteration warning saturates the
				 * UART and starves the LVGL refresh, which makes
				 * a sensor problem look like a display problem.
				 */
				imu_warned = true;
				LOG_WRN("IMU read failed (%d) - tilt meter frozen", ret);
				ui_set_status("IMU read failed - LEDs still live");
			}
		}

		{
			/*
			 * TEMPORARY DIAGNOSTIC - not part of the lab.
			 *
			 * Counts how many of these ticks actually repainted.
			 * A repaint hands the display controller a different
			 * frame buffer, and the infineon_dc driver then blocks
			 * until the next frame-complete interrupt, so a real
			 * repaint costs tens of milliseconds while an idle
			 * tick costs tens of microseconds. The two are
			 * trivially separable by time.
			 */
			static uint32_t worst_us;
			static uint32_t repaints;
			uint32_t t0 = k_cycle_get_32();
			uint32_t dt;

			lv_timer_handler();

			dt = k_cyc_to_us_floor32(k_cycle_get_32() - t0);

			if (dt >= 20000U) {
				repaints++;
				worst_us = MAX(worst_us, dt);
			}

			if ((int32_t)(now - next_report_ms) >= 0) {
				next_report_ms = now + REPORT_PERIOD_MS;
				LOG_INF("repaints %u / %u ms (worst %u us)",
					repaints, REPORT_PERIOD_MS, worst_us);
				repaints = 0;
				worst_us = 0;
			}
		}

		k_sleep(K_MSEC(UI_TICK_MS));
	}

	return 0;
}