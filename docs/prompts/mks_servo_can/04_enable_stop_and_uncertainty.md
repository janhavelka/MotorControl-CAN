# 04 — Enable, stop and the no-reply and unknown-completion rules

Execute this stage under the [execution contract](execution_contract.md).
Status: NOT STARTED. Run after 02b and 03. MK-D13, D17 and D36 are central; prompt 57's enable/release policy and
prompts 55/74's per-protocol retry classes are consumed, not copied.

## Work

1. `prepareEnable` and `prepareRelease` with `nextAction`/`advanceAction`: F3H 01 holds the shaft, F3H 00 releases it,
   valid only in bus mode (MK109 p.81). A checked status 1 is `ACKNOWLEDGED` (the drive's native acceptance), status 0
   `REJECTED` with no rollback implied, no reply `UNKNOWN`. Confirmation is a separate fresh 3AH read, reported as its
   own observation. The enable state after power-on or restart is undocumented (MK-D13): never assume it, and never
   enable to prepare a stop or a stall release. Release keeps encoder tracking (MK109 p.9) but ends any commanded hold.
   Source states and first frames, under contract 02 "Operation admission": MKS has no drive state machine, so the
   starting state is fresh 3AH enable, 3EH stall and F1H run-status reads attributed `QUALIFIED` or `QUIET_INTERVAL`
   (02b step 6), plus 03's work-mode gate. The operation reads what the caller did not supply fresh before its first
   changing TX, within its one deadline.

   | Intent | Starting state | First changing TX |
   | --- | --- | --- |
   | Enable | Bus mode of the dialect, 3AH disabled, 3EH clear | F3H 01 |
   | Enable | 3AH enabled | None; already reached, reported from that read |
   | Enable | 3EH stalled | None; refused, 3DH is the separate step 5 operation |
   | Release | 3AH enabled, F1H 1 (stopped), no admitted motion or pending phase | F3H 00 |
   | Release | An admitted motion or pending phase, or F1H 2-6 | None; refused, release is not a stop |
   | Release | 3AH disabled | None; already reached |
   | Enable or release | Mode outside the dialect's bus modes or unknown; a needed read missing, stale or `possiblyLate` | None; refused, naming the missing evidence |
   | Stall clear (step 5) | 3EH stalled | 3DH |
   | Stall clear | 3EH clear | None; already reached |
   | Stall clear | 3EH missing, stale or `possiblyLate` | None; refused |

   Stops (step 2) need no starting-state read and are never refused for missing state evidence.
2. Profile-local `StopKind` (no Lichuan type reused):
   - `MODE_RAMPED`: the admitted motion's own code with speed 0 and acc not 0 (F6H p.84, FDH p.90, FEH p.94, F4H p.98,
     F5H p.102);
   - `MODE_IMMEDIATE`: the same with acc 0; the manual advises against it above 1000 rpm (p.84, p.90);
   - `EMERGENCY`: F7H, added in firmware 1.0.4 (MK109 p.1) and therefore absent from `V1_0_0` (MK100 documents
     none); "may fail to stop the unit if the current is too low" (p.81), fixed only in the Sep-2026 build (MK109
     p.2), so on `V1_0_9_NOV2025` its evidence keeps that limit;
   - `EMERGENCY_PRIORITY`: F7H 01, `V1_0_9_SEP2026` only (MK109 p.81).
   Each stop kind is admitted per dialect from its ledger row; both F7H forms are refused on `V1_0_0`. A mode stop
   needs the code of a motion this context or the application's retained record admitted. Whether one mode's stop
   code stops another mode's motion is undocumented: refuse it. With no known active motion only the F7H forms the
   dialect has are offered (none on `V1_0_0`). A motion whose interruption needs a stop the dialect lacks is not
   offered (07 step 8). None of these is a documented disable, and F7H's resulting enable state is not stated.
3. Stop replies, per stop form and phase from its ledger row, on the same code as the run (MK-D36): the position-mode
   speed-0 stops (FDH, FEH, F4H, F5H) answer 0 fail, 1 stop starting, 2 stop complete, 3 stopped by limit (MK109
   p.90, p.94, p.98, p.102); the F6H speed-0 stop documents only 0, 1 and 2 (p.84). A status the form does not
   document stays raw with execution unknown (02 step 7), never a definitive stop or completion; a 3 after an F6H
   stop may be the run's own limit stop (p.83) and claims no stop outcome. After a same-code stop is sent, a later
   status 2 means "motion ended, cause indistinguishable" (target reached or stop complete); the move's completion is
   not claimed. The move and the stop may each report, so the code's shapes enter 02b's ambiguous set (02b step 6)
   and a later same-code motion never takes a second 2 as its own. F7H status 1 is acceptance of the emergency stop,
   not observed standstill. `OBSERVED` completion is only the documented reply; F1H 1 with 32H 0 on fresh reads is a
   separate firmware-reported observation, still not a physical measurement.
4. Priority and the request slot: stops use S9's stop precedence (02b step 3) and never open a second slot beside the
   one outstanding request per address (execution contract). If that request's TX disposition is not sent, the owner
   retires it, proved unable to transmit later, and the stop takes the slot; a retired start never transmits after
   the stop. If it is sent or possibly sent with its reply outstanding, it ends locally with execution `UNKNOWN`, its
   stop obligation retained and its reply shape in 02b's ambiguous set, and the stop is admitted as new intent in the
   freed slot. Wire order is proved only when the start frame cannot transmit later: a final TX report or a
   successful retirement of that frame (02b step 3). The context requests that retirement before the stop's handoff;
   the stop never waits for it. If neither proof precedes the stop's handoff, the start may still leave the
   controller after the stop: the result records the order as unproved and the start's stop obligation survives the
   stop's own terminal, also across bus-off and recovery, until a stop is handed off after such a proof (a final
   report, a retirement or a recovery that closes `RETIRED`), as new intent and never automatically. Prefer an F7H
   form where the dialect has one: its code differs from the start's, so a late start reply is never taken for the
   stop's. A same-code mode stop leaves both immediate outcomes ambiguous. Record which was
   chosen. During a pending completion phase the slot is free and the phase stays bound.
5. `prepareClearAlarm` maps to 3DH "release stall state" with fresh 3EH reads before and after (MK109 p.55). It is
   never part of recovery, probe or startup; a new stall trips again; the drive released the shaft when protection
   tripped (p.53), and the enable state after 3DH is read, not assumed. Shaft loosening and the Enter key also clear the
   latch (p.53): a cleared flag does not prove the host cleared it.
6. No-reply rules: frames to broadcast or group IDs, 4BH, every command under 8CH respond-off, and everything during
   silent mode carry `AcknowledgementSupport::NONE`; after possible TX their execution is `UNKNOWN` and stays so until a
   separate observation. Absence of frames never proves completion, rejection or standstill. No command is repeated.
7. Unknown completion: completion "may not" arrive (95H threshold, p.89), no asynchronous frame is documented for a
   stall trip or a host-silence stop, and what the active move reports after a stall trip, a host-silence stop or an
   F7H stop is undocumented (MK-D17). A frame that does arrive stays raw and is neither the move's completion nor the
   stop's outcome; its absence proves nothing; F7H's own status 1 stays the stop's acknowledgement. A motion context
   reaching its deadline without
   a completion reply ends `DEADLINE` with completion `NOT_OBSERVED` and a retained stop obligation where motion may
   continue; it never manufactures completion or standstill. A caller fact `hostSilenceConfigured` (08) is recorded in
   the evidence because a 98H stop can end motion silently (MK-D18).
8. Restart, bus-off and rebinding invalidate enable evidence, pending phases and stop obligations' basis; recovery
   never re-enables, clears a stall or replays a stop that was transmitted.

## Deliver

`Actions.h/.cpp`, `ActionContext`, `StopKind`, enable/release/stop/clear preparations,
`test/mks_servo_can_actions_test.cpp`, owner-backed app and console routes (`mks enable`, `mks release`, `mks stop
KIND`, `mks stall-clear`), API/CLI parity (console routes and parity once S4 lands), coverage rows with per-action
acknowledgement and completion evidence, report and PROGRESS row.

## Acceptance

Tests through the real owner and fake drive: the first emitted frame, or none, for every row of the step 1 table;
an enable whose 3AH read meets its own timeout while the operation deadline arrives before F3H (no F3H TX, contract
02 "Operation admission"); success, fail and absent replies; reply after deadline; a success frame
before the request's handoff not matched; stop while the start is queued unsent (start retired, never transmitted
after the stop, also under queue pressure), possibly sent (start `UNKNOWN` with its stop obligation, late start reply
`possiblyLate`), handed to the controller without a final report while the stop is admitted (the fake controller
sends the start after the stop; order unproved and the stop obligation retained, under queue pressure and across
bus-off and recovery) and during a pending completion phase; same-code stop ambiguity and a second status 2 never
taken by a later same-code motion; a status 3 or other undocumented status after an F6H stop, and any frame of the
interrupted move after F7H, kept raw and never a definitive stop or completion; F7H plain versus 01 per dialect (both
refused on V1_0_0, 01 refused on MK109A); cross-mode stop refused; stall clear with stale and
fresh 3EH; restart invalidation; no repeat after any lost reply; local cancel not a stop. The full matrix of contract
02 "Registration acceptance" for this major change and CI on the exact final pushed commit pass. Hardware NOT RUN.
Commit and push before any motion stage relies on these stops.
