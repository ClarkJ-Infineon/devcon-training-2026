# PSOC™ Edge — A Touch Dashboard with LVGL
### Zephyr on PSOC™ Edge — Instructor Guide (DevCon FAE Training)

Companion to `lab-guide.md`, written for whoever is running the session. Section references below (§3, §7, §11) point into the attendee guide.

> This document is published alongside the lab guide, so assume an attendee *can* find it. Nothing here is secret — but §4 is the reason the lab works, and it reads differently to someone waiting for help than to someone deciding whether to give it.

---

## 1. Before the room arrives

| Must be true | How to confirm |
| --- | --- |
| Every attendee has a working workspace from the Software General Session | §3 of the attendee guide is the check; it takes them 3 minutes |
| Each bench has a KIT_PSE84_EVAL with the 4.3" panel attached | Panel is MIPI-DSI with capacitive touch |
| You have flashed a board yourself, in front of them, once | Sets the target behaviour and removes "is my hardware broken?" as a variable |

The workspace is the single biggest risk to the hour, and it is not something that can be fixed in the room — building one from scratch means roughly 425 MB of downloads and 2.5 GB on disk. Anyone who arrives without one should be paired with a neighbour immediately rather than left to catch up.

The in-room check that matters most is the flash configuration. A workspace missing the `west config build.cmake-args` line builds perfectly and cannot flash, and the attendee will not discover it until build #1 is already done. §3 item 2 catches it in ten seconds.

---

## 2. Opening — read this to the room

> You have all run a Zephyr sample before. Running a sample teaches you that Zephyr works. It does not teach you how a Zephyr application is actually assembled, because the sample has already made every decision for you.
>
> This hour is about the three places those decisions live. Devicetree says what hardware exists. Kconfig says what software gets built. Application code says what any of it is for. The thing that catches people — and the reason this lab is shaped the way it is — is that **all three have to agree, and when they disagree the board usually says nothing at all.**
>
> You are going to make them disagree. Several times. That is the lab. Each time, the question to ask is not "what did I type wrong" but "which of the three layers does not know about the other two."
>
> There are three checkpoints. At each one you build, flash, and get something you can see or hear. If you fall behind, there is a complete working copy of the application you can take any step from, and no, that is not cheating — there is a whole tier named after it.

---

## 3. Running the hour

| Segment | Budget |
| --- | ---: |
| Setup verification (§3) | 3 min |
| Step 1 — sliders drive the LEDs | 13 min |
| **Build + flash #1 — clean build** | **8 min** |
| Step 2 — steady the tilt meter | 4 min |
| Build + flash #2 | 6 min |
| Step 3 — audio feedback | 10 min |
| Build + flash #3 | 6 min |
| Verification (§6) | 3 min |
| **Total** | **53 min** |

That leaves about seven minutes for the wrap-up and for the hour not going to plan. It will not go to plan.

**Build #1 is the long one and it is the only clean build.** It costs 6 min 53 s plus flash, because nothing has been compiled yet — Zephyr, the vendor HAL, LVGL, and two images rather than one. Builds #2 and #3 are incremental at about 4 min 19 s each.

Nothing is pre-built before the session, deliberately. The two editable tiers stop at a compile-time `#error` until Step 1 is finished, so the only tree that *can* be built in advance is `cheat` — and a build directory warmed with `cheat` is tied to `cheat`, so an attendee editing their own tier would rebuild the finished application and flash a working dashboard that contains none of their work. In a lab whose thesis is that a layer reporting success proves only that that layer succeeded, that is not a trap worth setting.

**The three builds are the pacing mechanism, not dead air.** Every checkpoint changes `prj.conf`, which is the expensive case. Use that time: §7 of the attendee guide lists what to do with it, and the builds are where you run the discussion.

---

## 4. How to help

**The single most valuable thing you can do in this hour is *not* rescue people quickly.**

Eight of the ten defects found while building this application presented as silence — no error, no log line, nothing on the panel. That is the actual experience of bringing up an embedded application. Thirty seconds of letting someone sit with a dark LED is worth more than the answer.

§11 of the attendee guide has the full catalogue. **Use it to steer, not to solve** — ask which of the three layers has not been told, rather than naming the property.

Tier switching is free and you should offer it early to anyone falling behind: Steps 1 and 2 are byte-identical between `beginner` and `advanced`, so moving across costs nothing but a copy. The generator enforces that equivalence.

---

## 5. The wrap-up

The two pre-provided traps in §8 — the clock divider and `drive-push-pull` — are the best material in the lab for closing out.

The lesson is not the two properties. Nobody will remember them. The lesson is that **on embedded, a layer reporting success means that layer succeeded, and nothing more than that.**

The reusable rule, if you want one line on the last slide: every one of the silent eight was found by comparing a layer's claim against physical reality. When a layer says it succeeded and the board disagrees, the board is right.

---

## 6. Hardware validation provenance

Everything in the attendee guide has been run on real hardware on a KIT_PSE84_EVAL.

| Area | Status |
| --- | --- |
| Display — 4.3" MIPI-DSI panel, RGB565, double-buffered | ✅ Validated |
| Touch — FT5406 capacitive controller | ✅ Validated |
| LEDs — three channels of hardware PWM | ✅ Validated, silkscreen mapping confirmed |
| IMU — BMI270 accelerometer, pitch and roll | ✅ Validated, incl. sign convention |
| Audio — TLV320DAC3100 over I²S | ✅ Validated |
| Flashing — OpenOCD via KitProg3, QSPI | ✅ Validated |
| Build — all three tiers, both checkpoints | ✅ Verified on the Linux builder |

The flashed image measures 522,000 B, and the `cheat` tree builds to `text=518148 data=3847` — 521,995 B. The guide's tree and the board's image are the same code.

**Pitch sign.** Nose-up reads positive, confirmed on hardware; the code does `atan2f(-v[1], v[2])`.

---

