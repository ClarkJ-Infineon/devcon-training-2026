# PSOC Edge E84 Display / LVGL Lab — Zephyr Application

Smoke-test application for the PSOC Edge E84 display driver port that is
part of the `devcon-training-2026` branch on `ClarkJ-Infineon/zephyr`. It
exists to prove the ported `infineon_dc` display controller driver +
Waveshare 4.3" DSI panel driver (`waveshare,4p3`) work end-to-end on real
mainline Zephyr before attendees build on top of it.

**Target board:** KIT_PSE84_EVAL (PSOC Edge E84), with the **standard-issue
4.3" Waveshare DSI panel** that ships with the EVK (not the 7" panel — see
history below).

**Board target string:** `kit_pse84_eval/pse846gps2dbzc4a/m55`

## Why this driver needed porting

Mainline Zephyr has board support for `kit_pse84_eval` but never carried
over the `infineon_dc` MIPI-DSI display controller driver (DCNano8000) or
either Waveshare panel driver — they only ever existed on
`Infineon/ifx-zephyr:main` (commit `47b72d198ae7`), a public downstream
fork, and were never upstreamed. Porting them onto a clean mainline branch
(rather than staying on the downstream fork) is what lets this training
branch track `zephyrproject-rtos/zephyr:main` instead of Infineon's
internal release cadence. Full background:
`outputs/zephyr/engineering/devcon-training-branch/plan.md`.

Only the base driver + its Kconfig + DT binding were ported — **not** the
16 out-of-tree WiFi/audio/video patches from Tom Burdick's
`ifx-smart-thermostat` GitLab sample, which solves a much harder, unrelated
problem and is explicitly out of scope here. That sample's board overlay
was consulted only for reference DT timing/wiring values (never copied
wholesale — its memory map differs from current mainline's).

## Two bugs found and fixed during the port (see branch commit history)

1. **`gfx_mem` DT node missing from mainline.** `display_infineon_dc.c`
   hard-codes `DT_NODELABEL(gfx_mem)` for its framebuffer base address —
   this is a required integration contract, not a configurable DT binding
   property, and neither the original fork commit nor mainline's board
   files ever defined it. This app's `boards/*.overlay` adds a 2 MB
   `GFX_MEMORY` region in SOCMEM to satisfy it.
2. **Pre-existing mainline bug in `i2c_infineon_pdl.c`** (unrelated to the
   display port, but it blocks this app since the panel is wired over
   I2C0): `cy_stc_scb_i2c_master_xfer_config_t` no longer has a
   `continueXfer` member against the currently-pinned `hal_infineon`
   revision — it was folded into the existing `xferPending` field. This
   affects **every Infineon PDL I2C user in this training branch**,
   including CC2/C3M6, not just E84. Fixed on the branch; worth
   upstreaming separately.

## LVGL render mode — must be full-refresh, not partial

`display_infineon_dc.c`'s flush implementation only supports whole-frame
writes (it swaps the entire frame buffer pointer rather than copying
partial rects — confirmed via Burdick's `UPSTREAMING.md`). This app's
`prj.conf` therefore sets `CONFIG_LV_Z_FULL_REFRESH=y` (not the Zephyr
LVGL default of partial refresh). A full 832x480 RGB565 frame buffer is
~781 KB, too large for the M55 core's system RAM, so `prj.conf` also places
it in the `GFX_MEMORY` region via `CONFIG_LV_Z_VDB_ZEPHYR_REGION` +
`CONFIG_LV_Z_VDB_ZEPHYR_REGION_NAME="GFX_MEMORY"` — the same SOCMEM region
the overlay reserves for `gfx_mem`.

## Building — **must use `--sysbuild`**

```
# From a west workspace with the devcon-training-2026 manifest/branch checked out:
west build -p --sysbuild -b kit_pse84_eval/pse846gps2dbzc4a/m55 labs/e84-display-lab
```

A plain `west build` (no `--sysbuild`) still succeeds and produces a
`zephyr.hex`, but **that image can never run on real hardware.** The CM55
core is held in a ROM "safe" endless loop until the CM33 core calls
`Cy_SysEnableCM55()` — this board's `sysbuild.cmake` automatically adds
that release-stub as a companion image (`enable_cm55`, built from
`samples/basic/minimal` for the `m33` core) whenever the `m55` board target
is selected, but only if sysbuild is actually enabled for the build.
Without it, you get a clean-looking build that silently never boots.

Memory report from the last clean build (proxmox-build, mainline toolchain):

```
Memory region         Used Size  Region Size  %age Used
           FLASH:      348316 B      1536 KB     22.15%
             RAM:       30832 B       256 KB     11.76%
      GFX_MEMORY:        780 KB         2 MB     38.09%
```

## Flashing

**Do not trust `west flash` on this board/chip family.** It was found to
exit 0, report correct byte counts, and even produce a plausible-looking
post-reset UART boot banner on a run that was *later proven to have never
committed anything to flash* — the generated runner uses OpenOCD's
`flash write_image erase` + `verify_image`, which on this SMIF/QSPI part
can report success while reading back through a cache/echo of the write
buffer rather than persisted flash. (A sibling session flashing a
neighbouring PSOC Edge board, `kit_pse84_ai`, hit the identical failure
mode independently and is the reason this was caught here.)

The proven-safe recipe is one isolated OpenOCD session **per domain**,
nothing chained after the write:

```
openocd -s <openocd>/scripts \
  -c "set _ZEPHYR_BOARD_SERIAL <probe-serial>" \
  -f <board>/support/qspi_config.cfg \
  -f <board>/support/openocd.cfg \
  -c init -c "reset init" -c "program <hex> verify" -c shutdown
```

Flash `e84-display-lab/zephyr/zephyr.hex` (CM55) first, then
`enable_cm55/zephyr/zephyr.signed.hex` (CM33) — `domains.yaml`'s
`flash_order` — then run a **separate** OpenOCD session doing only
`init` → `reset run` → `shutdown` to boot the result.

**Why the explicit `-f .../qspi_config.cfg` matters, precisely:** the real
mechanism (confirmed against `kit_pse84_ai`'s identical setup) is
`target/infineon/cat1d/func_cat1d.tcl` around line 1398:

```tcl
catch {source [find qspi_config.cfg]}
if {![info exists SMIF_BANKS]} {
    global SMIF_BANKS
}
if {[info exists SMIF_BANKS]} {
```

Both routes below are supported **by design** — the comment just above that
block states `SMIF_BANKS` "might be provided either via the command line
parameters or in qspi_config.cfg file". They reach the final check differently:

1. **Explicit absolute `-f <path-to>/qspi_config.cfg` before `openocd.cfg`**
   (the recipe above). The file is sourced at **top level**, so `SMIF_BANKS`
   is a *global*. Inside the proc `[find qspi_config.cfg]` then fails and the
   `catch` swallows it, no local exists, and the `global SMIF_BANKS` fallback
   links the global in. Note this works *only* because of that fallback — a
   Tcl proc does not see globals implicitly.
2. **Add `-s <board>/support` to the search path** (alongside the generic
   `-s <openocd>/scripts`) — `[find qspi_config.cfg]` resolves and `source`
   runs *inside the proc*, creating a **local** that the `![info exists]`
   test then skips past. This is what `west flash`'s generated config
   effectively does.
3. **Set the variable directly** with `-c "set SMIF_BANKS { ... }"` at global
   scope, which reaches the check the same way as route 1. This only works in
   **dict form with `addr` and `size` keys** — see below.

**Route 3's value must be a dict, and the vendor file's own form explains
why.** `qspi_config.cfg` assigns a plain scalar:

```tcl
set SMIF_BANKS {
  1 {addr 0x60000000 size 0x4000000 psize 0x0000100 esize 0x0010000}
}
```

yet `func_cat1d.tcl` then calls `array size` / `array get` on it and
dereferences `$value(addr)`. Under standard Tcl 8.x that combination does not
work; it works here because OpenOCD embeds **Jim Tcl**, which treats a
dict-valued scalar as an array. Verified against this OpenOCD build:

```
array size = 1
key=1 addr=0x60000000 size=0x4000000
```

Both malformed forms are **reported**, which is the opposite of what a quick
`array size` test suggests. `array size` on an odd-length value returns 0
without complaint, but the script also calls `array get` on it at
`func_cat1d.tcl:1410`, inside a `catch` (`:1409-1418`) that prints
`Error: Error in parsing SMIF_BANKS definition`:

| Value passed | `array size` | What the script does |
|---|---|---|
| `{1 {addr 0x... size 0x...}}` (dict) | 1 | Correct — bank registers |
| A bare path, or any odd-length string | 0 | `array get` throws `missing value to go with key` — **reported** |
| `{0x60000000 0x4000000}` (address list) | 1 | `$value(addr)` throws `variable isn't array` — **reported** |

So you do not have to get the *form* right to avoid a silent failure; any
malformed value announces itself. Note the message goes to **stderr** and is
**non-fatal** — execution continues with no bank registered — so discarding or
not reading stderr turns a reported failure back into a silent one.

**The one genuinely silent case is a missing file, not a malformed value.** At
`:1398` the `catch` swallows a failed `source`, `SMIF_BANKS` is never set,
`:1403`'s `info exists` is false, and the entire block is skipped without a
word. That single case is the whole justification for route 2's `-s`.

So route 2's failure mode is attempting it without the board's support
directory on the search path: no warning, no global set either, and
`pse84xgxs2.cfg` proceeds with **no SMIF bank registered**. It is not a
source-*order* rule — it is "either set `SMIF_BANKS` globally, or make
`qspi_config.cfg` findable". Don't rely on just `-s <openocd>/scripts` alone,
since `qspi_config.cfg` lives under the **board's** support directory, not the
generic OpenOCD scripts tree.
**Cheap sanity check for any new invocation:**
add `-c "flash banks"` and confirm a `cat1d.cm33.smif1_ns` entry at
`0x60000000` appears before trusting a write — it verifies the outcome of the
final check regardless of which route got you there, and unlike the parse
error it cannot be lost by discarding stderr.

**`program <hex> verify`'s verify line is necessary but not sufficient —
it is not the verdict.** A sibling session found that on this chip
family, `verify_image`'s readback (and, plausibly, the same comparison
code backing `program`'s built-in `verify`) can pass against an XIP-cached
copy of the write rather than what's actually committed to the QSPI die —
they never got an independently-confirmed-bad write to test `program ...
verify` against, so its "OK" can't be trusted as proof, only its failure
can be trusted as a definitive "broken." Same logic applies to
`flash read_bank`/`dump_image`: it reported a byte-for-byte match against
a *stale* image, in a fully separate, non-chained invocation, repeatedly —
whatever path serves these debugger-side reads can carry the same
cached/stale state the write itself might not have escaped. **The actual
verdict has to come from outside the debugger** — a compile-time build
stamp on the UART console, cross-checked against something independent of
the flash/debug path (e.g. the rebuilt ELF's filesystem mtime), is what
closes the loop non-circularly. Treat `program`/`verify` as: fail means
definitely broken, pass means only "proceed to the real check," not
"done." Use `program`, never `write_image`, as the *write mechanism*
regardless (it sector-pads — 98304 vs 93904 bytes on a sibling's CM33
image — and the unpadded tail is what fails to commit) — but don't cite
its own verify line as the proof.

**If verifying via a build stamp (the actual verdict, per above), rebuild
clean.** An incremental build that decides the stamped source file is
unchanged will re-link the *old* `__DATE__`/`__TIME__` stamp, giving a
false-negative "stale flash" reading on a perfectly good write. Use a full
clean rebuild (`rm -rf <build-dir>` then `west build`) or
`west build -p always`.

**Pin the debug probe, and verify *where* the write went — not just what.**
With more than one KitProg3 attached, an unpinned OpenOCD selects by USB
enumeration order. It will then program, verify, and reset a *different* board
than the one whose UART you are reading, and report complete success for all
three. This is a nastier failure than a bad write, because every check passes:
byte counts are right, `** Verified OK **` appears, `reset run` logs normally —
and the board you are watching simply never changes. The symptom looks like a
hung target rather than a misdirected write, and the giveaway is that
successive capture logs are **byte-identical**.

`support/openocd.cfg` already supports pinning:

```tcl
if { [info exists _ZEPHYR_BOARD_SERIAL] } { adapter serial $_ZEPHYR_BOARD_SERIAL }
```

so pass `-c "set _ZEPHYR_BOARD_SERIAL <serial>"` **before** `-f openocd.cfg`, on
*every* invocation — programming, reset and monitor alike. Pinning only the
reset path is not enough. Confirm it took by checking the output for
`Info : Using CMSIS-DAPv2 interface with VID:PID=0x04b4:0xf155, serial=<serial>`,
and treat its absence as a hard failure rather than a warning.

Better still, derive the serial from the COM port you are about to capture
rather than hardcoding it, so the two cannot disagree. On Windows the probe
serial is the PnP parent of the UART:

```powershell
$dev = Get-PnpDevice -Class Ports -PresentOnly |
    Where-Object { $_.FriendlyName -match "\(COM64\)" }
(Get-PnpDeviceProperty -InstanceId $dev.InstanceId -KeyName 'DEVPKEY_Device_Parent').Data
# USB\VID_04B4&PID_F155\<your probe serial>
```

This derivation holds **only because a KitProg3's UART and CMSIS-DAP probe are
two interfaces of one composite device**, so the port's parent instance ID
carries the probe serial. Run the console over an external USB-serial adapter
instead and it resolves a serial unrelated to the board being watched —
reintroducing the exact failure it was meant to remove, and quietly. Check the
parent matches `VID_04B4&PID_F155` and refuse anything else, rather than
resolving a plausible-looking wrong answer.

A board-specific boot error is also a cheap identity assertion. This lab's EVK
carries the 4.3" Waveshare panel; a `waveshare_panel@45 not ready` on a clean
boot almost certainly means the image is running on hardware that has no such
panel — i.e. the wrong board — rather than indicating an I2C fault worth
debugging.

## Hardware validation status

**Flashed and confirmed running on a real E84 EVK, 2026-10-01 — reflashed
and re-verified the same day after discovering the above `west flash`
false positive.** Clark connected a physical board on COM64; OpenOCD live-detected it
as `PSE846GPS2DBZC4A`.

An initial `west flash` run exited 0 with correct byte counts, and a
follow-up UART capture even showed a Zephyr boot banner — this was
documented as a success. It wasn't: re-testing with an isolated
`program <hex> verify` reproduced a genuine blank read-back on the same
region, and the boot banner turned out to prove nothing specific (its hash
comes from the kernel commit, shared by every app built from it, not from
the app actually on the board). See "Flashing" above for the root causes
(echo-read verify, and a missing `qspi_config.cfg` source needed for SMIF
bank registration) and `build-sweep-results.md` for the full narrative.

To get a result that can't suffer from either false-positive mode, a
one-line build stamp was added to `src/main.c`
(`LOG_INF("e84-display-lab build stamp: " __DATE__ " " __TIME__);`, with
`CONFIG_LOG_MODE_IMMEDIATE=y` in `prj.conf` so it can't be lost to a
deferred log buffer), the image was rebuilt on the build server, and both
domains were reflashed with the safe recipe above — each reporting a
genuine `** Verified OK **` against the now-correctly-registered SMIF
bank. UART (COM64, 115200 8N1) then showed:

```
*** Booting Zephyr OS build 0032669b5247 ***
[00:00:01.010,000] <inf> e84_display_lab: e84-display-lab build stamp: Oct  1 2026 19:54:52
```

The stamp matches the rebuilt `zephyr.elf`'s filesystem mtime
(`2026-10-01 19:55:14 UTC`, 22s later) and did not exist before this
rebuild — conclusive, non-circular proof this exact image is genuinely
running on the physical board.

**Still outstanding:** continuous post-boot liveness beyond the initial
boot sequence wasn't independently re-confirmed (an OpenOCD `curstate`
query shortly after boot showed CM55 `halted`, most likely a side effect of
OpenOCD's own attach/examine sequence rather than a crash, but not chased
further to avoid disturbing probe state again). CC2 and C3M6 EVK remain
completely unvalidated on hardware (no boards for those connected to this
machine).

### Display showed no content — found and fixed, 2026-10-01

After the above was proven genuinely running, Clark reported the physical
panel showed **no content**. Root cause: a real bug in
`drivers/display/display_infineon_dc.c` (not this app, not devicetree) —
`ifx_dc_init()` read the panel's true resolution (800x480), then a later
call to `ifx_dc_get_capabilities()` mutated the same struct, padding the
width to 832 (a 128-byte line-stride boundary for the framebuffer). The
padded 832 value was then reused for the MIPI DSI active-video
width/height sent to the panel, instead of the panel's real 800 — the
panel's TCON could never sync to video that claimed the wrong width,
giving a blank screen with no error logs.

**Fixed, committed, and pushed:** `1dc2d45fcbf` on
`ClarkJ-Infineon/zephyr:devcon-training-2026`. Rebuilt (same memory
footprint), reflashed both domains, and re-confirmed genuinely running via
a fresh build-stamp-on-UART cross-check (`21:39:38`, matching the rebuilt
ELF's mtime 22s later). **Visual confirmation on the physical panel is the
one remaining open item** — not yet checked by Clark since this fix was
flashed. See `build-sweep-results.md` for the full writeup.

Full build-sweep results (with the same update) for all three boards:
`outputs/zephyr/engineering/devcon-training-branch/build-sweep-results.md`.
