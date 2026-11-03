# Packet 3: TWAI submission, receive ordering and TX retirement

Executes audit W3, DEC-4, C-03, C-04, C-07, C-13, C-14 and C-20, and reopens prompt 27. Host and fake evidence plus SDK source review. **Nothing here is a measurement of arbitration, retransmission or any drive.** No port or CAN traffic.

## Delivered

| Change | Why | Where |
| --- | --- | --- |
| `TwaiSubmit`: SENT, DEFERRED, FAILED_UNSENT, FAILED_UNCERTAIN | A receive interrupt during promotion no longer quarantines (C-04). A false return no longer invents nontransmission | `adapters/twai/TwaiAdapter.h` |
| `pollEvents` before the owner, `promote` after it | A TX report normally precedes its reply (C-07). The race narrows and does not close: a frame can complete after the poll, so the owner's blocked handling stays and has a test | `TwaiAdapter`, `FirmwareProbe::service` |
| Hold until SDK submission | Possible effect and response clock start at submission, not adapter handoff, so a frame queued behind another axis's pending frame can expire with no effect (C-20) | `SdoClient::holdHandoff`, `noteSubmitted`, `withdrawUnsent` |
| Same hold for NMT | A proved-unsent NMT frame had left its transition intent live, so a later heartbeat could confirm a command that never left. Intent is now its own step: withdrawn while held, restored at submission | `recordNmtIntent`, `holdNmtHandoff`, `noteNmtSubmitted`, `withdrawNmtUnsent` |
| `fail_retry_cnt=-1` (DEC-4) | Arbitration loss and one error frame no longer end all SDO service to a drive (C-03). Applied after the S2 and S3 source review | `IdfTwaiDriver::start` |
| Bus-off during `stop()` is no longer a failed stop | Bus-off can arrive between the state query and disable, and retransmission makes that a normal path. Found by the source review | `IdfTwaiDriver::stop` |
| Real failure reasons | A rejected application or evidence turn was reported as a clock fault (C-13) | `FirmwareProbe::failure` |
| Classical DLC 9..15 accepted as eight bytes | It counted as a drop, which inhibits and quarantines (C-14) | `IdfTwaiDriver::rxDone` |

Decisions on questions the tests raised:

- `FAILED_UNCERTAIN` is unused by the IDF driver. The source review proves every non-OK `twai_node_transmit` return leaves the frame neither mounted nor started. The value stays for adapters without that proof, and the fake exercises it.
- `stats.txFrames` still counts a report arriving after bus-off. The field's contract is "matching final report says transmitted", and it never rewrites the terminal.
- A later NMT command replaces the record of an earlier observed one, including when the later one proves unsent. `intentPresent=false` shows it never left, and the earlier transition stays in the bounded traffic history.
- Adapter capacity is one slot per SDO channel plus the one NMT frame, the most the owner can have outstanding. It was two, so NMT behind two SDO frames was refused and reported as possibly transmitted.
- Lost eligibility has two terminals by order: refused at promote gives TRANSPORT_ERROR and a proved-unsent report, settled by the owner gives TIMEOUT at the queue cutoff. Neither quarantines, and both report no possible effect. This is intended.

## Recorded contract changes in tests

`fail_retry_cnt` is pinned at -1. Old negative `submit` assertions name the exact value. Captured RX is DEFERRED and has its own cases; the old false-submit quarantine stays as FAILED_UNCERTAIN, and FAILED_UNSENT is a new case that does not quarantine. `requiredPreOperational` and its age case now end with no possible effect, because the frame never reached the SDK. DLC 16..22 replaces 9..15 as the invalid range. No adverse assertion was deleted.

## Source review

A separate reviewer recomputed the hashes against `docs/adapters/twai_sources.json` and read the pinned S2 and S3 paths. Its conclusions are in [the SDK guide](../../adapters/twai_sdk.md#retransmission-consequences-dec-4): retired does not mean undelivered, late delivery has no bound until node deletion, and a duplicate reply is possible.

## Sibling refresh

| Sibling | Revision | Dirty | Seam | Disposition |
| --- | --- | --- | --- | --- |
| MotorControl-RS | `776facd`; HEAD was `3f871c7`, which adds only two lines to an RS audit report, so the seam is the same | Clean | Retirement and urgent admission in `RtuBusOwner` | Pattern only. RS transport makes no CAN transmission claim, and `AGENTS.md` keeps sensor retry out of motor commands |
| ADS1115 | `d613a13` | Clean | Owner service in `02_owner_safe_poll` | Unchanged since the handoff authoring; no behavior adopted |
| INA228 | `73f9548` | Clean | Validity callers | Unchanged since the handoff authoring; no behavior adopted |
| FieldCore-node | `5ad9292` | Clean | No CAN path | Unchanged |

## Owed to packet 11

These are the exact physical questions this packet cannot answer:

1. Failed TX and `on_error` counts under TPDO load; how often arbitration loss occurs.
2. A no-ACK retry loop: `ack_err` per attempt, interrupt load on the single S2 core and service gaps, and whether it reaches bus-off. *Measured 2026-11-03:* `ack_err` only, about 7,250 per second, service gaps under 4.7 ms, and no bus-off, with TEC held at 128 ([packet 11](packet_11.md#physical-questions)).
3. A lost TX command (`LL:277-279`) separated from a pending frame by the absence of `ack_err`.
4. Whether `twai_node_disable` stops a retrying frame on the wire, which needs an independent listen-only observer, and the TX pin's level after deletion.
5. What `is_tx_success` reads on a bus-off TX-done.
6. Actual S2 and S3 FIFO behavior on a frame with DLC 9..15.
7. Duplicate delivery after a late error.

## Checks

See the [execution table](README.md) for the commit and its CI result. Local gates are listed in that row.
