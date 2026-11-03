# CAN bench facts and evidence gaps

As of 2026-11-02, the [NMT/TPDO checkpoint](reports/2026-11-02_nmt_pdo_checkpoint.md)
records 26 successful raw uploads on each CL86-C node, node1 heartbeat1000,
qualified bounded Pre-operational observation and the fixed TPDO1 map change
with exact readback. At mutation completion node1 had map2 and producer1000; no save/restore,
start/reset or motor action was sent. A later user power cycle produced both
boot-up frames and withdrew cached producer confidence. Fresh readback found
node1 back at producer0/map3 without a save or restore command, for this one
cycle only. Both nodes then completed 30 readiness uploads; reads
607A=5000/60FF=120 equal the EDS defaults, and with mode0 they leave enable/motion readiness unresolved. Firmware starts
with CAN disabled; explicit controls own each experiment. The earlier
[connection checkpoint](reports/2026-11-02_s2_live_connection.md) remains historical.

The user reports an attached ESP-Prog2, an ESP32-S2 PCB and two
CL86 drives with free-shaft motors bolted to a table, and authorizes board testing.
Windows USB enumeration and read-only EasyEDA schematic/PCB net inspection are
recorded in [the board intake](reports/2026-11-02_s2_bench_intake.md). The user now
reports 3.25 V/4.75 V rails, bridged PCB JP1 and last-drive SW8 ON. Cable continuity,
reference ground and transient power behavior remain unmeasured. A bounded target
ROM identification succeeded through UART with automatic EN/BOOT entry. The later
upload and acquisition are recorded above; no JTAG target session or motor action
has been performed. The installed
drive version fields are now captured as raw values; their human version encoding
and exact motor ratings remain unknown. Host readiness, firmware
builds, acquisition and physical qualification remain separate facts.

## Actual fixture record

| Item | Established fact | Evidence needed before dependent live work |
| --- | --- | --- |
| Drive | User confirms both labels CL86-C; nodes 1/2 share the captured raw identity tuple | Manufacturer interpretation of version fields, unique specimen identity and applicable dialect |
| Motor and feedback | User identifies LC60H2127; free shafts with paper mask, somewhat visible but imperfect. [Manufacturer model page](https://en.xlichuan.com/product-xq/71.html) lists representative ratings | Installed variant/ratings, wiring, encoder configuration, adequate motion observation and command/feedback scale remain unqualified |
| PC interface | User reports PCAN-USB opto IPEH-002022 available; not inspected or exercised | Exact connected interface, driver/version, timestamp and queue behavior, wiring and applicable authorization; availability is not adapter integration or qualification |
| Embedded controller and SDK | EasyEDA specifies ESP32-S2-MINI-2-N4; ROM reports ESP32-S2FH4 revision 1.0, embedded 4 MB flash, no embedded PSRAM, 40 MHz crystal. S2 UART/core-0/TWAI firmware was built, uploaded and exercised on IDF 5.5.5 | Physical PCB/module revision still unrecorded; requested P4 adapter support remains separate |
| Transceiver | User confirms TJA1051TK/3/1J on the ESP32-S2 PCB; user-reported, physically unqualified | Supply, electrical connections, silent polarity/behavior and compatibility on the actual board |
| Interface and pins | User confirms CAN RX GPIO16, TX GPIO17, silent GPIO18; no physical signal verification | Pin orientation and actual board/firmware routing, CAN_H/CAN_L/reference wiring; no inferred RS COM port |
| Debug/programming | ESP-Prog-2 303A:1002 serial 94A990398B40 is uniquely COM15. With only UART/programming cable attached, esptool 4.12.0 automatic EN/BOOT entry, flash verification and application restart succeeded; firmware responds at 115200 baud | JTAG remains untested. Earlier manual/no-reset connection failure does not invalidate the successful automatic path or establish its cause |
| Bus topology | User reports PCB to drive to drive daisy chain; CN2 pin1 L, pin2 H in both design netlists | Physical cable/reference continuity and endpoint termination. Drive RJ45 pin1 is H, pin2 L, pin3 GND; CN2 is only two pins |
| Node and bitrate | Matched replies on 581/582 prove service communication with nodes 1/2 using 1 Mbit/s. User reports PCB JP1 bridged and last-drive SW8 ON | Measure unpowered bus resistance and cable/reference continuity; successful short exchanges do not establish electrical margins |
| Power | User reports both probe selectors and PCB jumpers at 5 V, measured rails 4.75 V and 3.25 V on probe/PCB; green VPROG LED blinks. Design supplies MCU through U14; U3 uses separate VCC/VIO | These are user measurements, not captured rail traces. Green activity indication alone is not a power diagnosis; transient behavior, EN and other attached supplies remain unconfirmed |
| External I/O | Post-cycle both nodes report 2400:01=0, :02/:03/:04=1/2/3, :05..08=0; raw2101/60FD=0. Terminals reportedly unconnected | Configured origin/positive-limit/negative-limit functions remain present; confirm electrical levels and active semantics needed by the operation. No function was disabled or reassigned |
| Mechanics | User reconfirmed free shafts, motors bolted to table and independent drive-power cutoff available | Actual ratings, finite motion bounds and independent observation for any motion experiment |
| Authorization | User explicitly authorizes starting board testing and, on 2 November 2026, physical tests generally for this promptset: bench ready, two drives in series, free shafts, motors fixed to the table. No repeat permission request for that scope is needed | Concrete electrical/session facts and experiment bounds must still exist; no arbitrary motion recipe, save/reset or eFuse change follows |
| Evidence | First image: heartbeat/NMT/fixed TPDO1; 197RX/80TX, stackFree9452. Second image: post-cycle26 plus readiness30 uploads/node; 112RX/112TX, stackFree9580. Both stats report zero errors/drops | Global network history separately reports49 then351 dropped entries. One-cycle reversion is observed; broader persistence, firmware, electrical, sample and motor qualification remain pending |

An unconnected input does not mean its protective or enabling function is
disabled. An SDO acknowledgement does not prove physical standstill, a valid
position reference or a cleared fault cause.

The reported parts/pins and successful communication establish the functional
path above, without electrical qualification or a defined motion envelope.
A PCAN interface and an embedded interface do not imply permission for
two active controllers. One designated operator and one application owner
control the selected physical interface. An independently configured observer
must not transmit or change the bus; its presence and electrical effect belong
in the fixture record.

## Documentary wiring facts, not fixture settings

Original sources are in [SOURCES.json](reference/SOURCES.json). These are
one-based PDF pages. The independent page review is recorded in
[the execution audit](reports/prompt_1.md).

| Source | Documentary claim | Limit |
| --- | --- | --- |
| EN86 pp5/9 | RJ45 pins 1 CAN_H/CAN+, 2 CAN_L/CAN-, 3 GND; pins 4-8 NC | Confirm exact connector/model and wiring before use; this is not an Ethernet interface |
| EN86 pp9-10 | SW1-5 select nodes 1-31; all OFF is user-defined | Actual address and DIP/parameter precedence are not established |
| EN86 pp9-10 | SW6/7 choices 125/250/500/1000 kbit/s; SW8 ON connects 120 ohms | 1 Mbit/s works on this fixture; total installed resistance remains unmeasured. EDS also advertises 50 kbit/s without a documented DIP choice |
| EN86 p3 | CL encoder specification 1000 lines; OL entry N/A | Does not establish actual-position units, quadrature conversion or command-pulse scale |
| OP86 pp3/10 | Cover says 86C; internal headers say 57C and p3 pin table says RS485 despite CAN labels | Preserve source inconsistency; do not substitute its labels for an inspected fixture |

The three-way supply disagreement inside each manual is an unresolved Q19
fixture prerequisite. No voltage is selected from those conflicting ranges.
Use the actual nameplate and applicable manufacturer evidence.

## Finite acquisition and interpretation

The [conflict guide](device_conflicts.md) lists the exact source claims,
observations and per-feature gates. The canonical ledger's Q20 remains open as
a device fact: no reviewed upload descriptor has established non-consuming
effects, and `read_effect` stays `not_established`. The owner's DEC-1 acceptance
of 2026-11-02 (`1000`, `1018:00..03`, `1009`, `100A`, `2000`, `2001`, `6041`,
`6061`, `6064`, `606C`, `2100`, `2101`, `2102`) is risk acceptance with the
fixture not yet run, not evidence that those reads have no effect. An ordinary
device plan is admitted only when every object is accepted; any other row is
rejected. `603F`, `1001` and `1003` stay unqualified explicit single reads (first
bench read records LED fault state before and after). An identity or RO label
does not waive this gate, and a successful upload does not resolve it.

The evidence harness uses the ordinary application and typed identity,
configuration and selected state reads through the same `CanBusOwner`. Startup,
cached inspection and construction are passive. Host fake mode demonstrates
request/response ownership and failure recording only. A real acquisition
requires an explicit request and an exact finite plan; there is no discovery
sweep, unsolicited polling, command parser or raw write sender.

The application-side API is
[`DeviceEvidence`](../examples/common/DeviceEvidence.h). `request(plan, nowUs)`
copies one `AcquisitionPlan` only after admission; `DEVICE` is the default
environment and returns `READ_EFFECT_UNQUALIFIED` unless every object in the plan
is on the DEC-1 accepted list.
`requestExperiment(review, nowUs)` takes a `ReadExperiment` containing the exact
plan and the source, independent review, fixture, control, failure and
reconciliation references. It does not authenticate those references or grant a
production capability. `HOST_FAKE` is construction-time test provenance and must
only be paired with an offline fake controller.

Run the ordinary application/owner service turn before `DeviceEvidence::service`.
The collector copies owner diagnostics and each retained completion before
advancing the ordinary typed read; it never receives or services the bus itself.
Inspect `record()` without I/O and explicitly `release()` the terminal record
after retaining it. `interrupt()` cancels local acquisition and preserves
evidence; it does not stop a motor. Do not independently advance/release the
borrowed application or drain owner diagnostics during an acquisition. Storage
is bounded to 64 traffic entries plus the typed field/terminal capacities;
overflow marks lost evidence and interrupts further acquisition.

The [native evidence example](../examples/device_evidence/README.md) documents
the build and runs as `device_evidence --fake-only`. It accepts no live mode or
controller arguments. It demonstrates ordinary `DEVICE` rejection, nine fake
identity and six fake communication reads, then a separate non-replayed timeout
with unknown effects. Its fixed revision label and review references are
explicitly synthetic. Retain its printed output as host evidence only.

The record also retains controller/recovery events, initial and final owner
statistics/resource snapshots, and the largest collector-service interval during
the acquisition. Owner statistics are cumulative; resource snapshots are not
peak or stack measurements. `rxLost` counts explicitly reported missing ingress;
`rxDropped` also includes rejected frames whose raw traffic may remain present.
Missing ingress, overwritten diagnostics, local trace capacity exhaustion and
untrustworthy loss counters mark incomplete evidence. Do not clear statistics
or independently drain diagnostics during acquisition. These observations do
not establish a device response deadline or a physically measured service rate.

Firmware uses the same collector through `FirmwareProbe::evidence()`, fixed to
the `DEVICE` environment. Its service turn runs the ordinary application/owner,
then the transport and collector. A clock/progression failure invalidates owner
evidence, interrupts acquisition and attempts local controller shutdown; it
does not reset, stop or re-enable a drive. This integration is not S2 hardware
qualification or live experiment authorization.

Use these review steps before a future device run:

1. Complete the fixture record above, choose the single interface/owner and
   preserve the exact software commit, build/SDK and board configuration. Record
   profile/model selection as unqualified until Q01 is actually satisfied;
   status dialect remains unknown until Q02 is satisfied.
2. Name every intended read, object/subindex and native width. The nine identity
   candidates are `1000:00`, `1009:00`, `100A:00`, `1018:00..03`, `2000:00` and
   `2001:00`. No `1018:04` is present in the retained EDS. Configuration/state
   acquisition is a separately selected plan, never an implicit follow-up.
3. Confirm each row is on the DEC-1 accepted list or has its own evidence and
   authorization. Otherwise normal device acquisition remains rejected. A separately
   reviewed, explicitly authorized read-effects experiment may request the exact
   finite typed plan with effects still unknown; it is not ordinary probing and
   does not grant future polling permission.
4. Specify route IDs/node, binding/configuration generations, deadline,
   service/observation interval policy, exact step/frame/storage bounds and
   expected observations. These are host policies, not manufacturer response
   guarantees. Retain the independent stop/control, observation, failure and
   reconciliation provisions and their review/authorization references.
5. Preserve raw traffic and terminal records, including failed, aborted,
   stale, interrupted and uncertain acquisition. Record timestamp basis,
   controller TX outcomes, matching object/route identity, owner request/step
   identity, boot-up/EMCY/heartbeat events, service gaps and capture overflow.
   Copy owner traffic without consuming frames from ordinary dispatch. A
   truncated capture is explicitly incomplete evidence.

An acquisition record needs fixture/experiment ID, exact software revision,
source/ledger revision, selected profile and dialect status, route/configuration,
intended rows and bounds, admission evidence references, raw requests/replies
and events, successful typed values with provenance, terminal outcome and
independent observations. Each record distinguishes **documented**,
**implemented**, **host-tested**, **physically observed** and
**hardware-qualified**. A matched SDO response establishes the service result
for that exchange, not motion, units, terminal state or a harmless read.

Read-experiment review applies only to the exact captured plan/binding and
bounds. References document the external decision; software cannot verify that
a physical fixture or authorization is true. Effects remain unknown on success
and potentially uncertain after an interrupted submitted request. Do not turn
local cancellation/recovery into a motor stop or replay a request automatically.
Boot-up, bus-off, rebind or configuration change invalidates dependent current
evidence while the failed historical acquisition remains retained.

## Bench tooling

These tracked scripts replace the ignored `.build` helpers. One lead alone runs
anything that opens the port; `--dry-run` and the parser never do.

**Build and upload.** [`scripts/s2_flash.py`](../scripts/s2_flash.py) requires one
explicit mode. Run it from PowerShell or cmd, because ESP-IDF refuses MSYS.

```text
python scripts/s2_flash.py --dry-run
python scripts/s2_flash.py --build-only --build-dir .build/s2-bench-live
python scripts/s2_flash.py --flash --port COM15 --serial 94A990398B40
```

- `--dry-run` prints the plan from files only. It runs no process, enumerates no
  USB device and creates nothing, and its verdict is `INCOMPLETE`.
- `--build-only` never touches a port. Tool paths come from arguments or
  `IDF_PATH`, `IDF_PYTHON` and `IDF_TOOLS_PATH`, then the known local installs;
  a named path that does not exist is refused rather than replaced.
- It requires ESP-IDF v5.5.5 with its reviewed source hashes, the compiler version
  the SDK's `tools.json` recommends and esptool 4.12.0. A different tool is refused
  and never upgraded. `IDF_COMPONENT_MANAGER=0` keeps the build from downloading.
- The build reconfigures first, because the embedded Git revision is set at
  configure time. It then refuses an image when sources or `HEAD` moved during the
  build, the compiled revision differs from `HEAD`, or `sdkconfig` drifted from
  `sdkconfig.defaults`. It also refuses generated flash options, offsets or chip
  IDs outside the reviewed set, and an application image that does not embed the
  linked ELF's hash at `0xB0`.
- `--flash` requires the recorded USB serial and 303A:1002 uniquely on the named
  port, before the build and again before writing. `read_mac` must then report the
  recorded chip and MAC, and `write_flash @flash_args` runs once. A failure is
  retained and never retried, and the image on the target is then unknown. The
  fixture is a table in the script; a changed fixture needs a reviewed edit there.

No mode erases all flash, changes eFuses or downloads a vendor executable. Each
real run creates one new directory, by default `docs/bench/<UTC>-s2-upload` for
`--flash` and `.build/s2-flash-artifacts/` for `--build-only`, or `--output`. It
holds `build.json` (status, phases, revision, `git status`, dirty and source hashes,
tool versions, image hashes, USB and target identity, commands), `build.log`,
`read_mac.log`, `flash.log`, `sdkconfig`, `flash_args`, `flasher_args.json` and
`project_description.json`. Add a retained upload to `docs/bench/SHA256SUMS.json`
as with any other evidence. Images are hashed, not copied.

Flashing restarts the MCU. That clears no uncertainty and establishes no fresh SDO
session. Verify the running image as two separate steps:

```text
python scripts/s2_bench.py --port COM15 --serial 94A990398B40 --command ping --seconds 2 --output NEW_PING_DIRECTORY
python scripts/s2_flash.py --check-ping NEW_PING_DIRECTORY --artifact UPLOAD_DIRECTORY
```

The second step is offline. It compares the reported base revision, ELF hash, SDK
and target with the build record.

**Offline evidence.** [`scripts/s2_evidence.py`](../scripts/s2_evidence.py) replays
retained capture directories without changing them. `rx.bin` is authoritative.
Older captures have CRLF-translated `rx.txt` files, reported as `crlf_only`.

```text
python scripts/s2_evidence.py docs/bench/20261102-final-node1 docs/bench/20261102-final-node2 --expect-terminals 30 --expect-traffic 90 --require-complete
python scripts/s2_evidence.py CAPTURE... --report NEW.json --transitions NEW.jsonl
```

| Verdict | Meaning |
| --- | --- |
| `complete` | Consistent and fully terminated successful record; not device qualification |
| `failed-evidence` | Consistent record of a failure, lost trace or uncertain effect, retained with its successful prefix |
| `incomplete` | Output missing or damaged: truncation, a dropped or corrupt line, a boot during the record, a failed capture |
| `inconsistent` | Contradiction: wrong run, node, channel, object, width, bytes, stale or duplicated frame |
| `control` / `refused` | A console response or refusal, with no SDO record |

It checks the requested byte and procedure, operation and step identities, node and
channel, address, subindex and direction, actual width and bytes against the wire
frame, terminal and traffic counts, and start and end completeness. Controller and
node epochs are checked only on lines that carry them: `epoch` on `TRAFFIC` and `ne` on
`RESULT` from the soak image on, and `not_emitted` for older captures. Exit status is nonzero
for `incomplete` and `inconsistent`, and `--require-complete` adds `failed-evidence`.

A raw upload is never a typed success, so `typed_successes` counts only typed
`EVIDENCE` terminals. The one-byte `1009`/`100A` replies stay listed against their
EDS U16 widths. The typed rejection in `20261102-s2-identity1-fixed` stays failed
evidence with its wire width and abort code.

`GRAMMAR` in the script is the record format. A new firmware line needs one row; its
signals put it in the transition log. An unknown line or field is preserved and
counted, and does not change a verdict by itself. A record header's tail count is a
lower bound, because tail copies continue until release; `END` holds the total.

`RESULT` now also has `req` (the transfer's request), `oreq` (the owner's result handle),
`ne`, `reason` (the owner's closure) and `st` (stage bits: adapter admitted, controller
accepted, queued, possible TX, confirmed TX, acknowledged, uncertain, cleanup queued). `TRAFFIC`
has its local TX identity as `txk`, `txr` and `txf` on TX and report records, and its state as
`ctrl` or `rec` on controller and recovery records. `STATS` and `PONG` have a firmware time `t`.

The transition log is JSON lines, in capture order across the listed captures. Each
entry has category (`health`, `status`, `controller`, `nmt`), signal, node, previous
and new raw value, firmware processing time where the record has one, receive bounds
and the source line. Unchanged observations add nothing, and cumulative `rx`/`tx`
counters are not transitions. After an observed MCU boot, previous values are stale.
The log shows what the capture recorded, not continuous bus history: `NETWORK` and
`EVENT` come from bounded copies with their own dropped counter.

## Communication soak

A finite, read-only soak of the two-drive bus, through the ordinary public path. It does not
enable, move, write, send NMT, clear a fault, save or reconfigure a PDO, and it has no raw sender
or second receiver. Startup stays passive until the explicit request.

```text
python scripts/s2_soak.py --until 09:00 --dry-run
python scripts/s2_soak.py --duration-s 120 --interval-ms 200 --summary-s 10 --reads full
python scripts/s2_soak.py --until 09:00 --interval-ms 200 --summary-s 60 --reads full
```

Run the short one first. `2102` has no retained read on these specimens, and a reply of an
unexpected width ends with a local client abort, which leaves that route unresolved until an
MCU restart. If it fails, use `--reads identity` for the long run and record the result.

**Read set.** Each object has `read_disposition: accepted_not_qualified` in
[the ledger](reference/lichuan_objects.json). DEC-1 in the
[active audit](prompts/PROMPTSET_AUDIT_34_50_2026-11-02.md) lists them as accepted with polling
allowed. Their effects stay unknown, and a clean night does not show they have none.

| Selected by `--reads` | Objects | Typed read | Last read on these specimens |
| --- | --- | --- | --- |
| Always | `6041:00`, `6061:00`, `6064:00`, `606C:00` | `STATUS` and `FEEDBACK` blocks | `20261102-motion-readiness-node1/2` |
| `monitors`, `full` | `2100:00`, `2101:00`, `2102:00` | `MONITORS` block, a separate operation | `2100` and `2101` as above; **`2102` has no retained read** |
| `identity`, `full`: once per node | `1000:00`, `1009:00`, `100A:00`, `1018:00..03`, `2000:00`, `2001:00` | `IDENTITY` with `MEASURED_U8` | `20261102-final-node1/2` |

`603F`, `1001` and `1003` are never read: DEC-1 keeps them explicit single reads, and this is a
poll. `1017` is not accepted either, so the soak cannot learn the producer interval. Unless an
earlier explicit procedure established it, heartbeat freshness reads `PRODUCER_UNKNOWN`, and
observed heartbeats are still counted with their largest gap.

**Behavior.** One operation at a time, alternating nodes, each with its own identity and a
2 s deadline; queue and response budgets are 250 ms, as host policies. A failed read is retained
and never retried: the next scheduled tick is a new operation. An unresolved route stops that
node's reads with `SOAK_NODE_STOP`, and the other node keeps its cadence. Nothing reopens a route
or starts a recovery, so **one lost reply, one drive boot-up or a reply on a retired exchange ends
that node's reads for the rest of the MCU's run**, and a controller fault ends both. The soak then
keeps observing to its stated end. A new start needs a new MCU boot, which the runner never causes.

**Records.** Bounded, and never blocking service. Every line has `n`, without gaps.

| Line | Meaning |
| --- | --- |
| `SOAK_BEGIN`, `SOAK_END` | Trial, plan, budgets, resources; reason, `lines`, and whether the last read settled |
| `SOAK_TR` | One change, or `first=1` for a first observation: `sig`, old and new raw value, `t`, `ne`, `ce` |
| `SOAK_EMCY`, `SOAK_BOOT` | Raw bytes in wire order, node and network disposition |
| `SOAK_OP` | A failed operation. `local=0`: owner terminal with object, step, outcome, abort, `reason`, `stages`. `local=1`: no terminal, with the read outcome |
| `SOAK_NODE_STOP` | Reads ended for that node, and why |
| `SOAK_ANOM` | One anomaly: `kind`, and `dump=1` when a trace dump follows |
| `SOAK_T`, `SOAK_TEND` | One copied trace entry of that anomaly's dump; end of the dump |
| `SOAK_SUM`, `SOAK_GAP`, `SOAK_CON` | Summary: bus counters; service gaps, high-water marks, stack and heap; console and scheduling pressure |
| `SOAK_NODE`, `SOAK_HIST`, `SOAK_OUT`, `SOAK_ABORT` | Per node: totals and latency; histogram; counts by outcome, reason and read outcome; abort codes |

`SOAK_TR` signals: per node `health`, `streak`, `lastError`, `nmt` (`ff` before any observation),
`eligibility` (bits exact NMT, ordinary SDO, probe upload, service qualification), `heartbeat`,
`unresolved`, `nodeEpoch`, `serverSessions`, `producer` and the native values named by object.
Interface signals, with `node=0`: `controller`, `controllerEpoch`, `inhibited`, `recovery`,
`firmware`, `busOff`, `recoveries`, `twaiErrors`, `twaiFlags`, `twaiDropped`, `rxDropped`, `rxLost`,
`txRejected`, `unmatched`, `queueRejected`, `resultsRejected`, `traceOverwritten`, `networkDropped`
and `submitOverwritten`.

Trace kinds in `SOAK_T`, separating the stages of an exchange: `k=1` adapter handoff with
enqueue result, `k=6` SDK submission with `st` its `TwaiSubmit` result, `k=2` final TX report,
`k=0` RX with receive bounds as offsets `dlo` and `dhi` before processing time `p`, `k=3` terminal
publication, `k=4` controller state, `k=5` recovery. In `k=3`, `id` is index and subindex, `data`
is outcome, reason, stages, reply present and the little-endian abort code, `dlo` is admission to
completion, `tr` the operation and `tf` the step. A dump holds up to 96 entries around the
anomaly, including 50 ms after it. Latency buckets end at 2, 5, 10, 20, 50, 100 and 250 ms. A
successful reply processed before its final TX report counts as `earlyReply`, and local TX
retirement after the operation as `retireMax`.

**Under console pressure.** The UART limits output to about 11.5 kB/s, and the console queue takes
one complete line per turn at most, only when it fits. Waiting records sit in a 96-entry queue.
When full, records are dropped and counted (`evFull`), value and counter records are also rate
limited to ten per second (`evLimit`), and a dropped change shows as a break in that signal's
old-to-new chain, which the parser reports as `soak_transition_gap`. The first two anomalies of
each kind get a dump, sixteen in a soak; the rest are one line each and counted (`dumpSkip`).

Timing effects, all in the summary so they can be accounted for: formatting costs a share of a
1 ms turn, measured by `gapMax`; a dump holds the next scheduled read until it has printed,
about a second at this baud by calculation, recorded in `dumpUs` and `defDump`, and bounded
at 5 s by `dumpAb`.
A started read is never cancelled for the sake of the log. A held read means a dump's traffic
is not the normal cadence: compare intervals only away from one.

**Runner.** `s2_soak.py` checks the recorded USB identity by enumeration and opens only that
port. A run directory holds `run_request.json`, `events.jsonl`, `transitions.txt` and `summary.txt`,
appended and flushed as things happen, `run.json` at the end, and one `trial-NN` per trial with
`rx.bin`, `rx.times.csv` (byte offset, count, UTC, monotonic time and host queue depth for
each read) and `request.json`. A board reset or lost port ends that trial and keeps its evidence.
Within `--max-trials` the runner then waits for the port and starts a new trial, with a new
identity and directory. A soak still running under a lost port is stopped by command rather than
continued under a new name. A refused start ends the run. Exit status: 0 complete, 1 damaged or
not finished, 3 consistent with anomalies retained, 2 refused before anything was created.

Each trial directory replays through `s2_evidence.py`, which adds verdicts as in the table above:
`complete` needs no anomaly, no gap and a `SOAK_END`. Add a retained run to
`docs/bench/SHA256SUMS.json`, as with any other evidence.

**Limits.** A soak does not qualify anything electrical, sustained-load or motion-related, and it
carries no TPDO load unless a drive was started by other means. `IdfTwaiDriver` exposes the
error-callback count and flag union but not the TEC and REC counters, so `twaiErrors` and
`twaiFlags` are the controller's whole error record here. Bench time is MCU uptime; UTC comes only
from the host's side file.

## Conditional state-changing evidence

Some questions cannot be settled by readback: velocity start, target rearm,
zero ramps, stop behavior, clear/save effects and DO truth tables need applicable
manufacturer evidence or a scoped physical experiment. The exact gates and
[experiment prerequisites](device_conflicts.md#conditional-experiment-prerequisites)
remain owned by prompt 29 and can be resumed before completed motion APIs or the
later CLI exist.

The user authorized the bounded PDO test and confirmed independent power cutoff.
No state-changing sequence has run: selected-firmware edit/state prerequisites
are still unproved. Before one runs,
specify and independently review a single exact finite sequence through the
checked public codec/service path and the same owner, implement its isolated
harness support, and fake-test its bytes and failure/uncertainty branches.
Independent observation/control must bound the experiment without relying on
the disputed stop/completion semantics. No hidden enable, NMT start/reset,
fault clear, parameter save, I/O reassignment or PDO reconfiguration is allowed.
Never try both disputed output addresses or motion recipes to discover which
one works. Incomplete prerequisites keep only the dependent feature blocked.

## When hardware is needed

Prompts 1-28 can produce their independently supported software/design results
without a physical fixture. Unknown firmware semantics can keep specific
features in 18/20-22 pending while the owner/adapter path reaches 29. Prompt 29
builds and host-tests the evidence harness; its acquisition steps need the
actual fixture and authorization. Obtain matched-firmware evidence there for
the dependent production operations before qualifying their behavior. A
confirmed, applicable manufacturer clarification may resolve a documentary
question without an experiment.

Prompts 47-48 require physical hardware for finite and sustained qualification.
Host tests, firmware compilation and readback are different evidence. Missing
hardware does not prevent independent software work, and no fake result closes
a physical qualification requirement. Follow [COMMON](prompts/COMMON.md) and
retain the precise feature gates in [the backlog](backlog.md).
