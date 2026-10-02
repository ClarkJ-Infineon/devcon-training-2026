/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * PSOC Control CAN Command & Telemetry lab - brightness output.
 *
 * PROVIDED HELPER - not a lab touchpoint. Your code calls lab_led_init()
 * once and lab_led_set_duty() whenever the setpoint changes; you do not
 * need to read or change anything in this file.
 *
 * One duty value, two outputs:
 *
 *   1. On-board LED0 (P8.5), bit-banged from a dedicated thread. Neither
 *      on-board user LED has a TCPWM option in its HSIOM pin-mux table, so
 *      a real PWM signal cannot be routed to them directly - hence the
 *      software PWM.
 *   2. The hardware PWM channel on mikroBUS header 1 / P4.0. Nothing on the
 *      board lights up from this one, but the signal is genuinely there if
 *      you want to put a scope on it. This is why the lab has you enable
 *      the PWM nodes in devicetree and CONFIG_PWM in prj.conf.
 *
 * Implementation note, in case you are curious: the ON pulse is timed with
 * k_busy_wait() so brightness is smooth regardless of the kernel tick rate,
 * but the OFF phase uses k_sleep() so the thread actually blocks.
 * k_busy_wait() spins without yielding, so a thread that only ever
 * busy-waits starves every lower-priority thread - including the logging
 * thread, which silently costs you all console output, boot banner and all.
 */

#include "lab_led.h"

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/pwm.h>
#include <zephyr/sys/atomic.h>
#include <zephyr/sys/util.h>

#if !DT_NODE_EXISTS(DT_ALIAS(cmd_pwm))
#error "No 'cmd-pwm' devicetree alias. Complete Step 1 (boards/kit_psc3m5_evk.overlay, TODO 1c) before building - see the lab guide."
#endif

#if !defined(CONFIG_PWM)
#error "CONFIG_PWM is not enabled. Complete Step 2 (prj.conf, TODO 2) before building - see the lab guide."
#endif

#define SOFT_PWM_PERIOD_USEC 2000U /* 500 Hz - smooth to the eye, no flicker */
#define SOFT_PWM_STACK_SIZE  512
#define SOFT_PWM_THREAD_PRIO 7

/* Guarantees the thread blocks at least once per period even at full
 * brightness. Caps usable duty just under 100%, which the eye cannot see.
 */
#define SOFT_PWM_MIN_SLEEP_USEC 50U

static const struct gpio_dt_spec led0 = GPIO_DT_SPEC_GET(DT_ALIAS(led0), gpios);
static const struct pwm_dt_spec cmd_pwm = PWM_DT_SPEC_GET(DT_ALIAS(cmd_pwm));
static atomic_t duty_atomic;

static void soft_pwm_thread(void *a, void *b, void *c)
{
	ARG_UNUSED(a);
	ARG_UNUSED(b);
	ARG_UNUSED(c);

	while (1) {
		uint8_t duty = (uint8_t)atomic_get(&duty_atomic);
		uint32_t on_us = (SOFT_PWM_PERIOD_USEC * duty) / 255U;
		uint32_t off_us = SOFT_PWM_PERIOD_USEC - on_us;

		if (on_us > 0U) {
			gpio_pin_set_dt(&led0, 1);
			k_busy_wait(on_us);
		}

		gpio_pin_set_dt(&led0, 0);
		k_sleep(K_USEC(MAX(off_us, SOFT_PWM_MIN_SLEEP_USEC)));
	}
}

K_THREAD_DEFINE(soft_pwm_tid, SOFT_PWM_STACK_SIZE, soft_pwm_thread, NULL, NULL, NULL,
		 K_PRIO_PREEMPT(SOFT_PWM_THREAD_PRIO), 0, 0);

void lab_led_init(void)
{
	atomic_set(&duty_atomic, 0);
	gpio_pin_configure_dt(&led0, GPIO_OUTPUT_INACTIVE);
}

void lab_led_set_duty(uint8_t duty_0_255)
{
	atomic_set(&duty_atomic, duty_0_255);

	/* Same duty on the real hardware PWM pin (mikroBUS 1 / P4.0). */
	(void)pwm_set_pulse_dt(&cmd_pwm,
			       (uint32_t)((uint64_t)cmd_pwm.period * duty_0_255 / 255U));
}
