# 14a — First-contact questions for the first MOONS' fixture

Execute this stage under the [execution contract](execution_contract.md).
Status: NOT STARTED; hardware NOT TESTED.

Run it only when an exact MOONS' fixture, its recorded pre-power identity (the facts listed below under
"Before first power", except the firmware strings) and applicable authorization exist and the owner is
present. The CAN identity, firmware strings and the 0x2020 comparison are what Q1-Q3 establish, in this
order: Q1's passive observation (boot-up, heartbeat) and stage 03's probe; Q2's bootstrap identity reads
(stage 03 item 1); then Q3's reads, of which the manufacturer-area objects, 0x2020 included, follow stage 03's
layout admission (an admitting outcome from Q2). These read-only steps are the only bench steps admitted
before the CAN identity and firmware strings are recorded; every changing question and every later bench
step needs them recorded (execution contract "Hardware"). This stage does not authorize hardware work, does
not use the CL86-C bench (COM15) and does not run while COMMON records the owner as away. It follows canopen_shared 02 "First-contact questions": it
qualifies a fixture and never opens a gate. Each question below names one executable evidence path: an
ordinary API route that admits it at that point, applicable vendor evidence, or a separately assigned,
independently reviewed isolated experiment under the
[conditional experiment prerequisites](../../device_conflicts.md#conditional-experiment-prerequisites), with its
permitted actions, observation, original-value snapshot and restoration boundary recorded before it runs.
Without one the question is NOT RUN with the evidence it still needs. No raw console or frame bypass, no
weakened or temporarily opened production refusal, no presumed default as the restore value. Results never
resolve a discrepancy for every firmware.

## Work

Each question runs only after its evidence path exists: its API route (and its console route where S4 has
landed) for Q1-Q3 after 03, Q4's production words after 04, Q5-Q6 after 05, Q8 after 04 and 06, Q10's
already qualified methods after 07, Q11 after 10, Q12's store after 11 and Q14 after 09; the vendor evidence
or the review of its isolated experiment for Q4's other words, Q7, Q9, Q10's direction discovery and its
methods 35 and 37, Q12's node-ID part and Q13, which no production route admits. Results feed back: Q3 into 04's quick-stop
admission and 09's S13 entry, Q4 into 04's release and disable-voltage words, Q5-Q7 into 05's unit and set-point gates, Q8 into 04 and 06, Q9 into 11's
watchdog, Q10 into 07's direction table and its 35/37 sequence, Q11 into 10's halt and restart claims, Q12-Q13
into 11.

Before first power record: product label and part number, firmware strings once read, motor and encoder,
supply, wiring and termination, node switch and bitrate switch positions, RS-232 port disconnected (UM21 p9),
inputs wired or open, brake, free shaft, the host board, transceiver and image. Retain raw CAN traffic, timing
bounds, service gaps and restoration results. No automatic replay follows an uncertain write.

| Question | Work when prerequisites exist | Remaining limit |
| --- | --- | --- |
| Q1 transport and boot | Power with the declared switches; record boot-up on 0x700 + node, heartbeat period against 0x1017, and the probe replies of 0x1000 and 0x1018:01-04. Record 0x1018:02/03, which are 0 only in the EDS | One product code value is fixture evidence, not a family rule (D18 pattern). 10 or 12.5 kbit/s only after Q13 |
| Q2 identity strings | Read 0x2050, 0x2031, 0x100A, 0x1009 and, on SSDC, 0x2032/0x2033. Record which object carries "310A"/"310B" and whether long strings arrive segmented (MO-D01, MO-D27) | A string seen on one unit does not define the firmware row for others |
| Q3 initial state, no writes | With Q2's admitting outcome for the manufacturer-area objects, read 0x2020 against the node switch and the active node ID (stage 03 item 8), and 0x6041, 0x6061, 0x605A, 0x6502, 0x1029, 0x2060, 0x2617/0x220D, 0x260B/0x220F, 0x2001, 0x2604 or 0x2207/0x2218, 0x60FD, 0x603F, 0x200B and 0x2070 against the physical switch positions (MO-D05, MO-D11, MO-D16, MO-D24) | The power-up mode and 0x605A values feed stage 09's S13 entry and stage 04's quick-stop admission only for this fixture and firmware |
| Q4 release and current | With a bounded plan, stationary, from Operation enabled through stage 04's routes: the word stage 04 reviewed for `prepareRelease`, and 0x0000 as its disable-voltage stop, each once from a predecessor with halt clear and once from one with halt set (enabled with 0x010F), where stage 04 item 8's bit 8 rule sends 0x0106 and 0x0100; record the CiA 402 state and 0x200B bit 0 after each (MO-D28, MO-D33). The halt-set trials alone qualify 0x0106 and 0x0100; Q8's quick-stop results never do. The release word is 0x0006 (stage 04 item 4); the other word of the comparison, 0x0007 from Operation enabled, has no production route. Path: vendor clarification, or a reviewed isolated experiment with load and brake declared, one word per trial, state and 0x200B recorded, a person at the shaft, and re-enabling only as a separate stage 04 enable. Otherwise NOT RUN | Shaft freedom needs a person; holding torque is not inferred from state bits |
| Q5 units | Small native finite move; compare the 0x6064 delta with 0x2604 (STF) or 0x2207 and 0x2218 (SSDC) and an independently observed shaft displacement (MO-D03) | A unit result binds to that resolution setting; a later 0x2604 change invalidates it |
| Q6 measured position on SSDC | With the drive disabled, have a person turn the shaft and read 0x6064 and 0x606C (MO-D20) | No stall, load or following-error provocation is authorized by this question |
| Q7 set-point buffer | Stage 05 admits one idle move and refuses a second set-point, so no production route exists. Path: vendor clarification of MO-D31, or a reviewed isolated experiment: two small finite set-points (0x607A, 0x6081, the controlword words in order) with bit 5 = 0 while the first runs, recording bit 12, bit 10 and which targets execute; snapshot 0x607A, 0x6081, 0x6083, 0x6084; nothing persistent changes. Otherwise NOT RUN | No automatic retry; a lost trigger stays unknown |
| Q8 stops | On small finite moves compare quick stop with 0x605A = 1 and 2 (end state Switch on disabled), and halt in mode 1 (pause, then release through stage 04's halt-release operation) and mode 3, recording deceleration against 0x6084 and 0x6085 and the transitions 0x000B and 0x010B produce (MO-D05, MO-D06, MO-D34). The halt runs through stage 04's route with the fixture's local-transition declaration recorded (stage 04 item 9). 0x605A changes only through stage 04's setter, which accepts 1 or 2, and only when Q3's snapshot read it as 1 or 2, so that setter restores it with readback; the subcase at the value read needs no change; with the snapshot unread, 0 or any other value, every subcase that changes 0x605A is NOT RUN, since no admitted path restores that value | No moving link-loss test; host duration is not a physical bound after a link failure |
| Q9 watchdog | Stage 11's arm is refused while the 0x2060 sub-index writes are withheld (MO-D09), and its feed must be an S13-admitted SYNC schedule or an S14 RPDO (not landed); a SYNC-fed arm also needs stage 11's reviewed arm transition, because stage 09 revokes the S13 approval before the 0x2060 write and so stops the feed at arming. Path: stage 11's route once all three exist; vendor clarification; or, on SSDC-C only, where the arming widths agree and that transition has been reviewed, a reviewed isolated experiment: stationary and enabled, sub 5 left at its read value and run only if that is 0 or 1, sub 2 written as UM21 p38 documents, an S13-admitted SYNC feed, the feed stopped under a bounded plan, sub 2, the action and the state recorded, then disarm and read the snapshot values back. STF-C's conflicting widths admit no experiment, which may not choose between competing writes. Otherwise NOT RUN (MO-D18) | Running-motion watchdog tests need their own reviewed plan; action 2 and above also need the MO-D18 action-to-segment mapping |
| Q10 homing | With actual switches (MO-D07), three subcases. (a) A method whose direction and approach qualification already exists for this fixture and configuration: stage 07's route with its stop and travel plan, recording the bits 13/12/10 sequence and the final 0x6064 for 0x2009 = 0 and 1. (b) First discovery of a method's approach direction has no production route: stage 07 refuses without the qualification, and switching prompt 77's direction guard off is a temporarily opened refusal. Path: vendor clarification of MO-D07, or a reviewed isolated experiment per method: switches confirmed by fresh 0x60FD reads, low 0x6099 speeds, a travel bound and an independent physical stop, observing which limit or home bit changes first and the sign of the 0x6064 change, halting with stage 04's halt word (0x011F after an acknowledged start) at the first evidence or the bound; 0x6098, 0x6099, 0x609A, 0x607C and 0x2009 snapshot and restored by explicit writes with readback, the changed position origin being what stays changed. Its result becomes the qualification of (a). (c) Methods 35 and 37: vendor clarification of the trigger sequence only, since an experiment may not invent the words. Otherwise NOT RUN | No limit is disabled to make homing pass; index methods need an evidenced index |
| Q11 Q programs | Through stage 10's route, with a program whose content the owner supplies: mode -1 start, halt and restart, recording what halt stops and where the next start resumes, and 0x200B bit 14 timing (UM21 p35). Mode -2 is refused in this set and is not run | Program behavior is the owner's evidence, not the library's |
| Q12 persistence and node ID | With restart control: store once through stage 11's route if its preflight admits 0x1010, restart and compare. The node-ID part has no production route (0x2020's width is withheld and its store sub-index unstated, MO-D09, MO-D16), and an experiment may not choose between the two widths or guess a store group. Path: vendor clarification naming the width and store sub-index; then a reviewed isolated experiment snapshotting 0x2020 and the switch positions and restoring by an explicit 0x2020 write, store and restart, with the next power cycle's node ID as what stays changed if that fails. Otherwise NOT RUN (MO-D23 likewise for restore) | No repeated store campaign and no guessed store group; restore communication settings afterwards and read them back |
| Q13 bitrate list | Binding admission rejects 12.5 and 10 kbit/s (stage 02b), so no production route exists. Path: vendor clarification, or a reviewed isolated experiment: a person sets switch code 7, a listen-only analyzer records the boot-up frame at 12.5 and at 10 kbit/s, the host sends nothing, and the switch is restored and the node read back at its declared rate. Otherwise NOT RUN (MO-D17) | Code 8-9 reserved; never set them |
| Q14 EMCY and LEDs | Record naturally occurring EMCY frames with 0x603F and 0x1003; a person records LED codes against STFHW p30 and STFHWC p44 (MO-D19) | No deliberate fault induction under this question |

### Rules for every question

1. Snapshot first: read every setting a question may change through its own route, restore it on the same
   node before moving to another node, flashing or ending the session, and read the restore back.
2. One lead holds the port and all active CAN requests; workers do host analysis only.
3. Finite moves are small, nonnegative or bounded by an explicitly reviewed travel, with a prepared stop path.
   No unbounded velocity run, moving link-loss, stall, load or deliberate fault induction belongs to these
   questions; each needs its own reviewed plan.
4. A question that needs a person (shaft feel, direction against a mark, LEDs, turning the shaft, a power
   cycle, switch positions) is recorded "not observed: owner away" if the owner is away, and becomes a row of
   the [deferred table](../README.md) instead of a wait.
5. A failed or surprising result is retained with revision, configuration, raw traffic and timing; it is
   reproduced deliberately within authorization, never hidden by a retry or a longer timeout.
6. No question writes 0x1010, 0x1011, 0x2020 or 0x2021 except Q12 on its named path with restart control, and
   none writes 0x2060 except Q9 on its named path; nothing is left armed, stored or changed at the end.

## Deliver

`docs/reports/moons_canopen_14a_<date>.md` with the questions actually run, exact prerequisites, raw results,
restoration outcomes and unresolved measurements; ledger conflict updates with the fixture evidence, scoped
to model, firmware and configuration; any fixture-specific qualification applied through ordinary API routes.

## Acceptance

Every question run has its prerequisites, raw evidence and restoration read-back recorded; every question not
run says why. An MO-D item closes only when the evidence covers its full claim; otherwise it stays open with
the measurement still needed. Release qualification stays with [stage 14](14_qualification_and_release.md).
