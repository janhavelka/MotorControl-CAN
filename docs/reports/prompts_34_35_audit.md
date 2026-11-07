# Independent audit of prompts 34 and 35

## Scope and source refresh

The requested delayed audit started at 22:02 UTC on 7 November 2026, after the
45-minute wait. It re-read both original prompts in full and inspected commits
`865e0a2` and `eaaa7fa` against the current implementation. Prompt 36 was not
executed. Entry HEAD was `a04777e` on the existing `promptset/motorcontrol-can`
branch, tracking the same branch at origin; fresh fetch showed 0 ahead/behind.
Concurrent bench edits and three untracked `.vscode` files were preserved.
The separate bench session committed its work as `18d6c2a` during this audit;
that commit became the audit fix's parent without changing branches.

| Source | Fresh state | Adopted or rejected behavior |
| --- | --- | --- |
| MotorControl-RS | `cd27b12389f568198b72cecb6606123ea4c089bc`, clean; unchanged since prompt 35 | `Position.cpp::prepareMoveAngle` uses the shared position lifecycle and unchanged-output preparation. `Velocity.cpp::prepareVelocity` anchors duration at preparation and validates poll coverage; setup must not consume that running allowance. ESS wire values and timings do not transfer. |
| FieldCore-node | `5ad9292dc8481cf08be9a61f662495a6474d1da0`, clean; unchanged since prompt 35 | The `Rs485MotorModule` identity/admission seam retains separate binding/configuration and operation/step identities. It supplies no angle or velocity sequence. No sibling edits. |

The evidence reviewer reopened EN86/ZH86 PDF p13, OP86 p11 and EDS
`60FF/6060/6083/6084`. Their conflicting start order and signed operating range
remain unresolved. No manufacturer fact, source snapshot, object type or
firmware gate was changed. No serial port or controller was opened.

## Findings and correction

**Confirmed velocity defect:** preparation accepted
`durationUs == maxPolls * pollIntervalUs`, but mode/enable confirmations consumed
the same counter as running polls. A healthy zero-latency direct path stopped
at 491000 us rather than its 501000 us due time. A separate real-owner case
stopped at 86500 us rather than 136000 us, with `ITERATION_EXHAUSTED`. Both
reproductions exercised the public path; slower replies could hide this error.

`runningPolls` now counts only interval-spaced running cycles. The existing
`observations` and sample cycle retain all complete cycles. Setup remains
bounded to three enable confirmations and one optional mode confirmation;
counter exhaustion fails without wrapping. A positive running-poll allowance
is sufficient. Preparation anchoring, fixed deadlines, stop reserve, first
failure, uncertainty and the existing owner/stop engine are unchanged.

Direct and real-owner regressions cover exact coverage with and without mode
change, HALT and QUICK_STOP, minimal poll allowance, and maximum-counter/product
overflow boundaries. The equality regression failed against the original
library before the correction. Existing late-transmission, cancellation,
unknown-trigger and separate-stop tests remain in the regression suite.

**Documentation corrections:** removed obsolete claims that finite velocity
has no implementation or typed `60FF` read. Updated README, conventions,
API/CLI mapping and native coverage without relaxing Q20 or describing a host
test as physical qualification. The velocity guide and public header define
the separate setup/running bounds.

**Prompt 34:** no defect found. Review followed exact conversion, path/tie,
polarity, selected-path limits, reference/generation checks, integer narrowing,
owner admission, stop and unknown-trigger outcomes. An independent temporary
Python `Fraction` oracle matched 12000 deterministic production-preparation
vectors over rational scales/gearing, origins, both polarities and all paths.
That oracle supplements the retained native tests; it is not a vendor fixture.

## Verification and handoff

`python scripts/verify.py --mode full --compiler gcc --build-dir
.build/audit3435-final` passed all 104 registered native/Python contracts,
generated-file and registration checks, isolated source/export/relocated-package
consumers, and strict Doxygen 1.13.2. Retained summary:
`.build/audit3435-final/summary.json`. The final counter/test/comment tree was
rebuilt and both velocity tests passed again after the formatting/zero-budget
assertion was added. Repository, promptset integrity and diff checks passed.

The temporary angle oracle and seven focused angle/consumer tests passed; the
independent lifecycle review reran velocity, owner, action and session tests.
The integrated full verifier above includes the preserved concurrent bench
edits; the audit commit includes only this task's paths and changelog hunk.
Linux sanitizers and firmware/stack builds are checked by the existing automatic
push workflow on the final commit; their exact result and synchronized commit
are reported in the final handoff. No manual duplicate CI dispatch is needed.

Parallel review used `gpt-6.1-sol/high` for angle arithmetic and coverage/source
review, and `gpt-6-astra/high` for lifecycle, timing and uncertain stop settlement.
One writer owned the velocity state/count change and owner regression; another
owned the direct regression. The lead checked their actual diff and integrated
documentation, verification and Git. A separate reviewer challenged the final
counter correction. No new executor, receiver, allocation or retry was added.

Prompt 34 retains its completed software scope and inherited evidence gates.
Prompt 35 remains Partial for actual selected-firmware mode3 startup, released
mode changes, reverse range and stop evidence, with general `60FF` acquisition
still Q20-gated. The conditional prompt-29 experiment in
[the velocity guide](../velocity.md#remaining-specimen-evidence) is the next
measurement route; this audit does not defer it to prompt 47 or invent its result.
