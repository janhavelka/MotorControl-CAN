# Shared prerequisites for additional CAN drive profiles

Prepared 2026-11-19 at `5045423d`. **No additional profile, seam or command below is
implemented.** Every further drive family gets its own promptset (README, execution
contract, numbered stages, progress). Those sets keep their own evidence, object
policy, units, sequences and qualification. This set holds what they share: the
inventory of reusable code and the contract for plugging a profile into this
repository. It names no family; the families are chosen separately.

The model is MotorControl-RS `docs/prompts/lichuan_shared/` (read-only), adapted to
this repository's actual CANopen services, owner, console and tooling. It complements
[ADD_DRIVE.md](../ADD_DRIVE.md), which remains the playbook for selecting and auditing
a family; this set says how the selected family lands in the code.

| Work | When to execute | Completion evidence |
| --- | --- | --- |
| [01 Existing code and tooling](01_existing_code_and_tooling.md) | Before the family's first code stage. Re-run its refresh step whenever a family stage starts after the main sequence has moved. | Confirmed inventory with recorded deltas and the list of seams the family needs, in the family's stage report |
| [02 Profile integration contract](02_profile_integration_contract.md) | With the family stage that registers the profile (after its checked ledger and codec). Applies to every later family stage. | Real-owner probe, API/CLI parity, foreign-profile refusal and multi-profile consumer tests; exact-commit CI |

## One implementation per shared seam

The first family that reaches a shared seam implements it once, in a focused tested
commit, and records it in the table below. A later family extends that code (adds
its entry, row or case) and must not build a parallel copy. If an extension needs a
changed shared contract, it updates the contract, its callers and their tests together.

Some seams are already owned by prompts of the main sequence. They are implemented
by executing that prompt as written, never by a family-local substitute. A family
assignment that does not include that prompt leaves the dependent family work
Blocked; independent family work continues.

| Seam | Current state at `5045423d` | Owner | Implemented by |
| --- | --- | --- | --- |
| S1 Inventory entry (`Manufacturer`, `DriveProfile`, inventory table) | One hard-coded Lichuan entry in `src/Discovery.cpp` | First family | Not started |
| S2 Profile identity on a binding (`Endpoint.profile` to `DriveProfile`) and foreign-profile refusal | No relation; Lichuan preparations accept any nonzero `Endpoint.profile` | First family | Not started |
| S3 Per-profile best-effort stop shape | `SdoClient::validBestEffortStop` accepts only `6040:00=000B` (Lichuan DEC-2) | First CANopen family | Not started |
| S4 Common axis table and library session | `AxisSession` and `ProbeMotion` are Lichuan-typed | [Prompt 64](../64_common_axis_interface.md) | Pending |
| S5 Optional package components | Owner and adapters compiled from the repository only | [Prompt 65](../65_application_package.md) | Pending |
| S6 Console family entry, per-record JSONL profile, runner validation | Envelope and `scripts/console_jsonl.py` fixed to `lichuan_c` | First family | Not started |
| S7 Per-family ledger, generator, operation checker and verifier registration | Lichuan checker scans every public header | First family | Not started |
| S8 Shared CiA 402 helpers | None; controlword/statusword handling is Lichuan-local | First family whose evidence needs the same semantics as Lichuan | Not started |
| S9 Second protocol on one interface (identifier plan, route, raw transmit) | Owner admits only SDO work (including best-effort stops and drain markers), NMT and SYNC | [Prompt 63](../63_mixed_protocols.md) | Pending |

## Rules for every family set

- Follow root [AGENTS.md](../../../AGENTS.md), [COMMON.md](../COMMON.md) and the
  shared contracts it names. The current Git instruction applies: work on `main`,
  no new branches, until version0 is finished. Only the lead commits and pushes.
- Sibling repositories, including MotorControl-RS and FieldCore-node, stay read-only.
- No bench step runs while the [owner-absence rules](../COMMON.md) are in force
  unless those rules allow it; see contract 02, "Bench steps and owner absence".
- These files are work orders. Do not report a seam, API, command or test as
  existing until it is in the code and its checks passed.

Checks for edits to this set: `python scripts/check_promptset.py` and
`git diff --check`. They validate links and tables only; they compile nothing.
