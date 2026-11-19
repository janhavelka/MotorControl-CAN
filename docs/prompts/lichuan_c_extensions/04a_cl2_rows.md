# 04a — CL2 rows: mode-dependent status, encoder and loop rows, absent communication objects

Execute this stage under the [execution contract](execution_contract.md).
Status: NOT STARTED.

Scope: CL2-57C, CL2-86C, CL257C-ABZ and CL286C-ABZ. No EDS exists; every row comes from the CL2 manuals
(CL257, CL286, ABZ57, ABZ86), which give a P-parameter beside each vendor object (CL257 p14..19). The
statusword, loop-mode codes, encoder field, input codes, monitor bits and homing table differ from the C
family. This stage delivers the reads and decoders; [04b](04b_cl2_operations.md) admits operations. It is
independent of 03a/03b.

## Work

1. Communication objects. Only `1000`, `1009`, `100A`, `1600..1603` and `1A00..1A03` are listed (CL257 p14..15;
   LX-D20). `1018`, `1005`, `1014`, `1017`, `1200` and every PDO communication record are undocumented (X09).
   SDO on `0x600+node`/`0x580+node` rests on the CiA 301 predefined set (principles pack section 2) and the
   `1000` row's text "supports CiA301 and CiA402" (CL257 p14): a protocol claim, not CL2 behavior; 07a verifies.
   Passive heartbeat, boot-up and EMCY observations stay raw and unqualified ("EMCY" is a feature claim on
   CL257 p4). No PDO map, COB-ID, heartbeat, SYNC or NMT configuration path admits a CL2 binding, and 03a
   item 6's interface isolation (no SYNC, no node-0 NMT) applies from this stage to every interface holding
   a CL2 binding, with the same tests. Every CL2
   read plan holds only objects with a CL2 row: the state read's error block plans `603F` without `1001`, and
   the error-history block (`1003`) is refused, since neither is listed (CL257 p14..15).
2. Identity. A CL2 identity read plans the documented `1000`, `1009`, `100A` (CL257 p14), `2000` "drive model
   code" and `2001` "drive version" (RO U16, CL257 p15, P0B-00/P0B-01, no code table). `1018:00..03` is
   undocumented, and this is the one undocumented read the set admits: a caller-selected option of the
   identity read, off by default, uploads each subindex once as an explicit single read (`NOT_REPEATABLE`, no
   marker, no retry, never a download), and keeps a reply or an abort raw. An abort is evidence about
   `1018`, never "not a CL2" or any model. `1009`/`100A` are U16 (CL257 p14), so the read starts with
   `IdentityWidths::DECLARED_U16`; a wrong-width failure is retained, and the width used is reported with the
   firmware fields, never deciding the outcome (stage 01 item 6). The result feeds stage 01's identity check
   (X01); without `1018` a reference holding it stays undecided.
3. Read effects. DEC-1 does not transfer; CL2 uploads are explicit single reads, `NOT_REPEATABLE`, never
   polled, until the owner records a CL2 read acceptance. Because `1018` is undocumented, the default
   `SdoReuse` markers (`1018:01..03`) do not apply: a CL2 binding is set to `SdoReusePolicy::NONE` through
   `setSdoReuse` before its first admission and again after every rebinding, with the `ModelBinding`
   attestation and tests of 03a item 2, so the owner's automatic marker never reads `1018`, until a reviewed
   marker object exists. Seam S10 keeps Lichuan's default for `lichuan_c` bindings, so this stays here.
4. Mode-dependent statusword (X08, LX-D23). CL257 p12 and p14: bit3 alarm; bit10 "position complete" when
   `6060`=1 and "homing complete" when `6060`=6; no meaning for bits 8, 14 or 15; nothing for bit 10 in mode 3.
   Add a CL2 layout value and a mode-aware interpretation (proposed `interpretStatuswordInMode`) taking the
   `6061` display bytes from the same acquisition and the operation's commanded mode; disagreement or mode 3
   gives `UNKNOWN` for bit 10. `6041` and `6061` are separate uploads, so one acquisition is not an atomic
   sample, and a `6061` reply in the commanded mode after the statusword does not show which mode produced
   it. A CL2 status acquisition therefore brackets the statusword: `6061`, `6041`, `6061`, consecutive
   explicit uploads under the operation's continuous reservation, which excludes other changing work on the
   node. Bit 10 has a meaning only when both `6061` replies equal the commanded mode and all three replies
   belong to the current node generation and server session; anything else gives `UNKNOWN`, and a
   completion needs the low-to-high sequence over successive bracketed acquisitions in that mode (04b item
   1). Residual, stated in Doxygen: a mode changed and restored between the two `6061` reads by another
   master or the vendor software is not detected; the application keeps both away while an operation runs.
   The mode-less `interpretStatusword` refuses the CL2 layout (`INVALID_CONFIG`), so no caller can read bit
   10 without a mode. `STATUS_EN86_ZERO_SPEED` never applies to a CL2 binding, and pairing refuses every C-family
   and TL-C layout for CL2 models; CL2 operations take samples only through the model-taking getter, as 03a
   item 3 requires for TL-C.
5. Monitor bits (X12, LX-D28). `2100` (CL257 p15..16): bit0 in position, bit1 homing complete, bit2 running,
   bit3 alarm, bit4 "motor enable" with no polarity, bit5 positive and bit6 negative limit signal. So
   `MONITOR_RELEASED` and the soft-limit constants do not apply; a CL2 monitor observation gives these labels,
   bit4 raw. `2101` DI0..DI6 bits 0..6 and `2102` labelled DO1..DO3 (CL257 p16) against `2400:0D..0F` DO0..DO2
   (LX-D24): raw with both labels.
6. Loop mode and encoder (X10). `2500` (P02-00): 0 none, 1 open loop, 3 closed loop, range 0..3 (CL257 p17,
   ABZ57 p17); 2 is undocumented and reads as `UNKNOWN` (LX-D22). Add a CL2 loop-mode encoding; the C-family
   `LoopMode` value 2 never reaches a CL2. `2501` (P00-13) is U16 0..65535 (p17) while p10 sets P00-13 to 131072
   or 8388608 through the vendor tool (LX-D21): read raw only, never written over CANopen; the ABZ unit (lines
   or counts) is unknown (LX-D34). Motor matching with P02.19 and P00.00 over Type-C (CL257 p10, LX-D29) stays
   outside the library.
7. Current and tuning rows. `2502` mA 0..7000 (CL257 p17, ABZ57 p17) or 0..8000 (CL286 p17; check ABZ86's page);
   `2503`, `2504`, `2506` 0..100 %, `2507` 0..5000 ms, `2509..2519` the bounds of CL257 p18. `2505`, `2508` and
   `251A..2520` are not listed: no typed CL2 row reads them.
8. I/O rows (X12). Seven inputs, three outputs (CL257 p6). `2400:01` bits 0..6; `2400:02..08` codes 0 none,
   1 origin, 2 positive limit, 3 negative limit, 4 motor release, 5 stop, 6 emergency stop; range printed 0..22
   (CL257 p10, p16..17; LX-D25). Add a CL2 input-function decoding: code 4 is never `ENABLE_MF`, codes 7..22 are
   raw. `2400:09..0B` absent. `2400:0C` polarity bits 0..2, `0D..0F` DO0..DO2 with defaults 1/4/7 and codes
   0..5, 7 motor enable, 9..11 (CL257 p10, p17; LX-D24). `2401` single DI filter 0..1024, default 0 (CL257 p17);
   `2402..2407` undocumented, never read as typed rows. `60FE:01/02` bits 16..18 drive DO0..DO2 when set in both
   (CL257 p10).
9. Faults (X13, LX-D30). `603F` has no code list; LED codes Err1..Err10 (CL257 p20), with Err7..Err10 absent from
   ABZ57/ABZ86 p20, Err2/Err3 recovering by themselves and Err9 reset by P0D.07 with no CANopen index. FF01/FF05
   decoding does not transfer; `603F` stays raw for CL2.
10. Node and bitrate. DIP SW1..SW5 nodes 1..31, SW6/SW7 bitrate with the C-family table, SW8 termination
    (CL257 p7..8). `2203` (P02-23, 0..127) stays unwritten. Ratings: 24..50 V for the 57 drives, 30..110 V DC
    or 20..80 V AC for the 86 drives (LX-D31; X14). Intake correction (old claim, evidence, scope,
    disposition). Old claim: LX-D32, "no all-OFF row (CL257 p7..8); what all OFF selects is undocumented".
    Primary evidence: the CL257 p7 table starts at node 1, but the panel drawing on CL257 p8 labels SW1..SW5
    all OFF "自定义" (custom), as TLCEN p9 does for TL-C (rendered pages checked 2026-11-19). Scope: CL2-57C;
    compare CL286, ABZ57 and ABZ86 the same way. Disposition: LX-D32 stays open with both claims; what custom
    selects, and whether a stored value such as `2203` takes part, is undocumented, which is one reason a CL2
    save keeps a route uncertainty (04b item 11, X18); the LX pack review is not edited.
11. Typos (LX-D33): `6098` I8 against U8, `6099:02` "creep time (rpm)", `607A` I3C; the reviewed I32/U8 forms
    follow the C-family resolution only where the CL2 page agrees.

## Deliver

CL2 layout value, `interpretStatuswordInMode`, CL2 loop-mode and input-function decodings, CL2 monitor, I/O and
tuning observations with Doxygen; `test/support/LichuanCl2Drive.h` (fake server answering only manual-listed
objects, with undocumented objects configurable as abort or value, labelled synthetic); `test/cl2_reads_test.cpp`
(bit 10 by mode including mode 3, disagreement, a mode or generation change between reads, and a statusword
sampled while the first `6061` showed another mode and the second the commanded one with the node generation
unchanged, giving `UNKNOWN`; mode-less refusal, `2500`=2 unknown, `2501` raw, code 4; the identity read
with the `1018` option off (no `1018` frame) and on (exactly one upload per subindex, abort and reply kept
raw, no marker, no retry, no download, no model inferred from an abort); state reads without `1001` or
`1003`; `2402..2407` never planned); ledger, coverage, CHANGELOG, report and `PROGRESS.md`.

## Acceptance

A CL2 binding reads identity, state and configuration through the real owner with CL2 meanings; no decoder
reads bit 10 as zero speed or without a mode; no path plans a write to an undocumented object, and no path
plans a read of one except the caller-selected `1018:00..03` identity option; communication configuration
refuses CL2. Quick and full verification, firmware builds and exact-commit CI
pass. Hardware NOT RUN; X08, X09, X10, X12 and X13 stay open.
