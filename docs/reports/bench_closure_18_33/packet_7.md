# Packet 7: typed TPDO1 event-timer subset

[Packet 7](../../prompts/bench_closure_18_33/7_typed_pdo_subset.md), 2026-11-06. Input: the [packet 6 transition record](packet_6.md#transition-record-for-packet-7). Host work and its review are complete; the bench replay is recorded below once it runs. Back to the [execution index](README.md).

## Start state and sibling audit

Branch `promptset/motorcontrol-can`. Siblings clean and unchanged since the authoring rows: MotorControl-RS `cd27b12`, MB85RC `1a45e88`, RV3032-C7 `1b74184`, FieldCore-node `5ad9292d`.

- **Adopted:** RS `src/profiles/ess_rs/DriverSettings.cpp`: `prepareDriverSettings` (first mutation of caller output only after every check), the `STALE_SETTINGS` prior-state refusal, `advanceDriver` keeping effects and uncertainty and never rolling back, `getDriver` publishing only complete results, and `unresolvedWrite`. MB85RC `WriteResult::bytesAccepted`/`failedChunkOffset`, which shaped `confirmedStep` and `failedStep`. RV3032 `mutationAttempted`, the equivalent of `firstEffectStep`. FieldCore `Rs485MotorModule` `invalidateConfiguration`/`onBusInvalidated`, which never retag retained results.
- **Rejected:** an RS-style no-op skip (a baseline already at the target is refused instead); RV3032's automatic cleanup write (a rollback); MB85RC's bring-up restore; FieldCore ADS1115 automatic re-initialisation.

## Supported transition

`EVENT_TIMER` writes only `1800:05` of TPDO1, from raw 100 to raw 0 or from raw 0 to raw 100, in Pre-operational, on CL86-C. Sequence: nine exact baseline uploads, one acknowledged download (`2B 00 18 05 64 00 00 00` or `2B 00 18 05 00 00 00 00`), nine exact final uploads. It goes through the existing `prepareTpdo1Configuration`, `nextPdoConfiguration`, `notePdoConfigurationTransmission`, `advancePdoConfiguration` and `getTpdo1Configuration`, with a small typed request (`Tpdo1ConfigurationRequest`, `Tpdo1Transition`, `Tpdo1EventTimerRaw {RAW_0, RAW_100}`). The fixed map change is unchanged.

| Checked before any write | Evidence |
| --- | --- |
| COB-ID exactly `0x180+node`, valid, standard, bit 30 clear | EDS `LC_StepDriverCAN_V1.1.eds` line 810 (`$NODEID+0x180`); captures `1311Z` lines 14, 126, 237 |
| Type 255; inhibit raw 100; count 3; words `60410010 60610008 60640020 0` | EDS lines 816-846 and 1038-1070 for type, width and access (its defaults differ: inhibit 0, event 100, other words); captures `1311Z` lines 15-22, 127-129, 238-245; `1317Z` lines 37-44, 79-86 |
| Event value equal to the other literal, request literal 0 or 100 | 0→100: `1316Z` lines 2-10 and 18-26; 100→0: `1302Z` lines 148-161, `1315Z` 2-10, `1317Z` 2-10 and 18-26. 200→0 and 10→0 were measured on node 2 only and are not supported |
| Qualified Pre-operational after the reservation, exact sequential baseline provenance, same node epoch and binding, deadline and budgets, a map representable by the endpoint's profile | Operational writes are measured (`1302Z` line 35, `1311Z` line 336) but not admitted |
| Model CL86-C | 1800 and 1A00 are `eds_family_unbound` in the ledger and every measurement is CL86-C, so OL86-C is refused (`UNSUPPORTED`) before any work. Nodes 3 to 127 follow the EDS identifier rule; only nodes 1 and 2 were read |

The value is a raw U16 with no established unit. Raw 100 gave periods of about 101 ms on the S2's clock; raw 0 sent no periodic frame while stationary, and nothing more is claimed for zero. The map and literal are published only after the ninth matched final read. A context retains `request`, `confirmedStep`, `firstEffectStep` and `failedStep`; nothing rolls back, replays, retries or reuses an unresolved SDO route. A confirmed readback is host map evidence, never a qualified sample: a delayed same-ID, same-length frame stays unqualified.

## Callers

- **Bench:** `E<node>,<0|100>` on the S2 bench image (PONG `pdo=2`), admission line `PDO_CONFIG admitted node=N raw=V`, the record in the existing `PDO` dump with `node`, `transition`, `raw`, `lastStep`, `effectStep` and `failedStep`. `scripts/s2_pdo.py` checks the terminal (19 successful results, the tenth the download, the readback equal to the literal, exactly one non-upload frame) and refuses `E` on a `pdo=1` image. After any `R`, `W` or `E` on a node, the fixed-grant commands (`1`/`2`, `3`/`4`, `h`/`j`, `m`, `v`/`b`) are refused for that node until the next MCU boot.
- **Native:** [`examples/pdo_configuration_preview`](../../../examples/pdo_configuration_preview/README.md) runs raw 0→100 on node 2 through the lifecycle with retained replies (`1311Z` lines 246-272; `1316Z` lines 26, 28, 31), prints each frame and the published literal, and shows two refusals before any write with output unchanged. It is built and run in the test tree and in the relocated-install consumer.

## Harness defect fixed

A confirmed readback could never be installed through the owner, for the map experiment too. `SdoClient::advanceConfiguration` revokes the node's routes, which clears their replacement permission (`adapters/canopennode/Pdo.cpp:40`), and the harness then admitted the readback in the same turn. The harness now withdraws the node's own TPDO1 route again after the migration (`invalidatePdo(slot, true)`, as `SdoClient.h:149-151` documents) and reads back strictly later. The bench configures no PDO route, so its behavior there is unchanged.

## Review

An independent read-only reviewer traced every failure vector, checked each literal against the EDS and captures, and ran mutants (core M1-M10, harness H1-H5, parser, Python S1-S5). Confirmed core: checks before the write, atomic output on refusal, no rollback, replay, retry or early publication. Fixed after review, each with a regression shown failing on its mutant:

- **D1:** no test defended the route withdrawal before the write. With it removed, an uncertain write left the old map re-qualifiable. Now covered for ABORT and for a lost reply.
- **D2:** two runs without an install left the second readback uninstallable. The node's own route is now matched at its installed or an earlier configuration epoch.
- **D3, D4:** the header and scope wording overstated the measured transitions.
- **D5:** the boundary wait was unexercised. A test now has the adapter start at the boundary.
- **R1:** OL86-C is refused.
- **R4:** a refused `E` or `m` restores the node's evidence age policy.

## Checks

| Check | Result |
| --- | --- |
| `pdo_configuration`, `pdo_experiment`, `pdo_command`, `pdo_observations`, `pdo_configuration_preview`, `s2_pdo_contract`, `s2_evidence_contract`, `package_contract`, `repository_contract`, `documentation_contract` | Passed; 7968, 14191 and 102 checks in the first three |
| Strict Doxygen; S2 bench firmware | 0 warnings; built, 0 compiler warnings |
| Staged tree alone, in a detached worktree outside `.build` | `verify.py --mode full --compiler gcc` passed every step, documentation included; S2 bench firmware gate and strict S2 stack check passed |
| Remote CI | Recorded with the commit in the [execution index](README.md) |
| Bench replay | Pending: flash the image with `E`, then replay once from a fresh baseline with readback and a short regression |

Original prompt 18 stays Partial: commissioning (prompt 38), the CLI `pdo` route (prompt 41), other values, Operational writes, units, inhibit, other transmission types, the COB-ID valid bit, SYNC, RPDOs, persistence and sample qualification remain open under Q17.
