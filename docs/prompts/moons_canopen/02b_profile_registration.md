# 02b — Profile registration, probe and console family

Execute this stage under the [execution contract](execution_contract.md).
Status: NOT STARTED. Requires 02's ledger, generated tables and S7 checker partition.

Apply the [canopen_shared 02 contract](../canopen_shared/02_profile_integration_contract.md): its common and
CANopen-only rules and, at this stage, its registration acceptance. Recheck the
[seam table](../canopen_shared/README.md) and the Git log first. A seam another family has claimed or landed
is extended or waited for; an unclaimed one is claimed in the table (lead, stage 02b, date) before any shared
edit and landed in its own focused, tested commit with its commit ID, tested interface and consumer gate.
Commit tooling, core/profile and application/console changes separately.

## Work

1. Seam S1: append `Manufacturer::MOONS` and `DriveProfile::MOONS_CANOPEN` with the next unused explicit
   nonzero value, and a finite inventory entry in `src/Discovery.cpp`: `profileId` `moons_canopen`, protocol
   CANopen, node-ID bounds 0x01-0x7F with 0 reserved (UM21 p9), probe objects and model/firmware scopes from
   the ledger. `nonChanging`, `exactModel` and `firmware` stay false. The entry is a compiled candidate, never
   qualified support.
2. Seam S2: `Endpoint.profile` holds the `DriveProfile` value, resolved by the one pure lookup in
   `Discovery.h` (inventory value and matching protocol). If MOONS' lands S2, it also performs the Lichuan
   migration of that contract (old labels 41 and 1 refused, callers moved, CHANGELOG break entry). Every MOONS'
   preparation, and every Lichuan one, refuses a binding of another profile before touching output or yielding
   work. Test both directions, zero, unknown values, protocol mismatch and rebinding with a retained result.
3. Identity tests (canopen_shared 02 "Identity and device evidence"): with the correct label but a Lichuan
   server, a missing or stale identity (after boot-up or rebinding) and a colliding 0x1018 match, every
   dependent MOONS' operation refuses with nothing sent; selection and cached queries send nothing.
4. Probe: `prepareProbe`/`checkProbe` read 0x1000:00 and 0x1018:01 by expedited upload (UM21 p11, p41, p48).
   0x00060192 and 0x000002D9 publish only "responding CiA 402 server with the MOONS' vendor ID"; product,
   layout and firmware stay unknown (EDS product code and revision are 0 in all three files). Do not use
   0x1008 (24-character string; only expedited SDO is documented, MO-D27). UM21 states no read side effect for
   either object: record the read effect as an explicitly accepted review decision, not a device fact.
5. Seam S10: a MOONS' binding's effective `SdoReuse` default is `NONE` through the S2 lookup. Moving it to a
   marker policy needs the ledger review of the marker objects' read effects; 0x1018:02/03 are
   manufacturer-specific with unknown values, so a marker may compare only the value read on this binding.
   Test on a two-family owner: a MOONS' timeout and late reply send no marker and replay nothing, while the
   Lichuan route keeps its default marker.
6. Seam S3: a MOONS' binding has **no** best-effort stop shape until stage 04 reviews one; `admitBestEffortStop`
   returns `INVALID` with no TX and no fallback word. Neither candidate is safe yet: quick stop 0x000B reacts per
   0x605A, whose power-up value is undocumented and whose EDS default 0 (disable drive function) UM21 p58 does
   not support (MO-D05); halt 0x010F in profile position stops and then continues the set-point when released
   (UM21 p21), and in profile torque it frees the shaft (AMP10 p45). Lichuan's best-effort tests stay unchanged.
7. Bitrate admission: a MOONS' binding declares its bitrate. Admit only documented switch codes that the
   adapter's actual timing table supports: 1000, 800, 500, 250, 125, 50 and 20 kbit/s (UM21 p9; STFHW p13;
   SSDCHW p12). 12.5 kbit/s (all manuals) and 10 kbit/s (all EDS files) both reject until stage 11 resolves
   MO-D17. A declared rate that differs from the interface's active bitrate rejects before TX.
8. Seam S6: one `commands[]` entry in `examples/common/ProbeConsole.cpp` with parser and formatter in
   `examples/common/MoonsCanopenConsole.{h,cpp}`, and the JSONL `profile` rules by record (null on untargeted
   records, the target's `profileId` on targeted replies, the admitted value on operation records and retained
   results after a rebind). The console entry, `probe` route and parity tests are **Blocked until S4** (prompt
   64) lands; the `console_jsonl.py` schema and validator changes, inventory-derived `profile list`, `caps` and
   `help` proceed where they need no SDO read. `nmt`, `heartbeat`, `network` and `sync` refuse a MOONS' binding
   until stage 09 admits them.
9. Fake device `test/support/MoonsCanopenDrive.h` on the owner `Transport` callbacks: documented frames only,
   labelled synthetic; EDS defaults it returns are labelled as EDS values, not device values.
10. Multi-profile consumers: by default a Lichuan axis and a MOONS' axis run on separate interfaces with
    independent stops, caches and results; on one interface, reservations conflict on the same bus and node ID
    whatever the label, and an unanalysed second domain is refused (negative tests). The shared-interface
    positive test waits for stage 09's recorded analysis, with seam S13 as its SYNC part.
    `test/multidrive_contract_test.cpp` still passes.
11. Package: sources in `MOTORCONTROLCAN_SOURCES`, `install(FILES ...)` for the profile directory and
    `test/package_test.py` (`HEADERS`, exact archive set); `library.json` and `EXPORTS` unchanged. C++11
    consumers of core only, Lichuan only, MOONS' only and both; no profile-selection build option.

## Deliver

Probe declarations in the profile directory and their source; inventory, lookup and guard changes; the S10 and
S3 defaults; `MoonsCanopenConsole.{h,cpp}` (route Blocked until S4); `test/moons_canopen_discovery_test.cpp`,
`test/moons_canopen_owner_test.cpp`, `test/moons_canopen_console_test.cpp`; package and consumer entries;
operation-map rows; README scope, CHANGELOG `Unreleased`, `docs/api_cli_mapping.md`, `docs/cli_contract.md`,
`docs/source_map.md`, strict-gate Doxygen; seam claims and landings in the table; `docs/reports/moons_canopen_02b.md`.

## Acceptance

Registration acceptance of canopen_shared 02: the probe runs through the real `CanBusOwner`, `SdoClient` and
fake device at API level (and through the console once S4 lands) with identical TX frames and terminals;
foreign-profile and identity refusals send nothing; abort versus malformed reply, timeout and late identical
reply, stale generation, retained results, cancel that is not a stop, urgent stop under queue pressure, the
refused best-effort stop and no replay after a lost reply are tested; JSONL records follow S6; checker negative
tests fail as intended; all consumers build in C++11. `python scripts/verify.py --mode quick` and `--mode
full`, `python scripts/check_firmware.py --target all` with the pinned IDF, the family checks and CI on the
exact pushed commit pass. Hardware: NOT RUN.
