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
| Each **pair** has two KIT_PSC3M5_CC2 boards | Both partners must be on the same board, or they read different devicetree names in Touchpoint 1 |
| The CAN harness is made up | Three wires per pair: CANH, CANL, GND, CAN connector to CAN connector |
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

## 6. Known gaps, for your own awareness

Both rough edges in this lab are in stock upstream Zephyr rather than in
Infineon code: `can_mcan` does not expose one-shot transmit mode, and
`can_send()` with a `NULL` callback blocks in a way that is easy to walk into.
Attendee §6 explains the second where it bites. Both are recorded as
observations only.

This board needs no CAN transceiver node, so the standby-pin and init-priority
problems that affect some other PSOC Control boards do not arise here. The
attendee guide uses that contrast in §6 to make the point that devicetree
describes the board rather than the chip - worth reinforcing out loud if the
room is engaged, because it is the most transferable idea in the hour.

The practical failure mode to watch for instead is the flash configuration
(§1 and §2 above). It is the only setup problem that survives a successful
build.

Timing has not been rehearsed with a live audience; the 60-minute budget in §3
above is an estimate built from measured build times, not from a dry run.