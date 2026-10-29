# CANopen service fit proof

The original prompt-5 experiment selected CANopenNode at
`9b8beed8367241e96ac03f916cd5a500bcb2cf23`. Prompt 13 promoted its client,
FIFO, retirable staging and terminal storage into the
[optional production adapter](../../adapters/canopennode/README.md).
`fit_proof.h/.cpp` is now a compatibility facade over that adapter, preserving
these small test fixtures without a second engine or port implementation.
The eleven pinned upstream blobs and original Apache-2.0 notices remain
unchanged; see [UPSTREAM.json](UPSTREAM.json).

From either PowerShell or a Linux shell at the repository root:

```text
cmake -S experiments/canopen_service_fit -B .build/sdo-fit -G Ninja
cmake --build .build/sdo-fit
ctest --test-dir .build/sdo-fit --output-on-failure
```

CMake, Ninja and C99/C++11 compilers suffice. No SDK, network, Python or device
is needed. The standalone build now also builds the portable core and selected
adapter with their declared dependencies. The repository build registers this
proof alongside the larger independent production lifecycle harness.

## Responsibility and bounds

One synthetic node-1 route is explicitly configured as externally fresh and
confirmed, using IDs 0x601/0x581. Those fixture facts are not live defaults or
Lichuan applicability. The facade reserves adapter slot 0 and retains its legacy
one-request/result API and copied asynchronous event counters. Slot 1 remains
unconfigured. Requests now supply exact upload sizes instead of learning their
expected representation from a received response.

The application supplies all time and dispatches frames once. The production
adapter owns one actual CANopenNode client per configured channel, drains its
bounded FIFO and preserves immutable overall cutoffs. The facade owns no
additional protocol engine, callback or TX queue. Its old timeout diagnostic
marker remains compatible but does not turn a local timeout into a peer abort.
Current service-call bounds and cleanup behavior are documented in the
[service guide](../../docs/canopen_services.md), with production Doxygen beside
the implementation. Construction itself cannot establish a real fresh session.

## Executed checks

The retained executable covers expedited read/write, ten-byte segmented
read/write, copied caller data, interleaved synthetic heartbeat/EMCY/PDO,
unsigned aborts, malformed routing/command checks, stalled staging, exact
deadlines, cancellation before/after handoff, transport refusal, retained
terminals and no replay. The
[production fake-node tests](../../test/sdo_transfer_test.cpp) additionally
exercise all supported sizes through 32, toggle/final/size faults, explicitly
unsupported segmented upload, bounded abort cleanup, both channel slots,
route aliases, initial confirmation premises, reboot and bus-off.

The prior 28-byte upload limitation is closed by production FIFO draining.
Historical prompt-5 size/behavior measurements remain scoped to that revision;
current Windows x86-64 adapter size is 1464 bytes and facade size 1624 bytes.
These overlap and are not embedded RAM, call-stack or timing qualification.

## Adapter premises still requiring production work

The selected application adapter is implemented; physical SDK/controller ports,
real queue retirement, network event interpretation, full application-owner
integration and evidence-backed reopening remain their assigned later stages.
Quarantine already blocks ambiguous channels and survives release. Neither a
quiet interval nor a client abort reopens one. The fake fixture does not measure
bus load, service gaps, wire timing, a drive/motor or any stop behavior.
