# Prompt 3 network, I/O, tuning and family evidence review

Read-only review performed 2026-10-28 against execution branch
`promptset/motorcontrol-can`, initially `f25aa0b`. The only worker-owned change
is this report. No device traffic, Git mutation or source-snapshot change was
performed. Lead owns the merged ledger and final verification. Source IDs and
one-based PDF page numbers follow [the retained manifest](../reference/SOURCES.json).

The original EDS sections were reopened directly, together with original PDF
text. Rendered originals inspected in this review: ZH57 p8 and p17, ZH86 p3 and
p19, EN86 p19, ZH57 p3. These confirm the table-column associations and the
terminal-label conflict; this review did not rely on extracted tables alone.

## Network decisions

| Scope and exact source | Documentary decision | Smallest remaining qualification |
| --- | --- | --- |
| EN86/ZH86 p3 sections 1.1/1.2; ZH57 p3 | CANopen CiA301/CiA402, position/velocity/homing, SDO/PDO/EMCY and up-to-32-axis product claim are documented. Two RJ45s and `2 x 1 Mbps` do not establish two independent controllers, 2 Mbit/s operation or CAN FD. | Exact model/firmware bindings; no extrapolation to a common-library axis/address limit. |
| EN86/ZH86 p9-10 section 2.2.4; OP86 p7-8; ZH57 p9-10 | SW1..5 weights 1,2,4,8,16 select 1..31; all OFF says user-defined. SW6/SW7: ON/ON 125k, OFF/ON 250k, ON/OFF 500k, OFF/OFF 1000k. SW8 ON inserts 120 ohms. | Node parameter/DIP precedence and activation timing, including whether restart/save is required. All-OFF is not node zero. |
| EDS `[2203]`; EN86/ZH86 p18 | U16 RW parameter range 0..127; EDS default 0. This is a parameter range, not permission to address SDO or motion to node zero. | Meaning of stored zero and user-defined selection; effective identity before/after an explicit commissioned change. Owner 38. |
| EDS `[DeviceInfo]`; EN86 p9 | 50/125/250/500/1000 kbit/s flags are set, 10/20/800 unset. Manuals provide four DIP choices with no 50k selection. | 50k is advertised but selection/applicability unqualified, not proved unsupported. No software bitrate object was found. Owner 38. |
| EDS `[1005]`, `[1014]`, `[1200sub1]`, `[1200sub2]` | RW U32 route metadata; defaults SYNC 0x80, EMCY node+0x80, SDO request node+0x600, response node+0x580. | Acceptance of configurable values, high-bit/extended-ID features, activation and retirement boundary. U32 representation does not establish support for every COB-ID flag. Owners 15/38. |
| EDS `[1017]` | RW U16 producer heartbeat, default zero. Zero disabling is a CANopen convention corroborated by pinned `canopen/nmt.py` `start_heartbeat`, not an observed startup state. | Configured producer period and real boot/state traffic. A silent drive with disabled heartbeat cannot be labelled offline from absence alone. Owners 15/38. |
| EDS full object lists | No 1016 heartbeat consumer, 100C/100D guarding or documented drive reaction on host loss. | Manufacturer/firmware evidence and authorized traffic/physical observation for the exact proposed host-loss feature. Do not infer a watchdog from RPDO event timer 1000. |
| EDS `[DeviceInfo]` | SimpleBootUpSlave=1, SimpleBootUpMaster=0, DynamicChannelsSupported=0, GroupMessaging=0, LSS_Supported=0. | These are explicit metadata capability claims for the unbound EDS. Do not turn absence of unrelated objects into equally explicit unsupported claims. |
| EDS `[1005]`; full object lists | SYNC identifier exists; 1006 cycle period absent. | Neither SYNC production nor each synchronous PDO type/cycle behavior is established. Generic protocol decoding can proceed; synchronous drive behavior needs its own evidence. |
| EDS `[1014]`, `[1001]`, `[1003sub0..4]` | EMCY route, error register and four RO U32 error-history entries are declared; count is RO U8 0..4. | No write-to-clear history action follows. Retain raw EMCY bytes; do not equate EMCY receipt, CiA402 fault and application health or invent vendor error codes from LED counts. |
| All typed EDS sections | Explicit scalar widths are 1, 2 or 4 bytes; every declared scalar fits expedited payload capacity. | This does not establish that every upload response is expedited, segmented transfer is absent, or block transfer is supported. Negotiate/validate the received form; owning service 13 handles the bounded lifecycle. |

The manufacturer PDFs do not supply an NMT wire-format/broadcast table. The
pinned protocol sources are a separate authority for the wire boundary:
`public_code/canopennode/301/CO_NMT_Heartbeat.h` lines 44-98 describes NMT
payload command/node, zero addressing all nodes, heartbeat states 0/4/5/127
and commands 1/2/128/129/130. `public_code/canopen_python/canopen/nmt.py`
`NmtMaster.send_command` uses identifier zero and two data bytes;
`NmtSlave.send_command` and `start_heartbeat` use node+0x700 and zero for
boot-up. These support explicit broadcast versus individual network requests,
not broadcast SDO, a promise of CL86 startup state, a motor stop effect or
automatic reset. Retain NMT state separately from drive state. CANopenNode's
startup-control flags also demonstrate why a stack default cannot establish
Lichuan startup policy.

The protocol pin is CANopenNode `9b8beed8367241e96ac03f916cd5a500bcb2cf23`,
recorded in [public-code provenance](../reference/public_code/SOURCES.json).
Its `CO_SDOclient.c` lines1216-1257 checks upload response index/subindex and
branches on the expedited bit, including segmented size-indicated handling;
this corroborates why a four-byte object does not predetermine the server's
transfer form. The source is protocol evidence only.

The original `[1400..1403]` / `[1800..1803]` sections declare four receive and
four transmit PDOs. Every communication record count is RO U8 default 5;
`:01` RW U32 ID, `:02` RW U8 type default 255, `:03` RW U16 inhibit default 0,
`:04` RW U8 compatibility default 0, `:05` RW U16 event default 1000 for
receive and 100 for transmit. These raw values do not establish physical
timing or all firmware-accepted transmission types. Do not invent a useful
write meaning for compatibility entries. Mapping records contain RW U8
counts 0..4 and four RW U32 slots; unused slot defaults are absent.

| PDO | Default ID | Mapping record: active defaults | Derived length |
| --- | --- | --- | --- |
| RPDO1 | node+0x200 | 1600: 60400010, 60FF0020, 60600008 | 7 bytes |
| RPDO2 | node+0x300 | 1601: 607A0020, 60810020 | 8 bytes |
| RPDO3/4 | node+0x400/500 | 1602/1603: count zero | No active layout |
| TPDO1 | node+0x180 | 1A00: 60410010, 60FD0020, 60610008 | 7 bytes |
| TPDO2 | node+0x280 | 1A01: 60640020, 606C0020 | 8 bytes |
| TPDO3/4 | node+0x380/480 | 1A02/1A03: count zero | No active layout |

Lengths above are arithmetic from the EDS, not measured frames. EN86 p15-16
and ZH86 p15-16 corroborate four slots. Classical payload capacity and four
mapping slots are distinct checks. SDO access does not determine PDO direction:
WO 6040/6060 appear in RPDO1. Action objects marked PDO-mappable still need
effect-specific admission. Reconfiguration requires actual route/map evidence
and a retirement boundary; a host generation alone cannot identify delayed
same-ID/same-length traffic.

## I/O conflict dispositions

| Source conflict or field | Recommended reviewed disposition and narrow gate |
| --- | --- |
| C01: EN86 p8/p18, ZH86 p8, OP86 p6 place polarity at 2400:0B and DO0..2 at 0C..0E. EDS and rendered ZH86 p19 place polarity at 0C and DO0..2 at 0D..0F. | Preserve both layouts. Gate semantic output-configuration writes/mappings and semantic interpretation of affected reads until firmware-specific layout is established. ZH86 p19 still shows MODBUS addresses 002B..002E; those do not correct CANopen subindices. No trial of both writes. Owner 21. |
| C02: EN86/ZH86 p8 and OP86 p7 say function 2 running, 3 homing complete; EN86 p18 and ZH86 p19 say 2 homing complete, 3 drive status. | Gate semantic values 2/3 independently of the address gate. Consistent values 0,1,4,5,9,10,11 remain documented but cannot bypass unresolved target addresses. Values 6..8 are undocumented, not newly invented functions. |
| C03: rendered ZH57 p8 section 2.2.2 says 60FE bits16/17/18 DO0/1/2; rendered p17 section 3.6.2 says DO1/2/3 in both sub1 and sub2 rows. | Preserve the terminal-identity conflict; do not silently shift numbering or infer a fourth output. Three physical outputs are documented p6/p8. Neither variant resolves 86C identity. |
| OP86 p7 60FE:01/02 | The documented condition is both corresponding bits 1 for output, at bits16..18. EDS confirms RW U32 but no default. Output assignment, polarity, mask behavior for clearing/unchanged bits, terminal identity and OP86 applicability remain prerequisites to an exact output operation. Do not infer a complete masked-write truth table from this one condition. |
| C15: EDS 2400:0A reserved versus EN86 p8/p18 and ZH86 p19 input filter | Keep excluded from typed access while semantic identity unresolved; raw EDS flags remain. 2400:09/10 remain reserved; 0B also participates in C01. |
| 2400:01; EN86 p18/ZH86 p18 | Bits0..6 invert DI0..6 logical levels; no undocumented high bits. This consistent input contract can be reviewed separately from output conflicts. |
| 2400:02..08; EN86/ZH86 p8 | DI function values 0 undefined,1 origin,2 positive limit,3 negative limit,4 enable/MF,5 stop,6 emergency stop,7..13 user0..6 are documented. Actual wiring and active state remain prerequisites; no implicit disabling. |
| C14 assignment defaults | EDS all zero differs from manual DI0/1/2=1/2/3 and DO0=1. Retain metadata and require readback; do not initialize from either source. |
| C16 2401..2407; EN86 p8/p18-19, OP86 p6-7 | U16 filter coefficients documented without time units/equation. EDS defaults 50/40/100/4000/10/2/16 versus manuals all2. Raw observation is useful; physical filter-time setting/range/default requires exact firmware evidence. |
| Output polarity description EN86 p18/ZH86 p19 | Only bit0/1 are explained despite three terminals. Do not invent bit2 semantics. |
| C04 OP86 p6 | Brake example PA_01E versus same-page table PA_02E remains an editorial conflict; neither is a new CANopen address. Gate physical brake automation independently from ordinary raw reads. |

## Tuning review, owner 22

Every original EDS section 2500..2520 is RW U16 with PDOMapping=1 and no
default or range. EN86 p19-20 and ZH86 p19-20 support the following separate
facts. A numerical type bound is not an operating range. No factory value is
invented, and no tuning action is admitted automatically.

| Entries | Established meaning/unit | Missing information for physical setting/effect |
| --- | --- | --- |
| 2500 | 0 ineffective,1 open loop,2 closed loop | Model eligibility, activation/change prerequisites; not 6060 motion mode or proof that OL hardware gains feedback. |
| 2501 | Encoder resolution | Line/count/quadrature basis, admissible values and model applicability; 1000-line product description does not establish 4000 command counts/rev. |
| 2502 | Maximum peak current, mA | Drive/motor-specific operating range and activation; not RMS current. |
| 2503/2504/2505/2506 | Closed-loop maximum/base, open-loop maximum, locking current ratios, percent | Reference basis, valid limits and drive/motor applicability; do not assume 0..100 from percent alone. |
| 2507 | Locking-current time, ms | Valid limits, trigger/activation and physical effect. |
| 2508/2509 | Low-pass enable/coefficient | Enable encoding, coefficient equation, admissible values and behavior. |
| 250A/250B/250C | Position-error alarm threshold, positioning-completion threshold/time | Threshold basis and completion-time unit. In particular, 250C is not documented in ms merely because 2507 is. |
| 250D/250E/250F/2510/2511 | Mean-filter coefficient, current-loop ratio, Kp, Ki, Kc | Units, equations, safe range and activation. |
| 2512/2513/2514/2515/2516/2517/2518/2519 | LA velocity Kp1/Kv1/node1/Kp2/Kv2/node2, feedforward Kvf, position-loop Ki | Coefficient and velocity scales, ranges and update behavior. |
| 251A..2520 | EDS names Obj.v_kp/v_ki/v_kc/v_kvfbd1/v_kvfbd2/v_kvff/v_kacff only | Physical semantics, units, ranges, firmware applicability and effect; do not manufacture a tuning algorithm from names. |

Conservative read scope can retain native U16 observations with provenance.
The missing evidence gates each affected setter/conversion; it does not make
all 33 fields physically unsupported or make an unimplemented setter complete.

## Separate family entries and identity

| Model | Applicable product evidence | Explicit distinction |
| --- | --- | --- |
| CL86-C | EN86/ZH86 p3 | Stepper drive, matched motors60/86, 1000-line incremental encoder, max8A document claim. |
| OL86-C | EN86/ZH86 p3 | Stepper drive, matched motors60/86, no encoder, max8A document claim. |
| CL57-C | ZH57 p3 | Stepper drive, matched motors42/57/60, 1000-line incremental encoder, max5A document claim. |
| OL57-C | ZH57 p3 | Stepper drive, matched motors42/57/60, no encoder, max5A document claim. |

All advertise position/velocity/homing independently of feedback. No encoder
is an actual documented hardware distinction, not proof that a position
observation object is unavailable. The 57C profiles remain contrasts rather
than implicitly supported products. OP86's cover/internal-57C conflict C18
limits sole reliance on that source. C19 (86C product18..80VAC/36..110VDC versus
terminal24..50VDC) and C24 (57C product24..48VDC versus terminal24..50VDC) must
remain separate; neither supplies a bench default.

Original EDS `[DeviceInfo]` names generic LC_SterDriverCAN, vendor0x1000,
product1, revision0. `[1018]` declares sub0..3 only, count default3 despite
HighLimit4; no serial sub4 is declared. `[1009]`/`[100A]` are numeric U16,
not visible strings. EN86/ZH86 p17 identify 2000/2001 as model/version U16,
but no code-to-model/firmware table was found. Therefore discovery owner37
retains raw identity, user-selected exact model and provenance separately;
generic values cannot automatically select all four models or resolve any
conflict. No internal IC or shared firmware identification follows.

## Current ecosystem delta and checks

Read-only revision/dirty refresh found all seven checkouts clean:
MotorControl-RS `bd831c859daa9c8c07b8d27a2dd879bb4e750511`, ADS1115
`235af3e8c722661a89a44766a4ccd903a654f6fe`, INA228
`b28c5ea69bcb4d26b351c52c24d3c6fae9fd282b`, MB85RC
`1bf7c293549b1038b2d8f3b7a8d655fa74201e6e`, MCP45HVX1
`c6c7feefd79b9fc16ddc7b7786ad187e5185fde6`, RV3032-C7
`aabd8d9204e75072403fefb60cf8d2679a7f56de`, FieldCore-node
`5ad9292dc8481cf08be9a61f662495a6474d1da0`. RS `Reads.h` lines135-190
retains non-atomic observation/provenance and unresolved scale; ADS1115
`ADS1115.h` profile snapshot contract distinguishes verified generation and
recovery replay; INA228 `INA228.h` lines896-919 explicitly identifies destructive
reads. These establish why Lichuan read effects cannot be inferred from RO/RW,
and why sensor recovery replay must not become motor policy. They do not
resolve a Lichuan physical claim. The lead's stage-wide audit owns the broader
implementation/caller/test/CI comparison; this bounded worker reread those
declarations and refreshed revisions, not every sibling suite.

No target `.github` workflow directory exists. The existing planning verifier
was inspected. Worker checks `python scripts/check_promptset.py` and
`git diff --check` passed after this report was created (132 Markdown files,
99 ledger objects,202 entries,53 manual items,24 conflicts). Final merged
integrity/ledger/diff checks remain lead-owned. This documentary review is not
a CAN compilation, implementation test or hardware qualification.
