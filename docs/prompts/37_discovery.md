# Prompt 37 Non-changing discovery and binding

Apply [COMMON.md](COMMON.md). Prerequisites: [19](19_typed_reads.md), [25](25_health_stats.md).

## Objective and boundaries

Deliver finite non-changing discovery, implemented-profile inventory and explicit host binding selection. Discovery establishes only evidenced presence/identity/capabilities. Device communication changes and persistence are prompts 38–39. Do not discover a drive by changing NMT, heartbeat, node ID, bitrate, fault state or configuration.

Follow [CONVENTIONS](CONVENTIONS.md#4-one-application-can-owner) and [MULTIDRIVE](MULTIDRIVE.md). Generic CANopen identity is not proof of model, interface variant, configured motor/feedback or firmware support.

## Read and audit

Refresh MotorControl-RS, ADS1115 and INA228 revisions and dirty state. Read RS `Discovery`, profile inventory, minimal probe and uncertainty tests. Compare MB85RC family selection and actual I2C discovery CLI behavior without copying its address/retry policy. Inspect current FieldCore binding ownership read-only. Recheck Lichuan identity evidence and the contrast/profile support inventory.

### Concrete evidence to recheck

Recheck [identity and DIP facts](../audit/LICHUAN_OBJECT_FACTS.md). EDS identity defaults are vendor `0x1000`, product 1, revision 0; `0x1018` lists three identity fields and no serial subindex. `0x1009/0x100A` are U16, with defaults `0x2608`/1, while vendor `0x2000/0x2001` default 1/0. These values are generic metadata, not unique CL86-C/firmware detection. A configured host node binding or explicit conservative profile selection remains necessary when identity is insufficient.

## Implement

1. Implement `Discovery.h` and profile-specific discovery responsibilities where separate files are justified. Preserve `getDiscoveryProfile`, `getDiscoveryCapabilities` and the minimal `prepareProbe` meaning. List implemented profiles only; surveyed N5/EPOS4 and proprietary protocol fixtures are not supported drivers.
2. Provide bounded discovery through the existing owner with explicit target set, selected protocol domain, current interface bitrate, service budget and finite deadline. Use only reviewed non-changing reads; identity reads remain distinct from a minimal presence probe.
3. Treat passive heartbeat/PDO observations as their actual facts. Disabled heartbeat can coexist with a serviceable device. Outcomes distinguish no response, generic identity, unsupported firmware, contradictory identity and suspected duplicate node IDs.
4. Document duplicate-detection limits. One valid reply cannot prove a collision-free bus. CANopen node zero is not an ordinary addressed target; do not impose the CANopen 1–127 range on all future protocols or send foreign-protocol guesses across every CAN identifier.
5. Implement host binding selection separately from wire work. Selecting an existing binding performs no read, communication write, reset, save or restart. Generation changes invalidate relevant prepared work/caches through existing owner rules while preserving historical results.
6. Keep protocol/profile/model/firmware match confidence and configured motor/feedback facts explicit. Unsupported or unknown selection does not create a fabricated fully capable profile. Inventory/capability queries are cached and bounded.

## Verify and exit

Test finite scan and service bounds, current-bitrate restriction, absent heartbeat, unrelated vendor, unsupported or unresolved identity, contradictory replies, duplicate-looking traffic and wrong binding generation. Assert discovery and selection issue no NMT or device-setting mutations. Test result retention and cancellation without a hidden retry or reset.

Exercise both native API discovery and host binding selection through the existing fake owner. Verify generic identity never upgrades implementation or hardware-qualified status. Exit with complete finite discovery/inventory behavior, honest ambiguity and documented unsupported routes. Run focused discovery/read/owner tests and consumer/documentation gates.

## Documentation and delegation

Apply [DOCUMENTATION](DOCUMENTATION.md), [MAINTAINABILITY](MAINTAINABILITY.md) and [DELEGATION](DELEGATION.md). Document the order of host selection, minimal probe and identity read, plus disabled-heartbeat and duplicate-ID limits. Doxygen specifies bounds, output lifetime and no-change behavior. Normal coding agents can split inventory and discovery tests once the shared binding contract is fixed; an independent reviewer checks for accidental mutation and overclaimed identity.

Record results in `docs/reports/prompt_37.md`, update discovery coverage and setup guidance, then commit and sync on the same execution branch. Useful complete verified checkpoints may be synchronized during the prompt. Physical discovery/commissioning remains separately recorded bench work.
