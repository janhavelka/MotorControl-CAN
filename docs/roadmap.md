# Implementation roadmap

The [progress table](prompts/PROGRESS.md) owns each numbered prompt's current
state and report. This roadmap lists the next work and its dependencies;
dated implementation and bench history stays in those reports.

[Prompt54](reports/prompt_54.md) records the current qualification campaign and
its bounded diagnostics. Node1's uncertain stop needs an externally established
fresh server session before further SDO or motion. Independent raw SYNC traffic
measurement preserves that quarantine. Q27 sample provenance, M09/Q28 shared
RPDO semantics and the missing wired-stop fixture remain separate obligations.

## Changes during motion

Prompt52 implements the finite change paths and explicit measurement harness.
Its source/physical disposition is in [report52](reports/prompt_52.md). Prompt52
remains Partial: initial Q26 ramp-change meaning is unmeasured; bare ramp writes
do not establish the effect of changing ramps with a new set-point edge. Prompt54
owns finished-operation qualification, including sustained/multi-axis behavior;
unknown Q22-Q26 facts remain precise gates. No RPDO change delivery is implied.

## Software candidate

Prompt53 delivers concurrent application attachments, independent stops, a group
stop helper and finite paired runner. [Report53](reports/prompt_53.md) separates
native/build verification from prompt54's physical trials. Shared quick-stop RPDO
and synchronous start still require M09/Q28; host processing-time skew is not wire
or mechanical synchronization. Prompt56 owns the expiry-scan optimization.

[Prompt49](reports/prompt_49.md) delivers user/package corrections and A-17
descriptor cleanup. [Prompt50](reports/prompt_50.md) records the independent
contract review, literal README-shell correction, evidence reconciliation and
final verification. It assesses a scoped software candidate while inherited
software omissions, firmware and physical gates remain visible.

The installed C++11 core, conditional Lichuan operations, optional CANopen
owner/adapters, shared CLI and bounded Python runners are implemented to the
scope in [candidate notes](releases/0.1.0.md). Source/installed consumers and
production multi-drive contrasts are recorded in [prompt46](reports/prompt_46.md).
N5/EPOS4 fixtures do not constitute additional installed profiles.

[Prompt51](reports/prompt_51.md) adds the finite host SYNC producer and conditional
TPDO1 transmission-type setting through the same owner and console. It remains
Partial: Q27 same-cycle sample evidence, Q28 remote RPDO buffering/withdrawal,
M09 map artifacts and prompt56 throughput for the ten-minute two-axis 10 ms trial
are still required. The [SYNC guide](sync.md) defines the implemented boundary.

## Retained earlier work

| Work | Current boundary / next requirement |
| --- | --- |
| PDO remapping, prompt18 | Narrow TPDO1 subset implemented and specimen-observed. Q17 general remapping/provenance gates remain; see [services](canopen_services.md). |
| Reads/settings/actions, prompts19-22 and30-33 | Conditional typed operations exist. Remaining standalone read rows, unsupported mutations and source/firmware questions stay with their original owners in [coverage](coverage.md) and [backlog](backlog.md). |
| Velocity/homing, prompts35-36 | Conditional lifecycles and host regressions exist. Exact startup/reverse, wired method inputs, direction/offset/reference and physical outcomes need the observations in [velocity](velocity.md) and [homing](homing.md). |
| Communication/persistence, prompts38-39 | Typed reads and narrow reviewed workflows exist. Route activation, complete saved scope, restart/rearm and failure effects retain their device gates in [commissioning](commissioning.md). |
| Platforms, prompts27-28 | S3 Arduino/IDF and S2 IDF builds exist. Arduino S2, complete P4 application and electrical/controller qualification remain distinct work in [firmware](firmware_examples.md). |
| Physical campaigns, prompts47-48 | Both remain Partial. [Finite](reports/prompt_47.md) and [load/failure](reports/prompt_48.md) reports preserve observed scope, failed captures and exact missing measurements. The serial diagnostic loss remains unlocalized. |
| Additional drive support | Follow [ADD_DRIVE](prompts/ADD_DRIVE.md) with model-applicable evidence, real typed implementation, consumers and qualification. A survey is not support. |

Use [device conflicts](device_conflicts.md) and the current
[bench record](hardware_bench.md) before a resumed acquisition. An old session
does not establish that today's fixture is powered, wired or in the same state.
No evidence gap is resolved by a raw accessor, guessed native sequence or fake.

## Later feature assignments

The [promptset index](prompts/README.md) contains the executable assignments.
Prompts51-72 remain separate from the prompt50 candidate audit. Prompt51 has
delivered the host producer and typed TPDO1 experiment; its qualification work
remains open. Prompt52 delivers gated changes and initial measurements; prompt54 owns their
finished-operation qualification. Prompt53 delivers concurrent
application axes/group stop; 55 owns communication recovery/retry policy.
Configurable adapter capacities and application coordination are separate
contracts, both tested in prompt53. User-requested features and measurements
remain linked from backlog F/M/B rows, with their original ownership.

No new driver, reset policy, safety function or physical qualification is implied
by correcting documentation. Uncertain commands are never automatically replayed.

## Execution provenance

Promptset version2 started on `promptset/motorcontrol-can`, base `7878f7c`.
After prompt42, the user authorized integration and continued work on `main`
without new branches until version0 is finished. Follow [AGENTS](../AGENTS.md)
for explicit staging, checked commit/sync and final-commit CI. This work does not
authorize release tags or package publication.

Historical audit and implementation reports remain linked from
[progress](prompts/PROGRESS.md) and [the documentation index](README.md).
The former stage numbering is retained in [SPLIT_MAP](prompts/SPLIT_MAP.md);
do not restore its old catch-all assignments.
