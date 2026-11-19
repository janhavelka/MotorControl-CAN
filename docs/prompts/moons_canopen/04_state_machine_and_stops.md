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
   releasing halt is a separate intent of stages 05, 06 and 10 and of item 8's halt release. This is the stage's source-state table for
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
   14a. The word is Shutdown 0x0006 (UM21 p16 command 0 x 1 1 0, transitions 2, 6, 8; lead decision of
   2026-11-19): UM21 p16 gives transitions 8 (from Operation enabled) and 6 (from Switched
   on) the action "high-level power shall be switched off, if possible", ending in Ready to switch on, while
   disable operation 0x0007 (transition 5) only disables the drive function and 0x0000 is item 9's
   disable-voltage stop. Source-state table, from fresh 0x6041 and 0x6061 and item 8's record:

   | Fresh state | First word, or result | Settled by |
   | --- | --- | --- |
   | Operation enabled | 0x0006, or 0x0106 with halt possibly set (item 8's bit 8 rule) | A fresh 0x6041 showing Ready to switch on; 0x200B bit 0 reported separately; no current or shaft claim |
   | Switched on | 0x0006 (transition 6) | The same |
   | Ready to switch on, Switch on disabled | Already released in CiA 402 terms: nothing sent; the result carries the state, 0x200B bit 0 and "shaft free not observed" | No TX |
   | Quick stop active, Fault reaction active, Fault, Not ready to switch on | Refused before TX: transitions 12 and 14 are the drive's own (UM21 p16), and a fault needs item 5 | No TX |

   Brake sequencing by 0x2607/0x220C (UM21 p75, p83) is drive-managed and not described against the state
   machine; report the configured brake mode, claim no brake action.
5. Fault reset (`FaultResetMethod`, a MOONS'-local enum): `CONTROLWORD_EDGE` writes a word with bit 7 = 0 that
   does not enable (0x0000), then 0x0080; UM21 p16 says the reset happens only "if no fault exists currently",
   so success needs a fresh statusword without bit 3, and the state afterwards is Switch on disabled.
   `DSP_CLEAR_ALARM` writes 0x2006 = 1, which clears 0x603F and 0x200F (UM21 p66); it is a separate vendor
   action with its own evidence. Clearing 0x2217 misc flags (write 0, UM21 p79) and 0x1003 history (sub 0 = 0,
   UM21 p42) are distinct explicit operations, never side effects of a reset. Source-state table, from fresh
   0x6041 and 0x603F:

   | Fresh state | `CONTROLWORD_EDGE` | `DSP_CLEAR_ALARM` |
   | --- | --- | --- |
   | Fault | 0x0000, then 0x0080; settled by a fresh statusword without bit 3 (Switch on disabled); with the fault remaining, "fault remains" and no repeat | 0x2006 = 1 when 0x603F reads nonzero; settled by a fresh 0x603F = 0; the CiA 402 state afterwards is reported, not claimed, since UM21 p66 states no state effect |
   | Switch on disabled, Ready to switch on, Switched on | No CiA 402 fault: nothing sent (0x0000 here would be a state command) | As in Fault; with 0x603F = 0 nothing sent ("no alarm") |
   | Operation enabled, Quick stop active | No CiA 402 fault: nothing sent | Alarm-clear guard: UM21 p66 states no effect of an alarm clear on an enabled or stopping drive, so with the guard on (the default, settable per binding) it is refused before TX; with it off 0x2006 = 1 is sent as in Fault and the admitted result names the guard that was off and states that a pending target may resume (prompt 77's guard pattern; the owner's 2026-11-17 direction) |
   | Fault reaction active, Not ready to switch on | Refused before TX: transition 14 or initialization is the drive's own | Refused before TX |
6. Fault classification: 0x603F is a vendor bit field (UM21 p56); which bits disable the motor is marked only
   in AMP10 p20 for the 2010 generation (MO-D19). Publish named bits and the CiA 402 fault bit; claim no
   severity or automatic disable for UM21. No 0x605E exists (MO-D33), so the fault reaction is unknown.
7. Quick stop: the command clears controlword bit 2 (0x000B from Operation enabled, 0x010B with halt possibly
   set by item 8's bit 8 rule; principles s12). UM21 names bit 2 "Quick stop" (p37, p56) and transition 11's
   event "Quick stop command" (p16), but p16's
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
   record's two candidates; rebinding, a new session or a host restart makes the record unknown. Halt is
   possibly set when a candidate has bit 8 set or the record is unknown.

   Bit 8 rule (canopen_shared 02 "State transitions"): a word that leaves Operation enabled (item 4's release,
   item 7's quick stop, item 9's disable voltage) keeps bit 8 set while halt is possibly set, because no UM21
   page states that the quick stop or a state command takes precedence over the halt release of p21. The
   evidence admits those words with bit 8 set: p16 codes every state command in bits 7, 3, 2, 1 and 0 only,
   p56 says "All implemented bits of the controlword are valid independent of the PDS FSA state" and places
   Halt at bit 8, p37 gives bit 8 Halt in every mode, and p32, p34 and p35 send a state command with bit 8 set
   (0x010F). So the quick stop is 0x010B, the release 0x0106 and disable voltage 0x0100 in that case, all
   checked on the rendered pages; no UM21 example sends them, and they stay unqualified with MO-D34 until 14a Q8.

   Releasing halt to resume a pending set-point is its own explicit operation, `prepareHaltRelease` through
   `ActionContext`, never a step of another. It is a changing request: identity admission by outcome, the
   write preflight and one operation deadline apply, and no stop exemption. Source-state table, from fresh
   0x6041 and 0x6061 and the record:

   | Predecessor | First word, or result |
   | --- | --- |
   | Operation enabled, 0x6061 = 1, no fault, the record known with one candidate, bit 8 set | That word with only bit 8 cleared (0x011F to 0x001F, 0x010F to 0x000F), so bit 4 makes no new edge. Acknowledged by the reply; continuation observed only in a later fresh read; completion follows stage 05 item 8 against the retained target |
   | The same with bit 8 clear in the record | Not halted: nothing sent |
   | The record unknown or holding two candidates | Refused before TX: the other bits would be guessed |
   | Modes 3 and 4 | Refused before TX: clearing halt is the run start, which stage 06's finite run owns with its duration and stop path |
   | Modes 6 and -1 | Refused before TX: UM21 p22 and p35 do not say whether a released halt continues or restarts homing or a Q program (14a Q11) |
   | Any state other than Operation enabled | Refused before TX; the next enable's transition 4 clears the set-points (UM21 p16), so nothing remains to resume |
9. Stop kinds and seam S3: define the MOONS' stop kinds (quick stop, halt, disable voltage) as profile-local
   values with their documented reactions; do not reuse Lichuan's `StopKind`. NMT stop only stops PDO
   exchange (UM21 p14) and is never a stop kind. Admission by stage 03's identity outcome follows prompt 57's
   stop exemption, which removes only the identity refusal:

   | Stop | Admitting outcome | Refusing outcome, or none current |
   | --- | --- | --- |
   | Quick stop 0x000B with halt known clear, 0x010B with halt possibly set (items 7 and 8) | Sent through `admitUrgent` without waiting for a read; its reviewed reaction covers Ready to switch on, Switched on and Operation enabled (transitions 7, 10 and 11 by the bit names, MO-D34); a ramp claim only after a fresh 0x605A read of 1 or 2 in this node epoch | Admitted with its route, binding-generation and source-state checks; the result carries the outcome and makes no ramp or standstill claim |
   | Halt, bit 8 set on the recorded word (item 8) | Admitted while item 8's record for this server session is known; after a lost start or trigger reply its candidates fix the word as the mode's documented halt, 0x010F after a velocity or torque start (UM21 p32, p34), 0x011F after a position, homing or Q trigger (UM21 p23, p35) | The same; with the record unknown it is refused with that reason, since bits 0-6 would be guessed |
   | Disable voltage 0x0000, or 0x0100 with halt possibly set (item 8) | An explicit request; its mechanical effect is not observed (item 4) | Refused: not a quick stop or halt, so not exempt |

   The quick stop's exemption rests on UM21 p37 and p56 (bit 2 named "Quick stop") and p16 transition 11
   ("Quick stop command"), not on p16's command table (MO-D34; lead decision of 2026-11-19).
   A stop prepared or queued before the outcome changed is not withdrawn, and every other pending changing
   step on the binding is retired (prompt 57). S3: a binding keeps the default deny unless this stage's
   review shows the shape word's reaction from every state reachable after an uncertain write, a set halt bit
   included (canopen_shared 02 S3 bullet). For the quick stop it does not yet: UM21's command table
   contradicts the bit names (item 7, MO-D34). A shape would also have to be 0x010B, since no halt state is
   known on an unresolved route (item 8's bit 8 rule), but MO-D34 alone keeps the default deny. An opted-in
   shape is configured only while its premises hold (an admitting outcome and 0x605A read as 1 or 2 in this
   node epoch); otherwise `admitBestEffortStop` stays
   `INVALID` with its reason. Where neither a quick stop nor a halt is admitted and no shape exists, the stop
   result says "no software stop established" and names the application-owned fallback the deployment needs:
   a hardware stop such as the enable input configured by 0x2617/0x220D, which can remove enabling
   independently of the controlword (UM21 p75, p87) with an undocumented stop reaction, or power removal.
10. Owner interaction: urgent stops use `admitUrgent`; stop while queued, in flight or waiting; an interrupted
    operation retains `UNKNOWN` execution and the stop result is correlated separately. Cancel is not a stop.
    After a timeout the route stays unresolved under `SdoReuse` `NONE` (02b item 5): `admitUrgent` keeps it
    unavailable (`adapters/owner/CanBusOwner.h`, "quarantined routes remain unavailable") and
    `admitBestEffortStop` returns `INVALID` without a shape, so no stop frame leaves on that binding. The stop
    result then says "no software stop on the unresolved route" and names item 9's application-owned fallback.

## Deliver

Profile `Actions.h/.cpp`, `ActionContext`, `FaultResetMethod`, `prepareHaltRelease` and the MOONS' stop kinds;
the quick-stop option setter; `test/moons_canopen_actions_test.cpp`; console `enable`, `motor-release`,
`alarm-clear`, `stop` and a MOONS' family `halt-release` route for MOONS' bindings with parity tests; S3
registration update; coverage, ledger, Doxygen and report.

## Acceptance

Tests cover each documented transition word, the first frame or no TX for every row and mode of item 1's
table (no 0x000F in modes 3, 4, -1 or an undocumented mode, nothing sent when already enabled and halted or
with an unknown record, a present 0x60FF or 0x6071 not run), a mode change into 3, 4 or -1 with halt released
refused, refused transitions, enable blocked by the input function or
interlock, mode-accept timeout, partial enable, the first frame or no TX for every row of item 4's release
table (0x0106 with halt possibly set, nothing sent when already released), item 5's table for both reset
methods (nothing sent without a fault or alarm; 0x2006 while enabled refused with the alarm-clear guard on and
sent with it off, the result naming the guard and that a pending target may resume), reset with a remaining fault,
item 8's halt-release table (the exact mode 1 word, refusal with an unknown or two-candidate record, in modes
3, 4, 6 and -1 and outside Operation enabled), item 9's stop table under each identity outcome with halt known
clear (0x000B), set (0x010B) and unknown (0x010B) predecessors (an identity invalidated during motion: quick
stop and halt with a known record admitted, other pending steps retired; a Lichuan server under the MOONS'
label: quick stop admitted with the outcome named, halt with an unknown record and disable voltage refused), a
stop after a timeout under `NONE` with zero stop frames and item 10's result, quick stop
with 0x605A unread, 0, 1, 2 and out-of-range, halt with a pending set-point, stop under queue pressure, late
replies, stale generations and no replay, and the controlword record across halt, quick stop, a lost reply and
rebinding. ACKNOWLEDGED never becomes OBSERVED without a fresh state read; an acknowledged halt or quick stop
publishes no shaft holding or release and no Q program restart point.
Quick and full verify, firmware builds and exact-commit CI pass. Release, brake and quick-stop physical effects
stay open for 14a. Hardware: NOT RUN.
