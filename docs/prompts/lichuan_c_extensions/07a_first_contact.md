# 07a — First contact per model

Execute this stage under the [execution contract](execution_contract.md).
Status: NOT STARTED.

Run only when an exact TL-C or CL2 fixture exists, the owner has returned (COMMON's owner-absence section is
no longer in force) and the applicable authorization covers it. The standing authorization and the S2/COM15
fixture describe two CL86-C drives; they establish no TL-C or CL2 fixture. Use a separate bench configuration
and interface for the new drive; never put it on, or address it through, the CL86-C nodes on COM15. Questions
run per model as soon as their route exists: reads after [03a](03a_tlc_rows.md) or [04a](04a_cl2_rows.md),
motion after [03b](03b_tlc_operations.md) or [04b](04b_cl2_operations.md). This stage answers questions; it
does not qualify a release ([07b](07b_qualification_and_release.md) does).

## Work

Hardware is NOT TESTED for every model.

### Before first power

Record nameplate and order code, the declared model, encoder type, supply (inside the narrowest documented
range: LX-D09 for TL-C, LX-D31 for CL2-86), DIP positions (TL-C SW1..SW4 node, SW5 bitrate, SW6 termination;
CL2 SW1..SW5 node, SW6/SW7 bitrate, SW8 termination), wiring of every input (TL-C defaults DI0 origin, DI1
positive and DI2 negative limit are factory claims, TLCEN p7..8), brake, free shaft and transceiver. Derive the
model's forbidden-object list from its ledger rows. Use the production API and the normal debug observer;
retain raw traffic, timing, service gaps, settings snapshots and restoration results. Never replay an
uncertain write; no automatic retry.

### TL-C questions

| Question | Work when prerequisites exist | Remaining limit |
| --- | --- | --- |
| T1 identity (X01) | Read `1000`, `1009`, `100A` (record reply widths), `1018:00..04`, `2000`, `2001`; store a TL-C identity record; compare with the CL86-C record | Equal values mean identity cannot discriminate; one specimen is not a family |
| T2 bitrate candidates (X02) | With SW5 OFF (1 Mbit/s, TLCEN p9), read `2402` and `2407` once each; compare with TLEDS defaults 40 and 16 and codes 0..3 | No write. A selector experiment needs vendor clarification or a reviewed plan with SW5 ON, one code, one save, a restart, a host ready at the new bitrate and SW5 OFF as the documented way back |
| T3 node selection (X03) | Read `2203`; with an owner-set all-OFF DIP, observe which node boots | DIP changes and power cycles are owner actions; no `2203` write |
| T4 status table (X04) | Read `6041`, `6061`, `2100` released, after 6/7/15 and during and after one small finite move | Bits 3/8/14/15 freshness needs transitions after a trigger, not a stable bit |
| T5 enable and start words (X05) | With targets equal to actual position and `60FF`=0, test whether 6/7/15 enables; separately, only with a reviewed plan, whether `0x010F` as third word enables or halts | `0x011F` start and method 28 stay untested without vendor clarification (LX-D08) |
| T6 velocity order (X05) | One small finite velocity run per order (targets first; enable first), stop prepared, duration bounded | Writing 0 is not a stop; host duration is not a drive guarantee |
| T7 I/O (X06) | Read `2400:01..0F`, `2101`, `2102`; map each physical input and, with an isolated load, each output | No reassignment to provoke a function; never disable a limit |
| T8 homing | Methods 17/18/24/29 only with the actual switches and a bounded plan; record final coordinate and bit 8 | Method 28 is not run |
| T9 alarm reset (X07) | Read `2301` once; clear a naturally occurring alarm by `6040` bit 7 | No fault induction without its own plan |
| T10 save | With restart control: change one named field, save once, restart, compare; read `2402`/`2407` before and after | No save campaign; bitrate stays unchanged unless T2's plan runs |

### CL2 questions

| Question | Work when prerequisites exist | Remaining limit |
| --- | --- | --- |
| K1 SDO route (X09) | Probe `1000:00` on `0x600+node`; record reply or silence | A reply proves a CANopen server, not the model |
| K2 identity (X01) | Read `1009`, `100A`, `1018:00..03`, `2000`, `2001`; keep aborts raw; store a CL2 identity record | Abort on `1018` is evidence about `1018`, not about the model |
| K3 communication records (X09) | Explicit single reads of `1005`, `1014`, `1017`, `1200:01/02`, `1400..1403`, `1800..1803`, `1A00..1A03`; passive boot-up and heartbeat observation | Read effects stay unqualified; no writes; owner must accept the read risk first |
| K4 bit 10 by mode (X08) | Mode 1: one small finite move, bit 10 before, after trigger and at completion; mode 6: one homing run with fixtures; mode 3: observe without interpretation | One transition is fixture evidence, not every firmware |
| K5 monitor bits (X12) | `2100` bit4 released against enabled; bits 5/6 against wired limit inputs | Polarity holds for this firmware only |
| K6 loop and encoder (X10) | Read `2500` and `2501`; compare `2501` with the encoder declared at P00-13 by the vendor tool | No `2501` write; no motor matching through CAN |
| K7 homing (X11) | One method per available signal set (limit, home, Z on ABZ), bounded, with final coordinate and bit 10 | Index methods on CL2-57C/86C need a documented Z source first |
| K8 input codes (X12) | Read `2400:02..08`; code 4 (release) only with a reviewed plan and the drive released | No input reassignment to make a test run |
| K9 faults (X13) | Record naturally occurring `603F`, `1001` and LED codes against CL257 p20 | No battery or encoder fault induction |

## Deliver

`docs/reports/lichuan_c_extensions_07a_<model>_<date>.md` per fixture: questions run, prerequisites, raw
evidence paths under `docs/bench/`, results, unresolved measurements; identity records and qualification
inputs entered through ordinary API/CLI routes; ledger gate evidence (X gates stay open unless their stated
evidence is complete for that model and firmware); `PROGRESS.md` hardware column.

## Acceptance

Every question run has retained raw traffic and restoration; every question not run says why (no fixture,
owner away, no authorization, missing signal). No result is generalized beyond its model, firmware and
fixture. Software work never waits for this stage.
