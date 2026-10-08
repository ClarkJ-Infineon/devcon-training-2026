/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * PSOC Control CAN Command & Telemetry lab - Board B "Telemetry node"
 * (reference solution).
 *
 * Receives the setpoint sent by the paired "Command node" board over CAN
 * and mirrors it on the local brightness output (user LED on P8.4 plus
 * the PWM pin P5.0).
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

/*
 * Receive-activity indicator on LED4 (P8.5, alias led1).
 *
 * Blinking at about 1 Hz means setpoint frames are arriving; dark means they
 * have stopped. This deliberately differs from the command node, which drives
 * the same LED from the CAN controller's error state.
 *
 * The reason is that error state tells a receiver almost nothing. A node that
 * only receives never transmits, so it never collects transmit errors - pull
 * the bus wires and it simply sees silence and stays error-active, with the
 * LED stuck on. Blinking on reception is the honest signal, and it has the
 * useful side effect of identifying which board is which from across the room.
 */
static const struct gpio_dt_spec activity_led = GPIO_DT_SPEC_GET(DT_ALIAS(led1), gpios);

static const struct device *const can_dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_canbus));

/*
 * Depth 16, not 4. Setpoints arrive at a steady 20 Hz, which one frame of
 * queue would absorb - but reconnecting a pulled CAN wire flushes the command
 * node's transmit FIFO in one burst, and a shallow queue drops frames and
 * prints `can_common: Msgq overflowed` the moment the lab starts working
 * again. The lab guide asks attendees to pull that wire, so the burst is a
 * normal event here rather than an exceptional one.
 */
CAN_MSGQ_DEFINE(setpoint_msgq, 16);

/* Zephyr has no public can_state_str(), so the lab carries its own. */
static const char *state_name(enum can_state state)
{
	switch (state) {
	case CAN_STATE_ERROR_ACTIVE:
		return "error-active (bus is healthy - nobody is transmitting)";
	case CAN_STATE_ERROR_WARNING:
		return "error-warning (errors are accumulating)";
	case CAN_STATE_ERROR_PASSIVE:
		return "error-passive (too many errors - check the wiring)";
	case CAN_STATE_BUS_OFF:
		return "bus-off (controller has removed itself from the bus)";
	case CAN_STATE_STOPPED:
		return "stopped";
	default:
		return "unknown";
	}
}

static uint8_t unpack_setpoint(const struct can_frame *frame)
{
	if (frame->dlc < CAN_DLC_SETPOINT) {
		return 128;
	}

	return frame->data[0];
}

void run_telemetry_node(void)
{
	LOG_INF("Telemetry node starting (PSOC Control CAN lab)");
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

	/* Starts dark; the receive loop below blinks it once frames arrive. */
	gpio_pin_configure_dt(&activity_led, GPIO_OUTPUT_INACTIVE);

	err = can_start(can_dev);
	if (err != 0) {
		LOG_ERR("can_start() failed (%d)", err);
		return;
	}

	const struct can_filter setpoint_filter = {
		.id = CAN_ID_SETPOINT,
		.mask = CAN_STD_ID_MASK,
	};

	if (can_add_rx_filter_msgq(can_dev, &setpoint_msgq, &setpoint_filter) < 0) {
		LOG_ERR("Failed to add CAN RX filter");
		return;
	}

	lab_led_init();

	struct can_frame frame;
	bool link_up = false;
	int64_t last_log = 0;
	int64_t last_blink = 0;

	while (1) {
		if (k_msgq_get(&setpoint_msgq, &frame, RX_TIMEOUT) != 0) {
			/* No frame yet - keep the last known brightness, but
			 * stop the heartbeat so the stall is visible from the
			 * back of the room as well as on the console.
			 */
			gpio_pin_set_dt(&activity_led, 0);
			last_blink = 0;

			if (link_up) {
				enum can_state state = CAN_STATE_ERROR_ACTIVE;

				LOG_WRN("No setpoint for %d ms - is the command node "
					"powered and wired?",
					(int)k_ticks_to_ms_floor32(RX_TIMEOUT.ticks));
				link_up = false;

				/* Why the data stopped is the one thing this node
				 * cannot infer. It only receives, so it never
				 * accumulates transmit errors and stays error-active
				 * with the bus wires pulled out. "Healthy bus, nobody
				 * transmitting" and "bus in trouble" are different
				 * faults that look identical from here until asked.
				 */
				err = can_get_state(can_dev, &state, NULL);
				if (err != 0) {
					LOG_ERR("can_get_state() failed (%d)", err);
				} else {
					LOG_WRN("CAN controller is %s", state_name(state));
				}
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

		/* Timed rather than one toggle per frame: setpoints arrive at
		 * 20 Hz, which the eye reads as a steady dim glow rather than
		 * as activity.
		 */
		if (now - last_blink >= ACTIVITY_BLINK_MS) {
			last_blink = now;
			gpio_pin_toggle_dt(&activity_led);
		}

		/* Rate-limit to ~2 Hz so the console stays readable. */
		if (now - last_log >= 500) {
			last_log = now;
			LOG_INF("setpoint %u (%u%%)", setpoint, (setpoint * 100U) / 255U);
		}
	}
}
