# Packet 19: position evidence and one isolated finite experiment

**The experiment was not run.** It moves a motor, and the packet requires an independent cutoff, a shaft mark that shows direction and approximate rotation, and an operator. The bench ran an unattended communication soak overnight. Packets 15 and 16's attended prerequisites have not run either.

## What is ready

- **One named, application-local sequence**: the `position` route in `examples/s2_bench/BenchRoute.cpp`, separate from the production move engine and leaving `MotionPreview`'s `executionReady` untouched. It checks a reviewed start `6064`, the session's acknowledged `000F`, mode 1, low bits 111, released, alarm and running clear and `606C=0`; reads `6081`, `6083`, `6084`, `607A` for the record; writes `607A`, `6081`, `6083`, `6084` and `6040=001F` in order, each after the previous acknowledgement; then 1..16 cycles and an optional named stop. Every parameter comes from the host command, reviewed. It needs an attended trial identity, used once, within packet 22's motion caps.
- **The rearm as its own visible step** (added 2026-11-03): the `rearm` route writes one `6040=000F` through the session, only when the latest controlword is a certain, acknowledged `001F` from that node's last `position` trigger, then records fresh cycles and judges nothing. A lost `000F` leaves no latest word, so `position`, `rearm` and `move` all refuse after it. It is project policy until the mode 1 falling-edge behavior is measured.
- **Fake-board tests** in `test/bench_route_test.cpp`: abort and silence on each of `607A`, `6081`, `6083`, `6084` and the `001F`; late and mismatched replies; a boot-up, the route cutoff and a nonzero EMCY mid-route; cycles with no new bit change and with a bit already set before the trigger, both recorded without a claim; and a lost trigger, never replayed (`testPositionPrefixFailures`, `testPositionLateAndMismatchedReplies`, `testPositionRebootCutoffEmcy`, `testPositionNoClaims`, `testRearm`).
- **Packet 20's list of the evidence this packet must supply** before its gate opens: acceptance and completion words, masks and values; whether mode, low bits and the `2100` flags hold through a move; that parameter writes cause no motion before the edge; direction and distance against the command; what the rearm does; and the DEC-8 rows for a stop from `001F`.
- **Documented candidates** are recorded in the [move guide](../../position_parameters.md): `607A` I32 within ±1,000,000, `6081` 0..3000 rpm, `6083` and `6084` 0..2000 ms. Zero is a documentary value, not a useful profile. Absolute `001F` and relative `005F` differ from immediate-update `003F`/`007F`.

## The attended plan

Write the trial record first: the starting counters, reference validity, mode and target history, scale hypotheses, configured inputs, settings, time and movement bounds, and the operator's method of observation. The November reads `607A=5000`, `60FF=120` are historical warnings, not intent. Native coordinate 16 at 1 rpm with 1000 ms ramps is an offline proposal, not a safe value: keep or change it only with an explicit source and fixture basis. If command and feedback scale is the unknown being measured, bound the exposure independently rather than claiming a software angle bound from it. Hide no target zeroing, counter clear, enable, mode change or input reassignment in setup.

## Original-page review, 2026-11-03

An independent reviewer rendered EN86 and ZH86 PDF pages 11, 12, 16 to 19 and OP86 pages 9 and 10 with PyMuPDF, read the EDS sections for `607A`, `6081`, `6083`, `6084`, `6040`, `6041`, `6060`, `6061`, `6064`, `2201` and `2501`, and checked every claim in the move guide, `Position.h`, the stop table, the `position` route and the ledger. The lead rechecked each non-matching item on the rendered page before changing anything.

Types, ranges, write order, trigger words, status candidates and the ledger's EDS fields all match. Corrected:

| Old claim | Source | Correction |
| --- | --- | --- |
| "No source documents a target queue" (`Position.h` `RelativeBase`, move guide) | EN86/ZH86 p12 and OP86 p10: bit 5 = 0 updates the parameters after the current curve completes | A deferred update is documented; its capacity, acknowledgement and base are not. The lifecycle is unchanged: it starts moves only from idle |
| Ranges attributed to EDS `607A`..`6084` | The EDS gives types and defaults, no limits; the ranges are on EN86/ZH86 p11 and OP86 p9 | Attribution fixed in the guide and `Position.h` |
| Ledger `0x6040:00` cites ZH86 PDF p17 | The ZH86 `6040` row is the last row of p16; p17 starts at `6041`. ZH86 p11 also lists `6040` and was not cited | p17 replaced by p16, p11 added, in `source_refs` and the PDO observation refs; regenerated through `generate_lichuan_objects.py` |
| Stop guide: EDS `[6040]` "establishes" write-only | Every manual page prints `6040` RW | Cites conflict C08 |
| OP86 cited without its header caveat | OP86 p9 and p10 carry a CL57-C/OL57-C running header | C18 noted in the guide and `Position.h` |

Newly recorded in Q11: the OP86 p9 curve starts its ramps at a labelled nonzero start velocity, while EN86/ZH86 p11 start at zero; and p17 names `6081`..`6084` profile velocity, acceleration and deceleration, so their rpm and ms units rest on p11 and the EDS names.

Left as is: the ledger's `0x2501:00` `access_reviewed` is RO while the EDS and both manuals say RW. It is a conservative policy, explained by `write_policy: not_writable` and `next_evidence`, not a source fact.

## Open

Everything physical. Production capability gates stay closed until this evidence is reviewed.

## Sibling refresh, acceptance seam

| Sibling | Revision | Dirty | Disposition |
| --- | --- | --- | --- |
| MotorControl-RS | `a13f767` (`src`, `include` unchanged since `6499571`) | Clean | HW-15: the ESS drive acknowledged a start within about 0.6 s of an enable and ignored it, and RS now holds starts in its application (`ENABLE_SETTLE_US`, `MotorControl-RS-overnight` `examples/probe_cli/ProbeApp.cpp:50`). **Rejected** as CL86-C behavior or a library constant. **Adopted** for the trial plan: record the interval from the enable's acknowledgement to the trigger, and treat an acknowledged trigger with no fresh acceptance as the failure it is, which the route already records without claiming |
| ADS1115, INA228 | `d613a13`, `73f9548` | Clean | As packet 22: an uncertain effect is its own terminal, and a failed side-effecting transfer counts as possibly applied |
| FieldCore-node | `5ad9292` | Clean | `Rs485MotorModule::startMotor` keeps execution and completion separate, as the route records trigger acknowledgement and acceptance separately |

## Checks

The `position` route and its tests were committed in `01190cf` and passed the local gates recorded there. After this review, the ledger, generator and promptset checks pass on the corrected files.
