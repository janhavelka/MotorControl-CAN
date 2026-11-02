# Packet 1: inventory and prerequisite prompt repair

Executes audit W1 only. This repairs prompts, one checker, ledger read owners and evidence documents. **W2 to W8 and motion are not done**; the mandatory hold in [COMMON item 5](../../prompts/COMMON.md) stays. No port, firmware upload or CAN traffic. The [execution table](README.md) holds per-packet status.

## Entry state

Branch `promptset/motorcontrol-can`, base `7878f7c`, upstream `origin/promptset/motorcontrol-can`, HEAD `16a706e`, 0/0 ahead/behind after fetch. Nothing staged. Three untracked `.vscode` files are unrelated and preserved. Tools: Python 3.12.10, CMake 4.0.1, GCC 15.1.0, Doxygen 1.13.2, gh 2.96.0. CI succeeded on `7764c89`, `c643af8` and `4ae93e3`; the run on `16a706e` also succeeded, including the job that builds the S2 target.

| Sibling | Revision | Dirty | Seam | Disposition |
| --- | --- | --- | --- | --- |
| MotorControl-RS | `776facd` | Clean | R-05 persistence names; R-06 additions; R-14 stop behavior | Dated delta added to `docs/audit/MOTORCONTROL_RS_AUDIT.md`; each addition has an adopt or not-applicable row in `docs/api_cli_mapping.md` |
| ADS1115 | `d613a13` | Clean | Unchanged since the handoff authoring | No new behavior adopted |
| INA228 | `73f9548` | Clean | Unchanged since the handoff authoring | No new behavior adopted |
| FieldCore-node | `5ad9292` | Clean | `ownership.md` anchor | Superseded header corrected the anchor to `:54-56` |

## Delivered

- **Graph (G-01, G-02, G-03).** See the [changelog](../../../CHANGELOG.md). 18 has no descendants. 41 and 42 stay ancestors of 44, 45, 47 and 48. Guards and tests are in `scripts/check_promptset.py` and `scripts/check_promptset_test.py`.
- **Rule and state (G-02, N-02).** Complete keeps its meaning: owned scope and checks closed. Prompts 23, 25 and 26 become Complete because their reports close owned scope and they were Partial only on inherited gates. Decision: a prompt with an audit-confirmed defect in owned scope is reopened and Partial, so 24 and 27 stay Partial, 28 stays Partial for G-04, and 10, 13, 14 and 17 move from Complete to Partial until their packets close them. 29 stays Partial because acquisition is owned scope. The narrative moved unchanged to the [progress history](../progress_history.md), apart from one dash and rewritten link targets, verified by comparison with HEAD.
- **Read ownership (G-07, G-10).** 12 rows moved, 89 remain with 38, tested in `scripts/check_lichuan_ledger_test.py`. Decision: `607C` moves to 36 for read and write, because G-10 allows it, prompt 32 never named it and prompt 36 is the only user. The checker's owner rule follows.
- **Prompts 29 and 31 to 50.** Section 12's rows, section 10's audit rule and DEC-2, DEC-3 and DEC-8 obligations, written as what W6 will deliver. G-05 is retained as resolved: `ActionOperation.h` and `profiles/lichuan_c/Actions.h` exist with stop as the first action, as prompt 31's report records.
- **Shared documents.** N-01, N-03, N-05, G-08, G-11, G-12, G-13, G-14, R-05, R-06, R-12, S-12, DEC-7. N-04's NMT half no longer holds, so `docs/can_owner.md` is unchanged; its streak half stays with W5.
- **Decision.** `motion-profile restore|forget` belongs to prompt 41 as application code over the typed settings API, because RS does this in its application (R-05).
- **Audit file.** Its findings stay as verified at `c329c7e`; one bullet lists later corrections.

## Corrections to source and earlier claims

| Old claim | Evidence | Replacement |
| --- | --- | --- |
| `607A=5000` and `60FF=120` are retained targets or stale intent; "not defaults" in packet 12 | EDS defaults are 5000 and 120. Only post-cycle reads exist. No retained record writes them | Measured reads equal to EDS defaults; origin and effect unknown. [Corrected reading](../../device_conflicts.md#corrected-reading-of-the-november-2-inventory). Checkpoint and motion plan keep their text with a note |
| Addressed NMT produced a transition | Node 1 reported `7F` before and after command 128 | TX confirmed; no transition demonstrated. Raw NMT bytes are not retained |
| Q17 "mutation untested", Q18 "none", "no live sequence" | Retained checkpoint records | Measured cells in [device_conflicts](../../device_conflicts.md), with source, decision and measurement separate |
| "Exceeds the stack alone" (A-11); 8,192 bytes | `FIRMWARE_STACK_BYTES=12288`. The 12,352 worst chain was not re-measured | Noted in the audit; W7 and packet 8 own the measurement |
| G-04 "acquisition cannot run on the real board" | S2 firmware and CI job exist | T04 lists what remains: GPIO18 control only in the bench application, no shared S2 CLI, no Arduino S2 build |

These observations establish no status dialect, harmless reads, safe enable or fresh PDO samples. No gate was promoted. No PDO frame exists in any retained record.

## Facts for later packets

- `TwaiAdapter::transport()` still sets no `beforeTransmit`, so a firmware download needs an explicit pre-handoff note until one is added (C-09's remainder; packets 3, 5).
- Typed identity fails on the specimens at `1009` (packet 4). Qualified PDO samples are unreachable on TWAI today, because the driver supplies bounds only (packet 11).
- D-02's rule rejects a move while the observed mode differs and the drive reads as enabled. The specimens read `6061=0` and `2100=1`, so a move from power-up rejects until the mode question is measured (packets 15, 19).
- `Actions.cpp` still says "documented resettable FF05" (packet 17). `CONVENTIONS.md` keeps two owner and example notes for W2.
- G-11's list overstates the registration gate: `PdoConfiguration.*` passes without `check_repository.py`, so packet 2 should treat those lists as non-exhaustive.

## Checks and review

Passed locally on the final tree: `check_promptset.py`, `check_promptset_test.py`, `check_lichuan_ledger.py`, `check_lichuan_ledger_test.py`, `generate_lichuan_objects.py --check`, `generate_lichuan_objects_test.py`, `check_repository.py`, `git diff --check`. `Objects.h` and `Reads.h` changed only in a descriptor owner number and comments, so the full verifier was not run for this packet. That judgment was wrong: the automatic CI run on `c241b7e` failed the strict Doxygen gate in both full jobs, on a same-page anchor link added to `docs/canopen_services.md`. Packet 2's first commit fixes it and passes the full verifier locally. Doxygen inputs need the full verifier even when no code changes.

Delegated: read-row inventory, code and evidence recheck, seven disjoint prompt workers, and an independent review of the whole diff. The review confirmed nine defects, all fixed before commit: DEC-1 wording that still said non-changing, G-06 missed at prompt 37, stale retained wording in four guides, a stale `607C` owner, gate owners missing 36 and 38, no reciprocal integration in prompt 18, an unnamed mode 6 enable owner, D-07 and D-08 not everywhere, and inconsistent reopened states. Prompts 19-22 and 30 keep their older refresh sentence on purpose: the handoff has its own list and COMMON item 3 applies.

**Standing rule added.** The user's [bench and subagent authorization](../../prompts/COMMON.md) now applies to this promptset.
