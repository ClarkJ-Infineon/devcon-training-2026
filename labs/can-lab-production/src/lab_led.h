/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * PSOC Control CAN Command & Telemetry lab - brightness output.
 *
 * Provided helper, not a lab touchpoint.
 *
 * The user LEDs on P8.4 and P8.5 have no TCPWM route in their HSIOM
 * pin-mux tables, so a hardware PWM signal cannot be muxed onto them
 * directly. Two back-ends solve that, selected at build time:
 *
 *   lab_led_trigmux.c  (default) routes the real TCPWM waveform to P8.4
 *                      through the PSOC Control PERI trigger multiplexer,
 *                      via the pin's PERI_TR_IO_OUTPUT60 HSIOM option.
 *   lab_led_softpwm.c  (-DLED_SOFTPWM=y) bit-bangs P8.4 from a dedicated
 *                      thread.
 *
 * The hardware PWM channel is also brought out on P5.0, header J21 pin 11,
 * where it can be probed or used to drive an external load.
 */

#ifndef PSOC_CONTROL_CAN_LAB_LAB_LED_H_
#define PSOC_CONTROL_CAN_LAB_LAB_LED_H_

#include <stdint.h>

/* Bring up the brightness output (user LED on P8.4, alias led0). */
void lab_led_init(void);

/*
 * Update the brightness duty cycle, 0 (off) - 255 (fully on).
 *
 * Drives both the user LED on P8.4 (silkscreen LED3, alias led0) and the
 * hardware PWM channel on P5.0. Under the trigger-mux back-end both are
 * the same TCPWM channel, so one register write covers them; under the
 * soft-PWM fallback the LED is bit-banged and the PWM channel is
 * programmed separately.
 */
void lab_led_set_duty(uint8_t duty_0_255);

#endif /* PSOC_CONTROL_CAN_LAB_LAB_LED_H_ */