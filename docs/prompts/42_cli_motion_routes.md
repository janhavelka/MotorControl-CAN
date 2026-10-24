# Prompt 42 Motion CLI and platform parity

Apply [COMMON.md](COMMON.md). Prerequisites: [34](34_angle_moves.md), [35](35_velocity.md), [36](36_homing.md), [41](41_cli_device_routes.md).

## Objective and boundaries

Finish the console's finite motion/action routes and verify the complete shared command contract on native, Arduino and ESP-IDF. Every motor feature must already exist as a public typed API; this prompt adds parsing/dispatch and integration tests. Do not solve missing firmware evidence in the parser or duplicate an operation state machine. Unsupported/gated capabilities remain visible with exact reasons and cannot be counted as implemented native coverage.

## Read and audit

Read current RS motion/action command grammar, `ProbeApp` admission/result handling, stop responsiveness and retained results. Compare ADS1115/INA228 owner/health inspection and SHT3x shared command classification. Inspect FieldCore's actual motor result boundary read-only for semantic differences. Apply [CONVENTIONS.md](CONVENTIONS.md), the complete API/CLI mapping, [MULTIDRIVE.md](MULTIDRIVE.md), [DOCUMENTATION.md](DOCUMENTATION.md), [MAINTAINABILITY.md](MAINTAINABILITY.md) and [DELEGATION.md](DELEGATION.md).

## Implement

1. Add the accepted `moveby`, `moveto`, `move relative`, `move absolute`, `move angle`, `velocity` and `home` routes using the same typed requests/native preparation as direct C++ callers. Preserve exact units, angle policy, coordinate/reference meaning, duration/deadline and preparation timing. CLI conversion code must not become the only implementation of a motor feature.
2. Add supported `enable`, `motor-release`, `alarm-clear`, `position-clear`, `stop normal` and `stop fast` actions through public operation APIs. Preserve profile-specific action/stop semantics and unresolved firmware gates. Distinguish device release from `release ID`, and physical stop from local `cancel` or host `recover` in help and result records.
3. Complete application dispatch for admitted operation IDs, terminal results, explicit release and urgent stop capacity. Preserve the original failed/uncertain motion result when a later stop succeeds or fails. No timeout, console disconnect or selection change automatically retries a movement or re-enables a drive.
4. Finish command/API/help/capability coverage across prompts 40–42. Publish one canonical inventory with actual grammar, capability, API, mutation class and completion shape. Native settings/network commands apply only to matching profiles. No removed RS alias should reappear inadvertently, and no survey-only profile may appear as supported.
5. Rebuild both firmware examples using the actual shared console, application and owner. Verify matching command inventory, routes, payload units, schema and behavior on native/Arduino/IDF. Platform startup, USB and clock differences remain thin adapters; there is no private firmware-specific motion path.
6. Complete bounded console/load behavior. Passive debug copies ordinary traffic. Reserve owner service and stop admission under full output queues, long invalid input and bursty events. Record dropped output explicitly, preserve complete JSONL records and never consume a frame away from normal dispatch to capture it.

## Verify and exit

Test valid/invalid requests for every motion/action route, exact numeric boundaries, unsupported modes, stale configuration, queue/result exhaustion and explicit output modes. Exercise rejection before admission, loss after possible TX, stop during congestion, late completion, unrelated events and result lookup without replay. Verify public/CLI parity for units and terminal uncertainty. Run JSON escaping/line integrity, interleaved IDs and removed-alias tests; rebuild native, Arduino, standalone IDF and applicable component consumers. Compilation/fakes do not qualify motion. The console must be ready for an independent Python client with no runner-private command requirements.

## Documentation and delegation

Publish tested examples and full CLI/JSONL contract, update firmware READMEs, capability/API mapping and CHANGELOG, and document changed application/console ownership in Doxygen. One writer owns inventory/schema. Normal engineering agents can review parser boundaries or platform build parity on disjoint files. Use an independent stronger review for stop/uncertainty/correlation behavior. The lead integrates the combined tests and follows [DELEGATION.md](DELEGATION.md) for same-branch work.

Close this prompt's owned scope and checks; record `docs/reports/prompt_42.md` with actual platform gates and unresolved device facts. Commit and sync on the same recorded execution branch before handoff, including useful complete checkpoints when appropriate.
