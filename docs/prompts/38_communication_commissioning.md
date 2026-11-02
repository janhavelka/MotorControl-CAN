# Prompt 38 Explicit communication commissioning

Apply [COMMON.md](COMMON.md). Prerequisites: [24](24_owner_recovery.md), [37](37_discovery.md).

## Objective and boundaries

Deliver explicit typed workflows for supported node/communication settings and heartbeat parameters, and typed read coverage of the 88 PDO records. PDO remapping remains prompt 18; integrate through this workflow when 18 resumes. The delivered fixed TPDO1 configuration (`PdoConfiguration.h`) is one narrow subset: an isolated experiment or exact-fixture map3-to-map2 change with six ordered writes and nine readbacks, not general remapping. Compose the existing network, service and owner mechanisms; do not implement a second PDO engine. Host selection is already separate in 37; persistent save/reset remains 39. This prompt owns 89 read rows: the 88 PDO records as an explicit deliverable that does not depend on prompt 18, plus `2203`.

Use [CONVENTIONS](CONVENTIONS.md#4-one-application-can-owner) and [MULTIDRIVE](MULTIDRIVE.md). Communication services are protocol-specific; no common motor API receives fictional CANopen node, NMT or heartbeat requirements.

## Read and audit

For this prompt, refresh MotorControl-RS HEAD and dirty state and read the RS code for this seam; RS is authoritative. Check HEAD and dirty state only for the siblings named below, and inspect only the seam stated. Inspect current FieldCore-node read-only for ownership or integration changes. If HEAD, dirty state and seam match the last recorded row, write "unchanged since prompt N" and do not reread. A sibling row must name a behavior adopted or rejected, with path and symbol. This does not relax vendor evidence, safety, uncertainty or verification rules. Read RS `Communication`, binding changes and uncertainty tests. Keep INA228 `invalidateHardwareState` as a desired-versus-verified analogue and FieldCore binding invalidation (`Rs485MotorModule.cpp:659-680`) as a read-only reference. Recheck each Lichuan communication object's actual access, type, DIP/parameter relationship, restart behavior and firmware scope using original manuals/EDS.

### Concrete evidence to recheck

The [official communication tables](../audit/LICHUAN_OBJECT_FACTS.md) place node selection on SW1-5, bitrate on SW6-7 and termination on SW8; all node switches OFF means user-defined. `0x2203:00` is U16 RW/default 0, described as node setting 0..127, but activation/save/restart and DIP precedence need evidence; 0 is not a unicast node. Do not invent a software bitrate object or LSS workflow (`LSS_Supported=0`). EDS's 50 kbit/s flag has no documented DIP choice. `0x1005/0x1014/0x1017/0x1200` flags likewise do not establish live-change behavior. Keep each supported route tied to its source and applicability.

No object exposes DIP state; all-OFF is a caller attestation. `2203` readback is a stored setting, not the active node; only a reply on the configured server-to-client route (`1200:02`, default node+0x580, unchanged) evidences that. Bitrate is DIP-only in every source: it is a bench step, not a software change. `1001/1003/1005/1014/1017/1018/1200`, `1400`- and `1800`-series rows are EDS-only; the EN86 pp15-16 table lists only `1000`, `1009`, `100A`, `1600-1603` and `1A00-1A03`. No manual mentions NMT, boot state, heartbeat or remap conditions; the EDS gives only names, defaults and DeviceInfo flags. Label standard behavior "CiA 301: general vendor claim, service unconfirmed". Per-row restart and persistence are `unknown`. The heartbeat unit is 1 ms per CiA 301 and public stacks, not vendor-confirmed; verify by measurement. Inhibit (x100 us) and event (ms) units are gated separately (Q17). `1200` is read-only in typed workflows until vendor or firmware evidence supports route writes.

## Implement

1. Freeze this prompt's exact communication rows, including relevant `0x1005/0x1014/0x1017/0x1200` and the 88 PDO read rows, which do not depend on prompt 18. Earlier read coverage remains intact. Implement direct typed preparation/stepping/results in `profiles/lichuan_c/Communication.h` or justified existing files; a raw object writer does not close these workflows.
2. For each change specify preconditions, affected bindings, volatile effect, persistence behavior, restart requirement, expected reply route, partial/unknown effects and readback. Reject unsupported firmware/state/candidate before first write. Do not guess writable node fields from product marketing, and offer no software bitrate change.
3. Keep desired, actual, observed and persistent configuration distinct. Node/bitrate changes may remove the old route before confirmation. Old-route silence is not proof of success; never replay through guessed addresses/bitrates or scan with writes.
4. Use existing confirmed-map generation and the delivered fixed TPDO1 lifecycle as it stands; do not claim it covers other maps, firmware, timers/SYNC or persistence. PDO remapping stays with prompt 18, which integrates through this workflow when it resumes. Preserve required NMT state, disable/edit/readback and partial-failure semantics for that integration. Heartbeat-disabled service eligibility must not become circular: successful SDO proves serviceability, not Operational state, and exact NMT prerequisites remain operation-specific.
5. Invalidate dependent mappings, identity, reference and prepared work according to confirmed or unknown effects. Retire queued work through established adapter/controller evidence; clear only host work whose retirement is established. A host rebind never proves a device communication change. Reused PDO routes need prompt 17's provenance boundary: old same-ID/same-DLC bytes first arriving after rebind cannot renew state merely because they receive a new host generation.
6. Keep each workflow explicit. No startup, discovery, probe, selection, recovery or helper changes heartbeat, PDO maps, node ID or bitrate. A required restart/save is exposed as separate intentional work with its own outcome, not hidden cleanup.

The PDO and `2203` read rows are outside the DEC-1 accepted list; they stay Q20 gated and use an explicit reviewed acquisition.

## Verify and exit

Test every assigned type/range/access rule, invalid complete candidates, unknown NMT with serviceability, known-ineligible state, readback mismatch, timeout after communication write, route disappearance, reboot and rebind. Verify no automatic retry at old or guessed new routes, no implicit persistence and preserved uncertain effects.

Use native fake owner/adapter tests with both a response on the expected route and ambiguous lost replies. Exit with complete supported workflows and exact unresolved model/firmware facts. Physical electrical or topology changes remain bench activities. Run communication/PDO/network/owner regressions and documentation gates.

## Documentation and delegation

Apply [DOCUMENTATION](DOCUMENTATION.md), [MAINTAINABILITY](MAINTAINABILITY.md) and [DELEGATION](DELEGATION.md). A new header or source needs `CMakeLists.txt`, `test/package_test.py`, `scripts/check_repository.py` and `test/repository_test.py` updated together. Publish, in `docs/commissioning.md`, host-versus-device examples, route reconciliation and Doxygen for persistent/restart consequences. One implementer owns each state-changing workflow; delegate source/table review, and require stronger independent review of route loss and unknown writes.

Record closure in `docs/reports/prompt_38.md`, update coverage/commissioning guidance and commit and sync on the same execution branch. Synchronize complete verified workflow checkpoints when useful; preserve any blocked owned scope explicitly.
