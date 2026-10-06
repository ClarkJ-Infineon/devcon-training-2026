# PSOC™ Control — Command & Telemetry over CAN
### Instructor Guide (DevCon FAE Training, Session 2)

Companion to `lab-guide.md`, written for whoever is running the session.
Section references below point into the attendee guide.

> This document is published alongside the lab guide, so assume an attendee
> *can* find it. Nothing here is secret — but §4 is the reason the lab works,
> and it reads differently to someone waiting for help than to someone
> deciding whether to give it.

---

## 1. Before the room arrives

| Must be true | How to confirm |
| --- | --- |
| Every attendee has a working workspace from the intro session | Attendee §3 is the check; it takes them 3 minutes |
| Each **pair** has two KIT_PSC3M5_CC2 boards | Both partners must be on the same board, or they read different devicetree names in Touchpoint 1 |
| The CAN harness is made up | Three wires per pair: CANH, CANL, GND, CAN connector to CAN connector |
| You have run the pair yourself, in front of them, once | See §2 below |

The workspace is the single biggest risk to the hour and it cannot be fixed in
the room — `west update` alone is a 2.2 GB download. Anyone who arrives
without one should be paired immediately rather than left to catch up.

Flashing needs no configuration on this board; the kit's onboard J-Link LITE is
enough. That removes the one setup problem that used to survive a successful
build, so attendee §3 is now a genuinely quick check.

**This lab fails differently from a solo lab.** One attendee falling behind
strands a second one who did nothing wrong. Watch for a board that has not
flashed by the 35-minute mark and move that person to `can-lab-cheat` without
making it a conversation.

---

## 2. The hour-zero smoke test

Before the room starts editing, flash **both** demo boards from
`can-lab-production` — one per role — and show the LED sync working at the
front of the room.

This does two things. It sets the target behaviour in everyone's mind, which
matters more here than in a solo lab because the finish line involves someone
else's board. And it eliminates "is my hardware broken?" as a variable for the
rest of the hour.

Use `can-lab-production` rather than one of the attendee tiers: `advanced` and
`beginner` deliberately refuse to build until Steps 1 and 2 are done, so
neither can serve as a smoke test.

---

## 3. Running the hour

| Segment | Attendee § | Budget |
| --- | --- | ---: |
| Before you start | §3 | 3 min |
| Touchpoint 1 — devicetree | §5 | 10 min |
| Touchpoint 2 — Kconfig | §6 | 2 min |
| Checkpoint build | §7 | 5 min |
| Touchpoint 3 — code | §8 | 25 min |
| Bring the pair up | §9 | 10 min |
| Verification and wrap | §10 | 5 min |
| **Total** | | **60 min** |

Read the §4 framing below while the room is working through their §3 checks —
it costs you no budget and it lands better before anyone is stuck.

**Nothing is pre-built, deliberately.** It is tempting to prime a build
directory before the room arrives to hide the first clean build. Do not. West
caches the board, the source tree and the build type in the build directory;
if you prime it with one tier and the attendee then edits a different one,
their build silently reuses the primed tree and flashes code they did not
write. The first clean build is about two minutes and it buys correctness.

**The checkpoint build in §7 is load-bearing — do not let pairs skip it.**
It flashes a working devicetree and Kconfig with no application code, so a
problem in Steps 1 or 2 surfaces on its own rather than tangled up with
half-finished code twenty minutes later. Attendees who are moving fast will
want to push straight on to Touchpoint 3; the five minutes is cheaper than the
debugging it prevents.

Phase 1 of §9 is a single-board test on purpose — resist letting pairs wire
their boards together early, because it merges two independent failures into
one confusing one.

---

## 4. Tiers — do not allocate them

Attendees choose their own tier and can move between them mid-lab at no cost;
Steps 1 and 2 are byte-identical across all three trees. The attendee guide
says this plainly in §3.

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

## 6. Points worth making out loud

**The LED numbering is off by one.** The silkscreen starts at LED1; Zephyr's
aliases start at `led0`. Attendee §4 states it, but saying it once at the front
of the room saves at least one person ten minutes.

**This board needs no CAN transceiver node.** Some other boards do — their
transceiver standby pin is gated by a GPIO that nothing releases, so the node
is what tells Zephyr to drive it. The attendee guide keeps this to one line in
§9 on purpose, but it is the most transferable idea in the hour if the room is
engaged: same silicon, same driver, same application, and yet one board needs
an extra devicetree node and the other does not. Devicetree describes **the
board**, not the chip.

**The deceptive failure is two command nodes.** It looks like a working system
— both boards respond to their own knob and nothing logs an error, because
every CAN node acknowledges any valid frame regardless of its own filters. The
only tell is turning *one* knob and watching the *other* board. It is in the
§10 troubleshooting table, but it is worth pre-empting.

---

## 7. Known gaps, for your own awareness

Both rough edges in this lab are in stock upstream Zephyr rather than in
Infineon code: `can_mcan` does not expose one-shot transmit mode, and
`can_send()` with a `NULL` callback blocks in a way that is easy to walk into.
The lab passes a real callback for that reason. Both are recorded as
observations only.

Timing has not been rehearsed with a live audience; the 60-minute budget in §3
above is an estimate built from measured build times, not from a dry run.
