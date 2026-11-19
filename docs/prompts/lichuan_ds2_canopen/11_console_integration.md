# 11 — Console and mixed-profile integration

Execute this stage under the [execution contract](execution_contract.md).
Status: NOT STARTED. Runs after the implemented subsets of 02b..10. Reinspect the landed main-sequence prompts 63,
64, 65, 70, 73, 75 and 77 and any other family that extended the shared seams before changing shared code. MULTIDRIVE
rows 23, 24, 40-44, 53, 64, 68, 70 and 75 apply.

## Work

1. Audit every `ds2` route delivered in 02b..10 against its public API: grammar, units, mutation class, result
   identity and JSONL fields per the [CLI contract](../../cli_contract.md). No register encoding, unit conversion or
   motion workaround lives only in a console handler, Python runner or HIL path. Arduino and native ESP-IDF builds call
   the same installed DS2 APIs through the shared application code.
2. One owner, two families. A Lichuan C node and a DS2 node on one interface form one CANopen domain under one
   `CanBusOwner`: one bitrate, distinct node IDs, COB-IDs taken from live reads (09) and checked for overlap before
   admission, one SDO client channel per node, reservations by bus and address whatever the profile label, separate
   caches and results, independent stops and urgent-stop capacity under cross-profile queue pressure. Claim fairness
   only with per-family producer slots and tests. Prompt 63's identifier plan is not needed for two CANopen families;
   the shared-interface positive tests wait for the same-domain analysis canopen_shared 02 requires, recorded in the
   report: node IDs, configured COB-IDs, bitrate, and NMT-broadcast and SYNC effects on both families. DS2's NMT
   effects are undocumented (DS-D32) and its SYNC effects unknown, so the analysis excludes NMT broadcasts and SYNC
   production on that interface until S13 and DS2 evidence admit them. A recorded exclusion is not enforcement; S13's
   default deny, landed with S2 (02b), already refuses both on every interface with a DS2 binding. Seam S13
   ([canopen_shared](../canopen_shared/README.md)) enforces both: it admits SYNC and every NMT command to node 0 on a
   mixed interface only for reviewed effects on every declared node, so a DS2 node with unknown effects refuses
   both. The shared-interface positive tests stay BLOCKED until S13 lands; DS2 consumes it with no DS2-local guard.
   Its tests then send SYNC and node-0 NMT requests from both families, and attach DS2 while a SYNC schedule is
   armed, with no forbidden TX. Until then DS2 runs on its own interface. No other protocol is admitted on that interface by this
   stage.
3. Axis table. After [prompt 64](../64_common_axis_interface.md) lands, add DS2's row to the common axis table and
   pass its session and owner tests; common verbs then dispatch by the bound profile with unchanged grammar and units.
   Torque stays a `ds2` native route unless a reviewed common torque intent exists.
4. Node table and startup. For [prompt 75](../75_runtime_node_table.md) and [prompt 70](../70_startup_settings_reboot.md)
   entries, the expected product identity of a DS2 node is its model's claim set and identity outcome (execution
   contract), not one value. Extend their identity check with the shared comparison, through their owners, rather
   than adding a DS2-only startup path. Startup verifies and reports; it enables, homes or writes nothing by default.
5. Discovery. Add a bounded, explicit DS2 scan over caller-listed nodes: probe (under 02b's read-effect row; without
   it the scan refuses before TX) and identity reads only, profile-typed
   findings with the identity outcome, no model selection from replies and no `2xxx` read. The `discover` parser and the
   Python module report DS2 findings; unknown or unrouted profiles still reject before TX.
6. Python and runners. `scripts/console_jsonl.py` validates `lichuan_ds2` records with DS2 fields and never accepts a
   Lichuan C check for a DS2 result. Finite scenarios select by declared capabilities and declarations; unavailable
   evidence is an explicit non-pass (MULTIDRIVE row 44).
7. Traffic observation. Debug copies decode DS2 frames by the admitted profile and node; an unassociated frame stays
   raw. Passive observers never consume owner RX or block service under console pressure.
8. Timing guards. Service timeouts, deadlines and freshness guards are per node and per profile
   ([prompt 73](../73_timing_freshness_guards.md)): DS2 nodes get their own documented defaults, optional and
   settable, with no SDO response time, boot time or PDO period taken from Lichuan C measurements (none is
   documented for DS2).
9. Packaging. After [prompt 65](../65_application_package.md), follow its component layout; vendor material stays out
   of every export. Measure DS2 context sizes, owner capacities (`MOTORCONTROLCAN_SDO_CHANNELS`, PDO routes) and RAM or
   stack differences on the firmware targets.
10. FieldCore-node. Record its current CAN consumer boundary read-only and any future integration need; no edit and
   no integration claim.
11. Help and capability output. `profile list`, `caps` and `ds2 help` show DS2 as a compiled candidate with its
   declared model, identity outcome, active declarations and open `DS-G` gates, distinct from qualified support; they
   come from the inventory and the ledger, never from detection. Record the final file map in `docs/source_map.md`.

## Deliver

Extended `LichuanDs2Console`, the DS2 scan in the shared discovery code, Python DS2 module and validation,
`test/lichuan_ds2_scan_test.cpp`, mixed-profile owner and console cases in `test/lichuan_ds2_console_test.cpp` and
`test/multidrive_contract_test.cpp`, the DS2 axis-table row when prompt 64 exists, memory measurements in the report,
the same-domain analysis, and updated `docs/api_cli_mapping.md`, `docs/cli_contract.md`, `docs/multi_axis.md`,
`docs/runners.md`, coverage, Doxygen, CHANGELOG and `docs/reports/lichuan_ds2_canopen_11.md`.

## Acceptance

Tests run a Lichuan C axis and a DS2 axis through one owner: overlapping COB-IDs refused before admission, same-address
reservations conflicting across profile labels and profile changes while work is pending refused; once S13 has
landed (item 2; otherwise BLOCKED, recorded in PROGRESS), wrong-node and late replies,
an urgent stop under queue pressure and the forbidden-TX cases of item 2. Exact API/CLI parity holds for every `ds2`
route. All Arduino and IDF
firmware gates of the contract build with pinned toolchains; quick and full verification and required CI on the
exact final pushed commit pass. Flashing is not required to prove compilation; any image on a board never sends DS2
frames to the CL86-C bench. Hardware NOT TESTED. Commit, push and hand off.
