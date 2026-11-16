# Prompt 51 source audit

Read-only device and sibling audit on 2026-11-16, initially at CAN `63060047`.
No sibling fetch, mutation or physical CAN activity occurred. This report supports
the [prompt 51 contract](../prompts/51_sync_producer.md); it qualifies no firmware.

## Original device sources

Original files were reopened and hashed; EN86/ZH86 PDF page 3 and N5 PDF pages
119–120, 128–130, 165–166 were read. N5 page 119 was also rendered to verify its
identifier diagram. These hashes match the retained manifests.

| Original | SHA-256 |
| --- | --- |
| [EDS V1.1](../reference/vendor/LC_StepDriverCAN_V1.1.eds) | `dd3c96e7282c012a522dddb5bbdbbf561c1f5c23716b31f12472f45829d4f52c` |
| [EN86](../reference/vendor/86C_CANopen_user_manual_en.pdf) | `401df6c49c54fe56fed7c606ca45718f352235746e2faaeb65be778eef6c4b9f` |
| [ZH86](../reference/vendor/86C_CANopen_user_manual_zh.pdf) | `3969c914aa291662e8c77941bcfbb0559769a96b75306266370890d03e824d13` |
| [N5 manual 3.3.0](../reference/multidrive/vendor/nanotec_n5_canopen_v3.3.0.pdf) | `4c60e1415c3f2421a010c57d3fd050b49bed87606a0fda769dd874052867ea41` |

The EDS `1005` section starts at line 225: RW U32, default `0x80`, not
PDO-mappable. Transmission type sections start at lines 371/431/491/551 for
`1400:02` through `1403:02`, and 812/872/932/992 for `1800:02` through `1803:02`.
Each is RW U8, default 255, not PDO-mappable, without a value range.
`1006`, `1007`, `1019` and `1800:06` are absent. Their absence establishes neither
a counter byte nor any drive production, watchdog, window or sampling guarantee.
The host must not set drive `1005` bit 30.

EN86/ZH86 PDF page 3 claims multi-axis high-speed bus synchronization but supplies
no SYNC mechanism or timing. EN86 PDF pages 15–16 declare RPDO mapping records
as writable; they do not establish the needed remapping or buffered-write
procedure. Synchronous type 1 remains an explicit experiment. A successful
`1800:02` write and readback establishes stored contents only.

N5 is contrast only, for manual 3.3.0/FIR-v2213. PDF page 119 visibly prints
`80+NodeID` in its SYNC diagram, while page 165 gives the `1005` default `80`:
retain that conflict rather than copying the formula. Page 120 describes
buffered RPDO application and TPDO sampling at SYNC and an optional counter.
Page 128 describes RPDO types 0–240 buffering until SYNC; page 130 describes
TPDO types 1–240 every nth SYNC. Pages 165–166 describe producer bit 30,
`1006` in multiples of 1000 us and `1007` window behavior. None establishes a
Lichuan capability.

## Pinned stack mechanics

The experiment vendor directory named by the prompt contains neither `CO_SYNC`
nor `CO_PDO`. `CO_PDO` is retained under
`docs/reference/public_code/canopennode/301`. Exact
[CANopenNode revision `9b8beed8367241e96ac03f916cd5a500bcb2cf23`](https://github.com/CANopenNode/CANopenNode/tree/9b8beed8367241e96ac03f916cd5a500bcb2cf23)
blobs were acquired into ignored `.build/p51-evidence` for this review; canonical
snapshots were not changed.

| File | SHA-256 |
| --- | --- |
| `301/CO_SYNC.c` | `f140a45ea032cbf360a2fb8429ed7498a37c1e9ecedcb2b5d6dc387ea21c78a9` |
| `301/CO_SYNC.h` | `3e9ae95c5b548a645f691829913e2bd118bf8d1857a6e2b8fbb9288eccc954a6` |
| `301/CO_PDO.c` | `4791d3acaa044fbe4c05b3b9cd4cbe76c07d9534b04adda3f3df12fdc71a2252` |
| `301/CO_PDO.h` | `e370c8093356a60ef6cce7f346809182bc241f326090f7ecb568da37fc309201` |

`CO_SYNC_receive` (`CO_SYNC.c:32–52`) accepts DLC 0 without counter evidence and
DLC 1 when the optional counter is configured. `CO_SYNCsend`
(`CO_SYNC.h:171–178`) resets the timer and toggles the RPDO buffer before returning
the CAN send outcome; `CO_SYNC_process` (`CO_SYNC.c:339`) discards that outcome.
Do not copy failed-TX cycle advancement or send-relative timer drift.

`CO_SYNC.c:88–94` checks 11-bit format and the restricted identifier macro in
`CO_driver.h:425–429`. The latter excludes `000..07F`, `101..180`, `581..5FF`,
`601..67F`, `6E0..6FF`, `701..7FF`. It does not prevent collisions with configured
PDO or other service identifiers. Custom SYNC routes need a separate owner
collision check; transport format support is independent.

`CO_PDO_receive` (`CO_PDO.c:455–501`) overwrites the latest data in its selected
buffer. `CO_RPDO_process` (`746–777`) waits for SYNC for synchronous PDOs and
reads the previous buffer. Lines 920–925 clear buffers on invalid PDO or
non-Operational NMT. This source supplies no asynchronous SDO-controlword buffer
flush. `CO_TPDO_process` (`1423–1453`) implements every-nth-SYNC sending. These
are stack mechanics, not measured Lichuan effects.

## Current APIs and sibling seams

Requirement 3 already has typed coverage: `prepareCommunicationRead`,
`nextCommunication`, `advanceCommunication` and `getCommunication` publish
`ConfigObservation::syncCobId` through `CommunicationGroup::NETWORK`, and
`PdoCommunicationObservation::transmissionTypeRaw` through the RPDO/TPDO
communication groups with `pdoNumber` 1–4. The ledger and generated U32/U8
descriptors retain Q20 acquisition gates. Reuse this path.

| Sibling | Refreshed state | Adopted or rejected seam |
| --- | --- | --- |
| MotorControl-RS | `e43c8d0c8235e1077dc176829284cf94490b8831`, clean | Reopened `examples/common/RtuBusOwner.cpp::admitUrgent/collect`, `RtuRunner.cpp` timing and ESS `Actions.h::prepareStop`. Adopt copied bounded requests, independent urgent capacity, supplied time and retained uncertainty. Reject RTU direction/drain timings as CAN facts. No common PDO/SYNC API name exists. |
| FieldCore-node | `c40ea8341edf8d665460b902cafefbe857a7f73f`, clean | `Rs485MotorModule.cpp::applyRuntimeConfig/startMotor` unchanged since prompt 50 by working-byte hash. Timing seam `Rs485Task.cpp::pollModules` delivers retained closure before coarse timeout processing. Adopt application ownership; reject product types and RS485 timings as CAN facts. |

## Qualification boundaries

**Q27:** separate acceptance/effect of type 1 from same-SYNC sample provenance.
An unmarked TPDO and DLC-0 SYNC do not carry the host cycle number. Receive time
after SYNC alone cannot qualify delayed same-ID/same-length samples. Require an
evidenced retirement boundary or applicable observable marker and a bounded
response premise before claiming one sampling instant across axes.

The first type experiment requires an exact fixture/firmware record, fresh
`1005=80`, complete communication/map baseline, exclusive owner, no foreign
producer, no motion and no flash writes. Make the volatile 255-to-1 change in
explicit Pre-operational state, read it back, and separately request Operational.
Begin with one-shot SYNC at 100 ms; retain traffic, timing bounds, EMCY and service
gaps. Restore 255 with exact readback without saving. The ten-minute 10 ms trial
needs prompt 56 throughput artifacts.

**Q28/M09:** the existing configuration API has no controlword-only RPDO map.
Layout validation is insufficient. Review exact disabling/mapping/type/readback,
restoration and partial-effect rules before any RPDO traffic. First use an idle
drive with start bit low and a `607A`-only RPDO2: distinguish immediate versus
post-SYNC application, X-then-Y last-wins and event-timer behavior. A 3 s
observation interval is application policy, not a documented timeout.

A synchronous start needs its reviewed controlword-only map, one-shot SYNC and
the periodic producer disarmed. After failed or unconfirmed start SYNC, a later
SYNC remains a possible start trigger. An asynchronous `6040` without the start
edge does not establish that buffered RPDO withdrawal succeeded. Keep later SYNC
inhibited until a withdrawal mechanism is evidenced. Ordinary asynchronous
per-axis stops remain independent; they must never wait in a synchronous RPDO.
