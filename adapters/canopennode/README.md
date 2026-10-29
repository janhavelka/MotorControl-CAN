# Optional CANopenNode SDO adapter

`MotorControlCAN::CANOPEN::SdoClient` is a stationary cooperative application
adapter around the selected CANopenNode client. It stages Classical frames;
it never opens a controller or performs I/O. Read the
[service contract](../../docs/canopen_services.md) for ownership, deadlines,
effect uncertainty, initialization premises and permanent quarantine.

The installed/exported core includes only portable service values and codecs.
This optional adapter and CANopenNode are excluded from that core distribution.
The current port supports explicitly checked little-endian Windows/Linux native
targets; SDK/ISR/multithreaded ports remain separate work. A controller must not
call its native callbacks independently of the application receive owner.

## Build in a full checkout

Use CMake 3.19+, a C99/C++11 toolchain and Ninja. No SDK, Python, network or
generator is needed to compile. The exact retained upstream source pin is
`9b8beed8367241e96ac03f916cd5a500bcb2cf23`; the optional CMake path checks all
source/license hashes against the existing provenance manifest. Another
upstream revision requires an explicit reviewed adapter update.

From PowerShell at the repository root:

```powershell
$stackSource = (Resolve-Path experiments/canopen_service_fit/vendor/CANopenNode).Path
cmake -S . -B ".build/sdo adapter" -G Ninja -DBUILD_TESTING=OFF -DMOTORCONTROLCAN_BUILD_CANOPENNODE=ON "-DMOTORCONTROLCAN_CANOPENNODE_DIR=$stackSource"
cmake --build ".build/sdo adapter"
```

From a Linux shell:

```sh
cmake -S . -B '.build/sdo adapter' -G Ninja -DBUILD_TESTING=OFF -DMOTORCONTROLCAN_BUILD_CANOPENNODE=ON -DMOTORCONTROLCAN_CANOPENNODE_DIR="$PWD/experiments/canopen_service_fit/vendor/CANopenNode"
cmake --build '.build/sdo adapter'
```

Application source targets link `MotorControlCAN::CanopenNode` and include
`SdoClient.h`; the target supplies its declared stack/core dependencies. The
adapter target is deliberately not installed or exported as core support.
The retained source is Apache-2.0 with its original notices; adapter/core code
is MIT. Neither grants manufacturer capability or physical qualification.

For registered offline tests, configure the ordinary repository build and run
`ctest --test-dir <build> -R 'sdo_transfer|sdo_reconciliation' --output-on-failure`. The fake server
exercises the actual selected engine with independent literal vectors. One
configured channel uses explicit route/source/fresh-session evidence; tests
with `true` qualification flags are synthetic fixtures, never live defaults.

## Application order

1. Establish unique physical routes, fresh local/remote session premises and
   applicable non-changing confirmation; configure slots explicitly.
2. Supply copied `SdoTransfer` intent and a nonrenewable absolute deadline to
   `admit`. The profile caller must already have checked operation permission.
3. Call bounded `service(nowUs)` and offer owner-received frames to `receive`.
   Continue dispatching frames it does not consume to other ordinary handlers.
4. At the actual controller submission boundary, call `nextTx` and immediately
   submit the returned owned frame. If application staging delays submission,
   preserve its token and recheck `canTransmit(id, nowUs)` immediately before a
   one-shot enqueue serialized with cancellation/invalidation. This cannot recall
   frames already accepted by a controller. The handoff already counts as possibly
   transmitted; never replay it after cancellation or expiry. Report actual TX/ACK with
   its `SdoTxId` through `confirmTx`; this is separate from SDO success.
5. Inspect the non-consuming terminal `result`, copy needed evidence and
   explicitly `release`. Continue servicing bounded cleanup if it was staged.
   Report bus-off/reboot independently; never clear quarantine through release,
   quiet time, client abort or replacing an object on the same physical route.

The timestamp overload `receive(frame, receivedUs, nowUs)` accepts qualified
capture time in the supplied processing clock domain. Processing-time expiry
still wins over earlier capture; invalid/future capture rejects atomically.
`observation(slot)` retains the latest copied raw frame and both times separately
from the immutable terminal. It survives release; copy it for longer history.
Observed idle/duplicate/pre-handoff responses quarantine the route. `invalidate`
withdraws a binding without reopening or erasing earlier possible effects.

Two slots mean two explicit physical channels, not simultaneous aliases for
one reply ID. The application owns all scheduling, controller queues and RX.
No retry, motion command, stop, fault clear or recovery action is implicit.

## Passive node evidence

Include `NetworkState.h` with `SdoClient.h`. `configureNetwork` reserves passive
routing in the same two-slot table, with explicit heartbeat/EMCY IDs and a
borrowed stationary `NetworkObserver` for actual cache invalidation. It can
precede SDO setup; one anchor per physical node is shared by its SDO channels.
Offer every frame once through the ordinary receive path, preserving qualified
capture time and ingress binding/epoch when available. No native HB/EM consumer
runs in parallel; the [decision correction](../../docs/decisions/canopen_stack.md#prompt-15-passive-boundary-correction)
records the source-backed boundary.

`node(slot)` exposes independent NMT, heartbeat, effective producer interval,
checked SDO and raw EMCY facts. `takeNetworkObservation` consumes copied history
from four fixed records; overflow drops newest while lifecycle invalidation
continues. Report controller recovery/bus-off separately. AVAILABLE and a new
heartbeat cannot restore an invalidated cache or reopen an SDO channel. Read
the [service guide](../../docs/canopen_services.md#passive-network-events-and-node-lifecycle)
for timestamps, generation exhaustion and callback lifetimes. Focused host
commands are `ctest --test-dir <build> -R network --output-on-failure`.
