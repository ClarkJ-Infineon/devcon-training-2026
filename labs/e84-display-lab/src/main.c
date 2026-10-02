/*
 * Copyright (c) 2026 Infineon Technologies AG,
 * or an affiliate of Infineon Technologies AG.
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * PSOC Edge E84 display smoke-test for the DevCon 2026 training branch.
 * Confirms the ported infineon_dc display controller driver + Waveshare
 * 4.3" DSI panel (waveshare,4p3) are working end-to-end: initializes the
 * display, draws a label, and updates a counter every second so the panel
 * is visibly live (not just a static frame left over from a bootloader
 * splash). This is a smoke test, not a feature demo - keep it minimal.
 */

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/display.h>
#include <zephyr/kernel.h>
#include <lvgl.h>
#include <stdio.h>

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(e84_display_lab);

int main(void)
{
	const struct device *display_dev;
	lv_obj_t *title_label;
	lv_obj_t *count_label;
	/*
	 * Sized for the longest string this can ever produce, not the shortest.
	 * "Uptime: " is 8 characters, so an 11-byte buffer holds only 10 plus
	 * the terminator and snprintf() silently truncates from 10 s onward.
	 * Truncation is easy to miss because it eats the trailing 's' first and
	 * only later the digits, so the counter appears to stall for longer and
	 * longer stretches as uptime grows - at a 4-digit uptime the visible
	 * text changes once every 100 s, which reads as a frozen display rather
	 * than a formatting bug.
	 */
	char count_str[32];
	uint32_t seconds = 0;
	int ret;

	/*
	 * Per-build stamp, logged before anything else runs.
	 *
	 * OpenOCD's write_image + verify_image path can report success on this
	 * chip family while leaving the previous image running, so a clean
	 * programmer log is not evidence that a flash landed. The Zephyr
	 * kernel build-ID banner cannot close that gap either, because it is
	 * identical for every build of the same commit. A compile timestamp
	 * changes on every rebuild and can be cross-checked against the ELF's
	 * mtime, which makes it the one value that identifies the running
	 * image. Requires a pristine build (west build -p always) to update.
	 */
	LOG_INF("e84-display-lab build stamp: " __DATE__ " " __TIME__);

	display_dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_display));
	if (!device_is_ready(display_dev)) {
		LOG_ERR("Display device not ready");
		return 0;
	}
	LOG_INF("display device %s ready", display_dev->name);

	title_label = lv_label_create(lv_screen_active());
	lv_label_set_text(title_label, "DevCon 2026 - PSOC Edge E84 Display Lab");
	lv_obj_align(title_label, LV_ALIGN_CENTER, 0, -20);

	count_label = lv_label_create(lv_screen_active());
	lv_obj_align(count_label, LV_ALIGN_CENTER, 0, 20);

	/*
	 * Progress logging around each step that can block. ifx_dc_write()
	 * ends in k_sem_take(&dc_sem, K_FOREVER) waiting on a display-
	 * controller frame-complete interrupt, so a misconfigured GFXSS clock
	 * tree makes the first flush block forever without reporting an error.
	 * These markers separate that case from one where frames are flushing
	 * but nothing is visible on the panel.
	 */
	LOG_INF("first flush (expected to be discarded by the driver)");
	lv_timer_handler();
	LOG_INF("first flush returned");

	ret = display_blanking_off(display_dev);
	if (ret < 0 && ret != -ENOSYS) {
		LOG_ERR("Failed to turn blanking off (error %d)", ret);
		return 0;
	}
	LOG_INF("blanking off (ret %d)", ret);

	/*
	 * Explicit brightness write. The Waveshare panel's backlight is
	 * driven entirely over I2C (brightness register 0x86), independent
	 * of the MIPI DSI video link, so this is the one call that proves
	 * host-to-panel I2C control actually reaches the panel - a dark
	 * backlight with a passing return code here means the I2C write is
	 * being acknowledged by something that is not the panel.
	 */
	ret = display_set_brightness(display_dev, 255);
	LOG_INF("set_brightness(255) ret %d", ret);

	while (1) {
		snprintf(count_str, sizeof(count_str), "Uptime: %us", seconds);
		lv_label_set_text(count_label, count_str);
		lv_timer_handler();
		LOG_INF("frame %u flushed", seconds);
		k_sleep(K_SECONDS(1));
		seconds++;
	}
}
