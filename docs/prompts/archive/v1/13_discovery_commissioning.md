> Historical v1 promptset (00-19), retained for traceability. Do not execute this copy. Use the [current prompt index](../../README.md). Relative documentation links were relocated; task wording is retained from `dc8270740d0e16f19e170e2a982f67389c8a2c56`.

# Stage 13 Implement discovery, commissioning and persistence

Apply [COMMON.md](COMMON.md). Prerequisites: stages 08 and 12.

## Audit

Read RS Discovery/Communication/Persistence contracts, narrow presence probe and uncertainty tests. Compare MB85RC family selection, RV3032 persistence and MCP45HVX1 explicit output controls. Inspect I2C discovery CLI and FieldCore binding ownership. Recheck source evidence for every Lichuan topology/persistent operation.

## Deliver

1. Implement finite profile/manufacturer inventory and bounded non-changing discovery through the owner. Passive heartbeats/PDOs show only their evidenced facts; probe uses a documented minimal read. Identity reads remain separate. Never change node ID, bitrate, NMT state, fault state or heartbeat configuration to discover a device.
2. Define outcomes for no response, disabled heartbeat, generic identity, unsupported firmware, contradictory identity and suspected duplicate node IDs. Duplicate IDs cannot always be detected; do not claim a collision-free network solely from one valid reply. Node 0 is never an ordinary addressed target.
3. Implement explicit commissioning requests for supported node/communication settings, host binding changes, heartbeat parameters and PDO configuration. Separate host-side selection from drive changes and state required DIP/parameter/restart/persistence behavior from evidence.
4. Implement typed save/restore/reset commands only from resolved semantics. Object 2300 value 2 is save and value 1 factory reset in the source; no generic defaults dump. Preserve partial/unknown outcomes and require live post-change reconciliation. No automatic replay or presumed rollback after a timed-out write.
5. Track actual, desired, observed and persistent configuration independently. Node-ID/bitrate changes may make the old route unreachable; do not send retries through guessed addresses or scan with writes. Invalidate mappings/identity/reference and clear only host work whose retirement is established.
6. Add setup documentation: actual device label/manual revision, CAN transceiver/termination/wiring, node IDs, bitrate, identity read, configuration inspection and explicit changes. Include optional I/O requirements only for selected operations. Capture manufacturer supply-range conflicts without selecting a guessed electrical rating.

## Concrete implementation contract

Use `Discovery.h` and `profiles/lichuan_c/{Discovery,Communication,Persistence}.h` where the responsibilities justify separate files. Preserve `getDiscoveryProfile`, `getDiscoveryCapabilities` and `prepareProbe` semantics. The inventory lists implemented profiles only; surveyed N5/EPOS4 and proprietary protocols remain research/test fixtures.

Keep four operations visibly separate: select an existing host binding; perform a non-changing read; change a device communication parameter; save/restart/reset the device. A host selection command must not silently execute any of the other three. Read discovery is finite, bounded by explicit nodes/service budget and current interface bitrate; do not guess bitrates or broadcast foreign protocols to find a response.

Use typed operation contexts for confirmed save/reset/communication workflows, with the same prepare/next/advance pattern and retained evidence. Object `0x2300` values 1 and 2 have distinct meanings under the reviewed Lichuan profile; no common “persist” integer accepts arbitrary magic values. A timed-out save/reset can have happened. Keep current route and desired route distinct until reconciliation proves the change.

For each change specify: preconditions, affected bindings, volatile effect, persistence behavior, restart requirement, expected reply route, known/unknown partial effects and readback method. A loss of response on the old node ID or bitrate is not proof of a successful change. Never retry via a guessed new route automatically.

Keep original configuration records with provenance, but do not promise transactional rollback. A restore is another set of explicit writes and can fail partially. No discovery/helper/runner runs factory reset to obtain a known starting state. Commissioning docs identify the actual model label and electrical evidence rather than treating the Alibaba title as an authoritative electrical manual.

## Documentation and work split

Apply [DOCUMENTATION.md](DOCUMENTATION.md) and [MAINTAINABILITY.md](MAINTAINABILITY.md). Publish commissioning order, examples of host-versus-device changes, save/reset distinctions and failure reconciliation. Doxygen states persistent effects and route invalidation. Close the corresponding stage-13 ledger rows.

Under [DELEGATION.md](DELEGATION.md), discovery tests and source review of persistence effects are independent. Keep state-changing workflow design with one implementer and require independent review of every uncertain-write path.

## Verify and exit

Test finite scan bounds, passive discovery, absent heartbeat, unrelated vendor, unresolved identity, duplicate-looking traffic, unsupported operations before TX, save/reset distinction, timeout after communication write, restart and rebind, wrong readback and no hidden writes on startup/probe/recover. Assert persistent operations are never retried automatically.

Exit with reproducible commissioning APIs and documentation, explicit uncertainty and preserved normal motion vocabulary. Physical commissioning remains a separately recorded bench activity.
