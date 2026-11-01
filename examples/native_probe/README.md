# Native fake probe

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
Only the exact argument `--fake` is accepted; other arguments exit with code 2.

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

`ProbePlatform` provides 64 input bytes and 256 output bytes, with at most eight
input polls and 16 output attempts per service turn. Full input drops newest
bytes and counts them; whole output submissions are refused and counted when
capacity is insufficient. Partial output acceptance preserves the remaining
suffix. The fake demonstration supplies only a clock callback; focused
`probe_app` tests exercise byte I/O pressure while ordinary CAN service continues.
No input parser, firmware startup, live mode, writes, NMT commands, retries or
physical qualification is provided.
