/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * PSOC Control CAN Command & Telemetry lab - Board B "Telemetry node"
 * (reference solution).
 *
 * Receives the setpoint sent by the paired "Command node" board over CAN
 * and mirrors it on the local brightness output (on-board LED0 + mikroBUS
 * PWM).
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

static const struct gpio_dt_spec heartbeat_led = GPIO_DT_SPEC_GET(DT_ALIAS(led1), gpios);

static const struct device *const can_dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_canbus));

CAN_MSGQ_DEFINE(setpoint_msgq, 4);

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

	gpio_pin_configure_dt(&heartbeat_led, GPIO_OUTPUT_INACTIVE);
	lab_led_init();

	struct can_frame frame;
	bool link_up = false;
	int64_t last_log = 0;

	while (1) {
		if (k_msgq_get(&setpoint_msgq, &frame, RX_TIMEOUT) != 0) {
			/* No frame yet - keep the last known brightness. */
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

		/* Rate-limit to ~2 Hz so the console stays readable. */
		int64_t now = k_uptime_get();

		if (now - last_log >= 500) {
			last_log = now;
			LOG_INF("setpoint %u (%u%%)", setpoint, (setpoint * 100U) / 255U);
		}
	}
}
