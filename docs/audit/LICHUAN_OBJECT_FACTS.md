# Lichuan C object and commissioning evidence

Research date: 2026-10-24. This is an extraction and crosscheck for the implementation prompts, not a firmware-qualified object ledger. It covers the external CANopen interface of the complete CL86-C/OL86-C drives. It is not an internal chip register map. Every default below is a document value, not an observation of the user's hardware.

The [2026-10-28 recheck](PROMPTSET_REVIEW_2026-10-28.md) adds the ZH57 terminal-label conflict and separates SDO access from PDO direction eligibility. Original sources and numeric inventory values were preserved.

## Sources and how to use this file

Source IDs match [the retained source manifest](../reference/SOURCES.json). Page numbers here are **one-based PDF pages**, including the cover and contents; printed page numbers are usually two lower.

| ID | Original retained source | Relevant pages or sections |
| --- | --- | --- |
| EDS | [LC_StepDriverCAN_V1.1.eds](../reference/vendor/LC_StepDriverCAN_V1.1.eds) | All named sections; FileInfo modification date 2022-11-11, file version/revision 1/1 |
| EN86 | [English 86C CANopen manual](../reference/vendor/86C_CANopen_user_manual_en.pdf) | PDF 3, 5-10, 15-20 |
| ZH86 | [Chinese 86C CANopen manual](../reference/vendor/86C_CANopen_user_manual_zh.pdf) | PDF 3, 5-10, 15-20 |
| OP86 | [86C operating manual](../reference/vendor/CL86C_OL86C_operating_manual_zh.pdf) | PDF 3-8, 13-14; internal headers incorrectly identify 57C |
| ZH57 | [Chinese 57C CANopen manual](../reference/vendor/57C_CANopen_user_manual_zh.pdf) | PDF 8 section 2.2.2 versus PDF 17 section 3.6.2; conflicting output labels, not an 86C terminal specification |

The [official Lichuan download index](https://www.xlichuan.com/download/23.html), rechecked on this date, still lists this EDS and both 86C manuals. The documents have no established correspondence to a particular installed firmware. The EDS SHA256 is `dd3c96e7282c012a522dddb5bbdbbf561c1f5c23716b31f12472f45829d4f52c`.

[The indexed extraction](../reference/extracted/LC_StepDriverCAN_V1.1.object_inventory.json) preserves every EDS object field as a string, section name, and original line span. It contains all 99 index headers and all 202 scalar entries. Repeated fields are deliberate: headers describe objects; scalar entries describe transferable values. Expressions such as `$NODEID+0x600` remain expressions. A missing default or range is absent, not zero. This extraction is input to review; it must not directly grant access to objects.

For each implemented field, open its EDS section and the cited original PDF page, resolve the model/firmware applicability and conflicts, then put the decision into the single reviewed implementation ledger. Recheck PDF images where a table is ambiguous. Correct the ledger, generated declarations, tests and affected documentation together when new evidence changes a decision. A code example, successful SDO transfer or readback can corroborate a value; none proves units, physical effects or safe write semantics on its own. Do not settle conflicting addresses by trying both writes on a live drive.

## Inventory facts

The EDS lists 3 mandatory, 53 manufacturer and 43 optional object indexes: **99 distinct indexes**. Twenty-three are record headers. The **202 scalar entries** comprise 42 U8, 91 U16, 61 U32, 1 I16 and 7 I32 values; 34 are labelled RO, 166 RW and 2 WO. Ninety have `PDOMapping=1`; 112 have 0. Sixty-five scalar entries have no default. Only ten entries have explicit limits: `1003:00`, `1018:00`, and the eight PDO mapping counts. No tuning scalar has an EDS operating range. A U16 representation does not establish that all 0..65535 values are valid settings.

All `ObjFlags` are zero. EDS record `SubNumber` includes the count entry: for example `2400` has `SubNumber=0x11`, 17 entries including `00`; its count entry is unusually U16 with default 16. Do not impose a global U8 count codec. `1018` has entries 00..03 only: default count 3, even though its count HighLimit says 4. No serial-number entry `1018:04` is declared.

## Communication and PDO facts: prompts 11-18, 37-38

| Object or metadata | EDS fact | Implementation limit |
| --- | --- | --- |
| `1000:00` | RO U32, default `0x40192` | Preserve raw device type; it does not uniquely identify CL86-C firmware |
| `1001:00` | RO U8, default 0 | Error register, separate from drive alarm and health |
| `1003:00` and `:01..04` | Count RO U8, range 0..4, default 0; four RO U32 entries, no defaults | Do not copy the generic CANopen clear-error-history write to this RO count without evidence |
| `1005:00` | RW U32, default `0x80` | SYNC COB-ID; existence is not proof of every synchronous mode or timer |
| `1009:00`, `100A:00` | RO U16, defaults `0x2608`, 1 | These are numeric U16 in this EDS/manual, not generic visible strings |
| `1014:00` | RW U32, default `$NODEID+0x80` | EMCY identifier; retain raw events and invalidate stale interpretation after reconfiguration |
| `1017:00` | RW U16, default 0 | Producer heartbeat disabled by its CANopen zero convention; silence alone cannot prove offline. EDS does not document a drive-side host-loss stop |
| `1018:01/02/03` | RO U32, defaults `0x1000`, 1, 0 | Vendor/product/revision; shared family metadata is not automatic profile identification |
| `1200:01/02` | RW U32, defaults `$NODEID+0x600`, `$NODEID+0x580`; count RO U8 default 2 | An SDO channel change changes routing; no assumption that traffic always uses default IDs |
| DeviceInfo | `SimpleBootUpSlave=1`, `SimpleBootUpMaster=0`, `DynamicChannelsSupported=0`, `GroupMessaging=0`, `LSS_Supported=0`, `Granularity=0x08` | Evidence for this EDS only; it does not describe every reset or startup transition |
| BaudRate flags | 50, 125, 250, 500 and 1000 kbit/s set; 10, 20 and 800 unset | The 50 kbit/s flag has no matching manual DIP selection; do not invent a software bitrate object |
| PDO counts | `NrOfRXPDO=4`, `NrOfTXPDO=4` | Lichuan profile capacity, not the common-library maximum |

Absent from this EDS: `1006` SYNC period, `100C/100D` node guarding, `1010/1011` generic save/restore, `1016` heartbeat consumer, `605A` quick-stop option, `6085` quick-stop deceleration and `6502` supported modes. Absence is not a measured unsupported response, nor permission to implement them using another manufacturer's manual. Existing prompts discussing them must preserve that distinction.

For each `k=0..3`, communication records `1400+k` and `1800+k` have: `:00` RO U8 default 5; `:01` RW U32 COB-ID; `:02` RW U8 transmission type default 255; `:03` RW U16 inhibit value default 0; `:04` RW U8 compatibility entry default 0; `:05` RW U16 event value. Receive event defaults are 1000; transmit event defaults are 100. These are **raw EDS values**. Standard CANopen timing conventions and a stack implementation are separate evidence; do not claim that an RPDO event value is a CL86-C host-loss motor watchdog or that every type accepted by the integer codec is implemented by this firmware.

| PDO | Communication index and default ID | Mapping record | Default count and words | Decoded default payload length |
| --- | --- | --- | --- | --- |
| RPDO1 | `1400`, node+`0x200` | `1600` | 3: `60400010`, `60FF0020`, `60600008` | 7 bytes: control U16, target velocity I32, mode U8 |
| RPDO2 | `1401`, node+`0x300` | `1601` | 2: `607A0020`, `60810020` | 8 bytes: target position I32, profile velocity U32 |
| RPDO3 | `1402`, node+`0x400` | `1602` | 0; no defaults for map slots | No active mapping declared |
| RPDO4 | `1403`, node+`0x500` | `1603` | 0; no defaults for map slots | No active mapping declared |
| TPDO1 | `1800`, node+`0x180` | `1A00` | 3: `60410010`, `60FD0020`, `60610008` | 7 bytes: status U16, digital inputs U32, observed mode U8 |
| TPDO2 | `1801`, node+`0x280` | `1A01` | 2: `60640020`, `606C0020` | 8 bytes: actual position I32, actual velocity I32 |
| TPDO3 | `1802`, node+`0x380` | `1A02` | 0; no defaults for map slots | No active mapping declared |
| TPDO4 | `1803`, node+`0x480` | `1A03` | 0; no defaults for map slots | No active mapping declared |

Each mapping count is RW U8 with range 0..4; each record exposes four RW U32 mapping slots. The decoded lengths above are arithmetic consequences of the EDS words, not observations of the drive. Validate active mapping and COB-ID before accepting a PDO layout. A zero count does not make invented entries valid. `PDOMapping=1` on a factory action is not authorization to include that action in periodic PDO traffic.

SDO access and PDO direction eligibility are separate facts. EDS `6040` and `6060` are WO with `PDOMapping=1`, and appear in default RPDO1. Do not require an SDO upload of mapped payload targets to validate an RPDO. Reading supported communication/mapping records is a different operation. The [pinned CANopenNode implementation](LICHUAN_PUBLIC_IMPLEMENTATIONS.md) checks RPDO/TPDO attributes separately in `CO_PDO.c` lines 119-131; that corroborates the distinction, not Lichuan's unresolved action permissions.

## Typed reads, basic settings and access conflicts: prompts 19-20

| Object | Concrete manual meaning | EDS conflict or constraint |
| --- | --- | --- |
| `2000:00`, `2001:00` | Drive model code/version, U16 (EN86 p17; ZH86 p17) | EDS defaults 1/0; no published code-to-model/firmware table found |
| `2100:00` | U16 motion monitor: bit0 in-position, bit1 homed, bit2 running, bit3 alarm, bit4 released (`0` enabled, `1` released), bit5 positive soft limit, bit6 negative soft limit (EN86 p17; ZH86 p18) | Manuals RO; EDS RW with default 8. Never write the monitor solely from the EDS flag; default 8 is not a boot observation |
| `2101:00` | U16, bits 0..6 logical active states of DI0..DI6 (same pages) | Manuals RO; EDS RW, default 0; no read-to-clear behavior established |
| `2102:00` | U16, bits 0..2 logical active states of DO0..DO2 (same pages) | Manuals RO; EDS RW, default 0; distinct from `60FE` commands |
| `2200:00` | Default direction; text says `0: Pulse Sign`, `1: Pulse /Sign` (EN86/ZH86 p18) | U16 RW default 0; text does not establish which shaft direction is positive. Do not reinterpret as a known clockwise flag |
| `2201:00` | Subdivision setting 400..51200 (EN86/ZH86 p18) | U16 RW default 0 in EDS, outside the documented range. Determine whether every integer or a discrete set is supported; do not invent a fixed encoder-count conversion |
| `2202:00` | Software-limit enable: 0 disabled, 1 effective after homing (EN86/ZH86 p18) | U16 RW default 10 in EDS, inconsistent with documented choices. Do not write 10 or infer an undocumented bitfield |
| `2203:00` | CAN node setting 0..127 (EN86/ZH86 p18) | U16 RW default 0. The document's parameter range is not a valid unicast-node range; 0 is not an ordinary target. Activation/reset/save/DIP precedence need explicit evidence |

EDS labels `603F` RO but manuals list RW; `6040` WO but manuals RW; `6060` WO U8 but manuals RW I8; `6061` RO U8 but manuals RW I8; `6098` RW U8 but manuals I8. Public intent must respect observed-mode read-only meaning and preserve raw bytes while applicability is unresolved. Do not turn the manual's instruction to "set" a monitored mode into an actual write to `6061`. `607A` has the printed type typo `I3C`; EDS says I32. Each resolution needs an explicit ledger record, not a silent parser repair.

The monitor words and error-register readings are not an atomic motor snapshot. Keep observation time, raw values, validity and configuration generation, as the RS read contexts do. RO/RW access flags do not prove non-consuming reads. No destructive CL86-C read is established by these sources; do not invent one, and do not presume generic upload is always harmless.

## I/O: prompt 21 and the evidence gate

**There is a real, visually checked address contradiction.** EN86 PDF p8 and p18, ZH86 p8, and OP86 p6 put output level at `2400:0B` and DO0/1/2 assignment at `:0C/:0D/:0E`. EDS and the ZH86 p19 object table put output level at `:0C` and DO0/1/2 at `:0D/:0E/:0F`. The ZH86 manual disagrees with itself. EN86 p8/p18 and ZH86 p19 rendered pages were inspected; this is not just text-extraction drift. Keep both claims, do not choose by majority, and gate typed writes until the exact firmware is resolved.

| Function | EDS section/value | Manual claim |
| --- | --- | --- |
| Count | `2400:00`, RO U16, default 16 | No basis for assuming U8 count |
| DI polarity | `2400:01`, RW U16 default 0 | Bits0..6 invert corresponding logical input; 0 default,1 reversal (EN86 p18, ZH86 p18) |
| DI0..DI6 assignments | `2400:02..08`, RW U16 defaults all 0 | EN86/ZH86 p8: DI0=1 origin, DI1=2 positive limit, DI2=3 negative limit; remaining0 |
| Reserved/filter entries | EDS `:09/:0A/:0B` labelled reserved, all RW U16 default 0 | EN86 p8/p18, ZH86 p8/p19 and OP86 p6 call `:0A` an input filter coefficient; `:0B` also overlaps the output-level conflict |
| Output level | EDS `:0C`, RW U16 default 0 | Manual address conflict above; table describes bits 0/1 only despite three outputs, so do not silently invent bit 2 semantics |
| DO0..DO2 assignments | EDS `:0D/:0E/:0F`, RW U16 defaults all 0 | Manual address conflict; wiring table gives DO0 default 1 alarm, DO1/DO2 default 0 |
| Trailing reserved | EDS `:10`, RW U16 default 0 | Reserved is not a typed configurable feature |
| DI filters | `2401..2407:00`, RW U16, EDS defaults 50,40,100,4000,10,2,16 | EN86/ZH86 p8 and OP86 p6-7 give all 2; no filter-time unit or coefficient equation is specified |

DI assignment values in EN86/ZH86 p8 and OP86 p7: 0 undefined, 1 origin, 2 positive limit, 3 negative limit, 4 motor enable/MF, 5 stop, 6 emergency stop, 7..13 user-defined inputs 0..6. The electrical active state and motor-enable polarity still require the real wiring/configuration; do not automatically disable a missing signal.

DO values have a second contradiction: EN86/ZH86 p8 and OP86 p7 say 0 undefined, 1 alarm, **2 running**, **3 homing complete**, 4 in-position, 5 brake release, 9..11 user-defined outputs 0..2. EN86 p18 and ZH86 p19 instead say **2 homing complete**, **3 drive status**. No choices 6..8 are documented. Keep unresolved semantic choices out of a purportedly verified enum mapping.

OP86 p7 documents user-defined outputs through `60FE:01` physical outputs and `60FE:02` mask, with DO0/1/2 at bits 16/17/18; both corresponding bits must be 1 for an output. EDS confirms both subentries RW U32 and PDO-mappable, with no defaults. Treat this as an OP86-specific claim whose internal 57C headers weaken applicability; audit real firmware and output assignment before use. `2102` output-status bits 0..2 and `60FE` command bits 16..18 are different layouts. Do not guess a complete `60FD` function-to-bit mapping from either one.

The 57C manual also contradicts itself: ZH57 PDF p8 section 2.2.2 labels those `60FE` bits 16/17/18 **DO0/DO1/DO2**, while PDF p17 section 3.6.2 labels both `60FE:01` and `:02` **DO1/DO2/DO3**. Both original rendered tables were checked on 2026-10-28. Preserve both claims and bind the actual terminal identity explicitly; do not silently renumber, infer an extra physical output from the DO3 label, or apply the 57C interpretation to 86C without model/firmware evidence.

## Tuning: prompt 22

Every `2500..2520:00` entry is RW U16, PDO-mappable, with **no EDS default or range**. EN86 p19-20 and ZH86 p19-20 document only through `2519`. The EDS alone supplies names for `251A..2520`; their scaling and algorithm meaning remain unverified.

| Index | Manual meaning or EDS-only name | Documented unit/constraint |
| --- | --- | --- |
| `2500` | Drive operation mode | 0 ineffective,1 open loop,2 closed loop; distinct from `6060` motion mode |
| `2501` | Encoder resolution | No explicit line/count/quadrature convention or range |
| `2502` | Maximum peak current | mA; motor/drive rating is a separate bound |
| `2503` | Closed-loop maximum current ratio | Percent; no numeric operating range |
| `2504` | Closed-loop base current ratio | Percent; no numeric operating range |
| `2505` | Open-loop maximum current ratio | Percent; no numeric operating range |
| `2506` | Holding/locking current ratio | Percent; no numeric operating range |
| `2507` | Holding/locking current time | ms; no numeric operating range |
| `2508` | Low-pass filter enable | Factory tuning, normally left unchanged; no Boolean value set is specified |
| `2509` | Low-pass filter coefficient | No equation or physical unit |
| `250A` | Position-deviation alarm threshold | No scale supplied |
| `250B` | In-position threshold | No scale supplied |
| `250C` | In-position time | Name implies time but no unit supplied; do not infer ms from `2507` |
| `250D` | Averaging-filter coefficient | No equation or physical unit |
| `250E` | Current-loop proportional factor | Factory tuning |
| `250F` | Current-loop Kp | Factory tuning |
| `2510` | Current-loop Ki | Factory tuning |
| `2511` | Current-loop Kc | Factory tuning |
| `2512` | LA velocity Kp1 | Factory tuning |
| `2513` | LA velocity Kv1 | Factory tuning |
| `2514` | Velocity node 1 | No velocity scale supplied |
| `2515` | LA velocity Kp2 | Factory tuning |
| `2516` | LA velocity Kv2 | Factory tuning |
| `2517` | Velocity node 2 | No velocity scale supplied |
| `2518` | Velocity feedforward Kvf | Factory tuning |
| `2519` | Position-loop Ki gain | Factory tuning |
| `251A` | EDS `Obj.v_kp` | EDS-only name; no reviewed semantic contract |
| `251B` | EDS `Obj.v_ki` | EDS-only name; no reviewed semantic contract |
| `251C` | EDS `Obj.v_kc` | EDS-only name; no reviewed semantic contract |
| `251D` | EDS `Obj.v_kvfbd1` | EDS-only name; no reviewed semantic contract |
| `251E` | EDS `Obj.v_kvfbd2` | EDS-only name; no reviewed semantic contract |
| `251F` | EDS `Obj.v_kvff` | EDS-only name; no reviewed semantic contract |
| `2520` | EDS `Obj.v_kacff` | EDS-only name; no reviewed semantic contract |

Keep known raw widths and unknown physical meaning separate. Typed native coverage can expose an explicitly documented native setting without claiming an SI conversion; it cannot turn an opaque EDS-only name into a verified tuning algorithm. Do not choose substitute defaults or tune automatically. Source gaps remain obligations in coverage and the backlog.

## Node setup, electrical facts and persistence: prompts 37-39, 47

EN86/ZH86 p9-10 and OP86 p7-8 show eight DIP switches. SW1..SW5 encode nodes 1..31 using weights 1,2,4,8,16; all OFF is user-defined, not node 0. SW6/SW7 select 125k=ON/ON, 250k=OFF/ON, 500k=ON/OFF, 1M=OFF/OFF. SW8 ON connects 120 ohms across the bus. EN86/ZH86 p3 advertise up to 32 axes; this is a product claim, not a reason to cap all CANopen profiles at 32 or a node-address rule. Manual recommendations are 25 m at 1 Mbit/s, 100 m at 500 kbit/s, 250 m at 250 kbit/s, 500 m at 125 kbit/s. These are vendor wiring recommendations, not a measured signal-integrity guarantee or universal CAN limits.

EN86/ZH86 p5 and p9 identify RJ45 pins 1 CAN_H, 2 CAN_L, 3 GND; 4..8 unused. Two RJ45 connectors do not imply two independently addressable CAN channels or CAN FD. The English p3 `2 x 1 Mbps (half-duplex)` wording does not establish a 2 Mbit/s mode. p4 even labels termination RS485; OP86 p3 labels the CAN port RS485. Preserve those editorial defects as source-quality evidence.

EN86 p6-7 gives seven 24 V inputs with DICOM permitting common anode/cathode and three outputs whose DOCOM must connect to 0 V. OP86 p5 says the motor is locked on power-up. This is a hardware-behavior claim, not permission for library startup actuation. Supply specifications conflict **within EN86**: p3 lists 18..80 VAC / 36..110 VDC, while p6-7 wiring says 24..50 VDC. Do not select a bench supply or publish one merged operating range from these pages; use the actual drive nameplate and applicable manufacturer confirmation.

| Action | Documented command | What remains unproved |
| --- | --- | --- |
| Save parameters | `2300:00=2`, U16; SDO explicitly stated (EN86/ZH86 p15; OP86 p13) | Completion indication, minimum repeat interval, maximum execution time, endurance, full saved-field list and power-loss behavior |
| Restore factory settings | `2300:00=1`;0 ineffective (EN86/ZH86 p18) | Activation/reset timing, node/bitrate/PDO effects, response availability, completion indication |
| Clear alarm | `2301:00=1`,0 ineffective (EN86 p18; EDS) and controlword bit7 asserted then manually cleared (EN86/ZH86 p15) | ZH86 object table omits2301; not all alarm types are resettable; no equivalence to arbitrary recovery |
| Clear position | `2302:00` rising0-to1, then manually restore0 (EN86/ZH86 p15) | No documented arbitrary-position argument; not proof of homing, coordinate origin or safe concurrent motion |

EN86/ZH86 p20 list five LED patterns per 5 seconds: 1 overcurrent, 2 overvoltage, 3 undervoltage, 4 memory R/W error, 5 position error. The first three require restart; the latter two are listed resettable. This 5 s cadence is a diagnostic indicator period, not a CAN service timeout or fault-clear delay. The object dictionary explicitly identifies only alarm codes `FF01` overcurrent and `FF05` position deviation (EN86 p16; ZH86 p16); do not infer FF02..FF04 from LED counts without further evidence.

No retained document establishes SDO response maxima, minimum SDO inter-request gaps, flash-write delay, reboot duration, PDO remap settlement, host-loss stop or bus-off recovery timing. Keep these as explicit unknowns. The motion manuals' approximate 10 ms controlword spacing belongs to the enable sequence, not a generic CAN delay or proof of readiness.

## Prompt integration and review obligations

| Prompts | Required concrete work from this evidence |
| --- | --- |
| 2-3 | Import all 99 indexes/202 scalars into a review ledger; record each conflict and absence; establish firmware bindings before resolving them |
| 11-12 | Preserve U8/U16/U32/I16/I32 widths, numeric version objects, U16 `2400:00`, expression defaults and missing fields; generate only from reviewed entries |
| 13-18 | Use actual configurable SDO/PDO identifiers, exact mapping words and counts; separate standard protocol rules from measured CL86-C behavior |
| 19-20 | Preserve raw monitoring evidence; resolve access/default/direction/subdivision conflicts before typed write/physical conversion policy |
| 21 | Resolve DO subindex and function2/3 conflicts; never probe alternative output writes; retain separate input/output logical layouts and unknown filter units |
| 22 | Cover all 33 tuning indexes, including seven EDS-only tail entries as unresolved obligations; do not invent units/ranges/defaults |
| 29 | Obtain narrowly authorized evidence for exact firmware, address layout, changed-value behavior and timing; no production-capability claim from document-only tables |
| 37-39 | Reconcile identity/node/DIP routing, lack of software bitrate evidence, and vendor save/reset actions; preserve uncertain persistence outcomes |
| 41,47-48 | CLI and bench plans expose the same capability gates and conflicts; all electrical and actuation prerequisites remain explicit |

The read-semantics comparison was refreshed against clean siblings on this date: MotorControl-RS `bd831c859daa9c8c07b8d27a2dd879bb4e750511`, ADS1115 `235af3e8c722661a89a44766a4ccd903a654f6fe`, INA228 `b28c5ea69bcb4d26b351c52c24d3c6fae9fd282b`, FieldCore-node `a002abae533b0aa767c235c2197d3b548bdcdd83`. RS `include/MotorControlRS/profiles/ess_rs/Reads.h` identifies three reviewed non-consuming windows and keeps provenance/uncertainty; ADS1115 `include/ADS1115/Config.h` documents conversion reads clearing a latched alert; INA228 `include/INA228/INA228.h` distinguishes preserved snapshots from explicit destructive `readAndClearDiagAlert`. These are reasons to classify each CAN object's read effects; they are not evidence of a particular CL86-C read side effect.

## Complete EDS scalar table

The following table is mechanically extracted, not a resolved access policy. A dash means the EDS omits the field. `PDO` is the original 0/1 mapping flag. Every row links to a section and line span through the indexed JSON; the exact vendor parameter name is retained there. Record headers, device metadata and supported-object lists are also retained in that JSON.

| Object | EDS section / line | Type / access | PDO | Default | Low / high | Vendor name |
| --- | --- | --- | --- | --- | --- | --- |
| `1000:00` | `1000` / 158 | U32 / RO | 0 | `0x40192` | - / - | Device Type |
| `1001:00` | `1001` / 167 | U8 / RO | 0 | `0` | - / - | Error Register |
| `1003:00` | `1003sub0` / 182 | U8 / RO | 0 | `0` | 0 / 4 | HighestSubIndex |
| `1003:01` | `1003sub1` / 193 | U32 / RO | 0 | `-` | - / - | Pre-defined Error Field_sub1 |
| `1003:02` | `1003sub2` / 201 | U32 / RO | 0 | `-` | - / - | Pre-defined Error Field_sub2 |
| `1003:03` | `1003sub3` / 209 | U32 / RO | 0 | `-` | - / - | Pre-defined Error Field_sub3 |
| `1003:04` | `1003sub4` / 217 | U32 / RO | 0 | `-` | - / - | Pre-defined Error Field_sub4 |
| `1005:00` | `1005` / 225 | U32 / RW | 0 | `0x80` | - / - | SYNC COB ID |
| `1009:00` | `1009` / 234 | U16 / RO | 0 | `0x2608` | - / - | hardware version |
| `100A:00` | `100A` / 243 | U16 / RO | 0 | `1` | - / - | software version |
| `1014:00` | `1014` / 252 | U32 / RW | 0 | `$NODEID+0x80` | - / - | Emergency COB ID |
| `1017:00` | `1017` / 261 | U16 / RW | 0 | `0` | - / - | Producer Heartbeat Time |
| `1018:00` | `1018sub0` / 276 | U8 / RO | 0 | `3` | 1 / 4 | number of entries |
| `1018:01` | `1018sub1` / 287 | U32 / RO | 0 | `0x00001000` | - / - | Vendor ID |
| `1018:02` | `1018sub2` / 296 | U32 / RO | 0 | `0x00000001` | - / - | Product code |
| `1018:03` | `1018sub3` / 305 | U32 / RO | 0 | `0x00000000` | - / - | Revision number |
| `1200:00` | `1200sub0` / 320 | U8 / RO | 0 | `2` | - / - | HighestSubIndex |
| `1200:01` | `1200sub1` / 329 | U32 / RW | 0 | `$NODEID+0x600` | - / - | COB ID Client to Server |
| `1200:02` | `1200sub2` / 338 | U32 / RW | 0 | `$NODEID+0x580` | - / - | COB ID Server to Client |
| `1400:00` | `1400sub0` / 353 | U8 / RO | 0 | `5` | - / - | HighestSubIndex |
| `1400:01` | `1400sub1` / 362 | U32 / RW | 0 | `$NODEID+0x200` | - / - | COB-ID used by RPDO |
| `1400:02` | `1400sub2` / 371 | U8 / RW | 0 | `255` | - / - | Transmission Type |
| `1400:03` | `1400sub3` / 380 | U16 / RW | 0 | `0` | - / - | Inhibit Time |
| `1400:04` | `1400sub4` / 389 | U8 / RW | 0 | `0` | - / - | Compatibility Entry |
| `1400:05` | `1400sub5` / 398 | U16 / RW | 0 | `1000` | - / - | Event Timer |
| `1401:00` | `1401sub0` / 413 | U8 / RO | 0 | `5` | - / - | HighestSubIndex |
| `1401:01` | `1401sub1` / 422 | U32 / RW | 0 | `$NODEID+0x300` | - / - | COB-ID used by RPDO |
| `1401:02` | `1401sub2` / 431 | U8 / RW | 0 | `255` | - / - | Transmission Type |
| `1401:03` | `1401sub3` / 440 | U16 / RW | 0 | `0` | - / - | Inhibit Time |
| `1401:04` | `1401sub4` / 449 | U8 / RW | 0 | `0` | - / - | Compatibility Entry |
| `1401:05` | `1401sub5` / 458 | U16 / RW | 0 | `1000` | - / - | Event Timer |
| `1402:00` | `1402sub0` / 473 | U8 / RO | 0 | `5` | - / - | HighestSubIndex |
| `1402:01` | `1402sub1` / 482 | U32 / RW | 0 | `$NODEID+0x400` | - / - | COB-ID used by RPDO |
| `1402:02` | `1402sub2` / 491 | U8 / RW | 0 | `255` | - / - | Transmission Type |
| `1402:03` | `1402sub3` / 500 | U16 / RW | 0 | `0` | - / - | Inhibit Time |
| `1402:04` | `1402sub4` / 509 | U8 / RW | 0 | `0` | - / - | Compatibility Entry |
| `1402:05` | `1402sub5` / 518 | U16 / RW | 0 | `1000` | - / - | Event Timer |
| `1403:00` | `1403sub0` / 533 | U8 / RO | 0 | `5` | - / - | HighestSubIndex |
| `1403:01` | `1403sub1` / 542 | U32 / RW | 0 | `$NODEID+0x500` | - / - | COB-ID used by RPDO |
| `1403:02` | `1403sub2` / 551 | U8 / RW | 0 | `255` | - / - | Transmission Type |
| `1403:03` | `1403sub3` / 560 | U16 / RW | 0 | `0` | - / - | Inhibit Time |
| `1403:04` | `1403sub4` / 569 | U8 / RW | 0 | `0` | - / - | Compatibility Entry |
| `1403:05` | `1403sub5` / 578 | U16 / RW | 0 | `1000` | - / - | Event Timer |
| `1600:00` | `1600sub0` / 593 | U8 / RW | 0 | `3` | 0 / 4 | HighestSubIndex |
| `1600:01` | `1600sub1` / 604 | U32 / RW | 0 | `0x60400010` | - / - | Object 1600_sub1 |
| `1600:02` | `1600sub2` / 613 | U32 / RW | 0 | `0x60ff0020` | - / - | Object 1600_sub2 |
| `1600:03` | `1600sub3` / 622 | U32 / RW | 0 | `0x60600008` | - / - | Object 1600_sub3 |
| `1600:04` | `1600sub4` / 631 | U32 / RW | 0 | `-` | - / - | Object 1600_sub4 |
| `1601:00` | `1601sub0` / 645 | U8 / RW | 0 | `2` | 0 / 4 | HighestSubIndex |
| `1601:01` | `1601sub1` / 656 | U32 / RW | 0 | `0x607a0020` | - / - | Object 1601_sub1 |
| `1601:02` | `1601sub2` / 665 | U32 / RW | 0 | `0x60810020` | - / - | Object 1601_sub2 |
| `1601:03` | `1601sub3` / 674 | U32 / RW | 0 | `-` | - / - | Object 1601_sub3 |
| `1601:04` | `1601sub4` / 682 | U32 / RW | 0 | `-` | - / - | Object 1601_sub4 |
| `1602:00` | `1602sub0` / 696 | U8 / RW | 0 | `0` | 0 / 4 | HighestSubIndex |
| `1602:01` | `1602sub1` / 707 | U32 / RW | 0 | `-` | - / - | Object 1602_sub1 |
| `1602:02` | `1602sub2` / 715 | U32 / RW | 0 | `-` | - / - | Object 1602_sub2 |
| `1602:03` | `1602sub3` / 723 | U32 / RW | 0 | `-` | - / - | Object 1602_sub3 |
| `1602:04` | `1602sub4` / 731 | U32 / RW | 0 | `-` | - / - | Object 1602_sub4 |
| `1603:00` | `1603sub0` / 745 | U8 / RW | 0 | `0` | 0 / 4 | HighestSubIndex |
| `1603:01` | `1603sub1` / 756 | U32 / RW | 0 | `-` | - / - | Object 1603_sub1 |
| `1603:02` | `1603sub2` / 764 | U32 / RW | 0 | `-` | - / - | Object 1603_sub2 |
| `1603:03` | `1603sub3` / 772 | U32 / RW | 0 | `-` | - / - | Object 1603_sub3 |
| `1603:04` | `1603sub4` / 780 | U32 / RW | 0 | `-` | - / - | Object 1603_sub4 |
| `1800:00` | `1800sub0` / 794 | U8 / RO | 0 | `5` | - / - | HighestSubIndex |
| `1800:01` | `1800sub1` / 803 | U32 / RW | 0 | `$NODEID+0x180` | - / - | COB-ID used by TPDO |
| `1800:02` | `1800sub2` / 812 | U8 / RW | 0 | `255` | - / - | Transmission Type |
| `1800:03` | `1800sub3` / 821 | U16 / RW | 0 | `0` | - / - | Inhibit Time |
| `1800:04` | `1800sub4` / 830 | U8 / RW | 0 | `0` | - / - | Compatibility Entry |
| `1800:05` | `1800sub5` / 839 | U16 / RW | 0 | `100` | - / - | Event Timer |
| `1801:00` | `1801sub0` / 854 | U8 / RO | 0 | `5` | - / - | HighestSubIndex |
| `1801:01` | `1801sub1` / 863 | U32 / RW | 0 | `$NODEID+0x280` | - / - | COB-ID used by TPDO |
| `1801:02` | `1801sub2` / 872 | U8 / RW | 0 | `255` | - / - | Transmission Type |
| `1801:03` | `1801sub3` / 881 | U16 / RW | 0 | `0` | - / - | Inhibit Time |
| `1801:04` | `1801sub4` / 890 | U8 / RW | 0 | `0` | - / - | Compatibility Entry |
| `1801:05` | `1801sub5` / 899 | U16 / RW | 0 | `100` | - / - | Event Timer |
| `1802:00` | `1802sub0` / 914 | U8 / RO | 0 | `5` | - / - | HighestSubIndex |
| `1802:01` | `1802sub1` / 923 | U32 / RW | 0 | `$NODEID+0x380` | - / - | COB-ID used by TPDO |
| `1802:02` | `1802sub2` / 932 | U8 / RW | 0 | `255` | - / - | Transmission Type |
| `1802:03` | `1802sub3` / 941 | U16 / RW | 0 | `0` | - / - | Inhibit Time |
| `1802:04` | `1802sub4` / 950 | U8 / RW | 0 | `0` | - / - | Compatibility Entry |
| `1802:05` | `1802sub5` / 959 | U16 / RW | 0 | `100` | - / - | Event Timer |
| `1803:00` | `1803sub0` / 974 | U8 / RO | 0 | `5` | - / - | HighestSubIndex |
| `1803:01` | `1803sub1` / 983 | U32 / RW | 0 | `$NODEID+0x480` | - / - | COB-ID used by TPDO |
| `1803:02` | `1803sub2` / 992 | U8 / RW | 0 | `255` | - / - | Transmission Type |
| `1803:03` | `1803sub3` / 1001 | U16 / RW | 0 | `0` | - / - | Inhibit Time |
| `1803:04` | `1803sub4` / 1010 | U8 / RW | 0 | `0` | - / - | Compatibility Entry |
| `1803:05` | `1803sub5` / 1019 | U16 / RW | 0 | `100` | - / - | Event Timer |
| `1A00:00` | `1A00sub0` / 1034 | U8 / RW | 0 | `3` | 0 / 4 | HighestSubIndex |
| `1A00:01` | `1A00sub1` / 1045 | U32 / RW | 0 | `0x60410010` | - / - | Object 1A00_sub1 |
| `1A00:02` | `1A00sub2` / 1054 | U32 / RW | 0 | `0x60fd0020` | - / - | Object 1A00_sub2 |
| `1A00:03` | `1A00sub3` / 1063 | U32 / RW | 0 | `0x60610008` | - / - | Object 1A00_sub3 |
| `1A00:04` | `1A00sub4` / 1072 | U32 / RW | 0 | `-` | - / - | Object 1A00_sub4 |
| `1A01:00` | `1A01sub0` / 1086 | U8 / RW | 0 | `2` | 0 / 4 | HighestSubIndex |
| `1A01:01` | `1A01sub1` / 1097 | U32 / RW | 0 | `0x60640020` | - / - | Object 1A01_sub1 |
| `1A01:02` | `1A01sub2` / 1106 | U32 / RW | 0 | `0x606c0020` | - / - | Object 1A01_sub2 |
| `1A01:03` | `1A01sub3` / 1115 | U32 / RW | 0 | `-` | - / - | Object 1A01_sub3 |
| `1A01:04` | `1A01sub4` / 1123 | U32 / RW | 0 | `-` | - / - | Object 1A01_sub4 |
| `1A02:00` | `1A02sub0` / 1137 | U8 / RW | 0 | `0` | 0 / 4 | HighestSubIndex |
| `1A02:01` | `1A02sub1` / 1148 | U32 / RW | 0 | `-` | - / - | Object 1A02_sub1 |
| `1A02:02` | `1A02sub2` / 1156 | U32 / RW | 0 | `-` | - / - | Object 1A02_sub2 |
| `1A02:03` | `1A02sub3` / 1164 | U32 / RW | 0 | `-` | - / - | Object 1A02_sub3 |
| `1A02:04` | `1A02sub4` / 1172 | U32 / RW | 0 | `-` | - / - | Object 1A02_sub4 |
| `1A03:00` | `1A03sub0` / 1186 | U8 / RW | 0 | `0` | 0 / 4 | HighestSubIndex |
| `1A03:01` | `1A03sub1` / 1197 | U32 / RW | 0 | `-` | - / - | Object 1A03_sub1 |
| `1A03:02` | `1A03sub2` / 1205 | U32 / RW | 0 | `-` | - / - | Object 1A03_sub2 |
| `1A03:03` | `1A03sub3` / 1213 | U32 / RW | 0 | `-` | - / - | Object 1A03_sub3 |
| `1A03:04` | `1A03sub4` / 1221 | U32 / RW | 0 | `-` | - / - | Object 1A03_sub4 |
| `2000:00` | `2000` / 1229 | U16 / RO | 1 | `1` | - / - | Obj.driverID |
| `2001:00` | `2001` / 1238 | U16 / RO | 1 | `0` | - / - | Obj.softversion |
| `2100:00` | `2100` / 1247 | U16 / RW | 1 | `8` | - / - | Obj.MonitorWord |
| `2101:00` | `2101` / 1256 | U16 / RW | 1 | `0` | - / - | Obj.DiStatus |
| `2102:00` | `2102` / 1265 | U16 / RW | 1 | `0` | - / - | Obj.DoStatus |
| `2200:00` | `2200` / 1274 | U16 / RW | 1 | `0` | - / - | Obj.DefautDir |
| `2201:00` | `2201` / 1283 | U16 / RW | 1 | `0` | - / - | Obj.MicDivide |
| `2202:00` | `2202` / 1292 | U16 / RW | 1 | `10` | - / - | Obj.Soft_Limit |
| `2203:00` | `2203` / 1301 | U16 / RW | 1 | `0` | - / - | Obj.CAN_ID |
| `2300:00` | `2300` / 1310 | U16 / RW | 1 | `0` | - / - | Obj.FactoryPara |
| `2301:00` | `2301` / 1319 | U16 / RW | 1 | `0` | - / - | Obj.ClearAlm |
| `2302:00` | `2302` / 1328 | U16 / RW | 1 | `0` | - / - | Obj.ClearPos |
| `2400:00` | `2400sub0` / 1343 | U16 / RO | 0 | `16` | - / - | HighestSubIndex |
| `2400:01` | `2400sub1` / 1352 | U16 / RW | 1 | `0` | - / - | Obj.Dilevel |
| `2400:02` | `2400sub2` / 1361 | U16 / RW | 1 | `0` | - / - | Obj.Di0 |
| `2400:03` | `2400sub3` / 1370 | U16 / RW | 1 | `0` | - / - | Obj.Di1 |
| `2400:04` | `2400sub4` / 1379 | U16 / RW | 1 | `0` | - / - | Obj.Di2 |
| `2400:05` | `2400sub5` / 1388 | U16 / RW | 1 | `0` | - / - | Obj.Di3 |
| `2400:06` | `2400sub6` / 1397 | U16 / RW | 1 | `0` | - / - | Obj.Di4 |
| `2400:07` | `2400sub7` / 1406 | U16 / RW | 1 | `0` | - / - | Obj.Di5 |
| `2400:08` | `2400sub8` / 1415 | U16 / RW | 1 | `0` | - / - | Obj.Di6 |
| `2400:09` | `2400sub9` / 1424 | U16 / RW | 1 | `0` | - / - | rsvd1 |
| `2400:0A` | `2400subA` / 1433 | U16 / RW | 1 | `0` | - / - | rsvd2 |
| `2400:0B` | `2400subB` / 1442 | U16 / RW | 1 | `0` | - / - | rsvd3 |
| `2400:0C` | `2400subC` / 1451 | U16 / RW | 1 | `0` | - / - | Obj.Dolevel |
| `2400:0D` | `2400subD` / 1460 | U16 / RW | 1 | `0` | - / - | Obj.Do0 |
| `2400:0E` | `2400subE` / 1469 | U16 / RW | 1 | `0` | - / - | Obj.Do1 |
| `2400:0F` | `2400subF` / 1478 | U16 / RW | 1 | `0` | - / - | Obj.Do2 |
| `2400:10` | `2400sub10` / 1487 | U16 / RW | 1 | `0` | - / - | rsvd4 |
| `2401:00` | `2401` / 1496 | U16 / RW | 1 | `50` | - / - | Obj.Di0Filtime |
| `2402:00` | `2402` / 1505 | U16 / RW | 1 | `40` | - / - | Obj.Di1Filtime |
| `2403:00` | `2403` / 1514 | U16 / RW | 1 | `100` | - / - | Obj.Di2Filtime |
| `2404:00` | `2404` / 1523 | U16 / RW | 1 | `4000` | - / - | Obj.Di3Filtime |
| `2405:00` | `2405` / 1532 | U16 / RW | 1 | `10` | - / - | Obj.Di4Filtime |
| `2406:00` | `2406` / 1541 | U16 / RW | 1 | `2` | - / - | Obj.Di5Filtime |
| `2407:00` | `2407` / 1550 | U16 / RW | 1 | `16` | - / - | Obj.Di6Filtime |
| `2500:00` | `2500` / 1559 | U16 / RW | 1 | `-` | - / - | Obj.DrvMode |
| `2501:00` | `2501` / 1567 | U16 / RW | 1 | `-` | - / - | Obj.Encoder |
| `2502:00` | `2502` / 1575 | U16 / RW | 1 | `-` | - / - | Obj.Maxcurrent |
| `2503:00` | `2503` / 1583 | U16 / RW | 1 | `-` | - / - | Obj.ClMaxCper |
| `2504:00` | `2504` / 1591 | U16 / RW | 1 | `-` | - / - | Obj.ClbasCper |
| `2505:00` | `2505` / 1599 | U16 / RW | 1 | `-` | - / - | Obj.OlMaxCper |
| `2506:00` | `2506` / 1607 | U16 / RW | 1 | `-` | - / - | Obj.suo_per |
| `2507:00` | `2507` / 1615 | U16 / RW | 1 | `-` | - / - | Obj.suo_time |
| `2508:00` | `2508` / 1623 | U16 / RW | 1 | `-` | - / - | Obj.Filtable |
| `2509:00` | `2509` / 1631 | U16 / RW | 1 | `-` | - / - | Obj.Filtratio |
| `250A:00` | `250A` / 1639 | U16 / RW | 1 | `-` | - / - | Obj.PosAlrm |
| `250B:00` | `250B` / 1647 | U16 / RW | 1 | `-` | - / - | Obj.PosError |
| `250C:00` | `250C` / 1655 | U16 / RW | 1 | `-` | - / - | Obj.PosinTime |
| `250D:00` | `250D` / 1663 | U16 / RW | 1 | `-` | - / - | Obj..AvrFiltime |
| `250E:00` | `250E` / 1671 | U16 / RW | 1 | `-` | - / - | Obj.kpscale |
| `250F:00` | `250F` / 1679 | U16 / RW | 1 | `-` | - / - | Obj.kp |
| `2510:00` | `2510` / 1687 | U16 / RW | 1 | `-` | - / - | Obj.ki |
| `2511:00` | `2511` / 1695 | U16 / RW | 1 | `-` | - / - | Obj.kc |
| `2512:00` | `2512` / 1703 | U16 / RW | 1 | `-` | - / - | Obj.an_kp1 |
| `2513:00` | `2513` / 1711 | U16 / RW | 1 | `-` | - / - | Obj.an_kv1 |
| `2514:00` | `2514` / 1719 | U16 / RW | 1 | `-` | - / - | Obj.an_spd1 |
| `2515:00` | `2515` / 1727 | U16 / RW | 1 | `-` | - / - | Obj.an_kp2 |
| `2516:00` | `2516` / 1735 | U16 / RW | 1 | `-` | - / - | Obj.an_kv2 |
| `2517:00` | `2517` / 1743 | U16 / RW | 1 | `-` | - / - | Obj.an_spd2 |
| `2518:00` | `2518` / 1751 | U16 / RW | 1 | `-` | - / - | Obj.an_kvff |
| `2519:00` | `2519` / 1759 | U16 / RW | 1 | `-` | - / - | Obj.an_ki |
| `251A:00` | `251A` / 1767 | U16 / RW | 1 | `-` | - / - | Obj.v_kp |
| `251B:00` | `251B` / 1775 | U16 / RW | 1 | `-` | - / - | Obj.v_ki |
| `251C:00` | `251C` / 1783 | U16 / RW | 1 | `-` | - / - | Obj.v_kc |
| `251D:00` | `251D` / 1791 | U16 / RW | 1 | `-` | - / - | Obj.v_kvfbd1 |
| `251E:00` | `251E` / 1799 | U16 / RW | 1 | `-` | - / - | Obj.v_kvfbd2 |
| `251F:00` | `251F` / 1807 | U16 / RW | 1 | `-` | - / - | Obj.v_kvff |
| `2520:00` | `2520` / 1815 | U16 / RW | 1 | `-` | - / - | Obj.v_kacff |
| `603F:00` | `603F` / 1823 | U16 / RO | 1 | `0` | - / - | DSP Error Code |
| `6040:00` | `6040` / 1832 | U16 / WO | 1 | `0` | - / - | ControlWord |
| `6041:00` | `6041` / 1841 | U16 / RO | 1 | `0` | - / - | StatusWord |
| `605D:00` | `605D` / 1850 | I16 / RW | 0 | `1` | - / - | Halt option code |
| `6060:00` | `6060` / 1859 | U8 / WO | 1 | `0` | - / - | Mode of operation |
| `6061:00` | `6061` / 1868 | U8 / RO | 1 | `0` | - / - | Mode of operation display |
| `6064:00` | `6064` / 1877 | I32 / RO | 1 | `0` | - / - | actual position |
| `606C:00` | `606C` / 1886 | I32 / RO | 1 | `-` | - / - | actual speed |
| `607A:00` | `607A` / 1894 | I32 / RW | 1 | `5000` | - / - | target position |
| `607C:00` | `607C` / 1903 | I32 / RW | 1 | `0` | - / - | home offset |
| `607D:00` | `607Dsub0` / 1918 | U8 / RO | 0 | `2` | - / - | HighestSubIndex |
| `607D:01` | `607Dsub1` / 1927 | I32 / RW | 1 | `-` | - / - | Soft MIN Limit |
| `607D:02` | `607Dsub2` / 1935 | I32 / RW | 1 | `-` | - / - | Soft MAX Limit |
| `6081:00` | `6081` / 1943 | U32 / RW | 1 | `60` | - / - | posmode speed |
| `6083:00` | `6083` / 1952 | U32 / RW | 1 | `100` | - / - | accel time |
| `6084:00` | `6084` / 1961 | U32 / RW | 1 | `100` | - / - | decel time |
| `6098:00` | `6098` / 1970 | U8 / RW | 1 | `0` | - / - | homing method |
| `6099:00` | `6099sub0` / 1985 | U8 / RO | 0 | `2` | - / - | HighestSubIndex |
| `6099:01` | `6099sub1` / 1994 | U32 / RW | 1 | `120` | - / - | homing speed |
| `6099:02` | `6099sub2` / 2003 | U32 / RW | 1 | `60` | - / - | homing search speed |
| `609A:00` | `609A` / 2012 | U32 / RW | 1 | `100` | - / - | homing accel/decel time |
| `60FD:00` | `60FD` / 2021 | U32 / RO | 1 | `-` | - / - | Digital inputs |
| `60FE:00` | `60FEsub0` / 2035 | U8 / RO | 0 | `2` | - / - | HighestSubIndex |
| `60FE:01` | `60FEsub1` / 2044 | U32 / RW | 1 | `-` | - / - | Physical outputs |
| `60FE:02` | `60FEsub2` / 2052 | U32 / RW | 1 | `-` | - / - | Bit Mask |
| `60FF:00` | `60FF` / 2060 | I32 / RW | 1 | `120` | - / - | velocity speed |
