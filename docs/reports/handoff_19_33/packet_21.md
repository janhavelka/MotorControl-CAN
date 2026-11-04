# Packet 21: relative position and complete interruption behavior

Resumes prompt 33's relative scope. **When this packet closed, host and fake evidence only, and every relative request was refused**: no relative wire base was evidenced on any specimen, so `prepareMoveRelative` returns `UNSUPPORTED` with `MoveError::RELATIVE_BASIS` before any work unless the caller's `MoveEvidence` names a measured base. That default is unchanged. **On 2026-11-04 node 1 measured `ACTUAL`, and a library relative move then succeeded** ([bench results](#bench-2026-11-04-the-base-and-a-library-relative-move)). Prompt 33 stays Partial.

## Delivered

See the [move guide](../../position_parameters.md) and the [changelog](../../../CHANGELOG.md).

- **One lifecycle.** `prepareMoveRelative` and `validateMoveRelative` use the same `checkMove`, `MoveContext` and `nextMove`/`advanceMove` path as the absolute move. Only the trigger differs: `005F`, chosen from `parameters.target.requested.relative`. `AxisSession::requestMove` and `requestMoveRelative` share one private `startMove`.
- **Basis gate.** `RelativeBase {UNMEASURED, ACTUAL, COMMANDED}` in `MoveEvidence`. The request's basis must equal the measured base, otherwise `UNSUPPORTED`. A queued basis is always refused: moves start only from idle, and no target queue is documented.
- **Witness.** Required for a relative move: known, idle, stationary, of the request's basis, fresh, with a nonzero age cap, and observed at or after the predecessor `000F` completed.
- **Range.** The displacement must fit ±1,000,000 as requested and as quantized, and both endpoints must fit the axis limits. Zero displacement is refused before TX under every basis (R-08).
- **Rechecks at admission.** `noteMoveTransmission` rechecks every parameter and trigger write at admission, not only at prepare: the prepared target, the witness still idle and stationary, and the admission read within its age. The rearm is not rechecked, because it follows an observed completion. This also applies to absolute moves.
- **Two session fixes found on the way, both affecting absolute moves too.** Withdrawing a move now cancels a queued download the owner has not handed to the controller; before, a queued `001F` or `005F` could still leave after a cancel, fault or refused stop. An owner `CANCELLED` terminal the session did not ask for, after `invalidate`, now ends the move `INVALIDATED`.
- A completion never establishes the requested endpoint, and nothing writes it back into the witness.
- `examples/position_preview` prints the `RELATIVE_BASIS` refusal only; there is no relative simulation.

## Tests

- `test/support/MoveRig.h`, a shared rig; `move_absolute_test` now uses it.
- `test/move_relative_test.cpp`, 108,159 checks after the review fixes (`move_absolute_test` 39,814): 11 interruption points, with queued and handed-off variants for the downloads, against deadline, cancel, fault, boot-up, rebind, missing feedback, stale feedback and a real urgent stop. Separate scenarios: both nodes stopping urgently at once, a lost trigger reply then a late identical ACK, explicit recovery, repeated identical values as separately admitted edges, timeout then new motion, and the processing-time cutoff.
- `test/move_common_test.cpp`, 40 checks: the common intent vectors, compared with RS `preparePosition` compiled read-only, and test-only servo and non-CANopen fixtures for type neutrality.
- Nine hand mutants of the new rules, all killed.

## Independent review

A read-only reviewer traced uncertainty from each possibly-sent write into the next admission and ran a probe against the built libraries. Held: owner-side withdrawal in all three queue states, the unrequested `CANCELLED` rule, the basis gate, zero displacement and ranges, completion never writing the endpoint, the acknowledged `005F` as the stop's predecessor, and the fences.

| Finding | Disposition |
| --- | --- |
| **D1, confirmed.** A cancel or stop with a time behind the owner's clock failed inside the owner (`CanBusOwner::cancel` returns `INVALID`), but `withdrawMove` ignored the result: the move ended `CANCELLED`, or the stop request `FINISHED`, and the queued `005F` still left | Fixed: see below |
| **D2, confirmed.** The admission recheck ran right after admission, one owner turn before the frame left, so a reference invalidated in between did not stop the trigger | Fixed: see below |
| W1. The recheck's reason was lost; every refusal looked like a bare `INVALIDATED` | Fixed: see below |
| W2. A download cancelled before it was sent dropped the latest acknowledged controlword, so a later stop lost its sequenced path | Fixed: see below |
| W3. `requestMove` borrows the caller's axis and witness until the move ends | Documented contract; the bench route keeps both in its own storage |
| W4. Under `AFTER_ACCEPTANCE`, a stale bit 15 still succeeds while the fake drive moves | By design: the acceptance and completion rule is caller evidence, and it is only as true as packet 19's measurement |

### Fixes

A separate worker fixed the four findings in `examples/common/AxisSession.*`. Each new test was confirmed to fail with its fix reverted.

- **D1.** Every member that takes a time refuses one behind the session's or the owner's last time, with nothing changed. The exception is deliberate: a refused stop still withdraws an active enable or move, at the owner's time, because refusing it outright would let the queued trigger leave after a stop request. `withdrawMove` counts work as unsent only when its terminal shows no possible transmission. Test `ownerClockAhead`.
- **D2.** New `AxisSession::preflight` reruns the admission recheck when the owner hands a parameter or trigger download over, and refuses any download of a withdrawn move. The owner closes the download `PREFLIGHT_REJECTED`, provably unsent, and the move ends `INVALIDATED`. The application must call it from `Transport::beforeTransmit`. `test/support/MoveRig.h` does, and the S2 bench wires it through `FirmwareProbe::setPreflight` and `BenchRoute`, tested in `firmware_probe_test` (`preflightReachesTheOwner`). Tests `recheckAtHandover` (relative and absolute) and `cancelWhileHeld`, with a frame held in the TWAI adapter.
- **W1.** `moveRefusal()` keeps the refusing status and its detail.
- **W2.** A proven-unsent cancel restores the previous acknowledged controlword. Test `unsentKeepsControl`, and the matrix row for a stop after an unsent rearm now requires `010F`.

`recheckMove` repeats the library's three checks from `noteMoveTransmission`, because the session cannot call a private library helper. One shared library function would be the cleaner home, and is left for the next change to `Position.cpp`. `AxisSession` is 23,576 bytes.

Still untested, from the review: one node's urgent stop beside the other's queued or handed-off parameter or trigger; a fault reported in an observation cycle, rather than a local fault cancel, in the relative suite; the `COMMANDED` basis beyond one success; and a record of the build that produced the RS intent vectors in `move_common_test`.

## Bench, 2026-11-04: the base, and a library relative move

On node 1, after the user's second power cycle, drive reports only: no shaft was observed. One specimen, one firmware. See the [bench report](../2026-11-04_bench.md).

| Trial | What | Result |
| --- | --- | --- |
| [406](../../bench/20261104T1541Z-position-halt-node1-406/rx.txt), [408](../../bench/20261104T1541Z-rearm-halt-node1-408/rx.txt) | The start this measurement needs: a move 0 → 600 at 6 rpm halted after 2 cycles, then `000F` from the halt's `010F` | Stopped at 104 with target 600 retained; the `000F` did not resume it. So actual 104 and last commanded endpoint 600 differ by 496 |
| [409](../../bench/20261104T1542Z-relative-node1-409/rx.txt) | The `relative` route, +200 with `005F` | **Ended at 304**, in place, `6041=9437`: the drive added the displacement to the **actual** position. `TRIAL_BASE` read `607A=600` before the writes, matching the firmware's own record of trial 406's acknowledged target. Acceptance (`6041` bit 12 from the first cycle) and completion (`2100` bit 0 absent while running, then present) as after `001F` |
| [410](../../bench/20261104T1542Z-rearm-rel-node1-410/rx.txt) | `rearm` after `005F` | Bit 12 cleared; 304 held |
| [411](../../bench/20261104T1543Z-move-rel-node1-411/rx.txt) | **`AxisSession::requestMoveRelative`**, +100, `RelativeBase::ACTUAL` from record 409, witness from two fresh reads | **`SUCCEEDED`**: completed at **404**, the computed endpoint, with its own rearm |

So for node 1 on this firmware items 1 and 2 below are answered: `ACTUAL`, with the absolute signals. **Still owed:** item 3, a zero `607A`, which the library never sends; a negative displacement; node 2; and a shaft observation. The library default stays `UNMEASURED`.

## Physical evidence needed, as written before the session

Per specimen and firmware, with raw traffic and a shaft observation:

1. Whether `005F` adds the displacement to the actual position or to the last commanded endpoint. The trial needs a start where the two differ, for example after a halted move, without creating a dangerous following error.
2. Whether acceptance and completion behave after `005F` as after `001F`.
3. What `005F` does with a zero `607A`. The library never sends one.
4. Everything [packet 20](packet_20.md) owes for the absolute move.

## Sibling refresh

| Sibling | Revision | Dirty | Disposition |
| --- | --- | --- | --- |
| MotorControl-RS | `a13f767` (`src`, `include` unchanged since `6914d26`) | Clean | Adopted: the basis gate on verified evidence (`relativeBasisVerified`, `include/MotorControlRS/profiles/ess_rs/Position.h:90`, `src/profiles/ess_rs/Position.cpp:227`), the idle and stationary witness, zero-displacement refusal. Rejected: freshness proven only at prepare, since CAN rechecks at admission; the `AxisConfig` note that native bounds check the displacement, since CAN checks endpoints and the `607A` range covers the displacement; refusing `COMMANDED` outright, since CAN gates it on evidence; `positionMismatch` |
| BME280 | `48b8214` | Clean | Adopted: partial-setup reporting. Rejected: rollback |
| RV3032-C7 | `1b74184` | Clean | Adopted: the frozen intent is an owned copy. The session's borrowed axis and witness are live facts for the rechecks, not admitted work data |
| FieldCore-node, ADS1115, INA228 | Unchanged since packet 20 | Clean | No new behavior |

## Checks

On `261a25b`: the full verifier in a clean detached worktree, every firmware gate, and the quick suite under UBSan (88 of 88) are recorded in the [execution table](README.md).
