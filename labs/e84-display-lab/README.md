# PSOC™ Edge E84 Display / LVGL Lab — Zephyr Application

Display smoke-test application for the PSOC™ Edge E84. It exercises the
`infineon_dc` MIPI-DSI display controller driver and the Waveshare 4.3" DSI
panel driver (`waveshare,4p3`), and serves as the known-good starting point
for the graphics labs.

**Target board:** KIT_PSE84_EVAL (PSOC™ Edge E84), with the 4.3" Waveshare DSI
panel supplied with the EVK.

**Board target string:** `kit_pse84_eval/pse846gps2dbzc4a/m55`

## Requirements this application satisfies

- **`gfx_mem` devicetree node.** `display_infineon_dc.c` takes its framebuffer
  base address from `DT_NODELABEL(gfx_mem)`. This is an integration contract
  rather than a configurable binding property, so `boards/*.overlay` defines a
  2 MB `GFX_MEMORY` region in SOCMEM.
- **Full-refresh LVGL rendering.** The driver's flush implementation swaps the
  frame buffer pointer rather than copying partial rectangles, so `prj.conf`
  sets `CONFIG_LV_Z_FULL_REFRESH=y` instead of the Zephyr LVGL default of
  partial refresh. Partial refresh produces a blank or corrupted panel.
- **Framebuffer placement.** A full 832x480 RGB565 frame buffer is about
  781 KB, which exceeds the M55 core's system RAM. `prj.conf` places it in the
  `GFX_MEMORY` region with `CONFIG_LV_Z_VDB_ZEPHYR_REGION` and
  `CONFIG_LV_Z_VDB_ZEPHYR_REGION_NAME="GFX_MEMORY"`.

## Building -- requires `--sysbuild`

```
west build -p always --sysbuild -b kit_pse84_eval/pse846gps2dbzc4a/m55 labs/e84-display-lab
```

The `m55` target is a dual-core build. The CM55 core is held in a ROM safe loop
until the CM33 core calls `Cy_SysEnableCM55()`, and the board's
`sysbuild.cmake` supplies that release stub as a companion image
(`enable_cm55`) only when sysbuild is enabled. A plain `west build` succeeds
and produces a `zephyr.hex`, but that image never boots.

## Rendering configurations

Three configurations are provided. All three produce the same UI; they differ
in how LVGL rasterises it.

| Configuration | Build arguments |
|---|---|
| Baseline -- C software rendering | *(none)* |
| Helium -- Armv8.1-M MVE assembly blend kernels | `-DEXTRA_CONF_FILE=helium.conf` |
| GPU -- GCNanoUltraV through vg_lite | `-DEXTRA_CONF_FILE=gpu.conf -DEXTRA_DTC_OVERLAY_FILE=gpu.overlay` |

The GPU configuration needs **both** the config and the overlay. The overlay
binds the GPU node and enlarges `GFX_MEMORY` from 2 MB to 3.5 MB to hold the
vg_lite command and tessellation heap.

```
west build -p always --sysbuild -b kit_pse84_eval/pse846gps2dbzc4a/m55 \
    labs/e84-display-lab -- \
    -DEXTRA_CONF_FILE=gpu.conf -DEXTRA_DTC_OVERLAY_FILE=gpu.overlay
```

Measured footprints:

| Region | Baseline | Helium | GPU |
|---|---:|---:|---:|
| FLASH | 346 KB | 471 KB | 598 KB |
| RAM | 28.0 KB | 28.0 KB | 14.4 KB |
| GFX_MEMORY | 780 KB / 2 MB | 1560 KB / 2 MB | 2840 KB / 3.5 MB |
| ITCM | 6.1 KB | 7.7 KB | 7.7 KB |

Helium and GPU enable `CONFIG_LV_Z_DOUBLE_VDB`, which is why `GFX_MEMORY`
doubles. The GPU configuration moves the LVGL memory pool out of system RAM
into `GFX_MEMORY`, which is why its RAM figure is lower.

## LVGL patches

All three configurations require the LVGL patches carried in the manifest
repository at `zephyr/patches/`. West applies them during `west update`.

| Patch | Required by | Effect if missing |
|---|---|---|
| `0001-guard-c-only-includes-from-assembly.patch` | All configurations | Build fails |
| `0002-include-sw-blend-private-before-helium.patch` | Helium and GPU | Build fails |
| `0003-skip-gpu-for-tiled-images-without-repeat.patch` | GPU | Builds, renders incorrectly |

Patch 0001 is needed even by the baseline build: LVGL compiles
`lv_blend_helium.S` unconditionally, and Zephyr's `lv_conf.h` pulls C
declarations into it that the assembler cannot parse. Without it the build
fails with `Error: bad instruction` in `lv_math.h`.

Patch 0002 only matters once the Helium backend is selected, where it fails
with `invalid use of incomplete typedef 'lv_draw_sw_blend_fill_dsc_t'`.

Patch 0003 is a runtime correctness fix rather than a build fix. The
GCNanoUltraV on this part reports `gcFEATURE_VG_IM_REPEAT_REFLECT` as
unavailable, so tiled image blits must fall back to software.

## Flashing

```
west flash
```

Built with `--sysbuild`, this programs both images in the required order: the
CM55 application first, then the CM33 `enable_cm55` companion.

## Expected result

The panel displays the LVGL demo UI and touch input is active. The console
(115200 8N1) shows the Zephyr boot banner followed by the application's
startup log.

Confirm the result from the board's behaviour rather than from the flash
command's exit status. If the command reports success but the panel and
console do not change, ask your instructor rather than reflashing repeatedly.