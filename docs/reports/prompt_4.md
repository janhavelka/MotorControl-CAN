# Prompt 4 stepper and servo contrast ledger

Date: 2026-10-28. Status: Complete for documentary comparison and test-case
specification. Prerequisite prompts 2 and 3 were complete at starting commit
`afe8f0c`. Work stayed on `promptset/motorcontrol-can`, tracking
`origin/promptset/motorcontrol-can`; the initial fresh fetch showed 0 ahead /
0 behind and a clean worktree. No driver, installed profile, live CAN exchange
or physical qualification was added.

## Delivered boundary and review

[The contrast ledger](../reference/drive_contrasts.md) compares Lichuan C,
Nanotec N5 in stepper and BLDC configurations, and maxon EPOS4 Compact 50/5
CAN. It separates native width/sign, command and feedback units, ramp meaning,
capability/mode, trigger and completion evidence, stop/brake/heartbeat behavior,
identity and configured scale. Its nine test-only case groups give later prompts
6, 9–11 and 46 actual source-backed inputs or explicitly abstract invariants.
They include the N5 one-turn vector, no mandatory full-step geometry, separate
command/encoder/load scales, signed common velocity wider than ESS RPM fields,
float32 quantization, logical subaxis separate from a CANopen node, extended
VESC ID, Classical ODrive RTR, moteus FD rejection, absent acknowledgement and
profile-keyed `2300`/PDO capacity. Cases for 12/13/14/17/23/24/27/45 identify
their later component owners without creating those components now.

The common boundary retains exact intent and endpoint identity independent of
CANopen. CANopen NMT/SDO/PDO/EMCY/CiA402 remain service/profile concepts.
One owner receives each physical interface's frames. Multiple CANopen vendors
can share one service backend; any mixed-protocol physical bus needs explicit
identifier, broadcast and frame-format compatibility analysis. No test-only
vendor appears in installed support.

## Current source delta and independent checks

| Checkout | HEAD at audit | Dirty state | Current seam checked |
| --- | --- | --- | --- |
| `../MotorControl-RS` | `bd831c859daa9c8c07b8d27a2dd879bb4e750511` | clean | `include/MotorControlRS/Units.h`, `Axis.h`, ESS `Velocity.h`; `src/Units.cpp`, `Axis.cpp`; `test/axis_test.cpp`; `examples/probe_cli/AxisConsole.h` and `ProbeApp.cpp`. Exact rational positions, independently named command/full-step/encoder scales and generations are real; `Axis.cpp`/ESS velocity still narrow RPM to I16. |
| `../ADS1115` | `235af3e8c722661a89a44766a4ccd903a654f6fe` | clean | `include/ADS1115/ADS1115.h`, `src/ADS1115.cpp`, `test/test_basic.cpp`: `DeviceProfile` gain/data-rate, verified profile generation, `startApplyProfile` and typed reads depend on actual configuration. This is an ecosystem boundary, not a motor unit or startup recipe. |
| `../INA228` | `b28c5ea69bcb4d26b351c52c24d3c6fae9fd282b` | clean | `include/INA228/INA228.h`, `src/INA228.cpp`, `test/test_basic.cpp`: explicit calibration plan/range, current-derived sample validity and retained job outcomes. Current measurement and motor torque intent stay different contracts. |
| `../FieldCore-node` | `5ad9292dc8481cf08be9a61f662495a6474d1da0` | clean | `include/TunnelMonitor/contracts/MotorControl.h` and `src/devices/rs485_motor/Rs485MotorModule.cpp`: exact target/generation and logical binding feed the RS profile, but `uint16_t speedRpm` and RS address are application-specific, not generic CAN values. Read-only. |

These revisions match the prompt-3 source audit; no sibling drift or dirty-file
behavior changed this stage. Current declarations, implementations and actual
tests/callers were rechecked at the affected type/configuration seam. Unchanged
sibling suites were not rerun. The target verifier remains
`scripts/check_promptset.py`, an offline planning/reference checker; there is
no target CAN build/CI workflow yet.

Separate read-only source reviewers checked the original N5 PDF, official
EPOS4 PDFs and pinned VESC/ODrive/moteus files in parallel; the lead compared
their claims with the retained originals, EDS, rendered-page findings and
current docs. N5 original v3.3.0/FIR-v2213 hash matches the manifest. The
Lichuan EDS still says four Rx/Tx PDO and `2300` factory action; N5 p127 says
eight per direction and p283 calls `2300` NanoJ Control. The lead rechecked
N5 p64's one-turn example and p38's separate stepper/BLDC `3202` values in
the original extract and compared the Lichuan EDS sections directly. The
reviewers rendered N5 pp64–65 and p345 where extraction alone could not settle
the printed inconsistency. maxon's original PDFs were readable through the
manufacturer URL in a browser but not locally downloadable; their edition,
pages and selected object tables were rechecked online. The exact physical
EPOS4 firmware and EDS remain unavailable.

Delegation used three read-only agents on the inherited available model/effort:
N5 original/manual scope, EPOS4 official PDF scope and pinned proprietary
protocol frames. The protocol reviewer then independently challenged the
integrated ledger. All shared documents and Git changes remained lead-owned;
no worker changed a sibling or staged a file.

An independent challenge against the drafted ledger found two concrete fixture
gaps, both corrected before delivery. N5 `6063` feedback now names `3203`
source selection and invalidity when `608F` resolution is zero (manual pp360–361).
The abstract load/command/feedback case now fixes the ratio direction and
expected 3072 command increments versus 1536 feedback counts. No hidden
CANopen state, stepper geometry, narrow RPM, acknowledgement or completion
assumption remained in the other cases.

Two N5 source defects remain explicitly unresolved: p65 reverses `60A9` byte
labels relative to p64's diagram, and p345 uses “SAFEOPERATIONAL” in a
CANopen error description. EPOS4 p224 marks `6076` RW in a table while prose
forbids writing it. These do not establish a common state or writable API.
Newer CL2-86C/CL2-57C public research describes separate products;
none of their geometry, I/O, objects or features enters this contrast.

## Verification, documentation and next work

| Check | Result |
| --- | --- |
| `python scripts/check_promptset.py` | PASS: 50 prompts, 35 requirements, 123 retained hashes, 99 Lichuan indexes / 202 entries / 53 manual items and local links. Planning integrity only. |
| `git diff --check` | PASS: no whitespace errors. |
| `python -c` IEEE binary32 and rational arithmetic spot check | PASS: `1/10` encodes `cdcccc3d`, exact error `1/671088640`; the abstract scale case yields 3072 command increments and 1536 feedback counts. |
| `python scripts/check_promptset.py --check-git-ref INDEX` | PASS after task-owned staging: retained bytes and links. No original artifact or `.gitattributes` changed. |
| `python scripts/check_promptset.py --check-git-ref HEAD` | PASS after commit: retained committed bytes and links. |

Affected documentation: contrast ledger, reference and main indexes, source
map, backlog, coverage, roadmap, progress, CHANGELOG and this report. No C++
declaration, example, generated file, Doxygen input, production test or
firmware build was changed. The fixture cases are specifications for later
tests against actual common code, not executable host tests today. No remote CI
dispatch applies to this documentation-only stage; no CAN hardware was used.
The new document introduces no runtime module, storage, dependency or profile;
one table and one fixture table carry the small design contract.

Next: [prompt 5](../prompts/5_stack_decision.md) evaluates an actual CANopen
service stack against this boundary. Prompts 6, 9–11 and 46 implement and test
the assigned common/frame/consumer cases. Vendor-specific permissions, actual
model/firmware, motor/feedback configuration, stop/brake/host-loss behavior and
physical completion require their own later evidence; the contrast ledger does
not close those gates. The delivery commit ID and final fresh-fetch sync result
are recorded in the handoff, since this report is part of that commit.
