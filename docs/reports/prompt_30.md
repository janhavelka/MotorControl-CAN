# Prompt 30: drive-state interpretation and enable/release evidence

**Reopened by audit W8.** [Packet 10](handoff_19_33/packet_10.md) added `interpretStatusword`, which `getStateInterpretation` now calls, with the reviewed `2100` masks beside it.

The [fresh independent 29/30 audit](prompts_29_30_audit.md) rechecks the original
assignment, implementation and source gates, adding direct overlap and real
boot-up publication regressions without changing action support.

## Status and scope

**Partial.** Passive documentary state
interpretation through the existing checked raw-read path is implemented,
integrated, independently reviewed and host tested. Enable/release
operations, their owner lifecycle and action acceptance tests remain pending;
no unsupported placeholder is counted as implementation. Prerequisites 19, 20
and 29 retain their separately recorded unresolved scope.

Execution continues on `promptset/motorcontrol-can`, upstream
`origin/promptset/motorcontrol-can`, entry commit
`faf3662d3571ba3d820a471a1d46c04e2d0d996e`. Entry fetch was 0 ahead / 0 behind,
with an empty index. Promptset version 2
and original base `7878f7c` are unchanged. The lead owns final Git/CI checks and
delivery. Three unrelated untracked `.vscode` files were observed and preserved.

## Source review and decisions

Original EN86 PDF pp11-13, 17-18; ZH86 pp11-13, 17-18; ZH57 pp12-13, 18; OP86
pp9-11 were reopened and relevant tables rendered. EDS sections `[6040]`,
`[6041]`, `[6060]`, `[6061]` and `[2100..2102]` were read directly. The five
original file hashes match the canonical ledger. No original material was
modified. The [durable guide](../drive_actions.md) owns the source/transition
table and the precise boundaries below.

- Confirmed documentary subset: controlword-associated low-three status patterns,
  positive modes 1/3/6 and the monitor bit definitions, including inverted
  enabled/released bit 4. These do not form a complete CiA402 state decoder.
- Contradicted documentary fields: EN86 alarm bit 7, zero-speed bit 10 and
  target-effective bit 12 versus ZH86 alarm bit 3 and running bit 14. ZH57 and
  OP86 describe alarm bit 7 and running bit 14. OP86's internal CL57-C/OL57-C
  header was observed despite its 86C filename/cover. This is retained source
  applicability uncertainty, not a newly selected firmware dialect.
- Position enable 6/7/15 and approximate 10 ms spacing are documented separately
  from the later position trigger. A blanket claim that enable ordering is
  undocumented would be incorrect. The unresolved prerequisite is concrete
  initial-state/no-pending-motion provenance; fresh mode 1/released/nonrunning
  flags and a newly acquired reservation cannot establish it.
- Velocity start order conflicts across sources, and retained-target/mode-change
  effects remain unresolved. A previous interrupted curve resuming on enable
  is a counterexample hypothesis, not measured behavior.
- No reviewed source establishes a motor-release CAN command/effect pairing.
  Status low bits 0 after `6040=0` do not prove release. Torque/brake/reference
  effects cannot be supplied from generic CiA402 or MotorControl-RS behavior.

No device fact was physically measured and no production action gate was closed.
Manufacturer clarification or a separately reviewed finite experiment must
resolve the dependent facts. The [device-evidence procedure](../device_conflicts.md)
owns acquisition prerequisites. No CAN transmission, firmware flash or motor
operation occurred. In the separately authorized bench follow-up, COM15 initially
failed before opening; after the user reconnected ESP-Prog2 and detached the PCB,
the probe-only UART opened and closed with no data written. With the target UART
reconnected, a no-reset ROM attempt failed, then automatic EN/BOOT entry succeeded
and identified ESP32-S2FH4 revision 1.0 / 4 MB flash. Both logs are retained and
the MCU remains in the bootloader. This establishes target UART/automatic entry,
not drive semantics or S2 firmware support; see the current
[bench record](../hardware_bench.md).

## Current sibling audit

All listed worktrees were clean at this review. Sibling suites were not rerun,
and no sibling files were edited.

| Source / revision | Actual seam inspected and intentional difference |
| --- | --- |
| MotorControl-RS `c0f124bdd6bb8c5da8a879e09ebb452752b11ae6` | `ActionOperation.h`, ESS `Actions.cpp`, action tests and `docs/ess_actions.md`: explicit command/effect pairs and separate acknowledgement/observation. ESS release at 0x002D is not a CAN command. New HEAD since prompt 29; current action evidence was reopened. |
| ADS1115 `235af3e8c722661a89a44766a4ccd903a654f6fe` | `examples/02_owner_safe_poll/main.cpp`: one serialized owner and bounded time budgets; sensor retries are not imported. Revision unchanged. |
| INA228 `b28c5ea69bcb4d26b351c52c24d3c6fae9fd282b` | `invalidateHardwareState` and `getDeviceIdentity` in `src/INA228.cpp`: invalidation retains accumulated evidence and revokes verified validity. Revision unchanged. |
| FieldCore-node `5ad9292dc8481cf08be9a61f662495a6474d1da0` | `Rs485MotorModule.cpp` admission/wire/setup/trigger/completion projections and readiness invalidation: observation, execution uncertainty and health stay separate. Read-only; revision unchanged. |

Final refresh found RS at `2642c30bd5f4cb1ee16050665e813101194bb5de`, clean.
The only intervening commit records its portability audit in two documentation
files; inspected action/owner code and tests are unchanged. The other sibling
worktrees remained clean. No uncommitted behavior was attributed to HEAD.

## Implementation and independent review

The integration contract is `State.h`/`State.cpp` with checked
`getStateInterpretation`, using STATUS and MONITORS through `getStateBlock`.
Explicit `StatusLayout` values name documentary alternatives; raw fields,
unknown bits and mismatch survive interpretation. No second read/service
engine, device I/O or implicit action is introduced. The aggregate read window
and five owned provenance entries preserve sequential-sampling skew. The combined
getter requires SDO STATUS/MONITORS blocks: their nine bytes cannot fit one
Classical TPDO. A later independent failed block does not erase completed raw
state evidence; current target/lifecycle/freshness checks still apply.

The native preview, source/installed consumers and isolated IDF component
consumer call the actual getter. Export/header inventories include it, the new
guide is exported and included in Doxygen, and the relocated package test runs
the preview. Tests use the real CanBusOwner and selected SDO service with a small
fixed fake wire; a heartbeat is delivered while an upload is active. They assert
literal uploads, retained results, all status/monitor bits, every low-bit pattern,
mode bytes, source isolation, disagreement, age equality, changed epochs,
incomplete reads, failure-preserved history and output-on-failure atomicity.

An independent source reviewer challenged both a blanket enable blocker and
the apparent `6040=0` release shortcut against original pages. The lead retains
shared API, build and Git ownership. Strong source/code review used `gpt-6-astra`
at high effort; independent ecosystem/integration and adversarial test work used
`gpt-6.1-sol` at high effort on disjoint files. The lead rechecked original table
images, actual code and combined diff. Review found no decoder runtime defect;
it corrected UNSPECIFIED documentation to say all status meanings remain unknown
and removed test comparisons of padding across separate successful publications.
The hardcoded checker fixture was updated for the real added artifacts without
weakening its negative cases. No source fact, ledger policy or generated object
permission changed.

Maintainability: one profile header/source with a direct checked getter reuses
existing acquisition/provenance validation. Its only loops copy two and three
provenance entries; storage is fixed and caller owned. There is no action engine,
profile base, SDK dependency, allocation, hidden time source or fallback state
machine. Common action enums were not installed without a concrete lifecycle.
README, CHANGELOG, guide, preview, source/API maps, coverage, backlog, roadmap and
progress distinguish the implemented decoder from remaining action scope.

The current raw-read lifecycle uses its overall processing-time cutoff;
passive interpretation cannot override it. Future action receive/processing
precedence, exact transitions, uncertainty and owner integration remain owned
by 30. Passive decoder tests cannot be substituted for those action tests.

## Verification and remaining scope

| Check | Actual result |
| --- | --- |
| Original PDF/EDS hash comparison | PASS for EN86, ZH86, ZH57, OP86 and EDS against the canonical ledger |
| Focused decoder | PASS: 6,841 checks through the real fake owner/service; `.build/p30-state-focused/Testing/Temporary/LastTest.log` |
| Integrated native regressions | PASS: `python scripts/verify.py --mode full --compiler gcc --build-dir .build/p30-full`, 53/53 CTests, GCC 15.1 / CMake 4.0.1 / Python 3.12.10 |
| Package consumers | PASS: exported-source and relocated C++11/C++17/no-RTTI, 23 independent installed headers, isolated native applications/preview and IDF CMake boundary |
| Strict Doxygen | PASS: 1.13.2; full verifier and a focused rerun after adding the guide to INPUT |
| Promptset / ledger / generated freshness | PASS: `check_promptset.py` (214 Markdown files, 137 original hashes), `check_lichuan_ledger.py`, `generate_lichuan_objects.py --check` |
| Formatting / diff | PASS: clang-format 14 for new header/source/test; `git diff --check` |
| Exact delivery CI | Windows quick, Linux GCC/Clang sanitizers and Arduino/IDF builds run on the pushed delivery revision; their final status is reported with that commit in the handoff, not inferred from previous CI. |
| Hardware qualification / action tests | NOT RUN; unresolved action semantics and fixture evidence remain separate |

Local combined-tree evidence is retained in `.build/p30-full/summary.json` and
its timestamped run directory. The final commit/sync and exact-commit CI result
are reported in the handoff; no older green run verifies this change.

| Original requirement | Disposition |
| --- | --- |
| 1. State interpretation / transition table | Documentary decoder and source-backed conditional transition table delivered; firmware-qualified action interpretation remains gated |
| 2. Concrete enable/release lifecycle and common action evidence | Pending; no always-unsupported placeholder or fictional release command installed |
| 3. Explicit admissibility / setup / confirmation / consequences | Source table and precise gaps delivered; actual timed lifecycle/torque/reference semantics remain pending |
| 4. No stale-target enable | Missing initial/retained-motion provenance explicitly blocks the action; a new reservation or fresh mode/status alone does not supply it |
| 5-6. Real action work, independent outcomes and precedence | Existing owner/service/read regression evidence retained; new action-specific integration and timing tests remain pending, not claimed by decoder tests |

Stage status remains Partial
until confirmed enable/release semantics, real finite operations and all their
owned acceptance checks are complete. The next evidence work is a concrete
enable admission/retirement boundary and a source-backed release command with
its admissible states and consequences; neither is supplied by this decoder.
