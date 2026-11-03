# Prompt 29: device evidence readiness and conflict gate

**Resumed by packet 11, 2026-11-03.** [Packet 11](handoff_19_33/packet_11.md) added `scripts/s2_flash.py`, the offline parser `scripts/s2_evidence.py`, and a read-only soak mode (`examples/s2_bench/Soak.cpp`, `scripts/s2_soak.py`). It then ran a live baseline on two specimens: typed identity 9 of 9 per node, and all 30 readiness fields identical to the retained captures. The first live typed success exposed a parser defect, fixed with a regression. The soak is communication evidence on two specimens and one image, not qualification. Q18 stays unresolved, and packet 3's TWAI questions 1, 4 and 6 stay unanswered. "No port was opened" below describes this report's original checkpoint, not the current state. Gates: live bench; local full verifier and firmware gates; CI failed on clang at `fcea8b4` and passed at `5306eaf`, which contains `edba06e`.

**Packet 19, 2026-11-03: the isolated position experiment has not run.** [Packet 19](handoff_19_33/packet_19.md) prepared the application-local bench `position` route as the acquisition path for Q02 (acceptance and completion) and Q04 (command and feedback scale). It needs an attendant, an independent cutoff, a shaft mark showing direction and approximate rotation, and packets 15 and 16 first. No raw motion record exists yet, and Q02 and Q04 stay open.

The [fresh independent 29/30 audit](prompts_29_30_audit.md) rechecks the original
assignment, implementation and source evidence; it records current validation
and confirms that acquisition/qualification obligations remain open.

## Scope and status

The owned software-readiness package is implemented and host tested. Stage status
is **Partial** on inherited 19/28 Q20 read-effect evidence and conditional physical
acquisition. The already verified typed reads, application, owner and firmware
adapter are consumed independently. No port was opened, firmware flashed, drive
connected or state-changing experiment selected. No hardware qualification or
production capability gate is closed by this work.

Execution continues on `promptset/motorcontrol-can`, upstream
`origin/promptset/motorcontrol-can`, entry commit
`548e206040f21d40372a0e0ea6aad1316abb9bfe`. Entry fetch showed 0 ahead / 0 behind;
the index was empty. Three unrelated untracked `.vscode` files were preserved.
Promptset version 2/base `7878f7c` and the established GitHub remote are unchanged.

## Current source and device review

| Source | Revision and dirty state | Implementation/caller evidence and retained difference |
| --- | --- | --- |
| MotorControl-RS | `f5f600e0c8b285dc3e2f0f31d236b51cc995ca4d`, clean at initial audit; concurrent consumer edits at final recheck | `scripts/bench_session.py`, `scripts/_bench_evidence.py`, bench scenario tests, ProbeApp cached-inspection tests and `docs/reports/2026-11-01_ess_integration_faults.md` retain finite intent, raw failures and generation/producer identity. New integration tests since the previous audit do not change production owner/bench behavior. No serial port, free-shaft permission or retry policy transfers. |
| ADS1115 | `235af3e8c722661a89a44766a4ccd903a654f6fe`, clean | `tools/run_i2c_hil.py` separates contract/evidence verdicts; source `takeResult`, atomic-sample tests and owner examples retain exact token and output validity. A host pass is not physical evidence. |
| INA228 | `b28c5ea69bcb4d26b351c52c24d3c6fae9fd282b`, clean | `_readDiagAlertTracked`, `_captureDiagAlert`, `readAndClearDiagAlert`, `test_public_read_diag_alert_is_destructive_and_preserved` and CLI probe callers demonstrate a real consuming sensor read. This supports inspecting read effects, not asserting a destructive CL86 read. |
| FieldCore-node | `5ad9292dc8481cf08be9a61f662495a6474d1da0`, clean | `Rs485MotorModule` status/result copies and runtime-connection generation/producer boundaries remain application-owned. Inspected read-only; no framework types or second receiver imported. |

Sibling suites were not rerun; this assignment changed no sibling files. The final
RS recheck found concurrent edits to `test/idf_consumer/README.md`,
`test/idf_consumer/main/main.cpp` and `test/installed_consumer/main.cpp`: explicit
Discovery header coverage and consumer settings/protocol checks. Those are
uncommitted observations, not attributed to HEAD; they change no inspected bench
or owner implementation and were left untouched. Other audited siblings remained
clean at the same revisions. Current local owner,
typed reads, fake controller, TWAI adapter, firmware assembly, build inventories
and actual CI were inspected. SDK/stack pins and license/export boundaries are
unchanged. All proof inputs stay outside the installed C++11 core.

Original manual pages were rendered and read alongside raw EDS sections:
EN86 PDF pp3,6,8,11-15,17-18; ZH86 pp12-13,18-19; OP86 pp6-7,10;
ZH57 pp8,17. The [conflict table](../device_conflicts.md) cites further applicable
pages and exact EDS sections for all Q01-Q21/C01-C24. Original hashes are unchanged.

Source-confirmed facts include the exact identity/configuration upload addresses
and widths, `6083/6084/609A` millisecond ramp units, and the conflicting status,
output-address and velocity instructions. EN86 status alarm7/zero10/effective12,
ZH86 alarm3/run14 and OP86 alarm7/run14 remain competing claims. EN86 DO polarity
`:0B`/functions `:0C..0E` conflicts with ZH86/EDS `:0C`/`:0D..0F`. EN86 velocity
enable-before-parameters conflicts with ZH86's opposite order. No firmware winner,
generic CiA402 substitute, safe zero-ramp meaning or internal chip was inferred.

No factual ledger correction was warranted and no physical fact was measured.
The canonical ledger/generated read policy therefore stays unchanged: all current
upload effects remain UNQUALIFIED. The new table connects every existing gate to
the exact next observation; it is not a second object dictionary.

## Delivered contracts and acceptance coverage

| Prompt requirement | Delivered artifact and observable boundary |
| --- | --- |
| 1. Ordinary bounded native/firmware harness | [DeviceEvidence](../../examples/common/DeviceEvidence.h) borrows ProbeApp and its single CanBusOwner. Identity/configuration and explicit state blocks use existing typed preparation, owner admission and concrete read progression. Firmware services the collector after the ordinary app/transport. No parser, raw sender, motion stub or second engine. |
| 2. Actual fixture facts | [Bench record](../hardware_bench.md) records user-reported PCAN IPEH-002022, ESP32-S2/TJA1051TK/3/1J, RX16/TX17/silent18 and ESP-Prog/2. S3 compile fixtures are separate. Actual drive/firmware, electrical/mechanical setup, node/bitrate and control provisions remain unknown. |
| 3. One conflict/measurement table | [Q01-Q21 table](../device_conflicts.md) links original pages/EDS, required raw and independent observations, affected capabilities and gates. Vendor agreement, host checks and physical observation have separate columns. |
| 4. Finite acquisition records | Copied plan/fixture/revision/dialect/authorization, exact target and initial field descriptors, explicit deadline/queue policy, all consumed owner terminals and 64 raw traffic/event entries. Counters, local service gaps and resource snapshots diagnose failures. No record claims runtime image attestation or physical semantics. |
| 5. Conditional experiment route | DEVICE ordinary requests reject Q20 before application work. `requestExperiment` admits only an exact reviewed finite typed read plan with source/independent/fixture/control/failure/reconciliation references. No new production permission follows. State-changing experiments remain individually specified, reviewed, implemented and fake-tested when their external prerequisites exist; the precise conditional procedure remains owned by 29. |
| 6. Evidence feedback | No new firmware agreement/measurement exists, so all canonical gates remain. Coverage, bench, source map, API map, backlog and roadmap distinguish readiness from acquisition and qualification. Later operations retain their assigned scope. |

`EvidenceEnvironment::HOST_FAKE` is fixed at construction for an offline fake,
not a per-request live permission switch. Firmware fixes DEVICE provenance.
The API cannot authenticate external authorization references or a caller's
claim that its transport is a fake; these remain explicit application contracts.
An unconfirmed service channel cannot bootstrap itself: local preparation can
succeed, but ordinary owner admission returns INVALIDATED with no transmission.

Construction, selection and cached record access are passive. Each acquisition
has a strictly increasing local ID and an immutable deadline. Inputs are copied
only after successful validation; rejection leaves the previous record intact.
Traffic collection drains diagnostic copies, never authoritative receive queues.
At most one owner terminal is copied and advanced per turn; all consumed terminals
remain in the record after the owner releases its slot. Terminal acquisition
records remain stable until explicit release. Late events remain owner history.

Trace overwrite, traffic-capacity exhaustion and reported missing ingress mark
evidence incomplete and withdraw unread work. `Stats::rxLost` counts explicitly
reported missing frames, separately from `rxDropped` which also includes captured
rejected envelopes. Snapshot counter decreases/saturation fail conservatively;
the caller must not clear statistics or drain diagnostics during acquisition.
Unobservable SDK loss is still limited by the actual adapter evidence contract.
Resource snapshots are not peak measurements; local collector service gaps are
not drive timing guarantees.

The [native example](../../examples/device_evidence/README.md) requires
`--fake-only` and prints identity/configuration results plus an explicitly reviewed
unknown-effect read timeout. It uses the ordinary existing fake application and
prints exact metadata, terminals, traffic/controller evidence and failure counters.
It does not open a controller and has no live argument or future CLI schema.

Independent review and regression work corrected these issues before handoff:

- Interruption after an already-successful field originally allowed the remaining
  fields to continue. The successful field now wins its own race and the unread
  remainder is cancelled before another application turn.
- Controller/recovery events and reported RX loss now survive the acquisition
  record; successful SDO bytes alone cannot hide a truncated/faulted capture.
- Shared cancellation rejects time older than the owner's authoritative clock.
- Conflict-table wording now permits an independently bounded experiment to
  investigate a declared unknown effect. It does not require manufacturer proof
  of that answer first, and still forbids blind trials of disputed DO addresses.
- The firmware build-checker test now omits its named source explicitly rather
  than relying on a source-list position changed by the added collector.

`gpt-6.1-sol/high` owned disjoint adversarial harness tests; another owned sibling
audit, the native example and firmware integration tests. `gpt-6-astra/high`
independently checked original conflicts, gates and final ownership/uncertainty.
The lead owns central mutations, integration, final review and Git. Final
independent code review found no remaining contract defect within this scope.

## Verification

Windows: Python 3.12.10, GCC 15.1.0, CMake/CTest 4.0.1, Ninja 1.12.1,
Doxygen 1.13.2; SDK/framework pins unchanged from prompt 28.

| Check | Result and retained evidence |
| --- | --- |
| Focused acquisition suite | 1,468 checks: literal identity/config/state bytes, metadata rejection, passive startup/cache, retained success/prefix/abort/timeout, stale route/reply, boot-up, interruption before/after TX and after successful field, trace/RX loss, time bounds, no replay and exact experiment admission. |
| Firmware integration suite | 910 checks including automatic collector progression, DEVICE Q20 rejection, exact reviewed fake acquisition, clock fault and one owner path. |
| Full verifier | `python scripts/verify.py --mode full --build-dir .build/p29-full`: 52/52 registered contracts plus separate package and strict Doxygen passed. Run `20261102T100538724606Z-79444-orflhr6a`; Linux sanitizers are not available on this Windows host. |
| Isolated package consumers | Installed core still contains 22 public headers and no SDK/stack/Python/research dependency. Copied application/fake harness builds and runs against the installation without the source checkout. |
| Arduino | `python scripts/check_firmware.py --target arduino --pio C:/p55s/penv/Scripts/pio.exe --build-dir .build/p29-arduino`, with `PLATFORMIO_CORE_DIR=C:/p55s`: passed, run `20261102T100606743515Z-72520-4gapb5g6`; 19 shared and nine core sources verified. Static RAM 62,920 bytes, application flash 359,773 bytes; no runtime/stack peak measurement. |
| ESP-IDF | `python scripts/check_firmware.py --target idf --idf-python ../MotorControl-RS/build/p25/idf-python/Scripts/python.exe --build-dir .build/p29-idf`, using the pinned prompt-28 SDK/tool environment: passed, run `20261102T100643938821Z-70692-m1ugaui3`; firmware ELF/BIN linked, actual inputs verified. |
| Firmware inventory regression | `python test/firmware_build_test.py`: 26/26 after the final named-source correction; missing DeviceEvidence is now independently rejected. |

Full verification ran on the completed code over entry HEAD plus task changes;
subsequent edits were documentation and the focused build-inventory check above.
No hardware, real-time load, physical CAN/drive behavior or supported firmware
dialect was tested. Final integrity/diff checks and exact-commit CI/sync are
reported at handoff; a previous green commit is not their substitute.

## Remaining evidence and next work

Establish the actual fixture/session and applicable acquisition authorization.
Resolve Q20 for the chosen non-changing reads, or independently review one exact
unknown-effect read experiment and its external control/observation provisions.
Acquire identity/raw configuration first, preserve failed evidence, and never
equate an SDO response with safe effects or motion completion. For a fact that
requires mutation, follow the [conditional prerequisites](../device_conflicts.md#conditional-experiment-prerequisites)
and add only that isolated typed sequence when its actual candidate/fixture exists.
No state-changing placeholder is needed now.

Prompts 18/19/20-22 and motion/commissioning/persistence gates keep their original
owners. No completion of those features, future CLI/Python runners or 47-48
qualification is implied by this software-readiness delivery.
