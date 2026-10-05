# DevCon 2026 FAE Training — West Manifest

West manifest repository for Infineon's DevCon 2026 FAE training event. It
bundles the hands-on lab applications and pins a trimmed, Infineon-only Zephyr
project list on top of `ClarkJ-Infineon/zephyr`, branch `devcon-training-2026`
(a mainline-based fork, not Infineon's downstream `ifx-zephyr` fork).

## Quick start

```
pip install west
west init -m https://github.com/ClarkJ-Infineon/devcon-training-2026 devcon-ws
cd devcon-ws
west update --narrow
```

`--narrow` fetches just the pinned revision of each project rather than every
branch and tag, and is recommended for conference WiFi.

## PSOC Control boards: point west at ModusToolbox OpenOCD

Both `kit_psc3m5_cc2` and `kit_psc3m5_evk` flash through the `openocd` runner,
and the Zephyr SDK's bundled OpenOCD does not ship a PSC3 target. Set this once
per workspace, substituting your ModusToolbox programming tools path:

```
west config build.cmake-args -- "-DOPENOCD=<progtools>/openocd/bin/openocd.exe -DOPENOCD_DEFAULT_PATH=<progtools>/openocd/scripts"
```

With that set, `west flash` works for every lab in this repository; no manual
programming steps are needed. If it is missing, `west flash` stops before
touching the board, so nothing is left in a half-programmed state.

## What this manifest pins

Upstream Zephyr's `west.yml` places every vendor HAL (`hal_infineon`,
`hal_st`, `hal_nxp`, ...) in a single shared `hal` group, so a `group-filter`
cannot select one vendor. This manifest therefore lists projects explicitly:

| Project | Purpose |
|---|---|
| `zephyr` | the training branch: mainline plus the E84 display driver port and a handful of Infineon driver fixes |
| `hal_infineon` | PSOC™ Control and PSOC™ Edge PDL/HAL |
| `cmsis`, `cmsis_6` | ARM CMSIS core headers, required by `hal_infineon` |
| `lvgl` | graphics library for the E84 display and dashboard labs |

No `segger`, no other vendor HALs, and no babblesim, TF-M or testing-only
modules. Download footprint is approximately **2.6 GB**, against **7.9 GB**
for upstream Zephyr's complete default manifest — a **~67% reduction**.

## Zephyr branch contents

The pinned `devcon-training-2026` branch adds the following to mainline Zephyr:

- The `infineon_dc` MIPI-DSI display controller driver and the Waveshare DSI
  panel drivers, required by the E84 display lab.
- A fix to `drivers/i2c/i2c_infineon_pdl.c`, where the `continueXfer` field no
  longer exists in the pinned PDL revision. This applies to every Infineon PDL
  I2C user, not only the E84 labs.
- A flash-runner fix for `kit_psc3m5_cc2`. On mainline this board flashes with
  the `jlink` runner, which cannot program it: Zephyr links at the CBUS secure
  alias `0x12000000`, while the SEGGER loader exposes a single bank at
  `0x22000000`, so the chip erase succeeds and the program fails. The board now
  defaults to the `openocd` runner, as `kit_psc3m5_evk` already does.
- Driver fixes the E84 dashboard lab depends on: the `infineon,tcpwm-pwm`
  binding now marks `clocks` required (a PWM node without a clock divider
  builds cleanly and silently produces no output); `i2c_infineon_pdl` releases
  the caller buffer on an error path and no longer logs every abort timeout
  from the ISR; `tlv320dac310x` treats `reset-gpios` as optional, which the
  E84 codec requires; and the PDL DMA driver no longer reports a benign
  underrun as an error.

## E84 labs require `--sysbuild`

The E84 `m55` board target is a dual-core build and must be built with
`west build --sysbuild`. The CM55 core starts only once a CM33 companion image
calls `Cy_SysEnableCM55()`; a plain `west build` succeeds but produces an image
that never boots. `west flash` programs both images once built with sysbuild.

See each E84 lab's own README for the full build and flash commands.

## Patches

Three LVGL fixes required by the E84 display lab are applied with
`west patch` rather than carried in a fork of LVGL. After `west update`:

```
west patch apply
```

| Patch | Purpose |
|---|---|
| `0001-guard-c-only-includes-from-assembly` | Allows the Helium assembly sources to include `lvgl_public.h`. |
| `0002-include-sw-blend-private-before-helium` | Corrects an include order that leaves `lv_draw_sw_blend_dsc_t` incomplete. |
| `0003-skip-gpu-for-tiled-images-without-repeat` | Routes tiled images to the software renderer on GPUs without pattern repeat support. |

Definitions are in `zephyr/patches.yml`; the patch files are under
`zephyr/patches/lvgl/`. Patch 0001 is required by every configuration of
the display lab, not only the Helium and GPU ones -- see that lab's README.

## Labs

| Lab | Board(s) | Path |
|---|---|---|
| PSOC™ Control CAN Command & Telemetry (4 tiers) | KIT_PSC3M5_CC2 / KIT_PSC3M5_EVK | `labs/can-lab-{cheat,beginner,advanced,production}` |
| PSOC™ Edge E84 Touch UI dashboard (3 tiers) | KIT_PSE84_EVAL (4.3" Waveshare panel) | `labs/e84-dashboard-{cheat,beginner,advanced}` |

Each lab has its own README with build commands and board target strings. Both
`west build` and `west flash` take `-d <build directory>`, so `west flash -d
build/lab` flashes whatever `west build -d build/lab` produced.

## Classroom guides

| Guide | Session | Path |
|---|---|---|
| Getting Started with Zephyr on PSOC™ | 1 — intro, all attendees | `docs/psoc-zephyr-setup.md` |
| PSOC™ Control — Command & Telemetry over CAN | 2 — advanced | `docs/psoc-control-can-lab.md` |
| …instructor companion | | `docs/psoc-control-can-lab-instructor.md` |
| PSOC™ Edge — A Touch Dashboard with LVGL | 3 — advanced | `docs/psoc-edge-lvgl-lab.md` |
| …instructor companion | | `docs/psoc-edge-lvgl-lab-instructor.md` |

Both advanced labs are self-contained and assume only the intro session. The
two instructor companions are published alongside their lab guides rather than
held back.

## Limitations

- On KIT_PSC3M5_CC2, flashing requires the `zephyr` revision pinned by this
  manifest. Earlier revisions default to the `jlink` runner, which cannot
  program the secure flash alias the image is linked at, and `west flash`
  fails with "Writing target memory failed" after a successful erase.