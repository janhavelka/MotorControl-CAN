# Offline typed reads preview

This native console example calls `prepareIdentity`, `nextRead`, `advanceRead`
and checked `getIdentity` through nine finite uploads. Its independent literal
bytes are synthetic. The route, identity codes, generations, supplied times and
qualified successful `SdoCompletion` records are explicit fixture premises,
not captured traffic, device defaults or manufacturer capability evidence.
No controller, CANopen stack, clock, SDK or hardware is accessed.

A real application obtains retained completions from its sole qualified service
owner. Copying a host operation or generation does not qualify an indistinguishable
late wire reply. This example supplies that premise to demonstrate only the public
typed lifecycle and its owned values.

The output keeps hardware/software version and model/firmware codes as raw U16
values. `modelMatched` remains false. The conservative interval includes each
service's admission and completion: `[0, 83]` microseconds with 83 microseconds
of skew. These supplied times do not form an atomic device snapshot.

Read effects remain unqualified. Successful synthetic uploads retain
`effectPossible=true` and `effectUncertain=false`; success does not establish a
harmless read. `prepareProbe` reports `UNSUPPORTED` with the Q20 read-effect
blocker and yields no work. The preview does not close that evidence gap.

## Build from the source tree

Run from the repository root with CMake, Ninja and a C++11 compiler:

```text
cmake -S . -B .build/reads-preview -G Ninja -DBUILD_TESTING=OFF -DMOTORCONTROLCAN_BUILD_EXAMPLES=ON
cmake --build .build/reads-preview --target reads_preview
```

On Linux run `.build/reads-preview/examples/reads_preview/reads_preview`.
In PowerShell run
`& '.build/reads-preview/examples/reads_preview/reads_preview.exe'`.

## Build against an installed package

After installing the library to an absolute prefix, build this example as an
independent consumer:

```text
cmake -S examples/reads_preview -B .build/installed-reads-preview -G Ninja -DCMAKE_PREFIX_PATH=/absolute/install/prefix
cmake --build .build/installed-reads-preview
.build/installed-reads-preview/reads_preview
```

For a Windows prefix containing spaces, quote the complete argument:
`'-DCMAKE_PREFIX_PATH=C:/path with spaces/install'`. Run with
`& '.build/installed-reads-preview/reads_preview.exe'` in PowerShell.
The consumer requires no Python, sibling checkout, SDK or CANopen stack.
