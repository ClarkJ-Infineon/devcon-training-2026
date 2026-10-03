/*
 * Copyright (c) 2026 Infineon Technologies AG,
 * or an affiliate of Infineon Technologies AG.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef BEEPER_H_
#define BEEPER_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Bring up the codec and the I2S transmitter.
 *
 * Must be called once before any other function here. Safe to call from
 * main() at startup; it does not play anything.
 *
 * @return 0 on success, or a negative errno. A failure here is not fatal to
 *         the application - the other entry points become no-ops.
 */
int beeper_init(void);

/**
 * @brief Play the standard UI click, used when a control is released.
 *
 * Shorthand for beeper_tone() with the Kconfig-configured release
 * frequency and duration.
 */
void beeper_click(void);

/**
 * @brief Play the press tone, used when a control is first touched.
 *
 * Deliberately lower and shorter than beeper_click(). The pair reads as a
 * single gesture - down, then up - the way a physical button does, and the
 * pitch difference is what distinguishes "I have you" from "that is your
 * value".
 */
void beeper_press(void);

/**
 * @brief Queue a tone for playback.
 *
 * Returns immediately - playback happens on the module's own thread, so this
 * is safe to call from an LVGL event callback without stalling the frame.
 *
 * One further request may be queued behind the tone currently playing, which
 * is what lets a tap shorter than the press tone still produce its release
 * tone. Beyond that, requests are dropped rather than stacking up.
 *
 * @param freq_hz     Tone frequency. Clamped to a sane range.
 * @param duration_ms Tone length. Clamped to what the slab can hold.
 */
void beeper_tone(uint32_t freq_hz, uint32_t duration_ms);

#ifdef __cplusplus
}
#endif

#endif /* BEEPER_H_ */
