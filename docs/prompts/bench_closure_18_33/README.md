# Bench continuation for original prompts 18-33

Prepared 2026-11-06 against MotorControl-CAN 4eb7683. This is a continuation of
promptset v2 on branch `promptset/motorcontrol-can`, not a new execution branch
or replacement for the main 1-50 sequence. These packets have **not been executed**.
They resume specific original responsibilities; completed packets do not make
their original prompts complete while other owned requirements remain open.

Paste one entire numbered file into the coder's chat with this repository open.
Each file applies [COMMON](COMMON.md) and names exact code, evidence and checks.
To assign the whole continuation, paste:

> Read docs/prompts/bench_closure_18_33/README.md and COMMON.md. Execute packets
> 1 through 8 fully, in numeric order among ready independent work. Implement,
> test, independently review, document, commit and sync each packet on the
> existing execution branch. Use bounded subagents. Use the standing bench
> authorization; request only missing physical actions or observations. Preserve
> uncertainty and skipped physical work explicitly. Do not replace implementation
> with another plan or restart completed APIs. Return to earlier work when its
> evidence becomes available.

## Execute these packets

| Packet | Deliverable | Original owner | Dependencies and feasibility |
| --- | --- | --- | --- |
| [1](1_evidence_contracts.md) | Reconcile measured facts, typed coverage and action evidence regressions | 19-22, 29-33 | Host work now; no new live experiment needed |
| [2](2_position_regressions.md) | Missing position/admission tests and one focused duplication fix | 31, 33 | 1; host work now |
| [3](3_s2_shared_firmware.md) | Shared S2 Arduino/IDF builds and transceiver startup contract | 27-28 | 1; host integration now; pinned Arduino UART ownership needs adaptation; S2 smoke test on current bench |
| [4](4_passive_capture.md) | Explicit bounded silent capture through the existing owner | 27-29 | 3's board contract; passive software now, physical silence requires verification |
| [5](5_console_diagnostics.md) | UART/host capture comparison and bounded loss diagnostics | 28-29 | 1; independent of 2-4; root-cause localization needs a second capture path |
| [6](6_pdo_measurement.md) | Review and exercise the existing PDO experiment tooling | 18, 29 | 1; existing S2 bench sufficient; does not wait for 3-5 |
| [7](7_typed_pdo_subset.md) | Extend production PDO configuration only from established transitions | 18 | 6's exact evidence; blocked fields stay blocked, not the whole packet |
| [8](8_motion_stop_campaign.md) | Remaining finite relative/stop cases and integrated handoff | 29, 31-33 | 1-2; existing bench image/path can run independently of 3-7 |

Run one hardware session at a time, including soaks and firmware uploads.
Host workers may run in parallel with that one operator. If later commits have
already closed a packet's exact requirements, verify the delta and record it;
do not manufacture changes or repeat unchanged long tests.

## What can and cannot be closed with this fixture

The active bench is an ESP32-S2, TJA1051, ESP-Prog-2, two CL86-C nodes at
1 Mbit/s and LC60H2127 motors. Successful exchanges and several motion trials
exist. They do not establish every firmware capability.

| Gap | Why it remains | Disposition in this continuation |
| --- | --- | --- |
| Basic typed reads and measured widths | Already implemented; remaining Q20 concerns effects/acceptance, not missing getters | Packet 1 verifies this and fixes stale summaries; no duplicate read layer |
| Direction 2200 / subdivision 2201 | CAN effects, conflicting ranges and scale/counter activation remain unresolved | Preserve precise gates; a later exact experiment also needs independent shaft observation |
| Soft-limit enforcement | 2202 values 0/1 were written/read back; 607D domain/basis and homing-dependent enforcement remain unknown | Retain setter; do not invent limit units or confuse readback with enforcement |
| DO and filters | Terminal/function conflicts, output electrical truth table, filter domain/stimulus | Needs meters, correct DOCOM/pull-ups and selected stimulus/vendor facts; not autonomous with bare CAN connection |
| Tuning | 33 reads and 2500 setter exist; other 32 write domains/effects are unestablished | No invented setters. 2500 trial requires actual motor rating and a phase-current probe |
| Release API | No reviewed release transition; a quick stop reporting release is a different action | No prepareRelease stub/alias. Needs reviewed candidate plus independent shaft/torque observation and reference consequence, or manufacturer evidence |
| Alarm clear | Existing action and bench route; no natural identified fault available | Host coverage now; physical trial waits for a suitable fault, never induced by wiring abuse or a jammed shaft |
| Position/stop software | Concrete tests and small integration gaps remained after the earlier handoff | Packets 1, 2 and 8 close their named scope |
| PDO configuration | Only a fixed TPDO1 transition is in production; experiment tools were just added | Packets 6-7 can investigate and implement an additional supported subset now |
| Wire TX retirement / Q18 | Local queues and CAN ACK do not establish remote-session settlement | Passive tooling helps; independent verified observer and exact controller/server premise still required |
| Console loss | Same-path stress reproduces deletion, but does not locate the missing bytes | Packet 5 builds the comparison; independent GPIO43 UART capture is the next measurement |
| S2/S3/P4 support | S2 bench is not shared framework parity; P4 adapter/firmware remains separate | Packet 3 closes named S2 software. No S3/P4 physical qualification from this board |

Prompts 23-26 already own delivered software. Exercise them as dependencies;
do not reopen them merely because a neighboring firmware fact is unknown.
Prompts 34-50, general commissioning/CLI and release qualification are outside
this assignment. Prompt 18's eventual 38/41 integration remains visible.

## Models and authoring evidence

Use the actual runtime's models under [DELEGATION](../DELEGATION.md): a normal
coding model at high reasoning for 2-5 and settled implementation in 7; stronger
reasoning for 1, 6, 7's device decisions and 8's uncertain outcomes. A selected
Opus lead is also acceptable when exposed by the external coder. Do not invent
model identifiers or require a particular vendor. Use an independent reviewer
for protocol/ownership decisions; only the lead controls Git and hardware.

Read-only source audit for this authoring pass: RS cd27b123 (clean), settings
`DriverSettings.cpp::validatePriorSettingsAndStandstill`, position
`Position.cpp` basis/witness admission, `examples/probe_idf/main/IdfPlatform.cpp`
and `scripts/_bench_evidence.py`. Adopt provenance, finite evidence and separate
outcomes; reject ESS numeric limits, RS device delays and S3-only board defaults.
FieldCore-node 5ad9292d (clean) retains `ordinaryHistory`/`stopHistory` and reference
withdrawal. Named I2C seams were unchanged since archived packets 11-14:
ADS1115 d613a13, INA228 73f9548, BME280 48b8214, PCA9555 414e0f6,
MCP45HVX1 ff8f6c2 and MB85RC 1a45e88, all clean. Preserve candidate publication,
partial-effect evidence and register-versus-physical-output distinctions.
Execution must refresh these only at its relevant seams.

New PDO code landed during the audit as 4eb7683. Untracked editor files and
`docs/bench/20261106-stress-r7-after-outage/` were unrelated work and were left
untouched. Refresh the live revision and dirty paths before each packet.
