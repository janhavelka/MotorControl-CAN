# Implementation roadmap

Execution identity: version 2, started 2026-10-28 on
`promptset/motorcontrol-can`, base `7878f7c`. Keep this branch for the complete
promptset and resumed sessions. [Prompt 1](reports/prompt_1.md) and
[prompt 2](reports/prompt_2.md), [prompt 3](reports/prompt_3.md) and
[prompt 4](reports/prompt_4.md), [prompt 5](reports/prompt_5.md) and
[prompt 6](reports/prompt_6.md), [prompt 7](reports/prompt_7.md) and
[prompt 8](reports/prompt_8.md) and [prompt 9](reports/prompt_9.md) are complete.
[Prompt 10](reports/prompt_10.md) is complete with pure axis targets, offline
preview and passing local/Windows/GCC/Clang sanitizer CI.
[Prompt 11](reports/prompt_11.md) is complete with pure frame/SDO checks and passing
local full plus Windows/GCC/Clang sanitizer CI. [Prompt 12](reports/prompt_12.md)
is complete with reviewed generation and passing local/Windows/GCC/Clang
sanitizer CI. [Prompt 13](reports/prompt_13.md) completes the bounded selected-stack
SDO lifecycle with passing local and Windows/GCC/Clang sanitizer CI.
[Prompt 14](reports/prompt_14.md) completes ambiguity settlement, retained late observations and permanent quarantine with passing local/Windows/GCC/Clang sanitizer CI.
[Prompt 15](reports/prompt_15.md) completes passive network events and shared lifecycle invalidation.
[Prompt 16](reports/prompt_16.md) completes explicit NMT and service eligibility.
[Prompt 17](reports/prompt_17.md) completes checked PDO map/sample observations.
[Prompt 18](reports/prompt_18.md) is Partial: fixed TPDO1 configuration is
implemented, host-tested and observed on node1; broader Q17 gates remain.
[Prompt 19](reports/prompt_19.md) is Partial: all 30 assigned raw read rows are
implemented. Packet 4 admits the DEC-1 accepted list and `prepareProbe` as a
project decision; Q20 read effects stay open, and every other row stays gated.

**Handoff state, 2026-11-03.** Packets 1 to 22 have run their host scope. The [execution table](reports/handoff_19_33/README.md)
records each packet's delivery, checks and CI. Packet 10 closed the repair
audit's W1 to W8 for host scope. Packet 11 ran the first live typed reads and a
read-only communication soak on two specimens. A first
[attended campaign](reports/2026-11-03_attended_bench.md) enabled both
specimens, and on node 1 ran one absolute experiment move, its rearm and a
moving HALT. An [unattended campaign](reports/2026-11-03_unattended_bench.md), on
the user's written waiver, ran the library's move lifecycle once on node 2 and a
stationary `000B` on each node, which released the motor; the quick stop now
completes on that report. On 2026-11-04 ([bench report](reports/2026-11-04_bench.md)) that
quick stop completed on hardware, a moving quick stop completed on standstill after two library
fixes, the relative base measured `ACTUAL` and the library's relative move succeeded on node 1,
DI3's function and polarity writes were accepted, and a counter zero zeroed `6064` on node 2. No
specimen has cleared an alarm; both accepted `2202` codes 1 and 0 once a measured count width let `607D:00` through. The open
physical gates are listed in the [backlog](backlog.md).

The [NMT/TPDO checkpoint](reports/2026-11-02_nmt_pdo_checkpoint.md) retains
26 uploads per node, node1 heartbeat1000, fresh addressed Pre-operational and
complete fixed-map readback. No motor action, start/reset, save or restore was
sent. One physical power cycle subsequently reverted node1 producer/map to
0/map3 without save/restore; both nodes completed26 baseline plus30 readiness
uploads. Reads of 607A/60FF equal to EDS defaults, origin unknown, mode0 and configured DI functions require
further motion-admission evidence. Timer/SYNC, broader persistence, sample
qualification and executable finite motion remain pending.

The [sequence manifest](prompts/SEQUENCE.json) owns exact dependencies;
[PROGRESS](prompts/PROGRESS.md) owns stage status. This roadmap gives executable
milestones, not another dependency graph. Execute numeric order among ready
work, commit/sync each checked prompt or useful complete chunk, and preserve
blocked obligations under [COMMON](prompts/COMMON.md).

| Prompts | Deliverable and boundary | Current state / next acceptance |
| --- | --- | --- |
| 1 | Current source map, live delta, tools, bench facts and backlog | Complete; evidence and checks in its report |
| 2-4 | Full object/source intake, reviewed Lichuan semantics, N5/EPOS4 contrasts | Complete: 99 indexes / 202 entries and 53 manual functions reviewed; four Lichuan model scopes and 21 open feature gates. [Test-only contrast ledger](reference/drive_contrasts.md) covers N5 stepper/BLDC, EPOS4 and protocol boundary cases without claiming support. |
| 5-6 | Buildable stack fit experiment and common API/ownership proof | Complete: one selected engine, concrete common contracts, one fixed owner table and checked read/write/contrast fixtures. Production package/units now exist under 7–9; production axis/reference policy now exists under 10. |
| 7-8 | Native package, source/install consumers, CI/verifier and Doxygen | Complete: minimal C++11 core with isolated source/install/export consumers, quick/full verification, strict Doxygen and passing Windows quick and Linux GCC/Clang sanitizer CI. Exact results are in the stage reports. |
| 9-10 | Status, exact units and axis/reference conversions | 9 complete: production exact units/status and quantization with local/CI evidence. 10 complete: pure axis/reference conversions, offline preview and local/Windows/GCC/Clang sanitizer CI. No native RPM width, mandatory step geometry or guessed feedback scale. |
| 11 | Classical CAN and checked SDO codec boundary | Complete: local full and exact-commit Windows/GCC/Clang sanitizer CI pass. One selected encoder, explicit channel/width correlation, full aborts and unchanged failure outputs. |
| 12 | Reviewed generated object metadata and coverage | Complete: 194 resolved descriptors, four withheld/four excluded and active freshness; local and exact-commit Windows/GCC/Clang sanitizer CI pass. No semantic operation is implemented. |
| 13 | Bounded SDO lifecycle | Complete: optional selected-stack adapter, two unique channels, copied intent, immutable cutoffs, retained terminals, 1-32-byte forms and quarantine; packet 5 narrowed quarantine and added a drain marker no real route can use until Q18; [local/CI evidence](reports/prompt_13.md) |
| 14 | SDO ambiguity/reconciliation | Complete: processing/capture precedence, retained observations, invalidation and TX eligibility; permanent quarantine without proven reopening. Local/CI evidence in [report](reports/prompt_14.md) |
| 15 | Passive network events and invalidation | Complete: pure codecs, shared node anchors, captured-time/epoch facts, four copied diagnostics and synchronous cache invalidation; checks in [report](reports/prompt_15.md) |
| 16 | Explicit NMT requests and service eligibility | Complete: native producer, intent/TX/observations, domain effects and heartbeat-disabled service evidence; [report](reports/prompt_16.md) |
| 17 | Observed PDO maps and decoding | Complete: checked layouts, explicit actual-map acquisition, independent sample qualification and atomic passive routing; [report](reports/prompt_17.md) |
| 18 | Explicit PDO remapping | Partial: fixed TPDO1 map3-to-map2 implemented and specimen-observed; [checkpoint and remaining Q17 gates](reports/2026-11-02_nmt_pdo_checkpoint.md). Broader settings/timers/SYNC, persistence and sample qualification remain. |
| 19 | Typed raw identity/state/configuration reads | Partial: 30 rows, checked SDO/PDO evidence and getters implemented. Packet 4 admits the DEC-1 list and `prepareProbe`; packet 11 read the list live on two specimens. Q20 read effects stay open for every row. |
| 20 | Native driver, limit and motion-parameter groups | Partial: nine typed raw read rows, position-parameter settings and the `2202` selector (packet 12); the `2200`, `2201`, `607D` and `6060` gates remain [owned here](reports/prompt_20.md). Both answer `607D:00` with two bytes against the EDS U8; with the explicit measured count width both accepted `2202` codes 1 and 0 with readback (2026-11-04). Current/holding remains solely 22. |
| 21 | I/O settings and output actions | Partial: all 23 I/O reads and explicit DI settings implemented, the DI writes verified through the real owner (packet 13); DO, filter and output actions retain the precise [evidence gates](io_settings.md). Those remaining mutations retain their own partial-effect/invalidation obligations. On node 1, DI3's function 0 → 7 → 0 and polarity bit 3 were accepted and read back, and the polarity write did not survive a power cycle (2026-11-04). |
| 22 | Tuning/current groups | Partial: all 33 typed reads and explicit 86C loop selection implemented, the `2500` write verified through the real owner (packet 14); the other 32 writes retain precise admissible-domain/scale/effect gates in the [tuning guide](tuning.md). Current/holding remains solely here. All 31 reads of `2502..2520` ran on both specimens (2026-11-04); no write has run, and the `2500` trial needs a phase-current probe. |
| 23 | Ordinary owner dispatch/results | Complete: owned software implemented/tested, including actual typed read/loop-setting integration; packet 2 moved the owner to `adapters/owner/`. Inherited gates are listed in PROGRESS. See [report](reports/prompt_23.md). |
| 24 | Urgent/cancel/recovery | Complete: owned mechanisms implemented/tested; packets 5 to 7 added settlement, one urgent slot per channel and the best-effort stop route. Inherited gates are listed in PROGRESS. [Report](reports/prompt_24.md) retains real-adapter and remote-session limits. |
| 25 | Passive health/statistics | Complete: cached projection, exact saturating counters/reset, eight copied diagnostics. Inherited gates are listed in PROGRESS. [Report](reports/prompt_25.md). No drive or machine-health policy is inferred. |
| 26 | Shared native application and fake/offline examples | Complete: owned software implemented/tested. Inherited gates are listed in PROGRESS. [Application contract](native_application.md), [report](reports/prompt_26.md). |
| 27 | Pinned TWAI adapter | Reviewed ESP-IDF 5.5.5 S2/S3 port and compile evidence; packet 3 changed the submit contract and set `fail_retry_cnt=-1`. Partial: the W9 remainder, and packet 3's TWAI questions 1, 4 and 6, which packet 11 left unanswered. [Report](reports/prompt_27.md). Bounded S2 exchanges and a read-only soak observed; broader physical qualification pending. |
| 28 | Shared Arduino/ESP-IDF application | Arduino/S3 IDF/S2 bench/isolated component build gates; Partial on inherited 27/24/26/19/Q20. [Report](reports/prompt_28.md). S2 uses explicit fixture configuration and UART; P4 complete firmware remains pending. |
| 29 | Evidence harness, fixture record and conflict acquisition | Typed and raw finite collectors implemented and host/firmware checked; [two-node S2 raw acquisition](reports/2026-11-02_s2_live_connection.md) observed. Packet 11 added flash, parser and soak tools, and ran typed identity, the readiness image and a read-only soak live on two specimens. Partial on Q20 and remaining [measurement gates](device_conflicts.md); no PDO mutation or motor qualification. |
| 30 | State interpretation, enable/release | Partial: checked documentary decoder and a [gated enable](drive_actions.md) (packet 15), closed by default, which succeeded on both specimens in the attended session. The release command remains unresolved and absent. |
| 31 | Stop actions and urgent owner integration | Partial: halt and quick stop from the DEC-8 predecessors, the best-effort `000B` route and `AxisSession` history (packets 6 and 7) implemented on the host; fresh not-running reports decide completion. One HALT from `001F` stopped and held a 2 rpm move on node 1. A stationary `000B` from `000F` and from `010F` released the motor on each node, and the quick stop completes on that report on hardware (2026-11-04). A stationary `010F` holds; `000B` during a move releases with `2100` running latched and completes on standstill; `000B` to a released or never-enabled drive changes nothing or releases it. Only `005F`/`003F`/`007F` predecessors stay unmeasured; [remaining gates](stop_actions.md). No enabling preamble. |
| 32 | Fault/reference actions | Partial: alarm clear with explicitly unresolved fault identity (packet 17) and zero-only counter clear with a retained baseline (packet 18), with exact owner/reference handoff, both called through `AxisSession`. A counter zero ran on node 2 (2026-11-04, `6064` 1180 to 0); no alarm clear has run, as no natural alarm has occurred. [Other reset/firmware gates](fault_reference_actions.md) remain; no enable or home claim. |
| 33 | Finite position | Partial: [pure native parameters](position_parameters.md) and a gated absolute lifecycle (packet 20), closed by default. Packet 19 measured node 1's acceptance and completion signals, and with them the `move` route ran the lifecycle once, unattended, on node 2 and succeeded: one specimen and firmware, not qualified. Stop rows during a move (packet 16) and relative moves (packet 21) remain. |
| 34-36 | Angles, finite velocity and homing | Pending; the repair audit's hold is lifted only for consumed qualified contracts. Explicit trigger/acceptance/completion, reference/scale and bounded deadlines |
| 37-39 | Discovery, communication changes, save/reset | Pending; no writes in discovery or hidden persistence; retain uncertain route/partial effects |
| 40-42 | Strict CLI parser, typed device and motion routes | Pending; one shared app/inventory, exact units, cached versus refreshing routes and schema correlation |
| 43-44 | Python session/parser and finite campaigns | Pending; offline/fake tests plus explicit connection and retained failure evidence; no uncertain-write replay |
| 45-46 | Integrated faults and FieldCore-like/alternate-drive consumers | Pending; test real resumed settings/remap through owner and preserve framework independence |
| 47-48 | Finite physical tests and sustained-load qualification | Pending; bounded two-node S2 connectivity exists, while motor/load and sustained qualification retain their separate fixture/evidence requirements |
| 49-50 | Package/docs audit and final review | Pending; software verdict and physical qualification stay separate; no release publication implied |

The assigned [prompt 19](prompts/19_typed_reads.md) retains its Q20 probe
obligation; see the [read guide](reads.md). Later prompts require a new assignment.
[Prompt 18](prompts/18_pdo_configuration.md) retains its blocked explicit
PDO configuration work over the implemented
[bounded SDO lifecycle](canopen_services.md) and
[reviewed object metadata](reference/lichuan_generated.md),
[axis](axis_contract.md) and [production units](units_status.md), under the established
[verification gates](verification.md). The
[reviewed Lichuan dictionary](reference/lichuan_objects.md) and
[contrast cases](reference/drive_contrasts.md) are documentary inputs;
[Lichuan qualification gates](reference/lichuan_review.md) remain open for the
affected later features. No operational drive profile or physical qualification is claimed.

The former stage numbering is mapped in [SPLIT_MAP](prompts/SPLIT_MAP.md).
Use current owners 11/12 for codecs/catalogue, 13/14 for services, 15-18 for
network/PDO, 19-22 for typed reads/settings, 30-32 for actions, 33-36 for motion,
and 37-39 for commissioning/persistence. Do not restore the old stage 08
catch-all. Preserve all supplied prompts and historical audit evidence.
