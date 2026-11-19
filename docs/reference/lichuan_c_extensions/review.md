# Lichuan C-family extensions: delta evidence review

Reviewed 2026-11-19 from the originals in [vendor/](vendor/) (manifest:
[SOURCES.json](SOURCES.json)). This is documentary evidence only. Nothing here
is implemented, host-tested or hardware-observed, and no statement is a
firmware fact; no source names a firmware version.

This review is a **delta** against the existing C-family evidence:
[lichuan_review.md](../lichuan_review.md) and the reviewed ledger
[lichuan_objects.json](../lichuan_objects.json) (gates Q01..Q28, conflicts
C01..C24), which cover CL86-C, OL86-C, CL57-C and OL57-C from EN86, ZH86, OP86,
ZH57, OP57 and the generic stepper EDS. Where this review says "unchanged", the
cited extension source repeats the C-family claim and the existing disposition
applies to the new model only as a documentary claim; it is not evidence that
the firmware is shared.

Citation form: `TLCEN p11` is one-based PDF page 11 of source TLCEN. Printed
page numbers are two lower than PDF pages in EN57, TLCEN, the TL user manuals
and the CL2 manuals; the TL operating manuals print inconsistent numbers. EDS
citations give the section-header line: `TLEDS l.1569` for
[TL_CAN_V1.0.eds](vendor/TL_CAN_V1.0.eds), `C-EDS l.1505` for the existing
[LC_StepDriverCAN_V1.1.eds](../vendor/LC_StepDriverCAN_V1.1.eds), `DSCEDS l.1266`
for the DS-C EDS stored in the
[DS2-C pack](../lichuan_ds2_canopen/vendor/LC_DSServoCAN_V1.1.eds). Existing
sources keep their ids (EN86, ZH86, OP86, ZH57, OP57). Extracted text was used
for search; every table relied on below was checked against the rendered page.

## Scope and exact applicability

| Family | Models and order codes | Sources | Motor and feedback in the sources |
| --- | --- | --- | --- |
| TL-C integrated steppers | TLC42-C/TLO42-C (TLC42C-24V-04/-08, TLO42C-24V-04/-08), TLC57-C/TLO57-C (TLC57C-36V-1/-2/-3, TLO57C-36V-1/-2/-3), TLC60-C/TLO60-C (TLC60C-36V-3/-3.5/-4, TLO60C-36V-3/-3.5/-4), TLC86-C/TLO86-C (TLC86C-48V-4.5/-8.5/-10/-12, TLO86C-48V-4.5/-8.5/-10/-12) | TLCEN, TL42ZH..TL86ZH, TL42OP..TL86OP, TLEDS, PG-TL42..PG-TL86, PG-TL57EN | 1.8 degree two-phase stepper with integrated drive; TLC = 1000-line encoder, TLO = no encoder (TLCEN p3; PG-TL42..PG-TL86 tables). Brake body lengths are listed (PG-TL57) |
| CL2 absolute-encoder drives | CL2-57C (42/57/60 motors), CL2-86C (86 motors) | CL257, CL286, PG-CL257, PG-CL286 | Closed-loop stepper drive; 17-bit or 23-bit encoder, single or multi-turn (CL257 p4, p10) |
| CL2 ABZ drives | CL257C-ABZ (42/57/60), CL286C-ABZ (86) | ABZ57, ABZ86, PG-ABZ57, PG-ABZ86 | Closed-loop stepper drive; 1000-line ABZ encoder, Z optional (ABZ57 p4, p6) |
| C-family English 57C edition | CL57-C, OL57-C | EN57, PG-CL57EN | Same models as ZH57/OP57; CL57-C 1000-line incremental, OL57-C no encoder (EN57 p3) |

The "C" in TLC/TLO means closed loop, TLO open loop, and the trailing `-C` the
CANopen variant; the TL pulse, RS485 and EtherCAT variants (TL-P, TL-R, TL-E)
are other products and are out of scope. The CL2 product pages state that CL2
also exists as pulse and RS485 drives (PG-CL257); only the CANopen `C` models
are covered.

Ratings that a fixture must check (all document claims, contradictions in
LX-D09, LX-D17, LX-D31):

| Model | Supply | Current | Source |
| --- | --- | --- | --- |
| TL42-C | 24..36 V DC; 20..36 V; terminal 24..36 V; page 18..36 V (typ. 24) | 2.0 A max output; page "drive current" 2.8 A, phase current 2.5 A | TLCEN p3, TL42ZH p3, TL42ZH p5, PG-TL42 |
| TL57-C, TL60-C | 24..50 V DC; page 24..50 V (TL60 headline 20..50 V) | 4.0 A max output; page 5.6 A, phase current 4.0 A | TLCEN p3, TL57ZH p3, PG-TL57, PG-TL60 |
| TL86-C | 24..70 V DC; TLCEN p7 figure 24..48 V | 6.0 A max output; page 7.7 A, phase current 6.0 A | TLCEN p3, TLCEN p7, PG-TL86 |
| CL2-57C, CL257C-ABZ | 24..50 V DC | 7 A peak; `2502` 0..7000 mA | CL257 p4, p17; ABZ57 p4 |
| CL2-86C, CL286C-ABZ | 30..110 V DC or 20..80 V AC | 8 A peak; `2502` 0..8000 mA | CL286 p4, p5, p17; ABZ86 p4 |
| CL57-C, OL57-C | 24..48 V DC (EN57 p3); terminal 24..50 V (EN57 p6); page 24..50 V | 5 A max output; page "maximum peak current 5A" | EN57 p3, p6; PG-CL57EN |

Hardware and firmware revisions: no source names one. TLEDS is dated
2023-05-25 (TLEDS l.4); the TL user manuals were produced 2024-07-24, the TL
operating manuals 2025-07-29, TLCEN 2024-09-09 (modified 2025-09-22), the CL2
manuals 2026-03-31 and EN57 2022-11-29 (modified 2025-09-20), per PDF metadata
in SOURCES.json. These dates identify documents, not firmware.

Comparison method. The four TL user manuals and the four TL operating manuals
were compared word by word page by page; they differ only in the model name,
p3 ratings, p4 dimensions, the p5 supply text (user manuals) and the
recommended supply sentence on p3 (operating manuals). TLCEN was compared with
TL57ZH section by section and follows it closely (translation defects in
LX-D14). The CL2-86C, CL257C-ABZ and CL286C-ABZ manuals were compared with
CL257 page by page; they differ only in the items noted in the tables below.
EN57 was compared with EN86 page by page; apart from the product table (EN57 p3)
and layout reflow it repeats EN86. TLEDS was compared with C-EDS key by key
([delta](extracted/TL_CAN_V1.0_vs_LC_StepDriverCAN_V1.1.delta.txt)).

## Sources and retrieval failures

All 26 stored originals were downloaded fresh on 2026-11-19 with HTTP 200;
[SOURCES.json](SOURCES.json) gives URL, Referer, size and SHA-256. The CDN
serves files only with the official page as Referer and then redirects to a
signed URL. EN57, TLCEN, TLEDS, CL257 and ABZ57 match the SHA-256 recorded by the
[2026-11-19 survey](../../audit/2026-11-19_market_stepperonline_lichuan.md);
CL286, ABZ86 and the TL Chinese manuals had no survey hash. The existing
C-family originals were not re-downloaded; their working-file SHA-256 values
still match [../SOURCES.json](../SOURCES.json), including the C-EDS
(`dd3c96e7...4f52c`).

| ID | Original | Role |
| --- | --- | --- |
| EN57 | [57C English manual](vendor/57C_CANopen_user_manual_en.pdf) | First English 57C source; 20 pages |
| TLCEN | [TL-C English manual](vendor/tl-c_integrated_stepper_manual_en.pdf) | Primary TL-C source; 19 pages |
| TL42ZH..TL86ZH | Chinese TL user manuals (see README) | Per-size Chinese originals of TLCEN; 19 pages each |
| TL42OP..TL86OP | Chinese TL operating manuals | Wiring, DIP, SDO frame examples; 8 pages each |
| TLEDS | [TL_CAN_V1.0.eds](vendor/TL_CAN_V1.0.eds) | TL-C dictionary metadata |
| CL257, CL286, ABZ57, ABZ86 | CL2 manuals V1.1 | Only CL2 dictionary source (no EDS); 31 pages each |
| PG-* and IDXEN10 | Product pages and English index page | Order codes, ratings, linking pages |

Not stored: Chinese download-index pages 10, 20, 220, 230, 240 and 250, the
English index page 20 and two category pages (all reviewed, listed under
`remote_only`; every file they link is also linked from a stored page). TL60-C
and TL86-C product pages (ids 178 and 294) are not on the first category page;
they were found by requesting nearby product ids. The vendor debug software
was not downloaded. No EDS is published for any CL2 model. There were no
failed downloads.

## Identity

| Source | `1018:01` vendor | `1018:02` product | `1018:00`, `:03`, `:04` | `1000` | Other |
| --- | --- | --- | --- | --- | --- |
| C-EDS (CL86-C/OL86-C/CL57-C/OL57-C) | 0x00001000 (l.287) | 0x00000001 (l.296) | count default 3, HighLimit 4, no sub04 (l.276); revision 0 (l.305) | 0x40192 (l.158) | ProductName `LC_SterDriverCAN` (l.15) |
| TLEDS (TL-C) | 0x1000 (l.83) | 0x0001 (l.91) | count default 4 (l.75); revision 0000 (l.99); sub04 serial 0000 (l.107) | 0x40192 (l.54) | ProductName `TL_CAN` (l.15) |
| DSCEDS (DS-C servo) | 0x00001000 (l.270) | 0x00000001 (l.279) | count 3, no sub04 | 0x40192 | ProductName `LC_DsServoCAN` |
| CL2 manuals | not documented | not documented | not documented | value not given (CL257 p14) | no EDS |
| EN57, TLCEN, TL manuals | not documented | not documented | not documented | value not given (EN57 p15, TLCEN p14) | `2000` "drive model code" without a code table (EN57 p17, TLCEN p16) |

TLEDS and C-EDS also share `1009` default 0x2608 (TLEDS l.212, C-EDS l.234),
`100A` default 1 (l.220, l.243) and `2000` default 1 (l.1324, l.1229). No value
declared in either EDS distinguishes a TL-C from a CL86-C. TLEDS fixes the
C-EDS count anomaly A01 (count 3 with HighLimit 4 and no sub04) by declaring
sub04, but the live count of either product is unknown, so the count is not a
discriminator (LX-D02).

**Safety finding (backlog D18, [docs/backlog.md](../../backlog.md) line 138).**
C-EDS, TLEDS and DSCEDS declare the same vendor 0x1000 and product 0x0001 while
their manufacturer objects mean different things. TLEDS renames `2402` from the
C-EDS DI1 filter coefficient (`Obj.Di1Filtime`, C-EDS l.1505) to `Obj.BaudSet`
(TLEDS l.1569) and `2403..2407` to `Obj.RSVD` (l.1577..1609); TLCEN p9 puts the
bitrate selector at `2407` instead (LX-D03). DSCEDS turns `2201` into
`Obj.SavePara` (DSCEDS l.1266; stepper `Obj.MicDivide`, C-EDS l.1283), `2300`
into `Obj.GearNum` (l.1293; stepper `Obj.FactoryPara`, C-EDS l.1310), `2301`
into `Obj.GearDen` (l.1302; stepper `Obj.ClearAlm`, C-EDS l.1319) and
`2401`/`2402` into brake delays (l.1479, l.1488). The DS2-C family adds a second
collision set (see the [DS2-C review](../lichuan_ds2_canopen/review.md)). All
these EDS files declare `1000 = 0x40192`. Consequence: `1000`/`1018` values
cannot select CL86-C, TL-C or DS-C behavior, and a CL86-C profile bound by
identity could write a DI filter coefficient into a TL-C bitrate setting or a
subdivision into a DS-C save command. Binding must come from the application's
declared model; identity values remain raw corroborating evidence (existing
Q01). The CL2 drives publish no identity values at all.

Device type: under the bit layout that the Nanotec N5 manual documents for
`1000` bits 16..23 ([N5 p159](../multidrive/vendor/nanotec_n5_canopen_v3.3.0.pdf):
2 BLDC, 4 stepper, 6 both), 0x40192 declares a stepper. That is consistent for
TL-C; the same value in the DS-C servo EDS shows that Lichuan does not use it
to separate products. The normative CiA 402 text was not obtained.

## Object-by-object delta

### TL-C against the C family

| Object | C-family evidence | TL-C evidence | Delta and disposition |
| --- | --- | --- | --- |
| `1018` | C-EDS sub0..3, count 3 | TLEDS sub0..4, count 4, serial default 0000 (l.70..107) | Identity structure differs; values collide (D18). LX-D01, LX-D02 |
| `1000`, `1009`, `100A`, `2000`, `2001` | C-EDS defaults 0x40192, 0x2608, 1, 1, 0 | Same defaults (TLEDS l.54, l.212, l.220, l.1324, l.1332) | No discriminator. LX-D35 |
| `1005`, `1014`, `1017`, `1200`, `1400..1403`, `1600..1603`, `1800..1803`, `1A00..1A03` | C-EDS defaults (lichuan_review.md, PDO and network sections) | Identical after case and number-format normalization ([delta](extracted/TL_CAN_V1.0_vs_LC_StepDriverCAN_V1.1.delta.txt)); RPDO1 `60400010/60FF0020/60600008` (l.500), TPDO1 `60410010/60FD0020/60610008` (l.889) | Unchanged documentary defaults; still unqualified (Q17, Q18) |
| `[DeviceInfo] CompactPDO` | 0x00 (C-EDS l.34) | 31 (TLEDS l.30) | Meaning per CiA 306 not reviewed; record raw. LX-D05 |
| `[DynamicChannels]` | Present, empty (C-EDS l.48) | Absent | No behavioral meaning established |
| `6041` | EN86/ZH86/ZH57/OP86 dialects | TLCEN p11, TL57ZH p10: bit3 alarm, bit8 homed, bit14 running, bit15 position complete; p14/p13 homing: bit8, bit14 | Same bit positions as the ZH86 table, not EN86; the English TL edition follows the Chinese dialect. LX-D06 |
| `6040` words | EN86 p12: 6/7/15, 31/95/63/127, 11; ZH86 adds 271, 128 | TLCEN p11, TL57ZH p10: 6/7/15, 31, 95, 63, 127, 271 stop, 11 emergency stop, 128 alarm reset | Unchanged list (Chinese variant). TL57OP p7 frame examples write 0x010F and 0x011F. LX-D08 |
| `6060`, `6061` | Modes 0/1/3/6; I8 in manuals, U8 in EDS | Same (TLCEN p10; TLEDS l.1083 WO U8, l.1091 RO U8) | Unchanged (C08, C09, C10) |
| `607A`, `6081`, `6083`, `6084`, `60FF`, `607C` | Ranges -1000000..1000000 pulses, 0..3000 rpm, 0..2000 ms, I3C typo | Same ranges and typo (TLCEN p10, p12, p15) | Unchanged (C11, C21, Q03, Q04) |
| `6098`, `6099`, `609A` | Methods 17/18/24/29; `6099:02` creep | Same methods and descriptions (TLCEN p12..13); TL57OP p7 example writes method 28 | Method 28 undocumented. LX-D08 |
| `603F` | FF01 overcurrent, FF05 position deviation | Same (TLCEN p15) | Unchanged |
| `60FD`, `60FE:01/02` | 60FE output and mask bits 16..18 (OP86 p7) | Listed without bit labels (TLCEN p16) | Only two physical outputs; bit assignment undocumented. LX-D10 |
| `2100` | Bits 0..6 incl. bit4 0=enabled | Same text (TLCEN p16, TL57ZH p16) | Unchanged documentary claim |
| `2101`, `2102` | DI0..DI6, DO0..DO2 bits | Same seven/three bits listed (TLCEN p16) for five inputs and two outputs | Bits 5..6 and 2 have no terminal. LX-D04 |
| `2200`, `2201`, `2202`, `2302` | Direction, subdivision 400..51200 (table 0..65535), soft-limit enable, position clear | Same (TLCEN p10, p14, p17) | Unchanged (C12, C13, Q14) |
| `2203` | CAN ID 0..127; DIP all OFF user-defined | Same range (TLCEN p17); DIP has only SW1..SW4, nodes 1..15 (TLCEN p9) | Precedence unknown. LX-D12 |
| `2300` | 0 none, 1 factory, 2 save | Same (TLCEN p14, p17); TL57OP p7 frame `2B 00 23 00 02 00 00 00` | Also activates a changed bitrate (TL57OP p5). Side effect below |
| `2301` | EN86 p18 and C-EDS: alarm reset | Absent from TLCEN p17 and TL57ZH p16; present in TLEDS l.1404 | EDS-only for TL-C. LX-D11 |
| `2400:01..08` | DI polarity, DI0..DI6 functions 0..13 | Polarity bits 0..4 and DI0..DI4 functions in `2400:02..06` (TLCEN p7..8, p17); same codes 0..13 | Subentries 07..08 have no terminal. LX-D04 |
| `2400:0A..0F` | C01: EN86 0B polarity, 0C..0E DO0..DO2; C-EDS 0C polarity, 0D..0F | TLCEN p8: 0B polarity, 0C DO0, 0D DO1; TLCEN p17: 0B polarity, 0C..0E Y0..Y2; TLEDS l.1521..1545: 0C `Dolevel`, 0D..0F `Do0..Do2` | C01 conflict persists for two outputs. LX-D04 |
| `2401..2407` | Seven DI filter coefficients; manuals 2, C-EDS 50/40/100/4000/10/2/16 | TLCEN p8: one "DI terminal filter coefficient" at `2401`, default 2; TLCEN p17..18: X0..X6 coefficients at 2401..2407; TLCEN p9: `2407` bitrate selector 0..3; TLEDS: 2401 `DiFiltime` 50, 2402 `BaudSet` 40, 2403..2407 `RSVD` 100/4000/10/2/16 | Write hazard. LX-D03 |
| `2500..2519` | Tuning group; `2500` 1 open, 2 closed | Same entries and texts (TLCEN p18..19); TL57OP p7: `2502` 1000 = 1 A | Unchanged documentary claims (Q09); `2502` scale example is new |
| `251A..2520` | C-EDS only | TLEDS only | Unchanged |

### CL2 and CL2xxC-ABZ against the C family

The CL2 manuals give a P-parameter (Modbus register) for every vendor object
instead of the C-family register addresses (CL257 p15..18). There is no EDS, so
every communication object other than those below is undocumented (LX-D20).

| Object | C-family evidence | CL2 evidence | Delta and disposition |
| --- | --- | --- | --- |
| `1000`, `1009`, `100A`, `1600..1603`, `1A00..1A03` | EN86 p15..16 plus C-EDS defaults | Listed with types only (CL257 p14..15) | No defaults, no COB-IDs, no `1018`. LX-D20 |
| `6041` | EN86/ZH86 dialects | CL257 p12: bit3 alarm; bit10 position complete when `6060`=1, homing complete when `6060`=6 (p14). No bit8, bit14 or bit15 | New dialect; bit10 is a completion flag, not EN86 "zero speed". LX-D23 |
| `6040` words | As above | Same list as TL-C incl. 271, 11, 128 (CL257 p12) | Unchanged documentary list |
| `6098` | Methods 17/18/24/29 with C-family descriptions | "See appendix 1" (CL257 p13); appendix "servo homing methods" 1..14, 17..30, 33, 34, 35 (p21..31) | Same numbers, different described behavior. LX-D26, LX-D27 |
| `603F` | FF01, FF05 | No code list (CL257 p19); fault table uses Err1..Err10 (p20) | LX-D30 |
| `2100` | Bit4 0=enabled, bits 5/6 soft-limit status | Bit4 "motor enable" without polarity; bits 5/6 positive/negative limit signal (CL257 p15..16) | Meanings changed. LX-D28 |
| `2102` | DO0..DO2 | Bits labelled DO1..DO3 (CL257 p16) | Label mismatch with `2400:0D..0F` DO0..DO2. LX-D24 |
| `2200`, `2201`, `2202`, `2203` | As above | P02-02, P05-01 400..51200, P05-0F, P02-23 0..127 (CL257 p16) | Unchanged ranges; `2201` table still 0..65535 (p11) |
| `2300`, `2301`, `2302` | Save/factory, alarm reset, clear | P0D-02, P0D-03, P0D-04 (CL257 p16); all documented | `2301` documented for CL2 (unlike ZH86, TL-C) |
| `2400:01..08` | DI polarity, functions 0..13 | P02-10 polarity bits 0..6; P03-00..P03-0C functions, range 0..22, codes 0 none, 1 origin, 2 positive limit, 3 negative limit, 4 motor release, 5 stop, 6 emergency stop (CL257 p9..10, p16..17) | Code 4 changed meaning; 7..13 absent. LX-D25 |
| `2400:09..0B` | Reserved, filter, polarity (disputed) | Not listed (CL257 p17) | Absent from the CL2 dictionary |
| `2400:0C..0F` | C01 conflict | P02-11 polarity bits 0..2 at 0C; P04-00..02 DO0..DO2 at 0D..0F, defaults 1/4/7, codes 0..5, 7 motor enable, 9..11 (CL257 p10, p17) | Matches the C-EDS layout, not EN86; for the CL2 document only. LX-D24 |
| `2401` | DI0 filter | P02-14 single DI filter 0..1024, default 0 (CL257 p10, p17) | `2402..2407` undocumented |
| `2500` | 0 none, 1 open, 2 closed | P02-00: 0 none, 1 open, 3 closed, range 0..3 (CL257 p17) | Closed-loop code differs. LX-D22 |
| `2501` | Encoder resolution | P00-13, U16 0..65535 (CL257 p17); P00-13 = 131072 or 8388608 (CL257 p10) | Not representable. LX-D21 |
| `2502` | Peak current mA | P00-03, 0..7000 (CL257 p17), 0..8000 (CL286 p17) | New documented bounds |
| `2503`, `2504`, `2506`, `2507` | Current ratios, lock time | P08-00/01/02 0..100 %, P08-03 0..5000 ms (CL257 p17..18) | New bounds |
| `2505`, `2508` | Open-loop max ratio, low-pass enable | Not listed | Absent |
| `2509..2519` | Tuning | P05-03, P05-1D 10..4000, P05-19 0..200, P05-1C 0..5000, P05-04, P08-04..P08-0F (CL257 p18) | New bounds; meanings unchanged |
| `251A..2520` | C-EDS only | Not listed | Absent |

### EN57 against EN86, ZH57 and OP57

EN57 repeats EN86 in content and order (EN57 p3..20 against EN86 p3..20); the
differences are the 57C product table (EN57 p3: 42/57/60 motors, 24..48 V DC,
5 A, 134 x 83 x 33, weight blank) and page reflow. Its claims therefore match
EN86, not ZH57 or OP57:

| Topic | EN57 | ZH57/OP57 (existing) | Effect on the C-family ledger |
| --- | --- | --- | --- |
| `6041` | Bit7 alarm, bit8 homed, bit10 zero speed, bit12 target effective, bit15 complete (EN57 p12, p15) | Bit7 alarm, bit14 running (ZH57 p12) | A second 57C table; Q02 stays open. LX-D15 |
| Stop and reset words | 15→11 only; no 271, no 128 (EN57 p12) | ZH57 p12 lists 271 and 128 | LX-D15 |
| Velocity order | Enable first, then target (EN57 p13) | Target first, then enable starts motion (ZH57 p13) | C17/Q03 extends to 57C |
| `2400:0B..0E` | 0B polarity, 0C..0E DO0..DO2 with register 0x2B..0x2E (EN57 p8, p19) | ZH57 p8 same as EN57; ZH57 p19 0C..0F with register 0x2B..0x2E | C01 now has a 57C English witness. LX-D16 |
| DO functions 2/3 | p8: 2 running, 3 homing complete; p19: 2 homing complete, 3 drive status | Existing C02 | C02 extends to 57C |
| `60FE` labels | "Physical Outputs", "Bit mask", no DO bit labels (EN57 p17) | ZH57 p17 DO1/2/3, p8 DO0/1/2 (C03) | Does not resolve Q07. LX-D19 |
| `2301` | Alarm reset listed (EN57 p18) | Not in ZH57 dictionary | EN57 is the only 57C source for `2301` |
| `2401..2407` | Defaults 2 (EN57 p8) | Same | C16 unchanged |
| SW8 | p4 calls it an RS485 terminator; p9..10 CAN termination | ZH57 p9..10 | LX-D18 |

## Interface and protocol

| Topic | TL-C | CL2 and CL2xxC-ABZ | Change against the C family |
| --- | --- | --- | --- |
| Connector | 5-pin terminal: 1 CANL, 2 CANH, 3 GND, 4 CANH, 5 CANL (TLCEN p6) | RJ45: 1 CAN_H, 2 CAN_L, 3/5/6 GND (CL257 p6) | TL pins are a daisy chain, not a second controller; "2 x 1 Mbps" is repeated (TLCEN p3) |
| Node ID | DIP SW1..SW4 = nodes 1..15, all OFF "custom" (TLCEN p9); `2203` 0..127 (p17) | DIP SW1..SW5 = nodes 1..31; no all-OFF row (CL257 p7..8); `2203` 0..127 (p16) | Fewer DIP nodes on TL-C; custom path undocumented. LX-D12, LX-D32 |
| Bitrate | SW5 OFF = 1 Mbit/s; SW5 ON = value in `2407` (0 1M, 1 500k, 2 250k, 3 125k), "decimal address 20" (TLCEN p9); save and restart required (TL57OP p5) | SW6/SW7: OFF/OFF 1M, ON/OFF 500k, OFF/ON 250k, ON/ON 125k (CL257 p8) | TL-C adds a software bitrate object (none existed for CL86-C). LX-D03, LX-D13 |
| Termination | SW6 (TLCEN p9) | SW8 (CL257 p7) | Position changed on TL-C |
| NMT, boot-up | TLEDS `SimpleBootUpSlave=1`; no manual text | Undocumented | Unchanged for TL-C (Q18); unknown for CL2 |
| Heartbeat, guarding | TLEDS `1017` default 0 (l.236); no `1016`, `100C`, `100D` | Undocumented | No consumer heartbeat or host-loss reaction documented anywhere |
| SDO | TLEDS `1200:01/02` (l.244); TL57OP p7 shows expedited command specifiers 2F/2B/27/23 write, 40 read | Undocumented COB-IDs | TL57OP p7 is the first Lichuan frame example; it says nothing about responses or aborts |
| PDO | TLEDS same four RPDO/TPDO records and defaults as C-EDS | Four mapping records each with four U32 slots (CL257 p14..15); no communication records or defaults | CL2 PDO COB-IDs, types and defaults unknown. LX-D20 |
| SYNC, EMCY | TLEDS `1005` 0x80 (l.204), `1014` node+0x80 (l.228) | Undocumented; "EMCY" listed as supported (CL257 p4) | Unchanged for TL-C |
| LSS | TLEDS `LSS_Supported=0` (l.34) | Undocumented | Unchanged |
| Other ports | MicroUSB debug port (TLCEN p7, TL57OP p3, p6) | Type-C, station 1, 115200 baud, no parity (CL257 p6, p10) | RS232 port of the C family is replaced |

## Drive state machine against CiA 402

TL-C repeats the C-family state description: bits 0..2 follow controlword
0/6/7/15 as 000/001/011/111 (TLCEN p11), enable by 6, 7, 15 at about 10 ms
spacing (TLCEN p11; TL57OP p7 says at least 10 ms), stop 15→271, emergency stop
15→11, fault reset by bit 7 then manual return to 0 (TLCEN p14) or 15→128
(p11). The dialect is the ZH86 one (alarm bit3, running bit14), although the
manual is English (LX-D06). TL57OP p7 contradicts the user manual by enabling
with 0x010F, a word whose bit 8 the user manual lists as "stop", and by starting
moves and homing with 0x011F (LX-D08). Neither manual describes `605D` options,
`605A`, `6085` or a brake sequence; the brake is a DO function (code 5) only.

CL2 uses the same controlword list (CL257 p12) but a different statusword: bit3
alarm and bit10 a mode-dependent completion flag (p12, p14). Under the EN86
table bit10 means zero speed; under the CL2 table it means positioning or
homing complete. A decoder chosen by the wrong table would report completion
as standstill or the reverse (LX-D23). `2100` bit4 has no stated polarity
(LX-D28). CL2 DO function 7 "motor enable signal" (CL257 p10) is new.

EN57 repeats the EN86 state description (EN57 p12, p15), so for CL57-C/OL57-C
there are now two English-versus-Chinese tables exactly as for CL86-C.

No extension source documents a quick-stop option code, halt behavior beyond
the 271 example, a fault-reaction option or a CiA 402 state such as "switch on
disabled". Nothing documents what the drive does when the host stops
communicating.

## Supported modes and their objects

All extension manuals document `6060` = 1 profile position, 3 profile velocity
and 6 homing only (TLCEN p10, CL257 p11, EN57 p11). No source documents torque,
CSP, CSV or CST. Profile position and velocity objects and ranges are unchanged
(table above). Velocity start order: TLCEN p12 and CL257 p13 put target and
ramps first and say that enabling starts motion (Chinese ordering, as ZH86),
while EN57 p13 enables first (EN86 ordering).

Homing methods:

| Source | Methods | Notes |
| --- | --- | --- |
| TLCEN p12..14, TL57ZH p11..13 | 17, 18, 24, 29 | Same descriptions as EN86 (withdraw and creep back; 29 ends on leaving the origin); TL57OP p7 example uses 28 |
| EN57 p13..15 | 17, 18, 24, 29 | Same as EN86 |
| CL257 p21..31 (all CL2 manuals) | 1..14, 17..30, 33, 34, 35 | Edge-defined CiA 402-style descriptions with Z pulse for 1..14, 33, 34; 17/18 end at the limit falling edge; 24 and 29 end at a home rising edge after reversals; 35 sets the current position using `60E6` |

The CL2 table reuses the method numbers 17, 18, 24 and 29 with different
documented trajectories and end points (LX-D26). Methods with a Z pulse have no
documented Z source on the 17/23-bit CL2-57C/CL2-86C (CL257 p6 asks the user to
contact the manufacturer for ABZ encoders).

## Units and scaling

Unchanged documentary units for TL-C and CL2: positions in command pulses
(`607A` -1000000..1000000), speeds in rpm 0..3000, ramps in ms 0..2000, `2201`
subdivision 400..51200 (TLCEN p10, p17; CL257 p11, p16). New facts:

- TL57OP p7: `2502` 1000 corresponds to 1 A, an example for the mA unit already
  stated (EN86 p19).
- CL2 encoders: 17-bit (131072) or 23-bit (8388608) per revolution as values of
  P00-13 (CL257 p10), which the dictionary maps to U16 `2501` (CL257 p17;
  LX-D21). Command pulses per revolution remain `2201`, independent of the
  encoder (existing Q04).
- CL2xxC-ABZ: 1000-line ABZ encoder (ABZ57 p4); whether `2501` holds lines or
  quadrature counts is not stated.
- TLC models: 1000-line encoder; TLO models none (TLCEN p3). As for OL86-C,
  TLO reported position is not measured feedback.

## Limits and protection

| Topic | TL-C | CL2 | Delta |
| --- | --- | --- | --- |
| Soft limits | `2202`, `607D` as C family (TLCEN p16..17) | `2202` P05-0F, `607D` (CL257 p16, p19) | Unchanged (Q05) |
| Limit and home inputs | DI codes 1/2/3 (TLCEN p8) | DI codes 1/2/3 (CL257 p10) | Unchanged codes; CL2 code 4 is "release" |
| Position error | LED 5 flashes, resettable (TLCEN p19) | Err5 "power-off or manual reset", `250A` 10..4000 (CL257 p18, p20) | New bounds for CL2 |
| Over/undervoltage | LED, restart reset (TLCEN p19) | Err2/Err3 automatic recovery (CL257 p20) | Reset behavior differs. LX-D30 |
| Encoder faults | Not documented | Err7 encoder ROM data, Err8/Err9 battery, Err10 encoder communication (CL257 p20); absent in ABZ57/ABZ86 p20 | New CL2 faults |
| Host loss | Not documented | Not documented | No watchdog, communication timeout or consumer heartbeat anywhere |

## I/O

TL-C has five single-ended inputs DI0..DI4 and two differential outputs
DO0±/DO1± (TLCEN p6), with defaults DI0 origin, DI1 positive limit, DI2 negative
limit, DO0 alarm (TLCEN p7..8). The wiring figure and TL57OP p4 call the outputs
DO1/DO2 and show DO2 as brake release; TLCEN p7 keeps the C-family note that
DOCOM may only go to 0 V although TL-C has no DOCOM pin; TL57OP p4 gives the DI
voltage as 12..24 V against 24 V in TLCEN p6 (LX-D10).

CL2 keeps seven inputs and three outputs (CL257 p6) with defaults DO0 alarm,
DO1 in-position, DO2 motor enable (p10), the new DI and DO function codes above,
and `60FE:01/02` bits 16..18 for DO0..DO2 when both are set (p10). Its DI range
is printed as 0..22 although only 0..6 are described (p17).

## Persistence

`2300` = 2 saves and 1 restores factory settings for TL-C (TLCEN p14, p17) and
CL2 (CL257 p14, p16), as for the C family; no list of saved fields, completion
signal or duration is given (Q15 unchanged). New persistence side effects:

- TL-C bitrate: a changed `2407` (or, per TLEDS, `2402`) value takes effect only
  after a parameter save and a drive restart (TL57OP p5).
- CL2 motor matching: P00.03, P00.12 and P00.13 are written with the vendor tool
  over Type-C after P02.19=1 and P00.00=65535, then saved and restarted
  (CL257 p10). No CANopen path is documented for P02.19 or P00.00.
- CL2 battery fault: reset by P0D.07 after battery replacement (CL257 p20); no
  CANopen index is given.

## Timing statements

TLCEN p11 and CL257 p12: about 10 ms between enable words; TL57OP p7: at least
10 ms. TL-C LED overvoltage "twice every 2 seconds" (TLCEN p19) against every
5 seconds elsewhere (LX-D14). No SDO response time, save duration, restart time,
bitrate-change delay or minimum PDO period is documented for any extension.

## Vendor objects with new or changed side effects

| Object | Family | Side effect | Source |
| --- | --- | --- | --- |
| `2407` (manual) or `2402` (EDS) | TL-C | Selects the CAN bitrate when SW5 is ON; effective after save and restart, after which the node may be unreachable at the host bitrate | TLCEN p9, TL57OP p5, TLEDS l.1569 |
| `2401` | TL-C | Single DI filter per TLCEN p8; per TLCEN p17 only X0 | TLCEN p8, p17 |
| `2203` | TL-C, CL2 | Stored node ID; relation to DIP "custom" undocumented | TLCEN p9, p17; CL257 p16 |
| `2500` | CL2 | Value 3 selects closed loop; 2 undocumented | CL257 p17 |
| `2501` | CL2 | Encoder resolution of the motor; wrong value presumably breaks closed-loop control (inference) | CL257 p10, p17 |
| `2502` | TL-C, CL2 | Peak current in mA; CL2 bounds 0..7000 or 0..8000 | TL57OP p7, CL257 p17, CL286 p17 |
| `2301` | CL2 | Alarm reset documented; TL-C EDS-only | CL257 p16; TLEDS l.1404 |
| `60FE:01/02` | CL2 | DO0..DO2 output when the bit is set in both subentries | CL257 p10 |

## Existing C-family decisions that do not transfer

| Existing decision | Where it is recorded | Why it does not transfer |
| --- | --- | --- |
| Status tables EN86, ZH86, ZH57, OP86 (implemented `StatusLayout`) | lichuan_review.md status section; Q02 | Each table is tied to its document. TL-C needs its own TLCEN/TL57ZH table (bit positions as ZH86); CL2 matches no table; EN57 is a new 57C table. Choosing an existing layout for a new model would be a guess |
| Homing methods 17/18/24/29 and their trajectories (implemented `HomingMethod`) | lichuan_review.md motion table; Q10, Q21 | CL2 documents different trajectories for the same numbers and 25 more methods; TL-C repeats the C descriptions only on paper |
| `2401..2407` raw filter coefficients (C16, Q08) | lichuan_objects.json C16 | On TL-C these indices hold a bitrate selector and reserved entries; any write is a network-configuration change |
| Output-address conflict C01 and DO functions C02 (Q06, Q07) | lichuan_objects.json C01, C02 | TL-C has two outputs and three layouts; CL2 documents the C-EDS layout plus a new function 7; neither resolves the CL86-C firmware |
| DI function codes 0..13 (Q19) | lichuan_review.md I/O table | CL2 code 4 means release, not enable/MF, and 7..13 do not exist |
| `2500` 1 open / 2 closed and `2501` encoder (Q09) | lichuan_review.md tuning table | CL2 uses 3 for closed loop and documents encoder values that U16 cannot hold |
| DIP decoding SW1..SW5 node, SW6/SW7 bitrate, SW8 termination (Q16) | lichuan_review.md network section | TL-C uses SW1..SW4, SW5 and SW6; CL2 drops the all-OFF entry |
| "No software bitrate object; do not invent one" (Q16) | lichuan_review.md network section | TL-C documents one; it must be modelled as an explicit configuration action, never written implicitly |
| `2301` availability | EN86 p18 versus ZH86 p18 | TL-C manuals omit it; CL2 documents it; per-model choice needed |
| Default alarm clear by 6040 bit 7 (bench decision 2026-11-13) and measured one-byte `1009`/`100A` widths | `Actions.h`, lichuan_objects.json `identity_width_binding` | Both come from CL86-C specimens; they are not TL-C or CL2 observations |
| `2100` monitor bits 0..6 | lichuan_review.md status section | CL2 bits 5/6 are limit inputs, not soft-limit states; bit4 polarity is unstated |
| EDS-derived PDO, SYNC, EMCY and heartbeat defaults (Q17, Q18, Q27, Q28) | lichuan_review.md PDO section | Repeated by TLEDS for TL-C; no source at all for CL2 |
| LED fault reset classes | lichuan_review.md motion section | CL2 Err codes have automatic or power-off resets and encoder faults |
| CL86-C bench limits and timings (for example the velocity ladder) | [docs/bench](../../bench/20261113-ladder/ladder.jsonl) and the prompt reports | Fixture observations of CL86-C specimens and their firmware |

## What a profile must not assume

- That `1000`/`1018`, `1009`/`100A` or `2000` select TL-C, CL86-C or DS-C
  (D18), or that a CL2 drive reports any particular identity.
- That a CL86-C write to `2401..2407` is harmless on a TL-C, or that `2402`
  and `2407` have the EDS defaults' meaning.
- That TL-C "custom" DIP settings use `2203`, or that `2203` or the bitrate
  object take effect without save and restart.
- That `6041` bit10 means zero speed on a CL2, or that any C-family status
  table applies to a TL-C or CL2.
- That homing method numbers mean the same trajectory on CL2 and CL86-C.
- That 0x010F/0x011F (TL57OP p7) are correct enable/start words.
- That `2500` = 2 is closed loop on CL2, or that `2501` can be set to a 17/23-bit
  resolution over CANopen.
- That TL-C has seven inputs or three outputs because the EDS and dictionary
  list them.
- That CL2 has the C-EDS PDO communication defaults, heartbeat or EMCY layout.
- That any extension stops the motor when the host disappears.

## Discrepancies

- **LX-D01** TLEDS `1018:01/02` = 0x1000/0x0001 (l.83, l.91) equal C-EDS
  (l.287, l.296) and DSCEDS (l.270, l.279) while vendor objects differ (D18).
  No TL manual prints `1018`.
- **LX-D02** TLEDS declares `1018:00` = 4 and sub04 serial (l.75, l.107); C-EDS
  declares count 3 with HighLimit 4 and no sub04 (A01). The identity structure
  differs but the live count of neither product is known.
- **LX-D03** TL-C bitrate object: TLEDS names `2402` `Obj.BaudSet` with default
  40 (l.1569) and `2407` `Obj.RSVD` default 16 (l.1609); TLCEN p9, TL57ZH p8 and
  TL57OP p5 use `2407-00` with codes 0..3 at "decimal address 20"; TLCEN
  p17..18 and TL57ZH p17 list 2401..2407 as X0..X6 filter coefficients; TLCEN
  p8 lists one DI filter at `2401`. Register address 20 (0x14) fits neither
  `2402` (0x31) nor `2407` (0x36) in the dictionary's register column. Neither
  EDS default is a valid code.
- **LX-D04** TLEDS and the TL dictionaries describe seven inputs and three
  outputs (`2101`, `2102`, `2400:02..08`, `2400:0C..0F`, `2401..2407`) while the
  hardware has five inputs and two outputs (TLCEN p6..7); TLCEN p8 and p17 give
  different output subindex lists; TLEDS uses the C-EDS layout (0C polarity,
  0D..0F).
- **LX-D05** TLEDS `CompactPDO=31` (l.30) against 0x00 in C-EDS (l.34); the
  `[DynamicChannels]` section is dropped. Effect not reviewed (CiA 306 not
  obtained).
- **LX-D06** TL-C statusword (TLCEN p11, TL57ZH p10): bit3 alarm, bit8, bit14
  running, bit15; this is the ZH86 layout in an English manual, unlike EN86 and
  EN57.
- **LX-D07** TL-C velocity start: TLCEN p12 writes targets then enables, and
  enabling starts motion; TL57OP p7 sets `60FF` after enabling, so writing the
  target starts motion, and says that writing 0 stops it.
- **LX-D08** TL57OP p7 (identical in TL42OP, TL60OP, TL86OP): the third enable
  frame writes 0x010F (271, the documented "stop" word) instead of 15; moves and
  homing start with 0x011F instead of 31; the homing example sets method 28,
  which no TL source documents, with a stray byte 0x1E; the creep speed is
  labelled "100ms".
- **LX-D09** TL ratings: TL42 supply 24..36 V (TLCEN p3), 20..36 V (TL42ZH p3),
  24..36 V (TL42ZH p5), 18..36 V (PG-TL42); current 2.0 A (TLCEN p3) against
  2.8 A (PG-TL42); TL57/TL60 4.0 A against 5.6 A and TL86 6.0 A against 7.7 A
  (pages); TL86 24..70 V (p3) against 24..48 V (TLCEN p7 figure); TL57
  recommended 36 V (TLCEN p7) against 48 V (TL57OP p3); every TL product page
  shows a 24..50 V power-terminal row; PG-TL60 headline 20..50 V.
- **LX-D10** TL outputs are DO0/DO1 in TLCEN p6..8 and DO1/DO2 in TLCEN p7
  figure and TL57OP p4; TLCEN p7 keeps a DOCOM-to-0 V note without a DOCOM pin;
  DI voltage 24 V (TLCEN p6) against 12..24 V (TL57OP p4).
- **LX-D11** `2301` alarm reset is absent from TLCEN p17 and TL57ZH p16 but
  declared by TLEDS l.1404.
- **LX-D12** TL-C node selection: DIP covers nodes 1..15 with all OFF "custom"
  (TLCEN p9), `2203` allows 0..127 (p17); which value wins, when it applies and
  how node 0 is treated are undocumented. The product claim of 32 axes
  (TLCEN p3) exceeds the DIP range.
- **LX-D13** TL-C bitrates: DIP offers only 1 Mbit/s or "custom"; codes 0..3
  cover 1M/500k/250k/125k (TLCEN p9) while TLEDS advertises 50 kbit/s too
  (l.20); product pages state "baud rate 1000K" (PG-TL42..PG-TL86).
- **LX-D14** TLCEN translation defects: `250B` named "positioning completion
  time" (p18; TL57ZH p18 "threshold"); `2101` "1: input level inactive" (p16);
  overvoltage LED "every 2 seconds" (p19); the velocity procedure says
  "position mode" (p12); both power pins called "negative terminal" (p6).
- **LX-D15** EN57 statusword and controlword tables (EN57 p12: bit7, 8, 10, 12,
  15; no 271 or 128) disagree with ZH57 p12 (bit7, 14; 271 and 128): the
  EN86/ZH86 split repeats for 57C.
- **LX-D16** EN57 p8 and p19 give `2400:0B` polarity and `0C..0E` DO0..DO2 with
  register addresses 0x2B..0x2E; ZH57 p19 gives `0C..0F` with the same register
  addresses; C-EDS gives 0C..0F. The register column suggests the ZH57 subindex
  column is shifted (inference only). C01 now spans 57C.
- **LX-D17** 57C supply: 24..48 V DC (EN57 p3) against 24..50 V (EN57 p6,
  PG-CL57EN); EN57 calls 5 A "maximum output current", PG-CL57EN "maximum peak
  current"; EN57 p3 leaves the weight blank, PG-CL57EN gives 0.3 kg.
- **LX-D18** EN57 p4 labels SW8 an RS485 terminator while EN57 p9..10 describe
  CAN termination (same as EN86 p4).
- **LX-D19** EN57 p17 gives `60FE:01/02` no bit labels, so the ZH57 DO0..2
  versus DO1..3 conflict (C03, Q07) is not resolved.
- **LX-D20** CL2 publishes no EDS; its manuals list only `1000`, `1009`, `100A`,
  `1600..1603`, `1A00..1A03` among communication objects (CL257 p14..15). No
  `1018`, `1005`, `1014`, `1017`, `1200` or PDO communication parameters are
  documented.
- **LX-D21** CL2 `2501` (P00-13) is U16 0..65535 (CL257 p17), but CL257 p10 sets
  P00-13 to 131072 or 8388608.
- **LX-D22** CL2 `2500` uses 3 for closed loop (CL257 p17); the C family uses 2
  (EN86 p19, TLCEN p18).
- **LX-D23** CL2 `6041` bit10 means position complete in mode 1 and homing
  complete in mode 6 (CL257 p12, p14); EN86/EN57 bit10 means zero speed; CL2 has
  no bit8, 14 or 15 meaning.
- **LX-D24** CL2 `2400:0C` polarity (three bits) and `0D..0F` DO0..DO2
  (CL257 p10, p17) differ from EN86; `2102` labels the same outputs DO1..DO3
  (p16).
- **LX-D25** CL2 DI function 4 is "motor release" (CL257 p10, p17) where the C
  family has enable/MF; codes 7..13 are absent although the range is printed
  0..22.
- **LX-D26** CL2 homing appendix (CL257 p21..31) gives 17/18/24/29 different
  trajectories and end points from the C family (EN86 p13..14) and adds methods
  1..14, 19..23, 25..28, 30, 33..35; it is titled "servo homing methods", and
  Z-pulse methods have no documented Z source on CL2-57C/86C.
- **LX-D27** CL2 method 35 (CL257 p31) uses `60E6` and names `606C` as the
  offset; neither appears in the dictionary with that role (`606C` is velocity
  actual, p19; `607C` is home offset).
- **LX-D28** CL2 `2100` bit4 "motor enable" has no polarity and bits 5/6 are
  limit inputs (CL257 p15..16), unlike C-family bit4 0=enabled and soft-limit
  bits (EN86 p17).
- **LX-D29** CL2 motor matching (CL257 p10) uses P02.19=1 and P00.00=65535
  without explaining them, through the vendor tool; ABZ57/ABZ86 omit the
  section.
- **LX-D30** CL2 faults (CL257 p20): Err2/Err3 recover automatically where the C
  family requires a restart; Err5 needs power-off or manual reset; Err9 is
  reset by P0D.07 without a CANopen index; Err6 refers to RS485 Modbus frames on
  a CANopen drive; `603F` has no code list.
- **LX-D31** CL2-86C and CL286C-ABZ accept 30..110 V DC or 20..80 V AC with
  terminals labelled AC/AC (CL286 p4..5); the C-family 86C conflict (C19) does
  not carry over, but the ranges are new fixture limits.
- **LX-D32** CL2 node table starts at 1 with no all-OFF row (CL257 p7..8); what
  all OFF selects is undocumented.
- **LX-D33** CL2 repeats C-family typos: `6098` I8 (p19) against U8 (p13),
  `6099:02` "creep time (rpm)" (p13), `607A` I3C (p19).
- **LX-D34** CL2-57C/86C encoder pins are labelled for a serial encoder
  (EA+/D-, EA-/D+) with ABZ "on request" (CL257 p6); ABZ57 p6 lists A/B/Z. The
  `2501` unit for ABZ (lines or counts) is not stated.
- **LX-D35** `1000`, `1009`, `100A` and `2000` defaults are identical in TLEDS and
  C-EDS; no published value lets a host tell a TL-C from a CL86-C.

## What a profile can implement from this evidence

- **TL-C**: a model-scoped variant of the C profile, bound only by an explicit
  application model declaration. Profile position, velocity and homing
  (17/18/24/29) objects and ranges are documented as for the C family; the
  statusword table must be a new TLCEN/TL57ZH table; outputs and inputs must be
  limited to DO0..DO1 and DI0..DI4. `2401..2407` writes, `2203` and the bitrate
  object must be excluded from ordinary settings; a bitrate change can only be
  an explicit, documented configuration action with save, restart and route
  rebinding. Everything else stays gated as in Q01..Q28 and needs TL-C bench
  qualification.
- **CL2 and CL2xxC-ABZ**: a separate ledger from the manuals is required before
  implementation, since there is no EDS and the statusword, homing table,
  `2500`, `2501`, `2400`, `2100` and DI codes differ. Live reads of `1018`,
  `1400..1A03` and `1017` are the first measurements. pp/pv/hm sequences could
  follow the documented controlword list once the CL2 status table and method
  semantics are qualified.
- **CL57-C/OL57-C**: EN57 adds an English 57C source; it extends C01, C02, C17
  and Q02 to 57C rather than resolving them. No new capability follows.
- Nothing here supports torque, cyclic modes, host-loss detection or
  identity-based selection for any extension.
