# Prompt 10 Axis coordinates and references

Apply [COMMON.md](COMMON.md). Prerequisites: [9 Status and exact units](9_status_units.md).

## Objective and boundaries

Implement complete pure axis configuration, coordinate conversion and reference-aware target preparation using production exact arithmetic. Provide an offline preview example. This prompt produces checked targets and reference evidence; it does not transmit motion or claim drive completion. Finite position/angle/velocity/homing sequences remain assigned to their later prompts.

## Read and audit

Refresh RS, ADS1115 and INA228. Read RS `Axis.h/.cpp`, `Units.h/.cpp`, operation target callers and boundary tests; inspect implementation checks for ESS RPM/native-width leakage, not merely field names. Inspect current FieldCore exact motor targets, configuration/binding generation and reference semantics read-only. Apply [CONVENTIONS.md](CONVENTIONS.md#3-motion-typed-reads-and-terminal-evidence) and [MULTIDRIVE.md](MULTIDRIVE.md#required-contrasting-test-cases), using the reviewed contrast ledger.

## Implement

1. Implement `validateAxisConfig`, `configureAxis`, `setAxisOrigin`, `invalidateAxisReference`, `preparePosition` and `prepareVelocityTarget` with the agreed direct function style. Publish candidate configuration or prepared output only after complete validation. Preserve `prepareMoveAbsolute/Relative/Angle` vocabulary for later operation APIs without premature success implementations.
2. Model command scale, feedback scale, motor/load coordinates, gearing, origins and configured linear travel explicitly. A servo can have command increments and feedback counts without full-step/subdivision geometry. Unknown geometry is unavailable evidence, not zero and not an assumed default. Logical axis identity remains independent of CAN node/channel/subaxis.
3. Use exact integer/rational conversions, explicit rounding, quantization and inclusive soft/native limits. Generic signed velocity cannot contain an ESS int16 RPM bound; each profile later checks its native representation. Dimensional acceleration remains distinct from Lichuan millisecond ramp parameters. Current, torque and current-limit concepts are not substituted.
4. Define required versus optional reference policy, coordinate basis, observation time/freshness, generation and invalidation. Handle reboot, rebinding, dialect/mapping/scale changes, uncertain movement, device counter clear and explicit release according to their actual dependence. Invalidate prepared work when its required configuration or reference changes; do not make unrelated observations renew stale position evidence.
5. Specify generation exhaustion, monotonic time bounds and future/stale observation handling. Age expiration is an explicit application policy, separate from later operation completion requirements for new post-trigger evidence. Relative target preparation fails when its chosen basis is unavailable; it cannot silently switch to another coordinate basis.
6. Implement `examples/units_preview` and a small native consumer using supplied illustrative configuration. Show exact targets and quantization without opening a controller or creating a device lifecycle. Run prompt 4's servo-without-step-geometry, independent scale, subaxis and float-native boundary cases through actual common code; no alternate production driver is introduced.

## Verify and exit

Test motor versus load origin, gearing, independent command/feedback scale, exact and fractional native targets, inclusive limits, overflow, generation exhaustion and invalidated preparations. Verify optional/required reference behavior and stale/future timestamps. Keep adjacent positions beyond `2^53` distinct. Exercise wrapped-angle and half-turn target-policy primitives where part of this pure API; actual angle motion is prompt 34.

Check unchanged output on failure, signed velocity without RPM narrowing and rejection of excessive native float error. Compare applicable vectors with current RS and run preview, native consumers, focused verification/Doxygen, integrity and diff checks. Exit with working pure targets and explicit reference policy. Write `docs/reports/prompt_10.md`, update PROGRESS and affected documentation/coverage, then commit and sync this branch. Useful complete checked chunks may be synchronized during the prompt.

## Documentation and delegation

Apply [DOCUMENTATION.md](DOCUMENTATION.md#doxygen-contract-in-the-source) to every public field/function: unit, frame, validity, scale, rounding, time, ownership and failure behavior. Update the axis guide with one clearly illustrative conversion. Apply [MAINTAINABILITY.md](MAINTAINABILITY.md#make-control-flow-and-state-reviewable). Under [DELEGATION.md](DELEGATION.md#task-brief-and-handoff), one agent owns Axis types; independent reviewers supply coordinate/generation counterexamples and verify arithmetic boundaries using stronger reasoning where needed.
