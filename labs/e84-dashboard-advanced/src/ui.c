/*
 * Copyright (c) 2026 Infineon Technologies AG,
 * or an affiliate of Infineon Technologies AG.
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * DevCon 2026 PSOC Edge lab - Device Dashboard UI (cheat tier).
 *
 * All layout is absolute. LVGL's flex/grid layouts would be tidier, but this
 * screen is drawn once at a fixed resolution and absolute coordinates are far
 * easier to follow for someone meeting LVGL for the first time - which is the
 * entire audience for this file.
 *
 * A note on width: the display controller is configured for 832x480 total
 * timing, of which 800x480 is active. LVGL therefore believes the screen is
 * 832 px wide, and anything drawn beyond x=799 lands in the horizontal
 * blanking interval and is never visible. UI_W below is the usable width.
 */

#include "ui.h"

#include <lvgl.h>
#include <stdio.h>
#include <zephyr/sys/util.h>

/*
 * The beeper module only contributes a header when its Kconfig symbol is
 * set, so both the include and the calls below are guarded. Set
 * CONFIG_BEEPER=n and the dashboard builds and runs exactly as before,
 * silently.
 */
#ifdef CONFIG_BEEPER
#include <beeper.h>
#endif

/* ------------------------------------------------------------------ */
/* Palette                                                            */
/* ------------------------------------------------------------------ */

/*
 * Infineon Ocean (#0A8276) is the brand anchor. Everything else is a neutral
 * dark scale chosen to sit under it without competing - on an 800x480 panel
 * viewed from across a training room, contrast matters more than hue.
 */
#define C_OCEAN      0x0A8276
#define C_OCEAN_DEEP 0x05514A
#define C_OCEAN_LT   0x1ECBB5

#define C_BG         0x050D10
#define C_BG_DEEP    0x0A1A1F
#define C_CARD       0x102227
#define C_CARD_DEEP  0x0A171B
#define C_BORDER     0x1C3B42
#define C_TEXT       0xEAF4F3
#define C_MUTED      0x7E9A9C

/* One colour per LED, matching what the attendee actually sees on the board. */
static const uint32_t led_color[UI_LED_COUNT] = {
	0xFF5A52, /* LED0 - red   */
	0x3CE07A, /* LED1 - green */
	0x4DA3FF, /* LED2 - blue  */
};

/*
 * Two names per LED, because they disagree and the disagreement is exactly
 * the kind of thing that costs an attendee ten minutes.
 *
 * The board's silkscreen counts from one: LED1 red, LED2 green, LED3 blue.
 * The devicetree aliases count from zero: pwm-led0, pwm-led1, pwm-led2. So
 * the alias the attendee edits is always one lower than the number printed
 * next to the LED they are watching. Showing both on the card turns that
 * into a lookup table instead of a trap.
 */
static const char *const led_name[UI_LED_COUNT] = {
	"LED1  RED",
	"LED2  GREEN",
	"LED3  BLUE",
};

static const char *const led_alias[UI_LED_COUNT] = {
	"pwm-led0",
	"pwm-led1",
	"pwm-led2",
};

/* ------------------------------------------------------------------ */
/* Layout                                                             */
/* ------------------------------------------------------------------ */

#define UI_W        800
#define UI_H        480

#define HEADER_H    56

#define LEFT_X      14
#define LEFT_W      452
#define CARD_H      124
#define CARD_GAP    10
#define CARD_Y0     (HEADER_H + 12)

#define RIGHT_X     480
#define RIGHT_W     306

#define ARC_SIZE    238

/* ------------------------------------------------------------------ */
/* State                                                              */
/* ------------------------------------------------------------------ */

static ui_brightness_cb_t brightness_cb;

static lv_obj_t *pct_label[UI_LED_COUNT];
static lv_obj_t *tilt_value_label;
static lv_obj_t *tilt_arc;
static lv_obj_t *pitch_label;
static lv_obj_t *roll_label;
static lv_obj_t *status_label;

/* ------------------------------------------------------------------ */
/* Helpers                                                            */
/* ------------------------------------------------------------------ */

static lv_obj_t *panel_create(lv_obj_t *parent, int32_t x, int32_t y, int32_t w, int32_t h)
{
	lv_obj_t *p = lv_obj_create(parent);

	lv_obj_set_pos(p, x, y);
	lv_obj_set_size(p, w, h);

	/*
	 * lv_obj_create() gives a scrollable container with default padding and
	 * a light theme background. All three are wrong here, so they are
	 * cleared explicitly rather than fought with later.
	 */
	lv_obj_remove_flag(p, LV_OBJ_FLAG_SCROLLABLE);
	lv_obj_set_style_pad_all(p, 0, LV_PART_MAIN);

	/*
	 * Flat fill, not a gradient.
	 *
	 * The framebuffer is RGB565, so a gradient only has 32 distinct levels
	 * of red or blue to work with. Spread over a card this wide that lands
	 * as visible stair-stepping rather than a smooth wash, and LVGL 9 has
	 * no gradient dithering to hide it. Going to 24- or 32-bit colour is
	 * not an option here: two full-screen 800x480 buffers already take
	 * 1500 KiB of the 2 MB graphics region, and 32-bit would need 3000.
	 * Solid colours look deliberate, cost nothing, and render faster.
	 */
	lv_obj_set_style_bg_color(p, lv_color_hex(C_CARD), LV_PART_MAIN);
	lv_obj_set_style_bg_opa(p, LV_OPA_COVER, LV_PART_MAIN);

	lv_obj_set_style_border_color(p, lv_color_hex(C_BORDER), LV_PART_MAIN);
	lv_obj_set_style_border_width(p, 1, LV_PART_MAIN);
	lv_obj_set_style_radius(p, 10, LV_PART_MAIN);

	return p;
}

static lv_obj_t *label_create(lv_obj_t *parent, const lv_font_t *font, uint32_t color,
			      const char *text)
{
	lv_obj_t *l = lv_label_create(parent);

	lv_label_set_text(l, text);
	lv_obj_set_style_text_font(l, font, LV_PART_MAIN);
	lv_obj_set_style_text_color(l, lv_color_hex(color), LV_PART_MAIN);

	return l;
}

/* ------------------------------------------------------------------ */
/* Slider handling                                                    */
/* ------------------------------------------------------------------ */

static void slider_event_cb(lv_event_t *e)
{
	/*
	 * The LED index was stashed as the event user_data when the callback
	 * was registered. Casting through uintptr_t rather than int keeps this
	 * clean on both 32- and 64-bit builds (the native_sim target is 64-bit).
	 */
	uint8_t index = (uint8_t)(uintptr_t)lv_event_get_user_data(e);
	lv_obj_t *slider = lv_event_get_target_obj(e);
	int32_t value = lv_slider_get_value(slider);
	char buf[8];

	snprintf(buf, sizeof(buf), "%d%%", (int)value);
	lv_label_set_text(pct_label[index], buf);

	if (brightness_cb != NULL) {
		brightness_cb(index, (uint8_t)value);
	}
}

/*
 * Feedback fires on press and on release, not on every value change.
 *
 * VALUE_CHANGED is emitted continuously while a finger is moving - dozens of
 * times a second - so a tone there is a buzz rather than a tick. The two ends
 * of the gesture are the moments worth marking: the press acknowledges the
 * touch, and the release is where the value actually lands on this panel.
 *
 * The press tone is lower and shorter. That ordering is not arbitrary - a
 * low-then-high pair is heard as one gesture, the way a physical button
 * sounds different going down and coming back up, whereas two tones at the
 * same pitch sound like the thing fired twice.
 */
#ifdef CONFIG_BEEPER
/*
 * TODO 3c (part 1 of 2): write the two feedback callbacks.
 *
 * Both take an lv_event_t * and return void. Neither needs anything out
 * of the event, so ARG_UNUSED(e) keeps the compiler quiet.
 *
 *   slider_pressed_cb   calls beeper_press()
 *   slider_released_cb  calls beeper_click()
 *
 * Both are declared in modules/beeper/include/beeper.h, already included
 * at the top of this file.
 *
 * Part 2 registers them - see TODO 3c further down in led_card_create().
 */
#endif

static void led_card_create(lv_obj_t *parent, uint8_t index, uint8_t initial)
{
	int32_t y = CARD_Y0 + index * (CARD_H + CARD_GAP);
	lv_obj_t *card = panel_create(parent, LEFT_X, y, LEFT_W, CARD_H);
	lv_obj_t *accent;
	lv_obj_t *name;
	lv_obj_t *alias;
	lv_obj_t *slider;
	char buf[8];

	/* Colour chip down the left edge - the only hue cue at a glance. */
	accent = lv_obj_create(card);
	lv_obj_set_pos(accent, 0, 18);
	lv_obj_set_size(accent, 6, CARD_H - 36);
	lv_obj_remove_flag(accent, LV_OBJ_FLAG_SCROLLABLE);
	lv_obj_set_style_pad_all(accent, 0, LV_PART_MAIN);
	lv_obj_set_style_border_width(accent, 0, LV_PART_MAIN);
	lv_obj_set_style_radius(accent, 3, LV_PART_MAIN);
	lv_obj_set_style_bg_color(accent, lv_color_hex(led_color[index]), LV_PART_MAIN);
	lv_obj_set_style_bg_opa(accent, LV_OPA_COVER, LV_PART_MAIN);

	name = label_create(card, &lv_font_montserrat_18, C_TEXT, led_name[index]);
	lv_obj_set_pos(name, 24, 14);

	alias = label_create(card, &lv_font_montserrat_14, C_MUTED, led_alias[index]);
	lv_obj_set_pos(alias, 24, 38);

	snprintf(buf, sizeof(buf), "%u%%", initial);
	pct_label[index] = label_create(card, &lv_font_montserrat_28, led_color[index], buf);
	lv_obj_align(pct_label[index], LV_ALIGN_TOP_RIGHT, -20, 10);

	slider = lv_slider_create(card);
	lv_obj_set_pos(slider, 24, 70);
	lv_obj_set_size(slider, LEFT_W - 48, 26);
	lv_slider_set_range(slider, 0, 100);
	lv_slider_set_value(slider, initial, LV_ANIM_OFF);

	/*
	 * A 16 px track is about 2.4 mm on this panel - fine for a mouse, mean
	 * for a fingertip. The track is tall enough to aim at, and the click
	 * area is extended past it so a press that lands just above or below
	 * still grabs the slider the attendee was reaching for rather than
	 * falling through to the card behind it.
	 */
	lv_obj_set_ext_click_area(slider, 14);

	/* Track. */
	lv_obj_set_style_bg_color(slider, lv_color_hex(C_BG_DEEP), LV_PART_MAIN);
	lv_obj_set_style_border_color(slider, lv_color_hex(C_BORDER), LV_PART_MAIN);
	lv_obj_set_style_border_width(slider, 1, LV_PART_MAIN);

	/* Filled portion, in the LED's own colour. */
	lv_obj_set_style_bg_color(slider, lv_color_hex(led_color[index]), LV_PART_INDICATOR);
	lv_obj_set_style_bg_opa(slider, LV_OPA_COVER, LV_PART_INDICATOR);

	/* Knob - oversized on purpose; this is a finger-driven 4.3" panel. */
	lv_obj_set_style_bg_color(slider, lv_color_hex(C_TEXT), LV_PART_KNOB);
	lv_obj_set_style_pad_all(slider, 8, LV_PART_KNOB);

	lv_obj_add_event_cb(slider, slider_event_cb, LV_EVENT_VALUE_CHANGED,
			    (void *)(uintptr_t)index);

#ifdef CONFIG_BEEPER
	/*
	 * TODO 3c (part 2 of 2): register the two callbacks you just wrote.
	 *
	 * lv_obj_add_event_cb() takes the object, the callback, the event code
	 * and a user-data pointer (NULL here - the callbacks ignore it).
	 *
	 * The codes you want are LV_EVENT_PRESSED and LV_EVENT_RELEASED. Note
	 * the slider above already registers LV_EVENT_VALUE_CHANGED for the
	 * percentage readout; these are two further registrations on the same
	 * object, not a replacement for it. An object can carry as many as you
	 * like and LVGL calls them in registration order.
	 *
	 * Resist using VALUE_CHANGED for the tone. Read the comment above
	 * slider_pressed_cb for why that turns a tick into a buzz.
	 *
	 * https://docs.lvgl.io/master/API/core/lv_obj_event.html
	 */
#endif
}

/* ------------------------------------------------------------------ */
/* Public                                                             */
/* ------------------------------------------------------------------ */

void ui_init(ui_brightness_cb_t cb, const uint8_t initial_percent[UI_LED_COUNT])
{
	lv_obj_t *scr = lv_screen_active();
	lv_obj_t *header;
	lv_obj_t *title;
	lv_obj_t *subtitle;
	lv_obj_t *tilt_card;
	lv_obj_t *status_card;
	lv_obj_t *caption;

	brightness_cb = cb;

	/* Screen background: flat fill - see panel_create() on RGB565 banding. */
	lv_obj_set_style_bg_color(scr, lv_color_hex(C_BG), LV_PART_MAIN);
	lv_obj_remove_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

	/* ---- Header ---- */
	header = lv_obj_create(scr);
	lv_obj_set_pos(header, 0, 0);
	lv_obj_set_size(header, UI_W, HEADER_H);
	lv_obj_remove_flag(header, LV_OBJ_FLAG_SCROLLABLE);
	lv_obj_set_style_pad_all(header, 0, LV_PART_MAIN);
	lv_obj_set_style_radius(header, 0, LV_PART_MAIN);
	lv_obj_set_style_border_width(header, 0, LV_PART_MAIN);
	lv_obj_set_style_bg_color(header, lv_color_hex(C_OCEAN), LV_PART_MAIN);
	lv_obj_set_style_bg_opa(header, LV_OPA_COVER, LV_PART_MAIN);

	title = label_create(header, &lv_font_montserrat_28, 0xFFFFFF, "PSOC Edge Dashboard");
	lv_obj_align(title, LV_ALIGN_LEFT_MID, 18, 0);

	subtitle = label_create(header, &lv_font_montserrat_14, 0xBFE8E2,
				"DevCon 2026  |  Zephyr + LVGL");
	lv_obj_align(subtitle, LV_ALIGN_RIGHT_MID, -18, 0);

	/* ---- LED cards ---- */
	for (uint8_t i = 0; i < UI_LED_COUNT; i++) {
		led_card_create(scr, i, initial_percent[i]);
	}

	/* ---- Tilt meter ---- */
	tilt_card = panel_create(scr, RIGHT_X, CARD_Y0, RIGHT_W, CARD_H * 2 + CARD_GAP);

	caption = label_create(tilt_card, &lv_font_montserrat_14, C_MUTED, "BOARD TILT");
	lv_obj_align(caption, LV_ALIGN_TOP_MID, 0, 14);

	tilt_arc = lv_arc_create(tilt_card);
	lv_obj_set_size(tilt_arc, ARC_SIZE, ARC_SIZE);
	lv_obj_align(tilt_arc, LV_ALIGN_TOP_MID, 0, 34);
	lv_arc_set_rotation(tilt_arc, 135);
	lv_arc_set_bg_angles(tilt_arc, 0, 270);
	lv_arc_set_range(tilt_arc, 0, 90);
	lv_arc_set_value(tilt_arc, 0);

	/*
	 * The arc is an output, not a control. Without this it steals touch
	 * events from the sliders behind it and the attendee can drag the tilt
	 * reading by hand, which makes the sensor look broken.
	 */
	lv_obj_remove_flag(tilt_arc, LV_OBJ_FLAG_CLICKABLE);
	lv_obj_remove_style(tilt_arc, NULL, LV_PART_KNOB);

	lv_obj_set_style_arc_color(tilt_arc, lv_color_hex(C_BG_DEEP), LV_PART_MAIN);
	lv_obj_set_style_arc_width(tilt_arc, 18, LV_PART_MAIN);
	lv_obj_set_style_arc_rounded(tilt_arc, true, LV_PART_MAIN);

	lv_obj_set_style_arc_color(tilt_arc, lv_color_hex(C_OCEAN_LT), LV_PART_INDICATOR);
	lv_obj_set_style_arc_width(tilt_arc, 18, LV_PART_INDICATOR);
	lv_obj_set_style_arc_rounded(tilt_arc, true, LV_PART_INDICATOR);

	tilt_value_label = label_create(tilt_card, &lv_font_montserrat_40, C_TEXT, "0");
	lv_obj_align_to(tilt_value_label, tilt_arc, LV_ALIGN_CENTER, 0, -6);

	/* ---- Status / raw readout ---- */
	status_card = panel_create(scr, RIGHT_X, CARD_Y0 + 2 * (CARD_H + CARD_GAP), RIGHT_W,
				   CARD_H);

	pitch_label = label_create(status_card, &lv_font_montserrat_18, C_TEXT, "PITCH    --");
	lv_obj_set_pos(pitch_label, 20, 16);

	roll_label = label_create(status_card, &lv_font_montserrat_18, C_TEXT, "ROLL     --");
	lv_obj_set_pos(roll_label, 20, 44);

	status_label = label_create(status_card, &lv_font_montserrat_14, C_MUTED, "starting...");
	lv_obj_set_pos(status_label, 20, 82);
}

void ui_set_tilt(int32_t pitch_mdeg, int32_t roll_mdeg)
{
	/*
	 * Last values actually rendered, in the precision the labels show
	 * (tenths of a degree) rather than raw milli-degrees.
	 *
	 * lv_label_set_text() invalidates unconditionally - it does not compare
	 * against the current text - so calling it every poll repaints all
	 * 832x480 pixels about twenty times a second to redraw characters that
	 * have not changed. Beyond being wasteful, that hands the display
	 * controller a new frame buffer on a cadence unrelated to the panel's
	 * 60 Hz refresh, and the two slowly drift in and out of phase. Guarding
	 * each update keeps the screen static whenever the readings are.
	 */
	static int32_t last_magnitude = INT32_MIN;
	static int32_t last_pitch_tenths = INT32_MIN;
	static int32_t last_roll_tenths = INT32_MIN;

	char buf[32];
	int32_t magnitude;
	int32_t pitch_tenths = pitch_mdeg / 100;
	int32_t roll_tenths = roll_mdeg / 100;

	/*
	 * A single number for the arc: the larger of the two absolute angles.
	 * Summing them would read above 90 for a board tipped on a corner, and
	 * a vector magnitude needs a square root for no visible benefit at this
	 * size.
	 */
	magnitude = MAX(pitch_mdeg < 0 ? -pitch_mdeg : pitch_mdeg,
			roll_mdeg < 0 ? -roll_mdeg : roll_mdeg) / 1000;
	magnitude = CLAMP(magnitude, 0, 90);

	if (magnitude != last_magnitude) {
		last_magnitude = magnitude;

		lv_arc_set_value(tilt_arc, magnitude);

		snprintf(buf, sizeof(buf), "%d", (int)magnitude);
		lv_label_set_text(tilt_value_label, buf);
	}

	/*
	 * Printed as integer degrees plus one decimal rather than via %f: the
	 * default cbprintf build has no floating-point support, so a %f here
	 * renders as a literal "%f" on the panel.
	 */
	if (pitch_tenths != last_pitch_tenths) {
		last_pitch_tenths = pitch_tenths;

		snprintf(buf, sizeof(buf), "PITCH  %4d.%01u deg", (int)(pitch_mdeg / 1000),
			 (unsigned int)((pitch_mdeg < 0 ? -pitch_mdeg : pitch_mdeg) % 1000) / 100);
		lv_label_set_text(pitch_label, buf);
	}

	if (roll_tenths != last_roll_tenths) {
		last_roll_tenths = roll_tenths;

		snprintf(buf, sizeof(buf), "ROLL   %4d.%01u deg", (int)(roll_mdeg / 1000),
			 (unsigned int)((roll_mdeg < 0 ? -roll_mdeg : roll_mdeg) % 1000) / 100);
		lv_label_set_text(roll_label, buf);
	}
}

void ui_set_status(const char *text)
{
	lv_label_set_text(status_label, text);
}
