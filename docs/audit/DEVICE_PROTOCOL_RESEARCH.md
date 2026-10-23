# CL86-C and related Lichuan CANopen drive research

Audited 2026-10-23. No hardware was attached or exercised. This is a source audit,
not a support certification or a library implementation. Source IDs below resolve
through [the source pack](../reference/README.md) and its
[hash manifest](../reference/SOURCES.json). Page references are **one-based PDF
pages**, with printed page numbers two lower in the 20-page manuals.

## Identity, scope and evidence hierarchy

The [user's listing](https://www.alibaba.com/product-detail/CANopen-CL86-C-DC30V-110V-AC20V_1600643108175.html)
identifies a CL86-C CANopen stepper drive. Alibaba returned no usable article
body to the browser fetcher, so it is a discovery link, not the protocol authority.
The [manufacturer CL86-C page](https://en.xlichuan.com/product-xq/143.html)
establishes the product family and links the currently downloadable English
manual. The [Chinese manufacturer index](https://www.xlichuan.com/download/23.html)
provides the EDS and four other manuals.

CL86-C is a finished closed-loop motor drive. No internal DSP part number or
chip register map is identified. The future library controls its CANopen
interface; do not advertise a driver for an inferred chip. CL86-RX is a different
RS485 product and EtherCAT variants are separate profiles.

EN86/ZH86 p3 explicitly group CL86-C and OL86-C. ZH57 p3 groups CL57-C and OL57-C.
The CL versions describe a 1000-line incremental encoder; OL versions have no
encoder. ZH57 gives 24-48 VDC/5 A; ZH86 gives 36-110 VDC or 18-80 VAC/8 A.
The CL86 product page instead gives 30-110 VDC or 20-80 VAC. Preserve this supply
range conflict for the installation guide; confirm the actual unit label before
bench setup. Do not make current or encoder assumptions common to every model.

All grouped manuals state CiA 301 and CiA 402 V2, with Profile Position (mode 1),
Profile Velocity (3), and Homing (6). This supports a bounded vendor profile with
drive-generated trajectories. It does not establish CSP/CSV/CST, torque mode,
interpolated position, CAN FD, LSS, arbitrary CiA 402 devices, multi-axis timing
guarantees, a safety-rated stop, or compatibility with every Lichuan drive.

The sibling RS repository's `docs/reference/07_canopen_feasibility.md` previously
could not obtain the source files. This pack resolves that retrieval gap, not its
remaining firmware, watchdog and bench-qualification questions. Its accepted
boundary remains useful: separate CAN library, common motion semantics,
application-owned transport/stack, no automatic replay after uncertainty.

## Physical connection and provisioning

EN86/ZH86 pp5,9-10 identify two RJ45 CAN connectors with pin 1 CAN_H/CAN+, pin 2
CAN_L/CAN-, pin 3 GND, other pins unused. RS232 is a separate debugging port.
OP86 incorrectly titles its CAN pin table "RS485" (p3) and uses 57C internal
headers. Neither error proves an RS485 runtime interface.

SW1-SW5 select IDs 1-31; all OFF selects a user-defined ID. Object 2203 is listed
as the CAN ID, with table range 0-127 (ZH86 p18). Do not permit node 0 as an
ordinary addressed node merely because this table includes zero. Live effect,
persistence, and reset requirements for 2203 need qualification. Marketing's
32-axis limit is not the generic CANopen node-number limit.

SW6/SW7 select 125/250/500/1000 kbit/s. SW8 connects 120 ohms termination.
EDS additionally marks 50 kbit/s supported, but these manuals give no DIP setting
for it. Expose that as unresolved, not as a supported commissioning choice.
The phrase "2 x 1 Mbps" in p3 accompanies two connectors; it does not establish
two independently controlled CAN buses.

## EDS facts and dictionary baseline

EDS FileInfo reports EDSVersion 4.0, file version/revision 1/1, creation
2020-12-23, modification 2022-11-11. Its internal FileName is `LC_StepD.eds`, and
ProductName contains the literal spelling `LC_SterDriverCAN`. DeviceInfo gives
VendorName XLC, vendor ID 0x1000, product number 1, revision 0. These generic values
cannot uniquely identify every model or prove a firmware match. Object 1018 has
only subindexes 0-3; a serial-number subindex 4 is not supplied.

The file has 99 top-level indexes: 3 mandatory, 43 optional, 53 manufacturer
objects. Its [raw section export](../reference/extracted/LC_StepDriverCAN_V1.1.raw_sections.json)
preserves all fields. A future production catalogue must reconcile it against
manuals, add units/side effects/evidence, and retain unresolved entries instead of
silently generating all entries as safe read/write methods.

| Object | Documented role | Wire representation / evidence |
| --- | --- | --- |
| 1000:00 | Device type | EDS U32 RO, default 0x00040192 |
| 1001:00; 1003 | Error register/history | EDS U8 RO; error-history record |
| 1005:00 | SYNC COB-ID | EDS U32 RW, default 0x80; no cyclic-mode promise |
| 1009:00; 100A:00 | Hardware/software revision | U16 RO in EDS and manuals, not generic visible strings |
| 1014:00 | EMCY COB-ID | U32 RW, EDS default node+0x80 |
| 1017:00 | Producer heartbeat time | U16 RW, EDS default 0; no heartbeat by default is plausible |
| 1018:01-03 | Vendor/product/revision | U32 RO; 0x1000, 1, 0 are EDS defaults |
| 1200:01-02 | Server SDO COB-IDs | EDS node+0x600 request, node+0x580 response |
| 603F:00 | Error code | U16, EDS RO versus manual RW; examples FF01 overcurrent, FF05 following error (ZH86 p16) |
| 6040:00 | Controlword | U16, EDS WO versus manual RW |
| 6041:00 | Statusword | U16 RO; contradictory bit interpretations below |
| 605D:00 | Halt option | I16 RW, EDS default 1; supported option values/reactions not specified |
| 6060:00; 6061:00 | Mode command/display | EDS U8 WO/RO; manuals I8 and inconsistent access; documented positive values 1,3,6 |
| 6064:00; 606C:00 | Actual position/velocity | I32 RO; actual-value scale needs explicit qualification |
| 607A:00; 607C:00 | Target position/home offset | I32 RW; motion tables give -1,000,000 to +1,000,000 |
| 607D:01-02 | Minimum/maximum software position | I32 RW; enabling/referencing policy is separate |
| 6081:00 | Profile position speed | U32 RW, rpm, table range 0-3000 |
| 6083:00; 6084:00 | Acceleration/deceleration time | U32 RW, **milliseconds**, table range 0-2000 |
| 6098:00 | Homing method | EDS U8, dictionary table I8; methods 17,18,24,29 |
| 6099:01-02 | Search/creep speed | U32 RW, rpm, table range 0-3000; "creep time" is inconsistent wording |
| 609A:00 | Homing ramp time | U32 RW, **milliseconds**, table range 0-2000 |
| 60FD:00 | Digital input bitfield | U32 RO; verify functional versus physical bit interpretation |
| 60FE:01-02 | Digital outputs/mask | U32 RW; output allocation and reserved bits need qualification |
| 60FF:00 | Target velocity | EDS/dictionary I32, but mode table U32 and 0-3000 rpm: signed reverse motion unresolved |

Motion tables are EN86/ZH86 pp11,13; dictionaries pp15-20. Distinguish a 32-bit
wire representation from permitted range and mechanical travel. Do not promote
EDS defaults to actual configuration, limits, safe speeds, or units.

Manufacturer objects (ZH86 pp17-20; EDS) include:

- 2000/2001 model/version U16 RO. No complete model-code table is supplied.
- 2100 monitor word: manual RO, EDS RW. Manual bits 0 in-position, 1 homed,
  2 running, 3 alarm, 4 released (inverted enable sense), 5 positive soft limit,
  6 negative soft limit. Treat as observation; do not write it to resolve access
  conflict. 2101/2102 are input/output status with the same RO/RW conflict.
- 2200 direction-related setting; description mixes pulse-interface wording.
  2201 subdivision: main table says 400-51200, position table 0-65535, EDS default
  0. Determine actual accepted values and counts/revolution before conversion.
  2202 soft limits: manual values 0 disabled, 1 effective after homing, but EDS
  default 10. 2203 CAN node ID changes topology and must be explicit.
- 2300 parameter action: **1 factory reset; 2 save current parameters**. Never
  initialize by dumping defaults or automatically persist. 2302 clears current
  position on 0-to-1, then must be cleared manually. EDS also lists 2301 ClearAlm
  without a matching detailed procedure in these manuals; do not invent one.
- 2400 I/O settings record, 2401-2407 per-input filters. The manual calls 2400:0A
  a filter, while EDS calls it reserved; subindex 0 is U16 in the EDS. Manual
  output-function encodings differ between language editions (see below).
- 2500 selects open/closed loop; 2501 encoder resolution; 2502 current in mA;
  2503-2507 current ratios/holding time. 2508-2519 tuning entries are described
  as factory settings generally not requiring change. EDS extends through 2520,
  beyond the detailed manual table; retain explicit unknown semantic fields.

## PDO evidence, not an assumed live layout

EDS declares four RxPDOs and four TxPDOs. Each mapping record has four possible
entries; count zero for PDO3/4 means no active entries in the EDS template.

| PDO | EDS COB-ID | EDS active mapping in byte order | Size |
| --- | --- | --- | --- |
| RPDO1 | node+0x200 | 6040:00 U16, 60FF:00 I32, 6060:00 U8 | 7 bytes |
| RPDO2 | node+0x300 | 607A:00 I32, 6081:00 U32 | 8 bytes |
| RPDO3/4 | node+0x400 / node+0x500 | none | 0 entries |
| TPDO1 | node+0x180 | 6041:00 U16, 60FD:00 U32, 6061:00 U8 | 7 bytes |
| TPDO2 | node+0x280 | 6064:00 I32, 606C:00 I32 | 8 bytes |
| TPDO3/4 | node+0x380 / node+0x480 | none | 0 entries |

Transmission type defaults to 255 in 1400-1403 and 1800-1803. EDS event-timer
fields default to 1000 for Rx and 100 for Tx, inhibit time zero. A timer field's
presence does not prove a drive-side watchdog or its reaction. Read and verify
actual PDO configuration before decoding; bind mappings to an epoch invalidated
by boot-up, reset, node change or remapping. Test lengths, unknown mappings,
reserved bits and payload endianness. Do not infer atomic multi-object actuation
or cross-node synchronization from an EDS layout.

## Motion and completion: unresolved source conflicts

EN86/ZH86 p12 show enable controlword 6 -> 7 -> 15 with about 10 ms between
writes. Implement this as bounded externally clocked state advancement with
observed status and deadlines, not blocking sleeps or proof of acceptance.
Position start uses rising bit4, bit5 immediate change, bit6 relative:
15 -> 31 absolute; 15 -> 95 relative; 15 -> 63/127 immediate absolute/relative.
The correct bit4 rearm and acceptance correlation must be proven for the actual
firmware, especially after timeout/reconnect. A stale completion bit cannot
complete a newly submitted move.

| Evidence | Alarm indication | Motion indication | Position accepted/completed |
| --- | --- | --- | --- |
| EN86 p12 | status bit7 | bit10 = 1 means zero speed | bit12 effective target; bit15 completed |
| ZH86 p12 | status bit3 | bit14 = 1 means running | bit15 completed; bit12 not described |
| ZH57 p12 | status bit7 | bit14 = 1 means running | bit15 completed |
| OP86/OP57 p10 | status bit7 | bit14 = 1 means running | bit15 completed |

These differences were confirmed by rendering EN86/ZH86 p12, not just text
extraction. All editions use bit8 for homing complete; EN86 p15 again uses bit10
zero speed, ZH86 p15 bit14 running. Do not assume a generic CiA 402 target-reached
bit10 is completion here. Do not OR all contradictory bits into a permissive
"ready" or "done" result. Preserve raw values and require a qualified model and
firmware status dialect, with separate communication, NMT, drive-state and
operation-outcome observations. The monitor word 2100 supplies corroborating
evidence but cannot independently settle undocumented timing guarantees.

**Velocity start differs:** ZH86/ZH57 p13 write target/ramp parameters before the
6/7/15 enable sequence and say the motor starts at enable. EN86 p13 enables
first, writes parameters, and then instructs bits4-6 to start, apparently reusing
the position procedure. This is consequential: never enable a drive with an
unknown/stale velocity target. Treat the sequence as unresolved until qualified.

**Stop:** ZH86/ZH57 p12 and OP86/OP57 p10 list 15 -> 271 (0x010F) stop and
15 -> 11 (0x000B) quick/emergency stop. EN86 p12 lists only the latter. 605D
exists, but supported option values, stop ramp, final state, and torque behavior
are not sufficiently specified. No 605A quick-stop option object or 6085
quick-stop deceleration appears in the EDS. Do not invent their presence.
Successful SDO transmission is not evidence of stopping. NMT stopped, EMCY
reception, CAN bus-off, halt and drive disable are distinct events/actions.

**Fault reset:** bit7 of 6040 must be set then cleared manually (EN86/ZH86 p15).
The alarm table p20 says overcurrent/overvoltage/undervoltage need restart, while
memory and following-error entries are marked resettable. Do not auto-reset,
retry motion, or promise all faults can be cleared by the controlword.

**Homing:** methods 17 negative limit, 18 positive limit, 24 positive home switch,
29 negative home switch (pp13-15). Method 29 finishes when leaving the home switch
on the reverse creep; it is not merely "find any edge." Homing bit4 rises to
start and falls to interrupt. No evidence here supports methods 35/37 or setting
home by pretending a position-clear is physical homing. Define reference
invalidation on reset, release, position clear, interrupted homing and uncertain
motion according to the common axis contract.

**Units:** target position is described in pulses; subdivision 2201 and encoder
2501 are distinct settings. Encoder lines, quadrature counts, command pulses,
shaft turns and mechanical travel must not be conflated. Although target speed
tables say rpm, actual 606C scaling needs readback/bench evidence. 6083/6084/609A
are time objects in this vendor dialect. A public acceleration API cannot write
its value directly; conversion requires a documented physical definition or must
return unsupported/unknown. Keep ramp-time configuration as an explicit native
capability. No ESS register or unit defaults belong in this profile.

Additional contradictions include EN86 p12/p13/p15 saying to "set" 6061, where
Chinese correctly says monitor and the mode table/EDS says RO; never write 6061.
EN86 p8 gives output function 2 running/3 homing-complete, while ZH86 dictionary
p19 gives 2 homing-complete/3 drive-state. These need per-firmware verification
before exposing brake or status output configuration.

## Communications, recovery and capability limits

SDO, PDO and EMCY are explicitly named (manual p3); the EDS adds producer
heartbeat and SYNC COB-ID. 1016 consumer heartbeat, 100C/100D node guarding,
1010/1011 standard save/restore, 6502 supported modes, LSS, and a documented
communication-loss stop configuration are absent from this source set. Absence
from a file is not proof of hardware impossibility; it is insufficient evidence
for support claims. In particular, watching a drive heartbeat on the host does
not make the drive stop when the host disappears.

EDS says SimpleBootUpSlave=1, DynamicChannelsSupported=0, LSS_Supported=0. The
documents do not specify full NMT transition, SDO timeout, abort, segmentation,
remapping, bus-off and heartbeat-consumer behavior. Implement necessary standard
CANopen service behavior from primary standards/stack documentation, and keep
device-specific assumptions separate. Use a standard 11-bit Classical CAN
adapter for the selected profile; do not convert CAN into an RTU byte stream.

Evaluate CANopenNode against the selected embedded port, revision, memory
ownership and license before choosing a protocol implementation. The application
owns stack storage, process calls, transport, clocks and scheduling; the library
owns bounded drive behavior and descriptors. If a reviewed minimum in-house
service subset is chosen, enumerate supported/unsupported transfer forms and
test all framing, timeout, abort and unsolicited-event paths. A small expedited
SDO example is not a complete CANopen stack.

## Required qualification and implementation consequences

Record each issue below as resolved by a source/trace, explicitly unsupported,
or still unknown; never silently replace it with generic CiA 402 behavior.

1. Collect actual 2000/2001, 1009/100A and 1018 identity values and a unit label;
   map them to model/firmware without destructive discovery. Firmware-matched
   manufacturer confirmation is still missing. Do not send factory reset or
   rewrite node/bitrate/PDO/heartbeat during a probe.
2. Read live PDO mapping/communication settings, control mode, monitor/status,
   units and limits. A missing heartbeat is inconclusive with 1017 default zero.
3. Resolve status dialect, full drive-state masks, bit4 acknowledgement timing,
   stale completion handling and velocity start sequence with trace fixtures.
   Compare English/Chinese behavior; do not make arbitrary majority decisions.
4. Qualify halt/quick-stop/disable during motion, busy SDO, queued frames and lost
   responses. Define stop admission and stale-frame retirement. Unknown outcomes
   remain unknown after reconnect; no automatic enable, restart or move replay.
5. Resolve subdivision and feedback scale, negative velocity, range endpoints,
   0 ramp-time meaning, ramp-time physics, limits and reference behavior. Use
   non-overflowing checked conversions and explicit validity/freshness metadata.
6. Determine drive-side host-loss detection and reaction. Without evidence, the
   library must report communication loss and uncertainty, not promise motion
   stops when disconnected. Keep any physical emergency stop in the application
   and machine design; protocol commands do not establish certified safety.
7. Verify save/reset behavior and persistence separately. Permit vendor tuning
   and destructive native writes only by explicit caller action with side-effect
   metadata; do not include them in automatic health recovery.
8. Run separate qualification for OL86-C, CL57-C, OL57-C and firmware variants.
   Shared documents justify reusable implementation candidates, not identical
   encoder feedback, current limits, status semantics or certified support.

These gaps should narrow capability flags and hardware claims, not prevent the
coding agent from finishing portable codecs, tested bounded services, catalogue
provenance, CLI/reporting, simulation, runners and adapter boundaries. Preserve
realistic fixtures for the documented variants and mark unresolved behavior
unqualified until physical evidence arrives.
