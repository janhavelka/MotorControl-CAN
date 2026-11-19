# 13 — Further firmware and model rows

Execute this stage under the [execution contract](execution_contract.md).
Status: NOT STARTED. Runs when new source evidence for an unqualified row exists;
every row below currently lacks it. A row is qualified by its own evidence, never by
similarity to FIR-v2213 or to another model. Until then a firmware or hardware row is
not refused by name (execution contract, "No firmware lock"); this stage owns the
behavior gates below, and each stage that owns a gated operation implements its gate.

## Rows and what each needs

| Row | Known differences | Missing evidence |
| --- | --- | --- |
| FIR-v1650 (stock firmware of N5, C5-E and PD4-C; 1018h:03 = 06720000h) | No 1006h, 1016h, 1019h, 1F80h, 3203h, 3701h or 6007h in the manual index (N5V2 pp5-8); 2000 increments per revolution in 608Fh (FWUPD p5); no mode change in Operation enabled (FWUPD p6); statusword bit 15 meant "closed loop available" (FWUPD p13); no slow speed (FWUPD p9); 2290h only from FIR-v1748 (N5 p280); 1029h documented despite FWUPD p11 (NT-D31); NanoJ autostart differs (FWUPD p6, NT-D32) | A FIR-v1650 EDS (not in the package), the remote-only C5-E 2.0.1 and PD4-C 2.0.2 manuals stored with hashes, and a full object-by-object review against N5V2 |
| FIR-v2538 (packaged for all four families; 09EA0000h, B1077563) | EDS-only changes: 2007h:02 "Automatic COB-ID" default 1, 2017h LLDP, renamed 3242h, 20A0h/3390h and 4010h subentries (NT-D02); 3242h labels match the manual (NT-D25) | A manual or release note covering FIR-v2538 for these models; the effect of 2007h:02 on PDO COB-IDs |
| CL4-E FIR-v2315 (090B0000h) | Indexed only (IDX) | Its EDS stored and a manual statement of scope |
| CL4-E -5VDI, other PD4-C steppers (5918X, 5918L, 6018L, 5918L2104) | Manual-covered; EDS indexed with hashes but not stored; 5918L2104 missing from the PD4-C cover (NT-D14) | The EDS files stored unchanged; for PD4-C the product-code mapping |
| C5-E-x-09A | In the package, in no manual | A manual naming the variant |
| PD6-E (FIR-v2538 and higher) | Separate online manual 1.2.0; STO in -ES- variants (PD6EOM) | The 1.2.0 manual stored and reviewed; a separate scope, likely its own stage set |

## Behavior until a row is qualified

- Stage 03 names the row in its identity verdict and reports it as firmware or
  hardware unqualified with the declared and observed identities. Operations run under
  the declared model's FIR-v2213 scope; objects the row's evidence says are absent are
  not requested and read as "not in this generation". Only the behavior gates below
  refuse, before TX, with the gate name as the reason. A refusing product outcome
  still refuses as prompt 57 "Admission by outcome" defines.
- Applicability has three states per object, fixed by the identity verdict: present
  (requested as usual); absent by the row's evidence (not requested, reported "not in
  this generation", and every consumer decides on that fact explicitly, as 04b item
  1's Plug & Drive row does without a 2290h upload); and unread (a failed, aborted or
  unsupported upload of an object that should be present). Unread never becomes
  absent: it refuses only the operations that need that observation, naming it.
  Typed decoding follows the row too: on FIR-v1650, 6041h bit 15 is reported as
  "closed loop available" (FWUPD p13), never as active (03 item 4).
- A unit reporting an unlisted build (for example 1018h:03 = 08A50000h with a 100Ah
  build string that no row names), CL4-E FIR-v2315 and an out-of-range or missing
  hardware declaration are unqualified with no gate, because no evidence names a
  missing object or behavior. An inferred 1018h:03 high word never selects a gate.
- The console, `caps` and the node table show the unqualified row and its active
  gates explicitly. A gate is removed only by evidence recorded here, never by a
  caller declaration, flag or configuration value.
- Models without a stored EDS (CL4-E -5VDI, the other PD4-C steppers) stay outside the
  scope until their EDS is stored; that is a model-scope question, not a firmware gate.

| Behavior gate | Row | Refuses | Evidence |
| --- | --- | --- | --- |
| `FIR1650_HOST_LOSS` | FIR-v1650 | 09b's 1016h and 6007h setters and 10's 1F80h setter; the `Supervision` snapshot reports them absent | N5V2 pp5-8 index (no 1016h, 1F80h, 6007h); NT-D01 |
| `FIR1650_ERROR_BEHAVIOR` | FIR-v1650 | 09b's 1029h:01 setter while NT-D31 is open | NT-D31 (N5V2 documents 1029h, FWUPD p11 disagrees) |
| `FIR1650_LIMIT_REACTION` | FIR-v1650 | 07's limit-reaction report and discard (3701h) and every homing method whose prerequisite needs 3701h | N5V2 index (no 3701h) |
| `FIR1650_FEEDBACK_SELECTION` | FIR-v1650 | Capability checks and settings that read or write 3203h | N5V2 index (no 3203h) |
| `FIR1650_MODE_CHANGE_ENABLED` | FIR-v1650 | Mode select in Operation enabled (04 item 6) | FWUPD p6 |
| `FIR1650_CLOSED_LOOP_ACTIVE` | FIR-v1650 | PT and homing on block, whose admission needs bit 15 "closed loop active" (06 item 6, 07 item 2); bit 15 meant "closed loop available" and 03 decodes it so | FWUPD p13 |
| `FIR1650_SLOW_SPEED` | FIR-v1650 | Setting 3202h bit 7 | FWUPD p9 |
| `FIR1650_PLUG_AND_DRIVE` | FIR-v1650 | `preparePdiControl`. 04b's Plug & Drive row decides on the interface's documented absence without a 2290h upload, so ordinary control is not refused for it | N5 pp280-282 (2290h, 2291h and 2292h from FIR-v1748); none of them in the N5V2 index |
| `FIR2538_AUTOMATIC_COB_ID` | FIR-v2538 | PDO COB-ID writes (1400h-1407h:01, 1800h-1807h:01) and every 2007h:02 write | EDS 2007h:02 "Automatic COB-ID", default 1, undocumented (NT-D02) |
| `FIR2538_CHANGED_ENTRIES` | FIR-v2538 | Writes to the 3242h, 20A0h, 3390h and 4010h subentries whose EDS entry differs from FIR-v2213 | EDS comparison (NT-D02, NT-D25) |

FIR-v1650's 2000 increments per revolution in 608Fh (FWUPD p5) is no gate: 05's unit
relation uses the read 608Fh. NanoJ autostart (NT-D32) stays unknown for every row.

## Work

1. Source intake: download and store the new originals in the pack with
   `SOURCES.json` entries (URL, retrieval date, bytes, SHA-256, applicability,
   firmware match, license, revision, pages), keeping existing originals and hashes
   unchanged. Never extract or run firmware or vendor tools.
2. Ledger scope: load the row's EDS as its own scope and compare it with FIR-v2213
   object by object; every difference is reviewed against the row's manual. Objects
   the row lacks are gated for it; objects whose meaning changed get scope-specific
   entries. Update the identity table and the gate table together.
3. Code: each stage's behavior that depends on a differing object (for FIR-v1650:
   supervision, limits, units, mode change, bit 15) gets row-specific tests. A gate is
   removed when the row's evidence resolves it; where the row cannot support an
   operation, its gate stays with that reason.
4. Rerun the affected stage acceptance tests for the new row and for FIR-v2213, the
   ledger, generator and operation checks, and CI on the exact final pushed commit.

## Deliver

Updated pack and manifest, ledger scopes, identity rows, the gate table, generated
tables and tests for each qualified row; coverage and PROGRESS rows marking the row documented,
implemented and host-tested separately; a stage report per qualified row.

## Acceptance

A qualified row has its own stored sources, ledger scope, identity row and tests,
and no FIR-v2213 assumption leaks into it. Rows without evidence stay unqualified with
their gates; tests show each gate refusing before TX with its name, the ungated
operations running with the unqualified identities in their results (a FIR-v1650
enable admitted with no 2290h upload), an aborted read of a present object refusing
as unread rather than absent, the FIR-v1650 bit 15 reported as "closed loop
available", and a refusing product outcome still refusing. Hardware qualification of a new row follows 12A and 12B on a unit that reports
that row; NOT RUN otherwise.
