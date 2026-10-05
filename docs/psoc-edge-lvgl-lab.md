# PSOC™ Edge — A Touch Dashboard with LVGL
### Advanced Zephyr Lab Guide (DevCon FAE Training, Session 3)

|  |  |
| --- | --- |
| **Board** | KIT_PSE84_EVAL (PSOC™ Edge E84), 4.3" MIPI-DSI panel with capacitive touch |
| **Duration** | 60 minutes |
| **Prerequisite** | A working workspace from the intro session — see §3 |
| **You will touch** | Devicetree, Kconfig, an out-of-tree module, and application code |
| **Status** | Hardware-validated end to end on KIT_PSE84_EVAL |

---

## 1. Why this lab is shaped the way it is

You have all run a Zephyr sample before. Running a sample teaches you that
Zephyr works. It does not teach you how a Zephyr application is actually
assembled, because the sample has already made every decision for you.

This hour is about the three places those decisions live. **Devicetree** says
what hardware exists. **Kconfig** says what software gets built. **Application
code** says what any of it is for. The thing that catches people — and the
reason this lab is shaped the way it is — is that all three have to agree, and
when they disagree the board usually says nothing at all.

You are going to make them disagree. Several times. That is the lab. Each time,
the question to ask is not "what did I type wrong" but "which of the three
layers does not know about the other two."

There are three checkpoints. At each one you build, flash, and get something
you can see or hear. If you fall behind, there is a complete working copy of
the application you can take any step from — and no, that is not cheating.
There is a whole tier named after it.

---

## 2. What you're building

A single-screen touch dashboard on the E84's 4.3" panel:

- **Three colour sliders** — red, green and blue — each driving one of the
  board's user LEDs at real PWM brightness.
- **A tilt meter** — live pitch and roll derived from the on-board BMI270
  accelerometer, measured against wherever the board was resting at boot.
- **Audio feedback** — a short tone on touch-down and a higher one on release,
  through the TLV320DAC3100 codec.

Everything is on one screen. There is no menu, no navigation, no settings. That
is deliberate: the hour is about how the layers fit together, and a second
screen would buy nothing but LVGL boilerplate.

### The three things you will add

| Step | Layer(s) | What appears on the board |
| --- | --- | --- |
| **1** | devicetree → Kconfig → code | Moving a slider changes an LED's brightness |
| **2** | Kconfig | The tilt numbers stop jittering |
| **3** | Kconfig → code | Touching a slider makes a sound |

Note that **Step 1 touches all three layers on its own.** This is not padding.
Enabling the LED nodes in devicetree without also switching on the PWM driver
in Kconfig produces an application that compiles and then fails at link time,
and writing the code without either produces one that builds and does nothing.
Step 1 is the lab's thesis in miniature.

---

## 3. Setup verification (3 min)

Everything here was done in the intro session. This is a check, not a setup —
but do it now rather than at minute 29, because none of it is fixable mid-lab.

1. **Your workspace exists and your toolchain is registered.** From inside
   `devcon-ws`:

```
   west sdk list
```

   You want version `1.0.1` with `arm-zephyr-eabi` listed. If the command is
   not recognised, you are not in `devcon-ws`.

2. **Your flash configuration survived.** Still in `devcon-ws`:

```
   west config build.cmake-args
```

   This should print a line pointing at the ModusToolbox™ Programming Tools
   OpenOCD. If it says `build.cmake-args is unset`, re-run the `west config`
   command from the setup guide now — without it `west flash` cannot reach the
   board.

3. **Your board enumerates.** One USB cable to the KitProg3 connector, and a
   COM port in Device Manager. Note the number.

4. **Your serial console is open** at **115200 8N1**.

5. **You have flashed this board at least once** — the blinky from the intro
   session counts, and is the whole reason that session ended with one.

If any of these fail, pair with a neighbour for the hour. You will still get
the lab; you will just be driving somebody else's laptop.

> **Your first build in this room will be a clean one, and it takes about
> seven minutes.** That is expected, and it happens once — §5 tells you when.
> Every build after it is incremental.

---

## 4. LED labelling — read this before you debug

**The board's silkscreen and the devicetree aliases do not use the same
numbers.** This will cost somebody twenty minutes if it is not said out loud.

| Silkscreen on the board | Colour | Devicetree alias | Devicetree node | PWM channel |
| --- | --- | --- | --- | --- |
| **LED1** | Red | `pwm-led0` | `pwm_led_red` | `&pwm0_7` |
| **LED2** | Green | `pwm-led1` | `pwm_led_green` | `&pwm0_6` |
| **LED3** | Blue | `pwm-led2` | `pwm_led_blue` | `&pwm0_5` |

The silkscreen counts from one. Devicetree counts from zero. They are off by
one **and** the PWM channel numbers run backwards relative to both.

This is not a mistake in the board files — it is what happens when hardware
documentation and software conventions are written by different people to
different norms, which is the normal condition of embedded development. The
application's UI labels the sliders **LED1 / LED2 / LED3 to match the
silkscreen**, because the person looking at the board is looking at the
silkscreen. The code underneath uses the aliases.

> **If your green slider moves the red LED**, you have almost certainly wired
> an alias to the wrong node in TODO 1b. Compare against the table above rather
> than against your intuition about the numbering.

---

## 5. The three steps (27 min of editing, plus three builds)

**Pick your tier now, before you start editing.** §9 explains the three in
thirty seconds, and the choice matters here because you will name it in build
#1 — the build directory then remembers it for the rest of the hour. If you
are unsure, take `beginner`.

Every TODO is numbered `<step><letter>`. The number is the checkpoint, the
letter is the order within it. Do them in order; several of them depend on the
one before.

---

### Step 1 — Make the sliders drive the LEDs (13 min)

Four edits, across all three layers.

#### TODO 1a — devicetree: enable the counters
**File:** `boards/kit_pse84_eval_pse846gps2dbzc4a_m55.overlay`

Three `&tcpwm0_*` / `&pwm0_*` pairs are commented out. Uncomment them.

Each pair is one hardware counter plus the PWM wrapper that presents it to
Zephyr's PWM API. You need both: the counter is the silicon, the wrapper is
what `pwm_set_pulse_dt()` can actually talk to.

> **Already done for you:** the clock divider assignment
> (`&peri0_group1_16bit_0`) and the `drive-push-pull` pinctrl block. Both are
> provided deliberately — see §8 for what they are and why finding them cold
> costs an afternoon.

#### TODO 1b — devicetree: declare the LEDs
**Same file, at the bottom.**

Uncomment the `pwmleds` node **and** the `aliases` block under it.

The `pwmleds` node gives each LED a name, a PWM channel, a period and a
polarity. The `aliases` block is what lets code refer to `pwm-led0` without
knowing which board it is on. Both are needed — the node without the alias
compiles and leaves the application unable to find anything.

#### TODO 1c — Kconfig: build the driver
**File:** `prj.conf`

```
CONFIG_PWM=y
```

This is the half of Step 1 that is not devicetree. **Devicetree describing a
device does not cause its driver to be compiled.** Without this symbol the PWM
driver is never built, nothing binds to the nodes you just enabled, and the
failure arrives at link time as an undefined reference to a generated symbol
that names no file and no line.

The starting application catches this for you with a one-line error. In the
field, nothing will.

#### TODO 1d — code: drive the LED
**File:** `src/main.c`, in `led_set_percent()`

One call to `pwm_set_pulse_dt()`. The period already came from devicetree —
`PWM_DT_SPEC_GET()` captured it at build time — so all you supply is the pulse
width as a fraction of that period.

#### ✅ Build + flash #1 (7 min)

This is the one build where you type the full command. Substitute your tier —
`advanced`, `beginner` or `cheat`:

```
west build --sysbuild -b kit_pse84_eval/pse846gps2dbzc4a/m55 -d build/lab -s devcon-training-2026/labs/e84-dashboard-<tier>
```

Then flash:

```
west flash -d build/lab
```

**That is the only time you type any of that.** The build directory remembers
the board target, `--sysbuild` and which tree it was built from, so builds #2
and #3 are just:

```
west build -d build/lab
```

> **Expect about seven minutes, and know why.** This one is building
> everything from scratch — Zephyr, the vendor HAL, LVGL, and two images
> rather than one, because the E84's CM55 core is started by a companion CM33
> image. It happens once.
>
> The builds after it are incremental and cost about four and a half minutes
> each, because every checkpoint in this lab changes `prj.conf`. Had you
> changed only `src/main.c`, a rebuild would take 14 seconds; only the
> overlay, 38.
>
> That asymmetry is worth carrying out of this room. Configuration changes are
> expensive; batch them. Code changes are nearly free; iterate on them freely.

**Expected:** the three sliders now change LED brightness. The tilt numbers are
live but jumpy. Touching a slider is silent.

---

### Step 2 — Steady the tilt meter (4 min)

#### TODO 2a — Kconfig: switch on the filter module
**File:** `prj.conf`

```
CONFIG_TILT_FILTER=y
CONFIG_TILT_FILTER_ALPHA_PERCENT=18
```

`tilt_filter` is a **local out-of-tree module**, not a Zephyr subsystem. It
lives in `modules/tilt_filter/` inside this application and is picked up
because the application's `CMakeLists.txt` adds it to `ZEPHYR_EXTRA_MODULES`.
It has its own `Kconfig`, its own `zephyr/module.yml`, and its own build.

This is the shape every vendor library you will ever integrate arrives in. It
is worth thirty seconds of looking at the four files that make it one.

The second symbol is the filter strength, and the module declares a default for
it — so setting it here is optional. **Change it and rebuild.** Raising it
makes the readout steadier and laggier; lowering it makes it quicker and
noisier. It is the fastest way in this lab to watch a Kconfig value arrive in
running code.

#### ✅ Build + flash #2 (6 min)

**Expected:** pitch and roll now settle instead of flickering. Sliders still
work. Still silent.

---

### Step 3 — Add audio feedback (10 min)

#### TODO 3a — Kconfig: switch on the beeper module
**File:** `prj.conf`

```
CONFIG_BEEPER=y
```

Look at the four symbols immediately above it — `CONFIG_I2S`, `CONFIG_AUDIO`,
`CONFIG_AUDIO_CODEC`, `CONFIG_DMA`. They are already set, and they have to be,
because `BEEPER` depends on them.

**A Kconfig symbol whose dependencies are unmet is silently dropped, not
reported.** Setting `CONFIG_BEEPER=y` without those four would produce a build
with no beeper in it and no message saying why. If a symbol you set does not
seem to have taken effect, check the generated `build/lab/zephyr/.config` —
that file is the truth, `prj.conf` is only a request.

#### TODO 3b — code: bring the module up
**File:** `src/main.c`

One call to `beeper_init()`.

> Its position matters more than it looks. The sine table is built here, in
> floating point, under main's thread — and `CONFIG_FPU_SHARING` is off in this
> application, so exactly one thread may touch the FPU. The playback thread is
> deliberately integer-only. Moving this call into that thread builds cleanly
> and faults at runtime.

#### TODO 3c — code: the callbacks
**File:** `src/ui.c` — two parts.

**Part 1:** write the two LVGL event callbacks, one for press and one for
release.

**Part 2:** register them, further down in `led_card_create()`.

Both parts are required. A callback that is written and never registered is
dead code the compiler is perfectly happy with.

The two tones are an octave apart — 880 Hz down, 1760 Hz up. That interval is
load-bearing: low-then-high reads as a single gesture, whereas two tones at the
same pitch sound like the thing fired twice.

#### ✅ Build + flash #3 (6 min)

**Expected:** the finished dashboard. Sliders drive LEDs, tilt is smooth, and
touching a slider gives you a tone down and a tone up.

---

## 6. Verification — did it work? (3 min)

Work down the list on your own board:

| # | Check | Pass looks like |
| --- | --- | --- |
| 1 | Drag the **LED1** slider | The **red** LED changes brightness smoothly, not in steps |
| 2 | Drag it to 0 | LED fully off |
| 3 | Drag it to 100 | LED at full brightness |
| 4 | Repeat for LED2 / LED3 | Green and blue respectively — if the colours are swapped, see §4 |
| 5 | Tilt the board nose-up | Pitch goes **positive** |
| 6 | Tilt left-side-down | Roll changes sign consistently |
| 7 | Hold the board still | Numbers settle within about a second and stay settled |
| 8 | Touch a slider and hold | One tone on contact |
| 9 | Release | A second, higher tone |
| 10 | Tap quickly | Both tones still play — neither is dropped |

Item 10 is worth calling out. Tones are queued one deep — exactly one gesture's
worth — so a release tone is never discarded because the press tone is still
playing.

---

## 7. What to do with the rest of the hour

If you finish early, in rough order of value **per minute spent waiting** —
note that the first two cost a 4½-minute Kconfig rebuild each, while items 3
and 4 are a 14-second source rebuild:

1. **Change the two tone frequencies** in `ui.c` (14 s rebuild). Try making
   them equal and notice how much worse the interaction feels — the octave is
   doing real work.
2. **Read `modules/beeper/src/beeper.c`.** It is short, and it shows how a
   codec, I²S and a message queue fit together. No rebuild at all.
3. **Change `CONFIG_TILT_FILTER_ALPHA_PERCENT`** to 50, then to 5 (4½ min
   each). This is a two-character edit with a very visible result, and it is
   the clearest demonstration in the lab of a Kconfig value reaching code. If
   you only have time for one, use 50 — the lag is more obvious than the noise.
4. **Break something on purpose.** Comment out `CONFIG_PWM` and read the
   error. Then restore it, comment out one alias and read that one. Knowing
   what each failure *looks like* is the transferable skill here, and the two
   guards in this application exist precisely because those two failures do
   not look like their causes.

---

## 8. Two traps that were removed on purpose

Both of these are real, both cost real time during development, and both are
**pre-provided in the overlay** rather than left for you. They are documented
here because recognising them later is worth more than having fought them now.

### The clock divider

A TCPWM counter with no clock divider assigned does not fail to build, does not
warn, and does not run. The counter simply never counts, and the LED stays
dark. There is nothing in the log, because from software's point of view
everything succeeded.

The fix is a one-line assignment to `&peri0_group1_16bit_0`. Finding it
requires knowing that peripheral clock dividers are a separate devicetree
concern from the peripherals that consume them — which is obvious once, and
invisible until then.

### `drive-push-pull`

A PSOC™ GPIO defaults to a high-impedance input. Configure it as a PWM output
without setting the drive mode and the pin is electrically floating: the PWM
peripheral is genuinely running and toggling, and the pin is not driving
anything. The LED is dark, the counter is counting, and a scope on the pin
shows a signal that cannot source current.

This is the single most instructive failure in the whole application, because
every layer reports success. The `drive-push-pull` property in the pinctrl
block is the entire fix.

---

## 9. The three application trees — and picking your tier

| Tree | What is missing | Pick this if |
| --- | --- | --- |
| **`e84-dashboard-beginner`** | The same TODOs, **with the answer code supplied** next to each one as a comment block you uncomment and paste | You want to understand the structure without racing the clock |
| **`e84-dashboard-advanced`** | The TODOs, with explanation but no answers | You want to write the code |
| **`e84-dashboard-cheat`** | Nothing — this is the finished application | You are stuck, you want to see the target, or you want a reference afterwards |

### Moving between tiers mid-lab

**Steps 1 and 2 are byte-identical across `beginner` and `advanced`.** The two
trees differ only in whether the Step 3 answers are supplied as paste blocks.
This is enforced, not merely intended: the generator builds both tiers at the
Step 1+2 checkpoint and requires identical output.

So if you start in `advanced` and decide at Step 3 that you would rather have
the answers, you can move. **Copy the answer blocks across into the tree you
are already building**, rather than switching `-s` to the other directory —
your build directory is tied to the tree you named in build #1, and pointing it
somewhere else forces a pristine rebuild you do not have seven minutes for.

The same applies to `cheat`: read from it freely, but copy what you need into
your own tree.

### If you get stuck

In order: re-read the TODO text (it is longer than it looks and usually
contains the answer); check §11 for your symptom; open the same file in
`e84-dashboard-cheat`; ask.

---

## 10. What to expect from the build

**One pre-existing warning** appears in every build, from the vendor HAL
(`viv_dc_setting.c:1007`, `-Wdouble-promotion`). It is not yours and it is not
a problem. The lab's own code builds clean.

**Rebuild times**, measured on Windows:

| What you changed | Rebuild time |
| --- | ---: |
| Nothing | 4 s |
| Application source only | 14 s |
| Devicetree overlay only | 38 s |
| **Anything in `prj.conf`** | **4 min 19 s** |
| Clean build from scratch | 6 min 53 s |

Your first build is the clean one. All three checkpoints then change
`prj.conf`, which is why the two after it cost about four and a half minutes
rather than fourteen seconds. Plan what you read while you wait — §7 has
suggestions.

---

## 11. The failure catalogue — eight of these were silent

Every defect below was hit while building this application. Ten of them.
**Eight produced no error message of any kind.**

| Symptom | Actual cause | Layer |
| --- | --- | --- |
| LEDs dark, everything "succeeds" | GPIO left high-impedance; needs `drive-push-pull` | devicetree |
| LEDs dark, counter never counts | No clock divider assigned | devicetree |
| IMU reads exactly 0.00 forever | BMI270 left in suspend — configuration order matters | code |
| `west flash` reports success, board unchanged | Image never reached QSPI; verify compared against the write buffer | tooling |
| Touch events stop arriving after a while | Input queue full and silently dropping | Kconfig |
| Sliders ignore a held finger | LVGL's scroll limit, a bare `#define` with no Kconfig symbol | library |
| Screen blanks intermittently | Repaints beating the panel refresh | code |
| Tilt axes swapped | BMI270's Y is pitch and X is roll, not the reverse | code |
| Tilt under-reads by ~40% | `sensor_value_to_double()` returns m/s², not g | code |
| Fast tap loses its release tone | Release discarded instead of queued | code |

Two more worth knowing, which *did* produce errors:

- **`CONFIG_TILT_FILTER=n` would not build.** A module that is optional must
  actually compile when it is switched off, and that only gets tested if
  somebody switches it off.
- **A Kconfig symbol with unmet dependencies is dropped in silence.** Not an
  error. Check `build/lab/zephyr/.config`, never `prj.conf`.

> **The reusable rule.** Every one of the silent eight was found by comparing a
> layer's claim against physical reality — a scope on a pin, a register read, a
> hash of what was actually in flash. When a layer says it succeeded and the
> board disagrees, the board is right.

---

## 12. Reference

### Looking things up while you work

| You want | Go to |
| --- | --- |
| What properties a devicetree node accepts | `zephyr/dts/bindings/` — find the `compatible` string |
| What a Kconfig symbol does and depends on | `west build -d build/lab -t menuconfig`, then `/` to search |
| What your build actually enabled | `build/lab/zephyr/.config` |
| What devicetree actually produced | `build/lab/zephyr/include/generated/zephyr/devicetree_generated.h` |
| LVGL widget APIs | <https://docs.lvgl.io/master/> |
| Zephyr PWM API | <https://docs.zephyrproject.org/latest/hardware/peripherals/pwm.html> |
