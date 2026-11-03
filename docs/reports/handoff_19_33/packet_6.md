# Packet 6: per-axis urgent work and interrupted-operation retention

Executes the owner half of audit W6, with C-01, C-02, C-17, C-18 (owner part only) and C-19 (owner part only), and reopens prompts 24 and 31. Host and fake evidence only. **Nothing here is a validated motor stop: test requests are fixtures, and no drive saw these paths.**

## Delivered

See the [owner contract](../../can_owner.md) and the [changelog](../../../CHANGELOG.md).

- **Capacity.** One urgent slot per channel. The audit asked for `URGENT_REQUESTS = SEQUENCES`; the code uses `SdoClient::CHANNELS`, because the slot index is `REQUESTS` plus the channel and the two values are both 2. A channel carries one exact binding, so one axis on two channels would hold two slots, and one channel cannot use another's.
- **`beginUrgentOperation`, `findSequence`.** Queued old work ends with explicit no-TX results. In-flight work settles first.
- **Reservation reuse.** A retired reservation stops blocking its axis, and old results stay readable by request identity.
- **Best-effort route.** `admitBestEffortStop` with `SdoOutcome::UNCONFIRMED_SENT`, for packet 7 to consume. No action calls it yet.

## What packet 7 must decide

- `beforeTransmit` is skipped on this route, so packet 7 must withdraw `000F` eligibility itself before admission (W6 step 8).
- Urgent work inherits its holder's producer. `invalidateProducer(holder)` therefore ends a console stop that was admitted through that reservation. This matches the documented group meaning, and packet 7 decides if a stop should be exempt.
- A stop's queue budget must cover the settling time of the work ahead of it, or it ends `QUEUE_EXPIRED` unsent.
- `Settings.cpp`, `Reads.cpp`, `PdoConfiguration.cpp` and `Actions.cpp` reject outcomes above `UNSUPPORTED`. That fails closed and suits W6 step 7's separate context. Stop terminals must not pass through them.

## Review findings, all fixed before commit

An independent reviewer probed every bypass. It found no arbitrary-bytes path, no forbidden or second transmission, and no way for a reply to qualify a stop. It did find these:

| Finding | Fix |
| --- | --- |
| A server abort of old in-flight work ended the new operation's queued stop | Cleanup matches the operation |
| While busy, admission skipped validation, so malformed stops took the slot and blocked a correct one | Full validation always, split from state |
| Known `STOPPED` on an idle channel returned `INVALID`, and `ACCEPTED` while busy | `INHIBITED` in both, from one check |
| A staged cleanup abort made a valid stop `INVALID` | Admitted, and waits |
| The drive's normal reply to a second stop refused all later markers | A retained pair's late reply is expected. Only a frame no retained request explains refuses markers |
| The same refusal applied to a late reply to an ordinary request lost after a reopening | Same rule |
| A stray frame during a held, never-sent stop left the route clean | Sets the route unresolved |
| A stop on one channel cancelled in-flight work on a peer channel of the node, through peer quarantine | The stop waits for peer work, and no new peer work starts around it. Other peer quarantine still retires work, as `sharedPeerQuarantineRetiresActiveBeforeLaterRxOrTx` pins |
| Reuse took an invalidated slot while another was free, and its holder could never finish: `ProbeApp`, `RawEvidence` and `PdoExperiment` only finish when `releaseSequence` returns true | A free slot goes first. `releaseSequence` on an identity whose slot was reused returns true and changes nothing |
| `checkBestEffortStop` built a whole `Channel` on the stack: 1552 bytes on xtensa at `-Os`, against 48 for `checkWork` | Eligibility no longer needs one |
| A best-effort stop reset producer fairness | It no longer does |
| `findSequence` lacked the overlap check its siblings have | Added |

## Accepted behavior

A stop returns `RESULTS_FULL` while its channel's urgent slot holds anything: a queued or in-flight marker, in-flight urgent work or an unread result. It never displaces that work or result. The application reads and releases it, or cancels and releases queued work, then admits again. This costs one service turn at the moment DEC-2 cares about, and it preserves the original result, which the packet requires. A refusal is explicit, so a caller cannot mistake it for a sent stop.

## Open

- **Physical, packet 11 onward:** none of this touched a drive. Whether `000B` stops anything is packet 16's question.
- The owner-side guarantee is one frame submitted at most once. CAN retransmission inside the controller is covered by [packet 3](packet_3.md).
- A long segmented transfer on a peer channel delays a stop until it settles, within that transfer's own bounds and the stop's queue budget.

## Sibling refresh

| Sibling | Revision | Dirty | Seam | Disposition |
| --- | --- | --- | --- | --- |
| MotorControl-RS | `3f871c7` | Clean | `ProbeApp.cpp` `admitUrgent`, `STOP_RECORDS`, `test/bus_scheduling_test.cpp` | Adopted: reserved stop capacity and explicit `URGENT_FULL`-style refusal, without displacing work. Rejected: RS's single stop capacity, because this owner serves two nodes through one owner. RS has no outside-sequence route, because RTU replies are solicited in a strict window |
| FieldCore-node | `5ad9292` | Clean | Invalidation | Unchanged since packet 5; read-only |
| PCA9555 | `414e0f6` | Clean | `PCA9555::cancelOperation`, `detach` | Adopted in shape: active work becomes a retained result and a pending result stays exactly-once evidence. Rejected: its `IN_PROGRESS` cooperative cancel, because a CAN frame already handed off cannot be withdrawn, so in-flight work settles |
| ADS1115 | `d613a13` | Clean | Owner service | Unchanged since packet 5; no behavior adopted |
| INA228 | `73f9548` | Clean | Per-field `*Valid` flags in `INA228.h` | Same idea as keeping `transmissionPossible` and `effectUncertain` separate from the outcome. No retry rule imported |

## Checks

See the [execution table](README.md).
