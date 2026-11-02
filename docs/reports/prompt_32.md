# Prompt 32: explicit fault clear and device-position zero

**Changed by audit W6.** [Packet 7](handoff_19_33/packet_7.md) made the alarm clear poll MONITORS and STATUS only, then read ERRORS once after the alarm clears, and rejects a `maximumAgeUs` below the cycle's service budget. Statements below about reads per cycle describe the earlier contract.

Subsequent [independent audit of prompts 31/32](prompts_31_32_audit.md) adds
failure-to-later-admission coverage through the real owner. The original scope
and firmware gates below remain in effect.

Date: 2026-11-02. Promptset v2 continues on `promptset/motorcontrol-can`, upstream
`origin/promptset/motorcontrol-can`, original base `7878f7c`. Entry HEAD was
`1cb0d54b45591320ffb6285e098b7d14562d6f7f`; fresh fetch showed 0 ahead/0 behind.
The index was empty. Three unrelated untracked `.vscode` files and
`docs/audit/PROMPTSET_AUDIT_34_50_2026-11-02.md` remain untouched and excluded.

## Scope and disposition

**Partial.** Two concrete, source-backed conditional paths are implemented using
the existing action and owner contracts. The separate controlword reset route,
broader state/fault scope and firmware qualification remain gated. Inherited
21/30/31 obligations remain visible; this stage consumes their verified owner,
typed reads, action lifecycle and reference machinery, not unimplemented APIs.

The [action guide](../fault_reference_actions.md) owns the transition table and
application handoff. `prepareClearAlarm` supports the documented resettable
position-deviation alarm `FF05`, with fresh mode-1/nonrunning/alarm evidence and
retained error history. It writes `2301` U16 `1,0`; the second write returns to
the documented ineffective value as explicit library policy, not a vendor edge
or rearm claim. New monitor alarm-clear evidence establishes the narrow observed
result. Raw `603F` may remain nonzero because its latching behavior is unknown.

`prepareSetDevicePosition` accepts exactly zero and emits `2302` U16 `0,1,0`.
Its axis-aware transmission notice withdraws origins, limits, reference and
prepared-target validity before possible trigger transmission. The owner keeps
the old wire epoch through the three writes, then explicitly synchronizes the
already advanced host epoch before fresh readback. A zero `6064` observation
establishes raw actual-position zero, not physical home, causal proof of all
counter changes or automatic host rebase.

| Requirement | Delivered behavior / retained gate |
| --- | --- |
| 1. Explicit clear/position APIs | Both preparations use `nextAction/advanceAction`; nonzero position rejects with UNSUPPORTED before work. Native writes are not replaced by raw caller scripts. |
| 2. Reviewed transitions | Header contract specified before source implementation; durable table records admission, ordered values, ACKs, new reports and limits. No enable/controlword-reset preamble or inferred pulse time. |
| 3. Original fault evidence | Context owns the original typed read/history; owner/application incident snapshot preserves EMCY and failed operation results. New raw603F/status and reported alarm state remain separate. No retry, resume or health reconciliation. |
| 4. Counter reference effects | Checked atomic first-possible-trigger withdrawal uses existing COORDINATES invalidation; old typed positions and prepared targets reject. One configuration/reference increment survives success, failure and uncertainty; no manufactured origin/home. |
| 5. Atomicity and owner integration | Same owner/selected service engine, retained terminal per download, exact correlation, immutable processing deadline and later observations unable to rewrite failure. Trigger ACK survives final-zero failure. |
| 6. Assigned native rows | Canonical manual-function scopes for vendor alarm reset, current-position clear and controlword reset explicitly distinguish implemented subsets from pending route/firmware behavior. No I/O, tuning or persistence scope added. |

Still unsupported: controlword bit-7/full-word-128 reset without resolved
current-state/rearm semantics; other alarm classes/code mappings; nonzero device
position; arbitrary drive/mode states; guarantees about internal resumption,
which counters change, reference preservation or physical home. `2301` retained
level/repeat-trigger semantics remain unknown even after the explicit return to
zero. Q01/Q13/Q14/Q20 and applicable bench qualification remain open. These are
precise source/firmware limits, not success stubs or work transferred to another
stage. Save/factory reset stays 39 and successful homing/reference publication 36.

## Fresh source audit

All sibling work was read-only. No sibling fetch, edit or unchanged test suite
was performed. The four library HEADs below advanced since prompt 31; FieldCore
did not. MotorControl-RS was concurrently modified: 19 dirty paths at first
inspection, later additional tests and implementation files. The lead's retained
mid-audit snapshot records 60 dirty paths plus relevant content SHA-256 values
in `.build/p32-source-audit.json`. Observations describe those live working
files, not merely their HEAD. The immutable reference baseline was preserved.
At the exit audit RS had committed that concurrent work as
`6ff237471c77a9c4755ca87128c33619e5165f1b` and was clean. The four inspected
action header/source/test hashes were unchanged from the retained mid-audit
snapshot; the other checkouts and inspected hashes also stayed unchanged.
The exit snapshot is `.build/p32-source-audit-exit.json`.

| Checkout | Revision | Actual seams inspected |
| --- | --- | --- |
| MotorControl-RS | `238e0923df67dd6ca37f57a073464e452bd3ee84`, dirty | `ActionOperation.h`, ESS `Actions.h/.cpp`, `ActionEvidence.h`, `Axis.cpp`, `ProbeApp`, actions/axis/console/uncertainty tests. Literal preparation/next/advance names, explicit zero-only request, separate write/observation evidence and retained unknown outcomes. Concurrent rejection-classification changes are not CAN protocol facts. |
| ADS1115 | `d613a13cd3d43471905c507bfb4094bba1c1da94`, clean | Owner callback/terminal and hardware uncertainty paths. Sensor recovery/profile replay is not copied. |
| INA228 | `73f954838d3fac8cce747d54d0ad738940d4c625`, clean | Hardware invalidation, first dirty cause, ambiguous accumulator reset and retained partial effects in native tests. |
| PCA9555 | `414e0f66b13082e974ec27a4a6e86985a65d83fd`, clean | Explicit output APIs, ambiguous write shadow withdrawal and matching verification; no implicit replay. |
| FieldCore-node | `5ad9292dc8481cf08be9a61f662495a6474d1da0`, clean | Motor reference/binding invalidation and retained results, task lane inspection/take semantics, coordinator generation checks and independently retained calibration fault. |

Reopened original EN86/ZH86 p15, EN86 p16-18/p20, ZH86 p18, ZH86 p12 and
OP86 p10/p13, plus EDS `[2301]`, `[2302]`, `[603F]`, `[6064]`. Source-confirmed:
U16 objects, explicit position-clear edge/manual return, EN86 alarm object values,
FF05/resettable cross-reference, signed raw actual position and agreed monitor
bits. Unknown/contradicted: `2301` absent from ZH86 table, disputed `6041` alarm
bits, controlword reset forms, reset timing, counter mapping and physical effects.
Nothing was physically measured. Original bytes/hashes are unchanged.

Intentional differences: CAN does not copy ESS auxiliary command values,
Modbus exception/echo rules, RS permission Boolean, or its requirement that the
raw fault code become zero. Native observed results are narrower than readiness,
physical reference or fault-cause removal. Conditional implementation notes are
kept in the same canonical ledger and generated documentation; whole-function
coverage flags remain false while their broader obligations remain unresolved.

## Review, checks and handoff

`p32_ecosystem` (`gpt-6.1-sol`, high) performed the fresh sibling audit and owned
all central action mutations. `p32_reference` (`gpt-6-astra`, high) independently
reviewed reference/owner handoff and owned the disjoint native suite.
`p32_sources` (`gpt-6-astra`, high) independently reopened original evidence and
reviewed destructive/uncertain effects. Lead integrated source checks, packaging,
documentation, combined verification and Git. No worker used hardware or changed
Git state or siblings.

Review found no blocking core defect. It corrected two comment ambiguities and
a test that incorrectly injected a new EMCY after its successful-action
prerequisites; historical EMCY now precedes fresh acquisition and a separate
new-fault test withdraws the action. A test comparing padding across different
copied read structs was changed to explicit raw/provenance assertions, while
same-object atomicity/terminal byte snapshots remain tested.

Final combined verification and synchronization are recorded below and in the
handoff. No serial/CAN port was opened, firmware uploaded or motor command sent
during this prompt. Host checks do not close the physical qualification gates.

| Check | Actual result |
| --- | --- |
| Full Windows GCC 15.1 verifier | PASS: 55/55 CTests, repository/test registration, generated versions/objects, standalone installed consumers and strict Doxygen 1.13.2. Command: `python scripts/verify.py --mode full --compiler gcc --build-dir .build/p32-full`. |
| Focused reference/owner review | PASS: 9,840 checks in the new `reference_action` executable; reference/action, stop, axis and CAN owner CTests also passed in the review build. |
| Evidence ledger and generation | PASS: 99 EDS indexes, 202 typed entries, 53 manual items; `check_lichuan_ledger.py` and `generate_lichuan_objects.py --check`. |
| Promptset integrity | PASS: 221 Markdown files, 2,220 links, 137 originals; staged and committed original-byte checks are required at synchronization. |
| Formatting and diff | Scoped clang-format 14 and `git diff --check` passed. |
| Resource observation | Windows GCC x64 `sizeof(ActionContext)=7496`, `ReadContext=1944`, `StateObservation=520`, `SdoCompletion=216` bytes. Candidate copies also need stack. No target stack high-water or execution-time measurement was performed. |
| Remote gates | Required on the final pushed commit: Windows, GCC/Clang ASan+UBSan, Arduino 3.3.11 and ESP-IDF 5.5.5. The handoff and `.build/p32-final-ci.json` retain the exact final commit/run outcomes. |

Full local logs are in
`.build/p32-full/20261102T123501911938Z-60884-c4o93v3b/`; the review suite is in
`.build/p32-reference-review/`. These are disposable host verification artifacts,
not bench evidence. Doxygen covers 25 public headers, 1,813 members and 68 free
functions. Installed package consumers link the new public preparations without
SDK, stack-header or sibling dependencies.

Next work within this stage is precise firmware evidence: qualify `2301` repeat
and retained-level behavior, resolve the separate controlword reset route and
its admitted states, and establish the device counters/reference consequences
of `2302` on the actual selected firmware. The prompt-29 fixture and finite
typed evidence path apply. New motion implementation must not treat these
native report results as permission to resume a target or publish a home.
No new branch, release or sibling change belongs to this handoff.
