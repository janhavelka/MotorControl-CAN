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
   | Enable, release or stall clear | Step 4's admission barrier on the address | None; refused (stops stay admitted) |
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
   status 2 means "motion ended, cause indistinguishable" (target reached or stop complete): neither the move's
   completion nor the stop's is claimed. It stays a recorded observation and discharges no obligation, also when
   02b step 6 attributes it `QUIET_INTERVAL`, because the interval accepts timing risk in attribution, not which
   operation produced the frame; only step 4's post-stop observation discharges. The move and the stop may each
   report, so the code's shapes enter 02b's ambiguous set (02b step 6). A later same-code motion, admitted only after
   this obligation is discharged, never takes a second 2 as `QUALIFIED`; once that motion's quiet interval has passed,
   05 step 7 lets such a 2 end it only with `QUIET_INTERVAL` correlation (the risk the interval accepts), never as an
   outcome of the stopped move or of the stop. F7H status 1 is acceptance of the emergency stop, not observed
   standstill. No stop reply is therefore `OBSERVED` completion; F1H 1 with 32H 0 on fresh reads (step 4's post-stop
   observation) is a separate firmware-reported observation, still not a physical measurement.
4. Priority and the request slot: stops use S9's stop precedence (02b step 3) and never open a second slot beside the
   one outstanding request per address (execution contract). If that request's TX disposition is not sent, the owner
   retires it, proved unable to transmit later, and the stop takes the slot; a retired start never transmits after
   the stop. If it is sent or possibly sent with its reply outstanding, it ends locally with execution `UNKNOWN`, its
   stop obligation retained and its reply shape in 02b's ambiguous set, and the stop is admitted as new intent in the
   freed slot. Wire order is proved only when the start frame cannot transmit later: a final TX report or a
   successful retirement of that frame (02b step 3). The context requests that retirement before the stop's handoff;
   the stop never waits for it. If neither proof precedes the stop's handoff, the start may still leave the
   controller after the stop: the result records the order as unproved and the start's stop obligation survives the
   stop's own terminal, also across bus-off and recovery, until a stop meeting the discharge ordering below, as new
   intent and never automatically, is followed by its post-stop observation.
   Prefer an F7H form where the dialect has one: its code differs from the start's, so a late start reply is never
   taken for the stop's. A same-code mode stop leaves both immediate outcomes ambiguous. Record which was chosen.
   During a pending completion phase the slot is free and the phase stays bound; once a stop is admitted the phase
   only records observations.
   **Motion record and admission barrier.** Per interface and address the service keeps one motion record, kept
   across rebinding, bus-off and recovery. Its conditions can hold together; a changing operation on the address
   (enable, release, stall clear, 05-07 motion, 08 and 10 writes, 09 dispatch that reaches it) is admitted only when
   every holding condition admits it, and stops (step 2's dialect rows) always are. Reads stay available, but a
   stopped report taken before a late start proves nothing about it. "Attributed" means `QUALIFIED` or
   `QUIET_INTERVAL` under 02b step 6. "Settled" and "discharge ordering" are defined below the table.

   | Condition | Holds while | Changing TX admitted besides stops | Ends by |
   | --- | --- | --- | --- |
   | Lost history | From first attachment of the address, and from a host restart unless the application restores this record from its own storage: the drive may still run or hold work this host has no record of (02b step 6) | None | A stop under the discharge ordering with its post-stop observation, or the caller's explicit reconciliation record, a caller declaration under the owner's 2026-11-17 direction (an evidence reference such as a recorded power cycle with FFH and 9AH read off, or an operator record), named in later results as accepted risk (round 5 lead decision 1) |
   | Calibration | An 80H (08 step 4) is admitted and has not ended here; its deadline, a local cancel, a lost reply, rebinding, bus-off and recovery do not end it. This row, not uncertain execution or a retained obligation, covers 80H | None. Stops stay admitted, but no page says a stop interrupts calibration, so a stop ends nothing here | A disposition proving the 80H unsent; its terminal status 1 or 2 (MK109 p.41) attributed after its handoff; or, once the 80H is settled, two sequential attributed F1H reads (stream state `OFF`) reporting neither 6 (calibrating) nor 0 (query failed), the firmware's documented calibration report (MK109 p.80); then 08 step 4's 40H read |
   | Live intent | One admitted motion operation (05, 06, 07 homing or zeroing, a 09 step 7 latched staging) is pending: not cancelled, superseded, stopped, failed or past its deadline, in the binding generation and identity session that admitted it | Only that operation's same-intent transitions below | Its type's terminal evidence below (nothing remains). Any other end (local cancel, deadline, failure, a status 3 limit stop, an admitted stop, restart, bus-off, rebinding) leaves a retained obligation, except that an attributed status 0 rejecting a start with no earlier trigger of the operation leaves nothing, and a latched staging with its attributed status 5 and no 4BH since leaves `LATCHED_STALE` instead (09 step 7) |
   | Open TX | A changing frame to the address (start, update, latched staging, mode stop, 4BH or multi-target frame) may still transmit: handed to the owner without a final TX report, a successful retirement or a recovery closing `RETIRED` (02b step 3) | None | That proof for every such frame |
   | Uncertain execution | A trigger's acceptance (status 1, or 5 for a latched staging) is not attributed: its reply missing at the request's deadline, `possiblyLate` or of an undocumented status (02 step 7); or a 4BH may have reached a `LATCHED_LIVE` member (09 step 7) | None | No observation clears it, a 3BH transition after a lost 91H start reply included (07 step 4). The operation's end makes it a retained obligation; only a latched move's own terminal evidence (09 step 7) ends it with the live intent |
   | Retained obligation | Motion may continue, or a received command may still run, after its operation ended: step 7's `DEADLINE`, a start preempted or unanswered, a stop with unproved order (above), a 4BH possibly reaching a member whose latch was not `NONE` or `LATCHED_LIVE`, a multi-target motion frame (09 step 6), recovery | None | A stop, or a drive-side limit stop (an attributed status 3, no host stop frame needed, round 4 lead decision 2), that meets the discharge ordering below, then its post-stop observation |

   **Discharge ordering**, defined here once; 02b, 05-10 consume it by reference.
   - *Settled*: a changing frame is settled once proved unable to transmit later (a final TX report, a successful
     retirement or a recovery closing `RETIRED`, 02b step 3). Until then it may still reach the drive after any
     observation.
   - *Stop*: it counts only when handed off after every earlier changing frame to the address is settled, and when its
     own TX is then settled as one that could have reached the drive (a final TX report, or possibly sent and then
     closed `RETIRED` by recovery). A stop proved unsent (retired before transmission) discharges nothing; the
     obligation stays for a new stop. This asks for TX evidence, not a device acknowledgement.
   - *Limit stop*: an attributed status 3 counts only when its capture (earliest bound) follows the settlement of every
     earlier changing frame to the address. A 3 captured before that settlement discharges nothing, also when the
     proof arrives later, because an earlier frame could have transmitted after the limit hit and started motion.
   - *Post-stop observation*: F1H 1 with 32H 0 on two reads, the first handed off after the stop's settlement (or the
     status 3's capture), the second after the first's reply, both attributed with their codes' stream state `OFF`
     (03 step 8), with no changing frame to the address, restart, bus-off, rebinding or recorded power cycle between.
   - *Deciding reads*: any read that decides about a changing frame (08 step 9's readback, 10 step 2's deciding reads,
     02b step 6's recorded power cycle as a boundary) counts only when taken after that frame is settled; a reading
     taken while it may still transmit stays an observation.

   Same-intent transitions are admitted only while the record holds that live intent and nothing else (no open TX,
   uncertain execution or retained obligation); otherwise they refuse before TX. An F5H update (05 step 8) on a live
   F5H move of a dialect with the live update, after the latest trigger (the start or the previous update) has its
   attributed status 1 and with no request outstanding on the address (the update takes the slot): the update is a
   new trigger with its own open TX and acceptance, and the move's completion restarts from it. A 4BH (09 step 7) when every possible recipient's record
   holds nothing, or holds only a live latched staging with its attributed status 5 and its deadline after the 4BH
   handoff. 09's per-drive latch acceptance (round 3 lead decision) answers only the latch-state gate
   (`LATCHED_STALE`, `UNKNOWN`, `POSSIBLY_ARMED`); it never admits a 4BH over a recipient with open TX, uncertain
   execution, a retained obligation or a live intent other than a latched staging.

   Terminal evidence that discharges each admitted motion type, the latest trigger's acceptance and the terminal frame
   attributed and no same-code stop handed off since that trigger (a status 2 after one is step 3's observation; one
   decision per form, never a bare status): finite position, 05 step 7's
   `completed`; a finite F6H run with drive run time, its status 2 after its attributed status 1 (06 step 7); homing
   and 91H 01 zeroing, 07 step 4's completion; a latched move, 09 step 7's form-specific rule. A host-bounded run, a
   native continuous F6H run and a form without such evidence are discharged only by a stop and the post-stop
   observation. A stop's own reply discharges nothing: a mode stop shares its code with the motion it stops (MK-D36),
   so its status 2 is cause-indistinguishable even when attributed `QUIET_INTERVAL`, and F7H documents only
   acceptance (step 3).
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
bus-off and recovery, then a release and an enable attempted after the first stop's terminal and a fresh F1H 1,
each refused with no TX until the start frame is proved and the obligation discharged; the 82H and 84H writes of
the same scenario are 08's, through its real settings route) and during a pending completion phase; each condition
of the step 4 motion record refusing enable, release and stall clear with no TX while a stop stays admitted, and
ending only by its own "Ends by" column (a lost start reply staying uncertain after fresh stopped reads; a retained
obligation discharged by a stop after proof and the post-stop observation, not by the stop's reply, a single read or
reads with a changing frame between); the discharge ordering: a stop held below the owner queue while both
post-stop reads complete, then retired unsent (obligation retained, a following release refused with no TX), and an
attributed status 3 captured before a held start frame's final TX report, the start then transmitting (no
discharge); lost history: first attachment, and a host restart during a continuous F6H run, each refusing enable,
release and a move with no TX until a stop with its post-stop observation or the caller's reconciliation record; a
run's status 2 attributed `QUIET_INTERVAL` after a same-code stop's handoff
with the stop's own reply withheld, recorded as an observation with the obligation retained and a following
release refused with no TX until the post-stop observation; a stop admitted on missing, stale and contradicting
identity (03 step 5), its result carrying that identity and claiming no standstill; same-code stop ambiguity and a
second status 2 never `QUALIFIED` for a later same-code motion and never the stopped move's or the stop's outcome
(after that motion's quiet interval it ends it with `QUIET_INTERVAL` correlation, as 05's acceptance tests); a
status 3 or other undocumented status after an F6H stop, and any frame of the interrupted move after F7H, kept raw
and never a definitive stop or completion; F7H plain versus 01 per dialect (both refused on V1_0_0, 01 refused on
MK109A); cross-mode stop refused; stall clear with stale and
fresh 3EH; restart invalidation; no repeat after any lost reply; local cancel not a stop. The full matrix of contract
02 "Registration acceptance" for this major change and CI on the exact final pushed commit pass. Hardware NOT RUN.
Commit and push before any motion stage relies on these stops.
