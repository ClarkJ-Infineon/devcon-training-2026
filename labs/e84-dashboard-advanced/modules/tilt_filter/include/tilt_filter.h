/*
 * Copyright (c) 2026 Infineon Technologies AG,
 * or an affiliate of Infineon Technologies AG.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef TILT_FILTER_H_
#define TILT_FILTER_H_

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief State for one exponential moving-average channel.
 *
 * Values are in millidegrees throughout, so the filter stays in integer
 * arithmetic. The CM55 has an FPU, but a filter this simple has no reason
 * to need it, and integers keep the maths obvious.
 */
struct tilt_filter {
	int32_t value;
	bool primed;
};

/**
 * @brief Reset a filter back to its unprimed state.
 *
 * The next sample pushed in is adopted verbatim rather than being averaged
 * against a zero that was never a real reading. Without this the meter
 * visibly sweeps up from 0 every time the filter is started.
 */
void tilt_filter_reset(struct tilt_filter *f);

/**
 * @brief Push one sample through the filter.
 *
 * @param f       Filter state.
 * @param sample  New reading, in millidegrees.
 *
 * @return Smoothed value, in millidegrees.
 */
int32_t tilt_filter_update(struct tilt_filter *f, int32_t sample);

#ifdef __cplusplus
}
#endif

#endif /* TILT_FILTER_H_ */
