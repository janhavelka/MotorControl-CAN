# 03 — Identity, layout selection, typed reads and capabilities

Execute this stage under the [execution contract](execution_contract.md).
Status: NOT STARTED. Requires 02b. MO-D01, MO-D09, MO-D11, MO-D19, MO-D20 and MO-D27 gate interpretations here.

## Work

1. `prepareIdentity`/`nextRead`/`advanceRead`/`getIdentity` read, for a UM21 binding: 0x1000:00, 0x1018:01-04,
   0x2050 product series STRING(4) (UM21 p69, which gives no value; "STF-" and "SSDC" are the EDS-STF and
   EDS-SSDC defaults), 0x2031 DSP version
   STRING(4) (UM21 p68), 0x100A software version and 0x1009 hardware version (UM21 p44), 0x2051 customer name,
   and 0x2032/0x2033 on SSDC only (EDS-SSDC; absent from UM21). Retain every raw string and abort code.
2. Layout and product confirmation: the declared binding selects the profile and its product row the layout;
   identity only confirms them (MULTIDRIVE row 57) and never re-selects the profile. Publish this admission
   table in the coverage file and implement it; each outcome exists only for a current observation, and the
   invalidators of item 10 clear it:

   | Evidence | Established by | Admits |
   | --- | --- | --- |
   | Bootstrap | Nothing | The probe and this stage's identity, config and state reads, kept raw where the layout decides the meaning (0x2100) |
   | UM21 layout and series | Vendor ID 0x000002D9 and a complete 0x2050 upload equal to the declared row's reference, "STF-" or "SSDC" (basis: EDS default; UM21 p69 states no value) | Layout-typed reads (0x2100) and, with each stage's own gates, every changing operation |
   | Exact model (STF05-C, SSDC06-C, ...) | The declaration only; no object names the model | Nothing beyond the series row; model-dependent values stay caller-declared and are labelled so |
   | Firmware | Complete 0x100A and 0x2031 uploads plus a caller-supplied, generation-bound firmware qualification naming the recorded string and its fixture evidence | Hardware-qualified claims only (item 3) |

   Layout outcomes use prompt 57's names and meanings (CONFIRMED is unique among the supplied references, not
   physical proof): CONFIRMED (both values match), CONTRADICTED (another vendor ID; a complete 0x2050 equal to
   the other UM21 row's reference; or stage 13's AMP10 evidence, a 0x2050 abort with the CiA 301
   object-does-not-exist code 0x06020000, which no MOONS' document lists, so the raw code is retained) and
   UNCONFIRMED (no answer, another abort, an incomplete or oversized string, any other value). CONTRADICTED
   refuses every changing operation and layout-typed read before TX, as prompt 57's motion rule does.
   UNCONFIRMED refuses them too, unlike prompt 57: MOONS' has two object layouts with different widths and
   units under one vendor ID and device type, and 0x2050 is the only layout discriminator. Firmware never
   produces either refusal (item 3). A stale observation has no outcome until it is read again. No typed read
   of 0x2100 (user registers in UM21, error status bits in EDS-TSM) happens before CONFIRMED. An AMP10 declaration
   is refused for every operation except the probe until stage 13; do not probe 0x2200 or other AMP10 objects
   here. Host tests confirm through the fake drive's labelled EDS values, never through a production bypass.
3. Firmware: record 0x100A and 0x2031 raw. Which object carries the "310A"/"310B" string of UM21 p6 is not
   stated, and the EDS defaults "300F"/"300E" do not match it (MO-D01); a read equal to an EDS default is a live
   value, not evidence of a firmware row. Publish `firmwareKnown=false` until a caller-supplied,
   generation-bound firmware qualification names the recorded string and its fixture evidence. Firmware is
   reported, not a lock (execution contract): an unknown or unqualified string refuses no operation, and every
   terminal result carries "firmware unqualified" with both raw strings; a missing, incomplete or contradictory
   (changed within one session) string is reported as such. Only recorded evidence of a line below 310A/310B
   refuses. Host tests supply an explicitly synthetic qualification where a qualified result is under test.
4. String transfers: 0x2050 and 0x2031 are STRING(4) and fit an expedited upload. 0x100A, 0x1009 and 0x2051
   lengths are unknown; only expedited SDO is documented (UM21 p11; MO-D27). Each string field has a fixed,
   documented caller-owned capacity within `SdoTransfer`'s 32-byte limit, chosen here, not at stage 12. Report
   each field as complete, incomplete (segmented initiate without a finished transfer), oversized (indicated
   size or data beyond the capacity, transfer aborted locally), server abort (code retained raw) or no answer;
   only a complete field counts, and a prefix or incomplete value never confirms identity. A segmented initiate
   is recorded as evidence of segmented support, not of a transferred value. One field's failure leaves the
   other fields and earlier outputs unchanged.
5. `prepareConfig`/`getConfig` (UM21): 0x6061 mode display (0x6060 is write-only, UM21 p58), 0x6502 as a hint
   only (EDS default 0; principles s14), 0x605A, 0x6085, 0x607F, 0x607E (access conflict, MO-D09), 0x6081,
   0x6083, 0x6084, steps per revolution 0x2604 (STF, RW) or 0x2207 and encoder resolution 0x2218 (SSDC, RO),
   0x2020, 0x2070, 0x1017, 0x1029 sub 1-6 (MO-D11), 0x2060 sub 1-5 (widths per MO-D09), enable input
   0x2617/0x220D, limit definition 0x260B/0x220F, home switch 0x2001 and homing offset mode 0x2009.
6. `prepareState`/`getStateBlock`: 0x6041; 0x603F as the vendor alarm bit field of UM21 p56 (not a CiA 402
   error code); 0x200B DSP status (UM21 p67, width conflict MO-D09); 0x1001; 0x1003 count and entries (low 16
   bits error code, high 16 bits manufacturer-specific, UM21 p42); 0x6064, 0x606C, 0x60F4, 0x60FD; 0x2019
   temperatures in 0.1 °C and 0x2030 DC bus in 0.1 V (UM21 p67-p68); 0x6078 and 0x6074 on SSDC only.
   0x200F stays unread until its RO/WO conflict (UM21 p65 versus p67) is reviewed; reading it never clears it.
7. Typed meanings: decode CiA 402 state from statusword bits 0-6 with the 0x004F/0x006F masks only after
   checking UM21 p16 and p57 against the principles pack s12; bits 10-13 by the mode shown in 0x6061 (UM21
   p37); bits 8, 14 and 15 stay raw (reserved in UM21 p57). 0x603F and 0x200B bits get UM21 names; which alarm
   bits disable the motor is marked in AMP10 p20 only and stays unresolved for UM21 (MO-D19). 0x6064 and 0x606C
   are labelled "calculated" in the EDS; publish them as drive-reported values with `measured=false` until
   stage 05 resolves MO-D20 for the product.
8. Node ID: the reply on 0x580 + node proves the active node ID. Compare its upper three bits with 0x2020
   (values 0-7 select 0x00-0x0F ... 0x70-0x7F, UM21 p68); a difference is reported as "stored value differs from
   active node ID" (pending change or unknown precedence, MO-D16), never as a refusal or a write. 0x2070's
   low-16-bit layout is undocumented (UM21 p71); keep it raw.
9. Capabilities: the caller supplies `Capabilities` per configuration (motor kind, feedback, brake). The profile
   states documented modes per product: STF-C 1, 3, 6; SSDC-C 1, 3, 4, 6 (UM21 p58, p64); -1 Q is stage 10, -2
   refused. Velocity (VL), interpolated and cyclic modes are unsupported (UM21 p58). Fill applicable common
   `ReadCapabilities` and `DiscoveryCapabilities`; MOONS'-only reads use profile-local types.
10. Freshness and invalidation: each value carries acquisition time and node epoch; a stale value is flagged
    and kept. Boot-up, NMT reset, bus-off or controller restart, rebinding, a configuration generation change
    and a contradicting identity read invalidate cached identity, config and state. Each device-facing coverage
    row names the identity it consumes (product row, layout, firmware where its ledger says so) by ledger key,
    its holder (an application record keyed to binding, generation and server session) and these invalidators
    (canopen_shared 02 "Identity and device evidence"); the label and `Capabilities` grant nothing. A node
    held in Initialization by an RS-232 tool (UM21 p9) sends no boot-up; a timeout is reported as
    `NO_ANSWER`, not as absence of the drive.

## Deliver

Profile `Reads.h/.cpp` with `ReadContext`, `Identity`, `ConfigObservation`, `StateBlock` and the firmware
qualification declaration; `test/moons_canopen_reads_test.cpp`; console `read identity|config|state` routes
for MOONS' bindings with API/CLI parity; operation-map, coverage and ledger `coverage` updates; Doxygen; report.

## Acceptance

Tests cover exact read plans per product, each admission-table outcome (mismatching 0x2050, the other row's
0x2050, wrong vendor ID, absent object aborts, stale after each invalidator), unqualified firmware carried in a
terminal result without refusal, width candidates for MO-D09 objects, strings at exact capacity, one byte over,
without an indicated size, with interrupted segments and aborted, with the other fields preserved, malformed and
partial sequences, deadlines, stale and wrong-generation events, unchanged outputs and that no step writes.
The four identity cases of canopen_shared 02 refuse the layout-gated reads with nothing sent. No typed claim
of measured position, firmware row or alarm severity is made without its gate. Quick and full verify, family
checks and exact-commit CI pass. A good SDO reply closes no readiness, unit or completion gate. Hardware: NOT RUN.
