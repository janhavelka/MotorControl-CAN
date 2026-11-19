# 09 — PDO, SYNC, EMCY and heartbeat observation

Execute this stage under the [execution contract](execution_contract.md).
Status: NOT STARTED. Runs after 03. DS-D06, DS-D07, DS-D19 and DS-D32 belong here. MULTIDRIVE rows 15, 16, 17, 18 and
51 apply, and backlog D11 (delayed same-ID samples) applies to every reused route. Observation only: no remap and no
RPDO.

## Work

1. Resources. Four RPDO and four TPDO records with mapping HighLimit 4 come from DS2EDS l.32 and l.554; confirm the
   IBLEDS and ISLEDS equivalents and give MINI none from an EDS. Supply them to `PdoProfile` per scope; never copy
   Lichuan C's counts or maps.
2. Defaults conflict (DS-D06). DS2C p25-26 maps only `1600:01` = 0x60400010 (count 1) and leaves every other record
   at count 0, with event timers 0 and PDO COB-IDs RO. DS2EDS maps RPDO1 `6040`/`60FF`/`6060` (l.554-583), RPDO2
   `607A`/`6081` (l.606-626), TPDO1 `6041`/`60FD`/`6061` (l.995-1024) and TPDO2 `6064`/`606C` (l.1047-1067), with event
   timers 1000 and 100 ms (l.359, l.800) and RW COB-IDs. Assume neither. `preparePdoRead` reads the live
   communication and mapping records `1400`-`1403`, `1600`-`1603`, `1800`-`1803`, `1A00`-`1A03` through
   `pdoMapFromReadback`; a TPDO is decoded only by a confirmed live map with a host generation.
3. Reused routes. A confirmed map and a new host generation do not qualify a delayed same-ID, same-length sample
   (backlog D11). Require an evidenced transition or retirement boundary, a distinct verified route or an observable
   marker; keep ambiguous samples unqualified without invalidating unrelated traffic.
4. No RPDO frames. An RPDO carrying `6040` or `60FF` is a motion command; RPDO control is a later explicit capability
   with its own evidence. Remapping (the CiA procedure, principles pack section 6) is gated: variable or dynamic
   mapping is not stated and the manual marks PDO COB-IDs RO. Typed PDO mutation is UNSUPPORTED here.
5. NMT. PDOs flow only in Operational under CiA 301; DS2 documents no boot, start-up state or NMT effect (DS-D32;
   `SimpleBootUpSlave=1`, DS2EDS). The NMT state comes from boot-up or heartbeat frames only. NMT start, stop, enter
   pre-operational and resets are explicit operations through the existing `makeNmtRequest` admission for a DS2
   binding, never implicit, and an NMT stop is not a quick stop. A boot-up invalidates dependent evidence and work.
6. Heartbeat. `1017` producer (U16 ms, RW/S, default 0; DS2C p24) is read here and written only by 10. There is no
   consumer heartbeat, guarding, `6007` or `1029`: host observation of a heartbeat establishes nothing about the
   drive's reaction to host loss, a host heartbeat (seam S11) has no documented DS2 consumer, and node guarding
   (seam S12) is undocumented, so DS2 needs neither seam.
7. SYNC. `1005` COB-ID 0x80 RW (DS2C p24, DS2EDS); no `1006`/`1007`; synchronous PDO behavior is undocumented.
   `prepareSync` and synchronous transmission types for a DS2 binding are refused before work (MULTIDRIVE row 51).
   DS2 does not claim seam S13: its SYNC effects are unknown, so once S13 lands a DS2 binding enters it as unknown
   and prompt 51's producer refuses SYNC on that interface. Refusing `prepareSync` for a DS2 target does not stop a
   SYNC requested for another family's node or already armed on the interface, and prompt 51's producer does not
   check DS2's effects. Until S13 lands, a DS2 node therefore shares no interface with nodes of another family
   (execution contract, 11); afterwards DS2 consumes S13's own producer tests and adds the DS2 cases (requests from
   both families, DS2 attached while a schedule is armed: no further SYNC TX), never a profile-local producer. S13
   also covers every NMT command to node 0 on a mixed interface, so this stage's explicit NMT operations broadcast there
   only under S13's admission.
8. EMCY (DS-D07, DS-D19). `1014` is 0x80 in the manual (DS2C p24) and `$NODEID+0x80` in DS2EDS l.213: read it live and
   route EMCY only on the confirmed identifier. Deliver each EMCY through `checkNetworkEvent` with raw code, error
   register and vendor bytes into the application's bounded event queue; an entry the queue cannot hold is counted
   and reported, never silently dropped and never a lost terminal
   ([communication practice](../../communication_practice.md) C11, C14). No table maps Er codes to EMCY or `603F`
   values, so no meaning is decoded. `1001` and `1003` stay explicit single reads (03). An EMCY is a notification,
   not the drive state.
9. SDO channel. `1200` lists one RO entry in the manual and two RW entries in DS2EDS (l.290/l.299; DS-D07). Use the
   default 0x600/0x580 plus node identifiers and never write `1200`.
10. Timing fields. Read `1800`-`1803:02/03/05` (transmission type, inhibit, event timer; RW/S in DS2C p24-26) as raw
   values. Accepted transmission types, the inhibit unit (100 us under CiA, CP-D01) and whether the event timer is
   honored are undocumented, and Lichuan C's measured "raw + 1 ms" periods do not transfer. Writing them belongs to a
   later qualified PDO-configuration capability, not to this stage.
11. Host liveness. The application may consume a DS2 heartbeat or TPDO stream for its own health view after 10
   configures `1017`. That is application health, distinct from drive state, NMT state and fault events, and it says
   nothing about the drive's behavior when the host disappears.

## Deliver

`Pdo.h` additions (generated per-scope counts plus a profile `PdoObservation`), the PDO read plan and route setup from
readback, EMCY routing from a live `1014`; `test/lichuan_ds2_pdo_test.cpp` and `test/lichuan_ds2_network_test.cpp`;
owner-backed `ds2 pdo read` and DS2 admission of `nmt`, `heartbeat` and `network` with unchanged meanings; refused
`sync` with reason; coverage, operation map, `docs/canopen_services.md` and `docs/sync.md` DS2 notes, Doxygen and
CHANGELOG.

## Acceptance

Tests cover both default-map claims as fake-server variants with decoding only from the live readback, a delayed
same-ID sample left unqualified, no RPDO frame ever emitted, refused remap and SYNC with no TX, EMCY routed on a
live 1014 value and an EMCY overflow counted, a boot-up invalidating dependent work, and NMT commands sent only on explicit request. Quick and full
verification and required CI on the exact final pushed commit pass. Hardware NOT TESTED. Commit, push and hand off.
