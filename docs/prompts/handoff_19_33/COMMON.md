# External coder execution contract

This is a continuation of the existing MotorControl-CAN promptset, scoped to
prompts 19–22 and 29–33 and their necessary repairs. Apply root `AGENTS.md`, the
parent Projects guidance, and [COMMON](../COMMON.md), [CONVENTIONS](../CONVENTIONS.md),
[DOCUMENTATION](../DOCUMENTATION.md), [MAINTAINABILITY](../MAINTAINABILITY.md),
[MULTIDRIVE](../MULTIDRIVE.md) and [DELEGATION](../DELEGATION.md). Read the numbered
packet in full. Execute it, including corrections, tests, documentation and sync;
do not answer with another plan or rewrite this handoff as the deliverable.

## Scope, branch and agents

Use the user's selected **Opus 5.5** as lead if that model is exposed by the
external runtime. This is a requested model label, not a claim about its API ID,
context size or availability. Inspect available workers and supported reasoning
settings. Inherit the available model if overrides are unavailable. Use stronger
available reasoning for state, stop, uncertainty, source contradictions and final
review; ordinary workers can own settled code, tests and documentation.

Each packet specifies bounded parallel work. Normally one implementer owns the
central state, one worker owns disjoint tests, and an independent reviewer tries
to refute the implementation. Respect actual concurrency limits. Give workers
the exact packet, current declarations, source pages, ownership and acceptance
checks. Review their actual diff and evidence. Workers never operate hardware,
commit, push, switch branches or edit siblings. No competing implementations.

Inspect branch, upstream, recent commits, staged/dirty files and incoming changes
before editing. Resume `promptset/motorcontrol-can` when it is still the recorded
execution branch; **do not create a branch for this handoff or each packet**.
Preserve concurrent/unrelated work. The lead stages explicit owned paths, reviews
the staged diff, commits and pushes every completed packet and useful complete
checkpoint. Fetch and verify ahead/behind state. Never force-push or rewrite
shared history. A different checkout must establish the recorded continuation,
not invent a remote or push to the base branch.

## Fresh evidence, not copied completion claims

The authoring baseline is `4ae93e3` on 2026-11-02; `c643af8` is the tested
communication/PDO implementation. Refresh both code and reports. Read the
[active repair audit](../PROMPTSET_AUDIT_34_50_2026-11-02.md), particularly its
changes-since-baseline note, decisions and the packet's W/A/C/G/D findings.
Packets 1–10 divide W1–W8 into executable pieces. Production finite motion must
wait for their verified closure. Isolated prompt-29 experiments do not assert
that a production lifecycle exists. Do not remove the audit override early.

For every packet refresh MotorControl-RS, ADS1115 and INA228 revisions and dirty
state, then read the relevant implementation, real caller and focused test.
Add the named I2C seam and FieldCore read-only. Record deltas; do not fetch/change
siblings or rerun unchanged unrelated suites. RS provides common intent names;
sensor retries and RS wire values are not CAN behavior.

Use `docs/reference/lichuan_objects.json` as the reviewed ledger, its checker and
generator as the source of descriptors, and original files under
`docs/reference/vendor/`: `86C_CANopen_user_manual_en.pdf` (EN86),
`86C_CANopen_user_manual_zh.pdf` (ZH86),
`CL86C_OL86C_operating_manual_zh.pdf` (OP86),
`57C_CANopen_user_manual_zh.pdf` (ZH57), and `LC_StepDriverCAN_V1.1.eds`.
Page numbers in this handoff are PDF pages starting at 1; verify the actual page.
Render disputed tables. Preserve original bytes, contradictory claims and hashes.
Do not infer model/firmware equivalence, status masks, reset delays or safe values.
Regenerate; never hand-edit generated headers. A project risk decision remains
distinct from a vendor statement or specimen measurement.

## Physical test authorization and boundaries

**Standing bench authorization.** On 2 November 2026 the user stated, for this whole promptset: physical tests are generally allowed; the bench is ready, with two drives in series, free shafts and motors fixed to the table; run everything the work needs; and run as many subagents as needed. So do not stop to ask before a physical test or motion a prompt or packet needs. Check the fixture by what software can see, such as port identity and fresh readback, and report what ran. Ask only for something only the operator can supply: a physical action, an observation software cannot make, or a changed fixture. This does not change the evidence rules: an observed result stays scoped to its specimen and firmware, and one lead alone controls the port and active CAN requests.

The user explicitly authorizes this external coder to upload firmware, communicate
with the recorded board/drives, perform the bounded physical tests in these
packets, implement HIL diagnostics, deliberately reproduce failures, fix bugs and
retest. Routine tests within a verified envelope do not need repeated permission.
This authorization does not supply missing electrical facts or an unattended
operator. Verify the present fixture against [README](README.md) and
`docs/hardware_bench.md`. Ask only for missing physical information/action or a
materially changed test envelope; continue independent software work meanwhile.

Exactly one lead controls the serial port, flashing and active CAN requests.
Workers inspect saved evidence only. Keep the independent drive-power cutoff
available and the reported free, table-mounted shafts clear during motion tests.
Do not rely on an unqualified library stop as the independent control. If the
operator/cutoff or fixture state is unavailable, finish host work and report the
precise physical gate. Software cannot operate or verify a manual power cutoff.

Before each state-changing trial record the exact typed sequence, node, source
basis, starting state, intended values, maximum speed/travel/exposure, deadlines,
observation method, abort conditions and reconciliation. Unresolved quantities
cannot be replaced with generous defaults. Default to one drive; add the second
only after the isolated case passes. A native coordinate16 is not proof of a
small displacement. Never silently disable assigned limits/inputs, clear faults,
zero counters, enable, change mode or restore settings to make a test pass.

No object/controlword sweeps, full flash erase, eFuse changes, factory restore,
save, node/bitrate changes, unattended repeated motion or electrical fault
injection are part of this handoff. A specific future need must have its own
source-backed test envelope. Mode, alarm-clear and counter-zero tests explicitly
assigned here are new user intent, not startup/probe/recovery side effects.

## Diagnose, reproduce, correct and retest

Implement useful diagnostics through the ordinary public codec/service/owner
path, not a raw sender or second receiver. Copy authoritative traffic into
bounded application storage; console pressure must not stall RX/service or
silently cancel submitted work. Keep admission, SDK enqueue, TX report, SDO
confirmation, drive observation and physical movement distinct. S2 receive
bounds are not exact timestamps. Packet 11 owns the reusable HIL tooling.

For every failure retain the first failing intent, revision/image, fixture,
operation/step/epochs, exact request/reply bytes, queue/TX/retirement states,
receive bounds, processing/deadline times, errors, drops, stack and service gaps.
State observations separately from hypotheses. Identify the earliest divergent
boundary and a measurement that distinguishes competing causes. A later pass,
longer timeout or guessed explanation does not establish the culprit.

Deliberate repeats are authorized as **new finite trials**, not hidden retries.
Choose count and total budget before starting. Stop on unexpected motion,
uncertain write, lost required evidence or a bus fault. Reconcile/retire all old
work and establish valid current state before another trial. Releasing a result,
rebooting the MCU, clearing a latch, waiting or changing a local token does not
retire a remote reply or establish the effect of an earlier write. Never loop
until green. After a confirmed fix, add a focused native regression when
representable, rerun the failing HIL case, then a short existing-feature check.
Preserve failed records. If instrumentation cannot isolate the cause, report the
remaining alternatives and exact next measurement instead of declaring a fix.

## Verification and finish for every packet

Inspect `.github/workflows/verify.yml`, `scripts/verify.py`,
`scripts/check_firmware.py`, `test/CMakeLists.txt` and `docs/verification.md`.
Reuse actual registrations and pins. Use short Windows build paths.

For behavior/API packets run the focused CTests and
`python scripts/verify.py --mode full --compiler gcc --build-dir .build/hN`.
The full verifier owns native/package/strict Doxygen gates. Obtain its actual
native build directory from its artifacts for `ctest --test-dir ... -R ...`;
do not guess a nested path. Owner/adapter/application changes additionally use
the affected `check_firmware.py --target s2`, `idf`, `arduino` and `component`
gates, with the pinned SDK environment. Missing tools are reported, not bypassed.
For ledger changes run its checker/tests and generator/check tests. Always run
promptset integrity and source diff checks. Keep tests meaningful; record
intentional contract changes instead of weakening an old negative assertion.

Verify required CI on the exact final implementation commit for W2–W9 and other
major milestones, as the active audit requires. Small documentation-only
packets need local checks; do not manually dispatch redundant CI. Preserve
reference/bench bytes and verify changed hashes in working tree, INDEX and HEAD.
Raw evidence may have original line endings; do not normalize it for lint.

Write `docs/reports/handoff_19_33/packet_N.md` and update affected original prompt
reports, PROGRESS, ledger/coverage, backlog/roadmap, guides/examples, Doxygen and
CHANGELOG where facts changed. Record actual checks, review findings, source
delta, branch/commits/sync and remaining evidence. Keep the report short and
link canonical contracts. Each packet must leave a complete checked deliverable.
An unsupported stub, raw accessor or fake success does not complete an operation.
Mark partial firmware scope precisely and continue independent assigned work.
Close only the packet assigned in a single-packet chat; execute more only when
the user assigns a range. The [index](README.md) gives dependency rules.
