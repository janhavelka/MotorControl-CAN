# CANopen and CiA 402 principles (pack CP)

Reviewed 2026-11-19. This is a cited summary of the CANopen (CiA 301, 305, 306) and CiA 402 behavior that every CANopen family promptset relies on. It is not the normative standard: the full CiA texts need a login and were not obtained (see `remote_only` in [SOURCES.json](SOURCES.json)). Nothing here is device or hardware evidence. A drive profile must still confirm each fact in that drive's own manual and EDS.

## How to read the citations

| Marker | Meaning |
|---|---|
| **[CiA]** | Published by CAN in Automation: the public overview pages in this pack and the CiA 301 poster already stored at [../public/canopen_poster_2024.pdf](../public/canopen_poster_2024.pdf). These summarize the standard; they are not the normative text. |
| **[V]** | Vendor restatement of the standard (tutorial, whitepaper or drive manual). It may contain device-specific deviations or errors. |
| **[I]** | Open-source implementation (CANopenNode, python-canopen). It shows how one stack behaves, not what the standard requires. |
| **not established** | No stored free source states the fact. Do not implement it as a standard fact. |

Source IDs: pack sources CIAOV, CIANMT, CIAEC, CIASDO, CIAPDO, CIASFP, CIAARCH, CIA305, CIA306, CIA402, CIA890, CSS, CSSEDS, IXXAT, TECHNO, AKD, PYCODOC, CONODE_LSS and CONODE_SYNC ([SOURCES.json](SOURCES.json)). Cross-references to existing repository files: POSTER (CiA poster, one PDF page), N5 ([Nanotec N5 manual 3.3.0](../multidrive/vendor/nanotec_n5_canopen_v3.3.0.pdf)), EPOS4FW (maxon EPOS4 Firmware Specification 2026-07, `docs/reference/maxon_epos4_canopen/vendor/maxon_epos4_firmware_specification_rel13740.pdf` in the sibling pack, held out of the repository until the owner decides on its retrieval method; the same document is listed remote-only in the multidrive pack), PYCO ([python-canopen 14b3336](../public_code/canopen_python/canopen/)) and CONODE ([CANopenNode 9b8beed](../public_code/canopennode/301/)). `pN` is the one-based PDF page. For TECHNO and AKD the printed page equals the PDF page; EPOS4FW printed "2-15" is PDF p15. HTML sources are cited by section heading.

## 1. Object dictionary, identity and persistence

- Every object has a 16-bit index and an 8-bit sub-index [CiA CIAARCH]. Ranges: 0001h-025Fh data types, 1000h-1FFFh communication profile, 2000h-5FFFh manufacturer-specific, 6000h-9FFFh standardized profile, A000h-AFFFh network variables, B000h-BFFFh system variables, the rest reserved [CiA POSTER "Object dictionary"]. IXXAT p4 splits 6000h-9FFFh into 800h-wide logical devices (6000h-67FFh is device 1) [V].
- Multi-byte values are little-endian. In an SDO frame index 6040h is sent as byte 1 = 40h, byte 2 = 60h [V AKD p34; IXXAT p11].
- 1000h, 1001h and 1018h must be in an EDS's mandatory list. CSSEDS says 1018h is mandatory "from CANopen 4.0" [V CSSEDS "[MandatoryObjects]"].
- 1000h device type: the low 16 bits are the device profile number (402 = 0192h) and the high 16 bits carry additional information. AKD reads 0002_0192h, where type 2 means a drive [V AKD p46]. The split of the additional-information field is not established (CP-D15).
- 1001h error register bits: 0 generic, 1 current, 2 voltage, 3 temperature, 4 communication, 5 device-profile-specific, 6 reserved, 7 manufacturer-specific [V TECHNO p47]. It is byte 2 of every EMCY frame [CiA POSTER].
- 1018h identity: sub 1 vendor-ID, 2 product code, 3 revision number, 4 serial number. All four together form the 128-bit LSS address [CiA CIA305; V TECHNO p23]. The meaning of the revision and serial bits is vendor-specific: AKD puts the CAN version in the upper word and production data in the serial number [V AKD p57-58].
- 1010h store and 1011h restore: write the signature "save" (6576_6173h) or "load" (6461_6F6Ch) to sub 1. Reading sub 1 returns a capability value; Technosoft returns 1, meaning "saves on command only" [V TECHNO p48-49]. Which objects are stored depends on the device; Technosoft lists them and applies restored defaults only after a reset [V TECHNO p48-49]. Store/restore always has persistent side effects. A host must never trigger it implicitly. IXXAT p16 describes the two configuration strategies: store once in the device, or configure every boot from the NMT master [V].
- The POSTER lists objects 1000h-1029h with their object codes [CiA]. It shows 1010h and 1011h as VAR; vendor manuals implement them as ARRAY (CP-D06).

## 2. CAN identifiers and COB-ID entries

Pre-defined connection set for node-ID n (1-127) [CiA POSTER "Pre-defined CAN-IDs"; V IXXAT p16; CiA CIA305]:

| Service | CAN-ID | COB-ID object |
|---|---|---|
| NMT command | 000h | not configurable |
| SYNC | 080h | 1005h |
| EMCY | 080h + n | 1014h |
| TIME | 100h | 1012h |
| TPDO1 / RPDO1 | 180h + n / 200h + n | 1800h:01 / 1400h:01 |
| TPDO2 / RPDO2 | 280h + n / 300h + n | 1801h:01 / 1401h:01 |
| TPDO3 / RPDO3 | 380h + n / 400h + n | 1802h:01 / 1402h:01 |
| TPDO4 / RPDO4 | 480h + n / 500h + n | 1803h:01 / 1403h:01 |
| Default SDO server-to-client / client-to-server | 580h + n / 600h + n | 1200h, not configurable |
| NMT error control (boot-up, heartbeat, guarding) | 700h + n | not configurable |
| LSS slave-to-master / master-to-slave | 7E4h / 7E5h | fixed |

- The NMT, default SDO and heartbeat identifiers are not configurable, so a node can always be reached by node-ID [V IXXAT p15]. The other identifiers are defaults that may be reconfigured, and by default no RPDO listens to any TPDO [V CSS "COB-ID customization"].
- CANopen CC allows bit rates from 10 to 1000 kbit/s. All pre-defined identifiers are 11-bit, but 29-bit CAN-IDs are also supported [CiA CIAOV "CANopen CC lower layers"].
- COB-ID entries are UNSIGNED32. Bit 31: 0 = object valid/exists (PDO, EMCY). Bit 30: for a PDO, 1 = no RTR allowed; in 1005h, 1 = this device produces SYNC. Bit 29: 0 = 11-bit, 1 = 29-bit frame. Bits 28-0: the identifier. Bits 0-29 must not change while the object is valid [V TECHNO p31, p48, p52; IXXAT p15]. CANopenNode also rejects a valid PDO COB-ID that uses a restricted CAN-ID [I CONODE CO_PDO.c:544-546].

## 3. NMT state machine and boot-up

- States: Initialisation (substates initialising, reset application, reset communication), Pre-operational, Operational and Stopped [CiA CIANMT; POSTER "Network management"].
- After initialization the device enters Pre-operational on its own and sends the boot-up message [CiA CIANMT]. Some devices can be configured to start themselves, entering Operational without an NMT start [V IXXAT p8]. A profile must read the state and must not assume it.
- Pre-operational: SDO allowed, PDO not. SYNC, TIME and heartbeat may be produced. Operational: all services. Stopped: only NMT and error control (heartbeat or guarding) [CiA CIANMT; V IXXAT p7].
- Reset application sets the application area to power-on values: 2000h-9FFFh according to CIANMT, 2000h-FFFFh according to POSTER (CP-D05). It then runs reset communication, which resets 1000h-1FFFh [CiA CIANMT; POSTER].
- NMT frame: CAN-ID 000h, DLC 2, byte 0 = command specifier, byte 1 = node-ID (0 = all nodes) [CiA CIANMT; POSTER].
- Command specifiers: 01h start (to Operational), 02h stop (to Stopped), 80h enter Pre-operational, 81h reset node (application reset), 82h reset communication [CiA POSTER; V TECHNO p44-45; CSS "NMT"]. python-canopen also maps 50h/60h to sleep/standby, which are not CiA 301 states [I PYCO nmt.py:15-44].
- POSTER transitions: (1) power on; (2) automatic to Pre-operational; (3)(6) start; (4)(7) enter Pre-operational; (5)(8) stop; (9)(10)(11) reset application; (12)(13)(14) reset communication; (15) power off or hardware reset [CiA POSTER].
- NMT commands are unconfirmed. Only the next heartbeat shows the new state [V IXXAT p8; TECHNO p44].
- Boot-up: CAN-ID 700h + n, DLC 1, data 00h, sent as the last step of initialization. An unexpected boot-up during operation means a reset or a newly added node [CiA CIAEC "Boot-up protocol"].
- 1029h error behavior: sub 1 handles communication errors, sub 2 other device errors. 0 = go to Pre-operational (if Operational), 1 = no change, 2 = go to Stopped [V N5 p181-182].
- NMT state and the CiA 402 drive state are separate. No stored source says that an NMT stop stops the motor. AKD only says it stops PDO exchange [V AKD p31].

## 4. Heartbeat, node guarding and loss of the host

- Heartbeat producer: 1017h (UNSIGNED16, ms, 0 = off) [CiA CIAEC; POSTER; V TECHNO p52 "multiple of 1 ms"; AKD p57]. Frame: CAN-ID 700h + n, DLC 1, byte 0 = NMT state: 00h boot-up, 04h Stopped, 05h Operational, 7Fh Pre-operational [CiA POSTER; V IXXAT p9].
- Heartbeat consumer: 1016h is an array of UNSIGNED32 entries. Bits 31-24 are reserved, 23-16 hold the producer node-ID and 15-0 the time in ms; 0 means the entry is unused. Monitoring starts with the first received heartbeat [V AKD p56]. The consumer time must exceed the producer time [V AKD p56]; IXXAT suggests twice the producer time as a rule of thumb [V IXXAT p9].
- A missed heartbeat is reported locally. The recommended EMCY code is 8130h [V IXXAT p15; TECHNO p46].
- Node and life guarding: the master sends an RTR frame on 700h + n and the slave replies with one byte. Bit 7 is a toggle that changes on every reply; bits 6-0 hold the state [V AKD p38; I PYCO nmt.py:124-125 masks with 7Fh]. Life time = guard time 100Ch (ms) x life time factor 100Dh, and 0 disables it [V TECHNO p50; AKD p38]. CiA no longer recommends remote-frame services [CiA CIAEC].
- What the drive does when the host disappears is device-specific. AKD raises fault F129 and brakes to a stop [V AKD p38]. Technosoft acts according to 6007h (section 13) [V TECHNO p64-65]. N5 changes NMT state according to 1029h [V N5 p181-182]. Heartbeat observed by the host proves nothing about the drive's own reaction to host loss.

## 5. SDO

- Default server channel 1200h: client-to-server 600h + n, server-to-client 580h + n [CiA CIASDO; V TECHNO p30]. Server parameter sets are in 1200h-127Fh and client sets in 1280h-12FFh [CiA CIASDO].
- Every SDO frame has DLC 8. Byte 0 is the command specifier, bytes 1-2 the index (little-endian), byte 3 the sub-index and bytes 4-7 the data [CiA POSTER "Service data object"; V AKD p33-34].
- SDO is confirmed: the client waits for the response before it sends the next request on that channel [V AKD p33].
- Initiate command byte: bits 7-5 ccs/scs; bit 4 reserved; bits 3-2 n = number of bytes in 4-7 that carry no data (valid when e = s = 1); bit 1 e = expedited; bit 0 s = size indicated [V AKD p33-34; I PYCO sdo/client.py:272, 376].

| Frame | Byte 0 |
|---|---|
| Upload (read) request | 40h |
| Expedited upload response, 4/3/2/1 bytes | 43h / 47h / 4Bh / 4Fh |
| Expedited download (write) request, 4/3/2/1 bytes | 23h / 27h / 2Bh / 2Fh |
| Download response | 60h |
| Abort (from client or server), abort code in bytes 4-7 | 80h |

  [V AKD p33-35; IXXAT p11; I PYCO sdo/constants.py]. IXXAT's worked example contains errors (CP-D03).
- Segmented transfer [V CSS "Example 2"; I PYCO]: the initiate frame has e = 0 and gives the total size; for an upload the server sends it [V CSS]. Each segment has a 1-byte header and up to 7 data bytes, and the peer confirms every segment. Header: bit 4 toggle (10h), bits 3-1 n = unused bytes, bit 0 c = last segment [I PYCO sdo/constants.py; sdo/client.py:312, 417]. The toggle alternates on every segment [V CSS]; python-canopen starts at 0 and aborts with 0503_0000h on a mismatch [I PYCO sdo/client.py:247, 309-310]. Segment command classes: download segment request ccs = 0, download segment response scs = 1, upload segment request ccs = 3, upload segment response scs = 0 [I PYCO sdo/constants.py].
- Block transfer: ccs 6 = block download, 5 = block upload. Subcommands: 0 initiate, 1 end, 2 block response, 3 start upload. The CRC-supported flag is 04h. A block holds 1-127 segments [CiA CIASDO; V CSS "Example 3"/"Example 4"; I PYCO sdo/constants.py]. Each segment's byte 0 carries a sequence number in bits 6-0 and bit 7 = last segment. The receiver confirms each block with the last sequence number it received. The end frame carries n in bits 4-2 and, when CRC is supported, a 2-byte CRC [V CSS; I PYCO sdo/client.py:534-601]. Block upload needs an explicit start command; block download does not [V CSS].
- Many drives support only expedited and segmented transfer: AKD [V AKD p33] and Technosoft [V TECHNO p29].
- Abort codes [CiA POSTER "SDO abort codes"]:

| Code | Meaning | Code | Meaning |
|---|---|---|---|
| 0503 0000h | Toggle bit not alternated | 0607 0010h | Data type/length does not match |
| 0504 0000h | SDO protocol timed out | 0607 0012h | Length too high |
| 0504 0001h | Command specifier invalid or unknown | 0607 0013h | Length too low |
| 0504 0002h | Invalid block size (block mode) | 0609 0011h | Sub-index does not exist |
| 0504 0003h | Invalid sequence number (block mode) | 0609 0030h | Invalid value (download only) |
| 0504 0004h | CRC error (block mode) | 0609 0031h | Value too high (download only) |
| 0504 0005h | Out of memory | 0609 0032h | Value too low (download only) |
| 0601 0000h | Unsupported access | 0609 0036h | Maximum less than minimum |
| 0601 0001h | Read of a write-only object | 060A 0023h | Resource not available: SDO connection |
| 0601 0002h | Write to a read-only object | 0800 0000h | General error |
| 0602 0000h | Object does not exist | 0800 0020h | Data cannot be transferred or stored |
| 0604 0041h | Object cannot be mapped to the PDO | 0800 0021h | ... because of local control |
| 0604 0042h | Mapped objects would exceed PDO length | 0800 0022h | ... because of the present device state |
| 0604 0043h | General parameter incompatibility | 0800 0023h | OD generation failed or no OD present |
| 0604 0047h | General internal incompatibility | 0800 0024h | No data available |
| 0606 0000h | Hardware error | | |

  Vendors list subsets and use their own wording (CP-D28). A CAN transmit success or an SDO confirmation is not drive acceptance of motion (repository rule).

## 6. PDO communication and mapping

- A CANopen CC PDO is one CAN frame with up to 8 data bytes. It is unconfirmed (producer/consumer) and exchanged only in NMT Operational [CiA CIAPDO; CIANMT; V IXXAT p11].
- Parameter records: RPDO communication 1400h-15FFh, RPDO mapping 1600h-17FFh, TPDO communication 1800h-19FFh, TPDO mapping 1A00h-1BFFh [CiA CIAPDO; POSTER].
- Communication record: 00h highest sub-index, 01h COB-ID (U32), 02h transmission type (U8), 03h inhibit time (U16), 04h reserved, 05h event timer (U16), 06h SYNC start value (U8) [CiA POSTER; V IXXAT p13].
- Transmission type [V IXXAT p13; TECHNO p29]:

| Value | TPDO meaning |
|---|---|
| 00h | Acyclic synchronous: sent on SYNC if a mapped value changed (TECHNO lists it as reserved, CP-D19) |
| 01h-F0h | Cyclic synchronous: every n-th SYNC |
| F1h-FBh | Reserved |
| FCh | RTR-only, synchronous (not recommended) |
| FDh | RTR-only, event-driven (not recommended) |
| FEh | Event-driven, manufacturer-specific event |
| FFh | Event-driven, event defined by the device profile |

- A synchronous RPDO is applied when the next SYNC arrives, not when it is received [V IXXAT p12; I CONODE_SYNC].
- The inhibit time is in units of 100 us and applies to event-driven TPDOs [V TECHNO p30, p36; I CONODE CO_PDO.c:1027]; IXXAT says milliseconds (CP-D01). The event timer is in ms. On a TPDO it is the period; on an RPDO it is the reception timeout [V IXXAT p13; TECHNO p30; I CONODE CO_PDO.c:604, 1034]. IXXAT recommends EMCY 8250h for an RPDO timeout [V IXXAT p15].
- SYNC start value: the first synchronous TPDO is sent when the SYNC counter equals this value. It needs 1019h > 1 [V IXXAT p12-13; N5 p120]. CANopenNode rejects values above 240 and any change while the PDO is valid [I CONODE CO_PDO.c:1044-1045].
- A mapping entry is a U32: index (16 bits) | sub-index (8 bits) | length in bits (8 bits). For example 6041_0010h is the statusword, 16 bits [V TECHNO p38-40; AKD p84; CSSEDS "PDO mapping objects"]. Bit-wise mapping is allowed but not recommended [V IXXAT p12].
- Mapping kinds: static (fixed by the manufacturer), variable (changeable in Pre-operational only), dynamic (also changeable in Operational) [CiA CIAPDO].
- Remapping procedure [CiA CIAPDO "PDO mapping procedures"; V TECHNO p40]: (1) set bit 31 of the COB-ID, making the PDO invalid; (2) write 00h to mapping sub 0; (3) write the entries; (4) write the entry count to sub 0; (5) clear bit 31. AKD omits step 5 and says to map before Operational [V AKD p84] (CP-D18). CANopenNode enforces the order: it rejects mapping writes (ODR_UNSUPP_ACCESS) while the PDO is valid or sub 0 is non-zero [I CONODE CO_PDO.c:248-249], rejects a different CAN-ID while valid (ODR_INVALID_VALUE) [I CO_PDO.c:544-546], and rejects inhibit-time changes while valid [I CO_PDO.c:1023-1024]. Mapping errors abort with 0604_0041h or 0604_0042h [CiA POSTER].
- Remote-requested PDOs are not recommended [V IXXAT p13; CSS "PDO"].

## 7. SYNC and its counter

- The SYNC producer sends periodically on default CAN-ID 080h with DLC 0, or DLC 1 carrying a counter. The counter is optional and exists from CiA 301 v4.1 [CiA CIASFP; POSTER "Sync protocol"].
- 1005h COB-ID SYNC: bit 30 = 1 means this device produces SYNC, bit 29 selects the frame format, and bits 0-29 must not change while bit 30 = 1 [V TECHNO p48].
- 1006h communication cycle period in us; 0 = no SYNC production [V TECHNO p48; I CONODE_SYNC]. 1007h synchronous window length in us: synchronous PDOs must be sent inside this window after SYNC [I CONODE_SYNC; V N5 p120].
- 1019h counter overflow value: 0 = no counter. Otherwise the counter rises by 1 with each SYNC and starts again after reaching the overflow value [I CONODE_SYNC; V N5 p120]. N5 allows 02h-F0h and enables the function only for values above 1 [V N5 p120, p180]; CANopenNode enables it for any non-zero value (CP-D20). The counter's start value is not established.
- A SYNC of unexpected length raises EMCY 8240h [V IXXAT p15].
- Technosoft sends a 4-byte "high resolution time stamp" on 100h right after SYNC, which collides with the default TIME identifier (CP-D21) [V TECHNO p45, p50-51].

## 8. EMCY

- Frame: default CAN-ID 080h + n, DLC 8. Bytes 0-1 emergency error code (U16), byte 2 error register 1001h, bytes 3-7 manufacturer-specific [CiA CIASFP; POSTER "Emergency protocol"]. 1014h holds the COB-ID (bit 31 = 0 valid), 1015h the EMCY inhibit time, 1028h the consumed EMCY COB-IDs [CiA POSTER]. The unit of 1015h is not established by a stored source.
- An EMCY is sent once per error event, not repeatedly [CiA CIASFP]. Error state machine: the first error sends an EMCY; each further error sends another EMCY; when some errors clear, an EMCY with code 0000h carries the remaining error register; when all clear, an EMCY with code 0000h and error register 00h is sent [V AKD p32; IXXAT p14].
- 1003h pre-defined error field: sub 0 holds the number of entries and sub 1 the newest [V TECHNO p47; AKD p32].
- Error code classes: 00xxh no error/reset, 10xxh generic, 20xxh current, 30xxh voltage, 40xxh temperature, 50xxh hardware, 60xxh software, 70xxh additional modules, 80xxh monitoring (81xxh communication, 82xxh protocol), 90xxh external, F0xxh additional functions, FFxxh device-specific [V IXXAT p15]. Recommended communication codes: 8110h CAN overrun, 8120h error passive, 8130h heartbeat/guarding error, 8140h recovered from bus-off, 8150h CAN-ID collision, 8210h PDO length error, 8220h PDO length exceeded, 8240h unexpected SYNC length, 8250h RPDO timeout [V IXXAT p14-15]. A device that supports EMCY must support at least 0000h and 1000h [V CSS "EMCY"].
- Bytes 3-7 are vendor-defined: AKD byte 3 is a category [V AKD p32], N5 byte 3 an error number [V N5 p120], Technosoft copies vendor registers [V TECHNO p46]. An EMCY is a notification, not the drive state: the CiA 402 fault is statusword bit 3.

## 9. TIME

CAN-ID 100h, DLC 6: UNSIGNED28 ms after midnight, VOID4, UNSIGNED16 days since 1984-01-01 [CiA POSTER; CIASFP]. AKD does not support TIME [V AKD p31].

## 10. LSS (CiA 305)

- Master-to-slave 7E5h, slave-to-master 7E4h. The LSS address is 1018h sub 1-4 [CiA CIA305; V TECHNO p23]. LSS server states: waiting and configuration [I CONODE_LSS; V TECHNO p23].
- Command specifiers [V TECHNO p23-27; I CONODE_LSS]:

| cs | Service | Notes |
|---|---|---|
| 04h | Switch state global | mode 0 = waiting, 1 = configuration; unconfirmed |
| 40h-43h, reply 44h | Switch state selective (vendor-ID, product code, revision, serial) | all four needed; the matching slave enters configuration |
| 11h | Configure node-ID | 1-127, or FFh = unconfigured; reply 0 OK, 1 out of range, FFh manufacturer error |
| 13h | Configure bit timing | table selector 0 + table index; reply 0 OK, 1 not supported |
| 15h | Activate bit timing | switch delay in ms, used twice: before switching and before transmitting again |
| 17h | Store configuration | reply 0 OK, 1 not supported, 2 storage access error |
| 5Ah-5Dh | Inquire identity (vendor, product, revision, serial) | configuration state only |
| 5Eh | Inquire node-ID | configuration state only |
| 46h-4Bh, reply 4Fh | Identify remote slave (vendor, product, revision range, serial range) | used for range searches |
| 4Ch, reply 50h | Identify non-configured remote slave | answered by slaves with node-ID FFh |
| 51h, reply 4Fh | Fastscan | bit-pattern scan of unknown LSS addresses [CiA CIA305] |

- Bit-timing table indexes: 0 = 1000, 1 = 800, 2 = 500, 3 = 250, 4 = 125, 5 reserved, 6 = 50, 7 = 20, 8 = 10 kbit/s, 9 = automatic detection [I CONODE_LSS:180-195]. A device may support only a subset; Technosoft lists 0, 2, 3 and 4 [V TECHNO p25].
- A new node-ID takes effect only after NMT reset communication, and survives power-off only after store configuration [V TECHNO p24]. A slave set to FFh stays in reset communication and transmits only LSS replies [V TECHNO p24]. The master must ensure that only one slave is in configuration state when it assigns a node-ID [CiA CIA305].
- CiA warns that a bit-rate switch makes the network unusable if any device fails to follow, and that it is not suitable for initial bit-rate setting [CiA CIA305]. Node-ID and bit-rate changes are persistent configuration changes; a profile performs them only on explicit request.

## 11. EDS and DCF (CiA 306)

- An EDS describes an unconfigured device; it has no bit rate, node-ID or parameter values. A DCF describes one configured device and is derived from the EDS [CiA CIA306]. CiA 311 defines an XML alternative [V IXXAT p7].
- Syntax [V CSSEDS]: an ASCII INI-style text (ISO 646, at most 255 characters per line) with case-insensitive section and key names. Sections include [FileInfo]; [DeviceInfo] (vendor, product, supported bit rates, LSS support, numbers of RPDOs and TPDOs); [MandatoryObjects], [OptionalObjects] and [ManufacturerObjects]; and one section per object such as [1018] and [1018sub1]. Object keys: ParameterName, ObjectType (0x7 = VAR when absent), SubNumber, DataType (the data-type index), LowLimit, HighLimit, AccessType, DefaultValue, PDOMapping, ObjFlags. Values may use $NODEID formulas such as `$NODEID+0x180`. A DCF adds a device-commissioning section (node-ID, bit rate) and ParameterValue keys.
- A default COB-ID with bit 31 set marks a PDO that is disabled by default [V CSSEDS "Enabled vs. disabled PDOs"].
- EDS DefaultValue entries describe a nominal device. They are not live configuration and not an initialization recipe [CiA CIA306; repository rule in [../README.md](../README.md)].

## 12. CiA 402 power drive system state machine

CIA402 says the finite state automaton decides which commands are accepted and whether high power is applied; the controlword and internal events change the state, and the statusword reports it. It also warns that many optional functions limit interchangeability and that some vendors claim conformity while implementing only a subset [CiA CIA402].

**Statusword 6041h bits** [V AKD p95; TECHNO p56]: 0 ready to switch on, 1 switched on, 2 operation enabled, 3 fault, 4 voltage enabled, 5 quick stop (0 while a quick stop is active), 6 switch on disabled, 7 warning, 8 manufacturer-specific, 9 remote, 10 target reached, 11 internal limit active, 12-13 operation-mode-specific, 14-15 manufacturer-specific. Vendors reuse the free bits: AKD bit 8 = STO [V AKD p95]; Technosoft bit 7 = TML call warning, bit 8 = TML or homing running, bits 14-15 = event and axis on [V TECHNO p56]; N5 bit 15 = closed loop active [V N5 p53].

**State decoding** [V TECHNO p56; N5 p58; AKD p96; I PYCO profiles/p402.py SW_MASK]:

| State | Statusword pattern | Mask/value |
|---|---|---|
| Not ready to switch on | xxxx xxxx x0xx 0000 | 004Fh / 0000h |
| Switch on disabled | xxxx xxxx x1xx 0000 | 004Fh / 0040h |
| Ready to switch on | xxxx xxxx x01x 0001 | 006Fh / 0021h |
| Switched on | xxxx xxxx x01x 0011 | 006Fh / 0023h |
| Operation enabled | xxxx xxxx x01x 0111 | 006Fh / 0027h |
| Quick stop active | xxxx xxxx x00x 0111 | 006Fh / 0007h |
| Fault reaction active | xxxx xxxx x0xx 1111 | 004Fh / 000Fh |
| Fault | xxxx xxxx x0xx 1000 | 004Fh / 0008h |

EPOS4FW p15 shows bit 5 = 0 in four of these patterns (x00x 0000, x10x 0000, x00x 1111, x00x 1000); the 004Fh masks accept both forms (CP-D09).

**Controlword 6040h bits** [V TECHNO p55; AKD p94]: 0 switch on, 1 enable voltage (AKD calls it "Disable Voltage" but 1 still means enabled), 2 quick stop (0 = quick stop requested), 3 enable operation, 4-6 operation-mode-specific, 7 fault reset, 8 halt, 9-10 reserved in both sources, 11-15 manufacturer-specific (AKD: 11-12 reserved, 13-15 manufacturer). A CiA 402 meaning for bit 9 (later profile versions) is not established.

**Commands** (bits 7, 3, 2, 1, 0) [V TECHNO p55; N5 p57-58; AKD p94; EPOS4FW p16; I PYCO profiles/p402.py]:

| Command | Bits 7 3 2 1 0 | Typical word | Transitions |
|---|---|---|---|
| Shutdown | 0 x 1 1 0 | 0006h | 2, 6, 8 |
| Switch on | 0 0 1 1 1 | 0007h | 3 |
| Switch on + enable operation | 0 1 1 1 1 | 000Fh | 3 then 4 automatically (EPOS4FW only) |
| Disable voltage | 0 x x 0 x | 0000h | 7, 9, 10, 12 |
| Quick stop | 0 x 0 1 x | 0002h | 7, 10, 11 (TECHNO, AKD) or 11 only (N5, EPOS4FW) |
| Disable operation | 0 0 1 1 1 | 0007h | 5 |
| Enable operation | 0 1 1 1 1 | 000Fh | 4, 16 |
| Fault reset | 0 to 1 edge on bit 7 | 0080h after a word with bit 7 = 0 | 15 |

Sources disagree on whether bit 3 matters for Switch on and bit 7 for the other commands, and on the fault-reset transition numbers (CP-D07, CP-D08, CP-D10).

**Transitions** [V TECHNO p53-54; AKD p93; EPOS4FW p15-16]:

| # | From -> to | Trigger and action |
|---|---|---|
| 0 | start -> Not ready to switch on | power on or reset; automatic |
| 1 | Not ready -> Switch on disabled | initialization complete; automatic |
| 2 | Switch on disabled -> Ready to switch on | Shutdown |
| 3 | Ready -> Switched on | Switch on (power-stage meaning differs, CP-D11) |
| 4 | Switched on -> Operation enabled | Enable operation; may take time (Technosoft: 2 s on the first enable for phase alignment, p53) |
| 5 | Operation enabled -> Switched on | Disable operation; reaction per 605Ch |
| 6 | Switched on -> Ready | Shutdown |
| 7 | Ready -> Switch on disabled | Disable voltage or Quick stop |
| 8 | Operation enabled -> Ready | Shutdown; reaction per 605Bh |
| 9 | Operation enabled -> Switch on disabled | Disable voltage; vendors apply 605Ch (TECHNO) or 605Bh (EPOS4FW) (CP-D26) |
| 10 | Switched on -> Switch on disabled | Disable voltage or Quick stop |
| 11 | Operation enabled -> Quick stop active | Quick stop; reaction per 605Ah |
| 12 | Quick stop active -> Switch on disabled | quick stop finished or Disable voltage (TECHNO p54); "finished" applies to 605Ah codes that end in Switch on disabled (inferred from the code meanings) |
| 13 | any -> Fault reaction active | fault detected; reaction per 605Eh |
| 14 | Fault reaction active -> Fault | reaction finished; automatic |
| 15 | Fault -> Switch on disabled | Fault reset, only if no fault remains; the host must then clear bit 7 (AKD p93, TECHNO p54) |
| 16 | Quick stop active -> Operation enabled | Enable operation; only with 605Ah 5-8 (TECHNO p54); N5 needs a rising edge (p57) |

- Halt (bit 8) stops according to 605Dh and stays in Operation enabled [V TECHNO p64; N5 p58, p358].
- Bit 9 remote: with 0 the drive is in local control and ignores network commands [V TECHNO p56]. AKD sets it through FBUS.REMOTE, default 1 [V AKD p96].
- Mode changes: 6061h reports the active mode. Technosoft copies 6060h into 6061h only when Operation enabled is reached [V TECHNO p58]. AKD treats a mode as valid only once 6061h shows it and allows changes only at standstill [V AKD p98]. N5 allows a change at any time [V N5 p58] (CP-D24).
- A limit switch can stop the drive while the statusword still shows Operation enabled (Technosoft, p58) [V]. A profile must read the drive's documented limit behavior and must not infer it from the state.

## 13. Stop, fault and host-loss option codes

| Object | Value: action |
|---|---|
| 605Ah quick stop | 0 disable drive function (motor free); 1 slow-down ramp, then Switch on disabled; 2 quick-stop ramp 6085h, then Switch on disabled; 5 slow-down ramp, stay in Quick stop active; 6 quick-stop ramp, stay in Quick stop active. Default 2 [V TECHNO p62-63; N5 p356, p59; AKD p97] |
| 605Bh shutdown | 0 disable drive function; 1 slow-down ramp, then disable [V TECHNO p63; N5 p357] |
| 605Ch disable operation | 0 disable drive function; 1 slow-down ramp, then disable [V TECHNO p63; N5 p357] |
| 605Dh halt | 1 slow-down ramp; 2 quick-stop ramp; the drive stays in Operation enabled [V TECHNO p64; N5 p358] |
| 605Eh fault reaction | 0 disable drive (motor free); 1 slow-down ramp (N5) or reserved (Technosoft); 2 quick-stop ramp; Technosoft -1 = no action (manufacturer-specific) [V N5 p358-359; TECHNO p64] |
| 6007h abort connection | events: bus-off, heartbeat loss, life-guarding loss. 0 no action; 1 fault signal (then 605Eh); 2 disable voltage; 3 quick stop; negative = manufacturer-specific. Technosoft's default is 1 or 0 depending on firmware [V TECHNO p64-65] |

- The slow-down ramp is the mode's deceleration (for example 6084h; Technosoft uses 6083h) and the quick-stop ramp is 6085h [V TECHNO p62; N5 p356].
- Values 3, 4, 7 and 8 of 605Ah (and 3, 4 of 605Dh and 605Eh), the current-limit and voltage-limit stops, are not established by any stored source (CP-D12).
- Host-loss behavior combines 6007h, 1029h (section 3) and vendor faults (AKD F129). There is no uniform default. A profile must read and report these settings and must not assume the drive stops when the host goes silent.

## 14. Modes of operation

6060h modes of operation (INTEGER8, RW) and 6061h display (RO) [V TECHNO p57-58; AKD p98-99; I PYCO profiles/p402.py OperationMode]:

| Value | Mode | Value | Mode |
|---|---|---|---|
| < 0 | manufacturer-specific | 5 | reserved |
| 0 | no mode / reserved | 6 | homing (hm) |
| 1 | profile position (pp) | 7 | interpolated position (ip) |
| 2 | velocity (vl, frequency inverters) | 8 | cyclic synchronous position (csp) |
| 3 | profile velocity (pv) | 9 | cyclic synchronous velocity (csv) |
| 4 | profile torque (pt; AKD "tq") | 10 | cyclic synchronous torque (cst) |

CIA402 says CiA 402-2 v5.0.0 adds "some additional operation modes"; their names and numbers are not established. Negative values conflict between vendors: Technosoft -1 electronic gearing, -2 camming, -3 to -5 external reference; AKD -1 analog current, -2 analog velocity, -3 electronic gearing; python-canopen names -1 and -2 open-loop scalar and vector (CP-D14). A profile never interprets a negative mode without that device's evidence.

6502h supported drive modes bits: 0 pp, 1 vl, 2 pv, 3 tq/pt, 4 reserved, 5 hm, 6 ip, 7 csp, 8 csv, 9 cst, 10 cstca, 11-15 reserved, 16-31 manufacturer-specific [V AKD p83]. AKD's own default 00E5h omits modes that its 6060h accepts (CP-D16), so treat 6502h as a hint and confirm each mode.

Mode-specific bits:

| Mode | Controlword | Statusword | Main objects |
|---|---|---|---|
| pp | 4 new set-point (0 to 1 edge), 5 change set immediately, 6 relative (1) / absolute (0), 8 halt | 10 target reached, 12 set-point acknowledge, 13 following error | 607Ah target position, 6081h profile velocity, 6083h acceleration, 6084h deceleration, 6085h quick-stop deceleration |
| pv | 8 halt | 10 target velocity reached, 12 speed (zero indication), 13 maximum slippage error | 60FFh target velocity, 606Ch velocity actual |
| pt | 8 halt | 10 target reached | 6071h target torque, 6087h torque slope, 6077h torque actual |
| hm | 4 homing operation start (0 to 1 edge), 8 halt | 10 target reached, 12 homing attained, 13 homing error | 6098h, 6099h, 609Ah, 607Ch |
| ip | 4 enable interpolation | 12 ip mode active | 60C0h, 60C1h, 60C2h interpolation period, 60C4h |
| csp, csv, cst | 4-6 reserved | 12 drive follows the command value; 13 following error (csp) | 607Ah, 60FFh or 6071h written every cycle |

Sources: pp [V TECHNO p118-119; AKD p95-96], pv [V TECHNO p185], pt [V TECHNO p133; AKD p110-113], hm [V TECHNO p113-114; AKD p96], ip [V AKD p95-96, p117-120], cyclic modes [V AKD p95-96; TECHNO p151]. pp handshake: write 607Ah, set bit 4, wait for statusword bit 12 = 1, clear bit 4; the drive clears bit 12 when it can accept the next set-point. With bit 5 = 1 the new target replaces the running one [V TECHNO p118-119]. In homing, (bit 13, bit 12, bit 10) = 0,0,0 running; 0,0,1 interrupted or not started; 0,1,0 attained but target not reached; 0,1,1 completed; 1,0,0 error with motion; 1,0,1 error at standstill [V TECHNO p114]. Technosoft's csp bit 6 (relative) and pp bit 11 (additive) are vendor extensions [V TECHNO p118, p151].

## 15. Homing methods (6098h)

- 6098h is INTEGER8. 607Ch home offset; 6099h sub 1 fast speed (search for switch), sub 2 slow speed (search for zero); 609Ah homing acceleration. After homing the actual position is set to the home offset [V TECHNO p104, p114-115; AKD p122-125]. Technosoft says its method figures restate the CiA 402 figures [V TECHNO p104].
- Positive method numbers [V TECHNO p105-110; N5 p84]:

| Method | Reference |
|---|---|
| 1 | negative limit switch, then the first index pulse after leaving it (moving positive) |
| 2 | positive limit switch, then the first index pulse after leaving it (moving negative) |
| 3, 4 | positive home switch plus index; 3 after the switch's high-to-low edge, 4 after its low-to-high edge; the start direction depends on the switch state |
| 5, 6 | negative home switch plus index (mirror of 3, 4) |
| 7-10 | home switch with index; start positive, reverse at the positive limit; each method picks one edge and side |
| 11-14 | as 7-10 but start negative and reverse at the negative limit |
| 15, 16 | reserved |
| 17-30 | as 1-14 without the index pulse (17 matches 1, ..., 30 matches 14) |
| 31, 32 | reserved |
| 33, 34 | next index pulse, moving negative (33) or positive (34) |
| 35 | current position becomes home (Technosoft, N5); EPOS4FW uses 37 for this (CP-D13) |

- Negative methods are manufacturer-specific and conflict. Technosoft -1 to -4: mechanical limit, by current or step-loss detection [V TECHNO p110-113]. maxon -1 to -4: current threshold [V EPOS4FW p232]. N5 -1, -2, -7 to -14, -17, -18 and -23 to -30: homing on block, mirroring the positive numbers; closed loop only [V N5 p84]. AKD added -5 to -7 in revision R [V AKD p181].
- AKD maps the DS402 numbers it supports onto its own HOME.MODE and HOME.DIR settings and marks others (for example 9-11 and 13-14) unsupported [V AKD p123-124].
- Devices implement subsets. EPOS4FW lists 1, 2, 7, 11, 17, 18, 23, 27, 33, 34, 37 and -1 to -4 [V EPOS4FW p232]. A profile accepts only the methods its own device evidence lists and treats home, limit and index inputs as fixture facts.

## 16. Factor group and units

- Old DSP 402 factor group: notation and dimension indexes 6089h-608Eh, now declared obsolete and informative only; position factor 6093h (numerator/divisor); velocity encoder factor 6094h; acceleration factor 6097h; polarity 607Eh [V TECHNO p89-94].
- CiA 402-2 objects: 608Fh position encoder resolution = encoder increments (sub 1) / motor revolutions (sub 2); 6091h gear ratio = motor shaft revolutions (sub 1) / driving shaft revolutions (sub 2); 6092h feed constant = feed (sub 1) / driving shaft revolutions (sub 2) [V N5 p378-383; AKD p101-103]. 60A8h-60ABh SI unit codes for position, velocity, acceleration and jerk, coded as in CiA 303-2, which CiA 890 replaces [V TECHNO p94-95; CiA CIA890]. N5 describes the 60A8h layout: bits 16-23 unit, bits 24-31 decimal exponent [V N5 p64].
- Combining them, internal position = user position x (608Fh ratio) x (6091h ratio) / (6092h ratio). This is inferred from the object definitions; no stored standard text states it.
- Vendors scale differently by default: AKD uses 1/1000 rpm unless FBUS.PARAM05 bit 4 enables 204Ch [V AKD p99-100]; Technosoft uses internal units with 6093h, 6094h and 210Fh [V TECHNO p94]; Lichuan uses milliseconds for accelerations ([../README.md](../README.md)). Technosoft gives 6071h target torque in thousandths of the motor rated current 6075h [V TECHNO p133]; the standard reference for torque values is not established here. A profile states the exact native unit, scale and rounding of each object from that device's documentation.

## 17. Discrepancies

- **CP-D01** Inhibit time unit: IXXAT p13 says ms; TECHNO p30, p36 and CONODE CO_PDO.c:1027 use 100 us. Use 100 us unless device evidence differs.
- **CP-D02** IXXAT p8 gives the NMT confirmation (heartbeat) CAN-ID as 800h + node-ID; IXXAT p9 and POSTER give 700h + node-ID.
- **CP-D03** The IXXAT p11 SDO example (checked on the rendered page) shows the read request command byte as 43h (the 4-byte upload response code; a request is 40h, AKD p33), leaves the response command byte as "??", and encodes 2000 ms as D0 0F (4048) instead of D0 07.
- **CP-D04** IXXAT p16 connection-set table (rendered): RPDO_4 function code printed 1001b (should be 1010b); RSDO_1 range printed 601h-57Fh (POSTER: 601h-67Fh).
- **CP-D05** Reset-application range: CIANMT 2000h-9FFFh; POSTER 2000h-FFFFh.
- **CP-D06** POSTER object list (rendered) gives 1010h and 1011h object code VAR; TECHNO p48-49 and AKD p53-54 implement ARRAY with sub-indexes.
- **CP-D07** Switch on: TECHNO p55 and N5 p57 require bit 3 = 0 (0007h); AKD p94 and EPOS4FW p16 mark bit 3 don't-care. EPOS4FW defines 000Fh from Ready to switch on as Switch on followed automatically by Enable operation. Send 0007h, then 000Fh, unless device evidence allows otherwise.
- **CP-D08** Bit 7 in commands other than fault reset: TECHNO and N5 require 0; AKD marks it don't-care. Fault-reset transition: TECHNO p55 table says 13 but its figure on p53 says 15; EPOS4FW p16 says 14 and 15; AKD p94 and N5 p58 say 15. python-canopen writes 0080h as a level and does not generate the edge itself (profiles/p402.py).
- **CP-D09** State masks: EPOS4FW p15 requires bit 5 = 0 for Not ready, Switch on disabled, Fault reaction active and Fault; TECHNO p56, N5 p58, AKD p96 and PYCO treat bit 5 as don't-care.
- **CP-D10** Quick stop command: TECHNO p55 and AKD p94 list transitions 7, 10, 11; N5 p57 and EPOS4FW p16 list 11 only. Transition 16 needs a rising edge of bit 2 on N5 (p57); the others describe a level.
- **CP-D11** Switched on: TECHNO p54 says the power stage is switched off; AKD p92-93 says transition 3 enables the output stage and the drive has torque. Do not infer torque or brake state from Switched on.
- **CP-D12** 605Ah: TECHNO p63 lists 3, 4 and 7+ as reserved, yet TECHNO p54 makes transition 16 possible for 5, 6, 7 and 8. AKD p97 and N5 p356 list only 0, 1, 2, 5 and 6. 605Eh value 1 is a slow-down ramp on N5 (p358) and reserved on Technosoft (p64). The standard values 3, 4, 7 and 8 are not established.
- **CP-D13** Homing on the current position: method 35 (TECHNO p110, N5 p84) or 37 (EPOS4FW p232). N5 p84 also says both "17 to 32" and "17 to 30" are the index-free equivalents.
- **CP-D14** Negative homing methods and negative modes of operation mean different things at Technosoft, maxon, Nanotec, AKD and python-canopen (sections 14 and 15).
- **CP-D15** 1000h: AKD p46 shows bits 23-16 as the drive type and bits 31-24 as mode bits, while its text says "mode bits 28 to 31". CSSEDS gives only a 16-bit profile and 16-bit additional field.
- **CP-D16** AKD 6502h default 00E5h (pp, pv, hm, ip, csp) omits pt, csv and cst, which AKD 6060h accepts (p98). The 6060h value range includes 7, but the supported-mode table omits ip.
- **CP-D17** AKD p31 omits NMT 80h and describes 81h as a communication restart that resets all communication and mapping parameters. CiA (CIANMT, POSTER) defines 81h as reset application followed by reset communication.
- **CP-D18** The AKD p84 remapping example ends after writing sub 0 and never clears COB-ID bit 31 (CIAPDO step 5); its step 1 example writes C000_0181h.
- **CP-D19** Transmission type 00h: acyclic synchronous in IXXAT p13; reserved, meaning unsupported, in TECHNO p29.
- **CP-D20** SYNC counter: N5 p120 enables it only for 1019h > 1 and allows 02h-F0h (p180); CONODE_SYNC enables it for any non-zero value.
- **CP-D21** Technosoft sends a 4-byte high-resolution time stamp on 100h, the default TIME identifier, which POSTER defines as a 6-byte frame (TECHNO p45, p50-51). Technosoft also says setting 1014h bits 0-10 to 0 disables EMCY (p52); the validity flag is bit 31.
- **CP-D22** AKD p95 lists 6041h as R/W and stored in EEPROM; TECHNO p56 lists it as RO.
- **CP-D23** AKD p102 describes 6091h in words as feed per driving-shaft revolution (the 6092h meaning), while its formula is motor revolutions / driving-shaft revolutions.
- **CP-D24** Mode change timing differs: TECHNO p58 updates 6061h only in Operation enabled; N5 p58 allows changes at any time; AKD p98 requires standstill.
- **CP-D25** PYCODOC says CiA 402 state changes need NMT Operational, because its state machine relies on PDOs. No stored CiA source restricts controlword writes by SDO in Pre-operational (CIANMT allows SDO there). Treat this as a library constraint.
- **CP-D26** Transition 9 reaction: TECHNO p54 applies 605Ch; EPOS4FW p15 applies the shutdown option code 605Bh.
- **CP-D27** Statusword bits 7, 8, 14 and 15 carry vendor meanings (AKD bit 8 STO; Technosoft bits 7, 8, 14, 15; N5 bit 15). A profile must not interpret them generically.
- **CP-D28** AKD p35 describes 0504_0002h as "SDO segmented: invalid blocksize"; POSTER says invalid block size (block mode only). AKD also lists 0504_0004h as "invalid block CRC" in segmented wording.

## 18. Best source for each topic

| Topic | Best stored source | Corroboration |
|---|---|---|
| OD ranges, 1000h-1029h list | POSTER p1 | CIAARCH; IXXAT p4 |
| Identity 1018h, LSS address | CIA305 | TECHNO p23; AKD p57-58 |
| Store/restore 1010h/1011h | TECHNO p48-49 | IXXAT p16; AKD p53-54 |
| Pre-defined CAN-IDs, COB-ID bits | POSTER p1 | IXXAT p15-16 (CP-D04); TECHNO p31 |
| NMT states and commands, boot-up | POSTER p1; CIANMT; CIAEC | TECHNO p43-45; IXXAT p7-8 |
| 1029h error behavior | N5 p181-182 | none |
| Heartbeat producer/consumer | POSTER p1; AKD p56-57 | IXXAT p9; TECHNO p45, p52 |
| Node/life guarding | AKD p38; TECHNO p50 | CIAEC; PYCO nmt.py |
| SDO expedited/segmented frames | AKD p33-35; POSTER p1 | CSS; PYCO sdo/ |
| SDO block transfer | CSS Examples 3-4 | CIASDO; PYCO sdo/client.py |
| SDO abort codes | POSTER p1 | TECHNO p29; AKD p35 |
| PDO records, transmission types | POSTER p1; IXXAT p13 | TECHNO p29-31 |
| PDO mapping and its change procedure | CIAPDO | TECHNO p40; CONODE CO_PDO.c |
| SYNC, 1005h-1007h, 1019h counter | CIASFP; TECHNO p48; N5 p120, p180 | CONODE_SYNC; IXXAT p12 |
| EMCY frame, error codes, error register | POSTER p1; IXXAT p14-15 | AKD p32; TECHNO p46-47 |
| LSS services and codes | TECHNO p23-27; CIA305 | CONODE_LSS |
| EDS/DCF | CSSEDS; CIA306 | IXXAT p7 |
| CiA 402 state machine, bit patterns | TECHNO p53-56; EPOS4FW p15-16 | N5 p56-58; AKD p92-96; PYCO p402.py |
| Quick stop, halt, fault option codes, 6007h | TECHNO p62-65; N5 p356-359 | AKD p97 |
| Modes of operation, 6502h | TECHNO p57-58; AKD p83, p98 | PYCO p402.py |
| Mode-specific bits | TECHNO p113-119, p151, p185; AKD p95-96 | none |
| Homing methods and figures | TECHNO p104-117 | N5 p84; EPOS4FW p232 |
| Factor group, SI units | N5 p378-383; TECHNO p89-95 | AKD p99-103; CIA890 |

## 19. What a profile can rely on

Apart from the errata listed above, the CiA and vendor sources here agree on the following, so a shared CANopen layer can implement them: the pre-defined identifiers and 32-bit COB-ID flag layout; NMT command values and heartbeat state bytes; expedited and segmented SDO framing, the abort-code table and little-endian data; PDO records, mapping-entry format and the invalidate-remap-validate order; the EMCY frame layout; and the core CiA 402 bits (controlword 0-3, 7, 8; statusword 0-6, 10-13) with the 004Fh/006Fh state masks.

Each device profile must establish the rest from its own manual and EDS: the Switch on bit 3 and fault-reset edge handling, quick-stop transition and option-code support, power-stage and brake state per FSA state, mode list and mode-change rules, homing method subset and the meaning of any negative value, native units and factor objects, store/restore coverage, SDO block support, transmission type 00h, the SYNC counter range, and host-loss reactions (1029h, 6007h, guarding and vendor faults).
