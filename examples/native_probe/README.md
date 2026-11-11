# Native fake probe and shared console

This offline C++11 example runs explicit typed identity (nine fields) and
communication configuration (six fields) reads through `ProbeApp`, the ordinary
`CanBusOwner` and its existing CANopen `SdoClient`. It needs CMake, a native C++
compiler and the retained pinned CANopenNode source used by the optional adapter.
It needs no board, SDK, controller, CAN interface, motor or sibling repository.

From the repository root:

```sh
cmake -S . -B build/native -DBUILD_TESTING=OFF -DMOTORCONTROLCAN_BUILD_EXAMPLES=ON -DMOTORCONTROLCAN_BUILD_CANOPENNODE=ON -DMOTORCONTROLCAN_CANOPENNODE_DIR="$PWD/experiments/canopen_service_fit/vendor/CANopenNode"
cmake --build build/native --target native_probe
./build/native/examples/native_probe/native_probe --fake
```

For a multiple configuration generator, append `--config Debug` when building
and use the corresponding `Debug` executable directory. Windows adds `.exe`.
PowerShell accepts the same `$PWD` interpolation; use `-G Ninja` when selecting
the installed GCC/Ninja toolchain. These example-only builds require no Python.
The two accepted forms are `--fake` and `--console-fake FILE`; other arguments
exit with code 2. Both run entirely offline.

`--console-fake FILE` executes a bounded command file through the same
`ProbeConsole`, `ProbeApp`, owner and platform as the default Arduino and IDF
entries. For example, save these ASCII lines as `commands.txt`:

```text
@1 version
@2 axis config set command 2000
@3 prepare absolute 9007199254740993 steps native
@4 wiring di0 unconnected
@5 config
@6 stats
```

Then run:

```sh
./build/native/examples/native_probe/native_probe --console-fake commands.txt
```

Each command produces its ordinary correlated JSONL response. The exact prepared
integer is emitted as `"effective_native":"9007199254740993"`; it never passes
through floating point. The axis response identifies an offline workspace with
`"reference_known":false` and `"origin_native":null`. No live target, interface
or bitrate is selected, so `config` reports those facts as null. `stats` reports
zero CAN traffic. `wiring` records only a host declaration; it changes no terminal
function and establishes no electrical state.

The application starts without scales, origins or a native position witness.
The explicit scale command above supplies an assumption for offline arithmetic.
`axis origin`, relative preparation and wrapped-angle preparation need fresh
caller-qualified evidence; this batch entry never invents it. Device commands
owned by prompts 41 and 42 produce ordinary rejections. `recover` follows the
real owner path; this fake has no recovery callbacks, so its retained terminal
reports failure. A retained terminal can be inspected with `result` and freed
with `release ID`. [The CLI contract](../../docs/cli_contract.md) lists the working
grammar and result meanings.

The entire file is read and checked before any command executes: at most 4096
bytes, ASCII/TAB line contents, at most 255 bytes per line and 24 command tokens.
LF, CR and CRLF delimiters are accepted; every nonempty file must end with a
line terminator. EOF never supplies a missing terminator to a truncated command. An unreadable,
oversized or structurally invalid file exits with code 2 and executes nothing.
Command grammar and capability errors remain normal console responses; exit zero
means the batch completed, independently of each response's `ok` value.

Each turn advances the synthetic clock by one microsecond, services the owner and
application first, then feeds at most eight input bytes and services the shared
console. Commands are paced behind their complete output, including expanded
`help` and `caps` listings. The fixed capture accepts at most 64 KiB and applies
backpressure when full. No stdin or stdout operation runs during owner service.
After draining, the program writes the captured bytes to stdout. Capture pressure,
the 2,000,000-turn budget, output failure or unexpected CAN traffic exits with
code 1; an unfinished captured record is removed through the last newline before
publication. The batch entry performs no automatic retry.

The caller supplies synthetic logical axis 9, node 5, profile token 1, binding,
configuration and node epochs 1, SDO IDs `0x605/0x585`, heartbeat `0x705` and EMCY
`0x085`. These are fixture choices, not detected device settings or recommended
hardware defaults. Initial service usability and session freshness are explicitly
assumed for this fixture. Creating an owner, selecting a target, or obtaining
cached health establishes neither premise. Q20 read effects remain unresolved;
this is not a harmless hardware bootstrap procedure.

Expected output reports `FAKE ONLY`, nine identity fields, six configuration
fields, 15 explicit uploads, interleaved heartbeat/EMCY traffic and 15 unrelated
node 6 heartbeat frames, plus native `sizeof` storage measurements for the fake,
service, owner, application and platform on the selected ABI. These measure the
named instances, excluding call stacks and C runtime storage. The invented
identity data never establish a supported
model: `modelMatched=false`. Configuration values are observed without adopting
routes or timing. Each owner terminal is inspected repeatedly before explicit
typed advancement releases it; the final typed observation is copied before
`releaseRead()` clears its context.

The fake owns a 16-frame ingress ring and four TX-report slots. A handoff first
checks space for all its passive frames, response and report; refusal changes
none of those queues. Each admitted upload stages three passive frames and one
response (silent test mode omits the response). Separate final TX reports provide
synthetic controller evidence. One owner alone dequeues and routes the frames.
The supplied clock progresses by one microsecond per bounded turn; the 100 us
queue/response budgets and 1000 us whole-read budget are example policy choices.

`ProbePlatform` provides 64 input bytes and 512 output bytes, with at most eight
input polls and 16 output attempts per service turn. Full input drops newest
bytes and counts them; whole output submissions are refused and counted when
capacity is insufficient. Partial output acceptance preserves the remaining
suffix. The `--fake` read demonstration supplies only a clock callback; the batch
mode supplies its bounded output capture and uses the shared parser's `feed`
path. Focused `probe_app` and console tests exercise pressure while ordinary CAN
service continues. No live mode, implicit writes, NMT commands, retries or physical
qualification is provided.

Default Arduino and IDF firmware entries instantiate the same shared console in
their sole owner task. They service it after `FirmwareProbe::service`, including
while CAN remains inactive; accepting console input never starts the controller.
Accepted board configuration supplies host interface/bitrate facts without
claiming the controller is running. An explicit test turn hook, including the
optional IDF smoke hook, owns its input exclusively and keeps its separate finite
test protocol; it does not compete with the ordinary console for bytes.


Prompt 41's `--console-fake FILE` establishes one synthetic slot0 (node5), then
waits for an explicit `host 0` or `useaddr 5`. This is fixture configuration, not
physical discovery or a board default. `probe` and `read identity` run the real
shared typed lifecycle; `read config` returns Q20 before transmission. Use the
script in [the CLI contract](../../docs/cli_contract.md#typed-device-increment-prompt-41).
Each finite command is paced behind complete reply/terminal delivery. The batch
still uses at most4096 input bytes,65536 output bytes and2000000 turns; there is
no hardware backend. Device-capability gates and uncertainty are real application
outcomes. The separate `--fake` demonstration keeps its original explicit script.

Prompt42 compiles the same `ProbeConsoleMotion`, `MotionIntent`, `ProbeMotion` and
`AxisSession` sources in native/Arduino/IDF. [The motion CLI contract](../../docs/cli_contract.md#finite-motion-and-actions-prompt-42)
records exact command grammar, units, deadlines and stop/result retention. Default
startup selects/qualifies no motion context. An application that supplies an actual
registered session and exact native prerequisites can attach that same public
`ProbeMotion` bridge; no CLI command grants missing firmware or physical evidence.
`motor-release` stays caps-only gated; `position-clear` is zero-only and sends no save.
Native tests use manufactured owner/server events; compile/fake results qualify no
movement, stopping, wiring, service latency or output delivery on a physical fixture.
The shared IDF S2 console uses UART0/core0; Arduino S2 remains blocked by the pinned
SDK console conflict. S3 builds do not stand in for S2 evidence.
