# Stage 15 Implement the shared CLI and correlated JSONL

Apply [COMMON.md](COMMON.md). Prerequisites: stages 08–14.

## Audit

Read current RS ProbeConsole command inventory/parser/formatter, `@ID` JSONL protocol 3, ProbeApp public dispatch and Python protocol checks. Compare ADS1115/INA228/SHT3x commands, cached health/status and runner parser behavior. Reconcile source versus old docs: removed RS aliases must not reappear accidentally.

## Deliver

1. Implement one shared command inventory driving dispatch/help/capability output on native, Arduino and ESP-IDF. Connect it to the stage-14 application and bounded platform input/output seams. Follow `docs/api_cli_mapping.md`: local diagnostics, explicit reads, typed settings, exact preparation, finite motion, actions, discovery, persistence and CAN network services.
2. Use public library APIs for every drive command. Keep conversions, controlword sequencing, object writes and completion logic out of the parser. No private raw-register escape command may bypass public validation. Add native groups only for actual implemented typed features.
3. Preserve plain human commands plus `@ID` correlated JSONL, distinct request/operation IDs, retained result lookup and stable output mode for asynchronous replies. Publish library/profile/protocol identity, capability, terminal outcome, wire/acceptance/execution/uncertainty evidence and raw protocol detail. Version schema deliberately; RS protocol 3 compatibility must be checked, not assumed.
4. Keep `status`, `health`, `stats`, `result` passive; `stats reset` local; `recover` host-only; `cancel` local. Distinguish `release ID` from `motor-release`. Explain stop normal/fast profile behavior and unsupported operations with actionable reasons.
5. Implement strict bounded ASCII/numeric parsing, exact rational targets, overflow/negative/signedness checks, explicit units, token/line limits, trailing-junk rejection, whole-candidate validation and rejection of truncated lines before any admission. Use the audited 128-byte/22-token RS bounds as a comparison, choosing and documenting enough capacity for the CAN grammar.
6. Bound output/backpressure and reserve control responsiveness. Passive debug/capture copies owner traffic, never reads the controller itself. Enumerate every command's side effects and map it to public API and parser tests in the coverage ledger.

## Concrete implementation contract

Keep parsing/dispatch in `examples/probe_cli/ProbeConsole.*`, application actions in `ProbeApp.*`, and optional shared formatting helpers small. Freeze one canonical command inventory including grammar, API call, capability, side-effect class, completion shape and help text.

| Route group | Concrete behavior |
| --- | --- |
| `help/version/profile list/caps` | Describe actual installed profile and schema; no surveyed driver as supported |
| `status/health/stats/drv/result` | Cached views only; unknown/stale fields stay explicit |
| `probe/read identity/read config/read state` | Explicit bounded public read operation |
| `axis config/axis origin/prepare` | Exact host preparation/configuration with documented reference effect |
| `moveby/moveto/move relative/move absolute/move angle/velocity/home` | Same public typed requests as native callers |
| `enable/motor-release/alarm-clear/position-clear/stop normal/stop fast` | Profile-supported explicit action, never parser-owned controlword logic |
| `cancel/release ID/recover/stats reset` | Different local/host responsibilities; none silently clears a drive fault |
| `driver/io/control/tuning/communication/persistence` | Only reviewed typed groups; clear mutating versus read-only routes |
| CANopen `nmt/pdo/heartbeat/network` | Explicit protocol-specific operations with their effect classified |

Retain the fuller route inventory in `docs/api_cli_mapping.md`, including relevant settings, motion-profile snapshots, discovery, monitor, capture and workload routes. Do not add bare `reset`, `ping`, `ver`, `?`, `health check` or old `motor ...` grammar. `capture-read` is an active bounded diagnostic read through the owner; passive trace is a different activity. Do not make it a hidden raw controller receiver.

Define JSONL schema/version before the runner: library/profile identity, request correlation ID, admitted operation ID when present, record kind, typed status/outcome, uncertainty, optional native evidence and exact numeric representation. Select fields from actual RS protocol-3 source, documenting deliberate CAN changes rather than claiming binary compatibility. Preserve integers above 2^53 as exact decimal strings or another documented lossless schema representation. Distinguish unknown, false, zero and absent capability.

Test a request that is rejected before admission, one admitted then failed after possible TX, and one completed with later unrelated events. Retained results must explain each without reissuing a command. A malformed or overlong input line is rejected whole; an overlong output must never look like a complete JSON object. Backpressure cannot starve owner service or stop admission.

## Documentation and work split

Apply [DOCUMENTATION.md](DOCUMENTATION.md) and [MAINTAINABILITY.md](MAINTAINABILITY.md). Update CLI grammar, protocol examples, help, side-effect classifications and API coverage together. Use actual tested output in examples; Doxygen documents console buffer/lifetime/output behavior.

Under [DELEGATION.md](DELEGATION.md), independent parser tests and help/coverage review are useful. One writer owns command inventory and schema. Reviewers verify that every device route reaches the public API and no capability is implemented only in the console.

## Verify and exit

Run command/API parity, help inventory, all invalid parser boundary cases, JSON escaping/schema/line integrity, interleaved events, mismatched IDs, late replies, retained results, queue-full and stop while output is congested. Test removed aliases reject cleanly. Rebuild the native, Arduino and ESP-IDF examples with the real shared console now included; verify the same command inventory, routes, payload semantics and bounded service responsiveness. These are the console gates intentionally deferred from stage 14.

Exit with a documented CLI and a machine protocol ready for an independent Python client. No console-only motor functionality is allowed.
