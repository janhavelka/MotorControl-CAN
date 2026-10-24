# Prompt 38 Explicit communication commissioning

Apply [COMMON.md](COMMON.md). Prerequisites: [18](18_pdo_configuration.md), [24](24_owner_recovery.md), [37](37_discovery.md).

## Objective and boundaries

Deliver explicit typed workflows for supported node/communication settings, heartbeat parameters and PDO configuration. Compose the existing network, service, remapping and owner mechanisms; do not implement a second PDO engine. Host selection is already separate in 37; persistent save/reset remains 39.

Use [CONVENTIONS](CONVENTIONS.md#4-one-application-can-owner) and [MULTIDRIVE](MULTIDRIVE.md). Communication services are protocol-specific; no common motor API receives fictional CANopen node, NMT or heartbeat requirements.

## Read and audit

Refresh MotorControl-RS, ADS1115 and INA228 revisions and dirty state. Read RS `Communication`, binding changes and uncertainty tests; inspect MB85RC family policy and INA228 configuration validity. Review current FieldCore binding invalidation read-only. Recheck each Lichuan communication object's actual access, type, DIP/parameter relationship, restart behavior and firmware scope using original manuals/EDS.

## Implement

1. Freeze this prompt's exact communication rows, including relevant `0x1005/0x1014/0x1017/0x1200` and PDO records. Earlier read coverage remains intact. Implement direct typed preparation/stepping/results in `profiles/lichuan_c/Communication.h` or justified existing files; a raw object writer does not close these workflows.
2. For each change specify preconditions, affected bindings, volatile effect, persistence behavior, restart requirement, expected reply route, partial/unknown effects and readback. Reject unsupported firmware/state/candidate before first write. Do not guess writable node/bitrate fields from product marketing.
3. Keep desired, actual, observed and persistent configuration distinct. Node/bitrate changes may remove the old route before confirmation. Old-route silence is not proof of success; never replay through guessed addresses/bitrates or scan with writes.
4. Use existing confirmed-map generation and explicit PDO remapping semantics, including required NMT state, disable/edit/readback and partial failure. Heartbeat-disabled service eligibility must not become circular: successful SDO proves serviceability, not Operational state, and exact NMT prerequisites remain operation-specific.
5. Invalidate dependent mappings, identity, reference and prepared work according to confirmed or unknown effects. Retire queued work through established adapter/controller evidence; clear only host work whose retirement is established. A host rebind never stands in for proof of a device communication change.
6. Keep each workflow explicit. No startup, discovery, probe, selection, recovery or helper changes heartbeat, PDO maps, node ID or bitrate. A required restart/save is exposed as separate intentional work with its own outcome, not hidden cleanup.

## Verify and exit

Test every assigned type/range/access rule, invalid complete candidates, unknown NMT with serviceability, known-ineligible state, partial remap, readback mismatch, timeout after communication write, route disappearance, reboot and rebind. Verify no automatic retry at old or guessed new routes, no implicit persistence and preserved uncertain effects.

Use native fake owner/adapter tests with both a response on the expected route and ambiguous lost replies. Exit with complete supported workflows and exact unresolved model/firmware facts. Physical electrical or topology changes remain bench activities. Run communication/PDO/network/owner regressions and documentation gates.

## Documentation and delegation

Apply [DOCUMENTATION](DOCUMENTATION.md), [MAINTAINABILITY](MAINTAINABILITY.md) and [DELEGATION](DELEGATION.md). Publish host-versus-device examples, route reconciliation and Doxygen for persistent/restart consequences. One implementer owns each state-changing workflow; delegate source/table review, and require stronger independent review of route loss and unknown writes.

Record closure in `docs/reports/prompt_38.md`, update coverage/commissioning guidance and commit and sync on the same execution branch. Synchronize complete verified workflow checkpoints when useful; preserve any blocked owned scope explicitly.
