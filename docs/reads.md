# Typed Lichuan raw reads

Include `MotorControlCAN/profiles/lichuan_c/Reads.h` and use namespace
`MotorControlCAN::LICHUAN_C`. This installed C++11 layer prepares finite reads
and consumes copied evidence. It owns no controller, clock, scheduler or SDK.
The [compiled offline example](../examples/reads_preview/README.md) exercises
identity and state reads with synthetic replies; it establishes no device capability.
The separate [state interpretation getter](drive_actions.md) uses checked STATUS
and MONITORS blocks to compare documentary layouts. Raw reads themselves still
do not select or qualify a firmware interpretation.

## Read effects and the probe gate

The retained sources establish raw widths and read directions for the rows
below. They do **not** establish that these uploads are non-consuming. The
ledger `read_effect` stays `not_established` and `effectPossible` stays true for
every row, including read-only objects.

On 2026-11-02 the repository owner accepted the risk (DEC-1) for these objects:
`1000`, `1018:00..03`, `1009`, `100A`, `2000`, `2001`, `6041`, `6061`, `6064`,
`606C`, `2100`, `2101`, `2102`. They carry `ACCEPTED_UNQUALIFIED`. That is a
project decision; it is not evidence about the device, and these reads are not
called non-changing or harmless. Packet 11 read all 16 rows live on two
specimens, with no read-effect observation, so success there settles nothing
about effects. `603F`, `1001` and `1003` stay `UNQUALIFIED`: explicit single
reads, never polled, reached
through `requestExperiment` or an action's prerequisites. The first bench read
of each records LED fault state before and after. Every other row stays gated by
Q20 (owner prompts 19, 29, 37, 41, 44, 47). Ordinary reads need an already
usable SDO channel and the owner's ordinary admission policy. They cannot
bootstrap service reconciliation.

`prepareProbe` checks the single `1000:00` U32 candidate. It succeeds with
`ACCEPTED_UNQUALIFIED`, produces one upload intent and has no NMT or settings
effect. A first accepted upload can confirm an unconfirmed channel; it cannot
establish freshness. Q20 in the [review decision](reference/lichuan_review.md)
is closed for the accepted list only as a project decision, not as a device
fact. An access flag or a successful upload cannot close it.

## Lifecycle

1. Supply a `ReadTarget` with the explicitly configured SDO channel, binding
   epochs and node generation, plus monotonic microsecond time, immutable
   absolute deadline and nonzero queue/response budgets. Time zero is valid.
2. Call `prepareIdentity`, `prepareConfig`, or `prepareState`. Preparation
   validates a complete candidate before publishing it. State selects explicit
   blocks; it does not silently sweep unrelated objects.
3. `nextRead` yields one owned upload intent. Pass it to the existing single
   SDO owner. Repeated inspection yields the same intent, not permission to
   admit or replay it twice. No download or NMT request is produced.
4. Before releasing the owner's retained terminal, pass its copied
   `SdoCompletion` to `advanceRead`. The function checks channel, target,
   operation/request/step, object, width, epochs, effect, deadline and timing.
   A valid terminal failure advances the context while the call returns `Ok()`;
   inspect `state`, `outcome`, `status` and retained native evidence separately.
5. Get values through `getIdentity`, `getConfig` or `getStateBlock`, supplying
   the **current** target and application maximum age. Wrong kind, incomplete
   block, changed epochs, stale/future evidence or invalidation leaves output
   unchanged. Identity/configuration require whole-read success. A completed
   state block remains available after a later ordinary block fails.

The caller serializes mutation and retires actual owner work before reporting
a local cancellation, deadline or invalidation to the read context. Cancellation
does not undo an upload's unknown effects. Native abort bits remain unsigned
32-bit evidence; transport success, raw observation and interpreted state are
different facts. Deadline precedence does not erase independent lifecycle
invalidation: a reboot, bus-off, binding invalidation or lost correlation revokes
getters, including for earlier completed blocks, even when the terminal outcome
is `DEADLINE`. Retained raw history remains inspectable.

An SDO `TIMEOUT` completing before the overall deadline gives `SERVICE_TIMEOUT`,
meaning a queue or response budget was spent. Earlier blocks stay available,
since a missing reply says nothing against them. `DEADLINE` is reserved for the
immutable overall cutoff. Neither outcome says whether a frame left the host:
read `evidence.transmissionPossible` for that.

## Implemented rows

All values below retain per-field provenance and have no generated write route.
Applicability remains `EDS_FAMILY_UNBOUND`; no returned value establishes an
installed supported model or firmware.

| Read / block | Objects | Representation and interpretation |
| --- | --- | --- |
| Identity (9) | `1000`, `1009`, `100A`, `1018:00..03`, `2000`, `2001` | U32 device type; `1009`/`100A` raw value with reply width (see below); U16 model/firmware; U8 identity count; U32 vendor/product/revision. Raw fields, `modelMatched=false`. |
| State: errors (2) | `1001`, `603F` | U8 error register and U16 raw error code. |
| State: history (5) | `1003:00..04` | U8 active count, at most four U32 entries. Read only active entries; reject count above four. Ordering/coherency/consumption remain unknown. |
| State: monitors (3) | `2100..2102` | U16 raw monitors, despite EDS RW/manual RO conflicts. |
| State: status (2) | `6041`, `6061` | U16 raw statusword and raw mode byte. Conflicting dialect/signedness is not interpreted. |
| State: feedback (2) | `6064`, `606C` | Exact I32 native position/velocity. Physical coordinate scale remains unknown. |
| State: inputs (1) | `60FD` | U32 raw inputs; no assumed equivalence to `2101` bit layout. |
| Configuration (6) | `1005`, `1014`, `1017`, `1200:00..02` | U32 SYNC/EMCY routes, U16 producer interval, U8 record count, U32 client/server SDO routes. Observations do not reconfigure routes. |

`ReadOptions::identityWidths` selects the `1009`/`100A` reply width. The EDS
declares U16; two specimens measured U8 (`1009=0x0C`, `100A=0x20`). The default
`DECLARED_U16` fails on those specimens as a wrong width. An application that has
established its own fixture selects `MEASURED_U8` explicitly; nothing detects it.
`1018:00=5` stays raw. No version text is inferred, and
`hardwareVersionWidth`/`softwareVersionWidth` report the actual reply bytes.

`ReadOptions::countWidths` does the same for the array counts `607D:00`
(`DRIVER_SOFT_LIMITS`) and `60FE:00` (`IO_OUTPUT_COMMAND`). The EDS declares U8;
on 2026-11-04 both specimens, of one firmware identity, answered each with two
bytes of 0. The default `DECLARED_U8` fails there as a wrong width, and the
client abort leaves the route unresolved. `MEASURED_U16` is selected explicitly,
only for those two groups; any other kind refuses it, and a one-byte reply then
fails. `softLimitEntriesWidth`/`outputEntriesWidth` report the reply bytes, and
the count stays raw: it is not evidence of an empty array and bounds nothing.
See [device conflicts](device_conflicts.md#conflict-and-observation-table).

There is no `1018:04` serial field in this EDS and no established actual
following-error read. Driver, I/O configuration and tuning reads retain owners
20–22. Prompt 20's nine native rows use the same engine through the
[typed driver group API](driver_settings.md). Prompt 21 adds 23 finite
[I/O read rows](io_settings.md), checked `getIo` and separate documentary DI
labels without granting reassignment or output actions. Other communication/PDO record reads have explicit owner 38 in the
[ledger](reference/lichuan_objects.md); their writable counterparts do not
remove the read obligation. PDO mutation remains with 18.

## Time and PDO evidence

An SDO field's observation interval is admission through matching completion;
it is not a measured wire sample timestamp. `ReadWindow` exposes earliest,
latest and skew across a block. Age is checked from each earliest possible
observation, not the last completed field. A multi-object read is not an atomic
snapshot. Heartbeat or unrelated successful traffic cannot refresh it.

For PDO state projection, pass `&record.map.route` from an actual `PdoRecord` to `prepareState`
and copy a qualified record from the same owner into `advanceRead`. The layer
rechecks the active map, communication settings, source, qualification boundary,
known capture time, decoded words and generations. `getStateBlock` also needs
the current qualified record. A template, changed host generation or matching
COB-ID/DLC alone cannot qualify delayed bytes. See the
[PDO provenance contract](canopen_services.md#one-owner-independent-sample-provenance).

The shared `canopen/PdoState.h` now contains these pure value types; the optional
adapter forwards to them rather than maintaining another node/map table.
Installed consumers can use the read API without linking the optional adapter,
CANopenNode, catalogue strings, Python, ledger or vendor documents.
