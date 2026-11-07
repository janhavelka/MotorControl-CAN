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
| Motor and feedback | User identifies LC60H2127; free shafts with paper mask, somewhat visible but imperfect. [Manufacturer model page](https://en.xlichuan.com/product-xq/71.html) lists representative ratings. On this configuration `6064` counts command pulses, 1000 per revolution on node 1, where 250 turned the marked shaft about a quarter turn ([2026-11-03](reports/2026-11-03_attended_bench.md)), and **`606C` reads rpm**: 30 during moves at 30 rpm, 1 at 2 rpm, both nodes ([2026-11-04](reports/2026-11-04_bench.md)). Both shafts hold torque at power-up (user, 2026-11-03 and 2026-11-04, except trial 108; not checked by hand on 2026-11-05, see the NOTE.txt of trials 108, 704 and 712) | Installed variant/ratings, wiring, encoder configuration and adequate motion observation remain unqualified; node 2's scale has no shaft observation |
| PC interface | User reports PCAN-USB opto IPEH-002022 available; not inspected or exercised | Exact connected interface, driver/version, timestamp and queue behavior, wiring and applicable authorization; availability is not adapter integration or qualification |
| Embedded controller and SDK | EasyEDA specifies ESP32-S2-MINI-2-N4; ROM reports ESP32-S2FH4 revision 1.0, embedded 4 MB flash, no embedded PSRAM, 40 MHz crystal. S2 UART/core-0/TWAI firmware was built, uploaded and exercised on IDF 5.5.5 | Physical PCB/module revision still unrecorded; requested P4 adapter support remains separate |
| Transceiver | User confirms TJA1051TK/3/1J on the ESP32-S2 PCB; user-reported, physically unqualified | Supply, electrical connections, silent polarity/behavior and compatibility on the actual board |
| Interface and pins | User confirms CAN RX GPIO16, TX GPIO17, silent GPIO18; no physical signal verification | Pin orientation and actual board/firmware routing, CAN_H/CAN_L/reference wiring; no inferred RS COM port |
| Debug/programming | ESP-Prog-2 303A:1002 serial 94A990398B40 is uniquely COM15. With only UART/programming cable attached, esptool 4.12.0 automatic EN/BOOT entry, flash verification and application restart succeeded; firmware responds at 115200 baud | JTAG remains untested. Earlier manual/no-reset connection failure does not invalidate the successful automatic path or establish its cause |
| Bus topology | User reports PCB to drive to drive daisy chain; CN2 pin1 L, pin2 H in both design netlists | Physical cable/reference continuity and endpoint termination. Drive RJ45 pin1 is H, pin2 L, pin3 GND; CN2 is only two pins |
| Node and bitrate | Matched replies on 581/582 prove service communication with nodes 1/2 using 1 Mbit/s. User reports PCB JP1 bridged and last-drive SW8 ON | Measure unpowered bus resistance and cable/reference continuity; successful short exchanges do not establish electrical margins |
| Power | User reports both probe selectors and PCB jumpers at 5 V, measured rails 4.75 V and 3.25 V on probe/PCB; green VPROG LED blinks. Design supplies MCU through U14; U3 uses separate VCC/VIO | These are user measurements, not captured rail traces. Green activity indication alone is not a power diagnosis; transient behavior, EN and other attached supplies remain unconfirmed |
| External I/O | Post-cycle both nodes report 2400:01=0, :02/:03/:04=1/2/3, :05..08=0; raw2101/60FD=0. Terminals reportedly unconnected. On 2026-11-04 both also read `2400:00=0x10`, `:0C=0`, `:0D=1`, `:0E=:0F=0` and filters `2401..2407` = 2, 1, 0, 0, 0, 0, 0; `60FE:00` answered in two bytes against its U8 ([backlog D16](backlog.md)), and under `CountWidths::MEASURED_U16` `60FE:00`, `:01` and `:02` read 0, 0 and 0 (trials 608, 609). On node 1, DI3 (`2400:05`) was set 0 → 7 → 0, each read back, and polarity bit 3 set, which raised `2101` bit 3 but not `60FD`; that polarity read 0 again after the next power cycle, with no save sent ([report](reports/2026-11-04_bench.md)) | Configured origin/positive-limit/negative-limit functions remain present; confirm electrical levels and active semantics needed by the operation. DI0..DI2 were never written; no protective function was disabled or reassigned. Output terminals and their levels are unobserved |
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
`6061`, `6064`, `606C`, `2100`, `2101`, `2102`) is risk acceptance, not evidence
that those reads have no effect. Packet 11 read all 16 listed rows live on two
specimens, through typed identity and the soak, with no read-effect observation.
Packet 15 adds one `607A` and one `60FF` upload per enable as
`accepted_action_prerequisite`: a bounded prerequisite, not a poll, with effects
unknown; both specimens ran an enable in the attended session. An ordinary
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

- `--app` selects the image: `s2-bench` (the default, `examples/s2_bench`) or
  `probe-idf-optak-smoke`, the shared `examples/probe_idf` image with this fixture's
  board values and the one-`G` smoke hook. Each is checked before any write against
  its own project, defaults, target, compiler inputs and flash layout, and one is
  refused as the other. The smoke image is then checked with
  `scripts/s2_smoke.py --artifact <upload record>`
  ([firmware examples](firmware_examples.md)); `--check-ping` is for the bench image only.
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

## Silent capture

The bench image from packet 4 on (PONG `pdo=3 capture=0`) has one bounded receive capture,
[`SilentCapture`](../examples/s2_bench/SilentCapture.h). It records what the ordinary owner receives
while every transmission is refused and GPIO18 requests the TJA1051 Silent mode. It runs on the same
controller, driver, owner and tap as everything else, adds no receive task or TWAI node, and sends
nothing. Its record shows what the software did. It does not show what reached the bus.

`C<capture>,<seconds>,<frames>` and a line end start one capture. `capture` is a nonzero identity
above every earlier accepted capture in this boot; `seconds` is 1..600 and `frames` the record's
capacity, 1..128. `C0` ends a running capture early. Lines are collected like the `N`, `P`, `R`, `W`
and `E` lines ([`PdoCommand.h`](../examples/s2_bench/PdoCommand.h)). An older image reads `C` as an
unknown command followed by single-byte commands, so `scripts/s2_pdo.py` sends a `C` line only to
an image reporting `pdo=3`.

**Entry.** The firmware refuses a capture unless all of the following hold. The answer is
`CAP refused reason=...; no work`; the reason is in brackets below.

- `c` has started the controller and lowered S18, and no soak, route record, PDO, raw or evidence
  record, NMT result, watch, dump or network listing is held (`state`; `busy` while a capture
  collects or its record is held).
- The firmware runs, and the owner is neither inhibited nor recovering (`controller`).
- No ordinary, urgent or NMT request is queued or active, and no typed read or acquisition is active
  (`work`).
- Each configured SDO slot passes `CanBusOwner::localTxRetired`, no NMT handoff awaits its report,
  and the tap holds no frame the SDK may still transmit (`unretired`). Unknown retirement refuses.
  A reply that overtook its TX report, a staged client abort and an uncertain submission all
  refuse.
- The silent-level request is accepted (`silent`). If it is refused, the normal level is requested
  once. The gate opens again only if that request is accepted; otherwise transmission stays closed
  for the boot (`closed` from then on).

Then the tap's transmit gate closes, and only then is S18 requested high. The reply is
`CAP admitted cap=... seconds=... frames=... gpio=18 silentHigh=1; TX gate closed, silent level
requested, nothing sent`. Entry starts no drive PDO, NMT or SDO, and changes no route, quarantine
or retained result.

**While capturing.** Received frames reach the owner as before: heartbeats, EMCY, unsolicited SDO
and every unknown frame are still dispatched. The capture keeps a copy of each, taken at the tap as
the watch does. The watch is refused meanwhile, because it shares that ring. Every path to the SDK
meets the closed gate and closes provably unsent: SDO requests, segments and client aborts, drain
markers, best-effort stops, urgent work and NMT. The library has no PDO or SYNC producer, and the
gate would refuse those frames too. Only `p`, `s`, `n`, `d`, `r` and `C0` are served; any other byte
answers `CAP refused reason=active` and any other line `PDO_LINE refused reason=capture`. The
capture ends at its duration (`duration`), at `C0` (`stop`), or when the firmware stops, the owner
becomes inhibited or a recovery begins (`controller`).

**Exit.** The copies stop first. After a `duration` or `stop` end with a usable controller, S18 is
requested low. The gate opens only once the owner and the tap are idle and retired again, normally
in the same turn. Work admitted during the capture closes unsent at the gate before it opens. After a
`controller` end no normal level is requested, and the gate stays closed for this boot. A refused
normal request also leaves the level unknown and the gate closed for the boot. The record then
prints, one line per turn, is reprinted with `d` and is released with `r`
(`CAP record released; level, gate and SDO state unchanged`).

| Line | Fields |
| --- | --- |
| `CAP_REC` | `cap`, `reason`, `seconds`, `capacity`, `stored`, `overflow` (copies after the record was full), `tapDropped` (copies the 32-entry tap ring had no room for), `pre`, `post`, `untimed`, `txRefused` (submissions refused at the gate between begin and end), `lost` (any tap, driver or owner receive loss) |
| `CAP_TIME` | `begin` and `end` processing times, `nominal` end, `gap` (largest interval between capture services), `clock=esp_timer_us` |
| `CAP_LEVEL` | `gpio`, `silentHigh`, `begin=1`, `exit` (1 normal requested, 2 refused, 3 left silent), `reopened` and its time `at`, as at printing, and `latched` |
| `CAP_CTRL` (`which=0` begin, `1` end) | `t`, controller `epoch`, `counters` with `tec` and `rec` from `twai_node_get_info`, owner and adapter `inhibited`, adapter `twaiDropped`, `twaiErrors`, `twaiFlags` |
| `CAP_OWNER` (`which=0`, `1`) | Owner lifetime totals `rx`, `dropped`, `lost`, `unmatched`, `tx`, `txRej`, `busOff`, and the tap's lifetime `refused` |
| `CAP_IMAGE` | `base` (source revision) and `elf` (the image's ELF SHA-256) |
| `CAP_F` | `i`, `id`, `ext`, `rtr`, `dlc`, `data` (hex, `-` without payload), `bounds`, `exact`, `lo`, `hi`, `epoch`, and `pre` (received wholly before `begin`, copied after it) or `post` (received wholly after the nominal end, copied in the end turn) |
| `END capture` | `cap`, `stored`, `overflow`, `lost` |

`lo` and `hi` are the driver's receive bounds on the S2 clock, never bus timestamps. The record
holds frames the owner took between the begin turn and the end turn. A frame still in the driver's
FIFO at the end turn is outside it.

**What a capture shows, and what it does not.** It shows that the software refused every
submission (`txRefused` and the tap counters), which frames the owner received, the counters at both
boundaries, and the GPIO levels that were requested. It does not show that the transceiver was
silent, that the S2 sent no ACK or error flag, or that a drive's pending SDO session drained. The
controller stays in normal mode and still drives ACK and error bits onto TXD; only Silent mode on a
correctly wired pin S keeps them off the bus ([SDK evidence](adapters/twai_sdk.md#listen-only-mode-the-silent-transceiver-input-and-the-bench-capture)).
When both drives are powered, each acknowledges the other's heartbeats, so a correct capture sees
their frames whether or not the S2 is silent. With only one drive powered, the S2 would be the only
acknowledger. A silent S2's controller would then read its own dominant ACK back as recessive, a bit
error under the CAN 2.0B rule. It would drop each attempt and raise its own error counters, while
the drive retransmits. This is an expectation from the standard, not a measurement, and a wiring
fault could look the same. A capture of zero frames proves neither capture nor silence. Physical
no-ACK and no-TX need an independent listen-only observer, still to be integrated.

**Runner.** `scripts/s2_pdo.py run` accepts a `C` step with `expect` `CAP admitted`, `end`
`END capture` and an `end_timeout_s` longer than the capture. Its record is the one held record
until an `r` expecting `CAP record released`. The step succeeds only when the record names the sent
identity, ends at its duration with no loss, overflow or gate refusal, requested the normal level,
reopened the gate, and holds exactly the `CAP_F` lines it says it stored. Zero frames pass that
check. If a plan stops while a capture collects, only `C0` is sent. The preflight refuses an image
that reports `capture=1`, and `analyze` lists each capture's frames per identifier with their gaps
on `hi`, leaving out `pre` and `post` frames.

## Communication soak

A finite, read-only soak of the two-drive bus, through the ordinary public path. It does not
enable, move, write, send NMT, clear a fault, save or reconfigure a PDO, and it has no raw sender
or second receiver. Startup stays passive until the explicit request.

```text
python scripts/s2_soak.py --until 09:00 --dry-run
python scripts/s2_soak.py --duration-s 120 --interval-ms 200 --summary-s 10 --reads full
python scripts/s2_soak.py --until 09:00 --interval-ms 200 --summary-s 60 --reads full
```

Run the short one first. A reply of an unexpected width ends with a local client abort, which
leaves that route unresolved until an MCU restart, as `607D:00` and `60FE:00` did on 2026-11-04.
If it fails, use `--reads identity` for the long run and record the result.

`--keepalive-s N` is a HIL-only diagnostic: the runner writes one bare newline every N seconds
while it captures, which the firmware ignores outside a command line, and counts them in the
trial's `host_keepalives`. It exists to test whether console loss after a long quiet spell follows
the idle link (see the [soak report](reports/2026-11-03_overnight_soak.md)). It is off by default
and sends nothing else.

**Read set.** Each object has `read_disposition: accepted_not_qualified` in
[the ledger](reference/lichuan_objects.json). DEC-1 in the
[active audit](prompts/PROMPTSET_AUDIT_34_50_2026-11-02.md) lists them as accepted with polling
allowed. Their effects stay unknown, and a clean night does not show they have none.

| Selected by `--reads` | Objects | Typed read | Last read on these specimens |
| --- | --- | --- | --- |
| Always | `6041:00`, `6061:00`, `6064:00`, `606C:00` | `STATUS` and `FEEDBACK` blocks | `20261102-motion-readiness-node1/2` |
| `monitors`, `full` | `2100:00`, `2101:00`, `2102:00` | `MONITORS` block, a separate operation | `2100` and `2101` as above; `2102` in every `readiness-actions` record and soak since 2026-11-03 |
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
old-to-new chain, which the parser reports as `soak_transition_gap`. A value or counter signal
raises at most one `SOAK_ANOM` per summary window, at its first change in that window; its later
changes show only in its rate-limited `SOAK_TR` chain and in the summary's cumulative counters.
Before this, a lasting fault kept the queue full: after the drives stopped acknowledging on
2026-11-03, `twaiErrors` raised one anomaly per pass, about a million lines, and no summary
printed for 3 h. The first two anomalies of each kind get a dump, sixteen in a soak; the rest are
one line each and counted (`dumpSkip`). Every line fits the 448-byte line buffer with each field at
its type's maximum, and the console queue takes 512 bytes.

In `SOAK_CON`, `blocked` counts turns a formatted line waited for room. `conRej` is the console's
total of refused submissions since the MCU booted, from every writer. The other routes' dumps
retry a refused line on the next turn, so there it counts retries, not lost lines. The soak
writes only after measuring room and never adds to it, so a constant `conRej` through a soak
comes from earlier commands on the same boot. `defBusy` counts turns a due read waited for the
previous operation. `slips` counts ticks that had already passed when the next was set; the next
read then starts one interval later, with no catch-up burst. When the interval is shorter than
one operation, both grow every minute by design: the run is bound by the drives' reply time.
`submitOverwritten` is also a since-boot total: the TWAI tap keeps one submission record and only
the soak takes it, so each single-frame request by an earlier command on the same boot replaces
an untaken one. A constant value through a soak is that history, not loss during it.

`stale` in `SOAK_BEGIN` counts the owner's network-observation copies still queued from before
the soak (heartbeats, boot-ups, EMCYs between soaks). They are discarded at begin, so they never
become a heartbeat gap or an event of this soak. Images before 2026-11-06's fix lack the field,
and there the first heartbeat gap of a soak can span the time since the previous one.

`uartBytes` and `queued` (images from 2026-11-06 on; optional in the grammar) are taken when
`SOAK_CON` is formatted: the bytes the UART FIFO accepted since boot, modulo 2^32, and the bytes
still in the console queue. Their sum is the line's position in the S2's output stream, so
`scripts/s2_console_account.py rx.bin` compares, between consecutive `SOAK_CON` lines, the
bytes the S2 admitted with the bytes the host received. A shortfall is bytes the UART FIFO had
accepted that never reached the host: lost on the UART line, in the bridge, in USB or in the
host driver, which these counters cannot tell apart. FIFO acceptance is not host receipt. The
position is exact unless another writer's output (a `PONG`, a watch line) was admitted while
the formatted line waited for room; such windows are marked. Overhead: one 32-bit increment per
accepted byte, at most 39 more bytes per summary (0.3% of the 115200-baud line at 1 s summaries)
and 32 bytes of line buffer; the service budgets are unchanged.

The host checks every line's `n`. A gap or a malformed line in the digest, while the firmware's
`lines` stays contiguous, is loss between the S2's UART and pyserial. These losses, each a deletion of at most 64 bytes
in the host bytes, are analysed in the [soak report](reports/2026-11-03_overnight_soak.md) and
the [stress report](reports/2026-11-05_motion_and_stress.md#overnight-communication-stress).
An `OUTPUT_FORMAT_ERROR` line is different: the firmware itself could not format that line.

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
carries no TPDO load unless a drive was started by other means. `twaiErrors` and `twaiFlags` are
the SDK's error-callback count and flag union; `SOAK_GAP` adds the controller's TX and RX error
counters (`tec`, `rec`), sampled once per summary. Bench time is MCU uptime; UTC comes only from
the host's side file.

## Attended bench routes

Named finite routes for the attended trials of packets 13, 14, 15, 16, 19, 20, 21 and 22, and the
`1003` error history read of packets 4 and 22, in
[`BenchRoute`](../examples/s2_bench/BenchRoute.h). The [attended session](reports/2026-11-03_attended_bench.md)
ran `actions`, `mode1`, `enable`, `position` (one with a named HALT) and `rearm`; the
[unattended session](reports/2026-11-03_unattended_bench.md) ran `move` once, `stop` twice and
`history` on both nodes. The [2026-11-04 session](reports/2026-11-04_bench.md) ran every route but
`clear-alarm`, for which no alarm occurred: `actions`, `history`, `io`, `tuning`, `mode1`, `enable`,
`stop`, `position`, `rearm` (after a trigger, a halt and a relative trigger), `move` (absolute with a
planned stop, relative and observed), `released-quick`, `word67`, `relative`, `io-di3`,
`counter-zero`, and `settings-2202`, which first stopped at its prior read before any write and,
on `2920479`, wrote and read back codes 1 and 0 on both nodes (trials 604 to 607). The
[2026-11-05 campaign](reports/2026-11-05_motion_and_stress.md) ran `mode1`, `enable`, `move`,
`position` and `rearm` again at higher speeds. Each is
a fixed table through the ordinary owner and an `AxisSession` opened with `ProbeApp::openAxis`:
typed reads, the session's own `requestEnable`, `requestMove`, `requestMoveRelative`,
`requestStop`, `requestCounterZero` and `requestClearAlarm`, the library's settings transactions
(`prepareSoftLimitSettings`, `prepareIoSettings`) on the session's reservation, and caller-built
transfers through `admitPrepared` only for the 6060 write, the position and relative experiments'
writes, the rearm's `000F`, the `released-quick` `000B` and `word67`'s `0006` and `0007`. There is no
raw sender, object selector or command language. Startup is passive. A route is refused while a
soak runs or any other record is retained, and nothing else but `p`, `s`, `d`, `r` and `A` is served
while its own record is held.

| Runner `--command` | Firmware line | Reads and writes | Ends early on |
| --- | --- | --- | --- |
| `readiness-actions` (packet 15 P1) | `TA<node>,<trial>` | STATE `1001`, `603F`, `2100..2102`, `6041`, `6061`, `6064`, `606C`; MOTION_PARAMETERS `6081`, `6083`, `6084`; single uploads `1017`, `607A`, `60FF`; the accepted image again. No write | Any failed terminal |
| `readiness-io` (packet 13 P1) | `TI<node>,<trial>` | Image; `2400:00..08`, `2400:0C..0F`, `2401..2407`, `60FE:00..02` as the four typed I/O groups, `60FE:00` read in two bytes (`CountWidths::MEASURED_U16`), as both specimens answered it; image. `2400:0B` is never read. No write | As above |
| `readiness-tuning` (packet 14 T0) | `TU<node>,<trial>` | Image; the four typed tuning groups `2502..2520`; image. No write | As above |
| `readiness-history` (packet 4, packet 22 step 2) | `TH<node>,<trial>` | The `readiness-actions` STATE read; STATE `ERROR_HISTORY` alone: `1003:00`, then only the entries `1003:01..` its count names, at most four, each upload with its own one-use grant; STATE `ERRORS` (`1001`, `603F`) again, so both are on record before and after the first `1003` read. Values raw, never interpreted. No write | As above; a `1003:00` count above four fails the typed read after that upload |
| `mode1` (packet 15 P2) | `TM<node>,<trial>,1,<cycles>,<interval>` | The `readiness-actions` reads, a check, then `2F 60 60 00 01 00 00 00` once, then 1..10 fresh cycles, the first with `1001`/`603F`. No enable | Before the write: `1001` or `603F` nonzero, `2100` alarm or running, `606C` nonzero, `6061` not 0, an earlier write or controlword in this session. After: alarm, running, `606C` nonzero, a fault code |
| `enable` (packet 15 P3) | `TE<node>,<trial>,1,1,<shaft>,<6041>,<607A>,<60FF>,<deadline>,<interval>` | A fresh `ERRORS`/`MONITORS`/`STATUS`/`FEEDBACK` read, then `requestEnable` with `ATTENDED_EXPERIMENT`, the host's trial, reviewed values and `NO_MOTION_WRITE_RECORDED`, and the attendant's typed `FREE` or `HOLDING` | The library's own conditions; a nonzero EMCY of the node withdraws it with `DRIVE_FAULT` |
| `stop` (packet 15 P4, packet 16) | `TS<node>,<trial>,1,<1 halt/2 quick>,<deadline>,<best effort or 0>,<interval>` | A fresh `MONITORS`/`STATUS` read, then `requestStop` with that kind and policy; its age cap covers seven fields at the route budgets, so a QUICK_STOP may also read `6064` and `606C` each cycle to observe standstill | The session decides the path |
| `position` (packet 19) | `TP<node>,<trial>,1,<6064>,<607A>,<rpm>,<up>,<down>,<cycles>,<interval>,<stop>,<after>,<stop deadline>,<best effort>,<stop interval>` | A fresh full read and a check: latest controlword the session's acknowledged `000F`, mode 1, low bits 111, released, alarm and running clear, `606C` 0, `6064` equal to the reviewed start. Then `6081`, `6083`, `6084`, `607A` read for the record; `607A`, `6081`, `6083`, `6084` and `6040=001F` written, each after the previous acknowledgement; 1..16 fresh image cycles; an optional `requestStop` after a named cycle | Alarm, a failed terminal, EMCY. No stop follows unless planned |
| `rearm` (packet 19) | `TR<node>,<trial>,1,<predecessor>,<cycles>,<interval>` | The operator names the predecessor: 1 the acknowledged `001F` of a finished `position` record, 2 the acknowledged `010F` of that record's planned HALT (does clearing a halt resume the halted target?), 3 the acknowledged `005F` of a finished `relative` record, all on that node. Admitted only while the session's latest controlword is exactly that terminal: missing, uncertain or any other word refuses with `control`, and a `relative` record's HALT is no predecessor. A fresh full read and the `position` check without the start position. Then `2B 40 60 00 0F 00 00 00` once through the session, so it becomes the latest word when acknowledged, then 1..16 fresh image cycles, recorded and never judged. Project policy until the mode 1 falling-edge behavior is measured | Alarm, a failed terminal, EMCY. A lost reply leaves no latest word: `position`, `relative`, `rearm` and `move` then refuse |
| `move` (packets 20, 22 step 4) | `TG<node>,<trial>,1,<6064>,<607A>,<rpm>,<up>,<down>,<deadline>,<interval>,<record>,<accept source>,<accept mask>,<accept value>,<complete source>,<complete mask>,<complete value>,<rule>,<1000>,<1009>,<100A>,<1018:01>,<1018:02>,<1018:03>,<2000>,<2001>,<relative base>,<base>,<observer>,<stop>,<after>,<stop deadline>,<best effort>,<stop interval>` | Refused unless the latest word is the acknowledged `000F` and no earlier move is fenced. A fresh identity read (`1009`/`100A` one byte), a fresh full read and the `position` check, then `requestMove` with `MoveEvidence` built only from the line: the record, both signals (1 `6041`, 2 `2100`), the rule (1 after absence, 2 after acceptance) and the raw identity, which must equal the fresh read. An absolute `STEPS` target in the native frame and rpm as motor turns per minute, with no scale claimed. The session writes `607A`, `6081`, `6083`, `6084`, `001F`, runs its cycles to its own terminal and writes its own `000F` only after an observed completion. An optional `requestStop` after a named number of move cycles, with the move's last complete cycle. `<relative base>` 0 is that absolute move, with `<base>` 0. 1 (`ACTUAL`) or 2 (`COMMANDED`), the base packet 21 measured, makes `<607A>` a nonzero displacement: a second fresh full read must pass the same check, and `requestMoveRelative` gets that basis as `MoveEvidence::relativeBase` and the request's basis, and an idle, stationary witness from the two reads (`ACTUAL`: `<base>` equal to `<6064>`, read back; `COMMANDED`: `<base>` the reviewed commanded position, the host's assumption), observed at the second read's earliest `6064` bound and aged by the move's own cap. The session's trigger is then `005F`. `<observer>` 0, or the other node (packet 22 step 6): before the move a fresh full read of it must show running and alarm clear and `606C` 0, released or not as found; while the move or its planned stop runs, its image is read at the move's interval through its own route, session and read context, never the moving node's, and it gets no write. Its cycles are recorded and never judged: `TRIAL_OBSERVER` reports whether any read `606C` nonzero, running or another `6064`. The planned stop stays on the moving node | As the absolute move. A second read that does not repeat the first's state refuses before any write. An observer that does not read stationary refuses (`why=observer`); an EMCY or failed read of it ends only the observing |
| `released-quick` (packet 16 step 3) | `TQ<node>,<trial>,1,<cycles>,<interval>` | Two fresh full reads, each with `2100` bit 4 released, alarm clear, `6041` low bits 000, mode 1, `606C` 0, `1001` and `603F` 0; `2100` bit 2 set in either only with the same `6064` in both, because it stayed latched after a `000B` stopped a move (trial 213). No predecessor: the session's latest word is recorded as `TRIAL_CONTROL`, and must be unchanged when the write is offered. Then `2B 40 60 00 0B 00 00 00` once through `admitPrepared` under its own grant, which becomes the session's latest word when acknowledged, then 1..16 fresh image cycles, recorded and never judged. An application-local experiment, not a stop: `requestStop` refuses a released drive by design | The check (`unreleased`, `status`, `mode`, `alarm`, `velocity`, `running` and the error reasons); after the write alarm, a failed terminal, EMCY |
| `word67` (packet 15) | `TW<node>,<trial>,1,<cycles after 0006>,<cycles after 0007>,<interval>` | The `released-quick` reads and check, then `2B 40 60 00 06 00 00 00` once and 1..4 fresh image cycles; only when the last of them shows mode 1 and low bits 001, `2B 40 60 00 07 00 00 00` once, offered only while this route's acknowledged `0006` is the latest word, and 1..16 fresh image cycles with `6064` and `606C`, recorded and never judged. No other word: the drive is left switched on with `0007` the latest word | As `released-quick`; after `0006` failure 9 `mode` or `status`, and nothing more is written |
| `relative` (packet 21) | `TL<node>,<trial>,1,<6064>,<displacement>,<rpm>,<up>,<down>,<cycles>,<interval>,<stop>,<after>,<stop deadline>,<best effort>,<stop interval>` | Exactly `position`, with the signed nonzero displacement (-1000000..1000000) in `607A` and the trigger `6040=005F`: the base measurement. `TRIAL_BASE` shows the `607A` read before the writes and the firmware's own record of the last `607A` it had acknowledged on that node (trial, value, whether a displacement); the start `6064` is in `TRIAL_CHECK` | As `position` |
| `settings-2202` (packet 12 T1/T2) | `TD<node>,<trial>,1,<code 0/1>` | A fresh `SOFT_LIMITS` read (`2202`, `607D:00..02`, `607D:00` in two bytes under `CountWidths::MEASURED_U16` here and in the readback, as both specimens answered it) and a fresh `STATUS`/`MONITORS` read, then `prepareSoftLimitSettings` with that code on the session's reservation (its own prerequisites: a prior code of 0 or 1, no running, alarm or undocumented `2100` bit; for code 1 the minimum below the maximum and homing not complete). Its one write `2B 02 22 00 <code> 00 00 00` through `admitPrepared` under its own grant, with the library's pre-transmit notice, then its readback of the group. An unchanged code sends nothing (`why=unchanged`, `noop=1`). Prior and readback raw in `TRIAL_LIMITS`. Not motion: attended, three per route and node | The library's refusal (`why=settings`, `TRIAL_STATUS`), a failed terminal, EMCY. Codes other than 0 and 1, the EDS default 10 among them, are refused by the parser |
| `io-di3` (packet 13 P2/P3) | `TJ<node>,<trial>,1,<case 1..4>` | A fresh `IO_INPUT_CONFIGURATION` read (`2400:00..08`) and a fresh `STATUS`/`MONITORS` read, then one complete `IoSettings` candidate through `prepareIoSettings` changing exactly one thing: case 1 `2400:05` (DI3 function) 0 to 7 (USER0), case 2 7 to 0, case 3 sets polarity bit 3 of `2400:01`, case 4 clears it; every other field and unknown polarity bit is the prior readback's. Its one write, the readback, then `2101` (MONITORS) and `60FD` (INPUTS) once (`TRIAL_INPUTS`). Packet 13 P4 (output `2400:0E`) stays unrouted. Not motion, as `settings-2202` | Before any write: the prior not in the state its case names (`why=di3`), another input holding an undocumented code a candidate cannot carry (`why=undocumented`), the library's refusal |
| `counter-zero` | `TZ<node>,<trial>,1,<deadline>,<interval>` | A fresh `MONITORS`/`STATUS`/`FEEDBACK` read, then `requestCounterZero` with native position 0, this node's coordinate configuration and a witness that knows no position; the session writes `2B 02 23 00 00..`, `..01..`, `..00..`, moves its reservation to the new configuration epoch itself and reads until `6064` reads 0. Motion class: it moves the position reference | The library's refusal (`why=clear`), EMCY (`cancelClear` with `DRIVE_FAULT`), abort or lost evidence (`cancelClear`) |
| `clear-alarm` | `TC<node>,<trial>,1,<deadline>,<interval>` | A fresh `ERRORS`/`ERROR_HISTORY`/`MONITORS`/`STATUS` read, refused unless `2100` reports the alarm (`why=no-alarm`), then `requestClearAlarm`: `2B 01 23 00 01..` then `..00..`, cycles of `MONITORS`/`STATUS` until the alarm reads clear, then `ERRORS` once. Nothing follows: no enable, controlword or resume. Motion class | As `counter-zero` |

Every line also ends in an integrity field the table leaves out: `,K` and eight lowercase hexadecimal
digits holding the CRC-32 (IEEE, as Python's `zlib.crc32`) of every byte from `T` up to that comma,
for example `TA1,5,K62328164`. The firmware checks it before parsing and refuses a missing or
mismatched field with `reason=crc`, because a digit corrupted in transit can still parse as another
valid plan. `PONG routes=5` names this protocol (3 added the integrity field; 4 the rearm's
predecessor, the move's relative base and `TQ`, `TW`, `TL`; 5 `TD`, `TJ`, `TZ`, `TC` and the move's
observer); the runner refuses any other value before sending, so an image that reports an earlier
protocol takes no line from this runner.

**Hold-back.** No route read or write starts once less than its queue and response budget (350 ms
at the route budgets) remains before its operation deadline or the sixty-second route cutoff, for the
reason of the library's own rule (trial 213: an exchange cut at its deadline with its request possibly
sent left node 2's route unresolved until an MCU restart). The route then ends at once with failure 7
(`why=cutoff`, or `why=operation` at an operation deadline) and nothing in flight. A `000B` is a stop
word and is exempt. The sessions' own actions follow the library's rule.

`trial-abort` sends `A`: a local withdrawal, with no stop sent. A request on its way settles first,
an active enable ends through `cancelEnable`, an active move through `cancelMove` and an active clear
through `cancelClear`, and a stop in progress refuses it. The runner sends one `A` on Ctrl-C.

A relative `move` needs the base `relative` measures first: until a record shows what the drive adds
a displacement to, `<relative base>` 1 or 2 is an unsupported claim, and the library only binds the
host's statement. A `move` refused by the library still counts as a motion admission, as every motion
route does.

**Prerequisite for an enable.** The session admits its enable words and a sequenced stop as
ordinary SDO work, which needs a current exact NMT state or service qualification. This fixture's
ingress carries receive bounds only. A checked SDO reply now qualifies service from its bounds when
the earliest bound is strictly after the request's handoff and after the node boundary; the full
rule and its residual risk are in [SDO service qualification](adapters/twai_receive_time.md#sdo-service-qualification).
Before any such reply nothing qualifies, so a route's fresh reads come first, and `enable` is
refused with `reason=eligibility` before anything is sent when no current qualification exists. A
heartbeat is no longer needed for this. `heartbeat1` stays a separate `1017` write experiment, not a
route prerequisite; if it is run, run it and `release` before the first route on that node, because
`RawEvidence` takes its own reservation and refuses once a session holds one. Without current
qualification, a stop can only take the policy's best-effort `000B` branch. The routes' own writes
take one-use download grants and do not need it.

**Every route** records its admission in `TRIAL admitted` and has a new trial identity, rising for
the firmware's life; the host's trial log must never reissue one across boots. Motion routes
(`mode1`, `enable`, `stop`, `position`, `rearm`, `move`, `released-quick`, `word67`, `relative`,
`counter-zero`, `clear-alarm`) and the settings routes (`settings-2202`, `io-di3`) need the attended
flag the runner sets only after the operator typed `ATTENDED <trial>`. Packet 22's campaign cap is
enforced per boot at admission: at most twelve motion admissions, and three per route and node. A
settings route has no motion weight; it counts three per route and node of its own. A planned stop inside `position`, `relative` or `move` counts against `stop` as well. Across boots the runner enforces the same caps
from the campaign ledger below. Every unqualified upload and every write of the route's own takes
an exact one-use grant, numbered from 100000. Queue budgets are 100 ms and response budgets 250 ms,
as host policy; with them the shortest enable deadline the library admits is 9.13 s. A plan whose
polls could exceed the record (128 terminals) or the sixty-second cutoff is refused before anything
is sent. Any failed terminal, lost reply, refused grant or admission, changed epoch, nonzero EMCY,
lost evidence or abort ends the route and keeps its evidence. Nothing is retried, enabled, cleared,
zeroed, restored or switched to another mode. A lost reply leaves the state unknown: after one on
`6060` the session refuses any enable, and the route stays unresolved until an MCU restart.

**Lost evidence** (copied traffic or terminals overflowing the record, or the owner's trace or
receive path dropping frames) ends every route with failure 5 (`why=lost`), never 0. The lost span
may have held an EMCY or a status report, so an active enable or move is also withdrawn locally
through `cancelEnable` or `cancelMove`, as an abort withdraws it, and the header shows
`lossCancel=1`. Nothing is sent because of the loss: no stop, no rearm and no planned stop; the
drive may still be moving, and the attendant's independent cutoff is the control. A trigger that may
have left without an observed completion still fences later moves.

**Record capacity.** The firmware refuses a route as `capacity` when its polls could exceed 128
terminals, and as `duration` when its steps could need more than 55 s; it does so after the host has
counted the send, so the runner computes the same bounds (`plan_bounds`) and refuses such a plan
first, sending and counting nothing, and `--dry-run` prints them. For the same reason it refuses a
`move` whose `--deadline-ms` is 4550 or less: the library refuses a move deadline that does not
exceed 13 services at the routes' 100 ms queue and 250 ms response budgets, before any write but after
the firmware counted the line and its planned stop. A stop's cycles are bounded at seven
fields, because a QUICK_STOP that observes standstill also reads `6064` and `606C`: a stop takes 3 plus
7 per cycle, one at once and one per interval to its deadline. So `stop` with a 2000 ms deadline needs
an interval of at least 200 ms (85 terminals; 100 ms would be 155). `position` and `relative` take 18
plus 7 per cycle, so at most 15 cycles without a stop, and with a planned stop of 2000 ms polled every
250 ms (66 more) at most 6, although the parser accepts 16. `rearm` takes 10 plus 7 per cycle (16
fit), `released-quick` 19 plus 7 per cycle (at most 15), `word67` 20 plus 7 per cycle of either word
(at most 15 in all), and a relative `move` nine more than the absolute one. An observer node adds 9 plus 7 per cycle,
one at once and one per interval over the move's deadline and its planned stop's span: an observed
move fits only short deadlines or long intervals, for example a 6000 ms deadline at 2000 ms (103
terminals); the move's own 10000 ms at 1000 ms does not. `settings-2202` takes 14, `io-di3` 28,
`counter-zero` 10 plus 7 per cycle and `clear-alarm` 16 plus 5 per cycle, each with one cycle at once
and one per interval to its deadline.

**Release after every route.** A route sent while a record is held is refused before its line, and the host still counts it.

**Planned stops and the cutoff.** A `move`, `position` or `relative` with a planned stop is refused with
failure 11 (`why=deadline`) before its first write unless the stop's whole span (its deadline, or
the best-effort cutoff when later) still fits the sixty-second route cutoff when the stop is asked
for at its latest: for `move`, after the move's deadline; for `position` and `relative`, after its writes' five-
second operation deadline and `<after>` cycles of up to five seconds each (an interval, or one read's
operation deadline). A plan that could never fit is refused at admission with `reason=duration`. A
stop refused for the cutoff can therefore not leave a trigger running.

**Predecessor at the write.** The check step judges the session's latest controlword (the
acknowledged `000F` for `position` and `relative`, the named acknowledged predecessor of a finished
record for `rearm`, no write or controlword yet for `mode1`) on one turn; the first write is offered
on a later one. The same predecessor is judged again each time that first write is offered, and a
changed one ends the route with failure 8 and the same reason before anything is written.
`released-quick` and `word67` require no predecessor, but the latest word they recorded at admission,
or none, must still be the latest when their first word is offered, and `word67`'s `0007` is offered
only while its own acknowledged `0006` is the latest. A plan built in code is judged by the parser's
own range check at admission (`reason=plan`).

**Sessions stay open** once a route has opened them, so that their written-motion record, latest
controlword and trial record survive between routes, as P3 and P4 need. While one is open,
`RawEvidence` and `PdoExperiment` on that node refuse. Release (`r`) frees the record only.

**Records.** After the terminal the firmware prints the record once; `d` reprints and `r` releases.
It has the same form as `RAW`: a `TRIAL` header (route, failure, `why`, counts, step, cycles,
motion counts, `unsettled`, gap, `lossCancel`), summaries, then `RESULT` for every owner terminal including the
session's own, `TRAFFIC`, `TAIL` and `END trial`. From protocol 5 each `RESULT` names its `node`, and
each TX and TX report its SDO channel as `txc`, so that an observed move's record holds both nodes:
`s2_evidence.py` then matches each node's terminals to that node's own frames, steps and epochs, and
the record's outcome follows the moving node. A settings transaction and a state read that includes
`1003` number steps by field slot (the readback from slot 10, unread entries skipped): for
`settings-2202`, `io-di3` and `clear-alarm` steps rise with gaps; every other route still needs 1..N. A
client abort a request's terminal names as its cleanup is that request's own second TX, with its own
TX report. Summaries:

| Line | Meaning |
| --- | --- |
| `TRIAL_PLAN` | The parsed request, only the fields the route uses: `pred` a rearm's predecessor, `c6` the cycles after `0006`, `rel` and `base` a move's relative base and witness position (`rel=0 base=0` absolute); records before 2026-11-04 lack them |
| `TRIAL_RES` | `stack0` and `stack1`, the task stack high-water mark at admission and at the terminal (a lower `stack1` was reached during the route), heap free before and after, least ever, largest free block, service gap, lost records, a refused admission, and the controller TX and RX error counters at admission and terminal (`tec0`, `rec0`, `tec1`, `rec1`; absent from records before 2026-11-03's counter change) |
| `TRIAL_FIRST`, `TRIAL_CHECK`, `TRIAL_LAST` | The first of two fresh check reads (`released-quick`, `word67`, a relative `move`), the values a prerequisite check judged (with two reads, the second), and the latest fresh read; `passed` is the check's verdict on both check reads; `blocks` names the StateBlock bits it holds (`history` ends with `01`, its closing `1001`/`603F`) |
| `TRIAL_HISTORY` | `history` only, after a successful `1003` read: `count=<1003:00>` and `e1=..e<count>=` the entries read, raw hexadecimal; an entry not read is absent, not zero. `s2_evidence.py` requires it to equal the record's own `1003` terminals |
| `TRIAL_EMCY`, `TRIAL_STATUS` | The EMCY that ended it; a library refusal with its code, detail and message |
| `TRIAL_ENABLE`, `TRIAL_ENABLE_STEP` | The enable context and each word's acknowledgement and accepted report |
| `TRIAL_STOP`, `TRIAL_BEST_EFFORT` | The requested and final path, the sequenced context with its predecessor, and both best-effort contexts; `own=1` marks this route's |
| `TRIAL_REARM` | The word (`pword`, from 2026-11-04), operation and completion time of the predecessor the rearm followed, and the session's latest word afterwards (`latest=0` after a lost `000F`) |
| `TRIAL_CONTROL` | `released-quick` and `word67`: the session's latest word at admission (`prior=0` when it knows none, as after an MCU restart), and its latest word afterwards |
| `TRIAL_BASE` | `relative` and a relative `move`: the `607A` read before the writes (`read`, `target`), the firmware's own record of the last `607A` it had acknowledged on that node at admission (`known`, `prev`, `prevTrial`, `prevRelative`; absent after any `607A` that may have been written unacknowledged, or across a node boot-up), and a relative move's witness (`basis`, `witness`). None of them measures what the drive adds a displacement to |
| `TRIAL_EVIDENCE` | The move's evidence exactly as the line gave it: record, signals, rule, raw identity, and `rb` its relative base |
| `TRIAL_SETTINGS`, `TRIAL_LIMITS`, `TRIAL_IO`, `TRIAL_INPUTS` | A settings transaction's result (prepared, unchanged, state, outcome, changed and acknowledged fields, effects, uncertainty), the configuration epoch before and after its write and whether the owner and the session followed; the group raw, `which=0` the prior read and `which=1` the matched readback; `io-di3`'s `2101` and `60FD` after it |
| `TRIAL_OBSERVER`, `TRIAL_OBSERVER_CHECK`, `TRIAL_OBSERVER_LAST`, `TRIAL_NODES` | A move's observer: its check, cycles, whether any read showed motion, the `6064` range, largest `606C`, `2100` of all cycles combined, an EMCY; its check and last reads; both nodes' copied RX frames and those dispatch did not accept, the service gap, and the interface's dropped, unmatched and lost counters at admission and terminal (a dropped frame has no node) |
| `TRIAL_CLEAR`, `TRIAL_CLEAR_STEP`, `TRIAL_INCIDENT`, `TRIAL_BASELINE`, `TRIAL_EXTRA` | The session's clear context (kind, state, outcome, execution, completion, a 1 that may remain, withdrawn coordinates, `clearBlocked`), each write's terminal by slot, the alarm incident or the counter baseline, and the extra observation, raw |
| `TRIAL_MOVE`, `TRIAL_MOVE_SAMPLE` | The session's `MoveContext`: state, final phase and phases seen, outcome, interruption, setup, trigger, completion and rearm execution, uncertainty, cycles, fence; then its admission, latest, acceptance, absence and completed samples, raw |

`failure` values: 0 none, 1 grant, 2 admission, 3 service, 4 correlation, 5 evidence loss,
6 invalidated, 7 deadline, 8 prerequisite, 9 observation, 10 action, 11 refused, 12 aborted.
`s2_evidence.py` reads every line, checks the request, plan, summary count and, for the four read
routes, the fixed inventory (for `history`, as many entries as its own `1003:00` reply names), and
gives the verdicts above. A record that reports `lost=1` cannot be
accused by matching its terminals to the copied traffic it says is incomplete: those findings stay
as warnings and the verdict is `failed-evidence`. In the transition log the enable, stop and move
summaries publish under their own names (`enableOutcome`, `stopPath`, `stopOutcome`, `moveOutcome`,
`moveCompletion`, `moveUncertain`, `moveFenced`), so a move and its planned stop in one record are
never one signal. `scripts/s2_bench_test.py --transcript` replays a record of every route that the
real `BenchRoute` printed against a synthetic drive, and one move whose copied traffic overflowed.

```text
python scripts/s2_bench.py --port COM15 --serial 94A990398B40 --command readiness-actions --node 1 --trial 101 --fault-led prompt --output NEW_DIRECTORY
python scripts/s2_bench.py --port COM15 --serial 94A990398B40 --command readiness-history --node 1 --trial 102 --fault-led prompt --output NEW_DIRECTORY
python scripts/s2_bench.py --port COM15 --serial 94A990398B40 --command mode1 --node 1 --trial 104 --cycles 5 --interval-ms 500 --campaign CAMPAIGN.jsonl --new-campaign --output NEW_DIRECTORY
python scripts/s2_bench.py --port COM15 --serial 94A990398B40 --command enable --node 1 --trial 105 --status 0x8037 --position-target 5000 --velocity-target 120 --deadline-ms 10000 --interval-ms 800 --no-motion-write-recorded --campaign CAMPAIGN.jsonl --output NEW_DIRECTORY
python scripts/s2_bench.py --port COM15 --serial 94A990398B40 --command stop --node 1 --trial 106 --kind quick --deadline-ms 2000 --best-effort-ms 0 --interval-ms 200 --campaign CAMPAIGN.jsonl --output NEW_DIRECTORY
python scripts/s2_bench.py --port COM15 --serial 94A990398B40 --command position --node 1 --trial 107 --start-position 0 --target 16 --rpm 1 --ramp-up-ms 1000 --ramp-down-ms 1000 --cycles 4 --interval-ms 250 --stop-kind halt --stop-after 2 --stop-deadline-ms 2000 --stop-best-effort-ms 0 --stop-interval-ms 250 --campaign CAMPAIGN.jsonl --output NEW_DIRECTORY
python scripts/s2_bench.py --port COM15 --serial 94A990398B40 --command rearm --node 1 --trial 108 --predecessor position-halt --cycles 4 --interval-ms 250 --campaign CAMPAIGN.jsonl --output NEW_DIRECTORY
python scripts/s2_bench.py --port COM15 --serial 94A990398B40 --command move --node 1 --trial 109 --start-position 16 --target 32 --rpm 1 --ramp-up-ms 1000 --ramp-down-ms 1000 --deadline-ms 10000 --interval-ms 1000 --record RECORD --accept-source SOURCE --accept-mask MASK --accept-value VALUE --complete-source SOURCE --complete-mask MASK --complete-value VALUE --rule RULE --device-type RAW --hardware-version RAW --software-version RAW --vendor-id RAW --product-code RAW --revision RAW --model-code RAW --firmware-code RAW --stop-kind none --campaign CAMPAIGN.jsonl --output NEW_DIRECTORY
python scripts/s2_bench.py --port COM15 --serial 94A990398B40 --command released-quick --node 1 --trial 110 --cycles 4 --interval-ms 250 --campaign CAMPAIGN.jsonl --output NEW_DIRECTORY
python scripts/s2_bench.py --port COM15 --serial 94A990398B40 --command word67 --node 1 --trial 111 --word6-cycles 2 --word7-cycles 6 --interval-ms 250 --campaign CAMPAIGN.jsonl --output NEW_DIRECTORY
python scripts/s2_bench.py --port COM15 --serial 94A990398B40 --command relative --node 1 --trial 112 --start-position 0 --target 16 --rpm 1 --ramp-up-ms 1000 --ramp-down-ms 1000 --cycles 4 --interval-ms 250 --stop-kind none --campaign CAMPAIGN.jsonl --output NEW_DIRECTORY
python scripts/s2_bench.py --port COM15 --serial 94A990398B40 --command move ... --move-base actual --target 16 --stop-kind none --campaign CAMPAIGN.jsonl --output NEW_DIRECTORY
python scripts/s2_bench.py --port COM15 --serial 94A990398B40 --command move ... --deadline-ms 6000 --interval-ms 2000 --observer 2 --stop-kind none --campaign CAMPAIGN.jsonl --output NEW_DIRECTORY
python scripts/s2_bench.py --port COM15 --serial 94A990398B40 --command settings-2202 --node 1 --trial 114 --soft-limit-code limits-off --fault-led prompt --campaign CAMPAIGN.jsonl --output NEW_DIRECTORY
python scripts/s2_bench.py --port COM15 --serial 94A990398B40 --command io-di3 --node 1 --trial 115 --di3-change function-user0 --fault-led prompt --campaign CAMPAIGN.jsonl --output NEW_DIRECTORY
python scripts/s2_bench.py --port COM15 --serial 94A990398B40 --command counter-zero --node 1 --trial 116 --deadline-ms 3000 --interval-ms 500 --campaign CAMPAIGN.jsonl --output NEW_DIRECTORY
python scripts/s2_bench.py --port COM15 --serial 94A990398B40 --command clear-alarm --node 1 --trial 117 --deadline-ms 3000 --interval-ms 500 --campaign CAMPAIGN.jsonl --output NEW_DIRECTORY
```

`--predecessor` is `position-trigger`, `position-halt` or `relative-trigger` and is always required.
`--soft-limit-code` is `limits-off` (0) or `after-homing` (1); `--di3-change` is `function-user0`,
`function-undefined`, `polarity-set` or `polarity-clear` (cases 1..4). The settings routes ask the
attended confirmation and, with `--fault-led prompt`, the fault LED before and after, and no shaft
question. `--observer` names the other node.
`--move-base` is `absolute` (the default, as before), `actual`, whose witness is `--start-position`,
or `commanded` with `--base-position`, the reviewed commanded position; for either relative base
`--target` is the displacement.

The values are placeholders for review, not recommendations: P2 and P3 take theirs from P1's fresh
reads, and the position values need packet 19's reviewed bounds. The move's evidence has no
placeholder at all: every signal, rule and identity value comes from packet 19's reviewed
measurement record, and without one the move stays unrun. Every route value is required; the
runner refuses a missing one. Each command checks the recorded USB identity, pings, and refuses
unless the image reports `routes=5` (the line protocol above), no soak, no held
record and a started controller. It asks the
operator to type the attended confirmation, the shaft observation for `enable` and the fault LED for
`readiness-actions` and `readiness-history` with `--fault-led prompt` (both require `--fault-led`:
they upload objects whose read effect is unqualified), and after the route a free-text shaft
observation.
These go to `request.json` and are never interpreted; only the enable's `FREE` or `HOLDING` is
sent. The runner cannot tell who typed an answer: on 2026-11-05 the lead typed `HOLDING` and the
attended confirmation for trials 704 and 712 from the drives' start reports, and on 2026-11-03
relayed `HOLDING` for trial 108 with no fresh question after a power cycle, so those records hold
no observation of that power-up (see their NOTE.txt). `UNOBSERVED` is now the truthful answer when
nobody felt the shaft, and it refuses the enable before anything is sent. **With nobody at the bench**, `--unattended-waiver TEXT` replaces those questions, and only on
the user's own written statement that they accept the trial unattended: TEXT is stored verbatim in
`request.json` and the ledger's send entry, every answer only an attendant could give is stored as
`unattended: not observed`, and the line's attended field still reads 1, recording that the host's
gate was met by the waiver. It refuses `enable`, whose start report needs the attendant's power-up
shaft observation, and `--fault-led prompt`. The independent cutoff is then absent, so keep such a
campaign to bounded, slow, short cases. `--dry-run` prints the plan without enumerating, prompting or creating anything. Exit status: 0
complete, 3 a consistent record of a failure, 2 nothing admitted, 1 damaged or unfinished.

**Campaign ledger.** The firmware's caps restart with the MCU, so every motion command needs
`--campaign PATH`, a JSON-lines file the operator names for the whole campaign. Before a motion line
is written its `send` entry (route, node, trial, motion weight, planned stop, line, image) is
appended and flushed; a `result` entry follows the capture. Every send counts, admitted or refused by
the firmware. The runner refuses before prompting or opening the port once the ledger holds twelve
motion admissions, three for one route and node (a planned stop also counts against `stop`), or any
trial identity at or above the requested one. A missing ledger is refused unless `--new-campaign`
starts it, and `--new-campaign` on an existing one is refused, so a mistyped path cannot reset the
counts. A partial last line or a malformed entry is refused and never repaired. If the send entry
cannot be written nothing is sent (`reason=campaign_unwritten`). Read routes may name the ledger too;
their entries count toward trial identities only. A settings route's send entry carries `"settings": 1`
and motion 0, counts three per route and node and never toward the twelve; an entry whose mark does
not match its route is refused.

One invocation at a time holds a ledger: `PATH.lock` is created exclusively before the ledger is
read and removed after the result entry is written. A second invocation refuses with `campaign
busy`, naming the lock file. A lock left by a crash is refused the same way and never removed by the
runner: when no runner is active, check the ledger, then remove the lock yourself. After a ledger
write error, or a ledger that changed under the lock, the runner keeps the lock and says so. Right
before the send entry is written the ledger is read and checked again (`reason=campaign_refused` if
that refuses; nothing is sent). Each entry carries `seq`, its line number from 1, and `prev`, the
SHA-256 of the previous line's bytes without its line feed (64 zeros for the first). A gap, a broken
chain, a send entry whose `motion` does not match its route and `stop`, whose line does not carry
its own route, node, trial, stop kind and matching integrity field, a trial that is not an integer,
or a result entry without its send entry is refused. **The chain cannot detect a ledger cut at a line
end or an older copy restored in place: both are valid chains.** Commit the ledger together with each
session's evidence, so that git history is the anchor that shows a shortened or replaced ledger.

**Memory.** Static DRAM is unchanged by `rearm`, `move`, the integrity field and the shared plan
check: on the 2026-11-03 review-fix tree `dram0_0_seg` has 2520 bytes left (169512 used, 98.5%;
2576 were left at `01190cf`), and the map places no `BenchRoute` section in DRAM. The parser's field
tables and route grammars are in flash, and the CRC-32 is computed bit by bit with no table. On the 2026-11-03 tree `BenchRoute`
takes 41888 bytes of internal heap on the first route request (it holds the move's identity read,
one coordinate configuration per node and the move request, off the task stack), and each node's `AxisSession` 23576
bytes on its first route, with the session as in that tree. Like the soak's they are never freed, and a
failed allocation refuses with `reason=memory`. With no soak run on that boot, both sessions fit:
node 2's took the internal heap from 73,640 to 50,060 bytes free and the largest block from
49,152 to 25,600, once. Whether both sessions fit beside a soak is unmeasured; `TRIAL_RES largest`
reports it.

The routes of 2026-11-04 (`released-quick`, `word67`, `relative`, the rearm's predecessor, the
relative move) add no static DRAM: their step tables, field lists and words are in flash, and the map
still places no `BenchRoute` `.data` or `.bss`. On that tree `dram0_0_seg` has 2264 bytes left
(169768 used), the same as an image built without them earlier that day, so the 256 bytes less than
above predate these routes. `sizeof(BenchRoute)` is 42256 bytes on the S2 (368 more, for the relative move's
witness per node and the new record fields) and `sizeof(AxisSession)` 28784, measured with the target
compiler and the build's own flags. Both sessions and the route then need about 99.8 KB of internal heap
once; whether they fit beside a soak is still unmeasured. The deepest measured stack chain is a move
request from `BenchRoute::service`, 4784 bytes as a lower bound, inside the 6144-byte chain limit of
`scripts/check_stack.py`.

The settings routes, the observed move, the two clears and the hold-back add no static DRAM either
(2264 bytes left, no `BenchRoute` `.data` or `.bss`). `BenchRoute` is then 42616 bytes, and the
settings contexts and the observer's read context take a block of their own, `RouteWork`, 7712 bytes,
allocated once on the first settings route or observed move and never freed (`reason=memory` if it
cannot be). The deepest stack chain measured on that tree is 5072 bytes as a lower bound, again a move
request from `BenchRoute::service`.

**Not established.** Nothing here qualifies an enable, stop, move, scale or firmware. On one
image, the least stack ever free during the routes was 8,492 of 12,288 bytes, during a moving
HALT, and the largest service gap 5 ms ([attended report](reports/2026-11-03_attended_bench.md#resources-during-the-actions)).
The [2026-11-04 session](reports/2026-11-04_bench.md) holds the later stop and move records, and the
first hardware runs of the routes added that day, each once or twice on one or two specimens and one
firmware, judged from drive reports with no shaft observed. Their host tests use a synthetic drive
whose every response is the test's own. `clear-alarm` has not run on hardware; `settings-2202`
has written and read back `2202` codes 1 and 0 on both nodes (trials 604 to 607).

## Conditional state-changing evidence

Some questions cannot be settled by readback: velocity start, target rearm,
zero ramps, stop behavior, clear/save effects and DO truth tables need applicable
manufacturer evidence or a scoped physical experiment. The exact gates and
[experiment prerequisites](device_conflicts.md#conditional-experiment-prerequisites)
remain owned by prompt 29 and can be resumed before completed motion APIs or the
later CLI exist.

The user authorized the bounded PDO test and confirmed independent power cutoff.
State-changing sequences have since run through the bench routes: enables, moves,
stops, the counter zero, the `2202` and DI writes (2026-11-03 to 2026-11-05), and the
2026-11-06 [PDO sessions](reports/2026-11-06_pdo_stream.md)' `1017` and `1800:05`
writes and addressed NMT transitions. Each was a reviewed finite sequence; none
qualifies the firmware, and the selected-firmware edit/state prerequisites of other
sequences are still unproved. Before another runs,
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
