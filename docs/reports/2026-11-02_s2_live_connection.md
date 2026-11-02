# ESP32-S2 upload and drive connection evidence

Date: 2026-11-02. Branch `promptset/motorcontrol-can`, entry `c329c7e`.
The user explicitly requested firmware upload, board communication, drive
connection testing and resumption of Prompt 18. The lead alone operated COM15.
No drive enable, target, stop, reset, save or PDO mutation was sent.

## Observed path

ESP-Prog2 `303A:1002`, serial `94A990398B40`, is COM15. Automatic EN/BOOT entry
identifies ESP32-S2FH4 revision 1.0, MAC `dc:b4:d9:b5:a6:ce`, embedded 4 MB flash,
no embedded PSRAM and a 40 MHz crystal. This is UART programming evidence, not
a JTAG session or a scope measurement of the reset lines.

The complete original 4 MB flash was backed up locally before writing. A first
460800-baud read failed with corrupt data (expected 4096 bytes, received 4032).
The subsequent 115200-baud read succeeded: 4,194,304 bytes in 373.9 seconds.
Both logs and the backup remain in `.build/s2-bench-live/`; the failed read is
not treated as a successful backup. No full erase or eFuse operation occurred.

The retained uploads verified the flash hashes and reset through RTS. Firmware ping
returned its target, SDK, base revision and ELF hash. Startup kept CAN disabled
and S18 high; explicit start succeeded at TX17/RX16, S18 low, 1 Mbit/s.
The [first build](../bench/20261102T142524Z-s2-upload/build.json) and
[corrected build](../bench/20261102T143234Z-s2-upload/build.json) retain exact
images, actual compiler-input hashes and dirty state. Another session committed
only the unrelated promptset audit as `d77be38` during this work; it was inspected
and preserved. Image base revision is not a claim of a clean source snapshot.

| Node | Request/response identifiers | Actual retained observation |
| --- | --- | --- |
| 1 | `601` / `581` | `1000:00` reply `43 00 10 00 92 01 04 00`; `1009:00` reply `4F 09 10 00 0C 00 00 00` |
| 2 | `602` / `582` | Same object values and reply forms, on this node's independent IDs |

These matched exchanges demonstrate the functional computer/probe/UART/firmware/
CAN/drive path for both reported addresses. They do not certify cable/reference
ground layout, termination resistance, power transients, motor wiring, JTAG or
sustained-load margins. Both identity runs observed no CAN error callbacks or
RX drops. Raw TX/RX and terminal evidence is retained in the
[node 1](../bench/20261102-s2-identity1-fixed/rx.txt) and
[node 2](../bench/20261102-s2-identity2/rx.txt) captures.

`1000` returned raw U32 `0x00040192`. It is not automatic model identification.
`1009` returned an explicitly indicated one-byte `0x0C`, contradicting the
supplied EDS's U16 wire width. The strict typed read correctly terminated with
local client abort `0x06070010`, retaining possible/uncertain read effects.
This is not a server-generated abort or a valid completed identity block.
The original EDS and production typed width remain unchanged. A second request
after release was refused, and ping continued to respond.

After both attempts the owner reported RX=4, TX=6, dropped=0, bus errors=0.
Each terminal trace contains only two request frames: the later client-abort
cleanup is outside the frozen acquisition prefix. Consequently these records
are explicitly not complete continuous bus captures. No silent replay or
channel reopening followed the mismatch.

## Complete bounded raw acquisition

The user subsequently confirmed both labels **CL86-C**, independent drive-power
cutoff and free shafts bolted to the table, then power-cycled both drives.
The corrected [raw image](../bench/20261102T152627Z-s2-upload/build.json)
completed 20 explicit uploads per node. Full records are
[node 1](../bench/20261102-s2-raw-fixed-node1/rx.txt),
[node 2](../bench/20261102-s2-raw-fixed-node2/rx.txt) and
[final counters](../bench/20261102-s2-raw-fixed-stats/rx.txt).
No download, NMT command or motion action was sent.

| Object | Both fixtures, unless distinguished | Interpretation limit |
| --- | --- | --- |
| `1000:00` | U32 `0x00040192` | Raw device type, not model autodetection |
| `1009:00`, `100A:00` | U8 `0x0C`, U8 `0x20` | Actual indicated widths contradict EDS U16; no established human version conversion |
| `1018:00` | U8 `5` | Exceeds EDS maximum 4/default 3; no undocumented child was probed |
| `1018:01/02/03` | U32 `0x1000`, `1`, `0` | Shared values do not uniquely identify a specimen |
| `2000:00`, `2001:00` | U16 `0x0400`, `0x0200` | Manufacturer driver/software fields; no invented encoding |
| `1800:01` | U32 `0x181` / `0x182` | Actual TPDO1 identifier per node |
| `1800:02/03/05` | U8 `255`, U16 `100`, U16 `0` | Current type/inhibit/event values, not factory defaults or proven timing effects |
| `1A00:00` | U8 `3` | Three currently active mapping entries |
| `1A00:01..04` | `60410010`, `60610008`, `60640020`, `0` | Status/mode/actual-position layout differs from EDS status/digital-input/mode template |
| `6041:00`, `6061:00` | U16 `0x8037`, U8 `0` | Raw observations; no ready/standstill/NMT-state conclusion |

Each record contains 20 requests, 20 matched sized replies and 20 final TX
reports: 19 reports in its frozen prefix and one in its separately retained tail.
The initial header is a snapshot before that final report; count the actual
TAIL line. The subsequent firmware adds final tail counts to END output.
Owner totals were 40 RX/40 TX, zero reported errors/drops, maximum collector
service gap 2004 us and minimum remaining task stack 9852 bytes on this raw path.
The SDK cannot prove absence of hardware-FIFO loss; these are reported counters,
not a sustained-load guarantee. Capture time is unknown; times are owner
processing observations, not physical wire timestamps.

The first raw image reached only `1000`: its matching reply was processed before
the final TX report, then the collector attempted the next admission too early.
The [failed record](../bench/20261102-s2-raw-node1/rx.txt) is retained. The fix
waits for authoritative local TX retirement within the original deadline before
granting the next upload. It does not retry. Independent review additionally
reproduced lost diagnostic records hiding pending TX, and successful retirement
without a TX diagnostic leaving the collector stuck. The collector now consults
`CanBusOwner::localTxRetired()` rather than deriving ownership from lossy tracing.
Tests cover both cases and a completed SDK frame whose final report is pending.

Raw uploads use explicit `SERVER_INDICATED` capacity four and retain actual size;
typed operations still require `EXACT`. Unsized expedited replies and oversized
segmented payloads reject. One-use exact upload grants do not promote ordinary
service or read-effect qualification. The reviewed ledger records these physical
conflicts while preserving all original EDS declarations and templates.

## Reset investigation and correction

The first 8 KB-stack image reset on the first identity request, with ROM reason
`RTC_SW_CPU_RST`. The [raw boot trace](../bench/20261102-s2-identity1/rx.txt)
is retained. SDK console-none suppresses panic output, so the exact panic cause
was not captured and a power-transient claim is not made.

Target GCC stack-usage output nevertheless establishes a concrete defect:
owner task 592 + identity 1056 + experiment wrapper 32 + collector start 2368 +
app wrapper 48 + app request 2048 + profile wrapper 48 + profile prepare 2336
requires at least 8528 bytes before deeper calls, beyond the 8192-byte budget.
The three nested read contexts are each 1936 bytes on S2.
The [before](../bench/20261102-s2-identity-reset/stack-before.json) and
[after](../bench/20261102-s2-identity-reset/stack-after.json) compiler facts are
retained separately from observed runtime headroom.

The collector now uses its already-owned, unoccupied record for preflight
scratch; failed admission still exposes no record or traffic. The bench also
avoids a redundant 928-byte review temporary. Collector start fell from 2368
to 432 bytes and identity from 1056 to 160. The task budget is now 12 KB.
A new native refusal/reuse regression checks atomic observable behavior.
Both subsequent live requests completed without reset and measured 6356 bytes
of minimum remaining stack. This short run does not bound every future action
or prove that the earlier reset had no additional cause.

## Source and software boundaries

The S2 port reuses the existing single owner and CANopenNode service engine.
Reviewed [S2 TWAI retirement](../adapters/twai_esp32s2.md) and
[UART ownership](../adapters/esp32s2_uart.md) retain pinned SDK 5.5.5 evidence.
S2 uses core 0; S3 retains core 1 and USB Serial/JTAG. The S2 console is UART0
115200 8N1, TX43/RX44, with exclusive ownership and no SDK console writer.
NXP [TJA1051 datasheet section 7.1.2](https://www.nxp.com/docs/en/data-sheet/TJA1051.pdf)
establishes high-S silent polarity; electrical performance is not inferred.

An exact one-use upload experiment grant closes a real bootstrap gap in the
earlier collector: its experiment entry point alone could not admit an
unqualified upload on an unconfirmed service. The adapter keeps effect policy
unknown, matches full work identity/deadline, honors node epochs/quarantine and
never promotes experiment success into ordinary service qualification. No
general permission flag or second protocol engine was added.

Read-only sibling audit: RS `513251d9111a6bfb5e1f4ab436b7067d8dd4c746`, ADS1115
`d613a13cd3d43471905c507bfb4094bba1c1da94`, INA228
`73f954838d3fac8cce747d54d0ad738940d4c625`, MB85RC
`1a45e882bb3b1d29f5a18538a7658b0ed0f8a424`, RV3032-C7
`1b74184977be42e4145e2bfe47d7029e32480a5b`, FieldCore
`5ad9292dc8481cf08be9a61f662495a6474d1da0`; all clean when inspected.
RS now classifies settings writes through ActionEvidence and preserves unknown
write effects; sensor cleanup policies were not imported as motor replay.
Implementation, actual callers, focused tests and verification seams were read;
unchanged sibling suites were not rerun or siblings modified.

## Remaining Prompt 18 work

Labels, raw version fields, TPDO1 readback and independent power control are now
available. They do not establish PDO mutation support or NMT state. The narrow
candidate remains TPDO1 remapping in evidenced Pre-operational, retaining its
identifier and timers. CiA documents the conditional variable/dynamic mapping
[procedure](https://www.can-cia.org/can-knowledge/pdo-protocol); that sequence is
known, while selected-firmware applicability remains unproved.

Three concrete prerequisites remain. First, no current Pre-operational observation
was acquired; SDO replies are possible in both Pre-operational and Operational.
No heartbeat was observed and `1017` was not read, so its value is not inferred.
Second, the service's NMT request API has no corresponding concrete
`CanBusOwner`/TWAI TX/report path; those owner transport identities currently cover
SDO only. Third, this adapter supplies no qualified capture timestamp, and the
current exact-NMT freshness contract requires one. Dequeue time cannot be relabeled
as physical arrival. An applicable capture/observation-boundary contract and
explicitly reviewed experiment admission are needed before that path can qualify
state and authorize downloads. These are retained software/evidence obligations,
not a request for another routine permission confirmation.

No hypothetical remap context or always-rejecting implementation was installed.
New map confirmation and sample qualification remain separate: same-ID old
samples stay unqualified. Timer/SYNC/persistence behavior remains unresolved.
[Prompt 18's report](prompt_18.md) retains the implementation and
failure-at-every-step obligations; this connection checkpoint does not close it.

## Final verification and handoff

After the retirement fix and final-tail output correction, the
[final flashed image](../bench/20261102T154011Z-s2-upload/build.json) reports ELF
`6a96ff89016e30fe0a128488b139cb416d12b613efe4ba8f6157533016b068f8`.
[Node 1](../bench/20261102-s2-final-node1/rx.txt) and
[node 2](../bench/20261102-s2-final-node2/rx.txt) again completed all 20 uploads
with the same values and sizes. Final END records each contain 20 terminals,
59 prefix records, one tail report and zero capture-loss flags. The largest
collector gaps were 2008/2011 us; [counters](../bench/20261102-s2-final-stats/rx.txt)
remain 40 RX/40 TX, zero reported errors/drops and 9852-byte stack headroom.
Both records were explicitly released, a repeated node-2 acquisition was
[refused](../bench/20261102-s2-final-repeat-refused/rx.txt), and
[ping](../bench/20261102-s2-final-live-ping/rx.txt) still succeeds. The UART is
closed after each command; the board remains running with no periodic requests.

Integrated local checks passed:

- `python scripts/verify.py --mode full --compiler gcc --build-dir .build/s2-connectivity-final`:
  all 59 non-package CTests, separate package/relocated-consumer checks, generated
  output freshness and strict Doxygen 1.13.2 (26 public headers, 69 free functions).
- `scripts/check_firmware.py --target s2` with the pinned local SDK/toolchain:
  actual S2 ELF/BIN, 494 compiler inputs and all 38 reviewed SDK hashes verified;
  retained evidence under `.build/s2-connectivity-firmware-final/`.
- Focused raw collector, owner/recovery, SDO, TWAI, typed evidence and shared
  firmware tests passed; raw collector has 1819 checks and firmware-build checker
  has 32 tests. Final ledger, promptset and diff checks accompany staging.

Verbatim bench captures retain their acquired bytes and CR/LF, including the
earlier text writer's extra CR in decoded derivatives; `rx.bin` is the authoritative
serial byte stream. [SHA256SUMS.json](../bench/SHA256SUMS.json) hashes all retained
bench artifacts. Their staged Git blobs are compared byte-for-byte. Whitespace
lint covers source/documentation outside this verbatim evidence directory;
normalizing original captures to satisfy whitespace lint would alter evidence.

Separate agents reviewed SDK/S2 source applicability, exact raw grants and
length handling, documentary remap gates, and collector adversarial ownership.
Available `gpt-6.1-sol` high reasoning completed integration/reviews when the
stronger review model became unavailable. The lead operated hardware, reviewed
the combined diff and owns Git/CI. No sibling repository was changed.

The single branch remains `promptset/motorcontrol-can`; the delivery message
records the actual commit, remote synchronization and exact-commit CI result.
The source manifest identifies the flashed dirty-source snapshot independently
of its base commit. Unrelated `.vscode` user files remain outside this checkpoint.
