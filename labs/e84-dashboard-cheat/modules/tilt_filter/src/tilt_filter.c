/*
 * Copyright (c) 2026 Infineon Technologies AG,
 * or an affiliate of Infineon Technologies AG.
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Exponential moving average:
 *
 *     out = out + alpha * (sample - out)
 *
 * done in integer millidegrees. alpha arrives as a percentage from Kconfig,
 * so the multiply is by CONFIG_TILT_FILTER_ALPHA_PERCENT and the divide by
 * 100. The subtraction is performed first so the intermediate stays small
 * and signed, which keeps it well inside int32_t for any plausible angle.
 */

#include <tilt_filter.h>
#include <zephyr/kernel.h>

void tilt_filter_reset(struct tilt_filter *f)
{
	f->value = 0;
	f->primed = false;
}

int32_t tilt_filter_update(struct tilt_filter *f, int32_t sample)
{
	if (!f->primed) {
		f->value = sample;
		f->primed = true;
		return f->value;
	}

	f->value += ((sample - f->value) * CONFIG_TILT_FILTER_ALPHA_PERCENT) / 100;

	return f->value;
}
