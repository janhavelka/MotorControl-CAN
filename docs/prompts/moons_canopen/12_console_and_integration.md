# 12 — Console and mixed-profile integration

Execute this stage under the [execution contract](execution_contract.md).
Status: NOT STARTED. Requires the implemented subsets of 02b-11; stage 13 is not a prerequisite. Consumes
prompt 64 (common axis table), prompt 65 (package components) and prompt 75 (runtime node table) when landed;
the dependent items stay Blocked otherwise while the rest proceeds.

## Work

1. Audit every MOONS' route delivered in 02b-11 against its public API: one `commands[]` entry, parser and
   formatter in `MoonsCanopenConsole.{h,cpp}`, no register encoding, unit conversion or motion workaround that
   exists only in a console handler, runner or HIL path. Existing top-level verbs keep their Lichuan meaning;
   there is no selected-profile mode. After prompt 64, common verbs may dispatch by the target axis's bound
   profile with unchanged grammar and units.
2. Prompt 64: implement the MOONS' row of the common axis table and pass that prompt's session and owner tests;
   add no second session. Capability refusals (torque on STF-C, index homing on STF-C, Q and SYNC Q, velocity
   change) come from the profile, not the console.
3. Prompt 65: follow its component layout for packaging; vendor material stays out of every package and
   archive (`test/package_test.py` exact archive set).
4. Prompt 75: a runtime node-table entry for a MOONS' node records the profile, the declared product row
   (STF-C or SSDC-C), node ID, bitrate and role; a monitor-only entry is observed passively and never probed
   by writes. The table never selects the product from a 0x1018 reply.
5. Mixed-profile owners: by default Lichuan and MOONS' axes run on separate interfaces with independent stops,
   caches and results. A shared interface is admitted only with stage 09's recorded analysis; then test one
   bitrate intersecting both profiles' admitted rates, reservations that conflict on the same bus and node ID
   whatever the label, heartbeat, EMCY and PDO frames routed by node to the admitted profile and generation,
   the S13 SYNC refusal in both directions, and an unknown or unrouted profile rejected before TX. Late
   completions route by the admitted profile, never by a later selection.
6. Storage: size request, event, result and cache storage from the actual largest MOONS' operations; keep the
   one program-wide `MOTORCONTROLCAN_SDO_CHANNELS` and `MOTORCONTROLCAN_PDO_ROUTES` capacity set and check that
   four RPDOs and four TPDOs per MOONS' node fit it, or document the required setting. Measure RAM, stack
   (`python scripts/check_stack.py --strict`) and image size before and after.
7. Help, caps, status and debug: derived from the inventory; status decoding by admitted profile; passive trace
   copies labelled with the admitted profile and left raw when unassociated; observers never consume owner RX
   or block protocol progress under console pressure.
8. Python tooling: `scripts/console_jsonl.py` accepts `moons_canopen` records under S6 and rejects records
   whose profile does not match the admitted operation; bounded family plans for offline and dry-run use only.
   The S2 bench scripts stay Lichuan-only and refuse a MOONS' target.
9. Discovery: a bounded scan over caller-declared node IDs that runs the MOONS' probe only; no global CAN-ID
   sweep and no frames of another protocol. A probe finding is "MOONS' vendor ID at node n", never a product.
10. FieldCore-node: inspect read-only whether its owner and module boundaries admit a second CANopen profile;
    record the facts and required future changes; make no FieldCore edit.
11. Platform parity: Arduino and native ESP-IDF compile the same application, owner and console; platform
    sources provide clock, transport and startup only. A native example without either framework shows one
    MOONS' read and one refused unsupported request (for example torque on STF-C) through the public API.
12. `profile list` and `caps` show MOONS' as a compiled candidate with its documented modes per product and
    every gated capability as unsupported with its reason; they never claim detection, qualification or
    TSM/TXM-C support beyond stage 13's delivered part.

## Deliver

Extended `MoonsCanopenConsole`, the MOONS' axis-table row, node-table support, discovery scan support and the
Python profile handling; `test/moons_canopen_scan_test.cpp`; mixed-profile cases in
`test/moons_canopen_owner_test.cpp` and `test/multidrive_contract_test.cpp`; API/CLI parity for every MOONS'
route; `docs/api_cli_mapping.md`, `docs/cli_contract.md`, `docs/source_map.md`, `docs/coverage.md`; memory and
stack measurements; report.

## Acceptance

Two profiles and several nodes run through one owner with fair scheduling as far as producer slots exist,
urgent stops under pressure, wrong-node and late frames, profile changes with work pending, console saturation
and exact API/CLI parity. Arduino and native IDF build the same application (`python scripts/check_firmware.py
--target all` with the pinned IDF). Quick and full verify and exact-commit CI pass for all required jobs.
Flashing is not needed to prove compilation; no MOONS' frame goes to the CL86-C bench. Hardware: NOT RUN.
