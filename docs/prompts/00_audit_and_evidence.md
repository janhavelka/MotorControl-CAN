# Stage 00 Refresh source audits and evidence

Apply [COMMON.md](COMMON.md) in full. This stage begins the implementation assignment; do not rewrite the supplied promptset as a substitute for source inspection.

## Audit

Inspect MotorControl-RS root guidance, current `include/`, `src/`, `examples/common/`, `examples/probe_cli/`, `examples/probe_idf/`, `test/`, `scripts/`, manifests and verification workflow. Compare current CLI inventory to protocol-3 documentation. Read ADS1115, INA228, SHT3x-main, BME280, MCP45HVX1, MB85RC, PCA9555 and RV3032-C7 at the seams covered by the ecosystem report. Also inspect the SHZK-PT/VTN4xx/VibWire-108 stateless codec pattern where relevant. Inspect FieldCore's current motor contract, dormant RS motor module, owner/backend and native tests read-only.

## Deliver

1. Record revisions, dirty state, existing CI commands, toolchain availability and current target-tree state. Preserve user work; do not fetch/update siblings as part of an audit unless separately needed/authorized.
2. Create implementation-facing `docs/source_map.md`, `docs/backlog.md` and `docs/roadmap.md`. Separate evidence gaps, software tasks, integration work and hardware qualification. Retain dated supplied audits as historical evidence, with a new live delta report.
3. Build an actual naming/ownership inventory: Status helpers, preparation/advancement/result access, context lifetimes, config/generation, CLI routes, Python correlation, native test registration and package boundaries. Record differences instead of forcing uniformity where I2C libraries differ.
4. Verify all downloaded originals against `docs/reference/SOURCES.json`. Inspect PDF covers and conflict pages. Establish which documents/EDS cover which family members; do not infer firmware from PDF metadata or EDS creation time.
5. Record no-hardware assumptions. The bench is unspecified for CAN. Create `docs/hardware_bench.md` with known facts and missing board/transceiver/drive/motor/firmware/node/bitrate information; do not copy COM13 or RS free-shaft permission.
6. Define tracked requirements from `docs/requirements_matrix.md` and an honest stage execution ledger. Retain all prompt instructions until executed and archived.

## Concrete implementation contract

Create `docs/source_map.md` as a navigation table, not a copied directory dump. Each row identifies a responsibility, current source declaration, implementation, real caller, focused test, target location and intentional CAN difference. Include these rows:

| Responsibility | Required source evidence | Target decision |
| --- | --- | --- |
| Result vocabulary | RS `Status.h`, `ReadOperation.h`, `ActionOperation.h` and actual terminal contexts | Literal names and meanings from [CONVENTIONS.md](CONVENTIONS.md) |
| Motion | RS `Units.h`, `Axis.h`, ESS position/velocity/homing and callers | Same intent names; remove native RPM and serial-address assumptions |
| Physical owner | RS `examples/common/RtuBusOwner.h` and implementation | `admit/admitUrgent/service/result/release/cancel`; one CAN receiver |
| Health | Current ADS1115, INA228, SHT3x and PCA9555 headers/implementations | Passive application health; explicitly chosen states and counters |
| Console/runners | RS ProbeConsole, runner session/parser/evidence helpers and tests | Current routes, correlation and retained outcomes |
| Documentation/build | RS CMake/verify and I2C Doxyfile/CI | RS package shape plus strict API documentation |

Read enough implementation to establish behavior; finding a symbol with `rg` is not an audit. Record live revisions and dirty paths before and after inspection when another session is changing siblings. Do not attribute uncommitted behavior solely to HEAD. The pinned baseline remains immutable.

Create the stage report with three concrete lists: conventions retained, CAN differences selected, and unresolved facts with the smallest affected feature. Establish a coverage row's owner stage now: codecs 05, service 06, network 07, reads/settings 08, actions 09, position 10, velocity/home 11, commissioning/persistence 13. This prevents stage 08 from becoming a catch-all assignment.

## Documentation and work split

Apply [DOCUMENTATION.md](DOCUMENTATION.md) and [MAINTAINABILITY.md](MAINTAINABILITY.md). Update the planning/implementation distinction in README and docs index; create the implementation source map, backlog, roadmap and bench facts. There is no new C++ API to annotate in this stage.

Use [DELEGATION.md](DELEGATION.md): source audits, manufacturer evidence and documentation inventory can run independently. The lead owns the resulting naming decisions and source map. Each reviewer returns exact declarations and counterexamples, not “looks consistent.”

## Verify and exit

Run the existing promptset integrity check. Check referenced source paths against live code. If the RS quick verifier has not changed since the pinned passing audit, use that evidence with its revision; do not spend time repeating unchanged suites. Run a fresh focused check only when source changes or an open question justifies it.

Exit with an evidence-backed delta report and executable backlog. No target library source is required yet. Missing optional sibling checkout does not stop unrelated work; missing device evidence gates affected features.
