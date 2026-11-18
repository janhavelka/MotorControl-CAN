# Prompt 55: communication self-repair, node presence and retry policy

State: **Partial**. The library and adapter primitives, console forms, bench routes and their
host tests are on `main`, and three short bench runs passed on the S2 with both CL86-C drives.
Four bench items need the user at the bench and remain open (below). Q18 stays unqualified.

## Scope

[Prompt 55](../prompts/55_self_repair.md) as revised on 2026-11-17 (primitives only; the
presence state machine, retry policy, reconciliation, restart schedule and opt-in fault clear
are [prompt 74](../prompts/74_reference_self_repair_example.md)'s). The user's planning commits
`97eaf0d8` and `28598674` extended it with the
[communication practice](../communication_practice.md) rules C2 (bus faults are interface
facts), C3 (evidence classes), C5 (controller error counters are bus evidence), C7 (hardware
retransmission is no retry) and C13 (counters).

## Source audit

| Repository | Revision and state | Seam and disposition |
| --- | --- | --- |
| MotorControl-RS | `afe3588`, clean, at execution; unchanged since the 2026-11-17 audit. Rechecked at this report: `5dbcaa1` with two files of concurrent RS bench work (`examples/common/BoardPins.h` modified, `examples/common/FixtureWiring.h` untracked). Between them only RS docs, prompts, bench evidence and its promptset checker changed; `include/`, `src/` and `docs/architecture.md` did not | `docs/architecture.md` "Only explicitly classified operations may be retried under application policy": adopted as `RetryClass`. RS has no retry type, so the names are CAN's own |
| FieldCore-node | `c40ea834`, clean, read-only | Its RS485 timeout, retry, offline and re-init policy is prompt 74's to adapt; the [audit](2026-11-17_fieldcore_rs485_audit.md) holds the comparison. Nothing from it is implemented here |

## Delivered

The contracts live in the guides; this list only names them.

- Timeout abort `0x05040000`, the per-binding `SdoReusePolicy` with its marker list and budgets,
  the owner's resync step, contradicting-traffic refusal and `reacceptReuse`: the
  [services guide](../canopen_services.md) "Settling and reopening a route" and the
  [owner guide](../can_owner.md) "Self-repair primitives".
- TX lifetime, NO_LISTENER and `recover()` as the one restart entry point with its causes: the
  [TWAI SDK guide](../adapters/twai_sdk.md) "TX lifetime, NO_LISTENER and restart".
- Presence observations, evidence classes, bus-level faults (interface `OFFLINE`, node rows
  `notObserved`), counters with their latest times and the TWAI error counters: owner guide and
  [CONVENTIONS](../prompts/CONVENTIONS.md) section 5.
- `RetryClass` on every prepared context, `classifyRead` and `RepeatPermission`.
- Console forms (`reuse`, `txlifetime`, `health link|counts`, `stats times`, `drv bus` and the
  new fields): [CLI contract](../cli_contract.md). Bench routes (`L`, `U`, `y`, `z`, the soak
  tail, the trigger response budget): [S2 bench README](../../examples/s2_bench/README.md) and
  [bench guide](../hardware_bench.md) "Prompt 55 self-repair bench".

## Decisions (lead)

- Reuse default `ACCEPTED_RISK_WITH_MARKER`, by the user's direction. It is never silent: the
  policy is named in `Health` and on every result, and every result on a reopened route carries
  `acceptedCorrelation`. `NONE` keeps the earlier quarantine.
- Retry classes: N is the default. R only for reviewed or DEC-1 accepted reads, or for an object
  with a recorded per-object permission. W for an absolute setting confirmed by readback: the
  write is W, while the readback read keeps its own class, N. M for motion, enable, homing, save,
  factory restore and NMT.
- A stop drops an unsent cleanup abort on its channel. An accepted best-effort stop ends the
  node's pending resync; reopening after it is explicit.
- The resync step arms on every quarantine with possible TX. It waits one service turn after a
  `RETIRED` recovery, so a held stop goes first, and it waits while NO_LISTENER holds.
- The marker list is four DEC-1 objects (`1000:00`, `1018:01` to `1018:03`) with marker budgets of
  their own, 0 inheriting the route's last request budgets.
- A heartbeat ends the miss streak, taking rule C3 literally.
- Bus-off, an unavailable or recovering controller, NO_LISTENER and an RX overflow episode take
  the interface `OFFLINE` and leave node rows `notObserved`. This revises prompt 25's delivered
  health, where bus-off made every binding `OFFLINE`.
- Only the bench image uses three SDO channels; the library default stays two.

## Review

An independent adversarial reviewer challenged the resync, restart and retry-class rules. The
owner worker fixed each finding, each with a regression that fails before the fix:

- D1: a no-marker reopening at a best-effort stop's TX report while its ACK was still pending.
- D2: the no-marker policy treated the timed-out request's own late reply as contradicting
  traffic.
- D3: a contradicting frame while a request waited did not quarantine the route.
- R1 (arming and re-arming after a recovery), C1 (an abort going before a stop), C2 and M1 to M9.

Before the review the owner worker found that a `RETIRED` recovery never reported pending SDO
handoffs as retired to `SdoClient`, which would have blocked every reopening after a restart.
The examples worker found that the bench image's `SoakTwaiTap` did not forward the new controller
status read, so the bench would have reported TEC, REC and the error state unknown.

## Checks

- `python scripts/verify.py --mode full --compiler gcc` passed on the final working tree of each
  code chunk: 171 registered tests, package and source consumers and strict Doxygen (retained
  under `.build/v55/`, runs `20261118T194904633241Z` before `12130ece` and
  `20261118T210540371808Z` before `50050825`). Two earlier runs of the first chunk failed at the
  package step: the packaged CHANGELOG linked to the unpackaged `docs/hardware_bench.md`, then
  to `docs/cli_contract.md`. Those links became code spans, and README links absolute URLs,
  before the passing run.
- Firmware gates for all targets passed. The strict stack gate reported 0 breaches. Doxygen 0
  warnings. The Python runner and evidence suites passed, as did the package link check.
- For this report: `python scripts/check_promptset.py`, `python scripts/check_promptset_test.py`,
  `git diff --check` and the package link check (export, then resolve every relative Markdown
  link in the zip).
- CI: the workflow passed on `12130ece` (run 44385907620) and `6e2f41f1` (run 44388150440). On
  `50050825` (run 44398900734) and `496a4342` (run 44399636377) it was still in progress when
  this report was written. For `50050825` the ESP-IDF, Arduino and Windows quick jobs had passed
  and both sanitizer jobs were running. For `496a4342` only the Arduino job had finished, and it
  passed. The lead records the final result on the exact commit; this report's own commit needs
  its own run.

## Bench

S2 board, two CL86-C drives (`100A=0020`), both read released in mode 1. These are single short
runs on two specimens, not qualification:

| Run | Image | Result |
| --- | --- | --- |
| (a) [1 ms response timeout on node 1, 89 s](../bench/20261118T200253Z-p55a-shorttimeout/summary.txt) | `12130ece` | 2067 timeouts, 2067 automatic markers, 2067 reopenings, 0 failed, reuse never stopped, no MCU restart. 0 aborts: every late reply was taken in the timeout's own owner turn. TEC and REC 0 |
| (c) [Absent node 3 beside nodes 1 and 2, 89 s](../bench/20261118T200459Z-p55c-absentnode/summary.txt) | `12130ece` | Nodes 1 and 2: 1171 and 1170 reads, all successful, at equal rates. Node 3: one probe and the owner's marker unanswered, then held with its route unresolved |
| [Link lines](../bench/20261118T211341Z-s2-link/rx.txt), idle | `50050825` | First board read of the controller status: state active, TEC 0, REC 0, no bus errors or arbitration losses |
| (c) [Absent node with a 250 ms marker budget, 45 s](../bench/20261118T211355Z-p55c2-absent-counters/summary.txt) | `50050825` | Nodes 1 and 2: 557 reads each, all answered. Node 3: probe, owner marker and explicit marker unanswered; [after the run](../bench/20261118T211441Z-s2-link-after-absent/rx.txt) `abortsSent=3`, node 3 `noResponse=3`, TEC and REC 0. One admission anomaly is recorded with `node=0`: not nodes 1 or 2, cause not established. The verdict `failed-evidence` reflects the absent node's expected failures and that anomaly |

A refused readiness attempt record (no CAN traffic) was deleted by the lead by mistake on
2026-11-18 and is not retained.

## Blockers and next work

These need the user at the bench:

- (e) a slow move with a forced 1 ms trigger timeout and no replay;
- (f) a quick-stop reissue;
- (d) unplugging the S2's CAN connector for 10 s inside the `U` window;
- (b) the 2026-11-04 explicit-marker cases (a best-effort stop or a client abort, then `y`/`z`),
  beyond what (a) shows.

The procedure for each is in the [bench guide](../hardware_bench.md). Q18 stays unqualified;
backlog D14, H04 and T04 keep their physical obligations ([backlog](../backlog.md)).
[Prompt 74](../prompts/74_reference_self_repair_example.md) owns the presence, retry,
reconciliation and restart policy built on these primitives.

## Branch and commits

Branch `main` (the prompt 42 instruction: no new branches until version 0). Commits:

- `12130ece`: primitives (abort, reuse policy, marker list and budgets, TX lifetime,
  NO_LISTENER, presence, retry classes, console, bench routes).
- `6e2f41f1`: first bench trials (a) and (c), and the soak evidence check accepting the new S
  line tail.
- `50050825`: evidence classes, bus-fault freeze, counters, TWAI error counters, console forms.
- `496a4342`: the board reads on `50050825`.

At this report the local `main` matched its `origin/main` tracking ref at `496a4342` (no fresh
fetch). This report and its PROGRESS row are uncommitted documentation for the lead to commit.
