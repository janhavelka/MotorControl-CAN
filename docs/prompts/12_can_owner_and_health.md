# Stage 12 Implement shared CAN ownership, scheduling and health

Apply [COMMON.md](COMMON.md). Prerequisites: stages 06–11; owner design was fixed in stage 02.

## Audit

Read RS `RtuBusOwner`, copied admission/result lifetimes, urgent stop, `ProbeApp` ownership/cache and passive traffic. Compare ADS1115 owner-loop, INA228 diagnostics, SHT3x health policy, PCA9555 cancellation and MB85RC retained results/counter overflow. Inspect current FieldCore bus owner and dormant motor-module boundary, not just old reports.

## Deliver

1. Implement `examples/common/CanBusOwner` and a narrow transport/service interface using the selected stack. Applications own controller/stack state, node table, queues, contexts and caches. Exactly one receive path dispatches all traffic; console/observer callbacks receive bounded copies.
2. Provide copied admission, fixed capacities, producer fairness, explicit deadlines, per-channel serialization, per-axis ordinary-operation reservation, urgent-stop capacity and terminal result retention/release. Preserve actual RS owner `admit`/`admitUrgent`/`service`/`result`/`release`/`cancel` naming; document CAN-specific signature differences.
3. Settle queued versus physically transmitted versus acknowledged work with real adapter evidence. Retire SDK/controller queued motion on cancellation/recovery/boot-up epochs before any re-enable can transmit it. Link-layer retries are not permission for application replay. If the SDK cannot prove retirement, preserve uncertainty and prevent unsafe resumption.
4. Integrate read/action/move/velocity/home work and asynchronous events, with bounded service work and declared storage. An active transfer cannot starve heartbeat/PDO reception or a stop on another channel/node. Stop through an occupied channel follows the explicit policy from stage 09.
5. Add the selected application `DriverState`/`Health`/`Stats` and cached accessors from CONVENTIONS, separating controller, NMT, drive, mode, liveness, data validity and operation state. Use its explicit saturating counter/reset rules. No hidden threshold, retry, rebind or motor reset is copied from a sensor.
6. Expose passive traffic and owner/resource diagnostics with drop accounting, fair console servicing and no normal-path interference. Recovery is explicit host recovery and cannot clear drive faults or auto-continue old motion.

## Concrete implementation contract

Implement `examples/common/CanBusOwner.h/.cpp` using the service/node/event structures already developed in stages 06–07. Follow actual RS owner names:

```text
admit(request, nowUs, outputId)
admitUrgent(request, nowUs, outputId)
service(nowUs, ...)
result(requestId)
release(requestId)
cancel(requestId, ...)
recover(...) / recoveryResult(...) / releaseRecovery(...)
```

These are naming/role sketches; freeze exact signatures from the CAN ownership proof. Do not mechanically retain UART parameters. Use `beginSequence` and generation invalidation only where their audited semantics apply. Avoid adding synonymous `submit/poll/getResult` APIs.

Apply the exact example health contract in [CONVENTIONS.md](CONVENTIONS.md): `DriverState { UNINIT, READY, DEGRADED, OFFLINE }`, `Health`, `Stats`, cached `health(binding)`, `driverState(binding)`, `stats()`, `clearStats()`. These are application projections, not CANopen drive states or portable-core lifecycle. No `LATCH_OFFLINE` policy blocks a stop or non-changing reconciliation read.

Per-binding Health counts complete logical service successes/failures and failure streaks; Stats counts frames/controller/queue/drop events. Each event has one authoritative counter owner. Use saturating unsigned counters and test the maximum boundary. Preserve observation freshness and raw drive faults separately. A heartbeat is not a successful service; a cleanup success does not overwrite the failed original service result.

Document the exact fixed capacity and per-service-turn budget for ordinary requests, urgent work, results, channels, bindings and observer copies. Admission reserves a terminal result slot before TX. A full retained-result table must cause an explicit admission failure, not eviction of unread uncertainty. Define ID/generation exhaustion instead of silently aliasing old results.

Cached `health/status/stats/result` calls produce no CAN traffic. `clearStats` clears diagnostic counters under its documented scope, never controller state, drive faults, pending operations, identity/reference evidence or required history needed to explain an uncertain operation. Health recovery does not auto-reenable motion.

## Documentation and work split

Apply [DOCUMENTATION.md](DOCUMENTATION.md) and [MAINTAINABILITY.md](MAINTAINABILITY.md). Publish owner API/lifetime and queue tables, health transition rules, counter scopes and reset semantics. Add Doxygen to example owner interfaces as well as installed headers.

Use [DELEGATION.md](DELEGATION.md): a strong independent reviewer traces ownership, stale TX retirement and urgent stop. Counter/fairness tests can be separate tasks after decisions are fixed. One agent owns receive dispatch and central owner storage.

## Verify and exit

Use multi-producer/two-node fake tests for full queues, retained-result exhaustion, fairness, concurrent channel work, urgency, cancellation at every wire phase, stale generation, bus-off/recovery with queued motion, receive overflow, clock boundaries, slow console and observer saturation. Verify caller-buffer mutation after admission cannot change admitted work.

Exit with a portable example owner exercised by a native fake application and documented RAM/queue limits. Do not import FieldCore tasks or mutexes into the library.
