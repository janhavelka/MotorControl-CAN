# 13 — Further firmware and model rows

Execute this stage under the [execution contract](execution_contract.md).
Status: NOT STARTED. Runs when new source evidence for a refused row exists; every
row below currently lacks it. A row is admitted by its own evidence, never by
similarity to FIR-v2213 or to another model.

## Rows and what each needs

| Row | Known differences | Missing evidence |
| --- | --- | --- |
| FIR-v1650 (stock firmware of N5, C5-E and PD4-C; 1018h:03 = 06720000h) | No 1006h, 1016h, 1019h, 1F80h, 3203h, 3701h or 6007h in the manual index (N5V2 pp5-8); 2000 increments per revolution in 608Fh (FWUPD p5); no mode change in Operation enabled (FWUPD p6); statusword bit 15 meant "closed loop available" (FWUPD p13); no slow speed (FWUPD p9); 2290h only from FIR-v1748 (N5 p280); 1029h documented despite FWUPD p11 (NT-D31); NanoJ autostart differs (FWUPD p6, NT-D32) | A FIR-v1650 EDS (not in the package), the remote-only C5-E 2.0.1 and PD4-C 2.0.2 manuals stored with hashes, and a full object-by-object review against N5V2 |
| FIR-v2538 (packaged for all four families; 09EA0000h, B1077563) | EDS-only changes: 2007h:02 "Automatic COB-ID" default 1, 2017h LLDP, renamed 3242h, 20A0h/3390h and 4010h subentries (NT-D02); 3242h labels match the manual (NT-D25) | A manual or release note covering FIR-v2538 for these models; the effect of 2007h:02 on PDO COB-IDs |
| CL4-E FIR-v2315 (090B0000h) | Indexed only (IDX) | Its EDS stored and a manual statement of scope |
| CL4-E -5VDI, other PD4-C steppers (5918X, 5918L, 6018L, 5918L2104) | Manual-covered; EDS indexed with hashes but not stored; 5918L2104 missing from the PD4-C cover (NT-D14) | The EDS files stored unchanged; for PD4-C the product-code mapping |
| C5-E-x-09A | In the package, in no manual | A manual naming the variant |
| PD6-E (FIR-v2538 and higher) | Separate online manual 1.2.0; STO in -ES- variants (PD6EOM) | The 1.2.0 manual stored and reviewed; a separate scope, likely its own stage set |

## Behavior until a row is admitted

- Stage 03 names the row in its identity verdict; reads stay available, and every
  dependent operation (enable, motion, settings, PDO configuration, persistence,
  supervision) is refused before TX with that row as the reason.
- The console, `caps` and the node table show the refused row explicitly. No
  caller declaration, flag or configuration value turns a refused row into an
  admitted one.
- A unit reporting an unlisted build of a known generation (for example 1018h:03 =
  08A50000h with a 100Ah build string that no row names) is unknown, not FIR-v2213.

## Work

1. Source intake: download and store the new originals in the pack with
   `SOURCES.json` entries (URL, retrieval date, bytes, SHA-256, applicability,
   firmware match, license, revision, pages), keeping existing originals and hashes
   unchanged. Never extract or run firmware or vendor tools.
2. Ledger scope: load the row's EDS as its own scope and compare it with FIR-v2213
   object by object; every difference is reviewed against the row's manual. Objects
   the row lacks are refused for it; objects whose meaning changed get scope-specific
   entries. Update the identity table and the refused-identity list together.
3. Code: each stage's behavior that depends on a differing object (for FIR-v1650:
   supervision, limits, units, mode change, bit 15) gets row-specific admission and
   tests. Where the row cannot support an operation, it is refused with that reason.
4. Rerun the affected stage acceptance tests for the new row and for FIR-v2213, the
   ledger, generator and operation checks, and CI on the exact final pushed commit.

## Deliver

Updated pack and manifest, ledger scopes, identity rows, generated tables and tests
for each admitted row; coverage and PROGRESS rows marking the row documented,
implemented and host-tested separately; a stage report per admitted row.

## Acceptance

An admitted row has its own stored sources, ledger scope, identity row and tests,
and no FIR-v2213 assumption leaks into it. Rows without evidence stay refused by
name. Hardware qualification of a new row follows 12A and 12B on a unit that reports
that row; NOT RUN otherwise.
