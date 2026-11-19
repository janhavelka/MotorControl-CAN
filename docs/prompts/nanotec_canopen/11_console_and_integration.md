# 11 — Console and mixed-profile integration

Execute this stage under the [execution contract](execution_contract.md).
Status: NOT STARTED. Runs after the independently implemented subsets of 02b-10.
Reinspect the
current Lichuan application code and any other family that landed since 02b before
extending the seams.

## Work

1. Once S4 has landed and its consumer gate passed (item 2), audit the `nanotec`
   family delivered in 02b-10: each route calls its public API,
   has a parity test and a `console` row in the operation map that the checker proves.
   Existing top-level verbs keep their Lichuan meaning. There is no selected-profile
   mode; family commands and, after [prompt 64](../64_common_axis_interface.md), common
   verbs dispatched by the target axis's bound profile use the same installed APIs,
   grammar and units.
2. Seam S4 (prompt 64). When it has landed, implement the Nanotec row of the common
   axis table and pass that prompt's session and owner tests; add no second session.
   Until then only the native Nanotec API and its owner-backed tests exist: the family
   console routes, common-verb dispatch and their API/CLI parity acceptance stay
   Blocked (execution contract, "Owner and bus rules"), with no second session and no
   temporary family-specific `ProbeApp`. Return work after S4's consumer gate passes,
   in stage order: the console routes and parity tests of 02b item 6, 03, 04, 04b, 05,
   06a-06b, 07, 08a-08d, 09, 09b and 10, then item 1's audit and items 6-7 here. If
   prompt 64 is still pending, record that Nanotec could be its real second profile
   instead of a test-only one; the decision belongs to prompt 64's execution.
3. Node table ([prompt 75](../75_runtime_node_table.md)): a Nanotec entry records
   profile, declared model, expected identity row and role; a monitor-only entry is
   observed passively. No entry implies services the profile refused.
4. Mixed bus. By default Lichuan and Nanotec run on separate interfaces. A shared
   interface under one owner needs a recorded same-domain analysis first
   (canopen_shared 02 "Multi-profile consumers and package scope"): node IDs,
   configured COB-IDs, bitrate, and NMT-broadcast and SYNC effects on both families
   (S13 entries); record it in this stage's report. The analysis does not admit a
   broadcast: every node-0 NMT command on that interface still passes the owner's
   S13 NMT admission (09 item 6), with zero NMT TX on refusal, tested through the real
   owner with an unknown or stale effect and with an unadmitted effect on a node other
   than the target. The user may instead require
   prompt 63 ([open question](README.md#open-questions) 2). With the analysis
   recorded, test with the fake transport: independent stops; reservations that conflict
   on the same bus and node whatever the profile label; separate caches, results and
   identity verdicts; urgent stop capacity under cross-profile queue pressure; late
   completions routed by the admitted profile and generation. Check the identifier
   plan at binding: node IDs unique, Nanotec PDO 5-8 COB-IDs (free) not colliding with
   any Lichuan or other node identifier, both boot-up messages of a Nanotec node
   routed to that node, and SYNC and EMCY identifiers unchanged; the same check runs
   again at the admission of each COB-ID change, with the old and candidate IDs both
   reserved until activation is evidenced (09 item 4). Land this check first, in its
   own tested commit: a reusable per-interface check with atomic reservation of the
   old and candidate IDs. That commit unblocks 09 item 4's COB-ID change and runs 09's
   COB-ID acceptance cases; the topology analysis and coexistence tests above stay
   with the rest of this item. No different-node
   assumption replaces this check.
5. Discovery. Bounded scans over caller-listed nodes only; the probe is the 02b upload
   of 1000h and the identity read of 03. A finding is profile-typed and never selects
   a profile by itself (backlog D18). Unqualified rows with their gates and product
   contradictions are reported as such. No
   global CAN-ID sweep, no LSS, no NMT broadcast.
6. Status, `caps`, help and runtime debug output: decode copied traffic by the admitted
   profile; an unassociated frame stays raw. Show identity verdict, control paths
   (04b), supervision state (09b), unqualified rows and their active gates explicitly. Passive observation
   copies owner data and never consumes frames or blocks protocol progress.
7. Python: extend the S6 per-record `profile` validation of `scripts/console_jsonl.py`
   (from 02b) and add a Nanotec module
   for the bench client with finite, capability-selected plans in `python_plans`;
   reject duplicate, missing or mismatched-profile records; never replay an
   uncertain command.
8. Platforms and consumers: Arduino and native ESP-IDF build the same application,
   owner and console; no Nanotec encoding, unit conversion or workaround lives only in
   a console handler or runner. Measure request, event, result and cache storage and
   RAM and stack watermarks before and after. After [prompt 65](../65_application_package.md)
   follow its component layout (seam S5).
9. FieldCore-node read-only: record its current CAN owner and module boundaries and
   what a Nanotec integration would need; make no edit and claim no integration.

## Deliver

Extended `NanotecCanopenConsole`, application records and discovery; a Nanotec
Python module and plans; `test/nanotec_canopen_scan_test.cpp` and mixed-profile owner
and console cases; updated `docs/cli_contract.md`, `docs/api_cli_mapping.md`,
`docs/profile_contract.md`, `docs/source_map.md` and the coverage file.

## Acceptance

Two profiles and several nodes run through one owner: profile changes with work
pending, wrong-node frames, fair scheduling where claimed, urgent stops, late replies,
console saturation and exact API/CLI parity (the console and parity parts once S4
has landed; until then they are recorded Blocked and the stage is PARTIAL). All
affected Arduino and IDF targets build with the pinned toolchains; quick and full
verification and CI on the exact final pushed commit pass. Flashing is not needed to prove compilation, and this stage
flashes nothing and opens no port (canopen_shared 02 "Bench steps and owner
absence"). If an independently authorized Lichuan lead runs the CL86-C regression on
an image containing this work, the report may cite that run as Lichuan-only evidence.
Hardware NOT RUN for Nanotec; owner-dependent fixture steps follow the execution
contract's deferred-table rule.
