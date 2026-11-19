# 11 — Console and multi-protocol integration

Execute this stage under the [execution contract](execution_contract.md).
Status: NOT STARTED. Run after the implemented subsets of 02b-10. Consumes [prompt 64](../64_common_axis_interface.md)
(seam S4) and [prompt 65](../65_application_package.md) (seam S5) where they have landed; reinspect the landed Lichuan
and any other family's console, discovery and owner code before extending the 02b seam.

## Work

1. Audit the `mks` family and every route delivered by 03-10 against the [CLI contract](../../cli_contract.md) and
   the API/CLI map. One command inventory drives help, dispatch, mutation classification, both firmware adapters,
   native console tests and the Python parser. No modal profile selector; no MKS encoding, unit conversion or motion
   workaround lives only in a console handler, Python script or HIL path.
2. After prompt 64, common verbs dispatch by the target axis's bound profile with unchanged grammar and units; the MKS
   row of its axis table passes that prompt's session and owner tests. Commands an MKS axis cannot serve (NMT,
   heartbeat, network, SYNC, PDO, save) refuse before TX with the evidence-based reason. Prompt 75's runtime node table
   records profile, role, dialect declaration and expected board code per MKS entry; prompt 73's per-protocol timeouts
   and freshness guards get MKS values from the application, not from CANopen defaults.
3. Interface ownership: one owner and one exclusive MKS domain per physical interface (02b). A firmware that runs
   CANopen axes and MKS axes needs separate physical CAN interfaces; the current examples target boards with one TWAI
   controller, so a second interface is a board and adapter milestone recorded in the backlog, not a profile shortcut.
   Tests refuse an MKS binding on a CANopen interface and the reverse, and keep stops, reservations, caches and results
   independent per interface.
4. Fairness and stops: give the MKS producer its own slot if owner fairness is claimed, and test urgent MKS stops
   under queue pressure on its interface. A completion routes by the admitted profile, address and generation, never
   by a later console selection.
5. Discovery: a bounded scan over an explicit candidate list of slave IDs at the current bitrate and declared dialect,
   one outstanding probe read (02b's per-dialect probe) at a time, no broadcast, no group frames, no writes, no bitrate or check-mode guessing.
   Findings are profile-typed and claim a responder only (02b). Extend `discover` grammar and Python parsing.
6. Observation and tracing: copied raw and decoded MKS traffic labelled by the admitted profile; unassociated frames
   stay raw. Passive observers never consume owner RX or block under console pressure. Decoding by code never labels
   an unsolicited frame as a reply.
7. Storage: size request, event, result, stream and cache storage from the actual largest operations (8-byte frames,
   per-address stream tables, pending phases, multi-target member lists). Fixed caller-owned storage; measure RAM,
   stack and image size changes for both firmware frameworks and record them.
8. Package and consumers under contract 02 "Multi-profile consumers and package scope": sources in
   `MOTORCONTROLCAN_SOURCES`, install rules and the `test/package_test.py` `HEADERS` and archive set without vendor
   files; `library.json` and `scripts/check_repository.py` `EXPORTS` stay unchanged. C++11 consumers of core only,
   Lichuan only, MKS only and both; after prompt 65 follow its component layout. Catalogue strings stay optional.
9. Python: an MKS module for the bench client with JSONL `profile` validation, finite scenarios selected by capability
   and evidence parsing; it calls the same console routes and never replays an uncertain command.
10. FieldCore-node read-only: record its current motor consumer boundary and what an MKS integration would need,
    without importing its types or editing it.
11. Startup example: the shared application performs only non-changing reads for an MKS axis at startup (40H, 3AH,
    F1H, readable settings), reports FFH and 9AH power-on motion hazards and the dialect declaration in `caps`, and
    never enables, homes, clears a stall or writes a setting. Its attachment record (02b step 6 power cycle or quiet
    interval; 03 step 8 stream-off declaration; 04 step 4 lost-history reconciliation) comes from the application's
    configuration; without it the startup
    reads report their `possiblyLate` or stream refusal instead of a value, and nothing writes a report stop. Help and
    caps list the never-written commands with the reason.

## Deliver

Extended `MksServoCanConsole`, application records and caches, scan support, Python module, mixed-interface tests in
`test/mks_servo_can_app_test.cpp` and `test/multidrive_contract_test.cpp`, `test/mks_servo_can_scan_test.cpp`, updated
help, caps, CLI contract, API/CLI map, source map, coverage, README scope, CHANGELOG `Unreleased`, backlog and roadmap;
report and PROGRESS row.

## Acceptance

Two interfaces under one application with a Lichuan axis and an MKS axis: independent stops, refused cross-domain
bindings, separate caches and results; profile changes with work pending; wrong-address and unsolicited frames; urgent
stop under pressure; console saturation; exact API/CLI parity. All Arduino and native-IDF firmware targets build with
pinned toolchains; quick and full verification and CI on the exact final pushed commit pass. Flashing is not needed to
prove compilation; if an image reaches an authorized board, run the short existing-profile regression, and never send
MKS frames to the CL86-C bench. Hardware NOT RUN otherwise. Commit and push.
