# Prompt 5 CANopen service fit experiment

Apply [COMMON.md](COMMON.md). Prerequisites: [3 Reviewed Lichuan semantics and family gates](3_lichuan_semantics.md), [4 Stepper and servo contrast ledger](4_drive_contrasts.md).

## Objective and boundaries

Select one authoritative CANopen service implementation using a narrow buildable experiment. Evaluate a pinned CANopenNode port against the actual ownership and boundedness requirements; choose the smallest viable alternative only when concrete costs or blockers justify it. This prompt delivers the experiment and decision, not a complete stack port or motion engine. Prompt 6 freezes the shared production contract using this evidence.

## Read and audit

Refresh RS, ADS1115 and INA228 and inspect current transport ownership, callback/storage and deadline seams. Read RS owner/service-facing operation code, ADS1115 owner-loop handling, INA228 transport boundaries and FieldCore's relevant owner contract read-only. Check real callers/tests, not just type names. Apply [CONVENTIONS.md](CONVENTIONS.md#4-one-application-can-owner) and [MULTIDRIVE.md](MULTIDRIVE.md#required-contrasting-test-cases).

Inspect current primary CANopenNode source, API, license, configuration and native test port at a recorded immutable revision. Inspect actual candidate controller APIs where needed to answer TX retirement and callback questions. The earlier RS feasibility note recommends evaluation; it does not mandate adoption without this assessment.

### Concrete evidence to recheck

Inspect the pinned CANopenNode and canopen-python sources in the [public implementation review](../audit/LICHUAN_PUBLIC_IMPLEMENTATIONS.md). Record selected revision, ownership, SDO lifecycle, timeout/retry configuration and license obligations. Python `RESPONSE_TIMEOUT`/retry defaults and the example's sleeps are host policies, not CL86-C timing guarantees. The third-party Lichuan example has incorrect upload command bytes and uncorrelated status; use it to test the fit experiment's rejection paths, not as the service implementation.

## Implement

1. Define the experiment's smallest required services from the reviewed ledger: initial SDO forms, heartbeat/NMT/EMCY handling, selected PDO/SYNC needs and observable controller evidence. Keep provisional request/frame types local to the experiment; identify which declarations prompt 6 must freeze rather than exposing an accidental public ABI.
2. Build one native fake-controller exchange using the candidate adapter. Exercise an SDO transfer with an asynchronous event interleaved. Demonstrate who owns RX dispatch, queued TX, request payload, session state, callback data, time input and terminal results. No real CAN traffic is required or authorized by this experiment.
3. Inspect and record allocation at setup versus steady state, capacity/configuration mechanisms, C/C++ interoperability, reentrancy/thread assumptions, callback lifetime, event routing, client capabilities, timeout semantics and cancellation. Establish whether accepted TX can still be retired before transmission and how uncertainty is reported when it cannot.
4. Record dependency export and license effects. The installed C++11 pure core must remain usable without SDKs, stack headers, Python, sibling repositories or network access. An optional application adapter may have documented setup allocation; that does not weaken or falsely extend the core's no-heap guarantee.
5. Compare the observed candidate against a bounded local subset, using actual code/API constraints and the required service list. Select exactly one authoritative active service engine. If the stack cannot meet an essential boundary, document the specific blocker and the direct local implementation plan; do not retain two competing SDO/PDO engines or a runtime stack-selection framework.
6. Write `docs/decisions/canopen_stack.md` with pinned inputs, build command, measured or inspected storage/resource facts, unresolved adapter premises and the chosen boundary. Keep the experiment small, reproducible and clearly separated from installed production files. Prompt 7 will register the retained proof in the real build.

## Verify and exit

Compile and run the native experiment without Arduino, ESP-IDF or FieldCore. Check payload retention, interleaved event delivery and terminal-result ownership. Review failure/cancellation/TX retirement behavior against actual source, even when the small executable demonstrates only representative cases. Distinguish measured values from source inspection and hypotheses; do not claim unperformed memory or timing measurements.

Exit with a reproducible proof and an actionable service decision. A feature matrix or prose-only stack choice is insufficient. Close scope/checks in `docs/reports/prompt_5.md`, update PROGRESS and architecture backlog, then commit and sync the same execution branch. Coherent reviewed experiment checkpoints may be synchronized earlier.

## Documentation and delegation

Apply [DOCUMENTATION.md](DOCUMENTATION.md#doxygen-contract-in-the-source) to any proof declarations and [MAINTAINABILITY.md](MAINTAINABILITY.md#keep-the-smallest-design-that-expresses-the-real-contract) to the choice. Under [DELEGATION.md](DELEGATION.md#task-brief-and-handoff), delegate candidate API/resource review separately from the narrow build. Require stronger independent review of ownership, late-response and cancellation premises. The lead owns the decision and integration; agents do not install alternate engines independently.
