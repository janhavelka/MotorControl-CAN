# Stage 17 Audit faults, cross-library contracts and consumers

Apply [COMMON.md](COMMON.md). Prerequisites: stages 03–16.

## Audit

Refresh current MotorControl-RS, all affected I2C examples/tests and FieldCore's actual motor contracts/owner/module. Read their recent changes rather than relying on stage-00 snapshots. Inspect target CI and every public API/CLI/runner path against the completed native ledger.

## Deliver

1. Build contract-focused cross-library tests for supported common semantics: exact units, preparation without I/O, output atomicity, unsupported rejection, admission versus wire versus execution, stale evidence, stop/cancel separation, retained uncertainty and reference invalidation. Separate namespaces/types do not require source or binary compatibility.
2. Add deterministic adversarial multi-node/multi-producer tests: SDO delayed/same-object ambiguity, malformed/duplicate replies, unexpected boot-up, PDO map changes, heartbeat loss, EMCY storms, bus-off, controller queued TX, receive/result overflow, CPU/console pressure and urgent stop at each stage. Failures must preserve the original motion result.
3. Use sanitizers where supported, strict warnings, deterministic malformed-input/fuzz-style corpora, ABI/public-header checks where relevant and all native/Python/package suites. Inspect no-heap/no-framework/no-I/O core assumptions and measured context/queue sizes. Avoid tests that simply restate helper implementation.
4. Implement a small framework-neutral fake consumer reproducing FieldCore's prepared work, asynchronous observation and typed motor-result boundary. Document how a future CAN owner/device module would map to FieldCore admission/wire/execution states, scheduler, settings and health. Do not edit FieldCore or claim its dormant RS adapter is deployed CAN support.
5. Reconcile catalogue/API/CLI/test coverage mechanically and manually. Check all confirmed documented native features, unsupported reasoning, warnings, removed aliases, default behavior, object access policies and hardware qualification labels. Fix confirmed defects at their root; do not hide them with longer timeouts or retries.
6. Write `docs/fieldcore_handoff.md`, update the source map and record concrete portability/maintenance risks with focused resolutions. Avoid creating a general bus framework or shared cross-repository dependency without demonstrated need.
7. Complete every contrast gate in `MULTIDRIVE.md`: N5 stepper/BLDC common intent, EPOS4 semantic differences, multi-axis logical identity, profile-specific rates/ramps/feedback, extended/RTR and FD rejection, native float quantization, optional acknowledgement/state and no automatic mixed-protocol probing. Test-only fixtures are excluded from production profile inventory and exports. Document why another real profile can be added without redesigning the common layer.

## Concrete implementation contract

Organize review by public contract and a reproducible counterexample, not by lines of code. Each finding identifies source, consequence, required fix and a focused regression where appropriate. Review these end-to-end traces:

1. Prepare a move, admit setup, lose the trigger response, request stop, receive a late matching write acknowledgement, then query both retained results.
2. Run an SDO on node A while node B sends PDO/heartbeat/EMCY; exhaust observer output and prove owner routing/stop responsiveness remain bounded.
3. Change configuration or receive boot-up between preparation and admission; prove stale work cannot execute.
4. Time out a write to an object, then attempt a different value at the same object; prove a late reply lacking echoed value cannot confirm the new value.
5. Build a native installed consumer with no sibling/framework/stack/Python/reference tree and use exact common preparation.
6. Feed N5 stepper/BLDC, EPOS4 servo and non-CANopen representation fixtures through the common boundary without fake installed support.

Add explicit event-timestamp/processing-time/cancellation precedence tests and prove health recovery or a later stop does not erase the failed operation. Check `clearStats` at counter saturation while a failure streak and uncertain result remain live. Check every result/caller buffer lifetime through release and ID exhaustion.

The FieldCore-like consumer remains a small example/test using ordinary values and supplied events. Map its admission, wire, execution and reference invalidation needs in documentation; do not copy FieldCore types or introduce a general application framework.

Reconcile public functions, generated descriptors, CLI routes, Python scenarios and ledger rows. A route may be deliberately API-only with rationale, but an in-scope confirmed function cannot disappear because a test never called it. Confirm descriptor/codec/read/setting/action/workflow coverage are counted separately.

## Documentation and work split

Apply [DOCUMENTATION.md](DOCUMENTATION.md) and [MAINTAINABILITY.md](MAINTAINABILITY.md). Update the FieldCore handoff, integration examples, fault semantics, coverage and actual verification results. Fix documentation defects alongside code defects; public comments and published examples are part of the review.

Use [DELEGATION.md](DELEGATION.md) for independent ownership/fault, native-coverage and package/docs reviewers. Avoid giving a reviewer only the author's summary. The lead resolves disagreements, integrates fixes and runs affected final checks once on the integrated tree.

## Verify and exit

Run the full appropriate verification matrix once against the integrated change, plus focused repeats for fixes. On a configured remote, verify major-change CI on the exact final commit and preserve terminal results. If no remote/toolchain exists, record that limit rather than pretending success.

Exit with an independent coverage/fault audit and no unresolved software failures hidden behind passing happy paths. Remaining vendor/hardware unknowns stay in the backlog with exact qualification tasks.
