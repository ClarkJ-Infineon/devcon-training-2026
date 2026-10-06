/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * PSOC Control CAN Command & Telemetry lab - Board B "Telemetry node".
 *
 * Receives the setpoint the paired "Command node" board sends over CAN and
 * mirrors it on the local LED - "command sent over the wire, remote node
 * responds."
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
 * mistakes - device_is_ready(), and the receive timeout below, which is how
 * you find out the other board is not wired up yet. The reference solution
 * checks every call, and that is the pattern to use in production code.
 *
 * Where these APIs come from
 * --------------------------
 * Every driver call below is part of a published, versioned Zephyr API, and
 * it is worth knowing that the docs show you two different views of it:
 *
 *   What you call        The functions and structs your application uses -
 *                        can_add_rx_filter_msgq(), struct can_frame, and
 *                        so on:
 *                        https://docs.zephyrproject.org/latest/doxygen/html/group__can__interface.html
 *
 *   What a driver        The table of function pointers each vendor driver
 *   implements           fills in behind those calls:
 *                        https://docs.zephyrproject.org/latest/doxygen/html/structcan__driver__api.html
 *
 * That split is the whole point: this code works against the PSOC Control
 * CAN FD controller here, and unchanged against any other Zephyr target,
 * because the application talks to the generic API and the vendor driver
 * supplies the implementation. Swapping silicon does not rewrite this file.
 *
 * Each TODO below ends with a "Docs:" line pointing at the page for the
 * calls that step needs.
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/can.h>
#include <zephyr/logging/log.h>

#include "can_protocol.h"
#include "lab_led.h"

LOG_MODULE_REGISTER(telemetry_node, LOG_LEVEL_INF);

#define CAN_BITRATE 500000
#define RX_TIMEOUT  K_MSEC(500)

/* LED1 toggles on every CAN frame received. Provided. */
static const struct gpio_dt_spec heartbeat_led = GPIO_DT_SPEC_GET(DT_ALIAS(led1), gpios);

static const struct device *const can_dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_canbus));

/* Received frames are queued here by the CAN driver so the main loop can
 * pull them off at its own pace. Provided.
 */
CAN_MSGQ_DEFINE(setpoint_msgq, 4);

static uint8_t unpack_setpoint(const struct can_frame *frame)
{
	/*
	 * TODO 3c: pull the setpoint back out of the frame.
	 *
	 * The command node put it in data[0]. Check `frame->dlc` is at least
	 * CAN_DLC_SETPOINT first, and fall back to 128 if the frame is
	 * shorter than expected.
	 *
	 * Docs: the `dlc` and `data` fields you are reading here
	 *   https://docs.zephyrproject.org/latest/doxygen/html/structcan__frame.html
	 */
	/*
	 * TODO 3c - ANSWER.
	 *
	 * Copy everything between the two ==== marker lines.
	 * Then delete the `ARG_UNUSED(frame);` and `return 128;` placeholder lines below, which
	 * only exists so the skeleton compiles.
	 */
	/* ==== copy from the next line ====

	if (frame->dlc < CAN_DLC_SETPOINT) {
		return 128;
	}

	return frame->data[0];

	==== stop copying at the line above ==== */

	ARG_UNUSED(frame);

	return 128; /* Replace this. */
}

void run_telemetry_node(void)
{
	LOG_INF("Telemetry node starting (PSOC Control CAN lab)");

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
	 * This must match what your partner's command node uses, or the two
	 * boards will not talk to each other.
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

	/*
	 * TODO 3b: tell the driver which frames you care about.
	 *
	 *   1. Declare a `struct can_filter`. Set its id field to
	 *      CAN_ID_SETPOINT (from can_protocol.h) and its mask field to
	 *      CAN_STD_ID_MASK, which means "match that ID exactly".
	 *   2. Register the filter against the message queue, using the CAN
	 *      receive-filter call whose name ends in _msgq. It takes the
	 *      device, the queue (setpoint_msgq, declared above) and your
	 *      filter, and returns a negative value on failure.
	 *
	 * Without a filter the controller receives nothing - this is the
	 * step people most often forget.
	 *
	 * Docs: can_add_rx_filter_msgq() and the filtering model
	 *   https://docs.zephyrproject.org/latest/doxygen/html/group__can__interface.html
	 * The filter you are declaring, field by field:
	 *   https://docs.zephyrproject.org/latest/doxygen/html/structcan__filter.html
	 */

	/*
	 * TODO 3b - ANSWER.
	 *
	 * Copy everything between the two ==== marker lines.
	 * Paste it directly below this block.
	 */
	/* ==== copy from the next line ====

	const struct can_filter setpoint_filter = {
		.id = CAN_ID_SETPOINT,
		.mask = CAN_STD_ID_MASK,
	};

	if (can_add_rx_filter_msgq(can_dev, &setpoint_msgq, &setpoint_filter) < 0) {
		LOG_ERR("Failed to add CAN RX filter");
		return;
	}

	==== stop copying at the line above ==== */

	/* Provided: heartbeat LED and the brightness output. */
	gpio_pin_configure_dt(&heartbeat_led, GPIO_OUTPUT_INACTIVE);
	lab_led_init();

	struct can_frame frame;
	bool link_up = false;
	int64_t last_log = 0;

	while (1) {
		if (k_msgq_get(&setpoint_msgq, &frame, RX_TIMEOUT) != 0) {
			/* Nothing arrived - hold the last known brightness. */
			if (link_up) {
				LOG_WRN("No setpoint for %d ms - is the command node "
					"powered and wired?",
					(int)k_ticks_to_ms_floor32(RX_TIMEOUT.ticks));
				link_up = false;
			}
			continue;
		}

		uint8_t setpoint = unpack_setpoint(&frame);

		lab_led_set_duty(setpoint);

		gpio_pin_toggle_dt(&heartbeat_led);

		if (!link_up) {
			LOG_INF("CAN link up - receiving setpoints");
			link_up = true;
		}

		/* Rate-limited to ~2 Hz so the console stays readable. Until
		 * TODO 3c is done this prints a constant 128 - that is the
		 * placeholder, not a broken bus. Provided.
		 */
		int64_t now = k_uptime_get();

		if (now - last_log >= 500) {
			last_log = now;
			LOG_INF("setpoint %u (%u%%)", setpoint, (setpoint * 100U) / 255U);
		}
	}
}
