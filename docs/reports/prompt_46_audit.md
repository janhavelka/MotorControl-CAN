# Fresh independent prompt 46 audit

Reviewed original prompt 46 in full against main at `1b1d352889fed052deb5ec221790169f0dfa8eb9`
and the actual `c74393a6..HEAD` implementation diff. Three fresh-context reviewers
independently inspected the module/package consumer, production contrasts and
coverage evidence. Models: two gpt-6.1-sol/high, one gpt-6-astra/high. The lead
verified findings against source and reproductions and owns final integration/Git.
No sibling changes, controller traffic, new branches or release actions.

## Confirmed findings and fixes

1. **Withdrawn pending work could not retire after rebinding.** The example passed
   changed live target/reservation to advanceMove, which rightly expects the
   identity of the admitted exchange. Explicit INVALIDATED succeeded but its
   original terminal then failed TARGET, retaining ACTIVE/outstanding forever.
   MotorModule now uses copied frozen facts only to settle a recorded withdrawal;
   ordinary live admission and exact service/node matching remain mandatory.
   Regressions cover setup, trigger, observation, deadline equality, wrong terminal
   generation and refusal without withdrawal. The old module fails the regression.
2. **Example time-domain validation was incomplete.** A timestamp above `INT64_MAX`
   could publish DEADLINE or mutate a pending fake terminal. All timed module
   entry points and fake service/poll now reject it before publication. The fake
   also reserves representable latency within the same domain. Tests cover maximum,
   maximum+1, wrap-near values, unchanged storage and later valid settlement.
   Capacity preflight applies only to active work, preserving terminal/retained
   no-op turns even at the maximum valid timestamp.
3. **Coverage evidence was too loosely associated.** Family-only bindings, command
   prefix checks and global scenario-name accounting could accept unrelated API
   evidence. The revised schema-2 API evidence map/checker binds concrete typed entries,
   dispatch evidence and scenario routes, retains obligations and rejects invented
   hardware qualification. Source scanning remains a finite project-convention
   check, not a C++ compiler, overload resolver or proof of physical behavior.
4. **Current documentation contradicted implemented scope.** Ledger prose still
   called controlword alarm-clear unimplemented, stops mode 1 only, and counter/
   vendor clear untried. The API/CLI map still denied working enable/control/tuning
   routes. Corrected these against actual Actions/ProbeSettings/console code and
   retained specimen reports; generated documentation comes from the ledger.
   Original source claims/gates and hardware flags are preserved. No new specimen
   test or family-wide qualification is claimed. Checked getIo/getTuning remain
   native-only; console group reads serialize raw native context values instead
   of calling those getters. The evidence map now records that actual boundary.
5. **Contrast clarity.** The production width helper existed despite a stale source
   note. Corrected it and added positive native ACK/completion without controller
   TX confirmation. Primitive implementation review found no confirmed arithmetic,
   capability or evidence-projection defect. The C1 wrapped-angle block is unchanged.

## Source and scope checks

MotorControl-RS remains clean at `2f41e27f2378c644b1358ed9a6af63c35f817058`;
FieldCore-node remains clean at `c40ea8341edf8d665460b902cafefbe857a7f73f`:
unchanged since prompt 46 for the recorded owner/module/exact-intent seam. Adopted
prepare/next/advance, copied submission and retained uncertainty; rejected RTU
framing/timing and fabricated CAN product integration. Symbols and prospective
FieldCore requirements remain in docs/fieldcore_handoff.md.

Independent primary-source review reopened original N5, Lichuan PDF/EDS (including the lead
reopening EN86/ZH86 p15 and OP86 p13 for bit7/manual clearing) and pinned
VESC/ODrive/moteus sources. EPOS4 rel13740 was reopened remotely; actual cached PDF
bytes read have SHA-256 `66fe32f15ab0bcbf9a008d0d0da8ffe395064c6716edcb3f3b28907f7160e64f`.
See docs/audit/PROMPT_46_CONTRAST_SOURCES.md for pages and hash limits. No vendor
artifact was replaced. Fixtures remain excluded from installation/profile inventory;
Lichuan remains the one always-built profile. Existing source, firmware and physical
gates and four pending standalone reads stay distinct from this audit's software fixes.

The consumer storage/capacities remain unchanged: one concrete move/read, one
outstanding exchange and one retained result. No new scheduler, receiver or
installed abstraction was added. The checker uses explicit API references and
small bounded source scans; obsolete loose family/bridge evidence was replaced,
not retained as a second path.

## Verification and handoff

Full GCC15.1 verification passed all147 non-package CTest contracts, the separate
isolated package/export gate and strict Doxygen1.13.2. Final focused consumer
regression passes442 assertions; the updated checker passes46 tests after its
last scanner cleanup. Native/proof/combined-header contrast tests, generated
freshness, repository checks, planning integrity and working/staged diff checks
passed. The independent installed-package check also passed separately.

Retained full evidence is
`.build/reaudit46-full/20261115T103158095562Z-22200-1s7e4oup/`; separate isolated
package evidence is `.build/reaudit46-package-final/`. Reproductions and the
expected original-module regression failure remain under
`.build/reaudit46-consumer/`; current operation facets are in
`.build/reaudit46-coverage.json`. No failed test was converted to a skip.
Commands:

```text
python scripts/verify.py --mode full --compiler gcc --build-dir .build/reaudit46-full
python scripts/check_lichuan_operations.py
python scripts/check_promptset.py --check-git-ref INDEX
git diff --check
```

Commit/push remains on main, with exact-final-commit CI and fresh synchronization
checked before handoff. Actual required CI status is linked from the final handoff;
no older run substitutes for it. Retained failed reproductions remain in .build.
