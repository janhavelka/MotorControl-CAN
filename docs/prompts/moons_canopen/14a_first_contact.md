# 14a — First-contact questions for the first MOONS' fixture

Execute this stage under the [execution contract](execution_contract.md).
Status: NOT STARTED; hardware NOT TESTED.

Run it only when an exact MOONS' fixture, its recorded identity and applicable authorization exist and the
owner is present. This stage does not authorize hardware work, does not use the CL86-C bench (COM15) and does
not run while COMMON records the owner as away. Results qualify the concrete fixture through ordinary API
routes; they never resolve a discrepancy for every firmware.

## Work

Each question runs only after its API route exists (and its console route where S4 has landed): Q1-Q3 after
03, Q4 after 04, Q5-Q7 after 05, Q8 after 04 and 06, Q9 after 11, Q10 after 07, Q11 after 10, Q12-Q13 after
11, Q14 after 09. Results feed back: Q3 into 04's quick-stop admission and 09's S13 entry, Q5-Q7 into 05's unit
and set-point gates, Q8 into 04 and 06, Q9 into 11's watchdog, Q10 into 07's direction table, Q12-Q13 into 11.

Before first power record: product label and part number, firmware strings once read, motor and encoder,
supply, wiring and termination, node switch and bitrate switch positions, RS-232 port disconnected (UM21 p9),
inputs wired or open, brake, free shaft, the host board, transceiver and image. Retain raw CAN traffic, timing
bounds, service gaps and restoration results. No automatic replay follows an uncertain write.

| Question | Work when prerequisites exist | Remaining limit |
| --- | --- | --- |
| Q1 transport and boot | Power with the declared switches; record boot-up on 0x700 + node, heartbeat period against 0x1017, and the probe replies of 0x1000 and 0x1018:01-04. Record 0x1018:02/03, which are 0 only in the EDS | One product code value is fixture evidence, not a family rule (D18 pattern). 10 or 12.5 kbit/s only after Q13 |
| Q2 identity strings | Read 0x2050, 0x2031, 0x100A, 0x1009 and, on SSDC, 0x2032/0x2033. Record which object carries "310A"/"310B" and whether long strings arrive segmented (MO-D01, MO-D27) | A string seen on one unit does not define the firmware row for others |
| Q3 initial state, no writes | Read 0x6041, 0x6061, 0x605A, 0x6502, 0x1029, 0x2060, 0x2617/0x220D, 0x260B/0x220F, 0x2001, 0x2604 or 0x2207/0x2218, 0x60FD, 0x603F, 0x200B and 0x2070 against the physical switch positions (MO-D05, MO-D11, MO-D16, MO-D24) | The power-up mode and 0x605A values feed stage 09's S13 entry and stage 04's quick-stop admission only for this fixture and firmware |
| Q4 release and current | With a bounded plan, compare 0x0006, 0x0007 and 0x0000 from Operation enabled: CiA 402 state and 0x200B bit 0 (MO-D28, MO-D33) | Shaft freedom needs a person; holding torque is not inferred from state bits |
| Q5 units | Small native finite move; compare the 0x6064 delta with 0x2604 (STF) or 0x2207 and 0x2218 (SSDC) and an independently observed shaft displacement (MO-D03) | A unit result binds to that resolution setting; a later 0x2604 change invalidates it |
| Q6 measured position on SSDC | With the drive disabled, have a person turn the shaft and read 0x6064 and 0x606C (MO-D20) | No stall, load or following-error provocation is authorized by this question |
| Q7 set-point buffer | Two small finite set-points with bit 5 = 0 while the first runs; record bit 12 and bit 10 and which targets execute (MO-D31) | No automatic retry; a lost trigger stays unknown |
| Q8 stops | On small finite moves compare quick stop with 0x605A = 1 and 2 (end state Switch on disabled), and halt in mode 1 (pause, then release) and mode 3, recording deceleration against 0x6084 and 0x6085 (MO-D05, MO-D06) | No moving link-loss test; host duration is not a physical bound after a link failure |
| Q9 watchdog | Stationary and enabled: arm 0x2060 with a host RPDO feed, stop the feed under a bounded plan and record sub 2, the action and the state; then disarm and restore (MO-D18) | Running-motion watchdog tests need their own reviewed plan; action 2 and above need a Q program qualification |
| Q10 homing | With actual switches, record the direction per method, the switch approached first, the final 0x6064 for 0x2009 = 0 and 1, and the bits 13/12/10 sequence (MO-D07) | No limit is disabled to make homing pass; index methods need an evidenced index |
| Q11 Q programs | With a program whose content the owner supplies: mode -1 start, halt and restart; 0x200B bit 14 timing. Mode -2 only after S13 has landed and with an owned SYNC schedule (UM21 p35-p36) | Program behavior is the owner's evidence, not the library's |
| Q12 persistence and node ID | With restart control: store once, restart and compare; write 0x2020, store, restart and record which of switch and stored value wins and whether a network reset activates it (MO-D16, MO-D23) | No repeated store campaign; restore communication settings afterwards and read them back |
| Q13 bitrate list | With a host adapter and analyzer that support them, try switch code 7 against 12.5 and 10 kbit/s and record which the node accepts (MO-D17) | Code 8-9 reserved; never set them |
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
6. No question writes 0x1010, 0x1011, 0x2020 or 0x2021 except Q12 with restart control, and none writes 0x2060
   except Q9; nothing is left armed, stored or changed at the end.

## Deliver

`docs/reports/moons_canopen_14a_<date>.md` with the questions actually run, exact prerequisites, raw results,
restoration outcomes and unresolved measurements; ledger conflict updates with the fixture evidence, scoped
to model, firmware and configuration; any fixture-specific qualification applied through ordinary API routes.

## Acceptance

Every question run has its prerequisites, raw evidence and restoration read-back recorded; every question not
run says why. An MO-D item closes only when the evidence covers its full claim; otherwise it stays open with
the measurement still needed. Release qualification stays with [stage 14](14_qualification_and_release.md).
