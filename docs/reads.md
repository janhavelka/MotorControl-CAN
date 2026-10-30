# Typed Lichuan raw reads

Include `MotorControlCAN/profiles/lichuan_c/Reads.h` and use namespace
`MotorControlCAN::LICHUAN_C`. This installed C++11 layer prepares finite reads
and consumes copied evidence. It owns no controller, clock, scheduler or SDK.
The [compiled offline example](../examples/reads_preview/README.md) exercises
identity reads with synthetic replies; it establishes no device capability.

## Read effects and the probe gate

The retained sources establish raw widths and read directions for the rows
below. They do **not** establish that these uploads are non-consuming. Their
generated read effect remains `UNQUALIFIED`, including read-only objects.
Ordinary reads therefore need an already usable SDO channel and the owner's
ordinary admission policy. They cannot bootstrap service reconciliation.

`prepareProbe` checks the single `1000:00` U32 candidate against that policy.
It currently returns `Err::UNSUPPORTED` with `ReadError::READ_EFFECT`, leaving
the context unchanged and yielding no work. Q20 in the
[review decision](reference/lichuan_review.md) needs applicable manufacturer
read-effect evidence or a separately authorized, identified firmware experiment.
The non-changing probe obligation in prompt 19 remains open. An access flag or
a successful upload cannot close it.

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
invalidation: a reboot, bus-off or binding invalidation revokes getters even
when the terminal outcome is `DEADLINE`. Retained raw history remains inspectable.

## Implemented rows

All values below retain per-field provenance and have no generated write route.
Applicability remains `EDS_FAMILY_UNBOUND`; no returned value establishes an
installed supported model or firmware.

| Read / block | Objects | Representation and interpretation |
| --- | --- | --- |
| Identity (9) | `1000`, `1009`, `100A`, `1018:00..03`, `2000`, `2001` | U32 device type; U16 hardware/software/model/firmware; U8 identity count; U32 vendor/product/revision. Raw fields, `modelMatched=false`. |
| State: errors (2) | `1001`, `603F` | U8 error register and U16 raw error code. |
| State: history (5) | `1003:00..04` | U8 active count, at most four U32 entries. Read only active entries; reject count above four. Ordering/coherency/consumption remain unknown. |
| State: monitors (3) | `2100..2102` | U16 raw monitors, despite EDS RW/manual RO conflicts. |
| State: status (2) | `6041`, `6061` | U16 raw statusword and raw mode byte. Conflicting dialect/signedness is not interpreted. |
| State: feedback (2) | `6064`, `606C` | Exact I32 native position/velocity. Physical coordinate scale remains unknown. |
| State: inputs (1) | `60FD` | U32 raw inputs; no assumed equivalence to `2101` bit layout. |
| Configuration (6) | `1005`, `1014`, `1017`, `1200:00..02` | U32 SYNC/EMCY routes, U16 producer interval, U8 record count, U32 client/server SDO routes. Observations do not reconfigure routes. |

There is no `1018:04` serial field in this EDS and no established actual
following-error read. Driver, I/O configuration and tuning reads retain owners
20–22. Other communication/PDO record reads have explicit owner 38 in the
[ledger](reference/lichuan_objects.md); their writable counterparts do not
remove the read obligation. PDO mutation remains with 18/38.

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
