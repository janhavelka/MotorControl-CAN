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
| `1`, `2` / `identity1`, `identity2` | Typed nine-field identity plan, selecting `MEASURED_U8` because `DECLARED_U16` fails on these specimens |
| `3`, `4` / `raw1`, `raw2` | Upload-only raw experiment on node 1/2, with the fixed 26-field inventory below |
| `v`, `b` / `motion-readiness1`, `motion-readiness2` | Separate finite 30-field upload-only readiness inventory on node 1/2; no motion command |
| `h`, `j` / `heartbeat1`, `heartbeat2` | Separate exact1017 experiment: require zero, write1000ms, read back; one attempt per node |
| `q`, `w` / `preop1`, `preop2` | Explicit addressed NMT Pre-operational through the same owner/TWAI result path |
| `n` / `network` | Two cached node snapshots and at most eight copied network events; no traffic |
| `m` / `tpdo1` | Node1 only: wait for qualified Pre-operational, nine exact baseline reads, six fixed mapping writes, nine exact final reads |
| `d` / `dump` | Reprint the retained record, without another upload |
| `r` / `release` | Release after output and local TX/cleanup retirement; never reopen or retry |
| `S...` / `scripts/s2_soak.py` | Finite communication soak, below; the one request longer than a byte |
| `X` / `soak-stop` | Stop a running soak after any started read settles |
| `T...` / `scripts/s2_bench.py --command readiness-actions` and the other routes | One named attended route, below |
| `A` / `trial-abort` | Withdraw the active route locally; sends no stop |

While a soak runs, only `p`, `s` and `X` are served; anything else answers
`SOAK_REFUSED reason=active`. While a route's record is held, only `p`, `s`, `d`, `r` and `A`
are served; anything else answers `TRIAL_REFUSED reason=active`. A response that finds the console queue full waits in
one retained buffer, and no further command is taken until it has gone.

The raw inventory is `1000:00`, `1009:00`, `100A:00`, `1018:00..03`,
`2000:00`, `2001:00`, `1800:01/:02/:03/:05`, `1A00:00..04`, `6041:00`
`6061:00`, `1017:00`, `2100:00`, `603F:00`, `6064:00`, `2201:00` and `606C:00`. Each inventory upload has four-byte capacity, server-indicated actual size
and unknown read effects. There is no arbitrary object selector or write sender.
The separate readiness inventory is `1001`, `603F`, `6041`, `6061`, `2100`,
`2101`, `6064`, `606C`, `607A`, `6081`, `6083`, `6084`, `605D`, `60FF`,
`2200`, `2201`, `2202`, `607D:01/:02`, `2500`, `2501`, `2400:01..08` and
`60FD`. It retains exact server-indicated bytes and unknown read effects;
it does not establish a safe enable predecessor, motion scale or physical
input state. It has its own one-attempt latch per node, so it may follow a
released baseline inventory without repeating it.

Queue/response budgets are 250 ms and the inventory/heartbeat deadline is 5 seconds; the PDO deadline is 8 seconds, all
host policies. The fixture requires exclusive ownership and an externally
established fresh SDO server session. Restarting the MCU does not establish a
fresh remote session after an uncertain transfer.

The raw collector uses the ordinary owner with one exact one-use adapter grant
per field. The typed collector uses `DeviceEvidence::requestExperiment`; both
retain external review references and unknown read effects. Neither changes
the generated read policy or establishes production service qualification.
One accepted inventory consumes the node's inventory latch even on failure, shared
between its typed and raw controls. Heartbeat and PDO experiments have separate
one-attempt latches and increasing exact grant/operation identities. Release does not remove uncertainty,
reopen an SDO channel or permit repetition of that experiment.

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

Output is bounded and does not delay CAN service. Raw history holds 30 terminals,
112 traffic copies and a separate 16-entry post-terminal tail. The collector waits
for each preceding accepted TX's final report before granting another field;
the deadline stays fixed and no refused grant is replayed. Its first terminal
facts remain frozen while the tail can retain later reports/cleanup. The earlier
typed width-mismatch captures have no such tail and are explicitly incomplete
continuous bus captures.

The original 8192-byte stack was insufficient for the compiled typed acquisition
call chain. Preflight now uses owned record scratch, and the task has 12288 bytes.
The [live report](../../docs/reports/2026-11-02_s2_live_connection.md) retains the
failed reset, compiler stack facts, pacing defect and corrected runs separately.

The earlier connection checkpoint
[node-1](../../docs/bench/20261102-s2-raw-fixed-node1/rx.txt) and
[node-2](../../docs/bench/20261102-s2-raw-fixed-node2/rx.txt) records each contain
20 successful raw uploads. Both indicate one byte for `1009=0x0C` and
`100A=0x20`, versus the retained EDS U16 widths. The default typed identity path
(`DECLARED_U16`) rejects the width mismatch; an application that has established
its fixture selects `IdentityWidths::MEASURED_U8` explicitly, and nothing detects
it. Raw success does not revise the EDS metadata or establish a human firmware
version. Raw `6041/6061` values establish neither
operation-mode meaning nor physical standstill.
The [post-run counters](../../docs/bench/20261102-s2-raw-fixed-stats/rx.txt)
report 40 RX/40 TX, zero errors/drops and 9852 bytes minimum remaining stack.
These runs establish the computer/probe/UART/CAN/two-drive connection, with
unknown read effects. No PDO write or motor action was sent in that earlier
checkpoint. The later [NMT/TPDO checkpoint](../../docs/reports/2026-11-02_nmt_pdo_checkpoint.md)
records heartbeat acquisition, addressed Pre-operational and the fixed TPDO1
map change with complete readback. Prompt18 is Partial; broader PDO and
motor/electrical/sustained qualification remain pending.

The later user physical power cycle and fresh readback found node1 back at
producer0/map3 without save/restore, for that one cycle only. Both nodes then
completed 30 readiness uploads. Nonzero retained `607A=5000`/`60FF=120`, mode0
and configured DI functions remain admission questions; no motion command or
blind enable sequence is implemented. No input assignment was changed.

The heartbeat experiment has three exact U16 services and stops if the baseline
is not zero. Its one download changes producer timing; this does not configure
a drive host-loss response. The effective producer fact becomes unknown before
possible TX and is published only after matching final readback. No automatic
restore or save occurs. The 1000ms value is an explicit experimental choice.

NMT uses a tagged owner transport identity and retains its own result. TX
confirmation does not confirm the remote transition. Release the result with
`r` after retaining it, then inspect `n`; a fresh post-handoff heartbeat is
required. No NMT reset/start/stop is hidden in any command.

The fixed PDO experiment is documented in
[the reviewed procedure](../../docs/reports/2026-11-02_pdo_motion_plan.md).
Its scope remains `ISOLATED_EXPERIMENT`. It preserves COB-ID, type255, raw
inhibit100, event0 and inactive slots, changes the active map from status/mode/
position to status/position, and stops at the first failure. Every queued
service carries a Pre-operational prerequisite rechecked through SDK submission.
Partial or uncertain changes are retained without rollback. Complete readback
confirms contents only; samples remain unqualified. No motion is implemented
by this harness.

The S2 reports exact capture as unknown and retains separate conservative
ingress bounds. See [the timing proof](../../docs/adapters/twai_receive_time.md).
A processing timestamp is never relabeled as arrival. A boot-up is recorded
only when an actual matching `00` event is received; absence of such an event
is not proof of a fresh server session. Network history and serial output are
bounded, with loss/truncation reported explicitly.

## Communication soak

`S` followed by `TRIAL,DURATION_S,INTERVAL_MS,SUMMARY_S,READS` and a line end starts one
finite soak. [`Soak`](Soak.h) runs in the owner task through the ordinary `ProbeApp` typed
reads and the sole owner, one finite operation at a time, alternating nodes 1 and 2. It sends
no write, NMT command or controlword, and has no raw sender or second receiver. Startup stays
passive: nothing runs before the request, and the routes are configured by the request with the
same exclusive-session premise as the other procedures. Details, record grammar and the runner
are in [hardware_bench.md](../../docs/hardware_bench.md#communication-soak).

`Soak` is allocated once, from internal heap, on the first request (10752 bytes on S2), because the
linker's `dram0_0_seg` limit has little room left. `SOAK_REFUSED reason=memory` reports a failed
allocation, and a build that never soaks spends nothing. Free heap is reported in `SOAK_BEGIN`
and every `SOAK_GAP`.

## Attended bench routes

`T` followed by a route letter and its fields starts one finite route of
[`BenchRoute`](BenchRoute.h): the packet 13, 14 and 15 reads, one `6060:00=1`, the session's
enable and stop, the isolated packet 19 position experiment, its rearm and the packet 20 move. The
line is drained whole each turn, because it can be longer than the platform's 64-byte input ring,
and it ends in `,K` and the CRC-32 of every byte before it, which the firmware checks before parsing
(`TRIAL_REFUSED reason=crc`). `BenchRoute` and one `AxisSession` per node are allocated from internal
heap on first use and never freed; `PONG` gains `routes=3` (this line protocol), `trial` and
`motion`. Routes, prerequisites, records and the runner are in
[hardware_bench.md](../../docs/hardware_bench.md#attended-bench-routes). `MotionPreview` stays an
offline review artifact, and its `executionReady` stays false.
