# Packet 19: Position evidence and one isolated finite experiment

Execute this packet fully. Apply [COMMON.md](COMMON.md). This continues original 29
and supplies evidence for 33; it does not build the production move engine.
Consume packet 11 diagnostics, packet 15's usable mode/enable evidence, packet 16's
stationary stop and software contracts, and packet 18's reference-invalidity rules.
An unavailable release, active-fault test or nonzero counter-clear measurement is
not a prerequisite. Moving-stop and scale measurements may be jointly established
here with packet 16: do not create a circular requirement for already qualified
motion to investigate the first motion. Until then the independent physical cutoff
and explicit fixture envelope provide the control, not an unqualified CAN stop.

## Reopen concrete evidence

Read `profiles/lichuan_c/Position.h/.cpp`, `Axis.h/.cpp`, `MotionPreview.h/.cpp`,
the current action/session implementation, `docs/position_parameters.md`,
`LICHUAN_MOTION_FACTS.md`, `device_conflicts.md` and the Nov2 motion/checkpoint
reports. Render EN86/ZH86 PDF 11–12 and 17–18, OP86 PDF 9–10, and relevant EDS
sections607A/6081/6083/6084/6040/6041/6060/6061/6064/2201/2501. Reconcile
current source claims; do not select a status dialect by language or generic
CiA402 masks. Refresh RS actual position/acceptance tests and FieldCore snapshots.

Documented candidates are I32 target 607A in−1000000..1000000 pulses, U32
6081 in 0..3000 rpm and 6083/6084 in 0..2000 ms. Zero is an allowed documentary
value, not a proven useful motion profile. Absolute trigger000F→001F and relative
000F→005F differ from immediate-update003F/007F. FIFO capacity, relative base,
acceptance delay and falling-bit4 behavior are not established by those words.

## Prepare, fake-test and run only the admitted experiment

1. Build one finite application-local typed sequence through public codec/service
   validation and the single owner. Reuse packet 11's records and parser. Keep
   production capability gates closed until evidence is reviewed; do not fork a
   second motor driver, add arbitrary send commands or flip MotionPreview's
   executionReady flag. Add only this named sequence and its tests.
2. Record current starting command/actual counters, origin/reference validity,
   mode/state/target history, scale hypotheses, configured inputs, settings,
   time/movement bounds and operator observations. Reuse retained target 5000/
   velocity120 as historical warnings, never current intent. Independent shaft
   marking must distinguish direction and approximate rotation. If the paper
   mark cannot supply the needed observation, request that concrete improvement.
3. Review one absolute candidate first. Native coordinate16,1 rpm,1000 ms ramps
   is an existing offline proposal, not a mandatory safe value or16-step travel
   guarantee. Retain/change it only with an explicit source and fixture basis.
   If command/feedback scale is the measured unknown, do not simultaneously claim
   a software angular bound derived from it. Specify an independent bounded
   exposure/containment instead; if the fixture cannot provide that, keep the
   trial pending and obtain the missing measurement/source. Do not hide target
   zeroing, counter clear, enable, mode changes or input reassignment in setup.
4. After independently admitted mode/enable work has settled, confirm current
   prerequisites, submit reviewed parameters once, then one rising-edge trigger.
   Record every service prefix, first possible trigger, fresh status/monitor/
   position/velocity samples and independent shaft movement. Never treat SDO ACK,
   an old bit15 or unchanged zero as new acceptance/completion. Keep fault-code
   acquisition explicit under its read-effect policy.
5. If the isolated moving-stop trial from 16 is needed, name its exact trigger
   predecessor/stop command and independent cutoff in the same reviewed plan.
   Observe both native stop reports and physical behavior. No000F stop preamble.
   A post-terminal000F rearm is its own visible experiment/step; it is project
   policy until the mode 1 falling-edge behavior is measured. Keep bit4 held during
   the move. Do not infer immediate-update or queued-target support.
6. Use the trace to decide what actually establishes acceptance, completion,
   scale/reference and stop for these specimens. Repeated coincident counters
   do not distinguish actual from commanded relative bases. Require a safe
   discriminating case or applicable source; do not deliberately create a
   dangerous following error. Unresolved relative scope stays separate.

Delegate independent original-page/sequence review and fake failures before
live admission. Only the lead operates hardware. Fake-test every prefix failure,
late/mismatched replies, missing acceptance, stale completion, reboot and cutoff;
verify uncertain triggers cannot replay. Apply COMMON's diagnose/fix/retest loop
to actual failures. Update the ledger, focused tests, packet 19 and original 29/33
reports with raw records and remaining Q gates; update16 when jointly measured.
Commit/sync verified work. One specimen trial does not qualify all firmware or
complete a production move implementation.
