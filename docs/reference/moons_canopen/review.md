# Applied Motion Products / MOONS' CANopen drives: evidence review

Family code MO. Reviewed 2026-11-19 from the originals listed in [SOURCES.json](SOURCES.json). Every statement
below is document scope: it says what a manufacturer document or EDS file claims. Nothing here is a hardware
observation, and no drive, firmware or configuration tool was contacted or run.

Citation form: `UM21 p58` means source UM21, one-based PDF page 58. EDS citations name the section, for example
`EDS-STF [1029sub1]`. Source IDs:

| ID | Document |
|---|---|
| UM21 | MOONS' CANopen User Manual, Rev 1.0, 8/16/2021 (the newest CANopen manual; STF-C and SSDC-C) |
| AMP10 | Applied Motion CANopen Manual 920-0025 Rev F, 7/27/2010 (the 2010 command reference; ST/STM/STAC6/SV generation) |
| STFHW | MOONS' STF-R/C Hardware Manual, Rev 1.0, 10/13/2025 |
| STFHWC | Applied Motion STF-R/C/D/IP Hardware Manual 920-0141 Rev C, 3/27/2025 |
| SSDCHW | MOONS' SSDC-R/C Hardware Manual, Rev 1.1, 08/20/2025 |
| HCR | MOONS' Host Command Reference 920-0002 Rev J, 12/2014 (SCL and Q commands) |
| TSM24, TXM24, TSM34, TXM34 | MOONS' hardware manuals for TSM24C, TXM24C, TSM34Q/C, TXM34Q/C (2013-2016) |
| EDS-STF | CANOPEN-EDS-STEPPER-STF-V310A.eds (06-02-2020) |
| EDS-SSDC | CANOPEN-EDS-STEPPER-SSDC-V310B.eds (06-01-2020) |
| EDS-TSM | CAN-EDS-212N-SSM-TSM-SS-TXM.eds (05-29-2020) |
| PG-... | MOONS' product page snapshots (2026-11-19) |

Where a fact comes only from the 2021 manual or only from the 2010 reference, the text says so. The two documents
describe different object layouts and unit systems; they must not be merged into one profile.

## 1. Scope and exact applicability

### 1.1 Families and what each document covers

| Family | CANopen models found | CANopen document | EDS | Motor/feedback |
|---|---|---|---|---|
| STF-C | STF03-C, STF05-C, STF06-C, STF10-C; STF05-C-FC, STF10-C-FC (UL type) | UM21 | EDS-STF | Open-loop two-phase stepper (STFHW p3, p23) |
| STF03-C-mini | STF03-C-mini | UM21 names it (firmware 1.02A+) | none linked | Open-loop stepper; product page lists RS-232 only (PG-STF03MINI) |
| SSDC-C | SSDC03-C, SSDC06-C, SSDC10-C and -FC variants | UM21 | EDS-SSDC | Closed-loop "Step-Servo" stepper with encoder (SSDCHW p3) |
| TSM-C / TXM-C | TSM17C, TSM23C, TSM24C, TSM34C, TXM24C, TXM34C (integrated step-servo motors) | none current; EDS-TSM follows the AMP10 layout | EDS-TSM | Integrated closed-loop stepper, 20000 counts/rev encoder (TSM24 p3, TSM34 p4) |
| MDX+ | MDXR/MDXT ...RC... variants | separate MDX CANopen manual (EN20251225) | separate EDS (MDXPLUS-Servo V1.0.1) | Integrated servo; **excluded** (PG-MDX) |
| M56S | M56S-...CN/CX | separate M56S CANopen manual (EN20251225) | separate EDS (V1.0.2 zip) | AC servo; **excluded** (PG-M56S) |

- UM21 p6 states the manual covers stepper drives STF03-C, STF05-C, STF06-C, STF10-C with firmware "310A and
  later", STF03-C-mini with firmware "1.02A and later", and StepSERVO drives SSDC03-C, SSDC05-C, SSDC06-C,
  SSDC10-C with firmware "310B and later". The EDS file names carry the same codes (V310A, V310B). SSDC05-C is
  named in UM21 p6 but SSDCHW p35 lists only SSDC03/06/10.
- The STF-C and SSDC-C product pages link UM21 and the matching EDS (PG-STF05, PG-STF10, PG-SSDC06). The -FC
  pages link neither UM21 nor an EDS; they link the hardware manual or a datasheet, the brochure, Stepper Suite and a
  UL certificate (PG-STF05FC, PG-SSDC06FC). STFHW p30 defines FC as "UL type". Treat -FC as the same CANopen product
  only with device evidence.
- AMP10 p3 and p9 describe the 2010 generation (ST5/10-C, STM-C, STAC6-C, SV7-C). It does not name STF, SSDC, TSM
  or TXM. Its object layout (vendor objects 0x7001-0x700F, user registers 0x4000-0x4016, internal units) matches
  EDS-TSM, not UM21. A Rev K (2015) that reportedly names TSM17C/23C/24C and TXM24C was not obtained (remote_only
  AMP10K).
- MDX+ and M56S use their own CANopen manuals and EDS files (PG-MDX, PG-M56S). Following the task rule, they are
  outside this pack. No SV200 CANopen document was found on a manufacturer host.
- Suffix: -C means CANopen for STF and SSDC (STFHW p30; SSDCHW p3). For TSM/TXM the CANopen variants are the
  "C-xCG" part numbers, for example TSM34C-xCG next to TSM34Q-xRG (RS-485) and TSM34x-xDG (Ethernet) (TSM34 p16;
  TSM24 p2-p3). The -R (RS-485/Modbus), -D, -IP, -ECX and -PN variants are different products.

### 1.2 Hardware and firmware revisions

- No firmware release notes were found on any MOONS product page. applied-motion.com could not be read statically
  (remote_only AMPSITE). Firmware identification is therefore limited to UM21 p6 and to the strings a drive
  reports.
- Firmware identity objects: 0x100A manufacturer software version (UM21 p44), 0x2031 DSP version STRING(4)
  (UM21 p68), 0x2032 FPGA version and 0x2033 ENC version (EDS-SSDC only, not in UM21), 0x2200 FWName (EDS-TSM only).
  EDS defaults are 100A = "300F" (EDS-STF) and "300E" (EDS-SSDC), which do not match the "310A"/"310B" lines of
  UM21 p6 (MO-D01). An EDS default is not the value a drive reports.
- Hardware revision: 0x1009 defaults "A001" (EDS-STF, EDS-SSDC) and "3.00" (EDS-TSM).

## 2. Sources and retrieval failures

All stored sources were fetched with an ordinary browser User-Agent; the MOONS downloads used the product page as
Referer. Every stored file returned HTTP 200. The 2026-11-19 market survey had recorded HTTP 403 for UM21 and the
EDS; that was not reproduced with a normal request.

Retrieval failures and sources not stored (all recorded in `remote_only`):

| ID | What | Status |
|---|---|---|
| AMP10K | 920-0025 Rev K (3/5/2015), guessed URL on the Applied Motion bucket | HTTP 403 AccessDenied; **blocked: needs manual browser download by the owner**. The 403 may also mean the object does not exist. Known only from a third-party mirror, not used. |
| AMPSITE | applied-motion.com product, manual and firmware listings | HTTP 200 but JavaScript shell only; no document list. Needs a browser session. |
| STFHWB | 920-0141 Rev B (2024) | Downloaded and reviewed, not stored: superseded by Rev C with no CANopen change. |
| TSM17, TSM23 | TSM17C and TSM23C hardware manuals | Downloaded and reviewed, not stored: same CANopen setup text as TSM24. |
| MDXUM, MDXEDS, M56SUM | MDX+ and M56S CANopen manuals and EDS | Not downloaded: out of scope (separate manuals). |

Not downloaded by design: Stepper Suite, STF Configurator, Step-Servo Quick Tuner, CANopen Test Tool, RS-485 Bus
Utility (installers and archives), EPLAN .edz files, CAD, brochures and certificates. Newer Host Command Reference
revisions (for example a 2019 Rev T) and the AMP10 Rev K are visible only on third-party mirrors and were not used.

## 3. Interface and protocol

### 3.1 Physical layer, bit rate and node ID

- Connectors: STF-C and SSDC-C have two RJ45 ports wired CAN_H pin 1, CAN_L pin 2, GND pins 3, 7, 8; COM1 pins 4/5
  carry RS-232 TX/RX for configuration, COM2 pins 4/5 are not connected (UM21 p8; STFHW p11; SSDCHW p10). The CAN
  port is isolated and its GND must be connected between drives and to the controller (STFHW p11; SSDCHW p10).
  Daisy chain with 120 ohm termination at each end (UM21 p8). SSDC-C has a switchable terminator (S2 SW4,
  SSDCHW p12); STF-C has none documented. AMP10 p11 describes a four-pin spring connector for ST5-C.
- Bit rates: 1000, 800, 500, 250, 125, 50, 20 and 12.5 kbit/s, selected by code 0-7 (UM21 p9; STFHW p13; SSDCHW
  p12; TSM24 p12; AMP10 p12; HCR p52 CB command). Codes 8 and 9 are reserved (UM21 p9; STFHW p13). All three EDS
  files declare 10 kbit/s and no 12.5 kbit/s (MO-D17).
- Bit-rate selection per family: STF-C rotary switch S2 (STFHW p13); SSDC-C DIP switch S2 SW1-SW3 (SSDCHW p12);
  TSM24C ten-position switch (TSM24 p12); TXM24C, TSM34C and TXM34C only through Step-Servo Quick Tuner (TXM24
  p12; TSM34 p19; TXM34 p20); object 0x2021 Bit rate (UM21 p68, value coding not given); EDS-TSM has 0x2102 CAN bit
  rate (default 250) and 0x5016 BitRate.
- Node ID: valid 0x01-0x7F; 0 reserved (UM21 p9; STFHW p13). On STF-C, SSDC-C and TSM24C a 16-position switch
  sets the lower four bits and software sets the upper three bits (UM21 p9 says Stepper Suite; STFHW p13 says STF
  Configurator; SSDCHW p12 and TSM24 p12 say Step-Servo Quick Tuner). Object 0x2020 sets the upper bits as values
  0-7 for ranges 0x00-0x0F ... 0x70-0x7F (UM21 p68). TXM24C, TSM34C and TXM34C set the whole node ID in Quick
  Tuner (TXM24 p12; TSM34 p19; TXM34 p20). AMP10 p12 describes a 7-position switch for the upper bits on its
  drives. EDS-TSM has 0x2101 CAN node ID (default 0x30) and 0x5015 NodeIDorUpper. 0x2070 Switch value reports the
  rotary switch settings in its low 16 bits (UM21 p71).
- When changes take effect: UM21 p9 and AMP10 p12 say the node ID and bit rate are captured only after a power
  cycle or a network reset command. UM21 p68 says 0x2020/0x2021 take effect at the next power cycle after storing
  with 0x1010. STFHW p13 and SSDCHW p12 say a power cycle is required. TSM24 p12, TXM24 p12 and TSM34 p19 say a
  power cycle or a CANopen reset is required for the bit rate (MO-D16).
- RS-232 interaction at power-up: the drive sends a power-up packet on RS-232. If a MOONS' application answers,
  the drive holds the CAN node in Initialization until the application is closed; otherwise it boots, sends the
  boot-up message and enters Pre-operational (UM21 p9; AMP10 p12, p48). A profile must expect a node that never
  boots while a configuration tool is attached.
- LSS is not supported (`LSS_Supported=0` in all three EDS files).

### 3.2 NMT, heartbeat, node guarding

- NMT commands start (1), stop (2), enter pre-operational (0x80), reset node (0x81), reset communication (0x82);
  heartbeat states 0 boot-up, 4 stopped, 5 operational, 0x7F pre-operational (UM21 p13; AMP10 p50).
- The node enters Pre-operational automatically after initialization (UM21 p14). SDO, SYNC, TIME and EMCY are
  available in Pre-operational and Operational, PDO only in Operational (UM21 p14; AMP10 p51).
- Heartbeat producer 0x1017, default 1000 ms (UM21 p48; EDS-STF/SSDC [1017] 0x3E8; EDS-TSM 1000). No heartbeat
  consumer object 0x1016 exists in any EDS or in UM21 p40, although UM21 p49 mentions heartbeat consumer errors in
  0x1029.
- Node guarding: AMP10 p15 lists 0x100C guard time and 0x100D life time factor, but no EDS (including EDS-TSM)
  contains them; UM21 does not list them (MO-D12). Treat node guarding as unsupported.
- Error behaviour 0x1029 (sub 1 communication error, 2 other, 3 passive, 4 generic, 5 device profile, 6
  manufacturer; values 0 = go to pre-operational if operational, 1 = no change, 2 = go to stopped) (UM21 p49).
  UM21 p49 gives all defaults as 1 and access RO; the EDS files give rw with sub 1 = 0 (go to pre-operational on
  bus-off or heartbeat-consumer error), sub 3 = 1 and others 0 (MO-D11). No document says what a transition to
  Pre-operational does to an active motion.

### 3.3 SDO

- One server SDO: 0x600+node ID receive, 0x580+node ID transmit, both read-only (UM21 p49; all EDS [1200]).
- UM21 p11 documents only expedited command specifiers (write 0x2F/0x2B/0x27/0x23, response 0x60; read 0x40,
  responses 0x4F/0x4B/0x47/0x43). Segmented or block transfer is not documented, although string objects such as
  0x1008 "AMA CANopen Motor Driver" (24 characters) need segmented upload (MO-D27). The only abort code named is
  0x06090030 (UM21 p42, p43, p47).

### 3.4 PDO resources and mapping

- Four RPDOs and four TPDOs (`NrOfRXPDO=4`, `NrOfTXPDO=4` in all EDS; UM21 p40). Each mapping record has eight
  entries (UM21 p52, p54; EDS [1600]-[1A03]). Granularity 8; no dynamic channels or MPDO (EDS DeviceInfo).
- Default COB-IDs 0x180/0x280/0x380/0x480 + node ID (TPDO) and 0x200/0x300/0x400/0x500 + node ID (RPDO) (UM21 p50,
  p53).
- Default mappings in EDS-STF and EDS-SSDC: RPDO1 0x6040; RPDO2 0x6040 + 0x607A; RPDO3 0x6040 + 0x60FF; RPDO4
  0x60FE sub 1 (32 bit); TPDO1 0x6041; TPDO2 0x6041 + 0x6064; TPDO3 0x6041 + 0x606C; TPDO4 0x60FD. EDS-TSM differs:
  TPDO2 0x6041 + 0x700A, TPDO3 0x6041 + 0x7009 (16 bit), TPDO4 0x7003 (16 bit). AMP10 p52 shows the 2010 default
  maps with 16-bit and 8-bit entries. UM21 p52/p54 show only a one-entry default for every PDO (MO-D14).
- Remapping procedure: invalidate the PDO (COB-ID bit 31), set sub 0 to 0, write entries, set sub 0, validate;
  allowed in Pre-operational and, "if supported", Operational (UM21 p52, p54). UM21 p90 shows remapping TPDO2 in
  Pre-operational followed by "save all parameters"; that example maps 0x700A, which is not in the STF/SSDC object
  dictionary (MO-D14).
- TPDO transmission types: 0 synchronous acyclic, 1-240 every n-th SYNC, 0xFC/0xFD RTR-only not supported, 0xFE
  and 0xFF event-driven (UM21 p53). RPDO types 0-240 synchronous, 0xFE/0xFF event (UM21 p51). EDS defaults: RPDO1
  0xFF, RPDO2-4 0xFE; all TPDO 0xFF. AMP10 p51-52 instead describes remote-request triggering and says types 0 and
  254/255 react to statusword changes (MO-D13).
- Inhibit time in 100 us units, TPDO1 default 100 (10 ms), others 0; event timer in ms, default 0 (UM21 p53; EDS).
- Change-of-state masks: 0x2040-0x2043 sub 1/2 mask the lower and upper four bytes of TPDO1-4 for change detection,
  default 0xFFFFFFFF (UM21 p69). EDS-TSM uses 0x2014-0x2017 for the same purpose.
- The COS column in UM21 marks which objects trigger an event TPDO on change (for example 0x6041, 0x6061, 0x6064,
  0x606C, 0x603F, 0x200B; UM21 p56-p67).

### 3.5 SYNC, EMCY, TIME

- SYNC COB-ID 0x1005 default 0x80; 0x1006 communication cycle period and 0x1007 window "in ms" (UM21 p43-p44; the
  generic CiA 301 unit is microseconds, MO-D10). Whether the drive can produce SYNC is not stated; an attempt to set
  bit 30 on a device that cannot is answered with abort 0x06090030 (UM21 p43). 0x1019 counter overflow (UM21 p48).
- SYNC has a vendor side effect: in SYNC Q mode every received SYNC starts the selected Q segment (AMP10 p48-p49;
  UM21 p36). See section 5.6.
- EMCY COB-ID 0x1014 = 0x80 + node ID, read-only (UM21 p47). EMCY inhibit 0x1015 exists only in EDS-TSM (default
  200; the unit is not stated, 20 ms if the generic CiA 301 unit of 100 us applies). Error history 0x1003 with eight entries, low 16 bits = error code, high 16 bits
  manufacturer-specific (UM21 p42). Neither UM21 nor AMP10 documents the EMCY error codes or payload (MO-D19).
- TIME is listed as available in UM21 p14, but there is no 0x1012 object (MO-D26).

## 4. Identity

- 0x1000 device type: 0x00060192 in all three EDS files (low 16 bits 0x0192 = profile 402). UM21 p41 does not give
  the value. The same value is used for the stepper and step-servo families, so it cannot identify the family.
- 0x1018: vendor ID 0x000002D9 "Shanghai AMP & Moons' Automation" (UM21 p48; all EDS). Product code and revision
  number are manufacturer-specific (UM21 p48) and are 0 in all EDS files. The serial number is a lot number
  (UM21 p48). The revision number is described as major (behaviour) and minor fields (UM21 p48).
- 0x1008 "AMA CANopen Motor Driver" in all EDS files and UM21 p44.
- 0x2050 Product series STRING(4): "STF-" (EDS-STF) or "SSDC" (EDS-SSDC) (UM21 p69). EDS-TSM has no 0x2050.
  0x2051 customer name (UM21 p69).
- Family discrimination must use 0x2050, 0x100A/0x2031 and the vendor ID together, and must not assume one object
  dictionary: index 0x2100 is "User registers" (array of 23 INT32) in EDS-STF/SSDC but "Error status bits"
  (10-byte octet string) in EDS-TSM.

## 5. Drive state machine and modes

### 5.1 State machine against CiA 402

- UM21 p15-p16 reproduces the CiA 402 power drive system state machine, transitions 0-16, the command codes
  (shutdown 0x06, switch on 0x07, switch on + enable operation 0x0F, quick stop, disable operation 0x07, enable
  operation 0x0F, fault reset = rising edge of bit 7) and the statusword state coding (UM21 p16, p57).
- Statusword bits: 0 ready to switch on, 1 switched on, 2 operation enabled, 3 fault, 4 voltage enabled, 5 quick
  stop, 6 switch on disabled, 7 warning, 8 reserved, 9 remote, 10 target reached, 11 internal limit active,
  12-13 mode specific, 14-15 reserved (UM21 p37, p57). Controlword bits 9, 6, 5, 4 are mode specific, 8 halt,
  7 fault reset (UM21 p37, p56).
- Access: 0x6040 and 0x6060 are write-only (UM21 p55; EDS). Read the mode from 0x6061 (UM21 p59; AMP10 p21).
- Enable sequence in every example: 0x06, 0x07, 0x0F (UM21 p19, p88). For velocity, torque and Q modes the
  examples enable with halt set (0x010F) and clear halt to start (UM21 p32, p34, p35; AMP10 p26). AMP10 p21 writes
  0x001F directly to enable and start a profile position set-point.
- Fault reset: rising edge of controlword bit 7; "a reset of the fault condition is carried out, if no fault exists
  currently" (UM21 p16). Vendor alternatives: write 1 to 0x2006 DSP clear alarm, which clears 0x603F and 0x200F
  (UM21 p66; EDS-TSM 0x7006), and the X6 alarm-reset input configured by 0x2609/0x220E (UM21 p75, p83).
- Quick stop: 0x605A supports only option codes 1 and 2 "at this moment" (UM21 p58); AMP10 p16 says "2 & 6 only".
  The EDS default is 0 (disable drive function), which UM21 does not list as supported (MO-D05). 0x6085 is used
  with option code 2 (UM21 p62).
- Halt: controlword bit 8. The halt option code 0x605D is referenced (UM21 p22, p31, p33) but exists in no OD list
  or EDS (MO-D06). In profile position mode halt stops the axis in position control and the set-point continues
  after halt is released (UM21 p21). In profile velocity mode halt decelerates with the profile deceleration and
  keeps torque applied (AMP10 p26). In profile torque mode halt ramps the torque to zero with the torque slope, which
  leaves the shaft free while the drive stays "Operation enabled" (AMP10 p45).
- AMP10 p26 says that to let the shaft move freely "the NMT state must be put in to the 'Drive Disabled' state";
  this mixes NMT and CiA 402 terms (MO-D28).
- Interlock: 0x60FD bit 3 reports an interlock input; if it becomes inactive the drive "shall enter the switch on
  disabled or fault reaction active state" (UM21 p63). The physical input used is not stated.
- Enable input: X5 is the default enable input; 0x2617/0x220D select open-active, closed-active or general purpose
  (UM21 p75, p87; STFHW p18). UM21 gives the default 3 (general purpose); the EDS default is 0 (MO-D34). A
  configured enable input can block or remove enabling independently of the controlword.
- Faults that disable the motor: AMP10 p20 marks over temperature, internal voltage/excess regen, over voltage,
  under voltage (STAC6), over current, bad hall sensor / open winding and bad encoder as drive faults that
  automatically disable the motor. UM21 p56 lists the 0x603F bits without that distinction.
- Brake: brake output function and delays in 0x2607 (stepper) and 0x220C (step-servo): sub 1 output mode (1 closed
  when enabled, 2 open when enabled, 3 general purpose), sub 2 move-command waiting time after brake release,
  sub 3 servo-off waiting time after brake engagement, in ms (UM21 p75, p83). STF uses Y2 as brake output
  (STFHW p19). The relation of these delays to the state-machine transitions is not described. EDS-TSM uses
  0x501C/0x501D/0x5022.

### 5.2 Modes of operation per family

| 0x6060 value | Mode | STF-C (UM21) | SSDC-C (UM21) | 2010 ST/STM/STAC6 (AMP10) | 2010 SV (AMP10) | TSM/TXM-C (hardware manuals) |
|---|---|---|---|---|---|---|
| 1 | Profile position | yes | yes | yes | yes | TSM24 p3, TXM24 p3, TXM34 p4 |
| 3 | Profile velocity | yes | yes | yes | yes | TSM24 p3, TSM34 p4, TXM34 p4 |
| 4 | Profile torque | no | yes | no | yes | TSM24 p3, TSM34 p4, TXM34 p4 |
| 6 | Homing | yes | yes | yes | yes | TSM24 p3, TXM24 p3 |
| -1 (0xFF) | Normal Q program (vendor) | yes | yes | yes | yes | "Execute stored Q programs" (TSM24 p3) |
| -2 (0xFE) | SYNC Q program (vendor) | yes | yes | yes | yes | not stated |

Sources: UM21 p58 (values and "Torque profile Mode is only supported for StepSERVO drive"), UM21 p64 (0x6502 bits
0, 2, 3 step-servo only, 5), AMP10 p13. Velocity mode (VL) and interpolated position are explicitly not supported
(UM21 p58; AMP10 p13). Cyclic synchronous modes are not listed anywhere. 0x6502 defaults to 0 in all EDS files.
TSM34 p4 lists only torque and velocity for the TSM34 CANopen mode (MO-D29).

### 5.3 Profile position (mode 1)

- Objects 0x607A target position, 0x6081 profile velocity, 0x6083/0x6084 acceleration/deceleration, 0x6085 quick
  stop deceleration, 0x607F max profile velocity, 0x607E polarity (UM21 p17, p55, p61).
- Controlword bit 4 new set-point (rising edge), bit 5 change set immediately, bit 6 relative, bit 9 change on
  set-point; statusword bit 12 set-point acknowledge, bit 10 target reached, bit 13 following error (UM21 p17-p18;
  AMP10 p21).
- Buffering: "MOONS' CANopen drive can be set up with two set-points; if bit 12 of statusword is 1, then the buffer
  is full and another set-point will be ignored" (UM21 p21; AMP10 p21). A set-point written while the buffer is
  full is ignored without an error; a profile must check bit 12 before toggling bit 4.
- Bit 9 = 1 continues at the old set-point speed to the old target and then changes speed (UM21 p17; AMP10 p21,
  p24). Bit 5 = 1 applies the new set-point immediately (AMP10 p25).

### 5.4 Profile velocity (mode 3)

- 0x60FF target velocity INT32 (UM21 p31 lists UINT32, p55 and p64 list INT32), 0x6083/0x6084 (UM21 p31).
- Start/stop by clearing/setting halt (UM21 p32; AMP10 p26). A target of 0 has the same effect as halt; both keep
  torque applied (AMP10 p26).
- Statusword bit 10 target reached, bit 12 speed (1 = speed is 0), bit 13 max slippage error (UM21 p31, checked on
  the rendered page).

### 5.5 Homing (mode 6)

- Objects: 0x6098 method INT8, 0x6099 sub 1 switch-search speed and sub 2 zero-search speed, 0x609A homing
  acceleration, 0x607C home offset, 0x2001 home switch input X1-X8 (default 3), 0x2009 homing offset mode
  (UM21 p22, p62, p65-p66). EDS-STF/SSDC also have 0x2008 "Homing inner offset" INT16 default 4000, which UM21 does
  not describe. AMP10 uses 0x7001 for the home switch (range 1-6) (AMP10 p17, p28).
- Start: controlword bit 4 rising edge; halt (bit 8) stops homing (UM21 p22-p23; example 0x001F start, 0x011F
  stop). Status: bits 13/12/10 per the CiA table (UM21 p22).
- Methods documented in UM21: 1-14, 17-30, 33, 34, 35, 37; 15/16 reserved (UM21 p27); 36 reserved (UM21 p30);
  31/32 are not described in UM21 and are reserved in AMP10 p42. AMP10 has 1-14, 17-30, 33-35 and no 37 (AMP10 p4-p5,
  p28-p44). Method 35 takes the current position as home; AMP10 p44 says the home offset is ignored and the motor does
  not move. Method 37 sets 0x6064 = 0x607C without requiring Operation enabled (UM21 p30).
- 0x2009 = 0: the motor stops at the machine home and the current position becomes the home offset; 0x2009 = 1: the
  motor moves by the home offset after finding home and that point becomes zero (UM21 p66).
- Homing uses the logical limit and home switch values of 0x60FD (UM21 p23). AMP10 p28 requires the limit-switch
  definitions to be set in the configurator before CANopen homing.
- Direction wording differs: AMP10 p28 says method 1 "homes to the first index CCW after the CW limit switch is
  reached" and p43 says method 33 homes "to the next index pulse CW"; UM21 p23 says method 1 starts leftward with
  the CCW limit and p30 says 33 is negative. The mapping between CW/CCW, negative/positive, 0x607E polarity and the
  direction-reverse settings 0x2208/0x260F sub 1 is not stated (MO-D07).
- Hard-stop homing: 0x2202 "Torque limit of hard stop homing" exists for step-servo (UM21 p72-p73), but no homing
  method number for hard-stop homing is documented (MO-D07). UM21 p62 lists "128 manufacturer specific" for the INT8
  method object.

### 5.6 Q program mode and SCL (vendor-specific)

- Purpose: Q programs run complex sequences outside CiA 402; they must be written and loaded with Q Programmer
  over RS-232 (UM21 p35; AMP10 p48, "v1.3.5 or later"). Loading requires the RS-232 tool, during which the CAN node
  is held in Initialization (AMP10 p48; UM21 p9).
- Normal Q (mode -1): write 0x6060 = -1, verify 0x6061, write the segment number to 0x2007 (UM21 p35, p66) or 0x7007
  (AMP10 p48, "1-12"), enable, then a rising edge of controlword bit 4 runs the segment to completion. Setting halt
  stops the program immediately, and the next start runs it from the beginning (AMP10 p48). UM21 p89 shows start
  0x001F and halt 0x011F.
- SYNC Q (mode -2): write 0x6060 = -2, select the segment, set the SYNC COB-ID in 0x1005; the drive then executes
  the segment every time a SYNC arrives, so several drives start together (AMP10 p48-p49; UM21 p36, p90). Halt
  stops it; the next SYNC after halt is cleared restarts from the beginning (AMP10 p49). This makes an ordinary
  SYNC used for PDO timing a motion trigger for any drive left in mode -2.
- Data exchange: 23 volatile 32-bit general-purpose registers shared with Q programs: 0x2100 sub 1-23 (UM21 p71;
  INT32 in EDS-STF/SSDC) or 0x4000-0x4016 mapped to Q register characters '0'-'9', ':', ';', '<', '=', '>', '?',
  '@', '[', '\', ']', '^', '_', '`' (AMP10 p18; EDS-TSM 0x4000 subs). UM21 gives no register-character mapping for
  0x2100 (MO-D20 related gap).
- Monitoring: Q-mode statusword bits 12/13 are reserved (UM21 p37); 0x200B bit 14 "Q Program is running", bit 7
  "Waiting for an input", bit 11 "executing a WT command" (UM21 p67; AMP10 p20 for 0x700B). 0x603F bit 14 "Blank Q
  Segment" (UM21 p56).
- Watchdog actions 2..n of 0x2060 sub 5 execute Q segments on communication loss (UM21 p38, p70).
- SCL: neither UM21 nor AMP10 documents a CANopen object for sending SCL commands. SCL is a serial protocol
  (HCR); for -C models STFHW p23 lists only "Compliant with CiA301 and CiA402" as the operation mode, and the RS-232
  port is described for configuration (STFHW p11; SSDCHW p10). HCR p52 (CB) and p65-p66 (CO) are SCL commands that
  set the CANopen bit rate and node ID. A Q program can contain arbitrary SCL motion and I/O commands, so starting
  one is an opaque vendor operation, not a CiA 402 motion.

### 5.7 Profile torque (mode 4, step-servo only)

- 0x6071 target torque INT16, 0x6087 torque slope, 0x2216 torque constant (UM21 p33-p34). AMP10 uses 0x7005 for
  the torque constant (AMP10 p45; EDS-TSM 0x7005 rw).
- Units conflict: 0x6071 is "per thousand of rated torque" and "mN·m" in the same paragraph (UM21 p59); 0x6087 is
  "per thousand of rated torque per second" and "Nm/s" (UM21 p62); the worked example uses mN·m and mN·m/s
  (UM21 p34; AMP10 p46). 0x2216 is read-only in UM21 p72/p78 and EDS-SSDC but written in the UM21 p34 example
  (MO-D04).
- Current check: the manuals tell the user to compare torque / torque constant with the drive's continuous and
  peak ratings (UM21 p34; AMP10 p46); no drive-side torque limit object other than 0x6073 is documented.

## 6. Units and scaling

### 6.1 2021 implementation (UM21, STF-C and SSDC-C)

| Quantity | Objects | Unit stated | Source |
|---|---|---|---|
| Position | 0x607A, 0x6064, 0x607C, 0x60F4 | "position units"; examples say steps | UM21 p59-p60, p63, p88 |
| Velocity | 0x6081, 0x60FF, 0x607F, 0x606C, 0x6099, 0x2209 | counts/s; examples say step/s | UM21 p59, p61, p62, p64, p74, p88 |
| Acceleration | 0x6083, 0x6084, 0x6085, 0x609A, 0x220A | counts/s²; examples say step/s² | UM21 p61-p62, p74, p88 |
| Following error window | 0x6065 | encoder counts | UM21 p59 |
| Current | 0x6073, 0x6078, 0x2203, 0x2206, 0x2601, 0x2603 | 0.01 A | UM21 p60, p73, p82 |
| Peak current | 0x2201 | mA | UM21 p73 |
| Torque | 0x6071, 0x6074 | mN·m (also "per thousand of rated torque") | UM21 p59-p60 |
| Voltage, temperature | 0x2030, 0x2019 | 0.1 V, 0.1 °C | UM21 p67-p68 |
| Times | 0x2205, 0x2210, 0x260C | 200 us cycles | UM21 p73, p76, p84 |

- Stepper resolution: 0x2604 steps per revolution is read-write on STF (UM21 p82); EG (HCR p85) sets 200-51200
  steps/rev. A profile must read 0x2604 rather than assume a microstep setting.
- Step-servo resolution: 0x2207 steps per revolution (read-only) and 0x2218 encoder resolution (read-only)
  (UM21 p72, p74, p79). Encoders are 20000 counts/rev (AM..SS-N motors) or 4096 counts/rev (AM..RS motors)
  (SSDCHW p3, p36); electronic gearing 200-51200 steps/rev (SSDCHW p36). UM21 does not say whether the "counts" of
  0x607A/0x6081 on SSDC are encoder counts or electronic-gearing steps (MO-D03).
- STF is open loop: EDS-STF names 0x6064 "Position value calculated"; EDS-SSDC names 0x6064 "Position value
  calculated" and 0x606C "Velocity value calculated". Whether SSDC reports encoder-measured or commanded values is
  not stated (MO-D20).
- No factor group (0x608F-0x6097), gear ratio or feed constant objects exist in any EDS.

### 6.2 2010 implementation (AMP10, and the EDS-TSM layout)

- Internal units: current 0.01 A, velocity 0.0042 rps (1/240 rev/s), acceleration 0.1667 rps/s (1/6 rev/s²),
  distance 1 step (AMP10 p19, rendered page checked). The same resolutions appear for VE, AC and DE in HCR p23,
  p74, p260.
- Effective lengths: 0x606C, 0x6081, 0x6083, 0x6084, 0x6099 subs 1-2, 0x609A and 0x60FF are 32-bit objects of
  which the drive uses only the low 16 bits; 0x7003 and 0x60FE use 8 bits; the unused upper bits should be written
  as zero (AMP10 p14). EDS-TSM maps 0x7009 velocity as 16 bit.
- Encoder-based actual values are separate objects: 0x7009 velocity actual and 0x700A position actual "calculated
  via motor encoder"; 0x6064 and 0x606C are "calculated" targets (AMP10 p16-p17).

## 7. Limits and protection

- Software position limits (0x607D) are not supported (absent from all EDS and OD lists). 0x607F max profile
  velocity (UM21 p61); step-servo 0x2209 velocity max and 0x220A acceleration max (UM21 p74).
- Limit inputs: STF X3 = CW limit, X4 = CCW limit (STFHW p17); TSM24C X7/X8 (TSM24 p13). Polarity/use set by
  0x260B (stepper) or 0x220F (step-servo): 1 closed-active, 2 open-active, 3 not used, 4-20 per the Host Command
  Reference (UM21 p76, p84). 0x603F bits 1/2 CCW/CW limit, bit 0 position limit (UM21 p56). 0x60FD bits 0/1/2
  negative limit, positive limit, home (UM21 p63). Disabling or reassigning limits is a configuration change with
  safety effect.
- Following error: 0x6065 window in encoder counts, 0 switches monitoring off (UM21 p59); present in EDS-SSDC and
  EDS-TSM, absent in EDS-STF. Statusword bit 13 in PP mode (UM21 p18). 0x2204/0x2205 and 0x2252 define in-position
  conditions for step-servo (UM21 p73, p81).
- Current, voltage, temperature: over/under voltage, over temperature, over current, open winding (STFHW p23;
  SSDCHW p36); current foldback (UM21 p56); 0x2217 misc flags including over-current, open winding and flash
  errors, cleared by writing 0 (UM21 p79).
- Communication loss: STFHW p23 lists "communication cable disconnection" protection without saying how it acts.
  The documented mechanism is the communication watchdog 0x2060 (EDS-TSM 0x2300):
  - sub 1 enable (default 0, disabled), sub 2 status (0 normal, 1 triggered, 2 armed), sub 3 timeout in ms (default
    0x1F4 = 500), sub 4 trigger messages (bit 0-3 RPDO1-4, bit 4 SYNC; default 0x0F), sub 5 timeout action (0 stop
    and disable, 1 stop and stay enabled, 2 and up execute Q segments) (UM21 p38, p70).
  - On a timeout the motor decelerates to zero with 0x6085 (UM21 p70). After triggering, sub 1 becomes 0 and sub 2
    becomes 1; re-arming is by writing sub 2 and sub 1 (UM21 p38). The object "can only be set with operation mode"
    and is described as "a safety feature in Profile Velocity mode" (UM21 p70) (MO-D18).
- No document states a default drive reaction to host loss: the watchdog is disabled by default, there is no
  heartbeat consumer, and 0x1029 only changes the NMT state. Host heartbeat observation does not establish any drive
  action.

## 8. I/O

- STF-C and SSDC-C: 8 optically isolated inputs (X1/X2 differential up to 2 MHz, X3/X4 differential 5 kHz,
  X5-X8 single-ended 5 kHz on STF; X5-X8 differential on SSDC per SSDCHW p36) and 4 isolated outputs, 30 V / 100 mA
  (STFHW p17-p20, p23; SSDCHW p36). SSDC adds two analog inputs, encoder outputs and a secondary encoder input
  (SSDCHW p3, p32-p33).
- 0x60FD: bits 16-23 = X1-X8 logical levels, bits 0-3 limit/home/interlock (UM21 p63, rendered page checked).
- 0x60FE sub 1: bits 0-3 = Y1-Y4; sub 2 bit mask. The mask "is edge triggered: set sub 2 first and then the bit of
  sub 1" (UM21 p63-p64). This differs from the generic CiA 402 output layout (MO-D21). EDS-TSM has only sub 1.
- Output functions: 0x2615/0x2214 motion output (moving, in-position, tach, timing), 0x2616/0x2213 alarm output on
  Y1 (UM21 p77-p78, p87). A function-assigned output does not follow 0x60FE.
- Analog: 0x221C analog inputs, 0x2212 analog configuration (UM21 p77, p79).

## 9. Persistence

- 0x1010 store: sub 1 "save config parameters", sub 2 "save communication parameters"; signature "save"
  (0x65766173) (UM21 p45; EDS names sub 1 "save config parameters"). UM21 p45-p46 lists the groups:
  - stepper config: 0x2601, 0x2603, 0x2604, 0x2607, 0x2609-0x260F, 0x2610-0x2617;
  - step-servo config: 0x2201-0x2205, 0x2208-0x2215, 0x221A, 0x2220-0x2222, 0x2224-0x2227, 0x2252;
  - communication: 0x1005, 0x1006, 0x1007, 0x1014, 0x1017, 0x1019, 0x1029, 0x1200, 0x1400-0x1403, 0x1600-0x1603,
    0x1800-0x1803, 0x1A00-0x1A03.
- 0x1011 restore: sub 1 all, sub 2 communication; signature "load" (0x64616F6C) (UM21 p47). EDS-TSM names 0x1011 sub 1
  "restore all default parameters(not yet)" (MO-D23). Whether a restore needs a reset to take effect is not stated.
- 0x2020/0x2021 take effect at the next power cycle after "store the parameter with the object at 0x1010h"; the
  sub-index that stores them is not stated (UM21 p68; MO-D16).
- Volatile: user registers 0x2100 (UM21 p71) and 0x4000 (AMP10 p18). Mode, controlword, targets and motion
  parameters (0x6060, 0x607A, 0x6081, 0x6083, ...) are not in any save list; a profile must assume they are lost at
  reset.
- Storage medium: flash in the DSP (STFHW p23; SSDCHW p36). 0x200B bit 8 "Saving" (UM21 p67); 0x2217 bits 0x4000
  "bad flash erase" and 0x8000 "bad flash save" (UM21 p79); LED code "save_failed" (STFHW p30). Write endurance is
  not stated.
- Many configuration parameters can also be written through Stepper Suite, STF Configurator or Quick Tuner over
  RS-232 (UM21 p9; STFHW p17-p19); a profile cannot assume the drive configuration was last written over CAN.

## 10. Timing statements

- TPDO1 inhibit default 10 ms; other TPDOs 0 (UM21 p53; EDS). Heartbeat 1000 ms default (UM21 p48). Watchdog timeout
  default 500 ms, unit 1 ms (UM21 p70). EMCY inhibit default 200 (unit not stated) on EDS-TSM only. Brake delays in ms (UM21 p75).
  In-position time and input filters in 200 us cycles (UM21 p73, p76).
- No SDO response time, minimum SYNC period, PDO processing latency, boot time, reset time or flash-save duration is
  stated in any source.

## 11. Vendor objects with side effects

| Object | Effect | Source |
|---|---|---|
| 0x6060 = -1 / -2 with 0x2007 (0x7007) | Starts stored Q programs (arbitrary motion and I/O); -2 starts on every SYNC | UM21 p35-p36; AMP10 p48-p49 |
| 0x2060 (0x2300) | Watchdog; actions stop+disable, stop+stay enabled, or run Q segments | UM21 p38, p70 |
| 0x200C (0x700C) | Write 1: zero all position parameters including 0x6064 | UM21 p67 |
| 0x2006 (0x7006) | Write 1: clear alarm codes 0x603F/0x200F | UM21 p66 |
| 0x2217 | Write 0: clear misc flags | UM21 p79 |
| 0x1010 / 0x1011 | Flash save or restore of whole groups | UM21 p45-p47 |
| 0x2020, 0x2021 | Node ID upper bits, bit rate (after save and power cycle) | UM21 p68 |
| 0x2604 | Changes the stepper steps per revolution, therefore every position and velocity scale | UM21 p82 |
| 0x260D, 0x2612 | Motor selection and custom motor data (changes current and motor model) | UM21 p84, p86 |
| 0x2601, 0x2603, 0x2201, 0x2203, 0x6073 | Running, idle, acceleration, peak current | UM21 p60, p73, p82 |
| 0x2208, 0x260F sub 1, 0x607E | Reverse motor direction or polarity | UM21 p61, p74, p85 |
| 0x260B, 0x220F | Limit input definition (can disable limits) | UM21 p76, p84 |
| 0x2617/0x220D, 0x2609/0x220E | Enable input and alarm-reset input usage | UM21 p75, p83, p87 |
| 0x2607/0x220C, 0x2616/0x2213, 0x2615/0x2214 | Brake, alarm and motion output functions | UM21 p75, p77-p78, p83, p87 |
| 0x2009, 0x2001, 0x2008 | Homing offset behaviour, home input, inner offset | UM21 p65-p66; EDS-STF [2008] |
| 0x2220-0x2227, 0x2211 | Step-servo control-loop gains and filters | UM21 p80-p81, p76 |
| 0x1003 sub 0 | Write 0 clears the error history | UM21 p42 |

## 12. What a profile must not assume

1. That STF/SSDC (UM21) and TSM/TXM (EDS-TSM, AMP10 layout) share an object dictionary, unit system or index
   meaning (0x2100 differs; 0x2060 vs 0x2300; 0x2001 vs 0x7001).
2. That 0x1000, 0x1018 or 0x1008 identify the family or firmware; they are identical across EDS files.
3. CiA 402 units: positions/velocities are counts or steps (UM21) or internal 1/240 rps and 1/6 rps² units with
   16-bit effective width (AMP10). Read 0x2604/0x2207/0x2218 and qualify on hardware.
4. Quick-stop option codes beyond 1 and 2 (or 2 and 6 on the 2010 generation), or the existence of 0x605D.
5. Torque mode on STF-C, or any cyclic synchronous, interpolated or velocity (VL) mode on any family.
6. That 0x603F is a CiA error code; it is a vendor alarm bit field (UM21 p56; AMP10 p20). EMCY codes are undocumented.
7. RTR, LSS, node guarding, a heartbeat consumer or segmented SDO support without device evidence.
8. That a SYNC is harmless: a drive in SYNC Q mode starts its Q segment on every SYNC.
9. That the communication watchdog is enabled or applies outside profile velocity mode; it is disabled by default.
10. That the node ID equals the rotary switch value; the upper three bits come from software or 0x2020.
11. That a node boots while an RS-232 configuration tool is connected.
12. That 0x1006/0x1007 use microseconds (UM21 says ms).
13. That EDS defaults (0 for many configuration objects, 100A "300F"/"300E") are the drive's values.
14. That a written set-point was accepted: with two set-points buffered, a third is ignored (UM21 p21).
15. That halt, a zero target or quick stop releases the shaft; halt in torque mode does release it while enabled.
16. That -FC, STF03-C-mini, MDX+, M56S or SV200 behave like STF-C/SSDC-C.

## 13. Discrepancies

- **MO-D01** Revision and firmware identity: UM21 is footed "Rev 1.0, 8/16/2021" but its history lists 1.1 of
  9/11/2020 (UM21 p1, p6). UM21 p6 names firmware 310A/310B while EDS 0x100A defaults are 300F/300E (EDS-STF,
  EDS-SSDC [100A]). UM21 p6 lists SSDC05-C, which SSDCHW does not contain.
- **MO-D02** Two object layouts: EDS-TSM (TSM/TXM-C) uses 0x7001-0x7012, 0x4000, 0x5000-0x5029, 0x2014-0x2017,
  0x2101/0x2102 and 0x2300 like AMP10 p17-p18, while UM21 uses 0x2001-0x2617. 0x2100 is user registers in
  EDS-STF/SSDC and error status bits in EDS-TSM. No current CANopen manual for TSM/TXM-C was found.
- **MO-D03** Position and velocity units: UM21 states counts/s and counts/s² (p59-p62, p64) but labels the same
  values step/s, step/s² and steps in its examples (p88-p89). For closed-loop SSDC the meaning of "counts" (encoder
  counts per 0x2218 or gearing steps per 0x2207) is not stated. AMP10 p19 uses 1/240 rps and 1/6 rps² internal
  units with 16-bit effective width (p14). The example on AMP10 p19 computes 550 x 0.1667 = 91.685 but states 91.85
  and labels acceleration "rps".
- **MO-D04** Torque objects: unit text of 0x6071 and 0x6087 is self-contradictory (UM21 p59, p62); 0x6087 is INT8 WO on
  UM21 p33 but UINT32 RW on p55/p62 and in EDS-SSDC; 0x2216 is INT8 RO on p33, UINT16 RO on p72/p78 and in EDS-SSDC,
  yet written in the p34 example. The torque example labels mode 4 "Profile velocity mode" (p34) and the p21
  set-point example labels mode 1 "Profile Torque mode".
- **MO-D05** Quick stop: UM21 p58 supports option codes 1 and 2 only; AMP10 p16 says 2 and 6 only; the EDS default 0
  is not a supported value in UM21.
- **MO-D06** Halt option code 0x605D is referenced (UM21 p22, p31, p33) but absent from the OD lists (UM21 p55) and
  all EDS files.
- **MO-D07** Homing: UM21 p62 lists method value 128 for an INT8 object and "1...37 Methods 1 to 35"; 31/32 are
  undescribed in UM21; no method number for hard-stop homing despite 0x2202; 0x2008 homing inner offset exists only
  in the EDS; CW/CCW versus negative/positive direction wording differs between AMP10 p28/p43 and UM21 p23/p30 and its
  relation to 0x607E/0x2208/0x260F is not stated; 0x2001 accepts X1-X8 in UM21 p65 but 0x7001 "0x01 thru 0x06" in
  AMP10 p17. Homing speed/acceleration types differ (UM21 p22: 0x609A INT32 vs p55/p62 UINT32).
- **MO-D08** STF-C and SSDC-C EDS defects: EDS-SSDC [1403sub2] has ObjectType 0x9 for an UNSIGNED8 entry; EDS-SSDC
  lacks 0x2206 acceleration current listed in UM21 p72; EDS-SSDC adds 0x2032/0x2033 not in UM21; EDS-STF lacks
  0x6065 although UM21 p55 lists it without a family restriction.
- **MO-D09** Access and type conflicts between UM21 and the EDS: 0x1010/0x1011 shown RO (UM21 p40) but RW subs (p45,
  EDS); 0x1400 sub 1 RO and sub 2 UNSIGNED32 RO (UM21 p50) but rw and UINT8 rw in the EDS, and the remap procedure
  needs a write; 0x60FD RW (p55), RO (p63, EDS-STF), rw (EDS-SSDC); 0x200F RO (p65), WO (p67), ro (EDS); 0x607E RO
  (p61), RW (p55), rww (EDS); 0x200B UINT32 (p67) vs UINT16 (EDS); 0x2070 UINT32 (p71) vs UINT16 (EDS); 0x2100
  UNSIGNED32 (p71) vs INT32 (EDS); 0x2020 UINT16 (p65, EDS-STF) vs UINT8 (p68, EDS-SSDC); 0x2060 subs UINT8 (p70,
  EDS-SSDC) vs UINT16 (EDS-STF), sub 0 = 4 (p38, p70) while five subs exist (EDS sub 0 = 5), sub 2 RW (p70) vs ro
  (EDS-SSDC); 0x60FF UINT32 (p31) vs INT32 (p55, p64, EDS).
- **MO-D10** 0x1006 and 0x1007 are stated in ms (UM21 p44); the generic CiA 301 definition uses microseconds. The
  drive's actual unit is unknown.
- **MO-D11** 0x1029 defaults: UM21 p49 says 1 (no change) for every sub-index, read-only; all EDS files give rw with
  sub 1 = 0 (go to pre-operational) and sub 3 = 1. EDS-STF names subs 2-6 "reserved". UM21 p49 refers to heartbeat
  consumer errors, but no 0x1016 exists.
- **MO-D12** Node guarding: AMP10 p15 lists 0x100C/0x100D; no EDS, including the 0x7000-style EDS-TSM, contains them.
- **MO-D13** RTR and TPDO triggering: UM21 p53 says RTR-only types are not supported; AMP10 p51-p52 describes
  remote-request triggering and statusword-change triggering for types 0 and 254/255; UM21 p69 describes change
  detection over the whole PDO with masks.
- **MO-D14** Default PDO maps: UM21 p52/p54 show a single default entry for all PDOs, the EDS files show two-entry
  defaults for RPDO2/3 and TPDO2/3, and AMP10 p52 shows 16-bit and 8-bit entries and 0x700A/0x7009/0x7003. UM21 p90
  maps 0x700A, which the STF/SSDC dictionary does not contain. UM21 p12 shows a TPDO1 with 0x603F/0x6041/0x606C that
  matches no default.
- **MO-D15** STF03-C-mini: named in UM21 p6 (firmware 1.02A) but its product page lists only RS-232 as the
  communication port and links no CANopen manual or EDS (PG-STF03MINI); it is not in STFHW.
- **MO-D16** Node ID and bit-rate activation and precedence: power cycle or network reset (UM21 p9; AMP10 p12),
  next power cycle after save (UM21 p68), power cycle only (STFHW p13; SSDCHW p12), power cycle or CANopen reset
  (TSM24 p12; TXM24 p12; TSM34 p19). The tool named for the upper bits differs (UM21 p9 Stepper Suite; STFHW p13 STF
  Configurator; SSDCHW p12 Quick Tuner). 0x2020 is described as setting the "high 4 bit" while its values set three
  bits (UM21 p68). The value coding of 0x2021 is not given; EDS-TSM 0x2102 defaults to 250, suggesting a different
  coding. Which 0x1010 sub-index stores 0x2020/0x2021, and whether switch or stored value wins, are not stated.
- **MO-D17** Bit-rate list: all EDS files declare 10 kbit/s and omit 12.5 kbit/s, while every manual lists 12.5 kbit/s
  and no 10 kbit/s. HCR p52 says code 1 = 800 kbit/s but its example says "CB1: set the baudrate to 1 Mbps".
- **MO-D18** Watchdog: timeout options "0x2-0x14: Q program with 1-12 segment" (UM21 p38) vs "0x2-0x0Eh: execute Q
  segments" (UM21 p70); neither range has 12 values. Applicability outside profile velocity mode, the meaning of "set
  with operation mode" and the re-arm sequence are unclear (UM21 p38, p70).
- **MO-D19** Alarm and error reporting: the LED alarm tables of STFHWC p44 (Rev C, 3/27/2025) and STFHW p30
  (10/13/2025) assign different meanings to the same flash codes (1 red 2 green "move while disabled" vs
  "drive_disabled_fault"; 3 red 3 green "blank Q segment" vs "nv_memory_fault"; 7 red 1 green "communication error" vs
  "serial_com_fault"; STFHW adds 5R3G comm_offline, 6R2G powerupunstable, 7R2G save_failed). UM21 p56 does not mark
  which 0x603F bits are faults, AMP10 p20 does. EMCY error codes are not documented anywhere.
- **MO-D20** Actual versus calculated feedback: EDS-STF and EDS-SSDC name 0x6064 "Position value calculated" and
  EDS-SSDC names 0x606C "Velocity value calculated"; AMP10 p16-p17 separates calculated targets (0x6064/0x606C) from
  encoder values (0x700A/0x7009). UM21 offers no encoder-position object for SSDC and does not say what 0x6064 is.
  UM21 also gives no Q-register mapping for 0x2100 (AMP10 p18 does for 0x4000).
- **MO-D21** 0x60FE uses bits 0-3 for Y1-Y4 with an edge-triggered mask (UM21 p63-p64), unlike the generic CiA 402
  layout; AMP10 p14 says 8 effective bits; EDS-TSM has no mask sub-index.
- **MO-D22** SSDCHW p36 states "Protocol: CoE conform CiA402" (an EtherCAT term) in a CANopen/RS-485 manual, and
  SSDCHW p34 lists ECAT and STO LED codes not relevant to -C drives.
- **MO-D23** Restore: EDS-TSM names 0x1011 sub 1 "(not yet)"; UM21 p47 does not say what is restored or whether a
  reset is needed.
- **MO-D24** Default values: UM21 gives defaults (for example 0x2617/0x220D = 3, 0x2616 = 3, 0x260C sub 1 = 60) that
  the EDS files give as 0 (EDS-STF [2617], [2616], [260Csub1]).
- **MO-D25** 0x6073 is "maximum permissible torque creating current" in UM21 p60 but "Running Current" in AMP10 p16;
  for an open-loop stepper the effect of writing it is not stated.
- **MO-D26** TIME is listed as supported (UM21 p14) without a 0x1012 object.
- **MO-D27** SDO: only expedited transfers are documented (UM21 p11); segmented support for string objects and the
  abort codes used are not documented.
- **MO-D28** AMP10 p26 asks for an NMT "Drive Disabled" state to free the shaft; there is no such NMT state.
- **MO-D29** TSM/TXM-C mode lists: TSM24 p3 and TXM24 p3 list torque, velocity, position, homing and Q; TSM34 p4 lists
  torque and velocity only; TXM34 p4 lists torque, velocity and position. Homing on the 34-frame motors is not
  stated.
- **MO-D30** AMP10 Rev K (2015), reported to cover TSM/TXM, was not obtainable from the manufacturer (remote_only
  AMP10K); applicability of Rev F to TSM/TXM is inferred only from the EDS layout.

## 14. What a profile can implement from this evidence

For STF-C (firmware 310A or later) and SSDC-C (firmware 310B or later), using UM21 with EDS-STF/EDS-SSDC, a profile
can implement host-tested support for: NMT control and boot-up detection with heartbeat production by the drive;
expedited SDO; four RPDO and four TPDO with explicit mapping in Pre-operational; the CiA 402 state machine with the
standard command codes and fault reset by bit 7; profile position with the two-deep set-point handshake; profile
velocity with halt; homing methods 1-14, 17-30, 33-35 and 37 with explicit home input (0x2001) and offset mode
(0x2009); profile torque on SSDC only; reads of 0x6064, 0x606C, 0x60F4, 0x60FD, 0x603F, 0x200B, 0x200F, 0x2019,
0x2030; outputs through 0x60FE; and explicit, separately authorized vendor operations (save/restore 0x1010/0x1011,
watchdog 0x2060, zero position 0x200C, clear alarm 0x2006, Q segment start in mode -1). Each must reject before TX
when the family or mode is unsupported.

Blocked until device or further documents are available: the actual unit of position and velocity on each family
(steps versus encoder counts) and the 0x2604/0x2207/0x2218 relation; whether 0x6064 is measured on SSDC; quick-stop
and halt option behaviour; 0x1006 units; 0x1029 and host-loss behaviour; watchdog semantics outside PV; EMCY codes;
segmented SDO; node-ID and bit-rate precedence; homing direction conventions and hard-stop homing; actual firmware
strings (0x100A, 0x2031) for the documented 310A/310B lines; and firmware release notes. TSM/TXM-C need their own
profile variant from EDS-TSM plus a current manual (AMP10 Rev K or later). MDX+ and M56S need their own packs. No
statement here is a hardware observation.
