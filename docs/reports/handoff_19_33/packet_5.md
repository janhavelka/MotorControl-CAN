# Packet 5: explicit SDO settlement and the drain-marker premise

Executes audit W5, DEC-3, C-05, C-06, N-03 and N-04, and reopens prompts 13, 14 and 24. Host and fake evidence only. **This does not qualify remote server behavior: no real route can reopen until Q18 closes.**

## Delivered

See the [service guide](../../canopen_services.md) for the contract and the [changelog](../../../CHANGELOG.md) for the API.

- **Settlement.** A matched abort on a clean exchange leaves the route open. Controller events quarantine only what may have left, sampled before retirement.
- **Drain marker.** Through the existing owner and service, with no second client. It stays narrow: outside any sequence, on the urgent slot, and refused for everything but its one purpose.
- **C-06.** Losing diagnostics no longer cancels a frame on its way, in `DeviceEvidence`, `RawEvidence` and `PdoExperiment`. Unstarted work is withdrawn at once. The trace holds more than one turn's records.
- **C-05.** The test that rebuilt `SdoClient` in place to fake recovery is gone. A later success on the still-open route clears the failure streak, so the streak documentation is true.

## Remote-ordering evidence

An independent reviewer searched the retained sources. The Lichuan manuals and EDS are silent on SDO concurrency, queuing or order. CiA 301 is not retained, only a one-page poster. `CO_SDOserver.c` is not in the repository. So "the server answers one request at a time and in order" rests on memory of CiA 301 and the protocol's shape. The wire cannot prove it either, because classic SDO has no transaction tag.

A marker on a distinct object is sufficient only with a stronger premise: the server handles requests FIFO with no deferred path for some objects, its CAN TX keeps same-ID order, and the marker's pair appears in no outstanding reply. If the premise fails, a stale reply can confirm a later write to the same object, and a probe reproduced exactly that. No client-side check can close this. **The premise is therefore a gate, off by default, set only by test fixtures, each naming it as a fake.** Packet 11 owns the physical question.

## Review findings, all fixed before commit

The reviewer's probes and the test worker's pins found these in the first implementation:

| Finding | Fix |
| --- | --- |
| A stray frame on a live transfer was not retained, so a second copy could settle a marker with no server action | Retain it |
| Reopening did not wait for the marker's own TX, so a duplicate reply could complete new work | Reopen only after retirement |
| A queued step transmitted after an earlier step's rejection | A rejection ends queued steps of its sequence |
| An abort outside an outstanding exchange left the route clean, and a live frame counted as not possible | Clean means one confirmed outstanding frame; `txLive` tracks frames separately |
| A queued marker survived a boot and started against the new session | `markerCurrent` |
| An experiment grant armed before quarantine came back after reopening | Reopening voids grants |
| Pre-event service proof survived a controller event | Cleared in `reportController` |
| A reported cleanup abort blocked the marker forever | `noteTxRetired` |
| A settled marker could outlive contrary evidence | Any later local boundary clears it |
| Later evidence against the premise never revoked it | An unexpected frame after a reopening refuses further markers |
| A marker's reply refreshed service qualification | No refresh while unresolved |
| A segmented marker reply refused all later markers | Markers must be expedited |

## Open

- **Physical, packet 11:** reply order across a slow write then a read, a new request while one is pending, and duplicate delivery.
- A route that refuses markers stays blocked until an MCU restart and new session premises. That matches DEC-3 and brings C-01's cost back for that route. W6's best-effort stop is what remains available there.
- **Bench watch, packet 11:** a same-tick abort that arrives before its TX-done quarantines, under the clean-exchange rule. The `pollEvents` split narrows that window and does not close it, so a route could still reach quarantine this way after an ordinary absent-object read. Measure how often it happens.
- A wrong-object frame on a clean route is retained, and only a reopening clears it, so five distinct strays over a session refuse markers before any loss. This is intended: such a frame contradicts the sole-client premise.
- A marker holds the single urgent slot until packet 6 makes urgent capacity per axis.
- The trace grew to 24 records of 416 host bytes each. Packet 8 measures firmware memory.

## Sibling refresh

| Sibling | Revision | Dirty | Seam | Disposition |
| --- | --- | --- | --- | --- |
| MotorControl-RS | `776facd` | Clean | Retirement, in-flight settling | Adopted: in-flight work settles (R-14). A marker has no RS analogue, because RTU replies are solicited in a strict window |
| ADS1115 | `d613a13` | Clean | Owner service | Unchanged; no behavior adopted |
| INA228 | `73f9548` | Clean | Validity | Unchanged; no behavior adopted |
| FieldCore-node | `5ad9292` | Clean | Invalidation | Unchanged; read-only |

## Checks

See the [execution table](README.md).
