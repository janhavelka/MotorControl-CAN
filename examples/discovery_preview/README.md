# Offline discovery preview

This native example runs the shared `DiscoveryScan` through the ordinary
`ProbeApp`, `CanBusOwner` and `SdoClient`. The existing `FakeTransport` answers
one explicit `1000:00` upload and interleaves synthetic heartbeat/EMCY traffic.
Only the owner receives frames. No SDK, controller, physical CAN or motor is used.

The application supplies one already configured route, a current interface
bitrate, finite service budgets and an immutable deadline. These are synthetic
fixture premises, including exclusion of old SDO responses; the example does
not establish them for hardware. The bitrate is never detected or changed.
The result records responder presence and raw device type. It does not select
a model, firmware, motor kind or feedback capability. DEC-1 permits the upload
while its read effects remain unknown.

Build from the repository root with the optional application adapter:

```text
cmake -S . -B .build/discovery-preview -G Ninja -DBUILD_TESTING=OFF -DMOTORCONTROLCAN_BUILD_EXAMPLES=ON -DMOTORCONTROLCAN_BUILD_CANOPENNODE=ON -DMOTORCONTROLCAN_CANOPENNODE_DIR="$PWD/experiments/canopen_service_fit/vendor/CANopenNode"
cmake --build .build/discovery-preview --target discovery_preview
```

Run `.build/discovery-preview/examples/discovery_preview/discovery_preview` on
Linux, or `& '.build/discovery-preview/examples/discovery_preview/discovery_preview.exe'`
in PowerShell. The pinned CANopenNode dependency setup is described in the
[adapter guide](../../adapters/canopennode/README.md).

Expected output:

```text
Synthetic node 5 responded: 1000:00 = 0x12345678.
1 upload, 1 occupied slot, 1 free slot; no model or firmware match.
FAKE ONLY: read effects remain unknown; no hardware qualification.
```

See [commissioning](../../docs/commissioning.md) for separate host selection,
passive observation, minimal probe and explicit identity reads. A timeout or
possible-TX cancellation can permanently consume an unresolved channel slot;
releasing a finding does not clear it. No scan sends a drain marker, NMT,
download, reset or retry. This example demonstrates no physical qualification.
