# Bench continuation 18-33: execution index

Execution of the [bench continuation](../../prompts/bench_closure_18_33/README.md) on branch `promptset/motorcontrol-can`, assigned on 2026-11-06 by the user pasting packets 1 to 8. Starting revision `89a7eea`; the untracked `.vscode/` editor files belong to the user and stay untouched. Packet completion, original-prompt completion and hardware qualification are separate columns: a finished packet completes only its own scope.

## Packets

| Packet | Report | Packet status | Original prompts | Hardware |
| --- | --- | --- | --- | --- |
| [1](../../prompts/bench_closure_18_33/1_evidence_contracts.md) Evidence contracts | [packet_1](packet_1.md) | Complete (`eccba4b`) | 19-22, 29-33 stay Partial | None: host work |
| [2](../../prompts/bench_closure_18_33/2_position_regressions.md) Position regressions | [packet_2](packet_2.md) | Complete (`5a664f1`) | 31, 33 stay Partial | None: host work |
| [3](../../prompts/bench_closure_18_33/3_s2_shared_firmware.md) Shared S2 firmware | [packet_3](packet_3.md) | Complete (`4df97bc`; checker fix `f28035b`; smoke on `3353ed2`) | 27, 28 stay Partial | Smoke check of the shared OPTAK image passed 2026-11-07; GPIO18 and TXD levels across reset unmeasured |
| [4](../../prompts/bench_closure_18_33/4_passive_capture.md) Passive capture | [packet_4](packet_4.md) | Complete (`1805a77`, smoke on that image) | 27-29 stay Partial | Capture smoke ran 2026-11-06 (20 heartbeats, nothing submitted); physical silence, no-ACK and Q18 need an independent observer |
| [5](../../prompts/bench_closure_18_33/5_console_diagnostics.md) Console diagnostics | [packet_5](packet_5.md) | Complete (`e8b449d`, `27903cb`; soak fix regression-tested on `f2c90ab`) | 28, 29 | Ran 2026-11-06: loss reproduced at 20 ms and placed after UART FIFO acceptance; attribution needs a GPIO43 capture |
| [6](../../prompts/bench_closure_18_33/6_pdo_measurement.md) PDO measurement | [packet_6](packet_6.md) | Complete, with recorded deviations; runner fixes P1-P5 | 18, 29 | Ran 2026-11-06, both specimens ([report](../2026-11-06_pdo_stream.md)); NMT boundary rerun on `f2c90ab` ([record](packet_6.md#nmt-boundary-rerun-2026-11-06-1616z)) |
| [7](../../prompts/bench_closure_18_33/7_typed_pdo_subset.md) Typed PDO subset | [packet_7](packet_7.md) | Complete (`f2c90ab`; replay record in the next commit) | 18 stays Partial | Replayed once on node 2 in both directions on `f2c90ab` |
| [8](../../prompts/bench_closure_18_33/8_motion_stop_campaign.md) Motion and stop campaign | [packet_8](packet_8.md) | Host part done (tests 28-30, `testPacket8Plan`, runner deadline check, [procedure](packet_8_procedure.md)); bench items 2-4 pending | 29, 31-33 stay Partial | Start baseline read 2026-11-07; motion needs a drive power cycle and an attendant's shaft observation |

## Remote CI

| Commit | Content | Result |
| --- | --- | --- |
| `eccba4b` | Packet 1 | Passed |
| `311b682` | Packet 6 | Passed |
| `e8b449d` | Packet 5, part 1 | Passed |
| `5a664f1` | Packet 2 | Passed |
| `27903cb` | Packet 5, part 2 | Passed |
| `4df97bc`, `f2c90ab`, `5690ecf` | Packets 3 and 7, bench round record | **Failed**, ESP-IDF job only: packet 3's exact checkout-input check counted the SDK, which CI installs inside the checkout (`.build/esp-idf`); local gates, with the SDK outside, passed |
| `980982a` | First checker fix | **Failed**: the shared images passed; the isolated component gate's forbidden-input check still counted the SDK |
| `f28035b` | Second checker fix | Passed, every job |
| `a6e002d` | CI record | Passed |
| `1805a77`, `1abb6aa` | Packet 4 and its smoke record | Passed |

## Originals: what exists and what is missing

Each row names existing public behavior, then what is missing by kind and who owns it. Prompts 23-26 are delivered and are exercised only as dependencies. A "Partial" status is not itself a missing implementation.

| Original | Existing public behavior | Missing software | Missing device fact | Missing instrument or observation | Owner |
| --- | --- | --- | --- | --- | --- |
| 18 PDO remapping | Fixed TPDO1 map 3 to map 2 lifecycle and the `EVENT_TIMER` `1800:05` raw 0/100 transition (`prepareTpdo1Configuration`, `nextPdoConfiguration`, `notePdoConfigurationTransmission`, `advancePdoConfiguration`, `getTpdo1Configuration`; packet 7); prompt 17's map validation and decoding; bench PDO tools (`N`, `P`, `R`, `W`, `E`) | Broader remapping; commissioning and CLI integration | Inhibit semantics, types other than 255, change-driven sending, valid-bit and persistence behavior, RPDO effects | Hardware-timestamping listen-only observer for exact periods and retirement; a moving source for change-driven tests | Packets 6, 7; prompts 38, 41 |
| 19 Typed reads | Typed read groups, `IdentityWidths::MEASURED_U8`, `CountWidths::MEASURED_U16` for `607D:00`/`60FE:00` | None found (packet 1 verifies) | Q20 read effects for rows outside the DEC-1 accepted list | Manufacturer statement or an observed-effect experiment | Q20 gate; packet 1 |
| 20 Driver settings | Typed reads; `2202` codes 0 and 1 with readback, run on both specimens (trials 604-607) | `2200`, `2201`, `607D`, `6060` setters, deliberately gated | `2202` enforcement, `2201` range and effect, `2200` meaning, `607D` domain and basis | Independent shaft observation; travel protection for a limit test | Later prompts or manufacturer evidence |
| 21 I/O settings | Typed I/O reads; DI function and polarity writes, run on node 1 (trial 403); full `60FE` readback (608, 609) | DO and filter setters, deliberately gated | DO address and function conflict, output truth table, filter domain | Meters on DO0..2 with DOCOM at 0 V; a filter stimulus | Handoff packet 13 P4 (operator) |
| 22 Tuning | 33 typed reads (2502..2520 read on both specimens); `2500` setter, host-tested | Setters for the other 32 rows, deliberately gated | Domains and effects of every tuning row | Phase-current probe and motor rating for `2500` | Handoff packet 14 T1/T2 |
| 27 TWAI adapter | Adapter, SDK evidence, bounded receive times | Explicit passive capture mode | TWAI questions 1, 4, 6; physical TX retirement (Q18) | Verified independent listen-only observer | Packets 3, 4; prompt 47 |
| 28 Firmware examples | S3 firmware; S2 special bench application; shared S2 IDF builds, target pin policy, application-owned silent control (packet 3) | Arduino S2: blocked by the pinned package's UART0 console | None | The S2 smoke check; GPIO18 and TXD levels across reset | Packet 3 |
| 29 Device evidence | Typed and raw collectors, bench routes, soaks, PDO sessions, console-loss characterization | Independent console capture comparison; passive capture | Where console bytes are lost | Independent UART capture at GPIO43 | Packets 4, 5, 6, 8 |
| 30 Enable and release | Gated enable, run on both specimens; documentary decoder | Release API: no reviewed transition | Release transition and its consequences | Independent shaft or torque observation | Outside this continuation |
| 31 Stop operations | HALT and quick stop operations; quick-stop standstill rule; run from `000F`, `010F`, `001F`, moving and no-predecessor states | None found | Stops from an acknowledged `005F`; velocity and homing stops | None for drive-report cases | Packet 8 (005F); prompts 35, 36 |
| 32 Fault and reference | Alarm clear and counter zero through `AxisSession`; counter zero ran once on node 2 | None found | An alarm clear on a naturally present alarm | A naturally present alarm | Waits for a natural alarm; packet 8 for counter cases only if justified |
| 33 Finite position | Absolute and relative lifecycles; node 1 relative base `ACTUAL`; moves run on both nodes; `validateMoveAdmission`; the drive-fault fence (`faultBlocked`) | None found after packet 2; a fault reported after the move ended sets no fence | Node 2's relative base; a negative relative move; stops from `005F` | None for drive-report cases | Packets 2, 8 |

## Sibling audit

Recorded by the continuation's authoring pass and unchanged at execution start (HEAD and clean state checked 2026-11-06): MotorControl-RS `cd27b12`, FieldCore-node `5ad9292d`, ADS1115 `d613a13`, INA228 `73f9548`, BME280 `48b8214`, MB85RC `1a45e88`; RV3032-C7 `1b74184`, all clean. Each packet report names the seams it read and the behavior it adopted or rejected.
