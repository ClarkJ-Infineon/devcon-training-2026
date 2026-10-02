/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * PSOC Control CAN Command & Telemetry lab - soft-PWM LED back-end
 * (default / fallback path).
 *
 * Provided helper, not a lab touchpoint - see lab_led.h for why this
 * exists. Implemented as a dedicated thread that bit-bangs LED0.
 *
 * The ON pulse is timed with k_busy_wait() so brightness is smooth and
 * independent of the kernel tick rate, but the OFF phase uses k_sleep()
 * so the thread actually blocks. That matters: k_busy_wait() spins
 * without yielding, so a thread that only ever busy-waits will starve
 * every lower-priority thread - including the logging thread, which
 * silently costs you all console output (boot banner included).
 */

#include "lab_led.h"

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/pwm.h>
#include <zephyr/sys/atomic.h>
#include <zephyr/sys/util.h>

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
