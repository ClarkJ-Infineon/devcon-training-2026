/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * PSOC Control CAN Command & Telemetry lab - trigger-mux LED back-end
 * (default path: real hardware PWM on the on-board LED).
 *
 * Provided helper, not a lab touchpoint.
 *
 * WHY THIS EXISTS
 * ---------------
 * P8.4 (LED3, alias led0) has no TCPWM option in its HSIOM table. On the
 * C3M6 the only non-GPIO options are SCB5_SPI_SELECT1,
 * PERI_TR_IO_INPUT60, PERI_TR_IO_OUTPUT60 and PERI_TR_IO_OUTPUT128. So a
 * TCPWM output cannot be pin-muxed directly onto the LED.
 *
 * It can, however, reach the pin the long way round. The TCPWM counter's
 * trigger output *line* carries the PWM waveform itself - not just the
 * overflow/compare event strobes - and the PERI trigger multiplexer can
 * route that line to a "trigger IO output", which *is* a HSIOM option on
 * P8.4:
 *
 *     tcpwm0_6 tr_line[6]  ->  PERI trigger mux group 2
 *                          ->  HSIOM TR_IO_OUTPUT60  ->  P8.4 (led0)
 *
 * This is the PSOC Control trigger multiplexer doing what it exists for:
 * hardware-to-hardware signal routing with no CPU in the path. Once the
 * route is made, brightness costs exactly one PWM compare-register write -
 * no thread, no bit-banging, no jitter.
 *
 * Zephyr has no driver or devicetree binding for the PERI trigger mux, so
 * the route is made here with a direct PDL call. That is the sanctioned
 * interim approach, not a workaround being smuggled in. The pin-mux half of
 * the job is done in devicetree - see boards/led_trigmux.overlay.
 *
 * If this back-end ever misbehaves, rebuild with -DLED_SOFTPWM=y to fall
 * back to the bit-banged path in src/lab_led_softpwm.c.
 */

#include "lab_led.h"

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/pwm.h>
#include <zephyr/logging/log.h>

#include <cy_trigmux.h>

LOG_MODULE_REGISTER(lab_led_trigmux, LOG_LEVEL_INF);

/*
 * Trigger mux endpoints for this board.
 *
 * Input:  pwm0_6 sits on tcpwm0_6 - TCPWM0 group 0, counter 6. Group 0's
 *         trigger lines are numbered from 0, so counter 6 is line 6.
 * Output: HSIOM trigger IO output 60, which is the P8.4 (led0) option.
 *
 * Both endpoints live in mux group 2 (hence the _MUX_2_ in both names), so
 * the connection is legal in a single Cy_TrigMux_Connect() call. Both
 * constants come from the C3M6 device configuration header pulled in by the
 * PDL; they are not board-specific magic numbers.
 */
#define LAB_LED_TRIG_IN  TRIG_IN_MUX_2_TCPWM0_GRP0_LINE_6
#define LAB_LED_TRIG_OUT TRIG_OUT_MUX_2_HSIOM_TR_IO_OUTPUT60

/*
 * The on-board LEDs are active low (anode to VTARG_REF, cathode to the pin)
 * and the PWM channel is configured with normal polarity, so invert the line
 * on its way through the mux.
 *
 * Level - not edge - triggering is what makes the mux pass the waveform
 * through. Edge mode emits a fixed two-cycle pulse per rising edge, which
 * would discard the duty-cycle information that *is* the PWM.
 */
#define LAB_LED_TRIG_INVERT true
#define LAB_LED_TRIG_TYPE   TRIGGER_TYPE_LEVEL

static const struct pwm_dt_spec cmd_pwm = PWM_DT_SPEC_GET(DT_ALIAS(cmd_pwm));

void lab_led_init(void)
{
	cy_en_trigmux_status_t status =
		Cy_TrigMux_Connect(LAB_LED_TRIG_IN, LAB_LED_TRIG_OUT, LAB_LED_TRIG_INVERT,
				   LAB_LED_TRIG_TYPE);

	if (status != CY_TRIGMUX_SUCCESS) {
		LOG_ERR("Trigger mux connect failed (0x%08x) - on-board LED will stay dark. "
			"Rebuild with -DLED_SOFTPWM=y to use the bit-banged fallback.",
			(unsigned int)status);
		return;
	}

	LOG_INF("On-board LED3 (P8.4) driven by tcpwm0_6 via the PERI trigger mux");
}

void lab_led_set_duty(uint8_t duty_0_255)
{
	/*
	 * One write covers both outputs. The on-board LED is wired - through
	 * the trigger mux - to the very same TCPWM channel as the P5.0 header
	 * pin, so it follows this duty cycle automatically.
	 */
	(void)pwm_set_pulse_dt(&cmd_pwm,
			       (uint32_t)((uint64_t)cmd_pwm.period * duty_0_255 / 255U));
}
