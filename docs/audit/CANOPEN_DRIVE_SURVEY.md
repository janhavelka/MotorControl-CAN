# CANopen stepper and servo drive survey

Audit date: 2026-10-23. This is a documentation and architecture feasibility study, not implemented support, a purchase recommendation, or hardware qualification. Page numbers below are one-based PDF pages. The [source manifest](../reference/multidrive/SOURCES.json) records exact originals, versions, retrieval outcomes and hashes; the [reference guide](../reference/multidrive/README.md) distinguishes local files from browser-only sources.

## Decision

One library for stepper and servo drives is feasible and preferable. The common boundary is motion and its evidence, not motor construction. CANopen/CiA402 is a practical first protocol backend: the official profile covers servo drives and stepper motors, while explicitly warning that optional or incomplete implementations limit interchangeability. The same source distinguishes newer 64-bit position definitions and CANopen FD extensions; neither is automatically supported by older classic-CAN products. [CiA profile overview](https://www.can-cia.org/can-knowledge/cia-402-series-canopen-device-profile-for-drives-and-motion-control).

Keep Lichuan C as the first implementation, with its current documented quirks and qualification gates. Make Nanotec N5 and maxon EPOS4 mandatory design contrasts before freezing public units, capabilities and profile boundaries. A contrast means completing the source/object/API mapping and adversarial host fixtures; it does not require implementing or advertising those products now. Add a real second implementation only when a concrete model, firmware and motor configuration have been selected.

Use one public motor-neutral motion vocabulary, a separate CANopen service layer, and explicit vendor/model/firmware profiles. Motor type, feedback arrangement, supported modes and protocol are independent facts. Other CAN protocols can later implement the common operations without invented SDO, NMT or statusword behavior. No runtime plugin system or manufacturer SDK belongs in the portable core merely to make this extension possible.

## Candidate order and exact scope

| Priority | Candidate | Evidence and why it matters | Scope decision |
|---|---|---|---|
| First delivered family | Lichuan CL86-C/OL86-C; CL57-C/OL57-C separately evidenced | Existing manuals and EDS expose unit, statusword and sequence conflicts. These are finished drives, not an identified common silicon chip. | Preserve the existing [device research](DEVICE_PROTOCOL_RESEARCH.md); no inference that another Lichuan product shares this dialect. |
| 1: strongest mixed-motor contrast | Nanotec N5-1-2 / N5-2-2, hardware W007+, FIR-v2213 | N5 manual 3.3.0 provides one CANopen implementation for stepper and BLDC configurations, with a substantial object dictionary. Original PDF is local. | Mandatory design proof; preferred next implementation candidate if suitable hardware is later selected. |
| Related alternative, not an alias | Nanotec C5-E-1-09 / C5-E-2-09, hardware W006+, FIR-v2213 | Its separate CANopen/USB manual 3.3.0 is local. Hardware and supported configurations require their own profile applicability. | Prefer the compact C5-E if project constraints favor it; do not use the plain C5-01 USB/open-loop product as CANopen evidence. [C5-E product](https://www.nanotec.com/us/en/products/1768-c5-e-2-09-motor-controller-drive-for-canopen-or-usb), [C5-01 product](https://www.nanotec.com/us/en/products/1740-c5-01). |
| 2: first dedicated servo contrast | maxon EPOS4 Compact 50/5 CAN, part 541718 | Official product identifies brushed DC / brushless EC motors, several feedback options, and CiA301/305/402. Firmware and communication manuals were readable through the browser but direct original downloads returned HTTP 403. | Mandatory design proof; strongest next distinct servo profile after obtaining an exact local firmware/manual/EDS pack. [Product and official downloads](https://www.maxongroup.com/maxon/view/product/control/Positionierung/EPOS-4/541718). |
| 3: additional servo/control contrast | FAULHABER Motion Controller V3, exact CO variant | Local CANopen and Drive Functions manuals expose materially different monitoring and scaling behavior. MC 5005 S RS, CO and ET are distinct communication variants. | Useful later profile; reconcile firmware M communication documentation with firmware O drive documentation before implementation. [Official product selector](https://eshop.faulhaber.com/en/MC-5005-S/Serie-MC-5005-S). |
| 4: industrial servo contrast | Kollmorgen AKD, exact CANopen-capable variant and firmware | CANopen Communications Manual Rev Y, October 2020, applies to firmware 1.20. Browser-readable primary manual; original download returned HTTP 403. | Good industrial error/stop/scaling contrast, but older firmware scope and larger commissioning surface make it a later implementation. [AKD product](https://www.kollmorgen.com/en-us/products/drives/servo/akd), [official manual](https://www.kollmorgen.com/sites/default/files/public_downloads/903-200004-00%20AKD%20CANopen%20Communications%20Manual%20EN%20REV%20Y.pdf). |
| Further source audit | Delta ASDA-A2-M; investigate any proposed -MN suffix separately | Official product page assigns CANopen to A2-M and EtherCAT to A2-E. A2-M's listed CANopen modes are PP, interpolated position and homing. The proposed CANopen manual URL returned a terms-of-use page, not PDF bytes. | Do not assume a generic A2 or A2-E/U profile, or torque/PV support from the general servo feature list. Complete exact-suffix manual audit first. [Official ASDA-A2 page](https://www.delta-americas.com/en-US/products/Servo-Systems-AC-Servo-Motors-and-Drives/1471). |

This ordering evaluates evidence quality and architectural contrast, not price, electrical suitability or availability for purchase.

## What the contrasting manuals establish

### Nanotec N5

[N5 manual](../reference/multidrive/vendor/nanotec_n5_canopen_v3.3.0.pdf), pp. 18, 45–46, 61–66, 80–81, 111–112, 134, 345, 367, 376:

- Open-loop operation applies to steppers; profile torque and cyclic torque require closed-loop operation. Motor technology alone does not define capabilities.
- Position/speed units use configurable scaling. Objects 6083/6084 express acceleration/deceleration in user units, unlike Lichuan's millisecond ramp times.
- Target torque uses a rated-current/torque basis. Object 3202 bit 5 distinguishes torque-limited velocity from real torque operation without a speed limit.
- Eight RxPDO and eight TxPDO parameter/mapping sets are documented. Four PDOs is not a universal core limit.
- Index 2300 controls NanoJ, whereas Lichuan uses it for save/factory reset. Manufacturer-specific object numbers have no cross-vendor semantics.
- Heartbeat consumption, LSS and configurable communication-fault actions are documented. Page 345 nevertheless uses `SAFEOPERATIONAL` wording in a CANopen manual; that is an unresolved vendor wording issue, not a new CANopen NMT state.

These are sufficient contrasts to reject a public API built around fixed RPM/counts, step subdivision, four PDOs, or one universal torque command. They do not establish that every listed function works on every N5 firmware or motor setup.

### maxon EPOS4

The [July 2026 Firmware Specification](https://www.maxongroup.com/medias/sys_master/root/9579215585310/EPOS4-Firmware-Specification-En.pdf), PDF pp. 17, 20, 73–75 and 224, documents configurable SI units; PP, PV, homing, CSP, CSV and CST; heartbeat consumer behavior governed by communication-error configuration; and identity fields with product/software-specific interpretation. Target torque is in thousandths of rated torque, with rated torque derived from nominal current and torque constant. Profile Torque mode 4 must not be assumed from the presence of CST.

The [April 2026 Communication Guide](https://www.maxongroup.com/medias/sys_master/root/9567255756830/EPOS4-Communication-Guide-En.pdf) is a separate source. A future EPOS4 profile needs both communication and firmware descriptions, exact EDS and firmware applicability. The product also exposes feedback and brake options; the generic motion API should report these as capabilities and configured evidence, not imply all have been wired or enabled.

### FAULHABER V3

The local [Drive Functions manual](../reference/multidrive/vendor/faulhaber_mc_v3_drive_functions_05048.pdf), firmware O, pp. 8, 11, 66–67, 95, 140 and 202, covers DC, brushless and linear servomotors. Stepper support is specifically added for MC 3602 B / MC 3606 B; it cannot be attributed to every V3 product. Position and speed scaling are independently configurable. CST explicitly has no speed limiting; its speed-warning threshold generates error-monitoring information rather than imposing the speed profile. Supported mode values do not establish Profile Torque mode 4.

The local [CANopen manual](../reference/multidrive/vendor/faulhaber_mc_v3_canopen_05050.pdf), firmware M, pp. 5, 30–33 and 41, states that node guarding and heartbeat cannot both be active. Its consumer configuration makes producer settings ineffective, despite earlier general producer/consumer wording. The default consumer event is EMCY 8130; reaction is configurable through vendor error handling. Objects 1016/1017 default to disabled. Confirm these interactions against the selected firmware and actual device; do not silently normalize them to another manufacturer's behavior.

The two manuals' firmware coverage differs. That is a binding gate for a future profile, not permission to combine all capabilities into a hypothetical maximum configuration. The older `EN_7000_05037.PDF` family manual is not the V3 authority used here.

### Kollmorgen AKD and Delta

The [AKD manual](https://www.kollmorgen.com/sites/default/files/public_downloads/903-200004-00%20AKD%20CANopen%20Communications%20Manual%20EN%20REV%20Y.pdf), PDF pp. 38–39, 97, 100, 112–113 and 129, documents node-guard/heartbeat services, configurable quick stop, gear/feed and velocity scaling, rated-torque units and a separate torque-mode velocity limit. Node-guard timeout has a specified drive fault/reaction. Object 6083 has mode-dependent relations to native acceleration parameters. These are useful counterexamples to assuming that one communication timeout, stop function or scaling formula works across profiles.

Delta's official product evidence is sufficient to include A2-M as a candidate, but insufficient here for an object-level profile. The manual retrieval limitation must remain visible. No claim about -MN, mode 4, cyclic torque or exact object units is made from the unavailable manual.

## Required architecture consequences

The following are engineering recommendations drawn from the contrasts above, not claims that the corresponding APIs already exist.

| Seam | Shared contract | Profile or application responsibility |
|---|---|---|
| Identity | Logical axis handle independent of CAN node, channel and subaxis; explicit binding and invalidation | Vendor/product/revision/serial evidence, firmware range and permitted explicit overrides. Identity alone does not prove motor setup or scaling. |
| Motion | Preserve current MotorControl-RS common names and operation-specific preparation/iteration/completion semantics | Mode availability, enable/acceptance handshake, target-reached interpretation, homing method and supported options. Unsupported operations must fail before transmitting. |
| Units | Exact quantities with checked conversion, overflow and quantization policy; retain raw native values where needed | Native width/signedness, physical dimension, rational or explicitly bounded conversion, gear/feed/encoder factors, scaling provenance and freshness. Do not cast every target to signed 32-bit or equate acceleration with ramp duration. |
| Torque/current | Distinct torque, normalized rated-torque and electrical current quantities | Motor constants, rating source, control mode, speed-limiting behavior and thermal/peak constraints. A current-derived estimate is not a measured shaft torque. |
| Feedback | Report quantity, freshness, validity and origin | Encoder/load feedback versus commanded, estimated or open-loop position; following-error definition and source. A field named actual position does not by itself prove measured load position. |
| CANopen services | Bounded transport, SDO/NMT/PDO/EMCY/heartbeat/SYNC primitives and evidence; consider an established stack | Supported object access, mapping counts, segmented services, event timing, consumer roles, LSS and node guarding. These are capabilities, not mandatory behavior on every profile. |
| Stops and faults | Distinguish request, transmission, acceptance and observed result; retain execution uncertainty | Quick-stop/halt/disable/fault-reaction options, brake coordination and error masks. Neither local cancellation nor NMT stop is a universal physical stop. |
| Configuration | Explicit commands, checked schemas and invalidation of dependent cached facts | Commissioning, motor/feedback setup, autotuning, persistent save, node/bitrate changes and vendor-specific reset/program functions. Startup/probe does not perform these actions. |
| Ownership | Caller-owned contexts, one application CAN owner, clocks/queues/retry/health policy outside the core | Physical interface/stack adapter, scheduling, hardware setup, configured watchdog policy, power and mechanical workflows. |

Keep protocol status, NMT state, CiA402 state, mode, command outcome and application health separate. Distinguish the host observing a heartbeat from the drive consuming the host's heartbeat and actually enforcing a configured loss reaction. Do not auto-replay motor commands or auto-enable after recovery.

Classic CANopen can deliver useful servo support through PP/PV/homing without a hard-real-time cyclic-motion implementation. CSP/CSV/CST require a separate explicit milestone covering SYNC scheduling, bus load, missed-cycle handling, bounded jitter, PDO mapping and evidence on the chosen hardware. Deferring that milestone does not mean the library only supports steppers. Conversely, merely exposing torque objects does not establish controlled cyclic torque operation.

The future common API may preserve wider exact values so a later 64-bit or different-protocol backend is possible; do not claim those devices or CAN FD are implemented. Keep native representation and transport limits explicit at each conversion boundary.

## Minimum proof before adding another profile

1. Refresh MotorControl-RS, relevant I2C libraries and FieldCore-node ownership/CLI/tests/runners. Document adopted names and meaningful protocol differences rather than importing a vendor SDK naming scheme.
2. Pin exact drive variant, firmware, hardware revision, protocol, manual and EDS. Record motor and feedback configuration separately from drive identity. Resolve source conflicts before enabling affected writes.
3. Build a source-linked object matrix: native type/access/unit/PDO support, scaling dependencies, validity conditions, state/mode/status interpretation, stop/fault/heartbeat behavior and persistence consequences.
4. Demonstrate current common operations against at least two contrasting mappings. Include non-default scaling, unsupported modes, missing feedback, overflow/quantization, PDO capacity differences, ambiguous identity and stale configuration in host tests.
5. Implement only the selected capability subset with bounded resources and explicit CLI reporting. Distinguish documented, implemented, host-tested and hardware-qualified coverage per model/firmware/configuration.
6. Obtain separate bench authorization and exact wiring for hardware qualification. Commissioning, enabling, motion, loss-of-host tests and stop behavior need deliberate workflows; documentation research does not establish their results.

## Source access and remaining gaps

Eight original local files were obtained: four PDFs and four manufacturer HTML pages, plus four derived text extracts. The excerpts are navigation aids; verify PDF tables and diagrams directly. No broader-survey EDS, firmware binary, installer or SDK was downloaded or executed. No vendor was contacted and no hardware was accessed.

maxon and Kollmorgen manuals were readable through the browser search tool, but direct downloads returned HTTP 403; they are recorded as remote-only with no invented size/hash. Delta's proposed PDF returned a terms-of-use page. Full normative CiA documents were not obtained; public overviews cannot substitute for the applicable standard when implementing services. Public download access is not an open redistribution license. Vendor material remains outside any future source-code license; review redistribution rights before public release.

Before a second supported profile is promised, acquire the exact firmware-matched EDS and missing originals, resolve FAULHABER firmware/monitoring discrepancies and Nanotec's mixed-state terminology, and select an actual motor/feedback configuration. These gaps do not prevent motor-neutral architecture or host-only design work now.

## Recurring ecosystem audit for this report

The source refresh was read-only. At refresh time MotorControl-RS was `a1f70d0bf91c7c33fd83909d7b9d21b4a777c381`, with untracked LC57BK-R reference material, extraction files and preparation script; ADS1115 was `381236ebfed813cd7a9aca34520ce031d8a0ff6b`; INA228 was `b28c5ea69bcb4d26b351c52c24d3c6fae9fd282b`; FieldCore-node was `da94ed5280ed42ee3a55e0d977f9cb6565c56b51`. The latter three worktrees were clean. Revisions and dirty states are snapshots, not upstream-sync claims.

Relevant reads included RS `include/MotorControlRS/Axis.h` and `docs/reference/07_canopen_feasibility.md`, ADS1115 `include/ADS1115/ADS1115.h` and `examples/02_owner_safe_poll/main.cpp`, INA228 `include/INA228/INA228.h` and `tools/check_owner_contract.py`, and FieldCore-node `src/devices/rs485_motor/Rs485MotorModule.cpp`. Their owner boundaries and existing motion vocabulary remain the ecosystem baseline. The vendor research adds profile-specific units, capabilities and protocol evidence; it does not replace those ownership contracts or authorize edits in sibling repositories.
