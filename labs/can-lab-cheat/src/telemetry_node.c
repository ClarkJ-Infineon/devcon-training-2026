/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * PSOC Control CAN Command & Telemetry lab - Board B "Telemetry node".
 *
 * Receives the setpoint the paired "Command node" board sends over CAN and
 * mirrors it on the local LED - "command sent over the wire, remote node
 * responds."
 *
 * CHEAT TIER. This is can-lab-advanced/ with every TODO already filled
 * in. The TODO comments are left in place so each instruction sits next
 * to its answer - so this still reads as a lab, not just as source.
 *
 * Use it if you are blocking your partner and would rather read working
 * code than write it, or to check your own work against. Read the TODO
 * above each block before the code under it; that ordering is the point.
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

/* Half-period of the receive heartbeat, so the LED blinks at about 1 Hz. */
#define ACTIVITY_BLINK_MS 500

/* Alias led1 - P8.5, silkscreen LED4, blue - the receive-activity indicator.
 *
 * Blinking at about 1 Hz means setpoint frames are arriving; dark means they
 * have stopped. The board silkscreen counts from one, the devicetree aliases
 * count from zero. Provided.
 *
 * Note that the command node drives this same LED from a different signal:
 * the CAN controller's error state. That is deliberate, not an oversight.
 * Error state tells a receiver almost nothing - a node that only receives
 * never transmits, so it never collects transmit errors, and pulling the bus
 * wires leaves it error-active with the LED stuck on. Blinking on reception
 * is the honest signal here, and it has the useful side effect of telling you
 * which board is which from across the room.
 */
static const struct gpio_dt_spec activity_led = GPIO_DT_SPEC_GET(DT_ALIAS(led1), gpios);

/* Provided. enum can_state has no string helper in the Zephyr API, so this is
 * the lab's own. These are the standard CAN error-confinement states, and the
 * parenthetical on each is what it means in this room.
 */
static const char *state_name(enum can_state state)
{
	switch (state) {
	case CAN_STATE_ERROR_ACTIVE:
		return "error-active (bus is healthy - nobody is transmitting)";
	case CAN_STATE_ERROR_WARNING:
		return "error-warning (errors on the wire)";
	case CAN_STATE_ERROR_PASSIVE:
		return "error-passive (errors on the wire)";
	case CAN_STATE_BUS_OFF:
		return "bus-off";
	case CAN_STATE_STOPPED:
		return "stopped - the controller was never started";
	default:
		return "unknown";
	}
}

static const struct device *const can_dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_canbus));

/* Received frames are queued here by the CAN driver so the main loop can
 * pull them off at its own pace. Provided.
 */
/*
 * Depth 16, not 4. Setpoints arrive at a steady 20 Hz, which one frame of
 * queue would absorb - but reconnecting a pulled CAN wire flushes the command
 * node's transmit FIFO in one burst, and a shallow queue drops frames and
 * prints `can_common: Msgq overflowed` the moment the lab starts working
 * again. The lab guide asks you to pull that wire, so the burst is a normal
 * event here rather than an exceptional one.
 */
CAN_MSGQ_DEFINE(setpoint_msgq, 16);

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
	if (frame->dlc < CAN_DLC_SETPOINT) {
		return 128;
	}

	return frame->data[0];
}

void run_telemetry_node(void)
{
	LOG_INF("Telemetry node starting (PSOC Control CAN lab)");

	/* Provided: the activity LED and the brightness output. The LED starts
	 * dark; the receive loop below blinks it once frames arrive.
	 */
	gpio_pin_configure_dt(&activity_led, GPIO_OUTPUT_INACTIVE);
	lab_led_init();

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

	if (!device_is_ready(can_dev)) {
		LOG_ERR("CAN device not ready");
		return;
	}

	can_set_bitrate(can_dev, CAN_BITRATE);
	can_set_mode(can_dev, CAN_MODE_NORMAL);
	can_start(can_dev);

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


	const struct can_filter setpoint_filter = {
		.id = CAN_ID_SETPOINT,
		.mask = CAN_STD_ID_MASK,
	};

	if (can_add_rx_filter_msgq(can_dev, &setpoint_msgq, &setpoint_filter) < 0) {
		LOG_ERR("Failed to add CAN RX filter");
		return;
	}

	struct can_frame frame;
	bool link_up = false;
	int64_t last_log = 0;
	int64_t last_blink = 0;

	while (1) {
		if (k_msgq_get(&setpoint_msgq, &frame, RX_TIMEOUT) != 0) {
			/* Nothing arrived - hold the last known brightness. */
			if (link_up) {
				LOG_WRN("No setpoint for %d ms - is the command node "
					"powered and wired?",
					(int)k_ticks_to_ms_floor32(RX_TIMEOUT.ticks));
				link_up = false;

				/*
				 * TODO 3d, part 1 of 2: say why the data stopped,
				 * and stop the heartbeat.
				 *
				 *   1. Declare an `enum can_state` and ask the
				 *      controller to fill it in with can_get_state().
				 *      It takes can_dev, a pointer to your state, and
				 *      NULL for the error counters you do not need.
				 *   2. LOG_WRN() the result through the provided
				 *      state_name() helper.
				 *   3. Drive activity_led dark with gpio_pin_set_dt()
				 *      and reset last_blink to 0, so the stall is
				 *      visible from the back of the room as well as
				 *      on the console.
				 *
				 * That first call is the one piece of information
				 * this node cannot get any other way, and it is the
				 * reason it is worth asking for. A receiver never
				 * transmits, so it never collects transmit errors -
				 * pull the bus wires and it stays error-active and
				 * simply hears silence. "The bus looks healthy and
				 * nothing is arriving" and "the bus is in trouble"
				 * are completely different faults that look identical
				 * from here until you ask.
				 *
				 * Docs: can_get_state() and the controller states
				 *   https://docs.zephyrproject.org/latest/doxygen/html/group__can__interface.html
				 * Error confinement, and what each state means:
				 *   https://docs.zephyrproject.org/latest/hardware/peripherals/can/controller.html
				 */

				enum can_state state = CAN_STATE_ERROR_ACTIVE;

				(void)can_get_state(can_dev, &state, NULL);
				LOG_WRN("CAN controller is %s", state_name(state));

				gpio_pin_set_dt(&activity_led, 0);
				last_blink = 0;
			}

			continue;
		}

		uint8_t setpoint = unpack_setpoint(&frame);

		lab_led_set_duty(setpoint);

		if (!link_up) {
			LOG_INF("CAN link up - receiving setpoints");
			link_up = true;
		}

		int64_t now = k_uptime_get();

		/*
		 * TODO 3d, part 2 of 2: blink LED4 while frames are arriving.
		 *
		 * Toggle activity_led with gpio_pin_toggle_dt(), but only once
		 * ACTIVITY_BLINK_MS has passed since the last toggle - compare
		 * `now` against the `last_blink` declared above, exactly the
		 * way the provided console rate-limit below compares against
		 * `last_log`, and update last_blink when you toggle.
		 *
		 * Do not simply toggle once per frame. Setpoints arrive at
		 * 20 Hz, and a 20 Hz toggle reads to the eye as a steady dim
		 * glow rather than as activity - which is exactly the signal
		 * you are trying not to send.
		 *
		 * Docs: gpio_pin_toggle_dt()
		 *   https://docs.zephyrproject.org/latest/doxygen/html/group__gpio__interface.html
		 */

		if (now - last_blink >= ACTIVITY_BLINK_MS) {
			last_blink = now;
			gpio_pin_toggle_dt(&activity_led);
		}

		/* Rate-limited to ~2 Hz so the console stays readable. Until
		 * TODO 3c is done this prints a constant 128 - that is the
		 * placeholder, not a broken bus. Provided.
		 */
		if (now - last_log >= 500) {
			last_log = now;
			LOG_INF("setpoint %u (%u%%)", setpoint, (setpoint * 100U) / 255U);
		}
	}
}
