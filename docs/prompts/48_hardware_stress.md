# Prompt 48 Failure load and related-model qualification

Apply [COMMON.md](COMMON.md). Prerequisites: [47](47_hardware_finite.md), plus fixture-specific prerequisites and applicable authorization for each additional action.

## Objective and boundaries

Extend established finite qualification with bounded interruption, persistence/restart, sustained-load and related-model evidence. Complete only campaigns the actual setup supports, preserving every required unperformed item as pending. This physical prompt cannot be completed through simulation or paperwork alone. If prompt 47 or a required fixture is pending, retain this prompt as **Pending** or **Blocked** and continue independent software prompts 49–50 without claiming qualification.

## Read and audit

Refresh RS failure investigation/session/repeat practices and ADS1115/INA228 distinctions among sustained observation, host regression and evidence-required results. Inspect current target firmware/runners, prior finite results, unresolved failures and relevant FieldCore evidence boundaries. Recheck original model-specific manual/EDS statements for persistent effects, restart, stop/watchdog, I/O and resource limits. Apply [CONVENTIONS.md](CONVENTIONS.md), [MULTIDRIVE.md](MULTIDRIVE.md), [DOCUMENTATION.md](DOCUMENTATION.md), [MAINTAINABILITY.md](MAINTAINABILITY.md) and [DELEGATION.md](DELEGATION.md).

## Implement

1. Prepare a bounded campaign plan per fixture and feature with exact revision, configuration, intent, duration/count/load bounds, observation method and independent stop/control provisions. Distinguish inherited finite evidence from new qualification questions. Do not run an unlimited soak, random fault injection or a new stress scenario merely because a prior motion test was authorized.
2. Exercise selected interruption/fault/link-loss/recovery scenarios only where the physical setup and authorization cover them. Retain original operation uncertainty and separate stop/cleanup outcomes. Host heartbeat observation does not prove drive behavior when the host disappears; a communication cut does not guarantee a stop. Never automatically reconnect/re-enable/replay uncertain motor work.
3. Test reviewed persistence, reset and restart behavior with explicit before/after readback and model-specific semantics. Record partial effects and actual restart/configuration identity. Backup or restoration uses reviewed typed operations; a failed restoration is not guaranteed rollback and must not become an automatic retry recipe. Preserve changed fixture state for reconciliation.
4. Run finite sustained-load and multi-node/console-pressure scenarios suited to the fixture. Measure service gaps, resource/capacity use, dropped events, raw TX/RX evidence, observation limits and physical outcomes. Account for tracing overhead. Software timestamps and CPU load alone do not establish electrical CAN timing or all-condition motor reliability.
5. Track OL86-C, CL57-C and OL57-C separately from CL86-C. For any required related fixture available, repeat its applicable baseline and finite prerequisites before failure/load trials; shared documents/code or motor technology do not qualify it. Keep per-model interface/firmware/motor/feedback/stop behavior separate. Unavailable related fixtures retain explicit qualification tasks.
6. Investigate every failure deliberately: retain exact evidence, separate observation from hypothesis, reproduce when appropriate, establish cause, apply the simplest fix, add representable host regression and rerun the failing case plus a short established-feature regression. Preserve failed stop/restoration evidence and do not count a later lucky pass as a fix.

## Verify and exit

Reconcile planned versus executed campaigns and finite bounds against retained logs. Verify artifact identity, physical observation method, partial effects, known limits and per-feature outcomes. Run affected software/firmware gates for any fix and obtain new hardware evidence for changed physical behavior. A host fake regression proves only what it models. Missing required campaigns leave this prompt partial/pending; a software candidate may proceed with exact limitations, but full family qualification cannot be inferred. Short regressions do not replace sustained, electrical or per-family evidence.

## Documentation and delegation

Update qualification reports, bench configuration, scenario guide, conflict ledger, failure backlog and release limitations. Learned public behavior requires matching Doxygen/tests/CHANGELOG. Use one authorized fixture operator. Normal engineering agents can review artifacts or add disjoint host tests; stronger reviewers challenge causal explanations and model generalizations. They must not open another console or alter the bench concurrently.

Close completed owned work and checks; record `docs/reports/prompt_48.md` with exact executed campaigns and pending obligations. Commit and sync verified completed chunks on the same execution branch before handoff; keep unavailable qualification visibly pending while independent software work proceeds.
