# Lichuan DS2-C family: CANopen evidence review

Reviewed 2026-11-19 from the originals in [vendor/](vendor/) (manifest:
[SOURCES.json](SOURCES.json)). This is documentary evidence only. Nothing here
is implemented, host-tested or hardware-observed, and no statement below is a
firmware fact unless a source says so; none does.

Citation form: `DS2C p24` is one-based PDF page 24 of the source with id DS2C.
Printed page numbers are two lower than PDF pages in DS2C, MINI and DS2OP; IBL
and ISL manuals print the PDF page number. EDS citations give the source id and
the line of the section header, for example `DS2EDS l.248`. Extracted text in
[extracted/](extracted/) was used for search only; every table relied on below
was checked against the rendered PDF page.

## Scope and exact applicability

| Model (product page) | Manual | EDS | Drive or motor facts in the sources |
| --- | --- | --- | --- |
| DS2-10C ([PG-DS2-10C](vendor/DS2-10C_product_zh.html)) | [DS2C](vendor/ds2-c_user_manual_25-12-18_zh.pdf), [DS2OP](vendor/ds2-c_operating_manual_zh.pdf) | [DS2EDS](vendor/DS2_C_V1.1.eds) | Page: 100/200/400/600 W motors, 24/48 V, rated 10 A, max 15 A |
| DS2-20C ([PG-DS2-20C](vendor/DS2-20C_product_zh.html)) | DS2C, DS2OP | DS2EDS | Page: 400/750 W, 24/48 V, rated 20 A, max 30 A |
| DS2-45C ([PG-DS2-45C](vendor/DS2-45C_product_zh.html)) | DS2C, DS2OP | DS2EDS | Page: 1/1.5 kW, 48 V, rated 45 A, max 68 A |
| DS2-Mini-15C-400 ([PG-MINI-15C](vendor/DS2-Mini-15C-400_product_zh.html)) | [MINI](vendor/ds2-mini-c_user_manual_26-8-26_zh.pdf) | none linked | Page: 200/400/600 W, rated 15 A, open-frame board |
| DS2-Mini-20C-750 ([PG-MINI-20C](vendor/DS2-Mini-20C-750_product_zh.html)) | MINI | none linked | Page: 750 W, 48 V, rated 20 A |
| IBL60-C ([PG-IBL60](vendor/IBL60-C_product_zh.html)) | [IBL60](vendor/ibl60-c_user_manual_26-6-6_zh.pdf) | [IBLEDS](vendor/IBL_C_V1.0.eds) | Integrated 60-flange servo; page: 200 to 600 W, DC 24 to 50 V |
| IBL80-C ([PG-IBL80](vendor/IBL80-C_product_zh.html)) | [IBL80](vendor/ibl80-c_user_manual_26-6-6_zh.pdf) | IBLEDS | Integrated 80-flange servo; page: 750/1000 W, DC 24 to 70 V |
| ISL60-C ([PG-ISL60](vendor/ISL60-C_product_zh.html)) | [ISL60](vendor/isl60-c_user_manual_26-6-6_zh.pdf), [ISLMP](vendor/isl_motor_parameter_setup_steps_zh.pdf) | [ISLEDS](vendor/ISL_C_V1.0.eds) | L-type integrated 60-flange servo; page: 200 to 600 W |
| ISL80-C ([PG-ISL80](vendor/ISL80-C_product_zh.html)) | [ISL80](vendor/isl80-c_user_manual_26-6-6_zh.pdf), ISLMP | ISLEDS | L-type integrated 80-flange servo; page: 750 W |

Model codes. DS2C p4: `DS2-{10,20,45}{P,C,E}-{power}` where the current field
is called "maximum output current" (10 A/20 A/45 A) and C means CANopen. MINI p4:
`DS2-Mini-{10,20}{P/R,C,E}-{power}`. IBL60 p5 and ISL60 p6:
`{IBL,ISL}{60,80}{PR,C,E}-{power}W-{24V,48V}-{C17,R17,C23,R23}` where C17/C23
are 17/23-bit incremental and R17/R23 are 17/23-bit absolute encoders. The
motor code on DS2C p4 and MINI p4 is `LC..S{C17,R17,C23,R23}..` (magnetic
encoder). The product pages use these suffixes with a placeholder, for example
`ISL60-C-200W-24V-口17` on PG-ISL60; PG-IBL60 lists `IBL60-PR-...` models on
the CANopen page (DS-D25).

Supply: DS2C p5 and MINI p7 give DC 24 to 70 V; IBL60 p7, IBL80 p7, ISL60 p8
and ISL80 p7 also give DC 24 to 70 V. Encoders: DS2C p4 and ISL60 p6 claim 17 and
23-bit, single and multi-turn; IBL60 p5 and IBL80 p5 claim 17-bit single-turn
only.

Hardware and firmware revisions: no source states a hardware revision,
firmware version or the meaning of `1009`/`100A` values. File-name dates
(25-12-18, 26-8-26, 26-6-6) and PDF dates identify documents, not firmware.
Every claim below therefore has document scope only.

Comparison method. MINI, IBL60, IBL80, ISL60 and ISL80 were compared with DS2C
section by section (modes, parameter tables, CANopen object tables, faults,
homing appendix) after removing page headers and whitespace. The CANopen object
tables (`1000`..`60FF`) and the homing appendix are textually identical in all
six manuals apart from the two items noted for IBL (`6098` range, IBL60 p30) and
layout. The differences that matter are: the status-word table (DS-D04), the
torque-mode speed-limit object (DS-D11), motor and drive codes in `P00-00`,
hardware terminals, DIP switches and brake wiring (DS-D22, DS-D26), touch-probe
inputs on ISL (DS-D28), `P0D-13` encoder calibration on IBL (IBL60 p24) and the
`P02-00` default (DS-D29). IBL60 and IBL80 differ from each other only in the
status-word tables and hardware pages; ISL60 and ISL80 differ only in hardware
pages and pagination.

## Sources and retrieval failures

All 25 stored originals were downloaded fresh on 2026-11-19 with HTTP 200;
[SOURCES.json](SOURCES.json) gives URL, Referer, size and SHA-256. The CDN
`omo-oss-file110.thefastfile.com` serves only with the official page as Referer
and then redirects to a time-limited signed URL on `omo-oss-cdn.thefastfile.com`.
DS2C and DS2EDS match the SHA-256 values recorded by the
[2026-11-19 survey](../../audit/2026-11-19_market_stepperonline_lichuan.md).

| ID | Original | Role |
| --- | --- | --- |
| DS2C | [DS2-C user manual 25-12-18](vendor/ds2-c_user_manual_25-12-18_zh.pdf) | Primary dictionary and behavior source, 45 pages |
| DS2OP | [DS2-C operating manual](vendor/ds2-c_operating_manual_zh.pdf) | Older companion (PDF 2025-10-25); debug software, mode tables, fault hints, 10 pages |
| MINI | [DS2-Mini-C user manual 26-8-26](vendor/ds2-mini-c_user_manual_26-8-26_zh.pdf) | DS2-Mini variant, 46 pages |
| IBL60, IBL80 | [IBL60-C](vendor/ibl60-c_user_manual_26-6-6_zh.pdf), [IBL80-C](vendor/ibl80-c_user_manual_26-6-6_zh.pdf) | Backpack integrated servos, 47 pages each |
| ISL60, ISL80 | [ISL60-C](vendor/isl60-c_user_manual_26-6-6_zh.pdf), [ISL80-C](vendor/isl80-c_user_manual_26-6-6_zh.pdf) | L-type integrated servos, 51 and 50 pages |
| ISLMP | [ISL motor-parameter setup](vendor/isl_motor_parameter_setup_steps_zh.pdf) | Two image pages: motor and drive codes set with the vendor tool |
| DS2EDS, IBLEDS, ISLEDS | [DS2_C_V1.1.eds](vendor/DS2_C_V1.1.eds), [IBL_C_V1.0.eds](vendor/IBL_C_V1.0.eds), [ISL_C_V1.0.eds](vendor/ISL_C_V1.0.eds) | Family EDS files; the ISL original URL name is `isl_c_v1.0(1).eds` |
| DSCEDS | [LC_DSServoCAN_V1.1.eds](vendor/LC_DSServoCAN_V1.1.eds) | Older DS-C servo EDS, stored only for the identity comparison |
| PG-*, IDX30..IDX60 | Product pages and download-index pages 30/40/50/60 | Model applicability and provenance of files without a product-page link |

Not stored: the DC-servo category page (reviewed; it lists the five DS2 CANopen
models plus DS-C), the DS2 debug software zip (vendor tool, out of scope, not
downloaded) and the DS-C manual (low priority; only its EDS is used here). These
are listed under `remote_only`. No English DS2, IBL or ISL CANopen manual appears
on the English download index. There were no failed downloads.

## Interface and protocol

| Topic | Evidence | Boundary |
| --- | --- | --- |
| Physical port | DS2C p6: 8-pin CANopen port, pin1 CANH, pin2 CANL, pins 3/5/6 GND; p7 wiring shows CAN-IN and CAN-OUT. MINI p8: same port; on DS2-Mini-100 CANH/CANL are also on DIDO pins 15/16. IBL60 p6: CANH/CANL on control-connector pins 15/16. ISL60 p8: 8-pin port. | Two connectors are a daisy chain, not two controllers. |
| Node ID | DS2C p6-7: SW1..SW3 give nodes 1..7, all OFF is "custom" (自定义), SW4 is the 120 ohm termination. MINI p8-9 and ISL60 p9 are the same. IBL60 p7, IBL80 p7: SW1..SW5 give 1..31, SW6 termination. DS2C p22: `P0C-01` CANopen address 1..255, default 1, effective after power cycle. | The manual does not say that "custom" selects `P0C-01`, and 128..255 are not valid CANopen node IDs (DS-D22). |
| Bitrate | DS2C p23: `P0C-08` 0=1 Mbit/s, 1=500 k (default), 2=250 k, 3=125 k, effective after power cycle. DS2EDS l.20 also sets `BaudRate_50=1`. | No DIP bitrate selection; 50 k is EDS-only (DS-D23). `P0C-08` has no documented CANopen index. |
| NMT and boot-up | No manual text. DS2EDS l.27 `SimpleBootUpSlave=1`. | Start-up NMT state, whether PDOs need Operational and NMT reset behavior are undocumented. |
| Heartbeat and guarding | DS2C p24: `1017` producer heartbeat, U16, ms, RW/S, default 0. No `1016`, `100C` or `100D` in any manual table or EDS. | No consumer heartbeat or node guarding: the drive has no documented way to detect host loss. |
| SDO | DS2C p24: `1200:00`=2 and only `1200:01` (RO, 600h+node) listed. DS2EDS l.290/299: `1200:01/02` RW with $NODEID+0x600/0x580. All EDS objects are 1, 2 or 4 bytes. | Expedited versus segmented responses, abort codes and response times are undocumented. |
| PDO resources | DS2EDS l.32: 4 RPDO and 4 TPDO; each mapping record has HighLimit 4 (l.554). DS2C p24-26: COB-IDs 200h/300h/400h/500h+node (RPDO) and 180h/280h/380h/480h+node (TPDO), transmission type FFh, inhibit 0, event timer 0. | The mapping and default contents conflict (DS-D06). Dynamic remapping rules and accepted transmission types are not stated. |
| SYNC | DS2C p24 and DS2EDS: `1005` COB-ID 0x80. No `1006`/`1007`. | Synchronous PDO behavior is not described. |
| EMCY | DS2C p24: `1014` EMCY COB-ID default 0x80; DS2EDS l.213: $NODEID+0x80. `1001` error register, `1003` history ("supports 5" but 4 subentries), `603F` manufacturer error code U16 RO (DS2C p26). | No table maps Er.xxx faults to EMCY codes or `603F` values (DS-D07, DS-D19). |
| LSS | DS2EDS l.31 `LSS_Supported=0`; same in IBLEDS and ISLEDS. | Negative claim is EDS scope only. |
| Other buses | DS2C p22-23: Modbus (`P0C-00`, `P0C-02`, `P0C-03`, `P0C-13`, `P0C-25`, `P0C-26`); DS2OP p5: the USB Type-C debug port uses station 1, 19200 baud, even parity. | The vendor tool and Modbus are the documented path to P-parameters. |

## Identity

| Source | `1018:01` vendor | `1018:02` product | `1018:00` and serial | `1000` |
| --- | --- | --- | --- | --- |
| DS2C p24, MINI p26, IBL60 p25, IBL80 p25, ISL60 p28-29, ISL80 p27-28 | 0x0100 | 0x0002 | count 3, no sub04 | 0x00040192 (DS2C p24) |
| DS2EDS l.248/l.257 (DeviceInfo l.14-16: `DS2_CAN`) | 0x00001000 | 0x00002000 | count 3, HighLimit 4, no sub04 (l.237) | 0x40192 (l.119) |
| ISLEDS l.83/l.91 (l.15 `ISL_CAN`) | 0x1000 | 0x2000 | count 4, sub04 serial default 0 (l.75, l.107) | 0x40192 (l.54) |
| IBLEDS l.83/l.91 (l.15 `ISL_CAN`) | 0x1000 | 0x3000 | count 4, sub04 serial default 0 (l.75, l.107) | 0x40192 (l.54) |
| DSCEDS l.270/l.279 (l.15 `LC_DsServoCAN`) | 0x00001000 | 0x00000001 | count 3, no sub04 (l.259) | 0x40192 (l.141) |
| CL86-C EDS [../vendor/LC_StepDriverCAN_V1.1.eds](../vendor/LC_StepDriverCAN_V1.1.eds) l.287/l.296 | 0x00001000 | 0x00000001 | count 3, no sub04 (l.276) | 0x40192 (l.158) |
| TL-C EDS `tl_can_v1.0.eds` (SHA-256 7f1bf3e6...461f, stored by the companion LX pack) l.83/l.91 | 0x1000 | 0x0001 | count 4, sub04 serial (l.75, l.107) | 0x40192 (l.54) |

The survey claim is confirmed on the rendered DS2C p24: the manual prints
`1018:01 = 0x0100` and `1018:02 = 0x0002`, while DS2EDS declares 0x00001000 and
0x00002000. Neither a 16-bit nor a 32-bit byte swap maps one pair onto the other.
All five servo manuals print the same manual values even though the EDS files
give DS2 and ISL product 0x2000 and IBL product 0x3000 (DS-D01).

**Safety finding (backlog D18, [docs/backlog.md](../../backlog.md) line 138).**
The CL86-C stepper EDS, the TL-C integrated-stepper EDS and the DS-C servo EDS
declare the same vendor 0x1000 and product 0x0001, while their manufacturer
objects mean different things. DS-C `2201` is `Obj.SavePara` (DSCEDS l.1266)
where the stepper `2201` is `Obj.MicDivide` (CL86-C EDS l.1283). DS-C `2300`
`Obj.GearNum` (l.1293) and `2301` `Obj.GearDen` (l.1302) sit where the stepper
has `Obj.FactoryPara` (l.1310) and `Obj.ClearAlm` (l.1319). DS-C `2401`
`Obj.BrakeOnDelay` (l.1479) and `2402` `Obj.BrakeOffDelay` (l.1488) sit where the
stepper has DI filter coefficients (l.1496, l.1505). The TL-C EDS turns `2402`
into `Obj.BaudSet` (l.1569). Within this family, DS2EDS and ISLEDS share
vendor/product 0x1000/0x2000 although they describe different products, IBLEDS
(0x3000) still names itself `ISL_CAN`, and all six EDS files share
`1000 = 0x40192`. The manuals print yet another pair (DS-D01), so even the
expected live value is unknown. Consequence: `1000`/`1018` values cannot select a Lichuan profile, cannot
distinguish stepper from servo and cannot authorize any vendor-object write. A
profile binding must come from the application's declared model, with identity
values kept only as raw corroborating evidence.

Device type. DS2C p24 says `1000 = 0x00040192` and calls it CiA 301/402
support. 0x0192 is profile 402. For bits 16..23, the Nanotec N5 manual
([N5 p159](../multidrive/vendor/nanotec_n5_canopen_v3.3.0.pdf)) documents value
2 as BLDC, 4 as stepper and 6 as both. Read that way, these servo products
declare a stepper motor type, the same value as the CL86-C stepper EDS. N5 is
another vendor's description; the normative CiA 402 text was not obtained
(DS-D03).

## Drive state machine and CiA 402 differences

Enable. DS2C p11 (position), p12 (velocity), p13 (torque, homing): write `6060`,
check `6061`, then write controlword 6, 7, 15 "about 10 ms apart". Status bits
0..2 read 000/001/011/111 after controlword 0/6/7/15 (DS2C p12). These match
the CiA 402 shutdown, switch-on and enable-operation commands, but no source
documents bits 4, 5, 6, 9, 11, 13 or 14, the switch-on-disabled state or the
fault state machine.

Status-word dialects (all checked on rendered pages):

| Bit | DS2C p12-13, MINI p13/p15, IBL80 p13-14 | IBL60 p13-14, ISL60 p15/p17, ISL80 p14/p16 | CiA 402 meaning for comparison |
| --- | --- | --- | --- |
| 3 | not listed | 1 = drive alarm | fault |
| 7 | 1 = drive alarm | not listed | warning |
| 8 | 1 = homing complete | not listed | manufacturer-specific |
| 10 | 1 = motor speed 0, 0 = moving | 1 = positioning complete when 6060=1, homing complete when 6060=6 | target reached |
| 12 | 1 = target position effective | not listed | mode-specific (setpoint acknowledge in pp) |
| 15 | 1 = position-mode positioning complete | not listed | manufacturer-specific |

IBL60 and IBL80 share one EDS but document different layouts, and the DS2C
layout puts "alarm" on the CiA warning bit and "zero speed" on the CiA
target-reached bit. A generic CiA 402 decoder would misread DS2C-style drives
(DS-D04).

Other controlword use:

- Quick stop: DS2C p11 lists 15 to 11 as "emergency stop" (急停). `605A` quick-stop
  option is absent from manual tables and EDS; `6085` quick-stop deceleration
  exists, U32 pul/s², default 10000 (DS2C p28). The stop reaction is undocumented.
- Halt: `605D` 0 = normal stop, 1 = quick stop, I16, default 0 (DS2C p27);
  DS2EDS l.1217 default 1. Controlword bit 8 is not described.
- Fault reset: no controlword fault reset (bit 7) is documented. DS2C p23 has
  `P0D-01` fault reset and `P0D-00` software reset; DS2C p9 has DI function
  FunIN.2 ALM-RST (edge-triggered), noting that resettable faults require
  servo-on to be removed first. The fault table (DS2C p30-32) marks each fault
  resettable or not. None of these has a documented CANopen index (DS-D21).
- Brake: DS2C p6 has BRK+/BRK- driven from an internal 24 V supply; `P02-09`
  brake-on to command delay 0..500 ms (default 250) and `P02-10` brake-off to
  motor de-energize delay 1..1000 ms (default 150) (DS2C p16); Er.625/Er.626
  detect abnormal brake states (DS2C p31). IBL and ISL use DO function FunOUT.9
  BK instead (IBL60 p11, ISL60 p13). The drive therefore appears to sequence
  the brake itself on enable and disable; this is inferred from the delays and
  not stated.
- Emergency stop input: FunIN.34 stops at zero speed and locks position (DS2C p9);
  `P0D-05` software emergency stop (DS2C p23); Er.900 warning (DS2C p33).

## Supported modes and their objects

`6060`/`6061` values: 1 profile position, 3 profile velocity, 4 profile torque,
6 homing (DS2C p10, p27). No interpolated position, CSP, CSV or CST mode is
documented and `6502` supported-drive-modes is absent.

| Mode | Objects (DS2C) | Documented sequence and notes |
| --- | --- | --- |
| Profile position (1) | `607A`, `6081`, `6083`, `6084`, `6064` (p10-11) | Enable, write targets, then controlword bit 4 rising edge starts; bit 5 0 = finish current profile first, 1 = update immediately; bit 6 0 absolute, 1 relative. Examples 15 to 31/95/63/127 (p11). |
| Profile velocity (3) | `60FF`, `6083`, `6084` (p12) | Write targets first, then enabling with 6, 7, 15 starts motion immediately (p12). |
| Profile torque (4) | `6071`, `6072`, `6087`, `607F`, `6077` (p12) | Write targets first, then enabling starts motion (p13). IBL60/IBL80 p13 replace `607F` with vendor object `2406h` (DS-D11). |
| Homing (6) | `6098`, `6099:01/02`, `609A`, `607C` (p13) | Enable, write parameters, controlword bit 4 0 to 1 starts and 1 to 0 interrupts (p13). Done flag: bit 8 or bit 10 by dialect (DS-D04). |

Homing methods (DS2C p13, appendix p34-45): 1..14, 17..30, 33..35. Methods 1/2
use the negative/positive limit with Z pulse; 3..6 the home switch with Z; 7..14
home switch plus limit with Z; 17/18 limit only; 19..30 home switch (plus limit
for 23..30) without Z; 33/34 the next Z pulse in negative/positive direction; 35
sets the current position as home after controlword 0x0F to 0x1F. Method 35's
description depends on `60E6` and an offset in "`606C`" (DS2C p45), neither of
which is defined (DS-D18). `6099:01` is the search speed and `6099:02` the speed
after the home is found (DS2C p28). `6098` default is 17 in the manual (DS2C
p28) and 0 in DS2EDS l.1472.

Torque objects: `6071` target torque and `6077` actual torque in 0.1 %, `6072`
maximum torque in 0.1 % with default 1000, `6087` torque slope in 0.1 % with
default 100 (DS2C p27-28). The reference (rated torque, `P00-12`) and the time
base of the slope are not stated (DS-D12, DS-D13).

## Units and scaling

- Position objects use "pul": `6064`, `6065`, `6067`, `607A`, `607C` (DS2C p27-28).
  Velocity objects use pul/s: `606C`, `606D`, `607F`, `6081`, `6099`, `60FF`.
  Acceleration objects use pul/s²: `6083`, `6084`, `6085`, `609A`. The EDS
  instead names `6083`/`6084` "accel time"/"decel time" with default 100
  (DS2EDS l.1407/l.1416) and DS2OP p6-7 calls them acceleration time (DS-D16).
- Electronic gear: `6091:01` numerator I32 default 131072 and `6091:02`
  denominator default 10000, type not printed (DS2C p28). DS2EDS l.1456 names
  them MotorRevolutions/ShaftRevolutions. With the usual electronic-gear reading
  (encoder counts = command pulses times numerator over denominator) the
  defaults give 10000 command pulses per revolution of a 131072-count encoder.
  That relation is an inference; the manual never defines "pul" against the
  encoder.
- Encoder resolution: `P00-31`/`P00-32` encoder lines low/high, 131072..8388608,
  read-only (DS2C p15). For a 23-bit motor the default gear would not give 10000
  pulses per revolution; read the live gear and encoder values.
- Absolute systems: `P02-01` 0 incremental, 1 absolute (DS2C p16); `P0B-70`
  multi-turn and `P0B-71` single-turn data (DS2C p22); `P0D-20` resets the
  absolute encoder and its multi-turn data (DS2C p23).
- Direction: `P02-02` 0 = CCW positive, 1 = CW positive, effective after power
  cycle (DS2C p16); `607E` polarity U8 with a range of 0..65535 and no bit
  meanings (DS2C p28).
- Motor: `P00-14` rated 3000 rpm, `P00-15` maximum 6000 rpm, five pole pairs,
  torque constant and inertia ranges (DS2C p14-15). These are motor-table
  ranges, not limits a profile can assume.

## Limits and protection

| Function | Evidence | Boundary |
| --- | --- | --- |
| Hardware limits and home switch | FunIN.14 P-OT and FunIN.15 N-OT block motion in one direction; factory DI4 = 14, DI5 = 15, DI6 = 31 home switch (DS2C p9). Er.950/Er.952 overtravel warnings (DS2C p33). | Inputs are configurable; an unconnected terminal still has its function. |
| Software limits | `607D` I32 limits with defaults 2147483647/2147483648 (DS2C p28); Er.D09 upper below lower, Er.D10 home offset outside the limits (DS2C p32). | Sub-index order conflicts with the EDS and activation conditions are unstated (DS-D15). |
| Following error | `6065` window, default 131072 pul; `6066` "following-error count" U16 (DS2C p27); `P0A-10` position-deviation fault threshold, default 3145728 (DS2C p21), raising Er.B00 (DS2C p32). | Relation between `6065` and `P0A-10` is not stated. |
| Speed, torque and overload | `P0A-08` overspeed threshold (Er.500); `P07-09`/`P07-10` torque limits 300 %; `P0A-04` overload gain; `P0A-26` can mask motor overload; `P0A-12` runaway protection (DS2C p18, p21, p31). | Protection masks are vendor parameters with no CANopen index. |
| Temperature and supply | Er.650 heatsink over-temperature, Er.400/Er.410 bus voltage (DS2C p31-32); `P01-15` overvoltage 85 V, `P01-17` undervoltage 16 V (DS2C p15). | Er.400/Er.410 texts cite 420 V/200 V from an AC drive (DS-D27). |
| Communication loss | No source documents a CAN communication timeout, `6007` abort-connection option, consumer heartbeat or guarding. | Host loss produces no documented drive action. |

## I/O

- Six inputs: DI1/DI2 differential (DS2C p8: 5..24 V; IBL60 p6: 5 V, 24 V needs a
  1.5 kohm series resistor), DI3..DI6 single-ended 12..24 V with NPN or PNP common
  (DS2C p6, p8). Two outputs DO1/DO2; DOCOM must be 0 V (DS2C p8); MINI p8 gives
  100 mA maximum.
- Input functions `P03-02..P03-13` (function 0..37, logic 0..4: low, high,
  rising, falling, both edges) with forced level/edge rules per function (DS2C
  p9, p16-17). Documented functions: FunIN.2 ALM-RST, FunIN.3 gain switch,
  FunIN.14 P-OT, FunIN.15 N-OT, FunIN.31 home switch, FunIN.34 emergency stop;
  ISL adds FunIN.38/39 touch probes tied to `60B8h` (ISL60 p12).
- Output functions `P04-00..P04-03` (DS2C p17): FunOUT.1 ready, 2 rotating,
  3 zero speed, 4 speed coincidence, 5 positioning complete (default DO1),
  6 near, 10 warning, 11 alarm (default DO2), 16 home attained, 18 torque
  reached, 19 speed reached (DS2C p10); IBL/ISL add 9 brake (IBL60 p11).
- CANopen views: `60FD` bit 0 negative limit, bit 1 positive limit, bit 2 home,
  bits 16..31 user (DS2C p28, default printed as 10000). `60FE:01` physical
  outputs and `:02` enable mask, bits 16..31 user (DS2C p29). Which bit drives
  DO1 or DO2 and how `60FE` interacts with `P04` assignments is not stated.
- `P0D-17..P0D-19` force DI and DO values (DS2C p23); a profile must not use them
  as ordinary I/O.

## Persistence

- No `1010` store or `1011` restore object exists in any manual table or EDS.
- `P02-31` system initialization: 0 none, 1 restore factory values except P00/P01,
  2 clear fault records, 3 save the 6000-group parameters (DS2C p16). No
  CANopen index, completion signal or duration is given (DS-D20).
- Communication objects are marked "RW/S" (DS2C p24-26), which suggests storable
  entries, but no store mechanism is described for them.
- `P0A-03` power-loss save enable (DS2C p21). `P0C-13` decides whether Modbus
  writes update EEPROM (DS2C p23); Er.942 warns when more than 200 parameters
  are changed at once and advises `P0C-13 = 0` (DS2C p33). Er.108 is a parameter
  store failure (DS2C p30).
- Effect columns: "立即生效" immediate, "停机生效" when stopped, "再次通电" after
  the next power cycle; setting columns: "停机设定" only while stopped, "运行设定"
  also while running (DS2C p14-23). Er.941 reminds that a power cycle is needed
  (DS2C p33).
- Motor and drive codes `P00-00`/`P01-02` are set with the vendor tool behind a
  password, followed by a power cycle (ISLMP p1-2; DS2OP p5-6). Er.120 reports a
  motor/drive mismatch (DS2C p30).

## Timing statements

- Controlword 6, 7, 15 "about 10 ms apart" (DS2C p11-13; DS2OP p6-7). This is a
  manual example, not a measured minimum.
- DS2OP p9: after power-off wait at least 10 s before powering on again, or
  Er.410 may occur.
- `6068` position window time and `606E` velocity window time in ms, defaults 100
  (DS2C p27); `P02-09`/`P02-10` brake delays (DS2C p16); `P07-40` torque-mode
  speed-limit window 0.5..30 ms (DS2C p18); Er.625 evaluates 100..500 ms after a
  zero command (DS2C p31).
- `1017` heartbeat period in ms (DS2C p24). No SDO response time, boot time,
  minimum PDO period or cycle time is documented.

## Vendor objects with side effects

The DS2, IBL and ISL EDS files declare no manufacturer objects (`ManufacturerObjects`
count 0, DS2EDS l.56). The only vendor index in any manual is `2406h`
"torque-mode speed limit", U16 0..65535 RW, in IBL60 p13 and IBL80 p13; it is
absent from IBLEDS. All vendor functions are P-parameters with no documented
CANopen index, so none is reachable by a profile from this evidence. Those with
side effects, kept here so that a later mapping is not treated as harmless:

| Parameter | Effect (DS2C page) |
| --- | --- |
| `P02-31` | Factory restore, fault-record clear or save (p16) |
| `P0D-00`, `P0D-01` | Software reset, fault reset (p23) |
| `P0D-03` | Initial-angle identification (p23); may energize the motor |
| `P0D-04` | Encoder ROM read/write (p23) |
| `P0D-05` | Emergency stop (p23) |
| `P0D-11` | JOG trial run (p23); moves the motor |
| `P0D-13` | Magnetic-encoder calibration, IBL only (IBL60 p24) |
| `P0D-17..P0D-19` | Force DI and DO states (p23) |
| `P0D-20` | Absolute-encoder reset including multi-turn data (p23) |
| `P09-00..P09-09` | Auto-tuning and offline inertia identification (p20); moves the motor |
| `P0A-12`, `P0A-26`, `P0A-35`, `P0A-36` | Disable runaway, overload, encoder-EEPROM read or multi-turn overflow protections (p21) |
| `P00-00`, `P01-02` | Motor and drive codes; mismatch gives Er.120 (p14-15, p30) |

## What a profile must not assume

- That a `1018` or `1000` value identifies a DS2, IBL or ISL drive, or that it
  distinguishes servo from stepper (D18, DS-D01..DS-D03).
- A CiA 402 status-word layout, or one layout across the family (DS-D04).
- That enabling is harmless in velocity or torque mode: the documented sequence
  starts motion on enable with the retained targets (DS2C p12-13).
- That controlword bit 7 resets faults, that 15 to 11 is a CiA quick stop with a
  known reaction, or that `605D`/`6085` behave as CiA defines.
- PDO default mappings, event timers or COB-ID writability (DS-D06).
- Any P-parameter access, store mechanism, factory reset or reboot path over CANopen.
- Units of "pul" relative to the encoder, the torque reference base or the torque
  slope time base.
- Any drive action on host loss.
- 50 kbit/s support, node IDs above 31 (IBL) or 7 (DIP, others), or that DIP "custom"
  selects `P0C-01`.
- That IBL `2406h` or ISL `60B8h` exist on a live drive.

## Discrepancies

- **DS-D01** `1018` values: manuals print 0x0100/0x0002 (DS2C p24, MINI p26,
  IBL60 p25, IBL80 p25, ISL60 p28-29, ISL80 p27-28); EDS files give
  0x00001000/0x00002000 (DS2EDS l.248/l.257), 0x1000/0x2000 (ISLEDS l.83/l.91)
  and 0x1000/0x3000 (IBLEDS l.83/l.91). Unresolved; read live values and keep both
  claims.
- **DS-D02** Identity collisions (D18): CL86-C, TL-C and DS-C EDS share 0x1000/0x0001
  with different `2xxx` meanings; DS2EDS and ISLEDS share 0x1000/0x2000; IBLEDS
  calls itself `ISL_CAN` (l.15) and differs from ISLEDS only in the product code
  and file information.
- **DS-D03** `1000 = 0x00040192` on servo products (DS2C p24; all EDS files). Under
  the N5 p159 description, bits 16..23 = 4 means stepper motor. Normative text
  not obtained.
- **DS-D04** Two status-word layouts (table in the state-machine section). IBL60 and
  IBL80 share IBLEDS but differ (IBL60 p13 versus IBL80 p13).
- **DS-D05** `1018` count and serial: manuals show count 3 without sub04; DS2EDS
  count 3 with HighLimit 4 (l.237); IBLEDS/ISLEDS count 4 with a sub04 serial (l.75,
  l.107).
- **DS-D06** PDO defaults: DS2C p25-26 maps only `1600:01 = 60400010` (count 1)
  and leaves every other record at count 0; DS2EDS maps RPDO1 6040/60FF/6060
  (l.554-583), RPDO2 607A/6081 (l.606-626), TPDO1 6041/60FD/6061 (l.995-1024),
  TPDO2 6064/606C (l.1047-1067). Event timers are 0 in the manual and 1000/100 ms
  in the EDS (l.359, l.800); PDO COB-IDs are RO in the manual and RW in the EDS.
- **DS-D07** `1014` default 0x80 (DS2C p24) versus $NODEID+0x80 (DS2EDS l.213);
  `1200` lists one RO entry in the manual versus two RW entries in the EDS
  (l.290/l.299).
- **DS-D08** Access and types: `6040` and `6060` RW in the manual (DS2C p27) and WO
  in DS2EDS (l.1199, l.1226); `6060` I8 versus U8; `6098` I08 default 17 (DS2C p28),
  U8 in the mode table (p13) and U8 default 0 in DS2EDS (l.1472).
- **DS-D09** `607A`: "total pulses" U32 default 50000 (DS2C p27) versus I32 in the
  mode table (p10) and I32 default 5000 in DS2EDS (l.1333). The printed range
  -2147483647..2147483648 does not fit I32.
- **DS-D10** `60FF`: named "maximum speed in velocity mode", I32 default 0 (DS2C
  p29); U32 0..2147483648 in the mode table (p12); signed range with type U32 in
  DS2OP p6, IBL60 p13 and ISL80 p14; I32 default 120 in DS2EDS (l.1562).
- **DS-D11** Torque-mode speed limit: `607F` U16 -65535..65535 (DS2C p12, MINI
  p14) versus `607F` U32 0..2147483648 default 2147483648 (DS2C p28) and ISL60 p16;
  IBL60/IBL80 p13 use vendor `2406h` U16 instead, and IBL60 p14 names `6072` as
  the speed limit; `2406h` is absent from IBLEDS.
- **DS-D12** `6087` torque slope: 0.1 % unit with no time base, range 0..65535
  (DS2C p12), 0..2147483648 (ISL60 p16), default 100 (DS2C p28).
- **DS-D13** `6072` maximum torque: I16 -32768..32767 in the mode table (DS2C p12)
  versus U16 default 1000 in the object table (p27) and DS2EDS l.1317.
- **DS-D14** `6077` actual torque: RW in the object table (DS2C p27) versus RO in
  the mode table (p12) and DS2EDS l.1325.
- **DS-D15** `607D`: both rows printed as sub-index 01, first positive then negative,
  negative default 2147483648 (DS2C p28); DS2EDS sub1 = MIN, sub2 = MAX (l.1366,
  l.1374).
- **DS-D16** Ramp units: pul/s² in DS2C p10/p28 versus "accel time/decel time"
  defaults 100 in DS2EDS (l.1407/l.1416) and "acceleration time" in DS2OP p6-7 and
  DS2C p13 (`609A`); the DS2C p11 profile figure uses rpm and ms. `6099` defaults
  20000/10000 pul/s (DS2C p28) versus 120/60 (DS2EDS l.1496/l.1505); DS2OP p7
  ranges 0..3000 and 0..2000.
- **DS-D17** `605D` default 0 (DS2C p27) versus 1 (DS2EDS l.1217); halt bit
  behavior undocumented.
- **DS-D18** Homing method 35 depends on undefined `60E6` and an offset named
  `606C` (DS2C p45); IBL object table gives `6098` range 1..14, 17..35 (IBL60 p30),
  which adds 31/32 to the documented list.
- **DS-D19** `1003` text says five error stores while the table has four entries
  (DS2C p24); no Er-code to EMCY or `603F` mapping exists.
- **DS-D20** Save and restore: `P02-31 = 3` saves the 6000 group (DS2C p16) with no
  CANopen index; no `1010`/`1011`. The table of contents lists sections 3.6.1 to
  3.6.3 (clear position, save, alarm reset) as broken references with no body (DS2C
  p3).
- **DS-D21** Fault reset is documented only through `P0D-01` and DI FunIN.2 (DS2C
  p9, p23), not through the controlword.
- **DS-D22** Node ID: DIP range 1..7 (DS2C p7, MINI p9, ISL60 p9) versus 1..31 on IBL
  (IBL60 p7, which lists the switches in reverse order against IBL80 p7); `P0C-01`
  range 1..255 (DS2C p22); the meaning of "custom" is unstated.
- **DS-D23** Bitrates: `P0C-08` offers 1 M/500 k/250 k/125 k (DS2C p23) while
  DS2EDS also advertises 50 k (l.20).
- **DS-D24** Current ratings: DS2C p4 calls 10/20/45 the maximum output current; the
  product pages give rated 10/20/45 A and maximum 15/30/68 A. Manual power range
  100..1500 W (DS2C p4) versus per-model ranges on the pages. MINI p4 offers current
  codes 10/20 while the product is DS2-Mini-15C-400.
- **DS-D25** Product-page conflicts: PG-IBL60 gives DC 24..50 V and 200..600 W versus
  DC 24..70 V and 200..1000 W (IBL60 p5, p7) and lists `IBL60-PR` models; PG-ISL60
  and PG-ISL80 give 17-bit encoders only versus 17/23-bit (ISL60 p6); IBL manuals
  say 17-bit single-turn only while the model code includes C23/R23 (IBL60 p5).
- **DS-D26** Brake wiring: DS2C p6 has BRK+/BRK-; MINI p8 has no brake pins while its
  wiring figure shows BK+/BK- to the brake (MINI p10); IBL and ISL use FunOUT.9 and
  label the braking-resistor terminals BK-/BK+ (IBL60 p7, ISL60 p8). FunOUT.9 is
  absent from the DS2C DO table (p10).
- **DS-D27** Fault texts copied from an AC drive: Er.400 420 V, Er.410 200 V and the
  220 V/310 V bus note (DS2C p31) against a 24..70 VDC drive with 85 V/16 V
  thresholds (p15); Er.601 cites `P05-30`/`P05-34`/`P05-35`, which are not listed
  (p33).
- **DS-D28** ISL touch-probe inputs reference `60B8h` (ISL60 p12, ISL80 p11), absent
  from all object tables and ISLEDS.
- **DS-D29** `P02-00` control mode offers only 9 (bus) but defaults to 1 in DS2C p15
  and MINI p17; IBL and ISL default 9 (IBL60 p16).
- **DS-D30** `60FD` RO input-state default printed as 10000 (DS2C p28); the monitor
  `P02-34` fault code (DS2C p22) is `P0B-34` in MINI p24.
- **DS-D31** IBL `P00-00` lists motor codes 10501..10506 but default 10101 (IBL60 p15);
  MINI p16 lists 106X4 and 106X5 both as 50 W 48 V.
- **DS-D32** No communication-timeout, host-loss, NMT or boot behavior is documented in
  any source (gap).

## What a profile can implement from this evidence

A host-side profile can implement, gated on the user's declared model and live
reads: a raw identity read that keeps manual and EDS claims side by side;
SDO access to the documented 1000- and 6000-group objects with conservative
types; mode selection 1/3/4/6 with `6061` confirmation; the 6, 7, 15 enable
sequence as a state-transition request that never starts motion implicitly;
profile-position moves with the documented bit 4/5/6 trigger; homing with the
listed methods and only the signals each method needs; profile torque with
`6071`/`6072`/`6087` and the model-specific speed-limit object; reads of `6064`,
`606C`, `6077`, `60FD`, `603F`, `1001`, `1003`; and a per-model status-word
decoder for the two documented layouts. It cannot yet implement fault reset,
quick-stop reaction, save/restore, P-parameter configuration, PDO defaults,
unit conversion to physical units, host-loss supervision or any vendor-object
write. Those need live reads of the actual drive, its firmware identification
and a qualified fixture.
