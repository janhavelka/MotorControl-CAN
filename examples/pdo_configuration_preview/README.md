# Offline TPDO1 event-timer change

This C++11 example runs the profile's supported TPDO1 transition, `EVENT_TIMER`,
through `prepareTpdo1Configuration`, `nextPdoConfiguration`,
`notePdoConfigurationTransmission`, `advancePdoConfiguration` and
`getTpdo1Configuration`. It changes `1800:05` on node 2 from raw 0 to raw 100.
The installed portable core is its only dependency.

The replies are the bench's own bytes, not invented ones. The nine baseline
uploads are node 2's TPDO1 record in
[1311Z](../../docs/bench/20261106T1311Z-pdo-node2-event-timer/rx.txt), lines
246-272. The write `2B 00 18 05 64 00 00 00` and its acknowledgement
`60 00 18 05 ...` are lines 26 and 28 of
[1316Z](../../docs/bench/20261106T1316Z-pdo-both-streams-soak/rx.txt). The nine
final uploads reuse the 1311Z replies, except `1800:05`, which answers raw 100 as
in that file's line 31. The installed `checkRequest` and `checkReply` check every
scripted frame against the yielded work. The route, reservation, review records,
heartbeat observation and every time value are fixtures. No controller, CANopen
stack, SDK, clock, heap or hardware is used.

The example prints each request and reply with its step number. Steps 1..9 are
the baseline and step 1 of the configuration is its one write. It first shows two
refusals before any write, each leaving the context byte-for-byte unchanged:

- target raw 0 against a baseline already at 0: `INVALID_CONFIG`, detail 6
  (`ReadError::VALUE`). No transition was measured, so nothing is written.
- target raw 200: `INVALID_CONFIG`, detail 2 (`ReadError::ARGUMENT`). It was
  measured on node 2 but is not supported; only `RAW_0` and `RAW_100` exist.

It then prints the published raw literal 100, the unchanged map (`182`, type 255,
count 3, 7 bytes), the configuration epoch advanced at the write's possible
transmission, and confirmed step 10, first-effect step 1 and no failed step. It
returns zero only when all of that holds.

`1800:05` is a raw U16 value whose unit is not established. On the two CL86-C
bench specimens raw 100 gave periods of about 101 ms on the S2's clock while
Operational, and raw 0 sent no periodic frame while stationary; neither fact is
claimed here. The transition is CL86-C only (an OL86-C reservation is refused)
and is host-tested, awaiting its first bench replay. A confirmed map qualifies no PDO sample. A real application
obtains each terminal from its sole service owner, withdraws dependent maps when
`notePdoConfigurationTransmission` succeeds, and never retries or rolls back. The
[service guide](../../docs/canopen_services.md#lichuan-tpdo1-configuration-subset)
gives the contract, and the S2 bench runs the same lifecycle as its `E` line.

## Build from the source tree

```text
cmake -S . -B .build/pdo-configuration-preview -G Ninja -DBUILD_TESTING=OFF -DMOTORCONTROLCAN_BUILD_EXAMPLES=ON
cmake --build .build/pdo-configuration-preview --target pdo_configuration_preview
```

On Linux run
`.build/pdo-configuration-preview/examples/pdo_configuration_preview/pdo_configuration_preview`.
In PowerShell run
`& '.build/pdo-configuration-preview/examples/pdo_configuration_preview/pdo_configuration_preview.exe'`.

## Build against an installed package

After installing to an absolute prefix:

```text
cmake -S examples/pdo_configuration_preview -B .build/installed-pdo-configuration-preview -G Ninja -DCMAKE_PREFIX_PATH=/absolute/install/prefix
cmake --build .build/installed-pdo-configuration-preview
.build/installed-pdo-configuration-preview/pdo_configuration_preview
```

Quote the complete prefix argument when a Windows path contains spaces. Run
`& '.build/installed-pdo-configuration-preview/pdo_configuration_preview.exe'` in
PowerShell. No Python, sibling checkout or optional stack is needed.
