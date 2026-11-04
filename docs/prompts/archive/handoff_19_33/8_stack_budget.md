> Executed handoff, retained for traceability. Do not execute this copy. All 22 packets ran between 2 and 4 November 2026. The [execution table](../../../reports/handoff_19_33/README.md) records what each delivered, each packet report records what remains, and the [backlog](../../../backlog.md) rows H03 and H04 track the physical trials still owed. Relative links were relocated; task wording is retained from `2920479`.

# Packet 8: Measured action stack and atomic publication

Execute this packet fully. Apply [COMMON.md](COMMON.md). Prerequisite: packet 7.
Execute W7/A-11/C-15 before calling actions in firmware. The current owner task
stack is 12,288 bytes; the audit's8,192-byte stack and measured older frames are
historical. Current read-loop `stackFree=9580` does not measure action execution.

Inspect `Actions.cpp`, `Reads.cpp`, `Settings.cpp`, `ProbeApp.cpp`,
`DeviceEvidence.cpp`, `examples/common/EspProbeMain.h`, sdkconfig files and
the actual IDF compiler commands. Identify nested full-context candidate copies
in prepare/advance/transmission-notice paths and automatic caller storage.
Read RS concrete-context publication and ADS/INA borrowed-input/terminal rules.
Preserve atomic output-on-rejection; this is not permission to mutate first and
return an error afterward.

Add the audit's reproducible `scripts/check_stack.py` using the pinned S2/S3
toolchains and actual options, both `-Os` and `-Og`. Include core and application
paths, record compiler/source hashes and parse frame/call-chain evidence. Initial
project limits are2,048 bytes per function and 6,144 bytes through statically
known call chains. Account visibly for indirect calls, SDK/ISR overhead and
unknown edges; do not claim a complete bound when call graph data is incomplete.
Fail for missing required CI toolchains; a local unavailable tool may report a
visible skip but is not a pass. Add parser/threshold regression fixtures and
integrate the check into the existing IDF CI job.

Reduce copies with validate-then-publish preparation and two-phase advancement.
Recheck every failure branch that formerly mutated a candidate before validation.
Use caller/owner-held contexts and small retained incident evidence rather than
embedding duplicate complete read histories. Shrink `ActionContext` where the
incident/history contract permits it; any public shape change gets migration
documentation. Do not replace one large copy with `context = Context()` and
assume it removes a temporary: measure generated frames. No heap workaround,
global shared operation state or larger stack used to hide the same defect.

Cover all W7-listed paths: action preparation/advance/notice, read advance,
driver/I/O/control settings prepare/advance, explicit read preparation,
ProbeApp requests and DeviceEvidence start. New best-effort-stop paths are also
included. Keep contexts static or explicitly owner-held in firmware and expose
real task high-water evidence through the existing diagnostics.

Delegate stack-script/parser work separately from lifecycle-copy refactoring;
one writer owns each core file. A strong reviewer checks aliasing, partial
mutation and terminal lifetime against actual assembly/stack outputs and tests.
Run unchanged-output/aliasing regressions, action/reference/owner suites, full
verification and all affected firmware builds. Record before/after frames,
call-chain limits and residual runtime measurements in packet 8 and affected
reports. Verify exact-commit CI and commit/sync. Live stop stack measurement
belongs to packet 16; host stack checks alone do not claim that measurement.
