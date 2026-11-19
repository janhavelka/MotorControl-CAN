# 01 — Model declaration, decision applicability and identity confirmation

Execute this stage under the [execution contract](execution_contract.md).
Status: NOT STARTED.

Backlog D18 ([backlog](../../backlog.md)): the C-EDS, TLEDS and DS-C EDS declare the same `1018:01/02`
(0x1000/0x0001) and `1000` (0x40192) while their vendor objects differ, so identity cannot select a model and
a CL86-C write can mean something else on a TL-C or DS-C. This stage makes the model an explicit application
declaration, makes every existing operation refuse the new models, records which existing decisions apply to
which models, and adds identity confirmation that can only confirm or contradict a declaration.

## Work

1. Refresh the [canopen_shared 01](../canopen_shared/01_existing_code_and_tooling.md) inventory for the rows
   model scoping touches: `Settings.h` `SettingsModel` and `AxisReservation`, `src/profiles/lichuan_c/ProfileEvidence.h`,
   `Reads.h` `IdentityObservation` and `prepareIdentity`, `Discovery.h`, `Position.cpp` `sameFirmware`, and the
   model checks in `examples/common/ProbeSettings.cpp`, `ConfigurationRestore.cpp`, `AxisSession`, `ProbeApp`
   and `examples/s2_bench/BenchConsole.cpp`. Record HEAD and main PROGRESS rows 57, 64, 72 and 77. RS row: read
   `include/MotorControlRS/Discovery.h` `exactModel` and `profiles/ess_rs/DriverSettings.h`
   `exactModelQualified`, and the caller-declared variant in RS `docs/prompts/lichuan_lc57bk_rs485/03_reads_and_discovery.md`
   item 2; adopt caller declaration and identity-as-evidence, reject confirmation by a unique model register
   (Lichuan has none).
2. Append one `SettingsModel` value per documented model designation, keeping `UNKNOWN`, `CL86_C` and `OL86_C`
   unchanged (proposed names; confirm against CONVENTIONS): `TLC42_C`, `TLO42_C`, `TLC57_C`, `TLO57_C`,
   `TLC60_C`, `TLO60_C`, `TLC86_C`, `TLO86_C` (TLCEN p3; PG-TL42..PG-TL86), `CL2_57C`, `CL2_86C` (CL257 p4,
   CL286 p4), `CL257C_ABZ`, `CL286C_ABZ` (ABZ57 p4, ABZ86 p4). Reasons: TLC/TLO differ by encoder (TLCEN p3),
   frame sizes by ratings (LX-D09), CL2 57/86 by `2502` bounds (CL257 p17, CL286 p17) and supply (LX-D31).
   Order-code suffixes stay application provenance. Encoder bits and turn type (CL257 p10) are declared
   configuration facts, not model values. Add no CL57-C/OL57-C value (stage 05 is documentary).
3. Fail closed. Every `SettingsModel` consumer refuses every new value before output change or TX until a later
   stage admits it through a ledger row. A `switch` whose default admits, or a check of `model == UNKNOWN`
   alone, is a defect. Add one table-driven native test that prepares each public operation that takes a model
   (settings, tuning, enable, stop, alarm clear, position clear, moves, velocity, homing, heartbeat, TPDO1,
   save, restore) with each new value and asserts `UNSUPPORTED` with `CAPABILITY` detail, unchanged output and
   no owner TX through `test/support/OwnerRig.h`.
4. Applicability register: add `semantic_review.decision_applicability` to `lichuan_objects.json`, one row per
   implemented decision in the README table "Existing decisions that do not transfer": id, public symbols,
   evidence (document pages or CL86-C bench records), models it applies to, and owning stage. The ledger
   checker fails when a row names a symbol absent from the public headers, a listed model lacks a documentary
   source, or a decision of that README table has no row. Add the negative cases.
5. Model binding, proposed `ModelBinding`: one application record per `ReadTarget` binding holding the
   declared model, a nonzero provenance record id and the current identity outcome with its node generation
   and server session. No I/O. Only the application replaces the declaration. The application owns this
   record as the holder of canopen_shared 02 "Identity and device evidence": boot-up, NMT reset, bus-off or
   controller restart, rebinding and a configuration-generation change clear the outcome; a contradicting
   read sets CONTRADICTED. Every preparation taking an `AxisReservation` refuses a reservation whose model
   differs from the binding's. Admission is rechecked at every changing step, not only at preparation: each
   `next...` call and the owner transmit guard (the points that already take the live reservation and current
   target, as `validateMoveAdmission` does) read the outcome from the live holder and compare its node
   generation and server session with the current target. A cleared or stale outcome, or a new one that no
   longer admits under item 7, retires every pending changing step on that binding unsent and sends nothing
   further there, except prompt 57's exempt stop: a reviewed stop of 03b or 04b item 3 already prepared or
   queued stays admitted under its own route, binding-generation and source-state checks, the outcome change
   never withdraws it, and its result carries the outcome and claims no standstill (an NMT command is never
   that stop). Other nodes' work is untouched, and a frame already handed to the controller is reported as
   possibly sent. A copy taken at preparation is an admission snapshot for the result, never permission to
   transmit. The record also holds the application's SDO reuse attestation: the channel binding and
   configuration epoch for which the application read `SdoReusePolicy::NONE` back through the owner's
   `sdoReuse` copy (03a item 2, 04a item 3). The owner's `setSdoReuse` advances no epoch, so the attestation
   also requires the policy to stay `NONE`: any policy change on that binding clears it before the change and
   retires the binding's unsent work (03a item 2). Rebinding clears it with the outcome, and every new-model
   preparation that takes the binding refuses before TX when it is absent or names another epoch than the
   target's channel; read preparations that take no model are covered by the application's route check
   (03a item 2). The core checks only the attestation; the owner read stays in the application.
6. Identity confirmation: the one comparison of the execution contract's
   [identity outcomes](execution_contract.md#identity-outcomes-prompt-57s-policy) (CONFIRMED, UNCONFIRMED,
   CONTRADICTED), with this set's model-scoped rule. If prompt 57 or DS2 stage 03 landed it first, extend it;
   otherwise land it profile-neutral, proposed `ModelIdentityRecord` and `checkModelIdentity`:
   - A record holds a profile-qualified model (or "no implemented profile" for DS-C or another product), the
     raw fields measured (`1000`, `1009` and width, `100A` and width, `1018:00..03`, optional `1018:04`,
     `2000`, `2001`) and a specimen/firmware provenance id. The application supplies records; the library
     embeds none. The ledger cites the measured CL86-C identity (Q01, `count_width_binding`) as evidence an
     application may copy.
   - Inputs and outcomes as the contract defines them, with a current `IdentityObservation` for the node
     generation. `IdentityObservation::modelMatched` keeps its meaning or changes only with a migration entry.
   - Match fields. A reference matches when every product identity field of prompt 57 (`1000`, `1018:01`,
     `1018:02`, `2000`) that the reference holds equals the observation's; a field the reference holds that
     the observation lacks (not read, aborted, wrong width) leaves that reference undecided, so the outcome is
     at most UNCONFIRMED. An undecided reference is neither a match nor a non-match. If it belongs to the
     declared model and no other declared-model reference matches, the outcome is UNCONFIRMED without a
     declared match and refuses. If it belongs to another candidate while a declared-model reference
     matches, the outcome is UNCONFIRMED with a declared match, admitted under prompt 57's precedence, and
     the result names the undecided reference and its missing field. CONFIRMED needs every other
     candidate's reference decided and none matching. Firmware fields (`1009`, `100A` and their reply widths, `1018:03`, `2001`), the
     `1018:00` count, the optional `1018:04` serial and the record's provenance are reported beside the
     outcome and never decide it. Adding a field to the match (for example the count, LX-D02) is a later
     reviewed change under X01.
   - Minimum record content. A record is accepted as a reference only when it holds `1000` and `2000` and,
     for a model whose sources declare them (the C family; TL-C, TLEDS l.83, l.91), `1018:01` and `1018:02`;
     a CL2 record holds `1018:01/02` only when its specimen answered them (04a item 2). A record with fewer
     product fields is rejected where it is loaded, so no reference matches by holding nothing.
   - Limit, stated in Doxygen: CONFIRMED means unique among the supplied references, not physical proof of
     the model (prompt 57): an unmeasured specimen can still equal another model's reference. If two
     candidates' records are equal, both stay UNCONFIRMED and protection rests on the declaration and the
     model rows alone. A TL-C node declared while only a CL86-C record exists and its identity equals that
     record is CONTRADICTED, which refuses every changing request (prompt 57) and forces a TL-C record
     (07a T1) first.
7. Refusal, prompt 57's vendor-object and motion rules under its "Admission by outcome" precedence: a central
   check in `ProfileEvidence.h` refuses every LICHUAN_C preparation that downloads to `2000h..5FFFh` when the
   binding's current outcome is CONTRADICTED. For the new models the same check applies that precedence to
   such a download and to every changing request of the motion rule: only a current outcome in which a
   reference of the declared model matches admits (CONFIRMED, or UNCONFIRMED with that match, whose result
   names every matching model); UNCONFIRMED without that match (a definite product mismatch, no reference of
   the declared model, an unread product field), CONTRADICTED and no current outcome refuse before TX, the
   exempt stop of item 5 excepted. A new-model binding therefore needs a MEASURED record of its own model
   (07a T1, K2) before any changing request. CL86-C and OL86-C keep today's admission when no identity check
   exists. Record that choice in the register as a compatibility choice with its residual risk, not as
   evidence that a legacy declaration is right: a TL-C or DS-C node declared as CL86-C still receives CL86-C
   vendor writes, `2300` save included, exactly as today; this stage does not change prompt 57's vendor-write
   policy for those models. Results carry the outcome. Writes to `6040`, `6060`, `607A` and other standard
   objects follow their own rows and prompt 57's motion rule under the same precedence. Pass the live
   holder of item 5 (in the live `AxisReservation` or as a parameter) rather than a preparation copy; any
   changed public signature gets a migration entry and updated callers in the same commit.
8. Prompt 57 coordination. Prompt 57's Identity policy (Pending in the main PROGRESS) now states D18: a
   matching product identity does not prove a CL86-C (LX-D01, LX-D35), the declared binding selects the
   profile and model, and the Lichuan family sets add model-scoped checks on that one policy. Its
   product-identity refusal (`1000`, `1018:01/02`, `2000`) and this comparison are one check: if prompt 57
   landed first, extend its check with the outcomes and model records; otherwise land the comparison so that
   prompt 57 and DS2 stage 03 extend it. Record the decision and the comparison's location in this report.
9. Intake correction (old claim, evidence, scope, disposition). Old claim: LX-D02, "the live count of neither
   product is known" (LX review). Primary evidence: Q01 in `lichuan_objects.json` and
   [the 2026-11-02 live connection report](../../reports/2026-11-02_s2_live_connection.md): both CL86-C
   specimens return `1018:00` = 5 (U8), above the C-EDS maximum 4. Scope: CL86-C, those two specimens and
   their firmware. Disposition: LX-D02 stays open for TL-C, whose live count is unmeasured (07a T1); the
   register's evidence and the report carry both claims; the LX pack review is not edited.
10. Create this directory's `PROGRESS.md` with every stage of the README table.

## Deliver

- `Settings.h` model values (or a `Models.h` included from it, keeping the name) and `ModelBinding`;
  `ModelIdentityRecord` and `checkModelIdentity` in a profile-neutral header, or the extension of the
  comparison prompt 57 or DS2 stage 03 landed; their Doxygen; the central refusal in `ProfileEvidence.h`.
- `test/model_applicability_test.cpp` (fail-closed table, binding mismatch, missing or stale reuse
  attestation, an attestation cleared by a policy change on the same binding, no TX) and `test/model_identity_test.cpp` (all outcomes, each with its exact reason and
  admission: a candidate without a reference and a TL-C record equal to the CL86-C record give UNCONFIRMED
  with a declared match, admitted with every matching model named; a definite product mismatch with no
  other match, a declared model without a reference and an unread product field give UNCONFIRMED without a
  declared match, refused; a declared-model match beside another candidate's reference left undecided by an
  unread or wrong-width product field gives UNCONFIRMED with a declared match, admitted, the undecided
  reference named; no current outcome, refused; an apparently unique reference gives CONFIRMED and
  nothing stronger; a TL-C declaration with only an equal CL86-C record gives CONTRADICTED; a DS-C record
  contradicting a CL86-C declaration; a record below the minimum content rejected; stale node generation;
  missing `1018:04`; a firmware-only or width difference that leaves the outcome and admission unchanged
  and is reported; CONTRADICTED blocks `2302` and `2500` downloads before TX in both declaration directions;
  invalidation between preparation and dispatch and between two changing steps, including a new
  CONTRADICTED outcome on the same model and a new server session of the same node: no further changing
  TX, other nodes unchanged, a handed-off frame possibly sent; the exempt stop is tested in 03b and 04b,
  which admit the first new-model stops), each registered in CTest and `lichuan_operations.json`.
- The register, its checker rule and negative tests; `docs/profile_contract.md` model section; backlog D18
  disposition; CHANGELOG `Unreleased`; `PROGRESS.md`; `docs/reports/lichuan_c_extensions_01.md`.

## Acceptance

Every new model is refused by every existing operation with no TX. No code path selects or changes a model
from identity. CONTRADICTED refuses manufacturer-area writes before TX, also when it arrives after
preparation; for the new models every outcome without a declared-model match, and no current outcome, refuses
them and every changing request (prompt 57's precedence, the reviewed stop exempt); CONFIRMED is reachable
only with records for every candidate and claims only uniqueness among them. Existing CL86-C/OL86-C tests
pass unchanged. Ledger, generator, operation, quick and full verification, the firmware builds and
exact-commit CI pass (public API change). Hardware NOT RUN.
TL-C and CL2 identities stay unmeasured (X01, stage 02) until 07a.
