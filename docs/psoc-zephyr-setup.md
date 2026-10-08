# Getting Started with Zephyr on PSOC™
### Software General Session (Zephyr) — DevCon FAE Training

|  |  |
| --- | --- |
| **Applies to** | Every attendee — PSOC™ Edge, PSOC™ Control and PSOC™ 4100T Plus |
| **Duration** | 2 hours, mostly presentation |
| **Before you arrive** | Run the prework installer — see §2 |
| **You will leave with** | A working Zephyr workspace and your own board blinking |

---

## 1. What this session is for

Most of this session is a presentation: what Zephyr is, how it is put together, and why it looks the way it does. The hands-on part is smaller, but it has a hard deadline — the advanced sessions later this week assume you walk in with a working workspace, and there is no time in those hours to build one.

There is one scheduling fact that shapes everything below. **Setting up a Zephyr workspace means downloading about 2.5 GB**, and no amount of cleverness makes that instant. So you start the download in the first few minutes, leave it running while the presentation happens, and come back to it.

**Do not wait to be told to start.** §3 is the first thing you do when you sit down.

You leave with three things:

1. **A workspace** holding the training fork of Zephyr and every lab application you will need this week.
2. **A toolchain** your builds can find, and a programmer that can reach your board.
3. **One blinky, built by you, flashed by you, running on your own board.**

That third one is the only proof that matters. The first two can look perfectly healthy and still be wrong; a blinking LED cannot.

---

## 2. Before you arrive

### The prework installer

Zephyr needs eight command-line tools on Windows. Installing them by hand is tedious and several of the usual routes do not work on a managed laptop, so there is a script:

**`https://gitlab.intra.infineon.com/JarvisC/zephyr-windows-training-prereqs`**

Download or clone it, then from a **regular, non-admin** PowerShell window:

```
powershell -ExecutionPolicy Bypass -File .\setup-zephyr-windows-deps.ps1
```

Close and reopen your terminal when it finishes. Everything it installs goes into your user profile — no admin rights, no registry changes.

**You are done when the verification table shows eight `[OK]` lines:**

```
==> Verifying installed tools
  [OK] cmake    cmake version 4.4.2
  [OK] ninja    1.13.2
  [OK] python   Python 3.12.10
  [OK] git      git version 2.55.0.windows.5
  [OK] wget     GNU Wget 1.21.4 built on mingw32.
  [OK] 7z
  [OK] gperf    GNU gperf 3.3
  [OK] dtc      Version: DTC 1.7.2
```

If any line says `[MISSING]`, reopen your terminal and run the script once more — it skips what is already installed and retries only what is not. The repo's README covers the handful of things that can go wrong; work through it before the session rather than during it.

### Your debug probe software

Two different probes are used across this week's kits, and between them the prerequisites are already covered — both of these are installed on every laptop in the room for the ModusToolbox™ classes at this event. There is nothing to install here.

| Your kit | On-board probe | Zephyr flashes it with |
| --- | --- | --- |
| **CY8CPROTO-041TP** | KitProg3 | Infineon's OpenOCD |
| **KIT_PSE84_EVAL** | KitProg3 | Infineon's OpenOCD |
| **KIT_PSC3M6_EVAL** | Onboard SEGGER J-Link | SEGGER J-Link |

Confirm whichever applies to you, if you like:

```
dir C:\Infineon\Tools\ModusToolboxProgtools-1.9\openocd\bin\openocd.exe
```

It is worth knowing *why* the OpenOCD one matters, because this catches people. The copy of OpenOCD that comes with the Zephyr toolchain is the generic upstream build — it knows about PSOC™ 4 and PSOC™ 6, but it has no target support for PSOC™ Edge and no KitProg3 support either. Infineon's build has all of it, and you point Zephyr at it with one command in §5.

The J-Link path needs no such pointing. Zephyr finds J-Link on its own, so PSOC™ Control attendees have nothing to configure — but the **version** does matter. KIT_PSC3M6_EVAL needs **J-Link V9.68 or later**. Older installations cannot describe this device's debug architecture and fail at flash time with:

```
Unsupported value for 'Type' parameter
```

**Check the version with this, not by browsing to `C:\Program Files\SEGGER\JLink\`.** SEGGER installs each release into its own versioned folder, so a machine can easily carry half a dozen at once — and Zephyr does not pick the newest. It reads the install path from the registry, which SEGGER sets to **whichever version was installed last**. That is the one your flash will use, so that is the one to check:

```
(Get-Item "$((Get-ItemProperty HKCU:\Software\SEGGER\J-Link).InstallPath)\JLink.exe").VersionInfo.ProductVersion
```

If that reports anything below 9.68, install the current version from SEGGER before the session — and if you have several versions installed, install the newest one **last**, so the registry points at it. This is the one tooling check a PSOC™ Control attendee cannot skip, and it is much cheaper to do now than in the room.

---

## 3. First thing in the room: start the download

Do this before the presentation starts. It runs unattended, and it is the long pole.

The prework script installed Zephyr's dependencies but not **west**, Zephyr's own command-line tool. Install it now:

```
pip install west
```

Then, from wherever you keep your projects:

```
west init -m https://github.com/ClarkJ-Infineon/devcon-training-2026 devcon-ws
cd devcon-ws
west update --narrow
```

| Command | What happens |
| --- | --- |
| `west init -m <url> devcon-ws` | Creates a `devcon-ws` folder and fetches **only the manifest** — a small file naming which repositories this training needs and exactly which commit of each. Seconds, not minutes. |
| `west update --narrow` | Fetches those repositories. **This is the 1.0 GB**, and takes roughly five minutes on a good connection. Leave it running. |

**`--narrow` is not optional advice.** Without it, west fetches every branch and every tag of every repository instead of just the one commit the manifest pins. It is the difference between a download that finishes during this session and one that does not.

Once `west update` is running, **leave that window alone and go and listen.** Nothing else in this guide can proceed until it finishes, and watching it will not speed it up.

> **If it fails partway through**, just run `west update --narrow` again. It resumes rather than starting over, so a dropped connection costs you only what was in flight.

### Why it is only 1.0 GB

Two things are working here, and they are independent.

**The project list is trimmed.** Upstream Zephyr's default manifest pulls in every vendor's hardware support — ST, NXP, Nordic, Espressif, all of it — because upstream has no way to ask for one vendor's worth. That is roughly 7.9 GB, and you would use none of it this week. The manifest you just used names its projects explicitly instead: the training fork of Zephyr, Infineon's hardware abstraction layer, ARM's CMSIS headers, and LVGL for the graphics lab. Nothing else.

**No history is fetched.** Every project in the manifest sets `clone-depth: 1`, so each repository arrives as the single pinned commit with no history behind it. You are building against that exact commit, so the history buys you nothing in this room. If you later want it for a given project, `git fetch --unshallow` inside that project recovers it.

Together: **1.0 GB instead of 7.9 GB**, almost all of it saved on this room's WiFi.

---

## 4. When the download finishes: install the toolchain

You now have Zephyr's source. You do not yet have a compiler.

The **Zephyr SDK** is that compiler. It is *not* part of the manifest you just downloaded and it is not something Infineon provides — it is the Zephyr project's own cross-toolchain, and it is a second download of about 1.5 GB.

Run this from inside `devcon-ws`:

```
west sdk install -t arm-zephyr-eabi
```

That one command does four things worth knowing about:

- **Picks the right version by itself.** It reads the SDK version this fork of Zephyr was built against and installs exactly that. No version matching by hand, and no chance of installing one that almost works.
- **Downloads only the ARM toolchain.** `-t arm-zephyr-eabi` is doing real work here. Every board this week is a Cortex-M part; without that flag you would download toolchains for a dozen architectures you will never compile for.
- **Installs the host tools**, including the generic OpenOCD. Leave this alone — it costs about half the download, but skipping it causes problems that are much harder to diagnose than they are to avoid.
- **Registers itself**, so that **every Zephyr build on this machine finds the toolchain by itself**, in any terminal, from now on. There is no environment variable to set and nothing to remember.

> **Why this could not have been started earlier.** `west sdk` is a command that Zephyr itself provides, so west does not know it exists until the Zephyr repository is on your disk. Before `west update` finishes, the command simply is not there:
>
> ```
> west: unknown command "sdk"
> ```
>
> That is why the two downloads are sequential rather than parallel, and why §3 has to start the moment you sit down.

Check it landed:

```
west sdk list
```

You want to see version `1.0.1`, a path, `hosttools: installed`, and `arm-zephyr-eabi` under the installed toolchains.

---

## 5. Finish the workspace

Three short commands, all still from inside `devcon-ws`.

**First, install Zephyr's Python dependencies:**

```
pip install -r zephyr/scripts/requirements-base.txt imgtool
```

Zephyr's build is driven by Python, and these are the packages it needs to generate devicetree and Kconfig output, build, and sign images. They could not be installed earlier — `requirements-base.txt` is inside the Zephyr repository and does not exist on disk until `west update` finishes.

**Second, apply the graphics patches:**

```
west patch apply
```

This applies three small fixes to LVGL that the graphics lab depends on. It prints very little, and a quiet run means they applied cleanly. If it reports a failure, flag it now — otherwise the first person to find out will be an Edge attendee on the day of their lab.

**Third, point Zephyr at Infineon's OpenOCD:**

```
west config build.cmake-args -- "-DOPENOCD=C:/Infineon/Tools/ModusToolboxProgtools-1.9/openocd/bin/openocd.exe -DOPENOCD_DEFAULT_PATH=C:/Infineon/Tools/ModusToolboxProgtools-1.9/openocd/scripts"
```

This is the §2 point made concrete: it tells your builds to flash with Infineon's OpenOCD rather than the generic one the SDK installed. The path is where the Programming Tools install by default, so it should be right as typed — note the **forward slashes**, which CMake wants even on Windows.

It is a workspace setting, so you type it once and it applies to every build in `devcon-ws` for the rest of the week.

> **Run it even if your kit is KIT_PSC3M6_EVAL.** That board flashes through J-Link and ignores this setting, so it costs you nothing — and it means everyone in the room runs the same command and ends the session in the same state.

> **If you skip this** and your kit flashes through OpenOCD, builds still succeed and `west flash` fails before it touches the board. That is the good outcome: it fails safely rather than leaving you with a half-programmed device.

### What you now have

```
devcon-ws/
├── zephyr/                     the training fork of Zephyr itself
├── modules/                    hal_infineon, cmsis, lvgl
└── devcon-training-2026/
    ├── labs/                   every lab application, all boards
    ├── docs/                   the guides, including this one
    └── west.yml                the manifest
```

You will spend the advanced sessions inside `labs/`. Everything else is machinery.

---

## 6. Your board

One USB cable, into the debug connector on your kit — marked **KitProg3** on CY8CPROTO-041TP and KIT_PSE84_EVAL, and the **debug USB** connector on KIT_PSC3M6_EVAL. Not the other USB connector, which is a device port and will not program anything.

Open Device Manager and look under **Ports (COM & LPT)** for a port that was not there before you plugged in. **Write the number down.** You will need it in your advanced session, and it is different on every laptop.

Open a serial terminal on that port at **115200 8N1**. PuTTY, TeraTerm and the terminal built into your editor are all fine.

> **If no COM port appears**, try a different USB cable before you try anything else. A surprising share of USB cables are charge-only and carry no data lines at all. This costs people more time than anything else in this session.

---

## 7. Build and flash blinky

Find your kit in this table. The **board target** is the string Zephyr uses to identify your exact hardware.

| Your kit | Board target | Clean build |
| --- | --- | --- |
| **CY8CPROTO-041TP** (PSOC™ 4100T Plus) | `cy8cproto_041tp` | about 2 minutes |
| **KIT_PSC3M6_EVAL** (PSOC™ Control C3M6) | `kit_psc3m6_evk` | about 1½ minutes |
| **KIT_PSE84_EVAL** (PSOC™ Edge E84) | `kit_pse84_eval/pse846gps2dbzc4a/m55` | about 4 minutes |

> **PSOC™ Control attendees — `kit_psc3m6_evk` is not a typo.** The kit is KIT_PSC3M6_EVAL; the board target in Zephyr still ends in `evk`, carried over from the earlier C3M5 kit. Type the target exactly as shown. The mismatch has been raised with the Zephyr platform team.

From inside `devcon-ws`, build Zephyr's standard blinky sample, substituting your board target for `<board>`:

```
west build -b <board> -d build/blinky -s zephyr/samples/basic/blinky
```

**PSOC™ Edge attendees — you need one extra flag.** The E84 has two processor cores, and the one you are targeting is started by the other one. Zephyr has to build both images together, which is what `--sysbuild` asks for:

```
west build --sysbuild -b kit_pse84_eval/pse846gps2dbzc4a/m55 -d build/blinky -s zephyr/samples/basic/blinky
```

Leave `--sysbuild` out and the build still *succeeds* — that is the trap. It produces an image with nothing to start it, and the board sits there doing nothing at all. Remember this one; it comes back in your advanced session.

A successful build ends with a memory report:

```
Memory region         Used Size  Region Size  %age Used
           FLASH:       14788 B       128 KB     11.28%
             RAM:        4432 B        32 KB     13.53%
```

Now flash it:

```
west flash -d build/blinky
```

**An LED on your board should now be blinking.** That is the finish line for this session.

> **Why blinky, and not something more interesting?** Because it is the smallest program that exercises the entire chain — toolchain, board definition, workspace, programmer and the board itself. Every one of those can be broken in a way that looks fine until something actually uses it. Blinky uses all of them, which is exactly why it is worth your time.

---

## 8. If something went wrong

| What you see | What it means |
| --- | --- |
| `'west' is not recognized` | `pip install west` did not land on `PATH`. Reopen your terminal. If it still fails, `python -m west` will tell you whether it installed at all. |
| `west: unknown command "sdk"` | `west update` has not finished, or you are not inside `devcon-ws`. The `sdk` command comes from the Zephyr repository itself. |
| `Unable to find a Zephyr SDK` | `west sdk install` did not complete. Re-run it — it skips what is already downloaded. |
| `ERROR: board <name> not found` | Usually a typo in the board target. They are long, and the E84 one especially so — copy it from the table rather than typing it. Otherwise you are running from outside `devcon-ws`. |
| `CMake Error ... does not contain a CMakeLists.txt` | The `-s` path is wrong, almost always because you are not in `devcon-ws`. |
| Build succeeds, `west flash` cannot find OpenOCD | The §5 `west config` line is missing or its path is wrong. Check the path exists, and that you used forward slashes. |
| Build succeeds, flash succeeds, no LED | On E84, you almost certainly left out `--sysbuild`. Rebuild with it. On any other board, flag it. |
| `west update` stops partway | Network. Run `west update --narrow` again; it resumes. |

If you are stuck for more than a few minutes, say so out loud. Everything here is a prerequisite for everything later in the week, and the worst possible outcome is finding out your workspace is broken at the start of your advanced session, when there is no time to fix it.

---

## 9. What happens next

Later this week you choose **one** advanced session. They run at the same time, so you cannot attend both.

**Zephyr on PSOC™ Control — CAN Command and Telemetry.** Two boards talking to each other over CAN. You bring up an ADC channel and a PWM output in devicetree, wire them into a message protocol, and watch one board's potentiometer drive the other board's LED.

**Zephyr on PSOC™ Edge — A Touch Dashboard with LVGL.** A touchscreen dashboard on the E84's 4.3" panel: colour sliders driving real LED brightness, a live tilt meter from the on-board accelerometer, and audio feedback on touch. You work across devicetree, Kconfig and application code.

Both start from the workspace you built today. Nothing further is installed on the day and no further downloads happen in the room — which is the entire reason this session exists.

There is nothing else to decide or prepare now. Each advanced session hands out its own guide and tells you exactly which application directory to build.
