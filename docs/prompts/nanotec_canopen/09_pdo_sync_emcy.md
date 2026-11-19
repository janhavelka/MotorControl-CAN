# 09 — PDO, SYNC, EMCY and boot-up

Execute this stage under the [execution contract](execution_contract.md).
Status: NOT STARTED. Runs after 03. NT-D02, D07, D09, D16 and D30 apply. Reuse `Pdo.h`, `PdoState.h`,
`Network.h` and `Sync.h` with a Nanotec `PdoProfile`; fork nothing. See
[sync.md](../../sync.md) and [prompt 51](../51_sync_producer.md).

## Work

1. `PdoProfile` for Nanotec: 8 RPDO and 8 TPDO sets (N5 p127; EDS `NrOfRXPDO=8`,
   `NrOfTXPDO=8` for every member), 8 mapping entries each (EDS [1600] SubNumber=9;
   N5 p209) although the text describes entries 01h-0Fh (N5 p128, p130; NT-D30), byte
   granularity (EDS Granularity=8) and at most 8 data bytes. Reviewed mappable
   objects come from the ledger's PDO columns, not from a CiA 402 list. Size owner
   routes (`MOTORCONTROLCAN_PDO_ROUTES`, per interface) for the routes an application
   binds, not for 16 sets per node.
2. Communication rules (N5 pp127-131): COB-IDs of PDO 1-4 are fixed to the CiA
   defaults and only bit 31 changes; PDO 5-8 COB-IDs are free, with defaults invalid
   (RPDO5 80000000h, TPDO5 C0000000h); a COB-ID change takes effect only after a
   restart or Reset Communication. Mapping and communication objects change only in
   Pre-operational or Stopped, otherwise abort 08000022h (N5 p127). RPDO transmission
   types 00h-F0h apply at the next SYNC, FEh/FFh at once; TPDO types 0, 1-240, FCh,
   FDh, FEh/FFh; inhibit in 100 µs units, preset 0064h = 10 ms; event timer in ms;
   SYNC start value active only when 1019h > 1 (N5 p130, pp210-211, p180; CP-D20).
   Dummy mapping 0002h-0007h is allowed (N5 p129).
3. Default maps are facts to verify by readback, never assumed: RPDO1 6040h + 6060h,
   RPDO2 607Ah + 6081h, RPDO3 6042h, RPDO4 60FEh:01; TPDO1 6041h + 6061h, TPDO2 6064h,
   TPDO3 6044h, TPDO4 60FDh (N5 p131). Note that RPDO1 maps the controlword: in NMT
   Operational any frame on 200h + node writes 6040h, which 04b treats as a control
   path.
4. Remapping uses the documented order (N5 pp131-132): valid bit 1, count 0, entries,
   count, valid bit 0, each step confirmed, in Pre-operational, with retained partial
   effects and no invented rollback. Saving is not part of remapping: the procedure
   says 1010h:03 while the category list puts PDO objects in communication 1010h:02
   (N5 pp111-112, p132; NT-D09); stage 10 keeps the PDO save gated until resolved.
   FIR-v2538 adds 2007h:02 "Automatic COB-ID", default 1, undocumented (NT-D02): that
   row stays refused.
5. PDO observations: decode TPDOs by the observed map and bind each sample to the map
   generation; a host generation is not a wire tag. After boot-up, remap or reset, a
   delayed same-ID same-length sample stays unqualified until an evidenced boundary.
6. SYNC: consumption only. 1005h preset 80h; bit 30 would make the drive a SYNC
   producer (N5 p165) and is never set. Seam S13 (claim if first, or extend): this
   family's reviewed SYNC effects are its entry, at least synchronous RPDO types
   00h-F0h applying their data at the next SYNC and synchronous TPDO transmission
   (N5 p130), read from the bound node's 1400h-1407h and 1800h-1807h; an unknown
   mapping or type refuses SYNC production on that interface. Prompt 51's producer
   stays the only producer. 1006h, 1007h (synchronous window, late PDOs
   rejected with EMCY, N5 p166) and 1019h are read and reported. The manual's SYNC
   figure shows CAN-ID 80h + node (N5 p119) against 1005h = 80h (NT-D07): use 80h.
7. EMCY (N5 pp120-121, pp161-165): 80h + node; error code, 1001h, manufacturer error
   number, padding; an error-cleared EMCY "00 00 E-REG E-Number" when no error
   remains. Decode with the N5 error-number table (watchdog reset 0, fieldbus 4,
   node guarding 6, following error 13, PDO timeout 17, PDO length 19 and 20, rated
   current must be set 22, unexpected sync length 27, slippage 30). 1014h bit 31 would
   disable EMCY and is never written. 1003h holds error number, class and code, up to
   eight entries although the text speaks of seven shifting (NT-D16); subindex reads
   abort 08000024h while empty (N5 p163).
8. Boot-up: the boot loader sends its own boot-up before the firmware's unless
   2007h:01 = 1 (N5 p132, p252), and 2084h delays boot-up (preset 0, N5 p275). Each
   boot-up invalidates dependent evidence, queued work and declarations (04b); the
   binding is not ready after the first one. 1F80h = 8 enters Operational
   automatically (default 0, N5 p250). NMT commands follow N5 p119; the "000 | 01 2A
   completely restarted" example mislabels Start Remote Node (NT-D07).

## Deliver

`profiles/nanotec_canopen/Pdo.h` content from the generator plus `PdoConfig.h/.cpp`
for typed map readback and explicit remapping; EMCY and boot-up decoding in the
profile's event module; `test/nanotec_canopen_pdo_test.cpp` and
`test/nanotec_canopen_events_test.cpp`; console routes `nanotec pdo`, `nanotec emcy`;
`nmt`, `heartbeat`, `network` and `sync` admitted for a Nanotec binding where this
evidence supports them, with unchanged meaning.

## Acceptance

Tests cover 8+8 sets, 8 entries, fixed versus free COB-IDs, the Operational abort,
partial remap, delayed samples across a remap, every transmission type, inhibit and
event-timer units, EMCY decoding including the cleared form and unknown numbers, the
empty-1003h abort, two boot-ups in sequence and invalidation on each. Quick and full
verification and CI on the exact final pushed commit pass. Hardware NOT RUN.
