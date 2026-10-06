/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * PSOC Control CAN Command & Telemetry lab - brightness output.
 *
 * PROVIDED HELPER - not a lab touchpoint. See lab_led.c if you are curious
 * about how it works; you only need these two calls.
 */

#ifndef PSOC_CONTROL_CAN_LAB_LAB_LED_H_
#define PSOC_CONTROL_CAN_LAB_LAB_LED_H_

#include <stdint.h>

/* Bring up the brightness output. Call once, before the main loop. */
void lab_led_init(void);

/*
 * Set the brightness, 0 (off) to 255 (fully on).
 *
 * Drives both the user LED on P9.4 (silkscreen LED1, alias led0) and the
 * hardware PWM pin P9.0, on connector X19.
 */
void lab_led_set_duty(uint8_t duty_0_255);

#endif /* PSOC_CONTROL_CAN_LAB_LAB_LED_H_ */
