# DevCon 2026 FAE Training — West Manifest

West manifest repository for Infineon's DevCon 2026 FAE training event. It
bundles the hands-on lab applications and pins a trimmed, Infineon-only Zephyr
project list on top of `ClarkJ-Infineon/zephyr`, branch `devcon-training-2026`
(a mainline-based fork, not Infineon's downstream `ifx-zephyr` fork).

## Quick start

Run these in order. The last two cannot be moved earlier: both depend on files
that do not exist on disk until `west update` has finished.

```
pip install west
west init -m https://github.com/ClarkJ-Infineon/devcon-training-2026 devcon-ws
cd devcon-ws
west update --narrow
west sdk install -t arm-zephyr-eabi
pip install -r zephyr/scripts/requirements-base.txt imgtool
```

| Step | What it does |
|---|---|
| `pip install west` | Installs west itself. Reopen your terminal afterwards if `west` is not found. |
| `west init -m <url> devcon-ws` | Fetches only the manifest — a small file naming which repositories are needed and which commit of each. Seconds. |
| `west update --narrow` | Fetches those repositories. **This is the 1.0 GB**, roughly five minutes on a good connection. If it drops, run it again; it resumes. |
| `west sdk install -t arm-zephyr-eabi` | Installs the Arm toolchain. `west sdk` is provided by the Zephyr repository, so the command does not exist until the step above completes. |
| `pip install -r zephyr/scripts/requirements-base.txt imgtool` | Installs the Python packages the build uses to generate devicetree and Kconfig output and to sign images. `requirements-base.txt` lives inside the Zephyr repository, so this is likewise only possible afterwards. |

`--narrow` fetches just the pinned revision of each project rather than every
branch and tag. The manifest additionally sets `clone-depth: 1` on every
project, so each repository arrives at its single pinned commit with no
history. Full history for any project can be recovered later with
`git fetch --unshallow` inside it.

## PSOC™ Edge: point west at ModusToolbox™ OpenOCD

`kit_pse84_eval` flashes through the `openocd` runner, and the Zephyr SDK's
bundled OpenOCD does not ship a PSE84 target. Set this once per workspace,
substituting your ModusToolbox™ Programming Tools path:

```
west config build.cmake-args -- "-DOPENOCD=C:/Infineon/Tools/ModusToolboxProgtools-1.9/openocd/bin/openocd.exe -DOPENOCD_DEFAULT_PATH=C:/Infineon/Tools/ModusToolboxProgtools-1.9/openocd/scripts"
```

Note the forward slashes, which CMake expects even on Windows. If this is
missing, `west flash` stops before touching the board, so nothing is left in a
half-programmed state.

**The PSOC™ Control CAN lab does not need this.** `kit_psc3m6_evk` defaults to
the `jlink` runner and flashes through the board's onboard SEGGER J-Link with
a stock Zephyr toolchain. J-Link software **V9.68 or newer** is required.
Infineon OpenOCD remains available on that board as an alternate runner via
`west flash --runner openocd`.

## What this manifest pins

Upstream Zephyr's `west.yml` places every vendor HAL (`hal_infineon`,
`hal_st`, `hal_nxp`, ...) in a single shared `hal` group, so a `group-filter`
cannot select one vendor. This manifest therefore lists projects explicitly:

| Project | Purpose |
|---|---|
| `zephyr` | the training branch: mainline plus the E84 display driver port, PSOC™ Control C3M6 analog and timer support, and a handful of Infineon driver fixes |
| `hal_infineon` | PSOC™ Control and PSOC™ Edge PDL/HAL |
| `cmsis`, `cmsis_6` | ARM CMSIS core headers, required by `hal_infineon` |
| `lvgl` | graphics library for the E84 display and dashboard labs |

No `segger`, no other vendor HALs, and no babblesim, TF-M or testing-only
modules. `west update --narrow` fetches approximately **1.0 GB**, against
**7.9 GB** for upstream Zephyr's complete default manifest. Adding the Zephyr
SDK brings the workspace to roughly **2.5 GB** in total.

`zephyr` is pinned by commit SHA rather than branch name, so a given commit of
this manifest always resolves to one exact Zephyr tree.

## Zephyr branch contents

The pinned `devcon-training-2026` branch adds the following to mainline Zephyr:

**PSOC™ Control C3M6, for the CAN lab**

- MCPASS v3 SAR ADC support — an MFD register-layout addition, an ADC driver
  addition, the HPPASS and SAR ADC devicetree nodes for the SoC, and the board
  enablement. This is what makes the lab's potentiometer readable.
- TCPWM devicetree nodes for the SoC and board enablement, used by the lab's
  LED brightness output.
- A `jlink` runner for the board, so `west flash` works with a stock Zephyr
  toolchain and no separate Infineon OpenOCD installation.
- A flash load-address fix in `soc/infineon/psc3`. PSC3 exposes one physical
  flash through two address aliases: code executes from the CBUS alias, which
  is what `flash0` describes in devicetree, but the device is programmed
  through the SAHB alias, a constant `0x20000000` higher. Without the
  adjustment the erase succeeds and the program fails.

**PSOC™ Edge E84, for the display and dashboard labs**

- The `infineon_dc` MIPI-DSI display controller driver and the Waveshare DSI
  panel driver.
- VG-Lite GPU support for LVGL on E84, and a fix so the LVGL Helium sources are
  assembled only when that path is selected.
- `tlv320dac310x` treats `reset-gpios` as optional, which the E84 codec
  requires.

**Infineon driver fixes, not specific to one lab**

- `drivers/i2c/i2c_infineon_pdl.c` releases the caller's buffer on an error
  path, corrects the I2C recovery GPIO configuration, and no longer logs every
  abort timeout from the ISR.
- `drivers/serial/uart_infineon_pdl.c` caches `uart_config` only on success,
  and an IRQ-driven API hang and zero-length assert are fixed.
- The `infineon,tcpwm-pwm` binding marks `clocks` as required. A PWM node
  without a clock divider previously built cleanly and silently produced no
  output.
- The PDL DMA driver no longer reports a benign underrun as an error.
- `kit_psc3m5_cc2` user LED polarity is corrected.

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
the display lab, not only the Helium and GPU ones — see that lab's README.

## Labs

| Lab | Board | Path |
|---|---|---|
| PSOC™ Control — CAN Command & Telemetry | KIT_PSC3M6_EVAL | `labs/can-lab-{beginner,advanced,cheat}` |
| …production reference | KIT_PSC3M6_EVAL | `labs/can-lab-production` |
| PSOC™ Edge — E84 Touch UI dashboard | KIT_PSE84_EVAL (4.3" Waveshare panel) | `labs/e84-dashboard-{beginner,advanced,cheat}` |

The CAN lab ships three attendee tiers — `beginner`, `advanced` and `cheat` —
which differ only in how much of the code is left for you to write. Steps 1 and
2 are byte-identical across all three, so you can change tier mid-lab by
opening a different `src/` file. `can-lab-production` is not a tier: it is a
complete reference implementation with full error handling and the trigger-mux
LED back-end enabled by default.

Each lab has its own README with build commands and board target strings. Both
`west build` and `west flash` take `-d <build directory>`, so `west flash -d
build/lab` flashes whatever `west build -d build/lab` produced.

## Classroom guides

| Guide | Class | Path |
|---|---|---|
| Getting Started with Zephyr on PSOC™ | Software General Session (Zephyr) | `docs/psoc-zephyr-setup.md` |
| PSOC™ Control — Command & Telemetry over CAN | Zephyr on PSOC™ Control | `docs/psoc-control-can-lab.md` |
| …instructor companion | | `docs/psoc-control-can-lab-instructor.md` |
| PSOC™ Edge — A Touch Dashboard with LVGL | Zephyr on PSOC™ Edge | `docs/psoc-edge-lvgl-lab.md` |
| …instructor companion | | `docs/psoc-edge-lvgl-lab-instructor.md` |

Both advanced labs are self-contained and assume only the general session. The
two instructor companions are published alongside their lab guides rather than
held back.

## Limitations

- The board target for KIT_PSC3M6_EVAL is spelled `kit_psc3m6_evk` upstream,
  an inherited naming error from the earlier C3M5 EVK. Use `kit_psc3m6_evk` in
  build commands; the kit itself is KIT_PSC3M6_EVAL.
- Flashing KIT_PSC3M6_EVAL requires the `zephyr` revision pinned by this
  manifest. Earlier revisions lack the PSC3 flash load-address adjustment, and
  `west flash` fails with "Writing target memory failed" after a successful
  erase.
