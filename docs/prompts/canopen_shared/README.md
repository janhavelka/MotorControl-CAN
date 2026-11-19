# Shared prerequisites for additional CAN drive profiles

Prepared 2026-11-19 at `5045423d`; revised the same day at `f3c3870a` after an independent
[audit](audit.md) ([application](audit_application.md)). **No additional profile, seam or
command below is implemented.** Every further drive family gets its own promptset (README,
execution contract, numbered stages, progress). Those sets keep their own evidence, object
policy, units, sequences and qualification. This set holds what they share: the inventory
of reusable code, the intake rules for evidence and tooling, and the contract for plugging a
profile into this repository. It names no family; the families are chosen separately.

The model is MotorControl-RS `docs/prompts/lichuan_shared/` (read-only), adapted to
this repository's actual CANopen services, owner, console and tooling. It complements
[ADD_DRIVE.md](../ADD_DRIVE.md), which remains the playbook for selecting and auditing
a family; this set says how the selected family lands in the code.

| Work | When it applies | Completion evidence |
| --- | --- | --- |
| [01 Existing code and tooling](01_existing_code_and_tooling.md), inventory | Before the family's first code stage. Re-run its refresh step whenever a family stage starts after the main sequence has moved. | Confirmed inventory with recorded deltas and the list of seams the family needs, in the family's stage report |
| 01 intake rules | Every family stage that adds sources, a ledger, generated output or the family's first public header | Pack hashes checked in working tree, INDEX and HEAD; ledger, generator and checker negative tests; header checkers disjoint and complete |
| [02 Profile integration contract](02_profile_integration_contract.md), standing rules | Every family code stage, from the first | The common rules plus the CANopen-only or other-protocol rules for the family's established protocol |
| 02 registration acceptance | Once, at the stage that registers the profile, and at each later major change | Real-owner probe, foreign-profile and identity refusals, multi-profile consumer tests; full matrix and exact-commit CI |

## Seams: claim, land, consume

Authority to implement a seam and permission to consume it are separate.

- **Claim before editing.** Before any shared edit, the implementing lead records a claim
  in the table below (lead, family stage, date), after rechecking this table and the Git
  log. A claimed seam is extended or waited for, never built in parallel. Seams owned by
  a main-sequence prompt (S4, S5, S9) are implemented only by executing that prompt as
  written, never by a family-local substitute.
- **Land.** One focused tested commit; record its commit ID, the interface it tested and
  the consumer gate (the checks a consumer reruns).
- **Consume.** A family consumes a landed artifact whose checks passed, regardless of its
  producing prompt's overall state ([COMMON.md](../COMMON.md) usable-prerequisite rule),
  without re-executing that prompt. If it is missing, only the named dependent
  deliverable is Blocked; independent family work continues and no family code
  substitutes for it.
- **Extend.** A later family adds its entry, row or case and does not build a parallel
  copy. If an extension needs a changed shared contract, it updates the contract, its
  callers and their tests together.

Before S4 lands, registration can deliver the API-level probe, refusals and owner tests,
ledger checkers and package entries; the family's console entries, common-verb dispatch
and API/CLI parity stay Blocked, because every console SDO read goes through the
Lichuan-typed `ProbeApp` (`requestOrdinaryRead`, `LICHUAN_C::ReadTarget`). Before S5, the
owner and adapters are repository builds; package tests cover the core and profile only.

| Seam | State at `5045423d` (S1-S7 and S10 rechecked at `f3c3870a`) | Implemented by | Claim (lead, stage, date) | Landed (commit, tested interface, consumer gate) |
| --- | --- | --- | --- | --- |
| S1 Inventory entry (`Manufacturer`, `DriveProfile`, inventory table) | One hard-coded Lichuan entry in `src/Discovery.cpp`; `LICHUAN_C` is the implicit value 0 | First family to claim it | None | Not landed |
| S2 Profile resolution on a binding (`Endpoint.profile` to `DriveProfile` and protocol) and foreign-profile refusal | No relation; Lichuan preparations accept any nonzero `Endpoint.profile` (Lichuan tests use 41, examples 1) | First family to claim it | None | Not landed |
| S3 Per-binding best-effort stop shape | `SdoClient::validBestEffortStop` accepts only `6040:00=000B` (Lichuan DEC-2) on every channel | First CANopen family to claim it | None | Not landed |
| S4 Common axis table and library session | `AxisSession`, `ProbeApp` and `ProbeMotion` are Lichuan-typed | [Prompt 64](../64_common_axis_interface.md) | Prompt 64 | Pending |
| S5 Optional package components | Owner and adapters compiled from the repository only | [Prompt 65](../65_application_package.md) | Prompt 65 | Pending |
| S6 Console family entry, per-record JSONL profile, runner validation | Envelope and `scripts/console_jsonl.py` fixed to `lichuan_c` | First family to claim it | None | Not landed |
| S7 Per-family ledger, generator, operation checker and verifier registration | Lichuan checker scans every public header | Family stage that adds the first new public header (stage 01 intake rules) | None | Not landed |
| S8 Shared CiA 402 helpers | None; controlword/statusword handling is Lichuan-local | First family whose evidence needs the same semantics as Lichuan | None | Not landed |
| S9 Second protocol on one interface (identifier plan, route, raw transmit) | Owner admits only SDO work (including best-effort stops and drain markers), NMT and SYNC | [Prompt 63](../63_mixed_protocols.md) | Prompt 63 | Pending |
| S10 Reviewed SDO reuse policy per binding | Every channel starts with `SdoReuse()`: marker reads of 1000:00 and 1018:01-03 (Lichuan DEC-1) | First CANopen family to claim it | None | Not landed |
| S11 Host heartbeat producer (owner-produced `0x700+host` frames at an application period, so a drive's heartbeat consumer `1016h` can supervise the host) | None: the owner produces NMT and SYNC only; drive-side host-loss supervision cannot be configured meaningfully without it | First CANopen family to claim it | None | Not landed |
| S12 Owner-admitted RTR frames for node guarding, where a family documents guarding and no heartbeat | None: the owner admits no RTR work | First family whose evidence needs it | None | Not landed |
| S13 SYNC side-effect admission: before the owner produces SYNC on an interface, each bound device's reviewed SYNC effects (synchronous PDO apply, program starts such as MOONS' mode -2) are known, and unknown or harmful effects refuse production | None: prompt 51's producer refuses only on a foreign SYNC producer | First family whose devices act on SYNC; it gates prompt 51's producer without forking it | None | Not landed |

## Rules for every family set

- Follow root [AGENTS.md](../../../AGENTS.md), [COMMON.md](../COMMON.md) and the
  shared contracts it names. The current Git instruction applies: work on `main`,
  no new branches, until version0 is finished. Only the lead commits and pushes.
- Sibling repositories, including MotorControl-RS and FieldCore-node, stay read-only.
- No bench step runs while the [owner-absence rules](../COMMON.md) are in force
  unless those rules allow it; see contract 02, "Bench steps and owner absence".
- These files are work orders. Do not report a seam, API, command or test as
  existing until it is in the code and its checks passed.

Checks for edits to this set: `python scripts/check_promptset.py` and `git diff --check`.
The checker validates links, tables and the prompt index, and verifies the size and
SHA-256 of every artifact in every reference manifest (with `--check-git-ref INDEX` or
`HEAD`, also the Git blobs). It compiles nothing and qualifies no hardware.
