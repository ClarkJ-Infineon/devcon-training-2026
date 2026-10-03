/*
 * Copyright (c) 2026 Infineon Technologies AG,
 * or an affiliate of Infineon Technologies AG.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef UI_H_
#define UI_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Number of LED channels the dashboard exposes. */
#define UI_LED_COUNT 3

/**
 * @brief Called when the attendee drags one of the brightness sliders.
 *
 * @param index    LED index, 0..UI_LED_COUNT-1.
 * @param percent  Requested brightness, 0..100.
 *
 * Runs in whichever thread calls lv_timer_handler() - here that is main(),
 * so it is safe to touch the PWM devices directly from it.
 */
typedef void (*ui_brightness_cb_t)(uint8_t index, uint8_t percent);

/**
 * @brief Build the dashboard on the active screen.
 *
 * @param cb               Invoked on every slider change.
 * @param initial_percent  Starting brightness for each channel.
 */
void ui_init(ui_brightness_cb_t cb, const uint8_t initial_percent[UI_LED_COUNT]);

/**
 * @brief Update the tilt meter.
 *
 * @param pitch_mdeg  Pitch in millidegrees.
 * @param roll_mdeg   Roll in millidegrees.
 */
void ui_set_tilt(int32_t pitch_mdeg, int32_t roll_mdeg);

/** @brief Replace the status line text. */
void ui_set_status(const char *text);

#ifdef __cplusplus
}
#endif

#endif /* UI_H_ */
