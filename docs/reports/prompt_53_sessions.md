# Prompt 53 group stops and session storage

This supplements [prompt 53](../prompts/53_multi_axis.md). It records application
helpers and host checks; it does not qualify a drive, electrical stop input or
physical standstill. The lead owns combined firmware, stack, packaging, capacity
variants, CI and synchronization evidence in the stage report.

## Group stop contract

`examples/common/GroupStop.h/.cpp` borrows one `ProbeApp`, its sole bus owner and
caller-reserved `GroupStopMember` rows. Capacity is the supplied array length,
independent of ordinary operation slots. One nonwrapping group identity remains
retained until explicit release. Member-pointer arrays are borrowed during the
request only; attachments and row storage stay stationary through release.

The complete nonempty list is checked for capacity, duplicate logical axes,
matching application ownership and time before any request. Then every selected
`ProbeMotion::requestStop` is called once, even after another member refuses.
Each row retains admission status and its own copied `MotionStopResult`, including
sequenced and best-effort components, native abort, execution and uncertainty.
Existing per-axis stop policies and deadlines are unchanged. `service` snapshots
the attachments; `ProbeApp` remains responsible for session/owner progression.

The aggregate stays active until all members are terminal. Success requires every
member's own successful, qualified `OBSERVED` completion without uncertainty.
Refusal, fault, loss, best-effort transmission or a missing/replaced identity
cannot become aggregate success. A later successful stop does not erase the
earlier motion's retained uncertainty. No NMT stop, enable, reset, fault clear or
automatic replay is issued. Group release frees only matching group-owned stop
identities; unrelated slots are untouched. Failed per-axis release retains the
group's copies so an explicit release can be repeated without replaying commands.
Console reserved output and attachment integration are separate lead/console work.

`test/group_stop_test.cpp` exercises real owner/session stop lifecycles with
manufactured server reports: all-member observed completion, first-member refusal,
lost ACK, fault isolation, ordinary result exhaustion, duplicate/capacity rejection,
retained original motion effects, premature external release and owned snapshots.
It checks absence of NMT fallback and duplicate HALT writes. Single-axis and wired
cases run at N=1; only intrinsically paired scenarios skip there.

## Wired output and closed shared-RPDO gate

`WiredStopOutput.h/.cpp` is an example-level explicit callback to write a known
electrical signal. Construction does nothing. The application supplies the actual
active level and verified wiring; each `setAsserted` invokes the callback once.
Failure retains possible electrical effects, with no retry. Deassertion does not
enable motion or clear faults. A successful callback is host-output evidence only,
not DI readback or a stopped observation. Tests cover both polarities, unknown
wiring, assertion, deassertion and callback failure. The installed core has no GPIO.

Original [EN86](../reference/vendor/86C_CANopen_user_manual_en.pdf) and
[ZH86](../reference/vendor/86C_CANopen_user_manual_zh.pdf), PDF page 8 (printed
page 6), were reopened. They name DI function 5 as stop and function 6 as emergency
stop, the effective-level field `2400:01`, and DI0 through DI6 selection at
`2400:02..08`. The original [EDS](../reference/vendor/LC_StepDriverCAN_V1.1.eds)
declares these fields U16 RW and PDO-mappable. Existing `prepareIoSettings` is the
volatile configuration path; this helper does not configure or save them. Neither
the output helper nor those tables establishes the actual fixture polarity,
deceleration, final drive state or restart requirements. Wired hardware remains
prompt 54 work requiring the fixture's physical signal.

M09 is still absent: there is no reviewed controlword-only map/procedure with
partial-effect and readback evidence. EDS `1600:00` defaults to three entries;
`1600:01..03` are `60400010`, `60FF0020`, `60600008`, rather than a controlword-only
map. [Prompt 51](prompt_51.md) also retains Q28 remote buffered-trigger withdrawal
as unresolved. Therefore no shared quick-stop RPDO sender or invented map was
added. Its next prerequisite is a reviewed volatile map, routing/collision and
remote-provenance contract; any eventual shared frame still needs independent
per-axis settlement. An asynchronous stop cannot prove withdrawal of a remote
buffered start.

## B03: bounded home/velocity storage sharing

`AxisSession` now has one tagged union for `HomeContext` and `VelocityContext`.
Both admission paths already reject unresolved motion fences. Validation and owner
admission happen before replacing storage. All shared-storage input overlap is
rejected. An earlier home must have completed deferred coordinate synchronization
and stop-result association before another context can replace it.

The native getter for the inactive kind returns a constant `EMPTY` view. A
successful cross-kind admission ends borrowed references to the retired native
context; callers must copy history needed beyond that boundary. Refusal and
separate stop requests preserve the current native context. Home's established
retention across reopen remains; reopening still clears the velocity context
without clearing its persistent unresolved-stop fence. Published caller-owned
`AxisReference`, first-failure fields, controlword evidence and all stop/fault
fences remain independent of shared storage.

The larger proposed five-context union was deliberately not adopted:

- Enable remains separate because callers borrow `enableAction().read` into later
  motion and stop admission.
- Clear remains separate because alarm clear is allowed while an older home or
  velocity still needs a stop. Sharing would erase the native evidence needed to
  compose and report that stop, or require another retained copy.
- Original and replacement moves remain separate because prompt 52 retains both
  terminal records and their relation simultaneously.
- Both explicit stop contexts remain separate for HALT-to-QUICK_STOP escalation.

There is no allocator, runtime framework or installed-core change. Native tests
exercise home-to-velocity-to-home, failed cross-kind admission with byte-unchanged
old context, checked empty getters and published reference retention. Existing
stop, reopen, home-coordinate and velocity-stop tests remain unchanged.

Measured type layouts, against the lead's original HEAD header:

| Compiler target | Before AxisSession | After | Per-axis reduction |
| --- | ---: | ---: | ---: |
| Windows GCC 15.1 x86-64 | 40520 B | 37200 B | 3320 B |
| Xtensa ESP32-S2 GCC 14.2.0 | 40272 B | 36960 B | 3312 B |
| Xtensa ESP32-S3 GCC 14.2.0 | 40272 B | 36960 B | 3312 B |

The target compiler is `esp-14.2.0_20241119`, C++11, `-Os`. Two target sessions save
6624 bytes. Target object inspection places the two `constexpr` empty views
(3912-byte velocity, 3320-byte home) in `.rodata`, with no dynamic initialization,
guard or `.data`/`.bss` payload. This trades 7232 bytes of constant data for lower
per-axis RAM. Final linked firmware placement and whole-image memory belong to the
lead's firmware gate; type sizes alone are not a whole-image free-heap measurement.
The lead's final S2 bench and shared S3 ELF inspections, using the SDK's actual
`esp-14.2.0_20260121` compiler, confirm both constants in `.flash.rodata`
(3912 and 3320 bytes). The same compiler's strict stack/type run confirms
36960-byte `AxisSession` on both targets.

## Source audit and local checks

MotorControl-RS HEAD `e56d4c0af6a7d678da11bb672a1b94126812b3a1` and FieldCore-node
HEAD `c40ea8341edf8d665460b902cafefbe857a7f73f` were refreshed read-only, both clean.
Revisions are unchanged since prompt 52; prompt 53's new seams were read:

- RS `examples/common/RtuBusOwner.h::BusStorage` keeps caller storage and urgent
  queue/result quotas; `examples/probe_cli/ProbeApp.cpp::admitActionStep` routes
  each STOP through `admitUrgent`. Adopt reserved independent results and per-axis
  urgency. RS provides no group API or CAN stop semantics to copy.
- FieldCore `src/devices/rs485_motor/Rs485MotorModule.cpp` validates each command's
  identity/deadline and maps explicit `UrgentStop` to that profile's stop policy.
  Preserve independent ownership and immutable deadlines; do not import its RTU
  normal/direct wire semantics into HALT/QUICK_STOP.

Focused strict GCC checks at default N=2: `group_stop` 1732 checks,
`axis_storage` 179, `probe_motion_actions` 11455, `axis_session` 105800,
`velocity_owner` 230790, `homing_owner` 4226, and `axis_change` 3664. N=1 focused
build/run: `group_stop` 840, `axis_storage` 177, `probe_motion_actions` 11435.
Both target `AxisSession.cpp` objects compile with `-Wall -Wextra -Wpedantic
-Werror`. Strict Doxygen passed after documenting the helpers. Disposable local
artifacts are under `.build/p53-*`; no hardware was used by this worker.

The worker inherited its runtime model/effort and selected no override; exact
current model/effort identifiers were not exposed to it. Independent review and
combined gates are the lead's next step. No worker Git mutations, sibling edits
or hardware actions were made.
