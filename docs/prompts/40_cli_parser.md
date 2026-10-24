# Prompt 40 Shared CLI parser and local routes

Apply [COMMON.md](COMMON.md). Prerequisites: [26](26_native_examples.md), [28](28_firmware_examples.md).

## Objective and boundaries

Deliver the shared bounded console parser, correlated JSONL contract and complete passive/local command subset. This is a usable console increment with no placeholder device routes. Device reads/settings/network routes follow in prompt 41; movement/actions follow in prompt 42. All use the same inventory/parser and application. Do not implement a second motor controller or place exact arithmetic, object writes or controlword logic in the console.

## Read and audit

Read current RS `ProbeConsole` inventory/parser/formatter, `@ID` protocol-3 records, application dispatch and Python protocol checks. Compare ADS1115/INA228 cached diagnostics and SHT3x command classification; inspect FieldCore only for affected ownership semantics. Reconcile source versus obsolete aliases. Apply [CONVENTIONS.md](CONVENTIONS.md), `docs/api_cli_mapping.md`, [MULTIDRIVE.md](MULTIDRIVE.md), [DOCUMENTATION.md](DOCUMENTATION.md), [MAINTAINABILITY.md](MAINTAINABILITY.md) and [DELEGATION.md](DELEGATION.md).

## Implement

1. Create the shared `examples/probe_cli/ProbeConsole.*` parser/dispatch and one canonical implemented-command inventory. Record each route's grammar, public API/application call, capability, side-effect class, completion shape and help text. Keep application behavior in `ProbeApp.*` and small formatting helpers where genuinely shared. Native, Arduino and IDF use these same sources.
2. Implement applicable `help`, `version`, `profile list`, `caps`, `status`, `health`, `stats`, `drv`, `result`, `release ID`, `cancel`, `recover`, `stats reset`, host axis configuration/origin and exact `prepare` routes. Audit exact grammar against current RS and the CAN mapping. Status/health/stats/result remain cached; reset is counters-only, recovery host-only and cancel local. None silently stops or clears a drive fault. Unimplemented routes stay out of working help/dispatch; retain their obligation in coverage.
3. Parse strict bounded ASCII and numeric input with explicit units, exact rational values, checked signs/ranges/overflow, token/line limits and trailing-junk rejection. Compare audited RS 128-byte/22-token capacities and choose sufficient documented CAN bounds. Reject truncated or overlong lines whole before any admission or local-state publication.
4. Define the actual versioned JSONL schema before its Python client. Preserve plain human commands and `@ID` correlation with distinct request/operation IDs, stable asynchronous output mode and retained-result lookup. Include identity/capabilities, typed status/outcome, uncertainty and optional native evidence. Check RS protocol-3 semantics; document deliberate CAN changes rather than claiming compatibility by number alone.
5. Preserve integers above 2^53 losslessly, for example exact decimal strings with documented field types. Distinguish unknown, false, zero and absent capability. Define escaping, record limits, atomic line output and bounded backpressure. An incomplete JSON object must not appear to be a complete record. Stop/owner servicing must retain reserved responsiveness under output pressure.

## Verify and exit

Run parser/help/inventory tests for every implemented route and invalid boundary, removed aliases, output escaping and exact numerics. Reject bare `reset`, `ping`, `ver`, `?`, `health check` and obsolete `motor ...` grammar unless new authoritative source changes the accepted contract. Use fake application results to test pre-admission rejection, asynchronous admission/failure, interleaved unrelated events, mismatched IDs and retained terminal records without production success stubs. Build/run the native shared console and verify passive routes emit no device traffic. Full route/platform parity remains prompt 42's integration gate.

## Documentation and delegation

Publish the working subset in `docs/cli_contract.md`, help and API/CLI mapping; separate planned routes from implemented commands. Add real tested human/JSONL examples, buffer/lifetime Doxygen and CHANGELOG entries. One lead owns grammar/schema/inventory. A normal engineering agent can write independent parser tests; use stronger review for asynchronous correlation and backpressure if needed. Follow runtime model availability and file ownership in [DELEGATION.md](DELEGATION.md).

Close this prompt's owned scope and checks; record `docs/reports/prompt_40.md` and outstanding route ownership. Commit and sync on the same recorded execution branch before handoff, including useful complete checkpoints when appropriate.
