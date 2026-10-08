# PSOC™ Control — Command & Telemetry over CAN
### Zephyr on PSOC™ Control — Instructor Guide (DevCon FAE Training)

Companion to `lab-guide.md`, written for whoever is running the session. Section references below point into the attendee guide.

> This document is published alongside the lab guide, so assume an attendee *can* find it. Nothing here is secret — but §4 is the reason the lab works, and it reads differently to someone waiting for help than to someone deciding whether to give it.

---

## 1. Before the room arrives

| Must be true | How to confirm |
| --- | --- |
| Every attendee has a working workspace from the Software General Session | Attendee §3 is the check; it takes them 3 minutes |
| Each **pair** has two KIT_PSC3M6_EVAL boards | Both partners must be on the same board, or they read different devicetree names in Touchpoint 1 |
| The CAN harness is made up | Three wires per pair: CANH, CANL, GND, CAN connector to CAN connector |
| You have run the pair yourself, in front of them, once | See §2 below |

The workspace is the single biggest risk to the hour and it cannot be fixed in the room — building one from scratch means roughly 425 MB of downloads and 2.5 GB on disk. Anyone who arrives without one should be paired immediately rather than left to catch up.

Flashing needs no configuration on this board; the kit's onboard J-Link is enough. That removes the one setup problem that used to survive a successful build, so attendee §3 is now a genuinely quick check.

**The one J-Link failure worth knowing in advance.** Zephyr finds the J-Link install by reading the path SEGGER writes to the registry, and SEGGER writes whichever version was installed **last** — not the newest. An attendee who installed a current J-Link and then let some other toolchain pull in an older one will have a stale path, and `west flash` fails with an obscure DLL or "unsupported device" error on a board that is otherwise fine. The floor for this board is **V9.68**. Have them check with:

```powershell
(Get-Item "$((Get-ItemProperty HKCU:\Software\SEGGER\J-Link).InstallPath)\JLink.exe").VersionInfo.ProductVersion
```

If that prints something below V9.68, re-running the current J-Link installer fixes it by rewriting the registry path — no uninstall needed.

**This lab fails differently from a solo lab.** One attendee falling behind strands a second one who did nothing wrong. Watch for a board that has not flashed by the 35-minute mark and move that person to `can-lab-cheat` without making it a conversation.

---

## 2. The hour-zero smoke test

Before the room starts editing, flash **both** demo boards from `can-lab-production` — one per role — and show the LED sync working at the front of the room.

```
west build -b kit_psc3m6_evk -d build/prod-cmd -s devcon-training-2026/labs/can-lab-production -- -DEXTRA_CONF_FILE=conf/role_command.conf
west flash -d build/prod-cmd

west build -b kit_psc3m6_evk -d build/prod-tlm -s devcon-training-2026/labs/can-lab-production -- -DEXTRA_CONF_FILE=conf/role_telemetry.conf
west flash -d build/prod-tlm
```

Separate build directories per role, and keep them out of `build/lab` so the room's own first build is still a genuine clean build.

This does two things. It sets the target behaviour in everyone's mind, which matters more here than in a solo lab because the finish line involves someone else's board. And it eliminates "is my hardware broken?" as a variable for the rest of the hour.

Use `can-lab-production` rather than one of the attendee tiers: `advanced` and `beginner` deliberately refuse to build until Steps 1 and 2 are done, so neither can serve as a smoke test.

**One thing to notice while you are up there.** `can-lab-production` drives LED3 from the real TCPWM waveform routed through the PSOC™ Control trigger multiplexer, where the attendee tiers bit-bang it. The LED looks the same across the room, so do not oversell it in the smoke test — but it is the demo to come back to in the wrap-up, and attendee §11 has the full explanation and the `-DLED_SOFTPWM=y` A/B build. Confirm the route took by looking for this line on the command node's console:

```
<inf> lab_led_trigmux: On-board LED3 (P8.4) driven by tcpwm0_6 via the PERI trigger mux
```

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

Read the §4 framing below while the room is working through their §3 checks — it costs you no budget and it lands better before anyone is stuck.

**Nothing is pre-built, deliberately.** It is tempting to prime a build directory before the room arrives to hide the first clean build. Do not. West caches the board, the source tree and the build type in the build directory; if you prime it with one tier and the attendee then edits a different one, their build silently reuses the primed tree and flashes code they did not write. The first clean build is about two minutes and it buys correctness.

**The checkpoint build in §7 is load-bearing — do not let pairs skip it.** It flashes a working devicetree and Kconfig with no application code, so a problem in Steps 1 or 2 surfaces on its own rather than tangled up with half-finished code twenty minutes later. Attendees who are moving fast will want to push straight on to Touchpoint 3; the five minutes is cheaper than the debugging it prevents.

Phase 1 of §9 is a single-board test on purpose — resist letting pairs wire their boards together early, because it merges two independent failures into one confusing one.

---

## 4. Tiers — do not allocate them

Attendees choose their own tier and can move between them mid-lab at no cost; Steps 1 and 2 are byte-identical across all three trees. The attendee guide says this plainly in §3.

The thing to actually do is **say out loud, early, that taking `can-lab-cheat` is a legitimate choice** — and then not editorialise when someone does. In a paired lab, a stuck attendee taking the finished code is strictly better than a stranded partner with no CAN peer. Framing that as the sensible move rather than as giving up is most of the job.

The dividing line between beginner and advanced is **transcription, not knowledge**. Both name the function and describe its arguments; only beginner prints a line that can be copied. Advanced is only fair because every TODO links its API reference — if someone is working from memory and struggling, point them at the `Docs:` line rather than at the answer.

---

## 6. Points worth making out loud

**The LED numbering does not line up, and it is not off-by-one.** The silkscreen says **LED3** and **LED4**; the aliases are `led0` and `led1`. Attendee §4 has the table, but saying it once at the front of the room saves at least one person ten minutes — and saying it *as a lookup, not a formula* stops the person who assumes LED3 must be `led2`.

**`kit_psc3m6_evk` is not a typo, and someone will ask.** The kit is KIT_PSC3M6_EVAL; the Zephyr board identifier still says `evk`, carried over from the C3M5 kit. It is noted in attendee §3. Answer it in one sentence and move on — it has been raised with the platform team, and nothing in the hour depends on it.

**This board needs no CAN transceiver node.** Some other boards do — their transceiver standby pin is gated by a GPIO that nothing releases, so the node is what tells Zephyr to drive it. The attendee guide keeps this to one line in §9 on purpose, but it is the most transferable idea in the hour if the room is engaged: same silicon, same driver, same application, and yet one board needs an extra devicetree node and the other does not. Devicetree describes **the board**, not the chip.

**The deceptive failure is two command nodes.** It looks like a working system — both boards respond to their own knob and nothing logs an error, because every CAN node acknowledges any valid frame regardless of its own filters. The only tell is turning *one* knob and watching the *other* board. It is in the §10 troubleshooting table, but it is worth pre-empting.

**LED4 means different things on the two boards, and that is on purpose.** Command node: steady lit while the controller is error-active. Telemetry node: blinking at ~1 Hz while frames are arriving. Somebody will call this an inconsistency, so get ahead of it — a receive-only node never transmits, so its transmit error counter never moves, and driving its LED4 from controller state leaves it confidently lit with the bus wires pulled out. It is the single best teaching moment in the hour about CAN error confinement, and it is why the telemetry node's TODO 3d exists. Pulling one wire live at the front of the room sells it in five seconds: the command node goes dark, the telemetry node stops blinking, and both consoles say why.

**"Zephyr does not model that yet" is a good answer, not an awkward one.** If the trigger multiplexer comes up — and in an FAE room it will — the honest shape of it is: the pin-mux half is plain devicetree, the routing half is a direct PDL call, and the two sit side by side in the same application without ceremony. That is the realistic pattern for any vendor block Zephyr has not abstracted yet, and showing it working is more useful to a customer than implying the gap is not there. See §7 for where the upstream work actually stands, in case someone asks when it lands.

---

## 7. Known gaps, for your own awareness

Both rough edges in this lab are in stock upstream Zephyr rather than in Infineon code: `can_mcan` does not expose one-shot transmit mode, and `can_send()` with a `NULL` callback blocks in a way that is easy to walk into. The lab passes a real callback for that reason. Both are recorded as observations only.

**Trigger multiplexer support, if someone asks when it lands.** As of the tree this lab is pinned to, there is no Infineon trigger-mux driver or binding in Zephyr, and nothing open upstream that adds one — which is why `can-lab-production` makes the route with a PDL call. Two things are worth knowing before you answer:

- Zephyr gained a **generic mux subsystem** in 4.5 (`drivers/mux/`, with an NXP TRGMUX driver among the first users). It is present in our tree. An Infineon driver landing there is now a more likely path than the per-vendor `drivers/misc/interconn/` folder the earlier internal design assumed — a second vendor's attempt to use that folder was turned down in favour of the generic API.
- The generic peripheral-interconnect API discussion upstream is still open and unresolved.

The safe answer in the room is "there is a generic mux API in 4.5 and Infineon support is not in it yet; today you use the PDL." Do not give a date.

Timing has not been rehearsed with a live audience; the 60-minute budget in §3 above is an estimate built from measured build times, not from a dry run.
