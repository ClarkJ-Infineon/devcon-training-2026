# PSOC™ and Zephyr — Setting Up Your Workspace
### Session 1 Guide (DevCon FAE Training)

|  |  |
| --- | --- |
| **Applies to** | Every attendee — PSOC™ Edge, PSOC™ Control and PSOC™ 4100T Plus |
| **Duration** | 60 minutes |
| **Prerequisite** | None. This is the one you start with. |
| **You will leave with** | A working Zephyr workspace and your own board blinking |

---

## 1. What this session is for

The advanced sessions later this week assume you already have a working Zephyr
workspace. Building one means a 2.2 GB download and a handful of one-time
installs. Neither of those belongs in an hour where you are supposed to be
learning devicetree — and neither is interesting enough to spend that hour on.

So we do it now, together, in a room where somebody can look at your screen.

You leave with three things:

1. **west installed, and a Zephyr SDK your builds can find.**
2. **A workspace** holding the training fork of Zephyr and every lab
   application you will need this week.
3. **One blinky, built by you, flashed by you, running on your own board.**

That third one is the only proof that matters. The first two can look fine and
still be wrong; a blinking LED cannot.

> **If you only read one thing:** do not skip §7. Everything before it is
> installation, and installation that has never compiled anything is a guess.

---

## 2. What you need

| | |
| --- | --- |
| **Laptop** | Windows 10 or 11, with permission to install software |
| **Free disk space** | 5 GB — 2.2 GB for the workspace, the rest for build output |
| **Hardware** | The kit you were issued, plus its USB cable |
| **Network** | See the note below |

**On the network.** The workspace download is the single biggest thing that
happens this hour, and conference WiFi is the single biggest reason it fails.
If you have a wired connection or a phone hotspot, prefer it. The `--narrow`
flag in §5 exists specifically to make this download as small as it can be.

---

## 3. One-time tool installs

You need Git, Python, CMake, Ninja and 7-Zip. If you have been doing embedded
work on this laptop you probably have most of them already.

```
winget install Git.Git
winget install Python.Python.3.12
winget install Kitware.CMake
winget install Ninja-build.Ninja
winget install 7zip.7zip
```

**Close and reopen your terminal afterwards.** Installers add directories to
`PATH`, and a terminal that was already open will not see them. This is the
most common reason the next command appears to fail.

Then install west itself:

```
pip install west
```

Check all of it at once:

```
git --version
cmake --version
ninja --version
west --version
```

Four version numbers means you are done with this section. Anything reporting
*"not recognized as an internal or external command"* is a `PATH` problem —
reopen the terminal first, and if it persists, say so now.

---

## 4. Install and register the Zephyr SDK

The SDK is the cross-compiler — the thing that turns your C into ARM code.
Download **Zephyr SDK 1.0.1** for Windows from the
[sdk-ng releases page](https://github.com/zephyrproject-rtos/sdk-ng/releases)
and extract it into your home directory, so you end up with:

```
C:\Users\<you>\zephyr-sdk-1.0.1
```

Then run its setup script **once**:

```
cd %USERPROFILE%\zephyr-sdk-1.0.1
setup.cmd /t arm-zephyr-eabi /c
```

Two flags, and both matter:

- **`/t arm-zephyr-eabi`** installs the ARM toolchain. Every board in this
  week's training is a Cortex-M part, so this is the only one you need. If the
  bundle you downloaded already contains it, this is a no-op.
- **`/c`** registers the SDK as a CMake package. This is the one that saves you
  grief: once registered, **every Zephyr build on this machine finds the
  toolchain by itself**, in any terminal, forever. Without it you would have to
  set `ZEPHYR_SDK_INSTALL_DIR` by hand in every new shell — and the day you
  forget, the build fails with an error that does not mention the SDK at all.

A successful run ends with:

```
Registering Zephyr SDK CMake package ...
Zephyr-sdk (C:/Users/<you>/zephyr-sdk-1.0.1/cmake)
has been added to the user package registry
All done.
```

> If `setup.cmd` stops immediately saying it requires `cmake` or `7z`, go back
> to §3. Those are the only two external programs it checks for.

---

## 5. Create the workspace

This is the download. Four commands, run from wherever you keep your projects:

```
west init -m https://github.com/ClarkJ-Infineon/devcon-training-2026 devcon-ws
cd devcon-ws
west update --narrow
west patch apply
```

What each one actually does:

| Command | What happens |
| --- | --- |
| `west init -m <url> devcon-ws` | Creates a `devcon-ws` folder and fetches **only the manifest** — a small file listing which repositories this training needs and exactly which commit of each. Seconds, not minutes. |
| `west update --narrow` | Fetches those repositories. This is the 2.2 GB, and the part that takes a while. |
| `west patch apply` | Applies three small fixes to LVGL that the graphics lab depends on. |

**`--narrow` is not optional advice.** Without it, west fetches every branch
and every tag of every repository rather than just the one commit the manifest
pins. It is the difference between a download that finishes during this session
and one that does not.

**`west patch apply` prints very little, and that is correct.** A quiet run
means the patches applied cleanly. If it reports a failure, flag it now — the
graphics lab will not build without it, and otherwise you will not find that
out until the advanced session.

### What you just downloaded, and why it is only 2.2 GB

Upstream Zephyr's default manifest pulls in every vendor's hardware support —
ST, NXP, Nordic, Espressif, all of it — because upstream has no way to ask for
one vendor's worth. Downloading all of that would be roughly 7.9 GB, and you
would use none of it this week.

The manifest you just used lists its projects explicitly instead: the training
fork of Zephyr, Infineon's HAL, ARM's CMSIS headers, and LVGL for the graphics
lab. Nothing else. That is the **2.2 GB** on your disk, against **7.9 GB** for
the default — a reduction of roughly **70%**, almost all of it saved on this
conference's WiFi.

### What is in the folder

```
devcon-ws/
├── zephyr/                     the training fork of Zephyr itself
├── modules/                    hal_infineon, cmsis, lvgl
└── devcon-training-2026/
    ├── labs/                   every lab application, all boards
    ├── docs/                   the guides, including this one
    └── west.yml                the manifest
```

You will spend the advanced sessions inside `labs/`. Everything else is
machinery.

---

## 6. Find your board

One USB cable, into the **KitProg3** connector on your kit — not the other USB
connector, which is a device port and will not program anything.

Then open Device Manager and look under **Ports (COM & LPT)**. You want a new
COM port that was not there before you plugged in. **Write the number down.**
You will need it in every session this week, and it is different on every
laptop.

Open a serial terminal on that port at **115200 8N1**. PuTTY, TeraTerm and the
terminal built into your editor are all fine.

> **If no COM port appears**, try a different USB cable before you try anything
> else. A surprising share of USB cables are charge-only and carry no data
> lines at all. This costs people more time than any other single thing in this
> session.

---

## 7. Build and flash blinky

Find your board in this table. It gives you the **board target** — the string
Zephyr uses to identify your exact hardware.

| Your kit | Board target | Clean build |
| --- | --- | --- |
| **CY8CPROTO-041TP** (PSOC™ 4100T Plus) | `cy8cproto_041tp` | about 2 minutes |
| **KIT_PSC3M5_CC2** (PSOC™ Control C3M5) | `kit_psc3m5_cc2` | about 1½ minutes |
| **KIT_PSE84_EVAL** (PSOC™ Edge E84) | `kit_pse84_eval/pse846gps2dbzc4a/m55` | about 4 minutes |

From inside `devcon-ws`, build the standard Zephyr blinky sample for your
board. Substitute your board target for `<board>`:

```
west build -b <board> -d build/blinky -s zephyr/samples/basic/blinky
```

**PSOC™ Edge attendees — you need one extra flag.** The E84 has two processor
cores, and the one you are targeting is started by the other one. Zephyr builds
both images together, and that needs `--sysbuild`:

```
west build --sysbuild -b kit_pse84_eval/pse846gps2dbzc4a/m55 -d build/blinky -s zephyr/samples/basic/blinky
```

Without `--sysbuild` the build still *succeeds*, which is the trap — it just
produces an image with nothing to start it, and the board sits there doing
nothing. Remember this; it comes back in the advanced session.

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

**An LED on your board should now be blinking.** That is the whole point of the
session. If it is blinking, you are ready for the rest of the week.

> **Why blinky and not something more interesting?** Because it is the smallest
> program that proves the entire chain — toolchain, board definition, your
> workspace, the programmer, and the board itself. Every one of those can be
> broken in a way that looks fine until something tries to use it. Blinky tries
> to use all of them.

---

## 8. If something went wrong

| What you see | What it means |
| --- | --- |
| `'west' is not recognized` | `pip install west` did not land on `PATH`. Reopen your terminal. If it still fails, try `python -m west` to confirm it installed at all. |
| `Unable to find a Zephyr SDK` | §4's `/c` registration did not happen, or you registered a different SDK folder than the one you extracted. Re-run `setup.cmd /c`. |
| `ERROR: board <name> not found` | Usually a typo in the board target — they are long, and the E84 one especially so. Copy it from the table rather than typing it. Otherwise you are running from outside `devcon-ws`. |
| `CMake Error ... does not contain a CMakeLists.txt` | The `-s` path is wrong, almost always because you are not in `devcon-ws`. Check where you are with `cd`. |
| Build succeeds, `west flash` fails | A programmer problem, not a build problem. Check the COM port still appears in Device Manager, and that no serial terminal is holding the port open — close your terminal and retry. |
| Build succeeds, flash succeeds, no LED | On E84, you almost certainly left out `--sysbuild`. Rebuild with it. On other boards, flag it. |
| `west update` fails partway | Network. Run `west update --narrow` again — it resumes rather than starting over. |

If you are stuck for more than a few minutes, say so out loud. Everything in
this session is a prerequisite for everything later in the week, and the worst
outcome is discovering a broken workspace at the start of your advanced
session.

---

## 9. What happens next

Later this week you choose **one** advanced session. Both run at the same time,
so you cannot attend both.

**PSOC™ Control — CAN Command and Telemetry.** Two boards talking to each
other over CAN. You bring up an ADC channel and a PWM output in devicetree,
then wire them into a message protocol, and watch one board's knob move the
other board's LED.

**PSOC™ Edge — A Touch Dashboard with LVGL.** A touchscreen dashboard on the
E84's 4.3" panel: colour sliders driving real LED brightness, a live tilt meter
from the on-board accelerometer, and audio feedback on touch. You work across
devicetree, Kconfig and application code.

Both sessions start from the workspace you just built. Nothing else is
installed on the day and no further downloads happen in the room — which is
exactly why this session exists.

You do not need to decide anything else now. Each advanced session hands out
its own guide and tells you which application directory to build. There is
nothing to prepare beyond what you have done in this hour.
