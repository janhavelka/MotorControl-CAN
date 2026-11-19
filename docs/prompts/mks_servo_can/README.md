# MKS SERVO42D/57D CAN profile promptset

Prepared 2026-11-19. **Source collection and prompt preparation complete. All implementation stages are NOT STARTED;
MKS SERVO42D/57D CAN hardware is NOT TESTED.**

Add the Makerbase MKS SERVO42D_CAN and SERVO57D_CAN closed-loop stepper drivers as the library's first
**proprietary (non-CANopen) protocol backend**, beneath the common motion vocabulary. The protocol is Makerbase's own
command/response format on classical 11-bit CAN data frames: no NMT, SDO, PDO, SYNC, EMCY, heartbeat, LSS, object
dictionary or CiA 402 state ([review](../../reference/mks_servo_can/review.md) section 1). The profile never fabricates
those services. Start from the [reference pack](../../reference/mks_servo_can/README.md), the
[engineering review](../../reference/mks_servo_can/review.md) with its discrepancies **MK-D01..MK-D33**, and the
[execution contract](execution_contract.md); they are required context for every stage. Dispatch one stage at a time
and carry its handoff into the next. Creating this promptset does not execute it.

Prepared against MotorControl-CAN `f3c3870a` (the CANopen principles pack and the family-selection audit were in the
working tree, uncommitted) and the MotorControl-RS model `docs/prompts/lichuan_lc57bk_rs485/` at `eb0dbad` (dirty).
Reconciled the same day with the revised [canopen_shared](../canopen_shared/README.md) contract at `b5f44b64` and its
[audit application](../canopen_shared/audit_application.md); see "Relationship to the main sequence and shared seams".

## Scope

- **Products:** SERVO42D_CAN (NEMA17, 40H board code 2) and SERVO57D_CAN (NEMA23, board code 4) with a 1.8° motor
  (MK109 p.10, p.38, p.115). The manual's board revisions (42D V1.1, 57D V1.3) and the retained schematics
  (42D_CAN V1.0_003, 57D_CAN V1.1_001) differ (MK-D24); revision is a recorded fact, not a support claim.
- **Bus control only:** motion through the work modes SR_OPEN, SR_CLOSE and SR_vFOC (MK109 p.23, p.78). The pulse
  modes CR_* are settings states; STEP/DIR/EN motion is outside the library.
- **Dialect tables:** three separate tables, each selected by an explicit declaration and never from a version label
  alone: V1.0.9 edition of Sep-2026 (MK109), V1.0.9 edition of Nov-2025 (MK109A) and V1.0.0 (MK100). The two V1.0.9
  manuals document different command sets under the same label (MK-D01).
- **Ledger rows only:** V1.0.6 (MK106) layouts are recorded for comparison; firmware V1.0.1-V1.0.5, V1.0.7 and V1.0.8
  have no retained CAN manual (MK-D33). These are unsupported dialects until a stage adds a table from evidence.
- **Excluded:** the RS485 boards and their 0xFA/0xFB frames (also printed inside the CAN manuals, MK-D26), the Modbus
  manual, SERVO57D_BUS (relation unexplained), SERVO28D/35D CAN (incidental rows only, MK-D33), IAP firmware upgrade
  (50H) and the vendor PC tool, extended identifiers, RTR and CAN FD (none documented, MK109 p.30).

## Stages

| Stage | Deliverable | Dependencies and gates |
| --- | --- | --- |
| 01 | [Evidence and command ledger](01_evidence_and_command_ledger.md) | Pack and canopen_shared 01 inventory; no hardware |
| 02 | [Bounded codecs and dialect tables](02_codecs_and_dialects.md) | 01 ledger and checkers; host-only |
| 02b | [Protocol domain and owner admission](02b_protocol_domain_and_owner.md) | 02; landed and passed artifacts of main-sequence [prompt 63](../63_mixed_protocols.md) (seam S9) for owner admission; seams S1, S2, S6 claimed or extended; registration acceptance |
| 03 | [Reads, identity and dialect selection](03_reads_identity_and_dialect.md) | 02b; MK-D01, D05, D10, D30 gate interpretations |
| 04 | [Enable, stop and unknown completion](04_enable_stop_and_uncertainty.md) | 02b, 03; MK-D13, D17, D36 |
| 05 | [Finite position](05_finite_position.md) | 03, 04; MK-D02, D14, D19, D29 |
| 06 | [Speed mode](06_speed_mode.md) | 04, 05 unit helpers; MK-D02, D14, D15 |
| 07 | [Homing and limits](07_homing_and_limits.md) | 03-05; MK-D16, D19, D25; [prompt 77](../77_switch_ready_homing_limits.md) vocabulary |
| 08 | [Settings and dangerous persistent settings](08_settings_and_persistence.md) | 03; MK-D10, D18, D21, D23, D27, D29 |
| 09 | [Asynchronous reports and reply matching](09_async_reports_and_reply_matching.md) | 02b matcher, 04-05 phases; MK-D11, D12 |
| 10 | [Bus settings, restart and factory restore](10_bus_settings_and_restart.md) | 03, 08, 09; MK-D08, D20, D22 |
| 11 | [Console and multi-protocol integration](11_console_and_integration.md) | Implemented subsets of 02b-10; prompts 64, 65 |
| 12A | [First-contact questions](12a_first_contact.md) | Identified MKS fixture and authorization; owner present |
| 12B | [Qualification and release reconciliation](12_qualification_and_release.md) | Software closeout any time; HIL needs 12A's fixture |

Dependencies are semantic prerequisites, not a demand to guess missing behavior or leave independent software idle. A
stage may finish a usable subset while named operations stay gated. Codec, ledger and host-test work (01, 02, parts of
03-10) proceeds without prompt 63; everything that sends a frame through the owner waits for prompt 63's artifacts to
land and pass, and every console route and API/CLI parity check waits for seam S4. Implementation, host tests,
firmware builds and physical qualification are separate columns, and every documented native command stays in the
coverage denominator.

## Prepared artifacts

- Seven originals with SHA-256 manifest ([SOURCES.json](../../reference/mks_servo_can/SOURCES.json)): four CAN manuals
  (MK109 118 pages, MK109A 106, MK106 60, MK100 43), the repository version notes (MKNOTES) and two schematics; six
  PyMuPDF extracts with one-based page markers. Extracts are not authoritative for tables, figures or screenshots.
- The review's command inventory (section 4), reply phases (5), identity and state map (6), units (7), firmware
  differences (13) and the seventeen "must not assume" rules (14). The [CANopen principles pack](../../reference/canopen_principles/principles.md)
  section 2 supplies the predefined CANopen identifiers that make bus sharing unsafe.

These are research inputs, not implemented tables or proof of any unit's firmware.

## Preparation findings for stage 01

Checked against original pages while writing this set; stage 01 enters them into the ledger with the next free IDs.

| Proposed ID | Finding | Evidence | Resolving stage |
| --- | --- | --- | --- |
| MK-D34 | The 00H "not readable" reply `[code, FF, FF]` has the same shape as a readable two-byte value 0xFFFF | MK109 p.37 | 03, 08, 12A |
| MK-D35 | A request and its status reply can be byte-identical on the same ID: `F3 01 F5` is both the enable request and its success reply to ID 1; any one-byte write of 0 or 1 equals the fail or success reply (inference from the layout) | MK109 p.30, p.81 | 02b |
| MK-D36 | Run and stop commands of one mode share the function code and status values 1 and 2, so "run complete" and "stop complete" are indistinguishable | MK109 p.83-84, p.89-90, p.93-94 | 04, 09 |
| MK-D37 | Single-turn homing needs the 9AH zero point, which the menu allows only with 0_Mode not Disable, which arms homing at every power-on (inference from the menu rule) | MK109 p.27, p.65, p.69 | 07 |

## Preparation verification

Run `python scripts/check_promptset.py` and `git diff --check` for this documentation bundle; they check links and
table shapes, verify every manifest artifact's size and SHA-256, and compile nothing. The pack's `SOURCES.json` is
discovered by the checker's `docs/reference/*/SOURCES.json` glob, so `reference_manifests` is not edited
(canopen_shared 01 "Intake rules", pack). No product source, firmware, CI or hardware changed or ran for this
preparation.

## Relationship to the main sequence and shared seams

The [canopen_shared set](../canopen_shared/README.md) at `b5f44b64` is binding. This family uses its
[01 inventory](../canopen_shared/01_existing_code_and_tooling.md) section "A non-CANopen CAN protocol instead" and
"Intake rules" (pack, other-protocol ledger, operation map and tools, seam S7), and its
[02 contract](../canopen_shared/02_profile_integration_contract.md) "Common rules" plus "Other-protocol rules". Its
"CANopen-only rules" (`SdoClient`, seams S3, S8 and S10) do not apply. Seams follow the shared README's "Seams: claim,
land, consume": the implementing lead claims S1, S2, S6 or S7 in that table (lead, stage, date) after rechecking it
and the Git log, extends a seam another family claimed or landed instead of building a parallel copy, and records the
landed commit, tested interface and consumer gate. S4, S5 and S9 are consumed as landed artifacts whose checks passed,
whatever their prompt's overall state; a missing one blocks only the named dependent deliverable.

| Seam | This family | Note |
| --- | --- | --- |
| S1 inventory entry | Claim or extend at 02b | `Manufacturer::MAKERBASE`, `DriveProfile::MKS_SERVO_CAN` with the next unused explicit nonzero value, `Protocol::PROPRIETARY` |
| S2 profile resolution and foreign refusal | Claim or extend at 02b | `Endpoint.profile` holds the `DriveProfile` value; the one `Discovery.h` lookup also checks `Endpoint.protocol`; if MKS lands S2 first, its commit carries the Lichuan migration (contract 02 "Profile resolution") |
| S3 best-effort stop shape | Not applicable | Opt-in per CANopen binding; an MKS binding has no SDO channel or shape. MKS stops are 04's native frames |
| S4 common axis table | Consume prompt 64 at 11 | No second session; console routes and API/CLI parity Blocked until it lands |
| S5 package components | Consume prompt 65 at 11 | Vendor material never packaged |
| S6 console entry and per-record JSONL profile | Claim or extend at 02b; entry Blocked until S4 | `profile` is `"mks_servo_can"`, `null` or the admission value per record type (contract 02 "Console, help and JSONL") |
| S7 ledger, generator, checker split, registration | Claim or extend at 01, before the first MKS public header (02) | Other-protocol command ledger of canopen_shared 01 "Intake rules"; no EDS, PDO or object fields |
| S8 CiA 402 helpers | Not applicable | No controlword or statusword |
| S9 second protocol, route, raw transmit | Consume prompt 63's artifacts at 02b | Blocks owner admission only; MKS needs go to prompt 63's owner (open question 1) |
| S10 SDO reuse policy | Not applicable | No SDO route; `SdoReuse` is excluded by contract 02 "Other-protocol rules" |

Points of the earlier contract that the revision settles, and where this set now follows it:

| Earlier point | Revised section | Followed in |
| --- | --- | --- |
| Every route through `SdoClient` | 02 "CANopen-only rules" versus "Other-protocol rules": own native service through the real owner after S9, never SDO | [Execution contract](execution_contract.md), [02b](02b_protocol_domain_and_owner.md) |
| Object ledger with EDS fields | 01 "Intake rules", other-protocol ledger: same accounting, no EDS, PDO or object fields | [01](01_evidence_and_command_ledger.md) step 3 |
| Pack registration in `reference_manifests` | 01 "Intake rules", pack: discovered by glob, never edited | [01](01_evidence_and_command_ledger.md) step 2 |
| Prompt 63 as a prerequisite prompt | Shared README "Consume" and 02 "Other-protocol rules": its landed and passed artifacts | [02b](02b_protocol_domain_and_owner.md) |
| Firmware gating versus "report identity, do not lock" | 02 "Identity and device evidence": product or interface identity, firmware only where the ledger says so | [Execution contract](execution_contract.md), [03](03_reads_identity_and_dialect.md) step 5 |

## Open questions against the shared contract

1. **Prompt 63 request (S9).** [Prompt 63](../63_mixed_protocols.md) plans a second protocol beside CANopen: plans
   are checked against configured CANopen identifiers and restricted ranges, and its raw kind has one admission class
   below stops with local TX evidence only (Implement 1 and 3). MKS needs a whole 11-bit plan on an interface with no
   CANopen node, an urgent class for MKS stops above MKS ordinary requests, and per-frame TX evidence (sent, not sent,
   possibly sent) for its own reply correlation. Stage 02b records these in its report as a request to prompt 63's
   owner (the lead executing it, or the user). Until the landed artifacts provide them, MKS owner admission stays
   Blocked; no family-local owner path, weakened check or second admission substitutes (shared README "Claim before
   editing").
2. **Tool names (01 "Intake rules", operation map and tools).** The rule names `generate_<family>_objects.py` with
   `Objects.h` and `Pdo.h`. A command ledger has neither; this set plans `generate_mks_servo_can_commands.py` and a
   private `Commands.h`. Stage 01 confirms the names with the shared set's owner or renames before the first commit.
3. **Identity without an identity read (02 "Identity and device evidence").** The contract refuses dependent work
   on missing or stale identity and lets the label and `Capabilities` grant nothing. `V1_0_0` has no documented 40H:
   whether a caller's `DialectDeclaration` may stand as its consumed identity is open, and until it is answered
   `V1_0_0` bindings admit the probe and reads only. For every dialect, the protocol reports no boot-up (review
   section 14 rule 10), so the holder's server session can end only on host-observed events (restart or bus-off
   notification, rebinding, 41H, 3FH, a confirmed ID or bitrate change); an uncommanded drive power cycle leaves the
   identity stale without detection. Stage 03 records this limit; the shared set's owner decides whether it is accepted.
4. **Console before S4 (inherited, [audit application](../canopen_shared/audit_application.md) question 2).**
   Whether a family read may extend the Lichuan-typed `ProbeApp` before prompt 64 is a decision for the prompt 64 lead
   or the user. Until S4 lands, or that decision allows an earlier extension, every `mks` console route and API/CLI
   parity check is Blocked (contract 02 "Console, help and JSONL"); API, application and owner tests proceed.

Main-sequence prompts 55 and 74 (per-protocol retry classes), 57 (enable and identity policy), 73 (per-protocol
timeouts), 75 (runtime node table) and 77 (limit and origin vocabulary) are consumed where the stages name them, never
replaced by a family-local copy.
