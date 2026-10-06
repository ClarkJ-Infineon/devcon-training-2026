/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * PSOC Control CAN Command & Telemetry lab - Board A "Command node".
 *
 * Reads the on-board potentiometer, turns it into a 0-255 setpoint, shows
 * that setpoint locally as LED brightness, and transmits it over CAN so the
 * paired "Telemetry node" board can mirror it.
 *
 * BEGINNER TIER. Every TODO below still explains
 * what the code does and why, but it now also carries the exact code to
 * write, in a block marked "copy from here". Copy it, paste it where the
 * block sits, and delete the block.
 *
 * You are still placing the code yourself and still have to get it in the
 * right place to build - what has been removed is having to recall the
 * Zephyr API signatures from memory.
 *
 * A note on style: this lab deliberately omits return-code checks on calls
 * that cannot realistically fail on a known-good board (can_set_bitrate(),
 * can_set_mode(), can_start()), so the code stays readable inside a
 * 60-minute session. The checks that are kept are the ones that catch real
 * mistakes: device_is_ready(), and the can_send() result - which is how you
 * find out the other board is not wired up yet. The reference solution
 * checks every call, and that is the pattern to use in production code.
 *
 * Where these APIs come from
 * --------------------------
 * Every driver call below is part of a published, versioned Zephyr API, and
 * it is worth knowing that the docs show you two different views of it:
 *
 *   What you call        The functions and structs your application uses -
 *                        can_send(), struct can_frame, and so on:
 *                        https://docs.zephyrproject.org/latest/doxygen/html/group__can__interface.html
 *
 *   What a driver        The table of function pointers each vendor driver
 *   implements           fills in behind those calls:
 *                        https://docs.zephyrproject.org/latest/doxygen/html/structcan__driver__api.html
 *
 * That split is the whole point: your can_send() works against the PSOC
 * Control CAN FD controller here, and unchanged against any other Zephyr
 * target, because the application talks to the generic API and the vendor
 * driver supplies the implementation. Swapping silicon does not rewrite
 * this file.
 *
 * Each TODO below ends with a "Docs:" line pointing at the page for the
 * calls that step needs.
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/adc.h>
#include <zephyr/drivers/can.h>
#include <zephyr/logging/log.h>

#include "can_protocol.h"
#include "lab_led.h"

LOG_MODULE_REGISTER(command_node, LOG_LEVEL_INF);

#define SAMPLE_PERIOD_MS 50
#define CAN_BITRATE      500000

#if !DT_NODE_HAS_PROP(DT_PATH(zephyr_user), io_channels)
#error "No ADC channel on /zephyr,user. Complete Step 1 (your board's overlay in boards/, TODO 1a/1b) before building - see the lab guide."
#endif

/* Alias led1 - P9.5, silkscreen LED2, red - toggles on every CAN frame sent,
 * a quick "yes, a frame just went out" indicator, independent of the
 * brightness demo on led0. Note the board silkscreen counts from one while
 * the devicetree aliases count from zero. Provided.
 */
static const struct gpio_dt_spec heartbeat_led = GPIO_DT_SPEC_GET(DT_ALIAS(led1), gpios);

/* The potentiometer's ADC channel, described by the overlay you edited in
 * Step 1. Provided.
 */
static const struct adc_dt_spec pot_adc = ADC_DT_SPEC_GET(DT_PATH(zephyr_user));

/* The CAN controller, picked up from the board's `zephyr,canbus` chosen
 * node. Provided.
 */
static const struct device *const can_dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_canbus));

static uint8_t read_setpoint(void)
{
	int16_t sample = 0;
	struct adc_sequence seq = {
		.buffer = &sample,
		.buffer_size = sizeof(sample),
	};

	/* Fill in the rest of `seq` (resolution, channel mask, ...) from the
	 * devicetree description. Provided.
	 */
	(void)adc_sequence_init_dt(&pot_adc, &seq);

	/*
	 * TODO 3b: take one reading from the potentiometer and turn it into
	 * a 0-255 setpoint.
	 *
	 *   1. Read a single sample with the ADC read call that takes a
	 *      devicetree spec and a sequence - pot_adc and the seq you just
	 *      initialised. It returns 0 on success; if it fails, return 128
	 *      so the LED sits at mid-scale rather than going dark.
	 *   2. The result lands in `sample` as a 12-bit value (0-4095).
	 *      Scale it down to 8 bits (0-255) and return it.
	 *
	 * Hint for the scaling: 12 bits is 4 more bits than 8 bits, and
	 * shifting a value right by one bit halves it.
	 *
	 * Docs: adc_read_dt() and friends
	 *   https://docs.zephyrproject.org/latest/doxygen/html/group__adc__interface.html
	 * The `seq` struct filled in above:
	 *   https://docs.zephyrproject.org/latest/doxygen/html/structadc__sequence.html
	 */

	/*
	 * TODO 3b - ANSWER.
	 *
	 * Copy everything between the two ==== marker lines.
	 * Then delete the `return 128;` placeholder below, which
	 * only exists so the skeleton compiles.
	 */
	/* ==== copy from the next line ====

	if (adc_read_dt(&pot_adc, &seq) != 0) {
		return 128;
	}

	return (uint8_t)(sample >> 4);

	==== stop copying at the line above ==== */

	return 128; /* Replace this. */
}

/*
 * Handed to can_send() so that transmission is fire-and-forget. We do not
 * need the result here, but passing *something* matters: with a NULL
 * callback, can_send() blocks until the frame is acknowledged, and a node
 * that is alone on the bus is never acknowledged - the application would
 * deadlock on the very first send. Provided.
 */
static void tx_done_cb(const struct device *dev, int error, void *user_data)
{
	ARG_UNUSED(dev);
	ARG_UNUSED(error);
	ARG_UNUSED(user_data);
}

static void send_setpoint(uint8_t setpoint)
{
	/*
	 * TODO 3c: put `setpoint` on the wire.
	 *
	 *   1. Declare a `struct can_frame` and fill in three fields: the
	 *      frame ID and the data length code both come from
	 *      can_protocol.h (CAN_ID_SETPOINT and CAN_DLC_SETPOINT), and
	 *      the setpoint itself goes in the first data byte.
	 *   2. Transmit it with the CAN send call. It takes the device, your
	 *      frame, a timeout, a completion callback and a user-data
	 *      pointer. Pass K_NO_WAIT for the timeout, tx_done_cb - the
	 *      empty function provided just above - for the callback, and
	 *      NULL for the user data.
	 *
	 *      Do NOT pass NULL in place of the callback: a NULL callback
	 *      makes the send wait until somebody acknowledges the frame,
	 *      and nobody will until your partner's board is wired up - the
	 *      program would stop dead right here.
	 *   3. The send call returns non-zero when the frame could not be
	 *      queued, normally because the second board is not powered or
	 *      wired yet. Warn on that - but only when the state *changes*.
	 *      This function runs 20 times a second, so an unconditional
	 *      LOG_WRN buries everything else in the console. A
	 *      `static bool warned;` is all you need: warn on the first
	 *      failure, and log recovery on the first success afterwards.
	 *
	 * Docs: can_send() and the rest of the CAN API
	 *   https://docs.zephyrproject.org/latest/doxygen/html/group__can__interface.html
	 * The frame you are filling in, field by field:
	 *   https://docs.zephyrproject.org/latest/doxygen/html/structcan__frame.html
	 */
	/*
	 * TODO 3c - ANSWER.
	 *
	 * Copy everything between the two ==== marker lines.
	 * Then delete the `ARG_UNUSED(setpoint);` line below, which
	 * only exists so the skeleton compiles.
	 */
	/* ==== copy from the next line ====

	struct can_frame frame = {
		.id = CAN_ID_SETPOINT,
		.dlc = CAN_DLC_SETPOINT,
		.data = {setpoint},
	};

	static bool warned;

	if (can_send(can_dev, &frame, K_NO_WAIT, tx_done_cb, NULL) != 0) {
		if (!warned) {
			warned = true;
			LOG_WRN("CAN send failed - is the second board powered and wired?");
		}
	} else if (warned) {
		warned = false;
		LOG_INF("CAN send recovered - partner board is on the bus");
	}

	==== stop copying at the line above ==== */

	ARG_UNUSED(setpoint);

	/* Blink so you can see a transmit attempt happen. Provided. */
	gpio_pin_toggle_dt(&heartbeat_led);
}

void run_command_node(void)
{
	LOG_INF("Command node starting (PSOC Control CAN lab)");

	/*
	 * TODO 3a: bring up the CAN controller. Four calls, in this order:
	 *
	 *   1. Check the driver initialised, using the generic
	 *      device_is_ready() helper with the CAN device handle can_dev.
	 *      It returns false if the driver did not come up - log an error
	 *      with LOG_ERR() and return if so.
	 *   2. Set the bitrate to CAN_BITRATE.
	 *   3. Set the mode to CAN_MODE_NORMAL.
	 *   4. Start the controller.
	 *
	 * Steps 2-4 are three separate CAN API calls, each taking can_dev as
	 * their first argument. Find their names in the docs below.
	 *
	 * Both boards must use the same bitrate or they will not talk to
	 * each other.
	 *
	 * Docs: can_set_bitrate(), can_set_mode(), can_start()
	 *   https://docs.zephyrproject.org/latest/doxygen/html/group__can__interface.html
	 * What a CAN controller is doing underneath:
	 *   https://docs.zephyrproject.org/latest/hardware/peripherals/can/controller.html
	 */

	/*
	 * TODO 3a - ANSWER.
	 *
	 * Copy everything between the two ==== marker lines.
	 * Paste it directly below this block.
	 */
	/* ==== copy from the next line ====

	if (!device_is_ready(can_dev)) {
		LOG_ERR("CAN device not ready");
		return;
	}

	can_set_bitrate(can_dev, CAN_BITRATE);
	can_set_mode(can_dev, CAN_MODE_NORMAL);
	can_start(can_dev);

	==== stop copying at the line above ==== */

	/* Provided: heartbeat LED and the brightness output. */
	gpio_pin_configure_dt(&heartbeat_led, GPIO_OUTPUT_INACTIVE);
	lab_led_init();

	/*
	 * TODO 3d: the ADC channel needs configuring once before it can be
	 * read. One call - the devicetree-spec form of the ADC channel setup
	 * function, passing the pot_adc spec declared at the top of this
	 * file. It belongs here, before the loop, not inside it.
	 *
	 * Docs: adc_channel_setup_dt() takes the `struct adc_dt_spec` that
	 * ADC_DT_SPEC_GET() built from your overlay:
	 *   https://docs.zephyrproject.org/latest/doxygen/html/structadc__dt__spec.html
	 *   https://docs.zephyrproject.org/latest/doxygen/html/group__adc__interface.html
	 */

	/*
	 * TODO 3d - ANSWER.
	 *
	 * Copy everything between the two ==== marker lines.
	 * Paste it directly below this block.
	 */
	/* ==== copy from the next line ====

	adc_channel_setup_dt(&pot_adc);

	==== stop copying at the line above ==== */

	while (1) {
		uint8_t setpoint = read_setpoint();

		/* Print the setpoint about twice a second. Until TODO 3b is
		 * done this stays pinned at 128, which is a quick way to
		 * check whether your ADC code is working. Provided.
		 */
		static uint32_t tick;

		if ((tick++ % (500U / SAMPLE_PERIOD_MS)) == 0U) {
			LOG_INF("setpoint %3u (%u%%)", setpoint,
				(setpoint * 100U) / 255U);
		}

		/* Local brightness feedback, so you can see the knob working
		 * before CAN is involved at all.
		 */
		lab_led_set_duty(setpoint);

		send_setpoint(setpoint);

		k_msleep(SAMPLE_PERIOD_MS);
	}
}
