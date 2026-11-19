# 04 — State machine, enable, release, fault reset and stops

Execute this stage under the [execution contract](execution_contract.md).
Status: NOT STARTED. Requires 02b-03. MO-D05, MO-D06, MO-D19, MO-D24, MO-D28, MO-D33 and MO-D34 are owned here.
Reuse prompt 57's common enable, release and mode-select names and its firmware identity policy where landed.

UM21 reproduces the CiA 402 state machine (UM21 p15-p16) but implements a subset. Each word below is an
explicit caller operation through `prepareEnable`, `prepareRelease`, `prepareClearAlarm`, `prepareStop` and
the concrete `ActionContext`; nothing runs on binding, probe, read or recovery.

## Work

1. Enable: the documented sequence is 0x0006, 0x0007, 0x000F (UM21 p19, p88); velocity, torque and Q examples
   enable with halt set, 0x010F, and start by clearing halt (UM21 p32, p34, p35). Each step confirms the target
   state by a fresh statusword read, never by the SDO download response alone. An enable never starts motion:
   releasing halt is a separate intent of stages 05, 06 and 10. This is the stage's source-state table for
   enable (canopen_shared 02 "Operation admission"), chosen from fresh 0x6041 and 0x6061 reads:

   | Fresh state | First word, or result |
   | --- | --- |
   | Switch on disabled | 0x0006, then 0x0007, then the final word |
   | Ready to switch on | 0x0007, then the final word |
   | Switched on | The final word: 0x000F in modes 1 and 6 (UM21 p19; transition 4 clears all internal set-points, UM21 p16; homing starts only on a bit 4 edge, UM21 p22-p23), with 0x010F as the caller's option there; 0x010F, not optional, in modes 3, 4 and -1, where clearing halt is the start (UM21 p32, p34, p35), and for 0 or any value UM21 does not document |
   | Operation enabled | Already enabled, nothing sent; the result carries item 8's controlword record, so a halted drive stays halted and an unknown record stays unknown |
   | Quick stop active, Fault reaction active, Fault, Not ready to switch on | Refused before TX: transition 16 needs option codes 5-8, which UM21 p58 does not support, and a fault needs item 5's explicit reset |

   Mode -2 refuses before TX in every state (stage 10). A target already present (0x60FF, 0x6071, a selected
   Q segment) therefore never runs from an enable. AMP10 p21's single 0x001F enable-and-start is the 2010
   layout and is not used here. Never enable to prepare a stop, a fault reset or a mode change.
2. Enable prerequisites: read the enable input function 0x2617 (STF) or 0x220D (SSDC) and the interlock bit 3
   of 0x60FD first (UM21 p63, p75, p87). A configured enable input can block or remove enabling independently
   of the controlword; its default is 3 (general purpose) in UM21 but 0 in the EDS (MO-D24), so read it, never
   assume it. Report 0x200B bit 0 "Motor Enabled" (UM21 p67) as vendor evidence separate from CiA 402 state.
3. Mode select: write 0x6060 (write-only) and confirm through 0x6061 within the operation's deadline (UM21
   p35: 0x6061 updates "when the current operation mode is accepted"). Admit 1, 3, 6 and, on SSDC only, 4.
   -1 and -2 are refused here (stage 10). UM21 does not state in which drive state or motion state a mode
   change is accepted (principles CP-D24); require an explicit caller policy (disabled, or host-established
   standstill) and keep the drive's own rule an open question for 14a. A change into mode 3, 4 or -1 while
   Operation enabled is admitted only when item 8's record shows halt set (the examples select these modes
   after enabling with 0x010F, UM21 p32, p34, p35); with halt released or the record unknown it is refused
   before TX, because a target already present in that mode could start.
4. Release: which word removes motor current is not documented. Shutdown 0x0006, disable operation 0x0007 and
   disable voltage 0x0000 reach different states, and the power-stage meaning of Switched on differs between
   vendors (principles CP-D11); 0x605B and 0x605C reactions are undocumented (MO-D33). AMP10 p26's NMT "Drive
   Disabled" state does not exist (MO-D28); NMT never releases. Implement `prepareRelease` with one reviewed
   word, report 0x200B bit 0 and the CiA 402 state as separate evidence, and keep "shaft free" unobserved until
   14a. Brake sequencing by 0x2607/0x220C (UM21 p75, p83) is drive-managed and not described against the state
   machine; report the configured brake mode, claim no brake action.
5. Fault reset (`FaultResetMethod`, a MOONS'-local enum): `CONTROLWORD_EDGE` writes a word with bit 7 = 0 that
   does not enable (0x0000), then 0x0080; UM21 p16 says the reset happens only "if no fault exists currently",
   so success needs a fresh statusword without bit 3, and the state afterwards is Switch on disabled.
   `DSP_CLEAR_ALARM` writes 0x2006 = 1, which clears 0x603F and 0x200F (UM21 p66); it is a separate vendor
   action with its own evidence. Clearing 0x2217 misc flags (write 0, UM21 p79) and 0x1003 history (sub 0 = 0,
   UM21 p42) are distinct explicit operations, never side effects of a reset.
6. Fault classification: 0x603F is a vendor bit field (UM21 p56); which bits disable the motor is marked only
   in AMP10 p20 for the 2010 generation (MO-D19). Publish named bits and the CiA 402 fault bit; claim no
   severity or automatic disable for UM21. No 0x605E exists (MO-D33), so the fault reaction is unknown.
7. Quick stop: the command clears controlword bit 2 (0x000B from Operation enabled; principles s12). UM21
   names bit 2 "Quick stop" (p37, p56) and transition 11's event "Quick stop command" (p16), but p16's
   command table, checked on the rendered page, gives "Quick stop" the bits 7, 3, 2, 1, 0 = 0 x x 0 x with transitions 7, 9,
   10 and 12 (the CiA 402 disable voltage pattern) and lists no command with bit 2 clear and bit 1 set; no
   UM21 example sends 0x000B (MO-D34, added by stage 01). The word is therefore reviewed from the bit names
   and transition 11 only and stays unqualified until 14a Q8; MO-D34 stays open for 14a Q8's observed
   transition and a vendor question on the p16 table. Its
   reaction follows 0x605A, which supports only option codes 1 and 2 "at this moment" (UM21 p58); both are
   documented to end in Switch on disabled (UM21 p16 transition 12). Publish three things separately: the word
   sent, the state and flags observed in a fresh read, and the mechanical effect, which stays not observed:
   whether the shaft holds or is released afterwards is not established (no 0x605B/0x605C, brake undocumented
   against the state machine, MO-D33; 14a Q4/Q8). The power-up value of 0x605A is undocumented and its EDS
   default 0 (disable drive function) is not a supported value (MO-D05; AMP10 p16's "2 & 6 only" applies to
   the 2010 layout, stage 13). Admit a quick stop with a ramp
   claim only after a fresh 0x605A read of 1 or 2 in this node epoch. Provide an explicit setter for 1 or 2
   with readback; 0x605A is in no save list (UM21 p45-p46), so assume it is lost at reset and re-read it.
   0x6085 is the code-2 ramp in profile-acceleration units (UM21 p62; units per stage 05).
8. Halt (controlword bit 8) has no option code: 0x605D is referenced but absent (MO-D06). Record the documented
   per-mode effects: profile position stops in position control and **continues the set-point when halt is
   released** (UM21 p21), so halt is a pause and a later halt release can restart motion; homing stops
   (UM21 p22); profile velocity and torque per stage 06. For Q programs UM21 shows only the halt word 0x011F
   (UM21 p35, p89); "stops immediately and restarts from the beginning" is AMP10 p48 for the 2010 layout, so a
   UM21 Q halt publishes "halt commanded" with the program's restart point unknown (stage 10). The halt ramp
   source on UM21 is unknown. A halt-based stop retains "pending set-point" and is never released implicitly by
   any stop, recovery or reset path. Keep a per-binding controlword record for the server session (0x6040 is
   write-only, UM21 p55): the last confirmed word and whether a set-point may be pending (a halted, quick-stopped
   or `UNKNOWN` start since the last confirmed enable transition 4, which UM21 p16 says clears all internal
   set-points, unqualified until 14a). A lost reply keeps the confirmed word and the unconfirmed one as the
   record's two candidates; rebinding, a new session or a host restart makes the record unknown. Releasing
   halt to resume a pending set-point is its own explicit operation, never a step of another.
9. Stop kinds and seam S3: define the MOONS' stop kinds (quick stop, halt, disable voltage) as profile-local
   values with their documented reactions; do not reuse Lichuan's `StopKind`. NMT stop only stops PDO
   exchange (UM21 p14) and is never a stop kind. Admission by stage 03's identity outcome follows prompt 57's
   stop exemption, which removes only the identity refusal:

   | Stop | Admitting outcome | Refusing outcome, or none current |
   | --- | --- | --- |
   | Quick stop 0x000B (item 7) | Sent through `admitUrgent` without waiting for a read; its reviewed reaction covers Ready to switch on, Switched on and Operation enabled (transitions 7, 10 and 11 by the bit names, MO-D34); a ramp claim only after a fresh 0x605A read of 1 or 2 in this node epoch | Admitted with its route, binding-generation and source-state checks; the result carries the outcome and makes no ramp or standstill claim |
   | Halt, bit 8 set on the recorded word (item 8) | Admitted while item 8's record for this server session is known; after a lost start or trigger reply its candidates fix the word as the mode's documented halt, 0x010F after a velocity or torque start (UM21 p32, p34), 0x011F after a position, homing or Q trigger (UM21 p23, p35) | The same; with the record unknown it is refused with that reason, since bits 0-6 would be guessed |
   | Disable voltage 0x0000 | An explicit request; its mechanical effect is not observed (item 4) | Refused: not a quick stop or halt, so not exempt |

   The quick stop's exemption rests on UM21 p37 and p56 (bit 2 named "Quick stop") and p16 transition 11
   ("Quick stop command"), not on p16's command table (MO-D34; lead decision of 2026-11-19).
   A stop prepared or queued before the outcome changed is not withdrawn, and every other pending changing
   step on the binding is retired (prompt 57). S3: a binding keeps the default deny unless this stage's
   review shows the shape word's reaction from every state reachable after an uncertain write, a set halt bit
   included (canopen_shared 02 S3 bullet). For 0x000B it does not yet: UM21's command table contradicts the
   bit names (item 7, MO-D34), and whether 0x000B, which clears bit 8, releases a halted set-point (UM21 p21)
   before the quick stop acts is not stated. An opted-in shape is configured only while its premises hold
   (an admitting outcome and 0x605A read as 1 or 2 in this node epoch); otherwise `admitBestEffortStop` stays
   `INVALID` with its reason. Where neither a quick stop nor a halt is admitted and no shape exists, the stop
   result says "no software stop established" and names the application-owned fallback the deployment needs:
   a hardware stop such as the enable input configured by 0x2617/0x220D, which can remove enabling
   independently of the controlword (UM21 p75, p87) with an undocumented stop reaction, or power removal.
10. Owner interaction: urgent stops use `admitUrgent`; stop while queued, in flight or waiting; an interrupted
    operation retains `UNKNOWN` execution and the stop result is correlated separately. Cancel is not a stop.

## Deliver

Profile `Actions.h/.cpp`, `ActionContext`, `FaultResetMethod` and the MOONS' stop kinds; the quick-stop option
setter; `test/moons_canopen_actions_test.cpp`; console `enable`, `motor-release`, `alarm-clear`, `stop` routes
for MOONS' bindings with parity tests; S3 registration update; coverage, ledger, Doxygen and report.

## Acceptance

Tests cover each documented transition word, the first frame or no TX for every row and mode of item 1's
table (no 0x000F in modes 3, 4, -1 or an undocumented mode, nothing sent when already enabled and halted or
with an unknown record, a present 0x60FF or 0x6071 not run), a mode change into 3, 4 or -1 with halt released
refused, refused transitions, enable blocked by the input function or
interlock, mode-accept timeout, partial enable, reset with a remaining fault, both reset methods, item 9's
stop table under each identity outcome (an identity invalidated during motion: quick stop and halt with a
known record admitted, other pending steps retired; a Lichuan server under the MOONS' label: quick stop
admitted with the outcome named, halt with an unknown record and disable voltage refused), quick stop
with 0x605A unread, 0, 1, 2 and out-of-range, halt with a pending set-point, stop under queue pressure, late
replies, stale generations and no replay, and the controlword record across halt, quick stop, a lost reply and
rebinding. ACKNOWLEDGED never becomes OBSERVED without a fresh state read; an acknowledged halt or quick stop
publishes no shaft holding or release and no Q program restart point.
Quick and full verify, firmware builds and exact-commit CI pass. Release, brake and quick-stop physical effects
stay open for 14a. Hardware: NOT RUN.
