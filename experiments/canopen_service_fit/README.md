# CANopen service fit proof

This is a host-only Prompt 5 experiment, with provisional local types. It executes
the unmodified pinned CANopenNode SDO client; it is not a MotorControl-CAN public
library, complete controller port or qualified drive implementation. Synthetic
node 1 / object 0x2000 fixtures make no Lichuan capability claim. There is no
device I/O, motor command, SDK, Python or sibling dependency in this build.

The upstream revision, source paths, sizes and SHA-256 values are recorded in
[UPSTREAM.json](UPSTREAM.json). The eleven original files in
[vendor/CANopenNode](vendor/CANopenNode/LICENSE) retain their Apache-2.0 notices and
license. Local proof code is marked MIT. There is no install/export target; this
vendor material must not become part of the distributable portable core.

From the repository root, with CMake 3.19 or later, a C99/C++11 compiler and Ninja:

```text
cmake -S experiments/canopen_service_fit -B .build/canopen_service_fit -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build .build/canopen_service_fit
ctest --test-dir .build/canopen_service_fit --output-on-failure -V
```

Another supported native CMake generator can replace Ninja. Configure checks each
vendored source against the SHA-256 manifest, and build reconfigures when a checked
input changes. The build performs no downloads. Prompt 7 will register this proof;
it currently has its own small CMake/CTest entry point. Assertions in the test
remain active with `NDEBUG`.

## Responsibility and bounds

The caller owns one stationary `CanBusOwner`. It owns the actual `CO_SDOclient_t`,
one 32-byte stack FIFO, one copied controller TX queue, one retained completion
and one last-event snapshot. Admission calls the upstream initiate/buffer-write
functions and copies the entire write payload before returning. Receive callback
objects point to the stationary client context. RX bytes are call-only borrowed;
the upstream callback copies eight bytes before returning. OD initialization
copies a temporary 0x1280 record; local OD transfers and dynamic OD extensions are
disabled, so no pointer to that record survives construction.

Only `receive` dispatches supplied frames. It rejects extended/RTR/wrong-length
frames at this CANopen route while `FitFrame` retains their format. It copies
synthetic heartbeat, EMCY and PDO events without diverting traffic to a second
receiver. These counters prove asynchronous delivery during an SDO transfer;
they do not implement heartbeat freshness, NMT boot invalidation, EMCY history,
PDO map qualification or application health. Their production stack/owner
integration remains a later stage.

`service(nowUs)` performs at most one upstream process call, with supplied
monotonic microseconds and no clock read or wait. `receive` makes at most two
such calls: before dispatch to enforce the deadline, and after the matching SDO
callback so the one-frame upstream mailbox is processed before another RX frame.
It handles one supplied frame, with no hidden drain loop. There is no retry loop.
The caller separately chooses when `handoff(nowUs)` permits a queued frame to become
possibly transmitted. That event makes no CAN ACK or endpoint-acceptance claim.
It checks the absolute deadline itself before transferring the frame, so an
expired queued request cannot escape merely because no service turn intervened.
Queued frames own a copy distinct from the upstream TX staging buffer.

There is one request/result slot. A terminal is copied once and viewed
non-consumingly through `result(id)` until matching `release(id)`. Foreign/stale
local handles fail. Local admission serials are never transmitted and establish
no SDO wire correlation. Cancellation before any handoff retires queued fake TX
with a known absence of transmission. Cancellation or timeout after a write
handoff retains possible execution. Release does not erase the route interlock.
Any failure after handoff prevents subsequent admission on this proof instance;
an adapter fault also interlocks it. No recovery/reconciliation API is invented.

The nonzero timeout is an explicit test/application policy in milliseconds. The
owner additionally enforces an absolute admission deadline, including time spent
waiting for TX handoff. At the exact deadline, timeout precedes RX dispatch. This
is stricter than the upstream response timer. No value here is a CL86-C firmware
timing guarantee. Nonmonotonic supplied time is ignored without mutation.

## Executed checks

The executable checks expedited upload/download and a two-segment upload/download,
mutation of the caller's write buffer after admission, retained upload bytes,
interleaved heartbeat/EMCY/PDO copies, terminal retention/release, local handle
ownership, bounds/refusal without changed output, controller queue backpressure,
and extended/RTR/wrong-length/wrong-ID filtering. It also checks:

- Wrong-object server abort filtering, native abort-code retention, malformed
  response command rejection and first-terminal retention across later turns.
- Correct 0x40 upload requests for 4/2/1-byte 0x43/0x4B/0x4F responses. Those
  response bytes, incorrectly used as requests in the reviewed third-party
  example, are never generated as upload requests here. They are valid inbound
  responses and are not rejected merely because that example misused them.
- Absolute timeout with a response exactly at the deadline, and with a TX buffer
  still full; neither a frame nor backpressure postpones owner closure. A direct
  handoff at the deadline without an intervening service turn also closes first.
- Cancellation before service, after queue acceptance and after irreversible
  handoff; no queued frame survives local cancellation and no write is replayed.
- A send failure latched by the fake adapter, since the upstream process ignores
  `CO_CANsend`'s return. No queue acceptance means no possible write execution in
  this injection; a real port must distinguish other failure/handoff evidence.

Measured on Windows x86-64, MinGW GCC/G++ 15.1.0, Release, C99/C++11 (pointer and
`size_t` 64 bits, `bool_t` 8 bits): `FitFrame` 16 bytes, `CO_CANmodule_t` 88,
`CO_SDOclient_t` 176, `Completion` 56 and the complete owner 392. These are `sizeof`
measurements, excluding compiler stack frames and executable sections; they are
not an embedded RAM budget or stack high-water measurement. The program prints
its own ABI and sizes so another toolchain can report its actual values.

Setup and steady-state proof paths contain no allocation. This is a source
inspection of the selected C files/configuration and local code, not an allocator
instrumentation measurement. The disabled FIFO ASCII helpers include allocation-
related standard headers but are not compiled into this configuration. The full
`CO_new` stack allocator is not built. No full-stack allocation guarantee follows.

The 32-byte storage is not a promise that every upload through 32 bytes completes.
Without incremental FIFO draining, ordinary seven-byte segments can fill 28 bytes
and stall before the next request; the owner then times out without publishing a
truncated success. This proof exercises a ten-byte segmented upload. Prompt 13
owns production draining, complete capacity-boundary and transfer-form tests.

## Adapter premises still requiring production work

This fake provides a strong retire-before-handoff guarantee; a hardware driver's
accepted queue or active mailbox may already be on the wire. `CO_SDOclientClose`
only changes stack state. The owner separately retires fake queued frames,
including any locally generated abort. It does not automatically transmit that
abort or claim the peer has cancelled. Actual SDK software-queue clearing alone
must not be projected as retirement of an active hardware frame. Prompt 6 and
the controller adapter stages must freeze the real evidence vocabulary.

The actual SDO client has one pending eight-byte receive mailbox, ignores
wrong-length frames, accepts server aborts without checking their object address,
and returns idle success if called again after some terminal errors. The wrapper
therefore filters addressed replies/aborts, processes matching frames promptly,
and latches the first terminal. It separately observes adapter send faults and
enforces an absolute deadline before invoking the upstream RX callback. These
are deliberate application-boundary requirements, not changes to vendor source.

Construction assumes a fresh externally qualified route. A successful exchange
does not prove that duplicate old responses cannot later arrive. This proof does
not solve an identical late SDO response on a reused route, controller reboot,
bus-off, node rebinding or simultaneous-channel retirement. A local generation
cannot solve that ambiguity. Production admission needs evidenced channel
retirement/reconciliation; reconstructing this owner is not such evidence.

The local port assumes a little-endian host and a single cooperative caller.
Its no-op locks/flags are unsuitable for ISR or multithreaded callbacks. It tests
no scheduling latency, real CAN arbitration, bus load, board memory placement,
hardware cancellation, drive behavior or firmware variant. Block transfer,
streaming larger than 32 bytes, local OD transfer, NMT/SYNC production, actual
heartbeat consumers, PDO mapping and EMCY decoding require their own integrated
services and host tests. They are not additional active engines in this proof.
