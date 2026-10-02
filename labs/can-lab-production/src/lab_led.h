/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * PSOC Control CAN Command & Telemetry lab - on-board LED brightness.
 *
 * Provided helper, not a lab touchpoint.
 *
 * Neither of the KIT_PSC3M5_EVK on-board user LEDs (P8.4 / P8.5) has a
 * direct TCPWM option in its HSIOM pin-mux table, so a hardware PWM signal
 * cannot simply be pin-muxed onto them. This interface therefore has two
 * interchangeable back-ends, selected at build time:
 *
 *   lab_led_softpwm.c  (default)   - bit-bang LED0 from a dedicated thread.
 *   lab_led_trigmux.c  (Option A)  - route the *real* TCPWM PWM line to
 *                                    LED0 through the PERI trigger mux.
 *
 * Build the trigger-mux variant by adding -DLED_TRIGMUX=y to the west
 * build command; see ../../README.md ("On-board LED drive path").
 */

#ifndef PSOC_CONTROL_CAN_LAB_LAB_LED_H_
#define PSOC_CONTROL_CAN_LAB_LAB_LED_H_

#include <stdint.h>

/* Bring up the on-board LED0 brightness output. */
void lab_led_init(void);

/*
 * Update the brightness duty cycle, 0 (off) - 255 (fully on).
 *
 * Drives both the on-board LED0 and the hardware PWM channel on mikroBUS
 * header 1 (P4.0). The two back-ends get there differently: the soft-PWM
 * path bit-bangs the LED and programs the PWM channel separately, while
 * the trigger-mux path programs the PWM channel only - the LED is wired to
 * that same TCPWM line through the mux, so it follows automatically.
 */
void lab_led_set_duty(uint8_t duty_0_255);

#endif /* PSOC_CONTROL_CAN_LAB_LAB_LED_H_ */
