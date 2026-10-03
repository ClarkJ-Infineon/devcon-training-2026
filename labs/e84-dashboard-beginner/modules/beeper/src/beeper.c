/*
 * Copyright (c) 2026 Infineon Technologies AG,
 * or an affiliate of Infineon Technologies AG.
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Touch feedback tones for the dashboard.
 *
 * The TLV320DAC3100 driver implements the audio_codec API, which configures
 * and routes the part but has no write operation - no audio flows through it.
 * Samples go to the codec over I2S, so playing a tone means generating the
 * samples here and handing them to the I2S driver. That is most of this file.
 *
 * The whole tone is generated, queued and only then started, rather than
 * being streamed. A click is tens of milliseconds, so it fits in the slab
 * several times over, and doing it this way removes the entire class of
 * underrun problems that a continuously-fed stream has to handle: there is
 * no feeder thread that can be late, because by the time the transmitter
 * starts there is nothing left to feed it.
 */

#include <beeper.h>

#include <zephyr/audio/codec.h>
#include <zephyr/device.h>
#include <zephyr/drivers/i2s.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/util.h>

#include <math.h>

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(beeper, LOG_LEVEL_INF);

/*
 * 16 kHz is the lowest rate that leaves the click tone well clear of Nyquist
 * and still divides exactly out of the 12.288 MHz MCLK the board overlay sets
 * up, so there is no sample-rate error to hear.
 *
 * The DAC3100 is mono into the onboard speaker, but the I2S frame is stereo
 * and both slots get the same sample: the codec sums them. Sending one
 * channel would work and halve the memory, at the cost of a configuration
 * that no longer matches the obvious reading of the datasheet.
 */
#define SAMPLE_RATE_HZ 16000U
#define CHANNELS       2U
#define WORD_SIZE_BITS 16U

/* 10 ms per block. */
#define BLOCK_FRAMES 160U
#define BLOCK_BYTES  (BLOCK_FRAMES * CHANNELS * sizeof(int16_t))

/*
 * Eight blocks is 80 ms, which is both the longest tone this module will play
 * and - deliberately - the entire budget. Nothing is reused mid-playback.
 */
#define BLOCK_COUNT 8U
#define MAX_TONE_MS ((BLOCK_COUNT * BLOCK_FRAMES * 1000U) / SAMPLE_RATE_HZ)

/*
 * MCLK the overlay delivers to the codec. The codec driver divides this down
 * to reach the frame rate, so it has to be told what it is actually getting
 * rather than what the SoC's default would have been.
 */
#define MCLK_HZ 12288000U

/*
 * One cycle of a sine, as a table. The alternative is calling sinf() per
 * sample, which would put floating point in the playback thread; with
 * CONFIG_FPU_SHARING off that is only safe for one thread in the system, and
 * main already uses it for the tilt maths. Building the table during init -
 * which runs in main's context - keeps the FPU in exactly one place.
 */
#define SINE_TAB_LEN 256U
static int16_t sine_tab[SINE_TAB_LEN];

/*
 * Ramp the tone in and out over 3 ms. Starting a 1760 Hz tone at full
 * amplitude from silence is a step discontinuity, and it is audible as a
 * click on the front of the click - the speaker is small enough that the
 * transient is most of what you hear.
 */
#define RAMP_FRAMES 48U

K_MEM_SLAB_DEFINE_STATIC(tx_slab, BLOCK_BYTES, BLOCK_COUNT, 4);

static const struct device *const i2s_dev = DEVICE_DT_GET(DT_NODELABEL(i2s0));
static const struct device *const codec_dev = DEVICE_DT_GET(DT_NODELABEL(audio_codec));

static bool ready;

/*
 * Pending tone requests.
 *
 * Depth 2 is deliberate and is exactly one gesture: a press and the release
 * that follows it. A tap can be shorter than the press tone takes to play,
 * and dropping the release in that case loses the half of the pair that
 * carries the result - audible, and the thing most likely to be noticed.
 *
 * It does not need to be deeper. Nothing requests a tone except those two
 * events, so anything arriving while both are already queued is a third tone
 * inside a single gesture, which there is no reason to want. Dropping those
 * is what keeps a fast series of taps from stacking up into a queue that
 * plays on long after the finger has stopped.
 */
struct tone_req {
	uint32_t freq_hz;
	uint32_t duration_ms;
};

K_MSGQ_DEFINE(tone_q, sizeof(struct tone_req), 2, 4);

static void sine_tab_init(void)
{
	for (uint32_t i = 0; i < SINE_TAB_LEN; i++) {
		double theta = (2.0 * 3.14159265358979 * (double)i) / (double)SINE_TAB_LEN;

		sine_tab[i] = (int16_t)lround(sin(theta) * 32767.0);
	}
}

/*
 * Fill one block with the slice of the tone starting at frame `start`.
 *
 * `phase` is a Q16 index into the sine table and is carried across blocks by
 * the caller, so the waveform stays continuous at block boundaries. Doing
 * this per-block from scratch would restart the phase every 10 ms and put a
 * discontinuity - another click - at each seam.
 */
static void fill_block(int16_t *dst, uint32_t start, uint32_t total_frames, uint32_t phase_inc,
		       uint32_t *phase, int32_t amplitude)
{
	for (uint32_t i = 0; i < BLOCK_FRAMES; i++) {
		uint32_t frame = start + i;
		int32_t sample = 0;

		if (frame < total_frames) {
			uint32_t ramp = MIN(RAMP_FRAMES, total_frames / 2U);
			uint32_t from_start = frame;
			uint32_t from_end = total_frames - 1U - frame;
			uint32_t edge = MIN(from_start, from_end);
			int32_t scale = (ramp == 0U || edge >= ramp)
						? 256
						: (int32_t)((edge * 256U) / ramp);

			sample = sine_tab[(*phase >> 16) & (SINE_TAB_LEN - 1U)];
			sample = (sample * amplitude) / 32767;
			sample = (sample * scale) / 256;

			*phase += phase_inc;
		}

		/* Same sample into both slots; the mono speaker sums them. */
		dst[i * CHANNELS] = (int16_t)sample;
		dst[i * CHANNELS + 1U] = (int16_t)sample;
	}
}

static void play(uint32_t freq_hz, uint32_t duration_ms)
{
	uint32_t total_frames = (duration_ms * SAMPLE_RATE_HZ) / 1000U;
	uint32_t blocks = DIV_ROUND_UP(total_frames, BLOCK_FRAMES);
	uint32_t phase_inc = (uint32_t)(((uint64_t)freq_hz * SINE_TAB_LEN << 16) / SAMPLE_RATE_HZ);
	uint32_t phase = 0;
	int32_t amplitude = (32767 * CONFIG_BEEPER_VOLUME_PERCENT) / 100;
	uint32_t queued = 0;
	int ret;

	for (uint32_t b = 0; b < blocks; b++) {
		void *block;

		ret = k_mem_slab_alloc(&tx_slab, &block, K_NO_WAIT);
		if (ret < 0) {
			LOG_WRN("slab exhausted after %u blocks", queued);
			break;
		}

		fill_block((int16_t *)block, b * BLOCK_FRAMES, total_frames, phase_inc, &phase,
			   amplitude);

		ret = i2s_write(i2s_dev, block, BLOCK_BYTES);
		if (ret < 0) {
			LOG_ERR("i2s_write failed (%d)", ret);
			k_mem_slab_free(&tx_slab, block);
			break;
		}

		queued++;
	}

	if (queued == 0U) {
		(void)i2s_trigger(i2s_dev, I2S_DIR_TX, I2S_TRIGGER_DROP);
		return;
	}

	ret = i2s_trigger(i2s_dev, I2S_DIR_TX, I2S_TRIGGER_START);
	if (ret < 0) {
		LOG_ERR("i2s start failed (%d)", ret);
		(void)i2s_trigger(i2s_dev, I2S_DIR_TX, I2S_TRIGGER_DROP);
		return;
	}

	/*
	 * DRAIN returns as soon as the request is accepted; it means "stop
	 * once the queue is empty", not "wait until it is". Everything is
	 * already queued, so the transmitter needs the tone's own duration to
	 * get through it. The margin covers the DMA finishing the final block.
	 */
	ret = i2s_trigger(i2s_dev, I2S_DIR_TX, I2S_TRIGGER_DRAIN);
	if (ret < 0) {
		LOG_ERR("i2s drain failed (%d)", ret);
		(void)i2s_trigger(i2s_dev, I2S_DIR_TX, I2S_TRIGGER_DROP);
		return;
	}

	k_sleep(K_MSEC(duration_ms + 20U));
}

static void beeper_thread(void *a, void *b, void *c)
{
	ARG_UNUSED(a);
	ARG_UNUSED(b);
	ARG_UNUSED(c);

	for (;;) {
		struct tone_req req;

		k_msgq_get(&tone_q, &req, K_FOREVER);
		play(req.freq_hz, req.duration_ms);
	}
}

/*
 * Preemptible and below main. The thread does nothing time-critical: it
 * queues a handful of blocks and then sleeps while the DMA does the work, so
 * it has no business competing with the LVGL loop for the CPU.
 */
K_THREAD_DEFINE(beeper_tid, 1024, beeper_thread, NULL, NULL, NULL, 5, 0, 0);

int beeper_init(void)
{
	struct i2s_config i2s_cfg;
	struct audio_codec_cfg codec_cfg;
	int ret;

	if (!device_is_ready(i2s_dev)) {
		LOG_ERR("i2s0 not ready");
		return -ENODEV;
	}

	if (!device_is_ready(codec_dev)) {
		LOG_ERR("audio codec not ready");
		return -ENODEV;
	}

	sine_tab_init();

	i2s_cfg.word_size = WORD_SIZE_BITS;
	i2s_cfg.channels = CHANNELS;
	i2s_cfg.format = I2S_FMT_DATA_FORMAT_I2S;
	/* The TDM block generates both clocks; the codec follows them. */
	i2s_cfg.options = I2S_OPT_FRAME_CLK_CONTROLLER | I2S_OPT_BIT_CLK_CONTROLLER;
	i2s_cfg.frame_clk_freq = SAMPLE_RATE_HZ;
	i2s_cfg.block_size = BLOCK_BYTES;
	i2s_cfg.mem_slab = &tx_slab;
	i2s_cfg.timeout = 100;

	ret = i2s_configure(i2s_dev, I2S_DIR_TX, &i2s_cfg);
	if (ret < 0) {
		LOG_ERR("i2s_configure failed (%d)", ret);
		return ret;
	}

	codec_cfg.dai_type = AUDIO_DAI_TYPE_I2S;
	codec_cfg.dai_route = AUDIO_ROUTE_PLAYBACK;
	codec_cfg.mclk_freq = MCLK_HZ;
	codec_cfg.dai_cfg.i2s = i2s_cfg;
	/* Mirror of the above: from the codec's side it is the clock target. */
	codec_cfg.dai_cfg.i2s.options = I2S_OPT_FRAME_CLK_TARGET | I2S_OPT_BIT_CLK_TARGET;

	ret = audio_codec_configure(codec_dev, &codec_cfg);
	if (ret < 0) {
		LOG_ERR("audio_codec_configure failed (%d)", ret);
		return ret;
	}

	/*
	 * Unmute once and leave it. The alternative is powering the output
	 * stage up and down around each tone, which is the usual advice for
	 * battery life but costs a dozen I2C transactions per click - on the
	 * same bus as the touch controller and the IMU, which is the busiest
	 * and most contended resource on this board.
	 */
	audio_codec_start_output(codec_dev);

	ready = true;
	LOG_INF("beeper ready: press %u Hz/%u ms, release %u Hz/%u ms, %u%% amplitude",
		CONFIG_BEEPER_PRESS_HZ, CONFIG_BEEPER_PRESS_MS, CONFIG_BEEPER_TONE_HZ,
		CONFIG_BEEPER_TONE_MS, CONFIG_BEEPER_VOLUME_PERCENT);

	return 0;
}

void beeper_tone(uint32_t freq_hz, uint32_t duration_ms)
{
	struct tone_req req;

	if (!ready) {
		return;
	}

	req.freq_hz = CLAMP(freq_hz, 100U, SAMPLE_RATE_HZ / 2U);
	req.duration_ms = CLAMP(duration_ms, 10U, MAX_TONE_MS);

	/*
	 * K_NO_WAIT, so a full queue drops the request rather than blocking.
	 * This runs from an LVGL event callback on the main thread, and
	 * stalling that to wait for a speaker would drop frames.
	 */
	(void)k_msgq_put(&tone_q, &req, K_NO_WAIT);
}

void beeper_click(void)
{
	beeper_tone(CONFIG_BEEPER_TONE_HZ, CONFIG_BEEPER_TONE_MS);
}

void beeper_press(void)
{
	beeper_tone(CONFIG_BEEPER_PRESS_HZ, CONFIG_BEEPER_PRESS_MS);
}
