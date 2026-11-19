# 03a — TL-C rows: reads, status table and the bitrate hazard

Execute this stage under the [execution contract](execution_contract.md).
Status: NOT STARTED.

Scope: the eight TL-C models of stage 01. Deliver the TL-C reads, the TL-C status table and the protections
against the BaudSet hazard, so that the operations of [03b](03b_tlc_operations.md) can build on checked
observations. Everything here can be verified on the host.

## Work

1. Identity and probe. The `1000:00` probe and `prepareIdentity` run unchanged for a TL-C binding with these
   differences: TLEDS declares `1009`/`100A` U16 (l.212, l.220), so a TL-C read starts with
   `IdentityWidths::DECLARED_U16`, and a wrong-width failure is retained as evidence. `MEASURED_U8` is CL86-C
   specimen evidence, not a TL-C default. The widths used are recorded with the observation; like every
   firmware field they are reported and never decide `checkModelIdentity`'s outcome (stage 01 item 6, prompt
   57). TLEDS declares `1018:04` (l.107); add an optional serial field
   read only when the caller selects it, with an abort kept raw. The result feeds stage 01's
   `checkModelIdentity`; nothing is inferred from it (LX-D01, LX-D02, LX-D35).
2. Read effects. DEC-1 is an owner risk acceptance for C-family rows and does not transfer. TL-C uploads are
   explicit single reads with `RetryClass::NOT_REPEATABLE`, never polled, until the owner records a TL-C read
   acceptance as a new decision row. The per-binding default `SdoReusePolicy::ACCEPTED_RISK_WITH_MARKER` and
   its markers (`1000:00`, `1018:01..03`, `adapters/owner/CanBusTypes.h`) rest on DEC-1, so a TL-C binding is
   set to `SdoReusePolicy::NONE` through `setSdoReuse` until that decision exists, before its first transfer
   and again after every rebinding (a new channel binding starts with the owner default). The example
   application does this for every new model and checks it through the owner's reuse-setting copy, with a
   test including a rebinding; the Doxygen of the new model values states the same obligation for other
   applications. canopen_shared seam S10, when landed, keeps Lichuan's marker default for every `lichuan_c`
   binding, so this model-level setting stays this set's responsibility.
3. Model-aware typed observations. Group reads keep their wire plans; meaning comes from the declared model.
   A getter that attaches documentary labels takes the model binding; the existing getter keeps its C-family
   meaning and says so in its Doxygen. No TL-C operation (03b) consumes a sample from the existing getter or
   from a binding of another model: its samples come from the model-taking getter, bound to the model, the
   operation and trigger, the mode and the current generations, and a mismatch is refused. For TL-C:
   - INPUT_CONFIGURATION: `2400:01` bits 0..4 and `2400:02..06` codes 0..13 (TLCEN p17); `2400:07..08` raw,
     "no terminal" (TLCEN p6, LX-D04).
   - OUTPUT_CONFIGURATION: three layouts (TLCEN p8: `0B` polarity, `0C` DO0, `0D` DO1; TLCEN p17: `0B`
     polarity, `0C..0E` Y0..Y2; TLEDS: `0C` polarity, `0D..0F`) give raw values only (X06).
   - INPUT_FILTERS: raw only, labelled unresolved: `2401` X0 filter (TLCEN p17) or single DI filter (p8);
     `2402` X1 filter (p17) or `Obj.BaudSet` (TLEDS l.1569); `2403..2406` filters (p18) or `Obj.RSVD`;
     `2407` X6 filter (p18), bitrate selector (TLCEN p9, TL57ZH p8, TL57OP p5) or `Obj.RSVD` (l.1609).
   - `2101` bits 0..4 and `2102` bits 0..1 have terminals; other set bits stay raw. `2100` repeats the C text
     (TLCEN p16) as a documentary claim only.
4. TL-C status table: append a layout value (proposed `StatusLayout::TLC`) for TLCEN p11, identical in TL57ZH
   p10: bit3 alarm, bit8 homing complete, bit14 running, bit15 position complete; bits 0..2 as the 0/6/7/15
   example; homing status TLCEN p14. It has ZH86's positions but its own value (LX-D06). The model-taking
   interpretation always runs a pairing check: it refuses EN86, ZH86, ZH57 and OP86 for a TL-C model and
   `TLC` for a C-family model. TL-C operations use only that form, so the pairing is never optional for them.
5. The bitrate hazard (TLCEN p9, TL57OP p5, TLEDS l.1569; LX-D03, LX-D13):
   - No path writes `2401..2407` for a TL-C model: typed settings, configuration restore, console and examples.
     Test each through the real `CanBusOwner`, `SdoClient` and fake server: refusal before TX.
   - The C-family side has no filter writer today (Q08), so nothing sends a filter coefficient now. Add a
     ledger rule and a test: a future writer of `2401..2407` stays blocked until its own reviewed stage names
     independent evidence of the model binding for the deployment (for example the node's nameplate record)
     sufficient for that write, and it also requires a CONFIRMED identity whose candidate set includes the
     TL-C models with their references. CONFIRMED narrows the risk and does not remove it: it means unique
     among the supplied references (prompt 57), so a TL-C specimen whose identity matches no supplied TL-C
     reference and equals the CL86-C one would be CONFIRMED as a CL86-C.
   - The TL-C bitrate change stays BLOCKED (X02): the selector index is unresolved (`2407` in TLCEN p9,
     `2402` in TLEDS; "decimal address 20" fits neither register `0x31` nor `0x36`, TLCEN p17..18), it acts only
     with SW5 ON, which no object reports, and it takes effect after a save and restart, after which the node
     answers at another bitrate. Record it in coverage with that reason; implement no setter.
   - `2203` stays unwritten (Q16, LX-D12): DIP SW1..SW4 select nodes 1..15 and all OFF "custom" (TLCEN p9);
     which value the custom path uses is undocumented. The binding's node is the application's declaration.
6. Network services. TLEDS repeats the C-EDS defaults for `1005`, `1014`, `1017`, `1200` and the PDO records
   (delta file), so passive heartbeat, boot-up, EMCY and PDO map readback may observe a TL-C binding as
   documentary-unqualified. The `1017` setter (`CL86_C` scope), TPDO1 transitions and every COB-ID, map or
   inhibit write refuse a TL-C binding, and no TL-C row admits `sync` (TL-C SYNC effects are unreviewed;
   README, seam S13). `CompactPDO=31` (LX-D05) stays a raw metadata fact.
7. Ledger: mark each delivered TL-C row implemented and host-tested per model; X02, X03, X04 and X06 open.

## Decisions that do not transfer here

`IdentityWidths::MEASURED_U8`, `CountWidths::MEASURED_U16`, DEC-1 and the `SdoReuse` default markers, the
`StatusLayout` C values, C16/Q08's filter reading, Q16's DIP decoding and "no software bitrate object", and
the `CL86_C` heartbeat and TPDO1 scopes. Each refusal is a test, not a comment.

## Deliver

`State.h` layout value and pairing check; model-taking typed getters in `Reads.h`/`IoSettings.h` with Doxygen;
`test/support/LichuanTlcDrive.h` (fake server answering only TLEDS-declared objects with documented values,
labelled synthetic); `test/tlc_reads_test.cpp` (decode, unresolved filters, pairing refusal, a legacy-getter
or other-model sample refused by a TL-C consumer, widths reported, serial abort kept raw) and
`test/tlc_hazard_test.cpp` (no download to `2401..2407` or `2203` planned on any TL-C path; the reviewed
explicit uploads of these indices and of PDO records emit exactly their request frames and nothing else; a
refused setter, restore plan or map mutation emits no frame; filter-writer rule; CL86-C regression
unchanged); ledger, operation map, coverage, CHANGELOG, report and `PROGRESS.md`.

## Acceptance

A TL-C binding reads identity, state and configuration through the real owner with model-correct labels and
raw values preserved; no TL-C path plans a download to `2401..2407`, `2203`, a COB-ID or a PDO record, and
reads of them are the reviewed explicit uploads only; the bitrate change is listed BLOCKED with X02's
missing evidence. Quick and full verification, firmware builds and
exact-commit CI pass. Hardware NOT RUN; X02, X03, X04 and X06 stay open for 07a.
