# Packet 20: finite absolute-position lifecycle

**After the 2026-11-03 sessions.** Packet 19's evidence exists for node 1: acceptance `6041` `0x1000`, completion `2100` `0x0001` under `AFTER_ABSENCE` ([packet 19](packet_19.md)). The unattended session then ran the `move` route, which passes that evidence through `AxisSession::requestMove`, once on node 2 (trial 121, record 111): 0 to 250 at 2 rpm, `SUCCEEDED` with completion observed and its own `000F` rearm acknowledged, with node 1's bits. Completion read `6064=251` and the next read 250; whether that is overshoot or counter dither is not established. No shaft was observed. The library default still refuses. One specimen and one firmware: not qualified. See the [unattended bench report](../2026-11-03_unattended_bench.md).

Owns the absolute part of original prompt 33. When this packet closed, host and fake evidence only. **The lifecycle is gated: the default `MoveEvidence` returns `UNSUPPORTED`.** No source establishes the acceptance and completion bits, so they are an explicit caller input from packet 19's measurement, not a guessed mask.

## Delivered

See the [move guide](../../position_parameters.md) and the [changelog](../../../CHANGELOG.md).

- `prepareMoveAbsolute`, `nextMove`, `beginMoveObservation`, `noteMoveTransmission`, `noteMoveWithdrawal`, `advanceMove`, `resetMove`, and `MoveContext` (3,016 bytes, with no embedded `ReadContext`). The terminal context is the result.
- Common `MoveOperation.h` holds only shared intent and outcome distinctions: `MoveRequest`, `MoveDisplacement`, `MoveInterruption`, `MoveError`.
- `AxisSession::requestMove`, `cancelMove`, `retainSettings`, and fencing after a trigger that may have reached the drive with no observed completion.
- `test/move_absolute_test.cpp`, 37,826 checks with literal bytes, run with and without heartbeat traffic.

## Phase order (D-01)

1. Admission: the actual latest acknowledged `000F`, then a fresh read showing mode 1, low bits 111 and `2100` idle. No implicit enable.
2. Parameters: `607A`, `6081`, `6083`, `6084` in that order. `retireMotionParameters` runs on every retained context before each may transmit; a refusal means the download is not sent.
3. Trigger: `2B 40 60 00 1F 00 00 00`, held until terminal (DEC-8).
4. Observation cycles, each after the trigger's acknowledgement.
5. Rearm `000F`, only after an observed completion, as its own step. A failed or lost rearm keeps completion observed and is never resent.

The move never writes `6060` or `6061`, never enables, and never stops itself.

## Gates, all before TX

- Capability: the gate, model, identity values and node epoch; immediate update refused.
- Values: `607A` within ±1,000,000, requested and quantized; ramps 1..2000 ms.
- Speed: must come out integer motor turns per **minute**, through `prepareVelocityTarget`. The pinned case, 1 load turn/min at gear 5 under `COMMAND_INCREMENTS` per second, gives 267 and is refused. Requested and quantized speed 1..3000 rpm. **This closes packet 9's and 12's open rate seam.**
- A supplied witness must be idle, stationary, ACTUAL and fresh. A known zero displacement is refused (R-08); an absolute zero with no witness is an ordinary target.
- The full binding, configuration epoch included, is frozen. A real `prepareSetDevicePosition` trigger afterwards makes the move refuse every call but its ending.
- Acceptance and completion: a caller-measured `(word & mask) == value` with a rule. Completion counts only after acceptance and only when new. A cycle consumed at `now >= deadline` expires (G-13).

## Decision

The session keeps **two** reads alternating, rather than one move member, so the last complete cycle stays available. That lets a stop from the held `001F` be sequenced (`010F`, `predecessorDocumented` false, no `000F` preamble) instead of every mid-move stop costing the route through `000B`.

## Independent review, all fixed

A reused read not retained; an aborted trigger not fenced; the partial-setup summary; and `nextMove` re-offering a download already noted. It confirmed that no path replays `001F` or reports a completion the evidence rule did not qualify. 21 hand mutants (not retained): 20 killed, and the survivor is a defensive check the API cannot reach.

## What packet 19 must supply

Per specimen and firmware, with raw traffic and a shaft observation:

1. Raw identity `1000`, `1009`, `100A`, `1018:01..03`, `2000`, `2001`, with the width policy.
2. Acceptance word, mask and value: absent at idle `000F` and after the rearm, present after `001F`, its latency, and whether it holds while bit 4 stays set.
3. Completion word, mask and value at idle, and whether it clears on acceptance or only while moving, including a move shorter than one cycle and a target equal to the current position.
4. That mode 1, low bits 111 and clear `2100` alarm, released and limit bits hold through a move.
5. That parameter writes at `000F` cause no motion before the edge, and the shaft moves in the commanded direction by the commanded distance.
6. What the `001F` to `000F` rearm does after completion.
7. The DEC-8 rows for a stop from `001F` (packet 16).

## Open

- **Closed by packet 21:** relative moves share this path (`prepareMoveRelative`) and are refused before work until a wire base is evidenced. Immediate update (`003F`, `007F`) stays unsupported.
- **Closed by packet 21:** `MoveContext` (3,008 bytes) is in `check_stack.py`'s measured context types.
- **Tested here for packet 18:** a counter zero advances the configuration epoch, so a move prepared before it can neither run nor be admitted again (`counterZeroInvalidates`).
- An explicit `000F` rearm after a stop or an unknown trigger needs its own action. The move's own rearm, after an observed completion, is part of this lifecycle, and the bench `rearm` route covers only the isolated `position` experiment.
- **Bench caller added 2026-11-03:** the attended `move` route runs this lifecycle through `AxisSession::requestMove`, with `MoveEvidence` taken only from the host command and a route-owned `AxisConfig` kept alive for the whole move. It is for use after packet 19's evidence is reviewed, and has not run.
- **Ledger:** not a gap. Every one of the 99 objects in `lichuan_objects.json`, `6040`, `2301` and `2302` included, keeps whole-object `coverage` false by design, because the ledger is a documentary dictionary with no command-coverage field. Command coverage is recorded in the [coverage matrix](../../coverage.md).
- `AxisSession` is 23,544 bytes after packet 21; the bench routes allocate it from the heap.

## Sibling refresh

| Sibling | Revision | Dirty | Disposition |
| --- | --- | --- | --- |
| MotorControl-RS | `6914d26` | Clean | Adopted: `MoveError`; the common move-intent shape; refusing a supplied reference that is not idle, stationary and ACTUAL (`src/profiles/ess_rs/Position.cpp` `prepareMove`); zero displacement refused; the terminal context as the result. Rejected: `positionMismatch`, `MoveSetup::USE_STORED`, ESS `PositionLimits`, the RUNNING-then-ARRIVED completion rule |
| FieldCore-node | `5ad9292` | Clean | `Rs485MotorModule::startMotor`: configuration epoch in the request and separate operation and step identity, mirrored here |
| BME280 | `48b8214` | Clean | Adopted: reporting partial dirty state (`src/BME280.cpp:1075-1095`). Rejected: local rollback |
| ADS1115, INA228 | `d613a13`, `73f9548` | Clean | Unchanged |

## Checks

The tests passed the local gates recorded for `01190cf` in the [execution table](README.md). CI failed at `01190cf` on a test registered twice, which is unrelated to this packet. The full verifier then ran on the exact commit `f1116b1` in a clean detached worktree, and CI passed there.
