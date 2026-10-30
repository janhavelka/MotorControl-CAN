# Offline application owner preview

This native executable connects `prepareControlRead`, `nextDriver`, owner
`admit`/`service`/`result`/`release`, `advanceDriver` and checked `getControl`.
It uses the actual optional CANopenNode SDO adapter and the application owner.
The application reserves a sequence and operation, copies each retained service
terminal, advances the typed context outside the owner, and releases the terminal
before yielding the next upload. It releases the sequence after publication.

The fixed four-entry fake ingress and controller-report rings supply two expedited
uploads of 2500:00 and 2501:00. Heartbeat and EMCY frames pass through the same
owner receive dispatch between each request and reply. Literal values 2 and 1000
are synthetic, with no physical encoder-scale or activation claim. Zero-code
EMCY events do not perform fault clearing. No controller, hardware, SDK, network
connection or system clock is opened; console printing is the only external I/O.

Node 5 and the selected 0x605/0x585 SDO, 0x705 heartbeat and 0x085 EMCY routes are
fixture choices following the documented identifier forms, not discovered live
configuration. The original EDS 1200:01/:02 and 1014 route metadata and the
reviewed [network evidence](../../docs/reports/prompt_3_network_review.md) describe
their source scope. Logical axis 9 is independent of node 5.

The two `true` arguments to `configure` explicitly supply illustrative external
usable-service and fresh-session premises. They do not demonstrate a qualified
Lichuan probe. A physical integration must establish exclusive SDO ownership,
retirement of old local/remote traffic and absence of indistinguishable delayed
duplicates, including between exchanges. Local request IDs cannot prove those
facts. Uploads retain `UNQUALIFIED` read effects; this preview does not resolve Q20.
All supplied microsecond budgets are application fixture policy.

## Build and run

From the repository root in PowerShell with CMake, Ninja and a C++11 compiler:

```powershell
$ownerPreviewStack = (Resolve-Path experiments/canopen_service_fit/vendor/CANopenNode).Path
cmake -S . -B .build/owner-preview -G Ninja -DBUILD_TESTING=OFF -DMOTORCONTROLCAN_BUILD_EXAMPLES=ON -DMOTORCONTROLCAN_BUILD_CANOPENNODE=ON "-DMOTORCONTROLCAN_CANOPENNODE_DIR=$ownerPreviewStack"
cmake --build .build/owner-preview --target owner_preview
```

For a POSIX shell, use the same CMake commands with
`"-DMOTORCONTROLCAN_CANOPENNODE_DIR=$PWD/experiments/canopen_service_fit/vendor/CANopenNode"`
in place of the PowerShell variable argument. The source directory must be absolute.

The optional adapter checks its required CANopenNode source pin and hashes using
`experiments/canopen_service_fit/UPSTREAM.json`. The retained source above avoids
a download; another source directory must match that same pin. This example
requires that optional dependency. The installed portable core package excludes
the application owner and adapter.

On Linux run `.build/owner-preview/examples/owner_preview/owner_preview`.
In PowerShell run
`& '.build/owner-preview/examples/owner_preview/owner_preview.exe'`.
Success prints two retained uploads and two passive EMCYs, and exits zero.
