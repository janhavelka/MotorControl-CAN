# ESP32-S2 board evidence harness

This is the OPTAK_FGI_2025 fixture, with ESP-Prog2 UART0 on GPIO43/44,
TJA1051 CAN TX17/RX16 and active-high silent GPIO18. The reported drive nodes
are 1 and 2 at 1 Mbit/s. These are explicit fixture facts, not library defaults.
The normal installed core, application, receive owner and CANopenNode adapter
are shared with the other examples. P4 is not supported by this firmware.

Build with the pinned ESP-IDF 5.5.5 tool environment:

```text
python scripts/check_firmware.py --target s2 --idf-path PATH_TO_IDF --idf-python PATH_TO_IDF_PYTHON
```

This checks compilation and never flashes. Add `--temporary-root C:/Temp` when
that existing short path is needed for Windows compiler dependency files. For
a retained local build, from an exported IDF shell, use:

```text
idf.py -C examples/s2_bench -B .build/s2-bench-live -DIDF_TARGET=esp32s2 -DSDKCONFIG=ABSOLUTE_BUILD_PATH/sdkconfig build
```

Use the generated flash arguments with esptool, explicitly selecting the
identified board's port, S2 target and 115200 baud. The actual board's faster
460800-baud backup read failed; do not infer that speed is reliable.
No eFuse, full-flash erase or drive reset is part of the upload command.

On startup S18 is set high and CAN stays disabled. A console request is one
fixed byte; this is an evidence procedure, not the future production CLI or a
raw CAN sender. `p` returns image identity, `c` initializes the local controller
once and lowers S18, and `s` returns cached counters and stack headroom. Normal
CAN mode acknowledges received frames after `c`. It sends no device request
until an explicit acquisition command. There is no retry or automatic recovery.

| Control | Explicit finite behavior |
| --- | --- |
| `p` / runner `ping` | Cached image/base/SDK identity and local startup facts |
| `c` / `start` | One local CAN initialization attempt; lower S18 on success, no device request |
| `s` / `stats` | Cached controller/owner counters and task stack high-water mark |
| `1`, `2` / `identity1`, `identity2` | Strict typed nine-field identity plan; retained EDS widths apply |
| `3`, `4` / `raw1`, `raw2` | Upload-only raw experiment on node 1/2, with the fixed 20-field inventory below |
| `d` / `dump` | Reprint the retained record, without another upload |
| `r` / `release` | Release after output and local TX/cleanup retirement; never reopen or retry |

The raw inventory is `1000:00`, `1009:00`, `100A:00`, `1018:00..03`,
`2000:00`, `2001:00`, `1800:01/:02/:03/:05`, `1A00:00..04`, `6041:00`
and `6061:00`. Each upload has four-byte capacity, server-indicated actual size
and unknown read effects. There is no arbitrary object selector or write sender.
Queue/response budgets are 250 ms and the overall deadline is 3 seconds, all
host policies. The fixture requires exclusive ownership and an externally
established fresh SDO server session. Restarting the MCU does not establish a
fresh remote session after an uncertain transfer.

The raw collector uses the ordinary owner with one exact one-use adapter grant
per field. The typed collector uses `DeviceEvidence::requestExperiment`; both
retain external review references and unknown read effects. Neither changes
the generated read policy or establishes production service qualification.
One accepted plan consumes the node's session latch even on failure, shared
between its typed and raw controls. Release does not remove uncertainty,
reopen an SDO channel or permit another acquisition on that session.

The finite serial runner verifies the ESP-Prog2 USB identity, sets neither EN
nor BOOT, sends one byte and saves raw output plus request metadata:

```text
python scripts/s2_bench.py --port COM15 --serial 94A990398B40 --command ping --seconds 2 --output NEW_EVIDENCE_DIRECTORY
```

For an admitted raw experiment, the operator uses `start`, `raw1`, `release`,
`raw2` and `stats` as separate requests with a new output directory each time.
For example, the already reviewed node-1 request was:

```text
python scripts/s2_bench.py --port COM15 --serial 94A990398B40 --command raw1 --seconds 5 --output NEW_NODE1_EVIDENCE_DIRECTORY
```

This documents the procedure; another run still needs its actual reviewed
fixture/session and finite experiment. The runner never resets the MCU,
automatically retries a write or interprets captured text as command acceptance.

Use only the confirmed port/USB serial for this fixture. Every output directory
must be new. Capturing bytes is not an assertion that the firmware accepted the
request. Compare the reported ELF hash with the build record. The Git revision
is the build's base commit; retained source hashes identify any working changes.

Output is bounded and does not delay CAN service. Raw history holds 20 terminals,
96 traffic copies and a separate 16-entry post-terminal tail. The collector waits
for each preceding accepted TX's final report before granting another field;
the deadline stays fixed and no refused grant is replayed. Its first terminal
facts remain frozen while the tail can retain later reports/cleanup. The earlier
typed width-mismatch captures have no such tail and are explicitly incomplete
continuous bus captures.

The original 8192-byte stack was insufficient for the compiled typed acquisition
call chain. Preflight now uses owned record scratch, and the task has 12288 bytes.
The [live report](../../docs/reports/2026-11-02_s2_live_connection.md) retains the
failed reset, compiler stack facts, pacing defect and corrected runs separately.

The [node-1](../../docs/bench/20261102-s2-raw-fixed-node1/rx.txt) and
[node-2](../../docs/bench/20261102-s2-raw-fixed-node2/rx.txt) records each contain
20 successful raw uploads. Both indicate one byte for `1009=0x0C` and
`100A=0x20`, versus the retained EDS U16 widths. The strict typed identity path
correctly rejected the width mismatch; raw success does not revise that metadata
or establish a human firmware version. Raw `6041/6061` values establish neither
operation-mode meaning nor physical standstill.
The [post-run counters](../../docs/bench/20261102-s2-raw-fixed-stats/rx.txt)
report 40 RX/40 TX, zero errors/drops and 9852 bytes minimum remaining stack.
These runs establish the computer/probe/UART/CAN/two-drive connection, with
unknown read effects. No PDO write or motor action was sent; Prompt 18 and
broader motor/electrical/sustained qualification remain pending.
