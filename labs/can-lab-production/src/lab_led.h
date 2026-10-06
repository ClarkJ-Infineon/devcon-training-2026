/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * PSOC Control CAN Command & Telemetry lab - on-board LED brightness.
 *
 * Provided helper, not a lab touchpoint.
 *
 * The KIT_PSC3M5_CC2 user LEDs (P9.4 / P9.5) have no TCPWM option in their
 * HSIOM pin-mux table - their only non-GPIO routes are SCB0 SPI selects - so
 * a hardware PWM signal cannot be pin-muxed onto them. The on-board LED is
 * therefore driven by lab_led_softpwm.c, which bit-bangs it from a dedicated
 * thread.
 *
 * The real TCPWM PWM output still exists and is still enabled by the lab; it
 * comes out on P9.0 (connector X19), where it can be probed.
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
