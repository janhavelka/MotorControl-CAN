# Native coverage reconciliation through prompt 36

This is the manual reconciliation required by original prompt 36, before prompt
46's automated coverage audit. The editable object authority remains
[lichuan_objects.json](../reference/lichuan_objects.json); its generated
[accounting](../reference/lichuan_generated.md) separates read, narrow setting,
semantic and hardware coverage. A descriptor is a wire shape, not an implemented
operation. A successful upload or acknowledged write is not physical qualification.

The audit compared the original prompts, current public declarations, their
implementation, registered native tests and maintained user guides. Earlier
stage reports contain explicitly historical checkpoints; their blanket write
blockers do not override the current APIs. No earlier stage is returned to Pending,
and the four confirmed missing typed read rows below stay assigned to their
original stages. Prompt 36 does not absorb those implementations.

## Finite read inventory

There are 102 standalone read descriptors and implemented read rows: prompt 19's
30, prompt 20's nine, prompt 21's 23, prompt 22's 33, prompt 35's one and prompt
36's six. All have documented/implemented/host-tested coverage; none has hardware
qualification. The 16 DEC-1 rows keep their existing accepted risk disposition.
Neither the homing reads nor the general velocity-target read expands that list.

| Owner | Exact rows / public path | Native tests / maintained documentation | Remaining obligation |
| --- | --- | --- | --- |
| 19 | Identity/state/configuration/probe: 30 scalar/subentry shapes in `Reads.h`; `prepareIdentity`, `prepareState`, `prepareConfig`, `prepareProbe`, `nextRead`, `advanceRead`, checked typed getters | `test/reads_test.cpp`; [read contract](../reads.md), [prompt 19](prompt_19.md) | Q20 remains object-specific. A probe is an ordinary upload with unknown effects, not a non-consuming discovery guarantee. |
| 20 | `2200`, `2201`, `2202`, `607D:00..02`, `6081`, `6083`, `6084`: nine rows via `prepareDriverRead`, `nextDriver`, `advanceDriver`, `getDriver` | `test/driver_settings_test.cpp`; [driver settings](../driver_settings.md) | All nine reads implemented. `607D:00` has an explicitly selected measured-U16 option; raw count never controls iteration. General reads retain Q20. |
| 21 | `2400:00..08/:0C..0F`, `2401..2407`, `60FE:00..02`: 23 rows via the same lifecycle and `getIo` | `test/io_settings_test.cpp`; [I/O](../io_settings.md) | All reads implemented. Reserved `2400:09/:0A/:0B/:10` stay excluded. `60FE:00` measured-U16 is an explicit fixture option; output readback is not voltage evidence. |
| 22 | `2500..2520`: 33 rows through `prepareControlRead`, `prepareTuningRead`, `getControl`, `getTuning` | `test/tuning_test.cpp`; [tuning](../tuning.md) | All reads implemented. Raw coefficients and EDS-only words do not gain a domain or write permission. |
| 31 | `605D:00`, raw I16 | Original [prompt 31](../prompts/31_stop_actions.md), requirement 10; no standalone typed read/getter/test | **Confirmed implementation omission.** Add a bounded typed raw read with wrong-width, signed-extrema, age/epoch, partial-failure and real-owner tests in owner 31. Q20 gates ordinary acquisition, not the host implementation. No halt-option interpretation follows. |
| 32 | `2301:00`, `2302:00`, raw U16 | Original [prompt 32](../prompts/32_fault_reference_actions.md), requirement 6; no standalone typed read/getter/test | **Two confirmed implementation omissions.** Implement both finite raw reads in owner 32. Existing clear/zero downloads and subsequent monitor/feedback reads do not implement stored-value readback. Q20 stays open; a stored 1 proves no clear or edge. |
| 33 | `607A:00`, raw I32 | Original [prompt 33](../prompts/33_position_moves.md), requirement 7; enable tests exercise only the separately accepted one-upload prerequisite | **Confirmed implementation omission.** An enable's bounded prerequisite is not a general typed read/getter. Implement the latter in owner 33 with UNQUALIFIED/Q20; do not copy the action-only acceptance. |
| 35 | `60FF:00`, raw four-byte word, EDS-I32 view via `DriverGroup::VELOCITY_TARGET` / `getDriver` | `test/driver_settings_test.cpp::velocityTargetRead`; [velocity](../velocity.md) | Implemented standalone read; RAW descriptor preserves Q03 signedness disagreement. General uploads are Q20-unqualified; one accepted upload per enable remains separately scoped. |
| 36 | `607C:00` I32, `6098:00` raw byte, `6099:00` U8, `6099:01/:02` U32, `609A:00` U32 via `DriverGroup::HOMING` / `getDriver` | `test/driver_settings_test.cpp::homingRead`, real-owner group, correlation/failure/deadline tests; public `DriverSettings.h` and [homing](../homing.md) | All six standalone reads implemented. Explicit reviewed Q20 acquisition only; no upload-effect, offset-effect or successful-home claim. |

`HOMING` always uploads the six declared fields in that order, regardless of the
returned `6099:00` count. It preserves unknown method bytes, zero, values outside
motion-admission ranges, I32 extrema and full U32 values. `homeOffsetRaw` has no
qualified physical units or coordinate effect. `homeMethodRaw` retains the
EDS/mode-table U8 versus dictionary I8 conflict without interpreting high bits.
`homeSearchSpeedRpm`, `homeCreepSpeedRpm` and `homeRampTimeMs` are source-native
units; readback does not validate them as a motion candidate. `getDriver` publishes
all six provenance records atomically only after complete current acquisition.

## Settings and actions

The 13 narrow setting rows are `2202`, `6081/6083/6084`, `2400:01..08` and
`2500`. They have their own setting coverage and do not grant a generic raw
writer. State-changing operations below remain conditional on their documented
admission/evidence contracts and the model/firmware uncertainty in their guides.

| Owner | Implemented public contract / rows | Native test and documentation evidence | Precise remaining scope |
| --- | --- | --- | --- |
| 20 | `prepareDriverSettings` for native position speed/ramp settings; `prepareSoftLimitSettings` for `2202` codes 0/1; `noteDriverTransmission`, readback and checked `getDriver` | `test/settings_write_test.cpp`, `test/settings_owner_test.cpp`; [settings decisions](../driver_settings.md#setting-decisions) | `2200`: labels do not establish CAN direction effect. `2201`: range/default, scaling and activation conflict (Q04). `607D:01/:02`: no admissible operating domain or coordinate basis (Q05). `2202` readback does not establish enforcement. `6060` has no independent settings setter: enabled mode changes remain Q10/Q11. Concrete operation-owned mode writes do not complete that general assignment. |
| 21 | `prepareIoSettings` writes complete explicit DI polarity/functions, preserves unexplained bits and compares whole readback | `test/settings_write_test.cpp`, `test/settings_owner_test.cpp`; [I/O decisions](../io_settings.md) | `2400:0C..0F` DO polarity/functions remain firmware-address/code conflicts (Q06), including codes 2/3. `2401..2407` need coefficient domain/equation/unit (Q08). `60FE:01/:02` output actions lack 86C terminal mapping and complete mask/clear/untouched-bit semantics (Q07). No typed output-action API exists. Existing measured DI changes are specimen reports, not electrical qualification. |
| 22 | `prepareControlSettings` for explicit `2500` loop selection, checked `getControl`; 33 native reads above | `test/tuning_test.cpp`, `test/settings_owner_test.cpp`; [per-subgroup decisions](../tuning.md#decisions-per-subgroup) | The other 32 writes remain assigned here. `2501` needs resolution convention/domain; `2502` phase-current bounds and installed motor rating; `2503..2507` percentage bases, ranges, ordering and holding timing; `2508..250D` code sets, equations, threshold scales and time semantics; `250E..2519` gain domains/scaling/order; `251A..2520` manufacturer meaning/units/domain (Q09). Each row's ledger `review.next_evidence` remains authoritative. |
| 30 | `prepareEnable`, `nextAction`, `advanceAction` with bounded reviewed targets and 6/7/15 word observations | `test/enable_action_test.cpp`; [drive actions](../drive_actions.md) | No `prepareRelease` implementation: no applicable qualified release sequence/reference or torque consequence. Mode1 specimen observations do not qualify mode3 or mode6 enabling, enabled mode changes, retained-target behavior or another firmware (Q02/Q03/Q11). Mode6 extension belongs to prompt36's explicit enable contract; home never enables implicitly. |
| 31 | `prepareStop`, `prepareEscalation`, `prepareBestEffortStop`, ordinary/urgent owner path and retained separate outcomes | `test/stop_action_test.cpp`, `test/velocity_owner_test.cpp`; [stops](../stop_actions.md) | DEC-8 host policy and qualified drive completion remain distinct. Moving predecessor rows, mode3/mode6 stopping and effects need applicable evidence (Q12); no guessed 605D semantics. Q18 still prevents an unproved drain marker from reopening ambiguous routes. Standalone605D read is the confirmed omission above. |
| 32 | `prepareClearAlarm` (2301 1 then0) and `prepareSetDevicePosition` (zero only, 2302 0/1/0), separate fault/reference withdrawal | `test/alarm_clear_test.cpp`, `test/counter_clear_test.cpp`; [fault/reference actions](../fault_reference_actions.md) | Q13: FF05-to-present-alarm identity and reset route/effect unresolved; no naturally present alarm has been cleared on the fixture. Q14: observed raw zero does not establish which counter changed, home, origin or arbitrary device position. 607C is owned by36, not32. Standalone2301/2302 reads remain omitted as above. |
| 33 | Exact native parameters and finite `prepareMoveAbsolute` / `prepareMoveRelative`, `nextMove`, `advanceMove` through the owner | `test/position_parameters_test.cpp`, `test/move_absolute_test.cpp`, `test/move_relative_test.cpp`, `test/axis_session_test.cpp`; [position](../position_parameters.md) | Q02/Q04/Q05/Q10/Q11/Q12 and fixture-specific acceptance/completion/reference facts remain separate. Node2 relative base, negative relative motion, remaining stop predecessors and physical scale/reference remain unqualified. Standalone607A read remains omitted. |
| 34 | `prepareMoveAngle` reuses finite position lifecycle with exact angle-path/reference policy | `test/move_angle_test.cpp`; [angles](../position_parameters.md#wrapped-angle-moves) | Software scope Complete; inherited scale/reference and motion evidence is unchanged. No separate angle wire register or hardware accuracy claim. |
| 35 | `prepareVelocity`, `nextVelocity`, `advanceVelocity`, separately settled stop and ordinary owner | `test/velocity_test.cpp`, `test/velocity_owner_test.cpp`; [velocity](../velocity.md) | Q03/Q11: positive target-before-enable startup, reverse domain, enabled mode change and zero semantics; Q12 mode3 stop evidence. No specimen velocity trial closes them. General60FF reads retain Q20. |
| 36 | Concrete method-specific home/reference lifecycle and explicit mode6 enable/interrupt are accounted in [the prompt36 report](prompt_36.md) | Homing tests and integration verification are recorded in that report; the six read rows above have independent native coverage | Q04 direction relative to2200, Q10 method/sign details, Q11 enable/mode/zero-ramp facts, Q14/Q21 offset application and qualified completion/reference need their exact evidence. Host lifecycle success cannot close them. |

The missing standalone reads are software omissions, distinct from the genuine
manufacturer conflicts governing writes or effects. They keep prompts31/32/33
Partial even if a future fixture closes their other device gates. Conversely,
missing write domains in20-22 are not repaired by an unrestricted object accessor,
and read success does not implement output, tuning, release or persistence actions.

## Later ownership retained

Communication commissioning remains38: `2203` node/bitrate acquisition and
mutation, explicit routing/node changes and remaining PDO communication/map
reads and settings. Existing19 communication reads,16 NMT,17 map observation and
18's fixed TPDO1/event-timer subset remain usable; they do not complete38.
Persistence/reset/reconciliation and2300 remain39 (Q15). No save, restore, reset,
NMT start or other communication side effect was added by this reconciliation.
CLI40-42 and Python43-44 remain their own deliverables; the existing finite bench
routes do not constitute that shared command surface.

## Evidence and verification

For the six newly implemented read fields, original EDS sections607C,6098,
6099sub0/sub1/sub2,609A and original EN86/ZH86 PDFp13, plus EN86p17's dictionary,
were reopened. The original EN86p13 table was rendered and visually checked.
These confirm wire widths and native rpm/ms fields; EN86p13 U8 versusp17 I8 and
the table's "creep time(rpm)" wording remain visible. Offset write order, sign,
units and final-coordinate effects are unresolved; no607C effect was inferred.
Original artifacts and hashes were preserved. Physical tests in this task: none.

Focused GCC15.1/Python3.12.10 checks passed: configured and built
`driver_settings_test` in `.build/p36-read-agent`, then
`ctest --test-dir .build/p36-read-agent --output-on-failure -R '^driver_settings$'`;
83 ledger tests;46 generator tests; actual generator/check mode; and diff checks.
The read tests include sole-owner wire requests, all six widths, raw extremes and
unknown methods/counts, wrong-width replies, each partial failure position,
stale epochs, preserved output and processing-time deadline equality. Integrated
action/homing, package, documentation and exact-commit CI results belong to the
main prompt36 report, not this focused subtask's results.
