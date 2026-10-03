/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * PSOC Control CAN Command & Telemetry lab - trigger-mux LED back-end
 * ("Option A", real hardware PWM on the on-board LED).
 *
 * Provided helper, not a lab touchpoint.
 *
 * WHY THIS EXISTS
 * ---------------
 * P8.5 (LED0) has no TCPWM option in its HSIOM table - the only non-GPIO
 * options are SCB5_SPI_SELECT2, PERI_TR_IO_INPUT43 and PERI_TR_IO_OUTPUT43.
 * So a TCPWM output cannot be pin-muxed directly onto the LED. It can,
 * however, reach the pin the long way round: the TCPWM counter's trigger
 * output line carries the PWM waveform itself (not just event strobes), and
 * the PERI trigger multiplexer can route that line to a "trigger IO output"
 * which *is* a HSIOM option on P8.5.
 *
 *     tcpwm1_4 tr_line  ->  PERI trigger mux group 2  ->  HSIOM TR_IO_OUTPUT43
 *                                                     ->  P8.5 (LED0)
 *
 * Zephyr has no driver or devicetree binding for the PERI trigger mux, so
 * the route is made here with a direct PDL call. The pin-mux half of the
 * job is done in devicetree - see boards/led_trigmux.overlay.
 *
 * STATUS: VERIFIED ON HARDWARE (2026-09-19, KIT_PSC3M5_EVK rev A0). Flashed
 * to one board while a second board ran the soft-PWM back-end; both LEDs
 * faded identically across the full potentiometer range with no flicker,
 * stepping or stuck extremes. That settles both residual risks recorded in
 * outputs/zephyr/engineering/psc3m5-pwm-led-trigger-mux/findings.md: the mux
 * passes the level through cleanly (no visible clock-domain resynchronisation)
 * and the tr_io_output path needs no extra PERI clock gating.
 *
 * If this back-end ever misbehaves, rebuild without -DLED_TRIGMUX=y to fall
 * back to the soft-PWM path.
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
 * Input:  tcpwm1_4 -> TCPWM group 1, counter 4. Group 1's trigger lines are
 *         numbered from 256, so counter 4 is line 260.
 * Output: HSIOM trigger IO output 43, which is the P8.5 (LED0) option.
 *
 * Both constants come from the PSC3 device configuration header pulled in by
 * the PDL; they are not board-specific magic numbers.
 */
#define LAB_LED_TRIG_IN  TRIG_IN_MUX_2_TCPWM0_GRP1_LINE_260
#define LAB_LED_TRIG_OUT TRIG_OUT_MUX_2_HSIOM_TR_IO_OUTPUT43

/*
 * The on-board LEDs are active low and the PWM channel is configured with
 * normal polarity, so invert the line on its way through the mux. Level (not
 * edge) triggering is what makes the mux pass the waveform through rather
 * than emitting a one-shot strobe per transition.
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
		LOG_ERR("Trigger mux connect failed (0x%08x) - on-board LED will stay dark",
			(unsigned int)status);
		return;
	}

	LOG_INF("On-board LED0 driven by tcpwm1_4 via the PERI trigger mux");
}

void lab_led_set_duty(uint8_t duty_0_255)
{
	/*
	 * One write covers both outputs here: the on-board LED is wired
	 * (through the trigger mux) to the very same TCPWM channel as the
	 * mikroBUS pin, so it follows this duty cycle automatically.
	 */
	(void)pwm_set_pulse_dt(&cmd_pwm,
			       (uint32_t)((uint64_t)cmd_pwm.period * duty_0_255 / 255U));
}
