# Packet 2: Reusable owner placement and shared test fixtures

Execute this packet fully. Apply [COMMON.md](COMMON.md). Prerequisite: packet 1.
Execute W2/C-08/C-11/C-12 from the active audit. This is a mechanical boundary
repair, not a new scheduler or service engine.

## Inspect and implement

Read `examples/common/CanBusOwner.{h,cpp}`, `CanBusDiagnostics.cpp`,
`OwnerCounters.h`, `CMakeLists.txt`, `examples/common/CMakeLists.txt`,
`examples/common/firmware_sources.json`, `adapters/twai/TwaiAdapter.h`,
`scripts/check_firmware.py` and package/repository checker allowlists. Trace
native_probe, device_evidence, s2_bench and firmware callers. Preserve the new
tagged SDO/NMT transport identities and receive bounds from `c643af8`.

Move the actual owner/diagnostics/counters into `adapters/owner/`, namespace
`MotorControlCANOwner`, with optional target `MotorControlCAN::CanBusOwner` as
DEC-5 specifies. Keep the installed pure core independent of stack/SDK headers.
ProbeApp, console, board code and framework entrypoints stay in examples.
Keep the pinned CANopenNode copy in place. Update every include, source inventory,
link target, package consumer, checker and maintained guide that names the old
location. Historical reports remain historical; add navigation rather than
rewriting immutable snapshots.

After the move passes its checks, commit/sync that complete mechanical change.
Then extract cohesive transport/request values into `CanBusTypes.h` and only
genuinely shared fake-owner plumbing into `test/support/OwnerRig.h`. Keep scenario
differences explicit in tests; no test DSL, compatibility wrapper tree or duplicate
owner. This second checked refactor can be a separate checkpoint.

Refresh RS owner placement/callers and ADS/INA transport boundaries. Record the
intentional CAN optional-owner target without copying RS transport assumptions.
Update the handoff's source map so later packets use actual new paths; references
to old paths in these instructions mean the baseline, not a request to recreate
the old files.

## Delegation and acceptance

One implementer owns the move, namespace and build registration. A worker audits
all references/consumers read-only; another reviews fixture extraction after the
move is stable. No concurrent owner writer and no hardware access.

Run the full verifier, isolated installed consumers and affected S2/S3/Arduino
firmware checks. Verify NMT, PDO experiment, raw evidence, owner/recovery/health,
TWAI and firmware tests still run with identical behavior. Confirm a copied
optional application can link using declared targets, while a minimal installed
core requires no examples, SDK, sibling or network. Do not accept a source-tree
include fallback as package success. Record migration in CHANGELOG and guides.
Finish packet 2 report and required exact-commit CI/sync under COMMON. Exit with
one real reusable owner and unchanged runtime semantics.
