# Prompt 37 Explicit discovery and binding

Apply [COMMON.md](COMMON.md). Prerequisites: [19](19_typed_reads.md), [25](25_health_stats.md).

## Objective and boundaries

Deliver implemented-profile inventory, capability queries, explicit host binding selection and passive heartbeat and boot-up discovery. Active SDO identity discovery is explicit-request only and follows the planned read-qualification and channel-release changes; write the slot and timeout design before implementing it. Discovery establishes only evidenced presence/identity/capabilities. Device communication changes and persistence are prompts 38–39. Do not discover a drive by changing NMT, heartbeat, node ID, bitrate, fault state or configuration.

Follow [CONVENTIONS](CONVENTIONS.md#4-one-application-can-owner) and [MULTIDRIVE](MULTIDRIVE.md). Generic CANopen identity is not proof of model, interface variant, configured motor/feedback or firmware support.

## Read and audit

For this prompt, refresh MotorControl-RS HEAD and dirty state and read the RS code for this seam; RS is authoritative. Check HEAD and dirty state only for the siblings named below, and inspect only the seam stated. Inspect current FieldCore-node read-only for ownership or integration changes. If HEAD, dirty state and seam match the last recorded row, write "unchanged since prompt N" and do not reread. A sibling row must name a behavior adopted or rejected, with path and symbol. This does not relax vendor evidence, safety, uncertainty or verification rules. Read RS `Discovery`, profile inventory, minimal probe and uncertainty tests. Compare MB85RC AUTO versus fixed variant selection. Use FieldCore `rs485 discover` as the bounded-probe contrast; an I2C `scan` proves ACK only and is no CAN model. Recheck Lichuan identity evidence and the contrast/profile support inventory.

### Concrete evidence to recheck

Recheck [identity and DIP facts](../audit/LICHUAN_OBJECT_FACTS.md). EDS identity defaults are vendor `0x1000`, product 1, revision 0; `0x1018` lists three identity fields and no serial subindex. `0x1009/0x100A` are U16, with defaults `0x2608`/1, while vendor `0x2000/0x2001` default 1/0. These values are generic metadata, not unique CL86-C/firmware detection. The EN86 pp15-16 table lists `1000`, `1009` and `100A`; `1018` appears only in the EDS. A configured host node binding or explicit conservative profile selection remains necessary when identity is insufficient.

## Implement

1. Implement `Discovery.h` and profile-specific discovery responsibilities where separate files are justified. Preserve `getDiscoveryProfile`, `getDiscoveryCapabilities` and the minimal `prepareProbe` meaning. List implemented profiles only; surveyed N5/EPOS4 and proprietary protocol fixtures are not supported drivers.
2. Provide bounded discovery through the existing owner with explicit target set, selected protocol domain, current interface bitrate, service budget and finite deadline. Reads are typed uploads with no write intent. `prepareProbe` uses `1000:00` under DEC-1, whose effect stays recorded as unknown. Identity discovery runs only on explicit request and stays distinct from a minimal presence probe. A probe timeout leaves its channel slot unresolved until a drain marker, so a scan over absent nodes must use a free slot and report what it consumed. Installed code holds the inventory, `prepareProbe` and `checkProbe`; the scan loop and retained findings live in `examples/common`, as in RS.
3. Treat passive heartbeat/PDO observations as their actual facts. Disabled heartbeat can coexist with a serviceable device. Outcomes distinguish no response, generic identity, unsupported firmware, contradictory identity and suspected duplicate node IDs.
4. Document duplicate-detection limits. One valid reply cannot prove a collision-free bus. CANopen node zero is not an ordinary addressed target; do not impose the CANopen 1–127 range on all future protocols or send foreign-protocol guesses across every CAN identifier.
5. Implement host binding selection separately from wire work. Selecting an existing binding performs no read, communication write, reset, save or restart. Generation changes invalidate relevant prepared work/caches through existing owner rules while preserving historical results.
6. Keep protocol/profile/model/firmware match confidence and configured motor/feedback facts explicit. Unsupported or unknown selection does not create a fabricated fully capable profile. Inventory/capability queries are cached and bounded.

## Verify and exit

Test finite scan and service bounds, current-bitrate restriction, absent heartbeat, unrelated vendor, unsupported or unresolved identity, contradictory replies, duplicate-looking traffic and wrong binding generation. Assert discovery and selection issue no download or NMT. Test result retention and cancellation without a hidden retry or reset.

Exercise both native API discovery and host binding selection through the existing fake owner. Verify generic identity never upgrades implementation or hardware-qualified status. Exit with complete finite inventory, host selection and passive discovery behavior, with SDO identity discovery as far as its gates allow, honest ambiguity and documented unsupported routes. Run focused discovery/read/owner tests and consumer/documentation gates.

## Documentation and delegation

Apply [DOCUMENTATION](DOCUMENTATION.md), [MAINTAINABILITY](MAINTAINABILITY.md) and [DELEGATION](DELEGATION.md). A new header or source needs `CMakeLists.txt`, `test/package_test.py`, `scripts/check_repository.py` and `test/repository_test.py` updated together. Document, in `docs/commissioning.md`, the order of host selection, minimal probe and identity read, plus disabled-heartbeat and duplicate-ID limits. Doxygen specifies bounds, output lifetime and behavior that issues no download or NMT. Normal coding agents can split inventory and discovery tests once the shared binding contract is fixed; an independent reviewer checks for accidental mutation and overclaimed identity.

Record results in `docs/reports/prompt_37.md`, update discovery coverage and setup guidance, then commit and sync on the same execution branch. Useful complete verified checkpoints may be synchronized during the prompt. Physical discovery/commissioning remains separately recorded bench work.
