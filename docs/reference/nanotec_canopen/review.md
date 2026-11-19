# Nanotec CANopen family (FIR-v2213 generation): evidence review

Reviewed 2026-11-19. Primary target: N5-1-2 / N5-2-2. Applicability rows: C5-E-1-09 / C5-E-2-09, CL4-E-1-12 / -2-12 (and -5VDI), PD4-C ...-E-08. PD6-E (FIR-v2538) is recorded only as a later row.

Everything below is document scope: vendor manuals, product pages and the vendor EDS/OD files. Nothing here is a hardware observation, and no claim here means the library implements or has qualified any of it. Page numbers are one-based PDF pages; in the N5, C5-E, CL4-E and PD4-C manuals the PDF page equals the printed page. Where text extraction was not reliable for a table, the rendered page was checked (N5 pp37-38, 57, 118; PD4C p166).

## Source identifiers used in this review

| ID | Document | Local file |
|---|---|---|
| N5 | N5 CANopen Technical Manual 3.3.0, FIR-v2213 | [../multidrive/vendor/nanotec_n5_canopen_v3.3.0.pdf](../multidrive/vendor/nanotec_n5_canopen_v3.3.0.pdf) |
| C5E | C5-E CANopen/USB Technical Manual 3.3.0, FIR-v2213 | [../multidrive/vendor/nanotec_c5e_canopen_v3.3.0.pdf](../multidrive/vendor/nanotec_c5e_canopen_v3.3.0.pdf) |
| CL4E | CL4-E CANopen/USB/Modbus RTU Technical Manual 1.5.0, FIR-v2213 | [vendor/nanotec_cl4e_canopen_usb_modbusrtu_v1.5.0.pdf](vendor/nanotec_cl4e_canopen_usb_modbusrtu_v1.5.0.pdf) |
| PD4C | PD4-C CANopen Technical Manual 3.3.0, FIR-v2213 | [vendor/nanotec_pd4c_canopen_v3.3.0.pdf](vendor/nanotec_pd4c_canopen_v3.3.0.pdf) |
| N5V2 | N5 CANopen Technical Manual 2.0.3, FIR-v1650 ("Stock Firmware") | [vendor/nanotec_n5_canopen_v2.0.3_stock_fir-v1650.pdf](vendor/nanotec_n5_canopen_v2.0.3_stock_fir-v1650.pdf) |
| FWUPD | Instructions for Firmware Update to FIR-v2039, 1.2.0 | [vendor/nanotec_firmware_update_fir-v2039_v1.2.0.pdf](vendor/nanotec_firmware_update_fir-v2039_v1.2.0.pdf) |
| PDI | Functional Description Plug & Drive Interface 1.0.1 | [vendor/nanotec_plug_drive_interface_v1.0.1.pdf](vendor/nanotec_plug_drive_interface_v1.0.1.pdf) |
| PNDS3 | Plug & Drive Studio 3 User Manual 1.4.0 | [vendor/nanotec_pnds3_user_manual_v1.4.0.pdf](vendor/nanotec_pnds3_user_manual_v1.4.0.pdf) |
| EDS model/fw | `PLC_<model>_<build>_EDS.eds` from the vendor "OD and Firmware Package" | [vendor/od_firmware_package](vendor/od_firmware_package) |
| OD model/fw | `OD_<model>_<build>.xml` (Plug & Drive Studio object dictionary) from the same package | [vendor/od_firmware_package](vendor/od_firmware_package) |
| IDX | Derived index of every family member of the package | [extracted/od_firmware_package_index.json](extracted/od_firmware_package_index.json) |
| N5P12, N5P22, C5EP209, CL4EP212, PD4CPM08, PD4CPB08, PD4CPM01, PNDS3P | Product page snapshots, 2026-11-19 | [vendor](vendor) |
| N5OM, CL4EOM, PD4COM, PD6EOM | Online manual front pages, 2026-11-19 | [vendor](vendor) |

"fw 2213" means build FIR-v2213-B1031134; "fw 2538" means build FIR-v2538-B1077563. EDS citations name the section, for example `EDS N5-1-2/2213 [DeviceInfo]`.

## 1. Scope and exact applicability

**Models and suffixes.**

- N5: N5-1-2 (low current) and N5-2-2 (high current), CANopen variants, hardware W007 and later (N5 p1). N5 p18 splits the N5-2-2 voltage range by hardware: 12-48 V up to W007, 12-57.4 V from W007b. N5-1-1/N5-2-1 are the EtherCAT variants (FWUPD p5). The -3, -4 and -5 variants, listed in FWUPD p3 and the package, were not reviewed. All of these are out of scope.
- C5-E: C5-E-1-09 and C5-E-2-09, CANopen/USB, hardware W006 and later (C5E p1). The package also contains C5-E-x-09A variants (IDX) that no reviewed manual names. Plain C5-01 is not a CANopen product.
- CL4-E: CL4-E-1-12, CL4-E-2-12, CL4-E-1-12-5VDI and CL4-E-2-12-5VDI, CANopen/USB/Modbus RTU, hardware W002 and later (CL4E p1). The -5VDI suffix changes only the digital input level, 5 V instead of 24 V (CL4E p16).
- PD4-C: PD4-C5918X4204-E-08, PD4-C5918M4204-E-08, PD4-C5918L4204-E-08 and PD4-C6018L4204-E-08 are integrated steppers; PD4-CB59M024035-E-08 is an integrated BLDC. All need hardware W006 or later (PD4C p1, p17). The -E-08 suffix is CANopen. The -E-01 suffix is a USB product with its own manual (PD4CPM01), so it is not CANopen evidence. PD4-C5918L2104-E-08 appears in the PD4-C object pages and the package (PD4C p146, p166; IDX) but not on the manual cover (NT-D14).
- PD6-E (later row): the PD6-E online manual 1.2.0 covers PD6-E891S95/M95/L95-E(S)-65-2 (stepper) and PD6-EB80SD/MD/LD/CD-E(S)-65-2 (BLDC), valid for FIR-v2538 and higher, with STO in the -ES- variants (PD6EOM). It was not reviewed further.

**Firmware generations.** The FIR-v2213 manuals say "valid with firmware version FIR-v2213 [and higher]" (N5 p1; N5OM). The product pages also link a second manual labelled "Stock Firmware". That manual is N5 2.0.3 for the N5, C5-E 2.0.1 for the C5-E and PD4-C 2.0.2 for the PD4-C, and all three cover FIR-v1650 (N5P12, N5P22, C5EP209, PD4CPM08, PD4CPB08; N5V2 p1; remote-only C5EV2 and PD4CV2). Per the pages, "Under Downloads you will find the technical manual that corresponds with the stock firmware" (N5P12). FWUPD p3 (10/2020) says these products "are currently delivered with firmware FIR-1650". The CL4-E page lists only the FIR-v2213 manual (CL4EP212), and CL4-E history starts at FIR-v1825 (CL4EOM).

The vendor's current firmware package contains FIR-v2538 images and EDS files for N5-x-2, C5-E-x-09, CL4-E-x-12 and PD4-C ...-E-08 (IDX; `Firmware_N5-x-2_2538-B1077563-343ccc7f.fw` and others). No manual for these four families covers FIR-v2538 (NT-D02). So a delivered unit may run FIR-v1650 (stock), FIR-v2213 (documented) or FIR-v2538 (packaged). A profile must identify the firmware first (section 4).

**Motor and feedback configurations.** In N5, C5-E and CL4-E, the motor type, open or closed loop, and feedback are configuration rather than variant: open-loop stepper, closed-loop stepper with encoder, BLDC closed loop with Hall sensors, and BLDC closed loop with encoder (N5 p18; C5E p18; CL4E p16). Device type 1000h is 00060192h, meaning stepper and BLDC (N5 p159; EDS N5-1-2/2213 [1000]). PD4-C variants are fixed: 1000h is 00040192h for the steppers and 00020192h for the PD4-CB (PD4C p146; EDS PD4-C5918M4204-E-08/2213 and PD4-CB59M024035-E-08/2213 [1000]). Each has an integrated single-turn absolute encoder of 1024 cpr (PD4C p18), and auto setup "was already performed at the factory" (PD4C p42, p85). The EDS default of 3202h is 1 (closed loop) on the PD4-C steppers and 0x41 (closed loop plus BLDC) on the PD4-CB, versus 0 (open-loop stepper) on N5, C5-E and CL4-E (IDX).

## 2. Sources and retrieval failures

Stored originals are listed in [SOURCES.json](SOURCES.json). The N5 and C5-E manuals and the 2026-10-23 N5 product page are referenced in place from the multidrive pack with their recorded hashes; they are not copied.

- **EDS found.** Nanotec publishes no EDS on its product pages. The Plug & Drive Studio 3 page offers an "OD and Firmware Package" (PNDS3P), and PNDS3 p24 calls it the source of "firmware-correct OD files for all Nanotec controllers". The zip was downloaded (117,940,798 bytes, SHA-256 c9d57c10...f9b7, Last-Modified 2026-06-01). It contains `PLCConfigs/PLC_<model>_<build>_EDS.eds` files, which are EDS 4.0, and `OD_<model>_<build>.xml` files. Stored unchanged: the all-objects EDS for N5-1-2, N5-2-2, C5-E-1-09, C5-E-2-09, CL4-E-1-12, CL4-E-2-12, PD4-C5918M4204-E-08 and PD4-CB59M024035-E-08 for both fw 2213 and fw 2538; the reduced fw 2213 EDS for the two N5 models; and the four N5 OD XML files. The other family members (CL4-E -5VDI, the other PD4-C steppers and PD6-E) are indexed with SHA-256 in IDX but not stored. The zip itself is not stored, because of its size and the firmware binaries it holds. Its `_EDS.eds` and `_all_objects.eds` members are byte-identical, and the shorter `<model>.eds` (fw 2213 only) omits the vendor objects marked `private` in the OD XML (NT-D27).
- **Newest N5 manual.** The online manual and both N5 product pages still give 3.3.0 (07/2022) as current (N5OM; N5P12). Guessed paths for a FIR-v2538 N5 manual returned HTTP 301 to site pages, not files (remote-only N5NEWER).
- **Firmware release notes: none found.** The only change records are the manual version tables (N5 pp10-11; CL4EOM; PD4COM) and FWUPD (FIR-v1650 to FIR-v2039).
- **Downloaded, cover checked, not stored (size budget):** the C5-E 2.0.1 and PD4-C 2.0.2 stock-firmware manuals.
- **Not downloaded:** the PD6-E 1.2.0 manual (later row; HTTP HEAD 200, 11,140,988 bytes); the Plug & Drive Studio installer (an installer); and the NanoCAN EDS zip, which covers only older PD...-O41 and SMCI products.
- **Not run:** no installer, firmware or tool was executed, and the firmware members of the zip were listed only.

## 3. Interface and protocol

Each item is N5 behaviour unless a family is named. C5-E has the same CANopen chapter text; a line comparison found no differences in C5E pp115-144. CL4-E and PD4-C contain the same CANopen sections at shifted pages (CL4E from p113, PD4C from p104), checked by section and keyword; no complete word comparison was made for them. Family differences are stated where found.

- **Frame format.** Only 11-bit CAN-IDs are supported, and data is little-endian (N5 p117; C5E p115; CL4E p113; PD4C p104). The references are CiA 301 4.2.0, CiA 402 3.0 (2007), CiA 306 1.3.5 and CiA 305 3.0.0 (N5 p117).
- **Node ID and bit rate.**
  - N5: two hex rotary switches, S1 the high nibble and S2 the low (N5 p30, p37). Switch value 0 gives node 2009h at fixed 1 Mbit/s. Values 1-127 give that node ID at fixed 1 Mbit/s. Value 128 gives 2009h and 2005h. Values 129-255 give (value - 128) as node ID with 2005h as bit rate (N5 p37). Value 1 always forces node 1 at 1000 kbit/s as a recovery position (N5 p37). The load order is default, stored value, configuration file, then rotary switch, each overriding the previous (N5 p37). Changes to 2005h/2009h must be saved with 1010h:0Ah = 65766173h and take effect after a power cycle or NMT Reset Communication (N5 p37).
  - Bit-rate codes: the 2005h object page lists 81h-86h as 10, 20, 50, 125, 250 and 500 kbit/s and 88h as 1000; the preset is 88h, and any other value means 1000 kbit/s (N5 pp250-251; EDS default 136). The commissioning table on p38 disagrees (NT-D04). The EDS has BaudRate_800=0 (EDS N5-1-2/2213 [DeviceInfo]).
  - C5-E: same scheme with switches S2/S3 (C5E p34-35).
  - PD4-C: one 16-position switch. 0 gives 2009h at 1 Mbit/s, 1-7 give node 1-7 at 1 Mbit/s, 8 gives 2009h and 2005h, and 9-F give (n - 8) with 2005h. There is an optional node-ID offset in 2006h (PD4C p28-29, p238).
  - CL4-E: no rotary switch. Node 127 and 1 Mbit/s are set by objects 2009h/2005h, and a "default switch" pad resets the communication settings (CL4E p28, p34-35).
  - Default node ID: 2009h has preset 7Fh in all four (N5 p252; EDS [2009]), but N5, C5-E and PD4-C text states a default of node 1 at 1 Mbit/s (NT-D06).
- **NMT.** Commands 01h (start), 02h (stop), 80h (pre-operational), 81h (reset node) and 82h (reset communication); node ID 0 broadcasts (N5 p119). Boot goes Initialising, then Pre-operational automatically. Setting 1F80h = 8 enters Operational automatically, default 0 (N5 p118, p250). Service activity (N5 p118, rendered): PDO only in Operational; SDO, SYNC and EMCY in Pre-operational and Operational; NMT in all states. Heartbeat in Stopped is not stated (NT-D17).
- **Boot-up.** 700h + node ID with data 00h when Pre-operational is reached. The **boot loader sends its own boot-up message** unless 2007h:01 = 1 (N5 p132, p252), so two boot-ups per reset are possible.
- **Heartbeat and node guarding.**
  - The producer heartbeat is 1017h in ms, default 0 (off) (N5 p133, p178).
  - The consumer is 1016h, with **one entry only**: time in bits 0-15 and producer node in bits 16-23, default 0 (N5 pp176-177). Errors clear when the heartbeat returns or monitoring is disabled (N5 p134).
  - Node guarding uses 100Ch/100Dh (N5 pp132-133, p168). Combining guarding with heartbeat is "not permissible" (N5 p132); if both are set, heartbeat wins and the guard timer is suppressed without EMCY (N5 p168).
  - A heartbeat or life-guard error is EMCY 8130h (N5 p164). Reactions are 1029h:01 for the NMT state (00 = go to Pre-operational, the default; 01 = no change; 02 = Stopped) and 6007h for the drive (section 8).
  - FIR-v1650 documents no 1006h, 1016h, 1019h, 1F80h or 6007h; the N5V2 object index (N5V2 pp5-8) has no entries for them. FWUPD p11 lists 1006h, 1016h, 1019h, 1029h and 1F80h as new in FIR-v2039, although N5V2 does document 1029h (N5V2 p151) (NT-D31).
- **SDO.**
  - Server at 600h/580h + node ID. Expedited transfer is described, plus segmented ("normal") upload with an example; block transfer is named but not described (N5 pp121-126). The abort code list is on N5 pp126-127.
  - 08000022h means PDO communication or mapping objects were written in Operational; those objects may only be changed in Pre-operational or Stopped (N5 p127).
  - Strings can be read character by character from subindex 1 upwards if the master lacks segmented transfer (N5 p123).
  - 1003h subindices cannot be read while the error stack is empty; the abort is 08000024h (N5 p163).
- **PDO resources.**
  - **8 RPDO and 8 TPDO**, each with at most 8 data bytes (N5 p127; EDS [DeviceInfo] NrOfRXPDO=8, NrOfTXPDO=8 for every family member, IDX). Each mapping object has 8 entries (EDS [1600] SubNumber=9; object pages such as N5 p209), although the text describes entries 01h-0Fh (N5 p128, p130) (NT-D30). The EDS has Granularity=8, so mapping is byte-granular.
  - COB-IDs of PDO 1-4 are fixed to the CiA defaults; only the valid bit (bit 31) can change. PDO 5-8 COB-IDs are free (N5 p127, p129); the EDS default for RPDO5 is 80000000h, invalid (EDS [1404sub1]), and for TPDO5 C0000000h (EDS [1804sub1]). A COB-ID change takes effect only after a restart or Reset Communication (N5 p127).
  - RPDO transmission types: 00h-F0h apply at the next SYNC, FEh/FFh apply immediately (N5 p128).
  - TPDO transmission types: 0 is acyclic synchronous; 1-240 is every nth SYNC; FCh is RTR synchronous; FDh is RTR event; FEh/FFh is event (N5 p130). Subindex 3 is the inhibit time in 100 µs, async only, default 0064h = 10 ms. Subindex 5 is the event timer in ms. Subindex 6 is the SYNC start value, active only when 1019h > 1 (N5 pp130, 210-211). Dummy mapping 0002h-0007h is allowed (N5 p129).
  - Default mapping (N5 p131; EDS [1600]-[1A03]): RPDO1 6040h + 6060h; RPDO2 607Ah + 6081h; RPDO3 6042h; RPDO4 60FEh:01. TPDO1 6041h + 6061h; TPDO2 6064h; TPDO3 6044h; TPDO4 60FDh. Identical in C5-E, CL4-E and PD4-C (IDX EDS defaults).
  - Remapping order: valid bit = 1, mapping count = 0, write entries, set count, valid bit = 0, then save (N5 pp131-132). The saving subindex given there conflicts with the save categories (NT-D09).
- **SYNC.** 1005h preset 80h. Bit 30 makes the drive a SYNC producer with period 1006h, in multiples of 1000 µs (N5 p165). 1007h is the synchronous window in µs; late synchronous PDOs are rejected with EMCY (N5 p166). The sync counter overflow is 1019h, 02h-F0h (N5 p180). The manual's SYNC frame figure contains an error (NT-D07).
- **EMCY.** At 80h + node ID, the frame is error code, 1001h, manufacturer error number and zero padding. An error-cleared EMCY "00 00 E-REG E-Number" is sent when no errors remain (N5 pp120-121). 1014h bit 31 disables EMCY, and bits 0-30 are regenerated from the node ID at each restart (N5 p176). The 1003h layout is error number (8), error class (8) and error code (16), with up to 8 entries (N5 pp161-165).
- **LSS (CiA 305).** Switch-state global and selective, configure node ID, configure bit timing (table index 0, 2, 3, 4, 6, 7, 8, so no 800 or 100 kbit/s), activate bit timing, store, inquire identity and node ID, and identify, on 7E5h/7E4h (N5 pp134-146). LSS takes effect only at the rotary switch positions that read the objects (C5E p35; PD4C p29). The EDS says LSS_Supported=0 (NT-D05).
- **Other buses and paths into the same OD.**
  - N5: Ethernet REST, GET/POST /od/xxxx/yy (N5 pp35-36).
  - C5-E and CL4-E: USB mass-storage configuration file cfg.txt, executed top to bottom at every boot and overriding saved values (C5E pp29-33, p108; CL4E pp30-34, p106).
  - CL4-E: CANopen and RS-485 Modbus RTU share connector X1 (CL4E p19). Object 2102h (preset 0019000Bh) enables fieldbuses (CL4E p294); behaviour with both buses active is not documented (NT-D28).

## 4. Identity

| Object | N5 (N5 pp159-179) | Other families (FIR-v2213 manuals and EDS) |
|---|---|---|
| 1000h | 00060192h; bits 16-23 motor type 2 = BLDC, 4 = stepper, 6 = both | C5-E, CL4-E 00060192h; PD4-C stepper 00040192h, PD4-CB 00020192h (PD4C p146) |
| 1008h | "N5-1-2" or "N5-2-2" | Model name (EDS [1008]) |
| 1009h | preset "0" (N5 p167) | Same; hardware revision is not exposed as a value here |
| 100Ah | "FIR-v2213-B1029645" in the manual (N5 p167); package EDS "FIR-v2213-B1031134" or "FIR-v2538-B1077563" | Same build strings (IDX); NT-D03 |
| 1018h:01 | 0000026Ch | 0000026Ch |
| 1018h:02 | N5-1-2 08h, N5-2-2 0Ah (N5 p179) | C5-E-1-09 10h, C5-E-2-09 11h (C5E p177); CL4-E-1-12 5Ah, -2-12 5Bh, -1-12-5VDI 5Eh, -2-12-5VDI 5Fh (CL4E p202); PD4-C see NT-D14 |
| 1018h:03 | 08A50000h (N5 p179) | All families: 08A50000h for fw 2213, 09EA0000h for fw 2538, 090B0000h for the CL4-E FIR-v2315 build (IDX); FIR-v1650 06720000h (N5V2 p149) |
| 1018h:04 | serial number | 4040h holds the serial as a string (N5 p343) |

**Observed encoding.** The high 16 bits of 1018h:03 equal the decimal firmware number: 08A5h = 2213, 09EAh = 2538, 090Bh = 2315, 0672h = 1650. This is an inference from four documented values; the manuals do not state it. The low 16 bits are 0 in every documented case, so the build number (Bnnnnnnn) is visible only in 100Ah.

**Minimum identity check before any write.** 1018h:01 = 26Ch, 1018h:02 in the profile's table for the selected model, 1018h:03 and 100Ah matching a firmware generation with reviewed evidence, and 1000h matching the expected motor class.

## 5. Drive state machine against CiA 402

The section is the same in N5, C5-E, CL4-E and PD4-C (N5 pp56-61; C5E pp54-59; CL4E pp54-59; PD4C pp45-50). A word comparison found C5-E identical. For CL4-E and PD4-C, every difference inspected was figure-label order in the extracted text.

- **Controlword (N5 p57, rendered; p347).** The values below are bits 7/3/2/1/0, with X meaning either. Shutdown 0/X/1/1/0 (transitions 2, 6, 8); switch on 0/0/1/1/1 (3); disable voltage 0/X/X/0/X (7, 9, 10, 12); quick stop 0/X/0/1/X (11); disable operation 0/0/1/1/1 (5); enable operation 0/1/1/1/1 (4). Transition 16, quick stop active to operation enabled, needs a **rising edge of bit 2** with bits 3, 1 and 0 set. Fault reset (15) needs a **rising edge of bit 7**. Bit 8 is halt in PP, VL, PV, PT and IP. Bits 4-6 and 9 are mode specific.
- **Statusword (N5 pp58, 348-349).** The usual CiA 402 masks. Bit 4 VE, bit 7 warning, bit 8 SYNC (in sync with the bus), **bit 9 "Remote" always 1**, bit 10 target reached, bit 11 internal limit active, bits 12-13 mode specific, and **bit 15 closed-loop active** (only in Operation enabled; N5 p53, p348). After the self-test the drive reaches Switch on disabled; an unrecoverable error leaves it in Not ready to switch on (N5 p58). FWUPD p13 records that bit 15 meant "closed loop available" in FIR-v1650.
- **Stop reactions (N5 pp59-61; presets N5 pp356-358 and EDS).**

  | Object | Allowed values | Preset |
  |---|---|---|
  | 605Ah quick stop | 0, 1, 2, 5, 6 | 2: quick-stop ramp 6085h, then Switch on disabled |
  | 605Bh shutdown | 0, 1 | 1 |
  | 605Ch disable operation | 0, 1 | 1 |
  | 605Dh halt | 1 (slow-down ramp), 2 (quick-stop ramp) | 1 |
  | 605Eh fault | 0, 1, 2 | 2 |
  | 3700h following/slippage error | -1, 0, 1, 2 | FFFFh = -1, **no reaction** |

  605Ah values 5 and 6 stay in Quick stop active with the motor energised. 6085h is the quick-stop deceleration, preset 5000 in user units (N5 p377).
- **Fault reset.** A bit 7 edge clears the fault "if possible", and 603Fh is reset to 0 (N5 p346, p347). Overtemperature faults need cooling plus a fault reset (N5 p19). The manuals do not state that a fault reset clears latched limit-switch positions; those need 3701h = -2 (N5 p66).
- **Mode changes.** "The operating mode can be set or changed at any time" (N5 p58). FWUPD p6: from FIR-v2039, changing mode in Operation enabled is allowed, and "the command starts immediately upon changing from Switched on to Operation enabled or upon changing the mode to the Operation enabled state if bit 4 was already set". N5 p68 says the same for entering Profile Position with bit 4 set. In FIR-v1650 the mode could not be changed in Operation enabled (FWUPD p6). See NT-D13.
- **Enable side effects.**
  - With 3202h bit 4 (auto alignment) set, the first enable after each restart moves the rotor to a magnetic pole before Operation enabled is reached, and needs the brake control off (N5 p53, p298).
  - With an encoder used for commutation and no auto alignment, the drive stays in open loop until the index is passed, at most one motor revolution (N5 p53, p298).
  - 3202h bit 0 cannot change in Operation enabled (N5 p297).
  - Unit and scaling objects written during Operation enabled are applied only after leaving that state (N5 p62). 3203h behaves the same way (N5 p298).
- **Brake (N5 pp107-108; C5E pp105-106).** 3202h bit 2 enables automatic brake control.
  - On enable: current on, wait 2038h:03, release brake, wait 2038h:04, then Operation enabled.
  - On leaving: stop, wait 2038h:01, close brake, wait 2038h:02, then current off.
  - Manual mode drives the brake with 60FEh:01 bit 0, where 1 means brake closed; the preset 60FEh:01 = 1 (N5 pp411-412).
  - The output is PWM: 2038h:05 is 50-2000 Hz and 2038h:06 is a 2-100 % duty cycle. Brake+ is tied to the controller supply (N5 p108).
  - CL4-E and PD4-C have no brake connector or automatic-brake chapter. The brake signal can only be routed to a digital output with 3252h (CL4E pp103-104; PD4C pp94-95), and their EDS preset of 60FEh:01 is 0 (IDX).

## 6. Modes and their objects

6502h = 000003EFh: PP, VL, PV, TQ, HM, IP, CSP, CSV and CST; bit 4 reserved (N5 p413; EDS [6502] for every family member, IDX). The vendor modes -1 (clock-direction) and -2 (auto setup) exist in 6060h but are not flagged in 6502h (N5 p359). 6060h preset is 0, and 6060h is saveable (N5 p359). The combinations of mode and control mode are tabulated on N5 p46: PT and CST need closed loop; homing on block needs closed loop; IP, CSP and CSV are "not advisable" in open loop.

- **PP, 6060h = 1 (N5 pp68-75).**
  - Bit 4 rising edge starts a move, and bit 4 is also accepted if it was already set when entering PP. Bit 5 = 1 applies the new set-point immediately; bit 6 = 1 makes the target relative, with the reference from 60F2h bits 0-1. Bit 9 is change on set-point.
  - Statusword: bit 12 acknowledges the set-point; bit 10 means within 6067h for 6068h, or halted and at standstill; bit 11 means outside 607Dh; bit 13 is following error (closed loop only).
  - One set-point can be buffered; a further one is ignored while the buffer is full (N5 pp69-70).
  - 60F2h preset 1: relative moves are relative to the ramp generator output (N5 p407). 60F2h bits 4-5 make the drive clear 6040h bit 4 itself, so "the controller [modifies] object controlword 6040h" (N5 p407). Bits 6-7 are rotary-axis direction options (N5 p408).
  - Chained relative moves with a non-zero end velocity can overshoot by one 1 ms sample (N5 p73).
  - Objects: 607Ah, 607Bh, 607Dh, 607Ch, 607Eh, 6081h, 6082h, 6083h, 6084h, 6085h, 6086h (0 trapezoid, 3 jerk-limited with 60A4h:01-04), 60C5h/60C6h, and the lower of 607Fh and 6080h (N5 p74).
- **VL, 6060h = 2 (N5 pp76-77).** Target 6042h is I16 (N5 p349), with ramps 6048h/6049h as delta speed / delta s, limits 6046h and quick stop 604Ah. Units are rpm unless 604Ch changes them (N5 p65, p76). Bit 11 means limit active.
- **PV, 6060h = 3 (N5 pp77-80).** Target 60FFh is I32. Bit 10 with halt means target reached (606Dh/606Eh) or standstill. Bit 12 means speed is zero (606Fh/6070h). Bit 13 is slippage error (60F8h, preset 400, over 203Fh). Ramps are 6083h/6084h, jerk is 60A4h, and 607Eh bit 6 inverts the sign.
- **PT, 6060h = 4, closed loop only (N5 pp80-82).** 6071h target, 6072h max torque, 6073h max current, 6074h demand and 6087h slope, all in **thousandths of rated torque, where rated torque corresponds to the rated current 203Bh:01** (N5 p81, p367). The effective limit is the lower of 6072h and 6073h, and everything is capped by 2031h (N5 p81). With 3202h bit 5 = 0 this is torque-limited velocity mode (6080h limits speed, field weakening possible); with bit 5 = 1 it is "real" torque with no speed limit (N5 p81, p298). Target reached means 6077h within 203Dh for 203Eh (N5 p80). Preset 6072h = 0064h, which is 10 % (NT-D20).
- **HM, 6060h = 6 (N5 pp82-89).**
  - Bit 4 = 1 runs homing until done or until bit 4 is cleared.
  - Status bits 13, 12 and 10: 000 running; 001 interrupted or not started; 010 homed since the last restart but target not reached; 011 complete; 100 error with the motor turning; 101 error at standstill (N5 p82). Bit 12 stays set after the first completed homing since restart and is reset during later homing or on homing error (N5 p83).
  - Methods: 1-14 with index; 17-30 without index; 33 and 34 to the next index; 35 at the current position. Negative numbers -1, -2, -7 to -14, -17, -18 and -23 to -30 select homing on block (N5 p84).
  - Block detection: 203Ah:01 is a current threshold in mA, or a percentage of 2031h if negative; 203Ah:02 is the time in ms to keep pushing (N5 p84, p268). Homing on block works only in closed loop (N5 p84).
  - Speeds: 6099h:01 searches for the switch, preset 50; 6099h:02 searches for zero, preset 10, and must be lower than :01. The ramp is 609Ah, used only when starting (N5 pp386-387). The minimum detectable index pulse width is 31.25 µs (N5 p387).
  - Home and limit switches must be enabled in 3240h, and 3701h must be -1 so that limits do not block homing (N5 p82).
  - **6098h preset is 23h = method 35** (N5 p385; OD N5-1-2/2213 default 35). Method 35 does not need Operation enabled (N5 p89).
  - 6063h is not zeroed by homing; 6062h/6064h are (N5 p360). 3243h captures the home-switch position (N5 p321).
  - The same text applies to C5-E, CL4-E and PD4-C (C5E pp80-87; CL4E pp80-87; PD4C pp71-78). NT-D11 covers the text errors.
- **IP, 6060h = 7 (N5 pp89-90).** Linear interpolation only, one buffered target in 60C1h:01; 60C4h:06 must be 1, and 60C2h is the period. Synchronisation starts when NMT Operational is entered; the SYNC period should be used (N5 p89).
- **CSP, CSV and CST, 6060h = 8, 9, 10 (N5 pp90-94).**
  - Targets are 607Ah, 60FFh and 6071h, absolute and sent each cycle; controlword bits have no mode function.
  - Statusword: bit 8 sync with the bus; bit 12 means the target is followed, otherwise the target is ignored; bit 13 following error (CSP).
  - **60C2h:02 must be -3 (1 ms base)**, and the cycle is 60C2h:01 ms, U8, preset 1 (N5 p92, pp394-395).
  - Offsets 60B0h, 60B1h and 60B2h (N5 pp391-392). CST needs closed loop (N5 p93).
- **Clock-direction, 6060h = -1 (N5 pp94-96).** Up to 1 MHz, minimum 200 ns pulse, 35 µs after a direction change. Step size is 2057h/2058h, ex works 128/1 = quarter step, where 512 is a full step. BLDC motors are treated as steppers (4 × pole pairs full steps per revolution). 205Bh selects TR or CW/CCW. On PD4-C, separate "special drive modes" in 4015h start clock-direction or analog speed from a virtual configuration switch (PD4C p326; FWUPD p12).
- **Auto setup, 6060h = -2 (N5 pp39-42, p96).**
  - Prerequisites: no load, nobody touching the motor, free rotation, and **no NanoJ program running** (2300h bit 0 = 0) (N5 p40).
  - It needs Operation enabled and a rising edge of bit 4. Bit 12 means done; bit 10 means an index was found (N5 pp41-42).
  - Results are saved automatically into categories drive (1010h:05) and tuning (1010h:06) (N5 p42). "After the auto setup, the internal coordinate system is no longer valid... Restart the device after an auto setup. Homing alone does not suffice" (N5 p42). It may starve fieldbus service (N5 p40).
  - Closed loop requires a completed auto setup (N5 p96).
  - PD4-C: done at the factory, recorded in 4016h:01 (PD4C p42, p85, p327). The PD4-C auto-setup chapter remains (PD4C p30).
- **Slow speed.** Requires closed loop plus 3202h bit 7, and an encoder. It switches to closed loop at 4000 × 60 / encoder ppr rpm with 10 % hysteresis (N5 pp54-56; C5E p52; CL4E p52; PD4C p44). It is absent in FIR-v1650 (FWUPD p9).
- **Plug & Drive interface (vendor, all families).** 2290h-2292h is an alternative command interface that "eliminates the need to run through the state machine" and is **enabled in factory settings** (PDI p5; EDS [2290] = 01h for every family member, IDX). Writing 2291h:04 executes switch-off, clear error, quick stop, OD read/write, auto setup (16), homing (17/18), PP absolute/relative (20/21), PV (23) or PT (25) **immediately** (PDI p6). These commands overwrite 6040h, 6060h, 607Ah, 6081h and 60FFh (PDI p14).

## 7. Units and scaling

The section is the same in N5, C5-E, CL4-E and PD4-C (N5 pp61-65; C5E pp59-63; CL4E pp59-63; PD4C pp50-54). A word comparison found C5-E identical; CL4-E and PD4-C differ only in table and figure layout in the extracted text.

- **Position.** 60A8h: bits 16-23 unit, bits 24-31 power of ten. Preset **FF410000h = tenths of a degree**: with a 1:1 gear, a relative move of 3600 is one mechanical revolution (N5 p64, p390). Units: m, in, ft, grade, rad, degree, arcminute, arcsecond, revolution (B4h), encoder increments (B5h), steps (ACh), electrical poles (C0h), and dimensionless (N5 pp62-63).
  - Gear ratio 6091h applies to all units except B5h, ACh and C0h; feed constant 6092h applies to length and dimensionless units (N5 p62).
  - In open loop or sensorless operation, one motor revolution is pole pairs × 65536 encoder increments (N5 p63). For a 2-phase stepper, one revolution is pole pairs × 4 steps; for a 3-phase BLDC, pole pairs × 6 (N5 p63).
  - FIR-v1650 used **2000 increments per revolution** in 608Fh as its default unit (FWUPD p5).
- **Velocity.** 60A9h: bits 8-15 time unit, 16-23 position unit, 24-31 exponent. Preset **00B44700h = rpm** (N5 p391). The p65 examples mislabel the fields (NT-D10). A further factor is 6096h:01/:02 (N5 p65, p383). Velocity mode (6042h) is always rpm, adjustable only through 604Ch (N5 p65).
- **Acceleration and jerk.** Acceleration is the speed unit per second, times 6097h; jerk is acceleration per second, times 60A2h (N5 p65). Presets 6083h/6084h = 500 and 6085h = 5000 (N5 pp376-377), so rpm/s by default.
- **Encoder and feedback.**
  - 608Fh and 6090h mirror the physical resolution of the sensor selected in 3203h, taken from 60E6h/60EBh (N5 pp378-381; FWUPD p5). 6063h is in raw increments and is invalid when 608Fh = 0 (N5 pp360-361).
  - 3203h has one bit mask per feedback: bit 0 position, bit 1 velocity, bit 2 commutation. The search starts at feedback 2, and subindex 01h is always the sensorless observer (N5 pp51-52, pp298-300).
  - Additional gear and feed constants for each feedback are 60E8h/60EDh and 60E9h/60EEh (N5 p43). An encoder without an index needs manual 60E6h/60EBh (N5 p43).
- **Open versus closed loop.**
  - Open loop drives the commanded field with full set current: 6073h × 203Bh:01 / 1000, limited by 2031h (N5 p368; FWUPD p5). There is no load-angle feedback (N5 p46).
  - Standstill current reduction: 3202h bit 3, idle time 2036h, value 2037h (N5 p47).
  - In open loop the actual position comes from the commanded field unless 3203h selects an encoder for display (N5 p300).
- **Torque.** As in section 6, in thousandths of the torque that corresponds to 203Bh:01. The manuals give no N·m conversion, and none is in the EDS.
- **Change timing.** Unit objects written in Operation enabled are not applied until that state is left (N5 p62).
- **Families.** CL4-E and PD4-C have the same unit objects. PD4-C has no 2059h encoder configuration or 3390h Hall object (EDS comparison: these exist in N5-1-2/2213 but not in PD4-C), and its encoder is the integrated absolute one (PD4C p18). PD4-CB: 2030h = 3 pole pairs and 6073h = 2000 (200 %) by default (IDX), against 50 and 1000 elsewhere.

## 8. Limits and protection

- **Limit switches.** Inputs 1-3 can be negative limit, positive limit and home once enabled in 3240h:01. 3240h:02 inverts, :03/:04 force a value, :06 selects 5 V or 24 V levels, and :08 enables input routing through 3242h (N5 pp97-101). On a limit, the position is stored, statusword bit 7 is set and 3701h acts (N5 p66):
  - -2: no reaction, discard the stored position.
  - **-1 (factory default): no reaction**, only note the position.
  - 0: coast, Switch on disabled.
  - 1 or 2: ramp, then Switch on disabled.
  - 5 or 6: ramp, then Quick stop active, energised.

  Travel beyond the noted position is blocked unless 3701h is -1 or -2. Leaving Quick stop active after a limit needs a bit 2 edge, and clearing needs 3701h = -2 (N5 pp66-67). Limits are not used unless enabled; an unconnected input does not prove the function is off.
- **Software limits.** 607Dh:01/02 limit absolute targets; home offset is not considered. Outside the range, only moves towards it are accepted (N5 p67, pp372-373). The presets are 0/0 (NT-D18). 607Bh sets the modulo range (N5 pp370-371).
- **Following and slippage.** 6065h preset 256 user units, with FFFFFFFFh turning it off; 6066h preset 100 ms (N5 pp361-362). 60F8h preset 400 with 203Fh, and 7FFFFFFFh turns it off (N5 p409). The reaction is 3700h, **default no reaction** (N5 p337). An error enters 1003h only if a reaction is defined (N5 p362). The PDI description confirms "the factory settings correspond to no action taken" (PDI p13).
- **Current.** 2031h is the maximum motor current in mA RMS and caps everything: N5-1-2 1000, N5-2-2 1800 (N5 p260). 6075h/203Bh:01 is the rated current in mA, and 6073h is the maximum current in tenths of a percent, preset 1000 (N5 pp368-369). I2t works in closed loop only, via 203Bh:01/02 and 6073h > 1000; in open loop the current is reduced to the lowest of 203Bh:01, 6073h and 2031h (N5 pp109-110, p268). With sine commutation, a winding can carry up to √2 × the set current (N5 p39). The activation conditions are worded inconsistently (NT-D12).
- **Temperature.** The power stage switches off at about 75 °C on the power board, setting the error bit; after cooling, a fault reset is required (N5 p19). 4014h reports supply and logic voltage in mV and PCB temperature in 0.1 °C (N5 pp340-341). The same 75 °C applies to CL4-E (CL4E p16) and PD4-C (PD4C p18).
- **Voltage.** 2034h/2035h are the upper and lower warning levels (N5 p260); the ballast is 4021h (N5 pp341-343).
- **Interlock.** If enabled (3240h:01 bit 3), 60FDh bit 3 = 0 produces a fault with the 605Eh reaction, error 5440h (N5 p101, p164).
- **Host loss.** 6007h, application category, preset 1. Its description says: "If an error (watchdog, heartbeat, etc.) occurs on the bus, the controller automatically switches to the SAFEOPERATIONAL state" (NT-D08). Values:
  - -1: set all RPDO inputs to 0, including 6040h, so the motor coasts.
  - 0: no reaction.
  - 1: fault, with the 605Eh reaction.
  - 2: disable voltage, coasting.
  - 3: quick stop per 605Ah.

  (N5 pp345-346; C5E p343; CL4E p367; PD4C p332.) The manual does not list the events that trigger 6007h. The documented bus-supervision mechanisms are the heartbeat consumer 1016h (N5 p177), node guarding with 100Ch/100Dh (N5 p168) and the synchronous window 1007h (N5 p166), and all three are off by default. **With factory defaults, no host-loss supervision is documented as active.** The 1029h:01 NMT reaction and the 6007h drive reaction are separate objects (N5 p182, p345). FIR-v1650 documents neither 6007h nor 1016h (section 3).
- **Watchdog and error numbers.** 1003h error numbers include watchdog reset (0), fieldbus error (4), node-guarding timeout (6), PDO timeout (17), PDO length errors (19, 20), unexpected sync length (27), slippage (30), following error (13) and "rated current must be set" (22) (N5 pp163-165).

## 9. I/O

- **N5 (N5 p18, pp97-106).**
  - Six inputs: 1-4 switchable between 5 V and 24 V (factory 5 V), 5-6 wide range.
  - Two open-drain outputs, 24 V / 500 mA, plus a brake output on X4.
  - Two 10-bit analog inputs, ±10 V or 0-20 mA (3220h/3320h-3323h).
  - Encoder input up to 65536 increments per revolution.
  - Inputs are sampled every 1 ms (N5 p97).
  - 60FDh: special functions in bits 0-15, levels of input n in bit 16 + n - 1 (N5 pp410-411). 60FEh:01: brake in bit 0, outputs in bits 16+ (N5 p412). Output routing is 3250h/3252h (N5 pp101-106).
- **C5-E.** Inputs 1-5 are 5 V/24 V switchable, and inputs 1 and 2 double as clock and direction. Analog input 1 is 0-10 V, with input 1 also usable as a current input. There is a USB power signal for routing (C5E pp96-99, p104).
- **CL4-E.** Four digital inputs at 24 V, or 5 V on -5VDI; one 0-10 V analog input; two positive-switching outputs (CL4E p16).
- **PD4-C.** Four inputs, 5 V/24 V switchable (factory 5 V); one 0-10 V analog input; two open-drain outputs, 24 V / 100 mA (PD4C p18). The brake is available only as a routed output (section 5). The logic supply needs 4013h:01 bit 0 (PD4C p29).

The same configurable routing applies to all families: configured input functions are active whether or not anything is wired to the terminal.

## 10. Persistence

- **Mechanism (N5 pp110-116; C5E pp108-114; CL4E pp106-111; PD4C pp97-102).** Only whole categories can be saved. Writing 65766173h ("save") to 1010h:nn starts the save; the drive overwrites the value with 1 when finished. Writing 64616F6Ch ("load") to 1011h:nn discards saved data, effective after a restart. A restart is triggered by 2800h:01 = 746F6F62h ("boot"), with an optional delay in 2800h:02 (N5 p115, p297).
- **Subindices.** 01 all categories except CANopen and Ethernet (for N5); 02 communication; 03 application; 04 customer; 05 drive; 06 tuning; 0A CANopen; 0C Ethernet (N5 only); 0B Modbus RTU (CL4-E, CL4E p36). 1011h:01 resets everything except tuning, CANopen and Ethernet, so auto-setup results survive a factory reset (N5 p116).
- **Side effects of saving.** "Control may be affected while saving... The motor must be at a standstill" (N5 p114). The fieldbus may be affected, saving "may take a few seconds", and power must not be interrupted (N5 p115). A full or damaged memory gives errors 14, 16 and 21, and error 16 resets all saved objects to default (N5 p163). Saved values survive a firmware update (N5 p110), except that the FIR-v1650 to FIR-v2039 update on the N5 deleted the file system and its stored files (FWUPD p5).
- **Command objects are saveable.** Category application includes **6040h controlword, 6060h mode, 607Ah, 60FFh, 6071h, 6042h, 60FEh outputs, 2300h NanoJ control, 6007h and 2800h** (N5 pp112-114; OD N5-1-2/2213 `saveable`). The manuals do not say what a restored 6040h does at boot. The state machine still starts in Switch on disabled (N5 p58). A blanket "save all" therefore persists whatever command values are present at that moment.
- **Configuration file (C5-E, CL4-E).** cfg.txt on the USB storage is applied at every boot and wins over saved values (C5E p108, p35); USB-connected controllers stop the motor when the file is saved (C5E p30). The N5 no longer has a file system from FIR-v2039 (FWUPD p5).
- **1020h configuration stamp.** Date and time are written before saving, and any later write to a saveable object resets them to 0 (N5 p116, pp180-181). This lets a profile detect an unsaved or unknown configuration.
- **Volatile objects.** Status and actual values, 1003h, 230Eh timer, 230Fh uptime and 2400h NanoJ inputs are not saved (N5 p148, p286).

## 11. Timing statements

| Item | Statement | Evidence |
|---|---|---|
| Application, NanoJ and position loop | 1 ms | N5 p67 |
| Velocity and current loops | 250 µs, 62.5 µs | N5 p67 |
| Input sampling | 1 ms; shorter pulses are missed | N5 p67, p97 |
| PP sampling | target reached is evaluated per 1 ms sample | N5 p73 |
| CSP/CSV/CST and IP period | integer ms (60C2h:02 = -3) | N5 p92, p395 |
| SYNC producer period | multiples of 1000 µs | N5 p165 |
| TPDO inhibit | 100 µs units, preset 10 ms | N5 p130, p210 |
| NanoJ start | up to 200 ms | N5 p283 |
| NanoJ CPU share | about 30-50 % of each 1 ms slot | N5 p147 |
| Save duration | "a few seconds" | N5 p115 |
| Auto setup | CPU heavy; the fieldbus may not be serviced in time | N5 p40 |
| Boot | 2084h boot-up delay (preset 0); boot loader and firmware boot-up messages | N5 p275, p132 |
| Clock-direction | 1 MHz max, 200 ns min pulse, 35 µs after a direction change | N5 pp94-95 |

The manuals give no SDO response time, no boot duration in ms and no reset delay. Do not invent them.

## 12. Vendor objects with side effects

| Object | Effect | Evidence |
|---|---|---|
| 2290h-2292h Plug & Drive interface | Enabled ex works; 2291h:04 starts motion, homing or auto setup and clears errors immediately, bypassing CiA 402; overwrites 6040h/6060h/607Ah/6081h/60FFh | PDI pp5-6, p14 |
| 2300h NanoJ control | Bit 0 starts the NanoJ program (rising edge reloads it); a loaded program starts at power-on unless 2300h bit 0 is 0; the object is saveable | N5 p147, p283 |
| NanoJ mapping | A program that maps 6040h rewrites it every 1 ms, so the fieldbus loses control; a program that restarts the controller can loop | N5 pp154-155 |
| 2800h:01 | Firmware reboot with an optional delay | N5 p297 |
| 2800h:03 (C5-E) | Switches the short-circuiting of motor windings in boot-loader mode | C5E p287 |
| 1010h / 1011h | Save takes seconds, disturbs control and fieldbus; restore takes effect after a reboot | N5 pp114-116 |
| 3202h | Changes the motor type, loop, brake control, auto alignment, torque mode and slow speed; bit 0 is locked in Operation enabled | N5 pp297-298 |
| 6060h = -2 | Auto setup moves the motor and invalidates the coordinate system until a restart | N5 p42 |
| 60F2h bits 4-5 | The drive itself clears 6040h bit 4 | N5 p407 |
| 3701h = -2 | Discards the stored limit-switch positions | N5 p66 |
| 1005h bit 30 | The drive becomes a SYNC producer | N5 p165 |
| 2007h:01 | Suppresses the boot-loader boot-up message | N5 p252 |
| 2007h:02 (fw 2538 only) | "Automatic COB-ID", default 1; undocumented | EDS N5-1-2/2538 [2007sub2]; NT-D02 |
| 2005h/2009h/2006h | Bit rate, node ID and node offset; effective after Reset Communication or a power cycle | N5 p37; PD4C p238 |
| 2102h (CL4-E) | Enables or disables fieldbuses | CL4E p294 |
| 4015h (PD4-C) | Special drive modes (clock-direction or analog speed) | PD4C p326 |
| 2700h (PD4-C) | NanoJ user storage; saves on write of 1 | PD4C p103 |
| 4013h:01 (PD4-C) | Logic-supply use; must be saved | PD4C p29 |
| 3212h:04 | Switches to the new control parameters 321Ah-321Eh and converts the old ones | N5 p49 |

## 13. What a profile must not assume

1. That the device runs FIR-v2213. Read 1018h:03 and 100Ah. FIR-v1650 (the stock manual) and FIR-v2538 (packaged) differ in objects, units and protection (NT-D01, NT-D02).
2. That 6040h with an all-off value is harmless on enable. Entering Operation enabled, or changing mode there, with bit 4 already set starts a PP move (N5 p68; FWUPD p6).
3. That host loss stops the motor. 1016h, 1017h and node guarding are off by default (N5 p177-178, p168). 6007h acts only if supervision is configured, and its value -1 coasts rather than ramping (N5 p345).
4. That following errors or limit switches stop the drive. 3700h = -1 and 3701h = -1 by default (N5 p337, p66).
5. That the CiA 402 state machine is the only motion path. The Plug & Drive interface and NanoJ can move the motor or overwrite 6040h (PDI p5; N5 p154). Check 2301h/2300h and 2290h before claiming exclusive control.
6. That node ID and bit rate are object values. On N5, C5-E and PD4-C the rotary switch overrides them (N5 p37).
7. That LSS is unavailable because the EDS says so, or available because the manual says so (NT-D05).
8. That "save all" is a neutral operation. It persists command objects and outputs (section 10) and must happen at standstill (N5 p114).
9. A torque unit in N·m, or rated torque from 6076h. 6076h is absent; the reference is the rated current in 203Bh:01 (N5 p367).
10. A fixed counts-per-revolution value. The unit depends on 60A8h, 6091h, 6092h, 3203h, the motor class and the firmware generation (N5 pp61-65; FWUPD p5).
11. That the PD4-C product code is unique per variant (NT-D14), or that the 1018h:02 table of one family applies to another.
12. That the boot-up message appears once (N5 p132), or that a node returns to Operational after reset. 1F80h defaults to 0 (N5 p250).
13. That auto setup is safe or unattended. It moves the motor, needs NanoJ stopped and requires a restart afterwards (N5 pp40-42).
14. That the C5-E or CL4-E configuration in saved objects is final. cfg.txt overrides it at boot (C5E p108).
15. That 607Dh = 0/0 means unlimited, or that a 6072h of 10 % is a useful torque limit (NT-D18, NT-D20).
16. That CL4-E or PD4-C have the N5 automatic brake (section 5).

## Discrepancies

- **NT-D01.** Stock firmware differs from the reviewed generation. Product pages link FIR-v1650 manuals as "Stock Firmware" (N5P12; C5EP209; PD4CPM08), and FWUPD p3 says the products are delivered with FIR-1650. N5V2 has no 1016h, 1F80h, 3701h, 3203h or 6007h (N5V2 pp5-8), uses 2000 increments per revolution (FWUPD p5), and forbids mode change in Operation enabled (FWUPD p6). Whether current production ships FIR-v1650, FIR-v2213 or FIR-v2538 is not documented.
- **NT-D02.** FIR-v2538 is shipped in the package for N5, C5-E, CL4-E and PD4-C (IDX), but no manual covers it for these models. EDS differences against fw 2213: 1018h:03 changes to 09EA0000h; new 2007h:02 "Automatic COB-ID", default 1; new 2017h LLDP; renamed subentries of 3242h, 20A0h/3390h and 4010h (EDS N5-1-2/2213 against /2538). The effect of "Automatic COB-ID" on PDO COB-IDs is unknown.
- **NT-D03.** The manuals' 100Ah preset is FIR-v2213-B1029645 (N5 p167; C5E p165; PD4C p155), but the package fw 2213 build is B1031134 (EDS [100A]). There are no release notes for either build.
- **NT-D04.** Bit-rate codes contradict each other. The commissioning tables (N5 p38, rendered; C5E p35; PD4C p29) give 78h = 100 kbit/s and 87h = 1000 kbit/s. The 2005h object pages (N5 pp250-251; C5E p248; CL4E p273; PD4C p237), the CL4-E commissioning table (CL4E p35), the FIR-v1650 table (N5V2 p38) and every EDS (default 136) give 88h = 1000 kbit/s, list no 100 kbit/s, and say an unknown value means 1000 kbit/s. N5 p11 calls the 3.3.0 table a "correction". What 87h selects is unresolved.
- **NT-D05.** LSS is documented in full (N5 pp134-146) and named as a commissioning path (C5E p35; CL4E p35; PD4C p29), yet every EDS has `LSS_Supported=0`.
- **NT-D06.** For the default node ID, N5 p36/p38, C5E p34 and PD4C p28 say node 1; 2009h preset is 7Fh (N5 p252); the LSS example says delivered as 7Fh (N5 p146); CL4E p34 says 127. On switch-equipped models the result depends on the as-delivered switch position, which these pages do not state.
- **NT-D07.** The SYNC frame is drawn with CAN-ID "80+NodeID" (N5 p119), while 1005h is 80h (N5 p165). The NMT example "000 | 01 2A" is described as "completely restarted", but 01h is Start Remote Node (N5 p119).
- **NT-D08.** 6007h refers to a "SAFEOPERATIONAL" state in a CANopen manual (N5 p345; same in C5E p343, CL4E p367, PD4C p332), and the triggering bus events are not enumerated.
- **NT-D09.** The PDO remapping procedure saves with 1010h:03 (application) (N5 p132), but the PDO objects belong to the communication category, 1010h:02 (N5 pp111-112).
- **NT-D10.** The 60A9h examples label the bit fields wrongly (N5 p65) against the field diagram (N5 p64, p391). The values themselves (00B44700h = rpm) are consistent with the diagram.
- **NT-D11.** Homing text errors: "Methods 17 to 32" versus "17 to 30" (N5 p84). Methods 23-26 and 27-30 are said to use the "home switch" where the limit switch is meant, and the method 26 text mentions an index pulse (N5 p87). The default method 35 does not need Operation enabled (N5 p89, p385), so a homing command with factory settings sets the position without motion. The same applies to all four manuals.
- **NT-D12.** The I2t activation conditions differ. N5 p110 says "maximum current must be greater than the rated current". N5 p268 says 6073h > 1000. N5 p270 compares 203Bh:01 in mA with 2031h and 6073h, although 6073h is per mille.
- **NT-D13.** The state diagram marks where mode selection is admissible (N5 p57, rendered), while the text says the mode "can be set or changed at any time" (N5 p58). FWUPD p6 adds that a mode change in Operation enabled with bit 4 set starts the move.
- **NT-D14.** The PD4-C 1018h:02 table gives two codes per variant: 04h/BAh (5918M), 05h/BBh (6018L), 06h/BDh (CB59M), 18h/BCh (5918L4204), 2Ch/B9h (5918X), plus 28h for PD4-C5918L2104-E-08 (PD4C p166, rendered). The EDS uses B9h-BDh and 28h (IDX). Which hardware revision reports which code is not stated, and 5918L2104 is missing from the cover (PD4C p1).
- **NT-D15.** The PD4-C introduction says "brushless motor with integrated controller" (PD4C p10), while the data table types the PD4-C as a stepper and the PD4-CB as BLDC (PD4C p17), consistent with 1000h (PD4C p146).
- **NT-D16.** 1003h says entries 1-7 shift and entry 7 drops out, but eight subindices exist (N5 pp161-163).
- **NT-D17.** Heartbeat or guarding activity in NMT Stopped is not shown in the service table (N5 p118, rendered).
- **NT-D18.** The meaning of the 607Dh presets 0/0 is not stated: disabled, or a range of zero (N5 p67, p372).
- **NT-D19.** The 60F2h rotary options cite 607Dh:01 where the position range limit 607Bh is meant (N5 p408). The preset 60F2h = 1 (relative to the ramp output) differs from the 60F2h = 0 examples (N5 pp72-73, p407).
- **NT-D20.** The 6072h max torque preset 0064h equals 10 % of rated torque (N5 p368; EDS [6072] for all members). It is not explained, and it limits PT/CST by default.
- **NT-D21.** For LSS activate bit timing, the frame shows cs=15h while the field text says "Value = 51h" (N5 p139). The LSS example lists the store confirmation after the switch-to-wait request (N5 p146).
- **NT-D22.** 320Eh is said to be eliminated in FIR-v22xx (N5 p49), but it remains as a private object in the fw 2213 OD (OD N5-1-2/2213 entry 0x320E, private=true).
- **NT-D23.** The N5 and PD4-C load sequence mentions "the configuration file... if the controller has one" (N5 p37; PD4C p28) without describing one for those models, and FWUPD p5 says the N5 file system was removed.
- **NT-D24.** Host-loss supervision is under-specified. There is one consumer slot (N5 p177). The interplay and order of EMCY 8130h, the 1029h:01 NMT change (default to Pre-operational, which stops PDOs) and the 6007h drive reaction is not described (N5 p164, p182, p345). Whether the consumer arms before the first heartbeat is not stated.
- **NT-D25.** The 3242h subindex labels in the fw 2213 EDS say subindex 1 drives "Bit #1", while the manual says subindex 01h drives bit 0 (N5 p99). The fw 2538 EDS labels match the manual.
- **NT-D26.** Statusword bit 9 "Remote" is always 1 (N5 p348), so it does not report whether the controlword is being processed, unlike its CiA 402 meaning. The PDI and NanoJ paths can still overwrite 6040h (section 12).
- **NT-D27.** The package has two EDS flavours for fw 2213 (public subset and all objects) but only the all-objects one for fw 2538 (IDX). Which one a master should use is not documented.
- **NT-D28.** CL4-E CANopen and Modbus RTU share X1 (CL4E p19), and the 2102h preset 0019000Bh appears to enable both (CL4E p294). Behaviour with both buses active is not documented.
- **NT-D29.** 1009h hardware version is "0" (N5 p167), so the W007 and W007b distinction that changes the N5-2-2 voltage range (N5 p18) cannot be read through the documented objects.
- **NT-D30.** The PDO mapping text describes mapping entries 01h-0Fh (N5 p128, p130), while the mapping objects and the EDS provide 8 entries (N5 p209; EDS [1600] SubNumber=9).
- **NT-D31.** FWUPD p11 lists 1029h Error Behavior as new in FIR-v2039, but the FIR-v1650 manual already documents it (N5V2 p151). Its FIR-v1650 behaviour was not reviewed.

## What a profile can implement from this evidence

For N5-1-2 / N5-2-2 on FIR-v2213 build B1031134, with matching EDS stored, a profile can implement:

- An identity gate on 1018h, 100Ah and 1000h, rejecting FIR-v1650 and FIR-v2538 until reviewed.
- NMT, 1F80h awareness, and handling of two boot-up messages.
- SDO expedited and segmented upload.
- 8 + 8 PDO configuration in Pre-operational under the documented COB-ID rules.
- SYNC consumption.
- EMCY and 1003h decoding with the documented error tables.
- Producer heartbeat and the one-slot consumer, with explicit 1029h/6007h configuration as an opt-in.
- The CiA 402 state machine with the documented edge requirements and stop-option tables.
- PP, PV, VL and HM with typed unit configuration through 60A8h/60A9h/6091h/6092h/6096h/6097h.
- CSP/CSV on a 1 ms-multiple SYNC.
- PT/CST only with closed-loop and auto-setup evidence.
- Explicit, standstill-only category saves.

The same text covers C5-E (switches S2/S3, USB cfg.txt priority), CL4-E (object-only node ID, shared CAN/RS-485 connector, no brake connector) and PD4-C (single switch with offset, factory auto setup, fixed motor class, dual product codes). These can be applicability rows with their own identity tables.

Blocked until evidence exists:

- FIR-v2538 behaviour, including "Automatic COB-ID".
- The meaning of 2005h value 87h.
- LSS support on real units.
- Which firmware ships today.
- The PD4-C product-code mapping.
- The order of host-loss reactions.
- Any hardware qualification.
