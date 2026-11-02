# Offline device evidence procedure

This C++11 executable exercises the finite `DeviceEvidence` collector through
the existing `ProbeApp`, `ProbePlatform`, `CanBusOwner` and `FakeTransport`.
Only `--fake-only` is accepted. It opens no port or controller and needs no SDK,
board, transceiver, drive, motor or sibling repository.

From the repository root, with a native compiler and the retained CANopenNode
sources:

```sh
cmake -S . -B build/evidence -DBUILD_TESTING=OFF -DMOTORCONTROLCAN_BUILD_EXAMPLES=ON -DMOTORCONTROLCAN_BUILD_CANOPENNODE=ON -DMOTORCONTROLCAN_CANOPENNODE_DIR="$PWD/experiments/canopen_service_fit/vendor/CANopenNode"
cmake --build build/evidence --target device_evidence
./build/evidence/examples/device_evidence/device_evidence --fake-only
```

PowerShell accepts the same `$PWD` interpolation. Windows adds `.exe`; select
`-G Ninja` for GCC/Ninja, or append `--config Debug` and use the `Debug` executable
directory for a multiple configuration generator. Unknown or missing arguments
exit with code 2. Procedure failure exits with code 1 and leaves its printed
prefix available to the caller for retention. Redirect stdout/stderr to a new
evidence path when retaining a run; the program creates no files.

The fixed fixture uses logical axis 9, profile token 1, node 5, epochs 1,
SDO IDs `0x605/0x585`, heartbeat `0x705` and EMCY `0x085`, taken from the existing
fake helper. These are synthetic route choices. There are no wiring, electrical
or live controller facts. The 40 hexadecimal `softwareRevision` digits are a
fixed synthetic procedure label, **not this checkout revision or running-image
attestation**. A real acquisition must supply its actual software revision and
fixture/authorization records; this executable supplies no physical admission.

Startup and target selection are passive. An ordinary acquisition using the
default `DEVICE` environment first expects `READ_EFFECT_UNQUALIFIED` rejection
without a CAN handoff. An ordinary `DEVICE` plan is admitted only when every
object is on the owner's DEC-1 accepted list (risk acceptance, not evidence). A separate `HOST_FAKE` collector explicitly acquires nine identity fields
and six communication configuration fields. Each turn services the ordinary
application first and the collector second. One owner receives all frames,
including interleaved heartbeat/EMCY and unrelated traffic. Invented identity
values leave `modelMatched=false`; configuration readback adopts no routes.

A separate silent fake explicitly admits an unknown-effect read experiment.
Its review references describe synthetic premises only. One upload receives
synthetic controller transmission confirmation but no SDO response. The immutable
100 us response bound ends the acquisition with one retained timeout and uncertain
effects; it performs no retry or subsequent identity field. The whole-acquisition
bound is 1000 us and the caller has at most 1100 turns. These are host fixture
policies, not vendor timings. This exercise grants no permission to run a physical
experiment and establishes no disputed firmware behavior.

Output is bounded human-readable evidence, without a future console/JSON schema.
It prints acquisition metadata, every retained native terminal and its raw reply,
plus every copied RX/TX/TX-report/controller/recovery record. Initial/final owner
statistics and resource snapshots retain their distinct meanings; observed owner
and collector service gaps are not device deadlines or physical timing measurements.
Resource snapshots do not measure allocation peaks or stack use. Successful identity/configuration produce
15 upload terminals; the separate failed experiment produces one. The collector
owns at most 15 full terminals and 64 traffic entries per acquisition. Loss is
reported explicitly and fails this procedure. Raw successful bytes establish only
the checked synthetic service result. No read-effect, supported-model, status-bit,
scale, motion, physical stop or electrical claim follows.

See [hardware bench requirements](../../docs/hardware_bench.md) and the
[conflict gates](../../docs/device_conflicts.md) before preparing any real fixture.
