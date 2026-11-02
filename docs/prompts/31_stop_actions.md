# Prompt 31 Stop operations and owner integration

Apply [COMMON.md](COMMON.md). Prerequisites: [24](24_owner_recovery.md), [30](30_drive_actions.md).

## Objective and boundaries

Implement reviewed normal/halt and fast/quick-stop actions, then integrate real stop work with the already tested owner urgent/cancellation/retirement mechanisms. This is the first actual Lichuan stop integration; prompt-24 test fixtures did not implement drive behavior. Fault clear, device counter changes and position/velocity operations remain separate tasks.

The delivered subset stops only from a latest acknowledged `000F` on a resolved route. This prompt also owns the contract that W5 and W6 of the [repair audit](PROMPTSET_AUDIT_34_50_2026-11-02.md) deliver under its [DEC-2, DEC-3 and DEC-8](PROMPTSET_AUDIT_34_50_2026-11-02.md#3-settled-decisions): a running move can be stopped (C-16), the stop procedure does not destroy the route (C-17), a stop can be sent on an unresolved route (C-01) and urgent capacity gives a usable stop (C-02). A CAN stop is best effort and not a safety function. An independent physical stop stays mandatory on the bench.

Use [CONVENTIONS](CONVENTIONS.md#3-motion-typed-reads-and-terminal-evidence), its [owner contract](CONVENTIONS.md#4-one-application-can-owner), and [MULTIDRIVE](MULTIDRIVE.md). NMT stop, local cancel, release and communication silence are not interchangeable with a documented motor stop.

## Read and audit

For this prompt, refresh MotorControl-RS HEAD and dirty state and read the RS code for this seam; RS is authoritative. Check HEAD and dirty state only for the siblings named below, and inspect only the seam stated. Inspect current FieldCore-node read-only for ownership or integration changes. If HEAD, dirty state and seam match the last recorded row, write "unchanged since prompt N" and do not reread. A sibling row must name a behavior adopted or rejected, with path and symbol. This does not relax vendor evidence, safety, uncertainty or verification rules. Read RS stop/action interruption and owner urgency tests, plus its retained uncertain-operation evidence, including the held stop, escalation and in-flight settling behavior in `docs/console.md`, `ProbeApp` and `test/stop_path_app_test.cpp` (R-14). Compare PCA9555 cancellation and INA228 validity. Inspect FieldCore stop admission and independent outcomes read-only. Recheck original Lichuan stop/option-code/controlword evidence and the selected firmware gate; absent `0x605A/0x6085` or vague `0x605D` semantics cannot be supplied from another drive.

### Concrete evidence to recheck

Recheck [stop evidence](../audit/LICHUAN_MOTION_FACTS.md): ZH86 p12 and OP86 p10 give normal stop `15 -> 271` (`0x000F -> 0x010F`); EN86 omits that row. All three give `15 -> 11` (`0x000F -> 0x000B`) as emergency stop. These are command examples, not a certified emergency-stop function, latency, braking curve or retained-torque guarantee. `0x605A/0x6085` are absent; `0x605D` is I16 RW/default 1 with unresolved options. Do not invent quick-stop settings or infer physical standstill from a reply or NMT state.

The manuals give whole-word stop arrows only from word 15. No manual defines bit 2 or bit 8: the bit tables cover bits 4 to 6, and bit 7 on p15. No manual says what a falling bit 4 does in mode 1. So every row below the first rests on derived words. Allowing them is a project decision, not vendor evidence, because a running move with no stop is worse, and clearing bit 4 while stopping cannot be the wrong direction. Stops from 31 or 95 are not documented arrows; use this DEC-8 table and carry `predecessorDocumented=false`.

| Mode | Latest acknowledged `6040` | Stop word | Status |
| --- | --- | --- | --- |
| 1 | `000F` | `010F` or `000B` | Whole words documented; `010F` in ZH86 and OP86 only. Implemented today |
| 1 | `001F`, `005F`, `003F`, `007F` | `010F` or `000B` | Arrow not documented; `predecessorDocumented=false`; measure under Q12 |
| 1 | `010F` | `000B` only | Escalation of a running normal stop (R-14); `predecessorDocumented=false` |
| Any | Unknown, or state evidence stale | `000B` only | DEC-2 path. This widens DEC-2 to stale evidence on a clean route |

W6 widens mode 1 only; prompts 35 and 36 add modes 3 and 6 with their operations. A prior `000B`, a released state or any other word still rejects in `prepareStop`, and a prior `010F` allows only `000B`. A later `stop normal` never replaces a running or held `stop fast`. Rearming to `000F` mid-curve to regain a predecessor is not a stop path, because the falling edge in mode 1 is undocumented.

The DEC-2 path uses only `000B`, the arrow `15->11` labelled emergency stop in EN86 p12, ZH86 p12 and OP86 p10. The label covers that arrow only; other predecessors are unmeasured (W11 step 6). Never `010F` here: it has the enable-operation bits set, and the vendor table `6040=15` gives status low bits `111`, so it could enable a released drive under CiA402 (from memory). That `000B` does not enable is also CiA402 from memory, because no manual defines bit 2. Bench-check it before relying on it.

Evidence route: prompt 29 conditional experiment or manufacturer clarification; do not wait for prompt 47.

## Implement

1. Specify the stop transition table before implementation: supported stop kinds, initial states, command/setup, option-code prerequisites, expected final state, target/queue disposition, reference/torque consequences and required new observations. The manual arrows are transition examples, not mandatory two-write scripts: never send `0x000F` or the `6,7,15` enable sequence to prepare a stop. Use an evidenced current-state transition from the table above or reject before TX.
2. Implement `prepareStop` through existing `nextAction`/`advanceAction` and concrete action context evidence. Widen it for mode 1 per the DEC-8 table and add `predecessorDocumented` to `ActionContext`. Unsupported stop kinds or unproved prerequisites fail before work. Acknowledged command and observed stopped state remain distinct; neither claims independent physical standstill.
   - `maximumAgeUs` bounds reads, not the `6040` history: remove it from `controlEvidence`, because a drive idle at `000F` must not lose its stop (C-18).
   - The application session keeps the latest `6040` terminal per axis and withdraws it on any new `6040`, target, mode, RPDO or configuration write, reset, rebind, recovery or uncertain submission, so a copied `000F` cannot stay eligible (C-19). This change and C-18's share a commit.
   - Reject at preparation a `maximumAgeUs` below the field count times `queueTimeoutUs + responseTimeoutUs` (A-20). Actual cycle time is unknown at preparation, so the `OBSERVATION_SCOPE` failure on a first complete cycle stays reachable through service gaps; test that path too.
3. Admit stop as its own urgent work with a finite deadline and reserved terminal result (C-02). Integrate the real profile action with owner capacity and occupied-channel policy rather than adding a separate priority receiver. W6's design, whose safety rule stays if its mechanism has to change:
   - `URGENT_REQUESTS = SEQUENCES`, with the urgent slot index `REQUESTS` plus the channel, so one axis cannot take both slots and an unread stop on one axis does not block the other.
   - `beginUrgentOperation(id, operation, deadline, now)` retires this sequence's unsubmitted work as terminal with no TX, leaves an in-flight request to settle and gives the stop its own operation and deadline.
   - `beginSequence` reuses an invalidated slot once `live` is false, so it stops blocking its axis while retained results stay readable. `findSequence(binding)` lets another producer find the sequence.
4. Retire stale queued work at every established software/adapter/controller boundary. Identify which entries are definitely retired and which submitted work remains uncertain. A stop never cancels in-flight work: cancel queued work only, because a cancelled handed-off poll quarantines the route (C-17). The stop waits for in-flight work to settle. If the settling request leaves the route unresolved and the stop never transmitted, the application issues the best-effort stop of item 7 with a new deadline. An ordinary stop cannot invent channel availability, and no stop can claim an ambiguous late SDO reply. A PDO stop path needs confirmed mapping and documented behavior; it is not a shortcut, because boot NMT state and PDO behavior are undocumented (D-05).
5. Preserve interrupted operation identity and outcome. Stop success updates its own evidence but never erases a failed/unknown original operation. Define before/after-stop sequence admission and generation checks so an earlier queued target cannot execute afterward.
6. Keep event/deadline/cancellation precedence deterministic, with one processing-time cutoff: `now >= deadlineUs` expires, even if the frame was received earlier ([CONVENTIONS section 3](CONVENTIONS.md#3-motion-typed-reads-and-terminal-evidence)). A failed, cancelled or expired stop retains its uncertainty. Any later stop is another explicitly admitted action, not an extension of an expired operation or automatic replay.
7. Best-effort stop on an unresolved route (DEC-2). A motor that may be moving is exactly when the host needs a stop, so an explicitly requested urgent `6040:00=000B` may be transmitted on an unresolved route or with stale state evidence. Ordinary `admit` and `admitUrgent` refusals stay as they are.
   - W6 adds owner `admitBestEffortStop(const SdoTransfer&, now, id)` on W5's `Slot.outsideSequence` path. It accepts only `6040:00=000B` and uses the reserved urgent slot. It never bypasses BUS_OFF, RECOVERING, known STOPPED, INITIALIZING or an exhausted node.
   - A separate small `BestEffortStop` context keyed by `ReadTarget`, not `ActionContext`, yields word `000B` and needs no state evidence. It never reaches SUCCEEDED. A transmitted TX report gives `STOP_SENT_UNCONFIRMED`; possible transmission with no report gives DEADLINE; both have `execution=UNKNOWN`. No possible transmission gives NOT_TRANSMITTED.
   - The reply never qualifies, because a late `6040` reply is indistinguishable from the stop's reply. `execution` stays UNKNOWN whatever arrives, and the route stays unresolved. Until the final TX report, a frame received on this route is recorded as unqualified and does not retire the handoff, because a late reply would otherwise withdraw the stop. These replies do not refresh service qualification.
   - The stop's effect can only be shown by later independent observation, and it never settles the earlier operation's uncertainty.
   - One request, one transmission, no resend. A second stop is a new explicit request.
   - "Never transmitted" means `transmissionPossible==false`, nothing weaker. A refused enqueue may still have been accepted.
8. Recovery and escalation (R-14). Hold a never-transmitted stop across one bounded recovery, then send it once as `000B` under the deadline fixed at admission, even if `010F` was requested, because it has lost its state evidence. RS keeps the requested policy here; this is an intentional difference, so record it. A second fault ends it not transmitted. A lost reply stays UNKNOWN and is never resent. `stop fast` escalates a running `stop normal`, and never the reverse. Console verbs wait for prompt 42.
9. Route reopening (DEC-3, delivered by W5). A matched server abort has the same correlation premise as a success, so it leaves the channel open. An unresolved route reopens only through an explicit application-requested drain marker: all TX on the route retired, then one upload of a DEC-1 accepted object whose index and subindex differ from every pair retained on that route, with every reservation on that node invalidated before reopening. A marker settles the channel only. Earlier effects stay uncertain, `000F` history stays consumed, new work needs a new reservation, and nothing replays or re-enables. It assumes this host is the sole SDO client on these COB-IDs, and that the server answers one request at a time and in order; the second is CiA 301 from memory and is not vendor-confirmed, so it is recorded under Q18 and measured. Quiet time, heartbeat, a new host epoch, rebuilding the client and same-object readback remain insufficient, so their negative tests stay.
10. This prompt owns the `605D` read row: a typed read of the raw I16 with no option meaning or braking claim. `605D` is not on the DEC-1 accepted list, so its read effect stays gated by Q20.

## Verify and exit

Run native owner/action tests for each supported stop kind/state, occupied and quarantined channels, full ordinary/result capacity, stop on another node, stale queued targets before/after stop, adapter retirement uncertainty, lost reply, conflicting completion evidence, fault/reboot and exact deadline boundaries. Use concrete non-motion work fixtures until later move APIs exist; later prompts add their real interrupted contexts.

Add through the real owner:

- Enable, trigger `001F`, poll in flight, stop: the stop is sent after the poll settles.
- Lost trigger reply, best-effort stop admitted and transmitted, identical late reply: stays unqualified and the route stays unresolved.
- Lost poll reply while a stop waits.
- Stop on axis 2 while axis 1's stop is unread.
- Stop from another producer through `findSequence`.
- `000B` never replaced by `010F` on the best-effort path, including a stop held across recovery.
- A late reply while a best-effort stop is pending does not withdraw the stop.
- One axis cannot occupy the other axis's urgent slot.
- `stop fast` escalates a running `stop normal`; `stop normal` never replaces `stop fast`.
- Idle at `000F` longer than `maximumAgeUs`: the stop is still admitted.

These existing tests change as recorded contract changes, never as weakened ones: `unsupportedPrerequisitesAreAtomic` and `exactDeadlineAndRepeatedStopRejection` keep rejecting `000B` and released states and reject a prior `010F` except for `000B`, but now accept trigger words; `freshnessAndStepExhaustion` drops the control-age rejection; `test/can_bus_recovery_test.cpp` follows the new urgent rules. `quarantineAndOtherNodeProgress` stays valid for ordinary work.

Assert precise writes and retained outcomes, including repeated stop and stop from halted/released states with no enabling preamble. No test helper may substitute NMT stopped for drive stop or silently clear faults. Exit with the complete reviewed stop/owner contract, leaving unsupported firmware semantics specifically blocked. Run focused stop/owner/service suites and Doxygen.

## Documentation and delegation

Apply [DOCUMENTATION](DOCUMENTATION.md), [MAINTAINABILITY](MAINTAINABILITY.md) and [DELEGATION](DELEGATION.md). Document normal/fast differences, interruption timelines and hardware uncertainty. Rewrite the owner handoff procedure in `docs/stop_actions.md`, add a best-effort section, correct the stop contract in `docs/can_owner.md` and the `Actions.h` file comment, and update the ledger `implementation_scope` notes for both stop functions and regenerate. Say in `docs/stop_actions.md`, `docs/can_owner.md` and the Doxygen that a CAN stop is best effort and not a safety function, and that an independent physical stop stays mandatory on the bench. One implementer owns the stop state machine; stronger independent review traces occupied-channel ambiguity and stale TX all the way to retained results. Delegate separate pressure/timeline tests after interfaces settle.

Record scope/checks in `docs/reports/prompt_31.md`, update coverage/owner guidance, then commit and sync verified work on the same execution branch. Complete independent checkpoints may be synchronized earlier; unresolved owned stop semantics remain pending.
