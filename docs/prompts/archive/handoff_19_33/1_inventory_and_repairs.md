> Executed handoff, retained for traceability. Do not execute this copy. All 22 packets ran between 2 and 4 November 2026. The [execution table](../../../reports/handoff_19_33/README.md) records what each delivered, each packet report records what remains, and the [backlog](../../../backlog.md) rows H03 and H04 track the physical trials still owed. Relative links were relocated; task wording is retained from `2920479`.

# Packet 1: Current inventory and prerequisite prompt repair

Execute this packet fully. Apply [COMMON.md](COMMON.md). This is W1 of the
[active audit](../../PROMPTSET_AUDIT_34_50_2026-11-02.md), not an instruction to
implement all its code repairs now. Read its Start here, baseline delta,
decisions, W1–W8, sections10/12/13 and G/N findings in full. Read original prompts
19–22 and 29–33 and their current reports. Preserve historical evidence.

## Inspect and deliver

1. Record live branch/base/upstream, dirty/staged files, toolchain availability,
   current CI and the exact scope already delivered. Read actual declarations,
   callers and tests named in this handoff's index. Refresh sibling evidence.
   Verify `c643af8`'s NMT/receive-bound/TPDO/readiness additions against their diff;
   do not treat the older audit's line numbers or 8KB stack as current facts.
2. Create one compact execution table in `docs/reports/handoff_19_33/README.md`:
   packet, original obligation/finding, actual implementation/test, status,
   smallest missing fact, artifact consumed by the next packet, commit/checks.
   A status needs a declaration, caller and observable test or retained device
   record. This is navigation, not a copied transcript. Distinguish host-complete,
   conditionally usable, physically observed and unqualified behavior.
3. Execute W1's prompt/graph corrections against current files. Apply G-01/G-02
   together: remove18 from 38 in prompt/index/SEQUENCE, preserve41/42 ancestry
   when changing44 to depend on42/43, and add the checker guard that18 is not an
   ancestor of49. Adopt the usable-prerequisite rule and inherited-gate column
   without changing the meaning of Complete. Do not weaken the checker to fit
   inconsistent instructions. G-05 is already resolved; prove and retain it.
4. Apply remaining W1 G/N and section 12 corrections, DEC-7 remote EPOS4 evidence,
   exact read-row ownership and DEC-2/3/8 obligations in affected original prompts.
   Use section 13's actual checker schema. The audit's original artifact count 137
   is historical; current retained count is 140. Do not delete the three CiA
   artifacts or make a new hardcoded stale expectation. Preserve the mandatory
   W1–W8 hold until their implementation/checks are done in packets 2–10.
5. Reconcile `docs/device_conflicts.md`, coverage and reports with retained Nov2
   evidence: identity widths, mode 0/nonzero targets, map readback, boot capture
   and one-cycle reversion are now observed. None establishes a status dialect,
   harmless reads, safe enable or fresh PDO samples. Do not overwrite originals
   or silently promote gates. Record source vs project decision vs measurement.

## Work split and checks

Delegate graph/schema review, native-row ownership inventory, and current-code
counterexamples independently. One writer owns COMMON/SEQUENCE/PROGRESS; workers
submit findings or disjoint prompt edits. A reviewer checks that every19–22 and
29–33 obligation has an owner and no firmware gate was erased to make a graph pass.
Do not contact maintainers or change sibling repositories; put cross-repository
findings in the handoff for the user.

Run `check_promptset.py`, `check_promptset_test.py`, the new graph regression,
`check_lichuan_ledger.py`, `check_lichuan_ledger_test.py`,
`generate_lichuan_objects.py --check`, `generate_lichuan_objects_test.py` and
source diff checks. Run additional checks only for actual changed tooling.
No port, firmware upload or CAN traffic belongs to this packet. Finish the
packet 1 report, documentation and commit/sync under COMMON. Exit with executable
ownership and corrected prompts, not a claim that W2–W8 or motion now work.
