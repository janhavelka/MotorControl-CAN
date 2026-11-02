# ESP32-S2/S3 TWAI adapter

This application adapter uses the ESP-IDF v5.5.5 node API, pinned to
`b774170ff46c393eeb5e495ea37936038d3f4f4f`. Arduino 3.3.11 with pioarduino
55.03.311 supplies the same SDK. Read the [SDK evidence and limitations](../../docs/adapters/twai_sdk.md)
before changing SDK versions or lifecycle behavior.

The adapter belongs outside the installed portable core. One application owner
receives and dispatches the physical interface's traffic. CANopen service and
motor semantics stay in the existing shared owner/application. The adapter does
not depend on the Lichuan object dictionary.

## Application configuration

The application supplies and verifies TX/RX GPIOs, bus bitrate, controller
configuration, storage lifetime and the pinned owner task/core. The MCU requires
an external CAN transceiver suitable for the actual electrical interface, plus
appropriate wiring and termination. RS UART/DE pins and an RJ45 connector do not
establish CAN wiring. No drive node, bitrate or board pin is a working default.

Support is ESP32-S2 and ESP32-S3, Classical CAN, with power management and cache-safe
TWAI ISR execution disabled. Keep the callback context and ISR storage in
internal RAM. Task stacks, console buffers and optional traces remain
application-owned; the portable API requires no PSRAM. The
[S2-specific lifecycle proof](../../docs/adapters/twai_esp32s2.md) covers core 0,
target GPIO restrictions and controller/interrupt retirement. P4 remains pending.

Use normal acknowledgement semantics: self-test and loopback are disabled.
Set `fail_retry_cnt=0`. Configure the SDK TX queue depth to one and submit only
one active SDK frame. Use a full-open acceptance filter to preserve standard and
extended asynchronous traffic for normal owner routing. Reject FD, BRS, invalid
identifiers/DLC and unsupported frame metadata before SDK admission.

## Storage and servicing

| Storage | Bound and owner |
| --- | --- |
| Adapter TX slots | Two total, including waiting, SDK active and retained reports |
| Active SDK TX | At most one of those slots; immutable owned frame and payload |
| ISR RX ring | Sixteen entries of storage; active application-selected depth 1..16 |
| TX completion mailbox | One copied result awaiting cooperative consumption |
| Controller state/errors | Sticky bounded fault/state/counter evidence |
| SDK TX queue | Depth one, without a second SDK submission |

Callbacks copy bounded RX/TX metadata or update sticky state/errors and return. The cooperative owner
performs routing and service advancement while asynchronous RX continues.
Ring/report overflow faults the adapter; it must not silently discard evidence
and publish success. Trace output must not consume normal traffic or block
service under console pressure.

SDK submission establishes admission. A matched TX callback can establish
controller transmission; endpoint acknowledgement and motion completion need
their own protocol evidence. RTR keeps its requested DLC with zero payload.
Exact physical RX arrival timestamps remain unknown. A read-only, pinned hardware
FIFO probe and ISR observation provide [conservative receive intervals](../../docs/adapters/twai_receive_time.md)
for network freshness. Future-upper-bound entries remain queued until an accepted
service cutoff reaches them. Owner time uses explicit monotonic microseconds. The selected
ESP timer is 64-bit, so the adapter performs no 32-bit wrap extension.

## Explicit shutdown and recovery

Create, delete and recover from the same pinned owner task/core. Retire a running
node by successfully disabling and deleting it. A confirmed bus-off node permits
direct deletion under the reviewed SDK source reasoning linked above. Retain
owned TX storage until checked completion and report consumption, or successful
deletion. Keep the callback context alive throughout the node lifetime.

Disable preserves pending SDK state, and enabling the same node can transmit it
again. SDK bus recovery can also restart queued work. Owner recovery therefore
deletes the old node and creates a fresh node; it never enables or recovers the
old one. Teardown/reconfiguration failure blocks fresh work and preserves the
resources and uncertain effects needed for investigation.

Recovery is local controller work. It sends no NMT start, drive enable, fault
clear or automatic replay. Successful retirement cannot retract a frame already
sent or establish that a motor stopped.

## Verification scope

Host fakes exercise admission, TX correlation, queue/event pressure, controller
faults and retirement. SDK compilation verifies the selected interface and build
configuration. Neither check authorizes flashing or physical traffic. Hardware
qualification requires the actual board, transceiver, drive/motor/firmware,
wiring, node/bitrate and applicable user authorization.

The SDK has no public count for every hardware RX FIFO overrun, and its own RX
ISR drains until the hardware FIFO is empty. The adapter's configured capacities
therefore do not promise complete reception or a fixed interrupt duration.
The earlier compile-only S3 baseline measured 192 bytes for `TwaiAdapter` and 752
bytes for `IdfTwaiDriver` before tagged NMT transport and receive intervals.
These are historical sizes; the current compile gate reports both layouts again.
These exclude SDK allocations, the existing owner/service store and task/ISR
stacks. The adapter creates no task. SDK allocation high-water marks and actual
service gaps still require the physical fixture.

Bounds here count copied entries and SDK calls. SDK creation/deletion uses local
allocation and internal locks; no hard elapsed-time bound is established. Each
recovery poll performs one lifecycle operation without an application wait or
retry loop. The owner checks its supplied deadline between turns; it cannot
preempt an SDK call. Keep lifecycle work in the exclusive pinned owner task and
qualify its service gaps before using the adapter in a real machine.

## Integration and repeatable checks

`TwaiConfig` requires explicit `txPin`, `rxPin`, `bitrate`, `sdkTxQueueDepth=1`
and `rxQueueDepth` from 1 to 16. `samplePointPermill=0` asks the SDK to calculate
timing; a nonzero value is an application timing choice, not device evidence.
Keep `IdfTwaiDriver`, `TwaiAdapter`, the existing service store and owner stationary.
Obtain `TwaiDriver` with `IdfTwaiDriver::driver()`, pass it to `TwaiAdapter`, and
give `TwaiAdapter::transport()` to the ordinary `CanBusOwner`. Construction sends
nothing; `start(config, owner.controllerEpoch())` explicitly initializes the local
controller. It does not submit a device frame. One initial attempt is allowed;
subsequent restarts go through `CanBusOwner::recover`, not a second `start`.

On each cooperative turn, supply the same nondecreasing microsecond value to
`owner.service(nowUs, budget)` **first**, then `adapter.service(owner, nowUs)`.
The latter processes sticky controller evidence and submits at most one frame.
Unread authoritative RX postpones submission, including when the owner RX budget
is exhausted. Repeated zero RX budgets can consume a request's deadline; they do
not authorize dropping boot-up evidence to make TX progress. Platform time can
come from 64-bit `esp_timer_get_time()`; validate its nonnegative value before
conversion to `uint64_t`. `TwaiAdapter` uses supplied time; `IdfTwaiDriver` samples
the same ESP timer solely for receive bounds. No wrap extension exists. Backward
adapter time inhibits transport and invalidates at the owner's last accepted
time, including explicit owner calls between service turns. Active TX remains
uncertain until checked completion or retirement.

The final copied-ingress/fault check and zero-wait SDK handoff use one critical
section, so an already captured boot-up cannot overtake the guard. Physical
arrivals while interrupts are masked remain unknown. The
[pinned SDK proof](../../docs/adapters/twai_sdk.md#final-handoff-and-gpio-validation)
depends on one outstanding frame and the same pinned task/core. GPIO values are
bounded before the SDK's shifting validity macros; those checks do not establish
board wiring or transceiver suitability.

The owner alone consumes RX/report copies. Exact queued IDs can be retired;
an active SDK transaction remains unretired until final evidence or destruction.
`shutdown()` ends transport service and must succeed before storage is destroyed.
Live owner work requires explicit owner recovery/invalidation; shutdown never
represents a stop or a successful device operation.

From PowerShell or a Linux shell, with native tools on PATH:

```text
cmake -S . -B .build/twai-native -G Ninja
cmake --build .build/twai-native --target twai_adapter_test twai_driver_test
ctest --test-dir .build/twai-native -R "^twai_(adapter|driver)$" --output-on-failure
```

The first suite uses the real service/owner with a fake driver. The second
compiles the actual shim against narrow SDK fakes; it is not an SDK build.
Neither needs CAN hardware. Full host/package/documentation checks remain in
`python scripts/verify.py --mode full`.

For a real SDK compile, export the installed ESP-IDF **5.5.5** environment
(`export.ps1` on Windows or `export.sh` on Linux), then run:

```text
python scripts/check_twai_sdk.py --idf-path "path to esp-idf" --idf-python "path to IDF Python" --build-dir ".build/twai SDK"
```

This command checks the original-byte SDK hashes, Python/SDK/compiler versions,
configures `test/twai_sdk_compile` and builds its `__idf_main` static archive in
C++11 with strict warnings. It records target ABI sizes and retained command logs
under the chosen directory. It links no firmware and performs no flashing or
controller I/O. A missing tool, dependency or wrong SDK returns nonzero. It does
not disable SDK dependency checks or install tools. Firmware entry points and
Arduino/native-IDF builds remain prompt 28; no empty firmware job is registered.
