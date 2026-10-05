# PSOC™ Control — Command & Telemetry over CAN
### Instructor Guide (DevCon FAE Training, Session 2)

Companion to `lab-guide.md`, written for whoever is running the session.
Section references below (§3, §6, §7, §11) point into the attendee guide.

> This document is published alongside the lab guide, so assume an attendee
> *can* find it. Nothing here is secret — but §4 is the reason the lab works,
> and it reads differently to someone waiting for help than to someone
> deciding whether to give it.

---

## 1. Before the room arrives

| Must be true | How to confirm |
| --- | --- |
| Every attendee has a working workspace from the intro session | §3 of the attendee guide is the check; it takes them 3 minutes |
| Each **pair** has two boards of the same variant | Mixing an EVK and a CC2 across one pair works electrically, but the two partners then read different devicetree names in Touchpoint 1 — avoid it |
| The CAN harness is made up | Three wires per pair: CANH, CANL, GND, screw terminal to screw terminal |
| You have run the pair yourself, in front of them, once | See §2 below |

The workspace is the single biggest risk to the hour and it cannot be fixed in
the room — `west update` alone is a 2.2 GB download. Anyone who arrives
without one should be paired immediately rather than left to catch up.

The in-room check that matters most is the flash configuration. A workspace
missing the `west config build.cmake-args` line builds perfectly and cannot
flash, and the attendee will not discover it until their first build is
already done. §3 item 2 catches it in ten seconds.

**This lab fails differently from a solo lab.** One attendee falling behind
strands a second one who did nothing wrong. Watch for a board that has not
flashed by the 35-minute mark and move that person to `can-lab-cheat` without
making it a conversation.

---

## 2. The hour-zero smoke test

Before the room starts editing, flash **both** demo boards from
`can-lab-production` — one per role — and show the LED sync working at the
front of the room.

This does two things. It sets the target behaviour in everyone''s mind, which
matters more here than in a solo lab because the finish line involves someone
else''s board. And it eliminates "is my hardware broken?" as a variable for the
rest of the hour.

Use `can-lab-production` rather than one of the attendee tiers: `advanced` and
`beginner` deliberately refuse to build until Steps 1 and 2 are done, so
neither can serve as a smoke test.

---

## 3. Running the hour

| Segment | Budget |
| --- | ---: |
| Opening (§4 below, read to the room) | 2 min |
| Setup verification — attendee §3 | 3 min |
| The three touchpoints — attendee §5 | 40 min |
| Build, flash, bring the pair up — attendee §6 | 10 min |
| Verification and wrap — attendee §7 | 5 min |
| **Total** | **60 min** |

**Nothing is pre-built, deliberately.** It is tempting to prime a build
directory before the room arrives to hide the first clean build. Do not. West
caches the board, the source tree and the build type in the build directory;
if you prime it with one tier and the attendee then edits a different one,
their build silently reuses the primed tree and flashes code they did not
write. The first clean build is about two minutes and it buys correctness.

The 40-minute touchpoint block is where the hour is won or lost. Phase 1 of
§6 is a single-board test on purpose — resist letting pairs wire their boards
together early, because it merges two independent failures into one confusing
one.

---

## 4. Tiers — do not allocate them

Attendees choose their own tier and can move between them mid-lab at no cost;
Steps 1 and 2 are byte-identical across all three trees. The attendee guide
says this plainly in §3 and §9.

The thing to actually do is **say out loud, early, that taking `can-lab-cheat`
is a legitimate choice** — and then not editorialise when someone does. In a
paired lab, a stuck attendee taking the finished code is strictly better than
a stranded partner with no CAN peer. Framing that as the sensible move rather
than as giving up is most of the job.

The dividing line between beginner and advanced is **transcription, not
knowledge**. Both name the function and describe its arguments; only beginner
prints a line that can be copied. Advanced is only fair because every TODO
links its API reference — if someone is working from memory and struggling,
point them at the `Docs:` line rather than at the answer.

---

## 6. The hardware-PWM variant

`-DLED_TRIGMUX=y` drives the on-board LED from a real TCPWM PWM channel routed
through the PERI trigger multiplexer, instead of the software PWM path the lab
uses by default. It is mentioned briefly in attendee §8.

It is **EVK-only** — the CC2''s user LEDs sit on P9.4/P9.5, which select only
SCB0 SPI, so there is no PWM-capable route to them.

It changes no touchpoint and no step, and the two fades are visually
indistinguishable, which is why it is not the default. It is worth having in
your pocket for the attendee who asks whether the soft-PWM path is a hardware
limitation. It is not — it is a Zephyr software gap, and this is the proof.

---

## 7. Known gaps, for your own awareness

Two **board-support-package gaps** are worked around in the lab''s overlay and
`prj.conf`: the CAN transceiver standby pin is not modelled upstream, and the
transceiver''s default init priority runs ahead of the Infineon GPIO driver it
depends on. Attendee §6 explains both where they bite. Both are drafted as
Jira tickets but not yet filed.

Two further rough edges are in stock upstream Zephyr rather than Infineon
code — `can_mcan` not exposing one-shot transmit mode, and the `can_send()`
NULL-callback blocking trap — and are recorded as observations only.

Timing has not been rehearsed with a live audience; the 60-minute budget in §3
above is an estimate built from measured build times, not from a dry run.