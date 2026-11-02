Execute this packet fully. Apply [COMMON.md](COMMON.md).

# Packet 11 Bounded HIL diagnostics and repeatable evidence

Prerequisite: packet 10 has verified all W1-W8 obligations. This packet owns prompt 29's diagnostic tooling and fresh baseline acquisition. It does not implement enable or motion. The user authorizes physical tests, diagnostic improvements and deliberate retesting within this handoff's fixture and bounds. The lead is the sole hardware operator; workers never open a port.

## Read and inspect

Read original `docs/prompts/29_device_evidence.md`, its report, `docs/hardware_bench.md`, `docs/device_conflicts.md`, and the three reports `2026-11-02_s2_live_connection.md`, `2026-11-02_nmt_pdo_checkpoint.md`, `2026-11-02_pdo_motion_plan.md` under `docs/reports/`. Recheck the affected original manuals/EDS through `docs/reference/SOURCES.json`.

Inspect actual `examples/s2_bench/{RawEvidence,PdoExperiment,MotionPreview}.{h,cpp}`, `examples/s2_bench/main/main.cpp`, `scripts/s2_bench.py`, `examples/common/{DeviceEvidence,FirmwareProbe,ProbeApp,ProbePlatform}.*`, the owner moved by packet 2, `adapters/twai/{TwaiAdapter,IdfTwaiDriver}.*`, and `adapters/canopennode/{SdoClient,Network,Nmt}.*`. Follow the current declarations after repairs. Read `docs/adapters/twai_receive_time.md`; processing time is not arrival time.

Compare RS `scripts/bench_session.py`, `_bench_evidence.py` and `bench_probe.py` for port ownership, retained errors and correlation; inspect ADS1115 contract-versus-evidence checks and PCA9555's dry-run guard. Use FieldCore's copied request/result and invalidation boundaries named in COMMON; it provides no CAN runner.

## Implement the diagnostic path

1. Preserve one application owner and one CANopen engine. Extend the existing finite evidence path with named, checked procedures, not an arbitrary object sender or command language. Construction, startup, cached inspection and plan preview stay passive. A record identifies the concrete requested procedure; a captured UART response is not an accepted operation.
2. Make build/upload reproducible using tracked scripts and documentation. Do not depend on `.build/flash_s2_bench.py` or other ignored helpers. Discover installed pinned tools; validate USB identity, board target and generated flash arguments. Retain revision/dirty-source hashes, compiler/SDK versions, ELF/binary hashes, configuration and flash log. Verify the running image using `ping`. Do not erase all flash, change eFuses, download vendor executables or upgrade toolchains to make this work.
3. Add an offline evidence parser and its registered tests. Replay retained `docs/bench/20261102-final-node1/rx.txt` and node 2: thirty successful field terminals and ninety traffic records each. Validate request/run/operation/step identities, node/channel/epochs, address/subindex/direction, actual width and bytes, terminal counts, start/end completeness and separate failed evidence. Preserve the earlier one-byte `1009/100A` discrepancy; never turn raw capture into a typed-width success.
4. Record owner queue admission, adapter handoff, SDK submission, TX report, RX bounds/processing, terminal publication and retirement distinctly. Include controller state/errors, SDO abort, native status/mode/monitor/fault values, queue high-water marks, service gaps, trace/RX/console loss and stack headroom. Add only instrumentation needed to distinguish hypotheses. Copy traffic without stealing RX or blocking service; keep bounded records and a terminal tail. Output pressure cannot cancel an in-flight transaction merely to save the log.
5. Make intentional repetitions new explicit finite trials with new identities and new artifact directories. Do not clear one-use latches or restart the MCU to erase uncertainty. Require old TX retirement, usable route/session, retained prior result and current fixture state. For baseline read trials set a maximum of three trials per node and sixty seconds per trial, with tighter operation deadlines. Apply the accepted-read versus explicit-single-read rules from packet 4; do not continuously poll fault/history fields.
6. Preserve first failure and uncertain effects. Diagnose the earliest divergence: host parser/capture, wrong image, UART/reset, scheduling/deadline, SDK submission, bus error, correlation, drive abort or device semantics. A longer timeout or later pass is not a cause. Add a native regression when representable, fix the implicated layer, rerun the same bounded case, then a short unaffected case. When electrical or independent observations are missing, state the exact measurement instead of inventing a culprit.

## Run the baseline

Refresh the actual fixture before opening it. Recorded identity is ESP-Prog2 COM15, USB serial `94A990398B40`, VID/PID303A:1002; ESP32-S2, UART43/44, CAN RX16/TX17/silent18, nodes1/2 at 1 Mbps. These are recorded facts, not discovery defaults. Confirm the continuing free-shaft/table-mount/power-cutoff conditions. No worker controls the second node.

Use the pinned IDF environment and a short Windows build path:

```text
python scripts/check_firmware.py --target s2 --idf-path <IDF> --idf-python <IDF_PYTHON> --build-dir .build/h11
idf.py -C examples/s2_bench -B .build/s2-bench-live -DIDF_TARGET=esp32s2 -DSDKCONFIG=<ABSOLUTE_BUILD_PATH>/sdkconfig build
```

From that generated build directory, after identity checks, use its arguments:

```text
python -m esptool --chip esp32s2 --port <VERIFIED_PORT> --baud 115200 --before default_reset --after hard_reset write_flash '@flash_args'
```

The existing bounded capture entry is:

```text
python scripts/s2_bench.py --port COM15 --serial 94A990398B40 --command ping --seconds 2 --output <NEW_DIRECTORY>
```

Run cached inspection, explicit local CAN start, then the reviewed typed/readiness procedures one node at a time. Preserve exact read effects and stop on the first failure. No heartbeat/PDO write, NMT, enable or target change is needed for this baseline. The current task stack is 12288 bytes; the historical8192 figure and read-only stackFree9580 do not qualify later action call chains.

## Drain-premise evidence owned here

Packet 5's software marker tests do not establish remote-server ordering. Own
that physical evidence question here: inspect the applicable protocol/server
source and current sole-client, retired-TX and distinct-object premises before
planning one finite no-motion accepted-read/marker case. Use copied traffic and,
if available and verified listen-only, an independent observer. Host delayed
processing is not delayed server response. Do not induce an uncertain write to
manufacture this test or assert universal ordering from one passing exchange.
If the necessary remote premise cannot be established, retain Q18 and actual
route reopening as unsupported for this fixture, with the precise missing
observation/source and next test in packet 11's report. Diagnostics and unrelated
channel tests can finish; no later packet may silently count this gate closed.

## Verify and hand off

Test truncated/duplicated/wrong-run output, stale frames, abort, timeout, reboot, uncertain submission, trace saturation and serial backpressure using fakes. Run `raw_evidence`, `device_evidence`, `network_receive_time`, `firmware_probe`, TWAI/owner/service suites and new parser tests. Run the full verifier with `.build/h11v`, firmware checks affected by tooling, integrity and diff checks. Preserve raw artifact hashes in `docs/bench/SHA256SUMS.json`; check staged and committed bytes.

Delegate parser/fixture tests separately from source/timing review. The lead integrates and runs hardware. Record actual versus unperformed checks in `docs/reports/handoff_19_33/packet_11.md`, update prompt 29's report and affected coverage/docs, then commit and sync the same execution branch. Diagnostics readiness does not close a firmware conflict by itself.
