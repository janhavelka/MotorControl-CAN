> Historical v1 promptset (00-19), retained for traceability. Do not execute this copy. Use the [current prompt index](../../README.md). Relative documentation links were relocated; task wording is retained from `dc8270740d0e16f19e170e2a982f67389c8a2c56`.

# Stage 18 Qualify the real drive and family behavior

Apply [COMMON.md](COMMON.md). The full motion/regression campaign requires stages 14–17, actual CAN bench facts and applicable user authorization. The early evidence phase below can run sooner to resolve facts that block stages 09–11.

## Audit

Refresh RS finite bench/evidence practices and Python runners, I2C HIL/parser discipline, target stage reports and current firmware build identity. Read the original target label/manual and current source conflicts. Existing RS bench permission, shaft configuration and serial ports are not CAN authorization or wiring evidence.

## Establish the fixture

Record exact controller/SDK, transceiver, pins/interface, termination/cabling, drive model/label/firmware/raw identity, motor/encoder/wiring, supply, node/DIP/bitrate, relevant I/O, mechanical arrangement and available observation/stop control. Resolve missing facts needed by the chosen tests; continue host-only work while they are pending. Do not use an RS transceiver or assume an RJ45 is Ethernet.

## Early evidence phase without a circular gate

Documentary checks and raw identity/configuration acquisition can begin after the necessary checked codecs/services/reads from stages 05–08 and a narrowly scoped stage-14 transport adapter slice are host-tested. They do not require finished motion APIs or the full CLI/Python campaign. Use the same public typed codecs/service path and single owner, with a bounded native/firmware qualification harness; do not add a second motor implementation or mark unknown state bits as ready/done.

If a firmware fact requires a state-changing experiment, establish that experiment's concrete action, actual fixture, independent stop/control prerequisites and applicable authorization. Exercise only the typed sequence under review, retain raw evidence and keep its qualification status explicit. Do not rely on the disputed completion/stop interpretation as the sole admission evidence. Feed the result back into the ledger/profile tests and resume stages 09–11. If the experiment cannot be performed, keep that feature gated while independent implementation continues. Full motion/load/family qualification follows the normal later prerequisites.

## Execute and retain evidence

1. Start with passive/read-only observations and identity/configuration, not enable or factory defaults. Verify SDO routing, node state, actual mappings, heartbeat setup, scale and status/monitor dialect. Save a non-changing baseline and document any intended changes separately.
2. Resolve the manufacturer conflicts through bounded tests suited to the fixture: state/fault/completion bits, velocity start order, target acceptance/rearm, signed rate, subdivision/position scaling, I/O encodings and documented clear/save behavior. A working bit pattern on one firmware is not all-family evidence.
3. Qualify explicit enable/release and stop behavior before dependent motion trials. Use small finite position moves first with explicit units/limits and retained start/stop evidence. Test return/relative/absolute/angle and zero target. Do not claim physical travel/speed from a status bit alone; record how it was observed/measured.
4. Exercise finite velocity/homing only after their resolved sequencing, signals and stop prerequisites are established. Physical fault/link-loss/persistence/restart tests run only where the actual setup supports them and authorization covers the action; a communication cut does not guarantee a stop.
5. Add short regression, targeted interruption/fault scenarios and sustained-load tests appropriate to the fixture. Record actual TX/RX observations, timing limitations, service gaps, faults, resource use and physical outcomes. Do not generalize software timestamps to exact CAN wire timing.
6. When a failure occurs retain it, reproduce deliberately when appropriate, distinguish evidence from hypotheses, fix the root cause, add a host regression where representable, then rerun the failed scenario and a short existing-feature regression. A later passing run does not erase the issue.
7. Qualify CL86-C first. Track OL86-C/CL57-C/OL57-C separately; shared documents/code do not qualify untested hardware. Back up/reconcile configuration only through reviewed read/write semantics; restoration itself can be uncertain and is never an automatic retry recipe.

## Concrete implementation contract

Create a finite test record before each live scenario. It includes fixture ID, exact firmware build, selected model/dialect, current route/configuration, explicit action, bounds, required observations, independent stop/control prerequisites, expected evidence and cleanup/reconciliation plan. Reuse the existing scripts and owner path; do not write a one-off raw controller sender to bypass blocked API behavior.

Separate qualification questions so a test has an interpretable result:

| Question | Evidence to retain | What it does not prove |
| --- | --- | --- |
| Object access/type | Request, matched response/abort, exact bytes | Motion semantics or all firmware versions |
| Status/trigger meaning | New raw status/monitor sequence with command timeline | Physical travel from status alone |
| Stop behavior | Requested stop, TX/service/drive observations and physical observation method | Safety certification or behavior under every fault |
| Scale/direction | Known configured scale, exact target, measured motion/reference | Other motor/encoder configurations |
| Persistence/restart | Before/after readback with documented restart and partial effects | Guaranteed rollback on failure |

Do not make the qualification harness more capable than the public API it is evaluating. A disputed operation may be exercised only as a clearly identified experiment under the early-evidence procedure, with independent prerequisites; its result is not a generally enabled capability until reviewed and encoded.

On a failure, record observed facts separately from explanation. Preserve raw logs, unsuccessful stop evidence and changed configuration state. Add a host regression only for the behavior a fake can represent. Do not convert a simulated reproduction into physical qualification.

At completion update each model/firmware/feature column independently. Related family members can share reviewed implementation yet remain unqualified physically. An unavailable fixture leaves stage 18 pending and allows the honest software candidate work in stage 19.

## Documentation and work split

Apply [DOCUMENTATION.md](DOCUMENTATION.md) and [MAINTAINABILITY.md](MAINTAINABILITY.md). Update bench facts, source-conflict resolutions, measurement methods, scenario instructions and qualification matrix. Any learned API behavior requires matching Doxygen, tests and release limitations.

Use [DELEGATION.md](DELEGATION.md) for read-only evidence review and host regression work. Only one authorized operator controls the physical fixture; concurrent agents must not open the same console, change settings or send motor commands.

## Exit

Write per-fixture/per-feature reports with exact software revisions, raw evidence and measurements, update dialect/capability gates and release scope. If no hardware is available, mark this stage **pending hardware** and proceed to the software release-candidate tasks in stage 19. Never fabricate HIL results or mark family qualification complete from compilation/emulation.
