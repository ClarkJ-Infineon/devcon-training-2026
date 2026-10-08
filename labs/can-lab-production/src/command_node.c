/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * PSOC Control CAN Command & Telemetry lab - Board A "Command node"
 * (full reference solution).
 *
 * Reads the onboard potentiometer, derives a 0-255 setpoint, drives the
 * local brightness output (user LED on P8.4 plus the PWM pin P5.0), and
 * transmits the setpoint over CAN so the paired "Telemetry node" board can
 * mirror it.
 *
 * PRODUCTION TIER. This is not one of the three difficulty tiers and has no
 * TODOs - nobody writes this during the hour. It is how the same application
 * would be written for real. Compared with can-lab-cheat/ it adds:
 *
 *   - return-code checks on every driver call, not just the ones that catch
 *     a likely classroom mistake;
 *   - detection of a second command node on the bus (see below), the most
 *     common and most deceptive way the paired lab is mis-flashed;
 *   - richer diagnostics on a failed send (controller state and error
 *     counters, not just the errno).
 *
 * Show this version to customers. The three attendee tiers are deliberately
 * simplified so they can be read and typed inside a 60-minute session.
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/adc.h>
#include <zephyr/drivers/can.h>
#include <zephyr/sys/atomic.h>
#include <zephyr/logging/log.h>

#include "can_protocol.h"
#include "lab_led.h"

LOG_MODULE_REGISTER(command_node, LOG_LEVEL_INF);

#define SAMPLE_PERIOD_MS 50
#define CAN_BITRATE      500000

/*
 * Bus-state indicator on LED4 (P8.5, alias led1).
 *
 * Lit means the controller is error-active, which is the healthy state. It
 * goes dark the moment the controller drops to error-passive or bus-off -
 * which is what a node alone on the bus, or one whose bitrate disagrees with
 * its partner, does within a second or two. The lab's central failure mode,
 * made physical, without needing the console.
 *
 * This works here because the command node transmits: unacknowledged frames
 * drive the transmit error counter up. The telemetry node cannot use the same
 * signal and blinks on reception instead - see telemetry_node.c.
 */
static const struct gpio_dt_spec bus_led = GPIO_DT_SPEC_GET(DT_ALIAS(led1), gpios);

static void bus_state_cb(const struct device *dev, enum can_state state,
			 struct can_bus_err_cnt err_cnt, void *user_data)
{
	ARG_UNUSED(dev);
	ARG_UNUSED(err_cnt);
	ARG_UNUSED(user_data);

	/* Runs in driver context, so it does nothing but set a pin. */
	gpio_pin_set_dt(&bus_led, state == CAN_STATE_ERROR_ACTIVE ? 1 : 0);
}

static const struct adc_dt_spec pot_adc = ADC_DT_SPEC_GET(DT_PATH(zephyr_user));

static const struct device *const can_dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_canbus));

/*
 * Duplicate-command-node detection.
 *
 * The most common way this lab goes wrong is both partners flashing the
 * command role. It is electrically harmless - CAN is multi-master, and every
 * node ACKs any valid frame it receives regardless of its own filters, so the
 * two boards acknowledge each other and the bus stays error-free. That is
 * exactly what makes it dangerous: each board's LED follows its own
 * potentiometer, so it *looks* like the lab is working while no data is
 * actually crossing the wire.
 *
 * In CAN_MODE_NORMAL a controller does not receive its own transmissions, so
 * a setpoint frame arriving at a command node can only have come from a
 * second command node. That makes this check exact - no false positives.
 */
static atomic_t peer_setpoint_frames;

static void peer_setpoint_cb(const struct device *dev, struct can_frame *frame,
			     void *user_data)
{
	ARG_UNUSED(dev);
	ARG_UNUSED(frame);
	ARG_UNUSED(user_data);

	atomic_inc(&peer_setpoint_frames);
}

static void warn_if_duplicate_command_node(void)
{
	static bool warned;

	if (warned || atomic_get(&peer_setpoint_frames) == 0) {
		return;
	}

	warned = true;
	LOG_ERR("Another node is transmitting setpoints on ID 0x%x - both "
		"boards look like they are flashed as the COMMAND node.",
		CAN_ID_SETPOINT);
	LOG_ERR("Re-flash one board with conf/role_telemetry.conf. Until then "
		"each LED only follows its own knob.");
}

static uint8_t read_setpoint(void)
{
	int16_t sample = 0;
	struct adc_sequence seq = {
		.buffer = &sample,
		.buffer_size = sizeof(sample),
	};

	(void)adc_sequence_init_dt(&pot_adc, &seq);
	if (adc_read_dt(&pot_adc, &seq) != 0) {
		return 128;
	}

	if (sample < 0) {
		sample = 0;
	}

	/* 12-bit sample (0-4095) -> 8-bit setpoint (0-255). */
	return (uint8_t)(sample >> 4);
}

/*
 * Handed to can_send() so that transmission is fire-and-forget. Runs in
 * driver context, so it does nothing but bump an atomic counter.
 *
 * Passing a callback rather than NULL is not optional. With a NULL
 * callback Zephyr waits K_FOREVER for the frame to be acknowledged (the
 * `timeout` argument only bounds the wait for a free TX mailbox), and a
 * node alone on the bus is never acknowledged - the controller settles at
 * error-passive rather than bus-off and retransmits indefinitely, so the
 * application deadlocks on its first send.
 */
static atomic_t tx_failures;

static void tx_done_cb(const struct device *dev, int error, void *user_data)
{
	ARG_UNUSED(dev);
	ARG_UNUSED(user_data);

	if (error != 0) {
		atomic_inc(&tx_failures);
	}
}

static void send_setpoint(uint8_t setpoint)
{
	struct can_frame frame = {
		.id = CAN_ID_SETPOINT,
		.dlc = CAN_DLC_SETPOINT,
		.data = {setpoint},
	};

	int err = can_send(can_dev, &frame, K_NO_WAIT, tx_done_cb, NULL);
	static uint32_t fails;

	if (err != 0) {
		/* Alone on the bus, nothing acknowledges our frames, so the
		 * controller retransmits them forever and all TX mailboxes
		 * stay occupied - K_NO_WAIT then returns -EAGAIN immediately
		 * rather than stalling the control loop. Warn on the first
		 * failure and every 5 s after, so the setpoint trace stays
		 * readable at 20 sends per second.
		 */
		if ((fails++ % (5000U / SAMPLE_PERIOD_MS)) == 0U) {
			enum can_state state = CAN_STATE_ERROR_ACTIVE;
			struct can_bus_err_cnt err_cnt = {0};

			(void)can_get_state(can_dev, &state, &err_cnt);

			LOG_WRN("CAN send failed (%d) state=%d tx_err=%u "
				"rx_err=%u (%u dropped, %ld aborted) - is the "
				"second board powered and wired?", err,
				(int)state, err_cnt.tx_err_cnt,
				err_cnt.rx_err_cnt, fails,
				(long)atomic_get(&tx_failures));
		}
	} else if (fails != 0U) {
		LOG_INF("CAN send recovered after %u failures - partner board "
			"is on the bus", fails);
		fails = 0U;
	}
}

void run_command_node(void)
{
	LOG_INF("Command node starting (PSOC Control CAN lab)");
	LOG_INF("can-lab-production build stamp: " __DATE__ " " __TIME__);

	if (!device_is_ready(can_dev)) {
		LOG_ERR("CAN device not ready");
		return;
	}

	int err = can_set_bitrate(can_dev, CAN_BITRATE);

	if (err != 0) {
		LOG_ERR("can_set_bitrate() failed (%d)", err);
		return;
	}

	err = can_set_mode(can_dev, CAN_MODE_NORMAL);
	if (err != 0) {
		LOG_ERR("can_set_mode() failed (%d)", err);
		return;
	}

	/* Must be registered before the controller starts, so that the very
	 * first transition out of error-active is reported.
	 */
	gpio_pin_configure_dt(&bus_led, GPIO_OUTPUT_INACTIVE);
	can_set_state_change_callback(can_dev, bus_state_cb, NULL);

	err = can_start(can_dev);
	if (err != 0) {
		LOG_ERR("can_start() failed (%d)", err);
		return;
	}

	/* The callback only fires on a transition, and starting on a healthy
	 * bus is not one, so seed the LED from the current state.
	 */
	{
		enum can_state state = CAN_STATE_ERROR_ACTIVE;

		(void)can_get_state(can_dev, &state, NULL);
		gpio_pin_set_dt(&bus_led, state == CAN_STATE_ERROR_ACTIVE ? 1 : 0);
	}
	/* Listen for setpoint frames we did not send - see the comment on
	 * peer_setpoint_frames above. Non-fatal: a full filter bank costs us
	 * the diagnostic, not the lab.
	 */
	const struct can_filter setpoint_filter = {
		.id = CAN_ID_SETPOINT,
		.mask = CAN_STD_ID_MASK,
	};

	err = can_add_rx_filter(can_dev, peer_setpoint_cb, NULL, &setpoint_filter);
	if (err < 0) {
		LOG_WRN("Could not register the duplicate-node filter (%d); "
			"continuing without that check", err);
	}

	lab_led_init();
	adc_channel_setup_dt(&pot_adc);

	while (1) {
		uint8_t setpoint = read_setpoint();

		/* Print the setpoint about twice a second so the knob position
		 * is visible on the console as well as on the LED.
		 */
		static uint32_t tick;

		if ((tick++ % (500U / SAMPLE_PERIOD_MS)) == 0U) {
			LOG_INF("setpoint %3u (%u%%)", setpoint,
				(setpoint * 100U) / 255U);
		}

		/* Local brightness feedback: the user LED on P8.4 plus the
		 * hardware PWM pin P5.0 on header J21 pin 11, both driven
		 * by the helper.
		 */
		lab_led_set_duty(setpoint);

		send_setpoint(setpoint);

		warn_if_duplicate_command_node();

		k_msleep(SAMPLE_PERIOD_MS);
	}
}
