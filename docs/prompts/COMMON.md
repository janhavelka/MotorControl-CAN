# Common execution contract

Every numbered prompt incorporates this contract and root [AGENTS.md](../../AGENTS.md). The active sequence is [SEQUENCE.json](SEQUENCE.json): work through its numbered prompts in numerical order within the assigned range, advancing to independent ready work when an earlier prompt is blocked as described below. Prompts 1-50 are the original sequence; 51-78 are explicit later additions, not prerequisites for claiming the original scoped software candidate. Historical 00-19 files in `archive/v1/` are traceability records, not additional assignments. Each current prompt is one complete work unit with its own tests, documentation and commit/sync gate.

The prompt42 user instruction authorizes merging into `main` after its gates and
continuing on `main` until version0 is finished, with no new branches. This takes
precedence over the historical execution-branch instructions for that period.

## Owner absence, 20 November to about 4 December 2026

The owner is away and nobody can touch the bench. These rules follow MotorControl-RS decision D15
(`docs/prompts/ess_wired_bench/COMMON.md` there) and override any text that asks for the owner, until the owner's return
is recorded here. A drive that becomes unreachable or needs a power cycle stays lost for the rest of the absence.

**Schedule (2026-11-23).** The owner is away from 2026-11-24 until about 2026-12-05. Prompts run unattended through a
Claude Code loop (`/loop 1h` in this repository's session; its instruction, pacer state and handoff live outside the
repository, in `C:\agent_loop\MotorControl-CAN`): at least one prompt per day, more when usage allows, in the order
of [SEQUENCE.json](SEQUENCE.json) (ascending id among ready work), starting with prompt 57. Agents run on Opus at
xhigh, with Fable at high for the most complex tasks. These rules apply to every such run.

**Waiver.** On 2026-11-19 the owner chose this statement for the whole period: "Yes, unattended motion: Enables, moves, velocity runs and stops within the library's limits, with nobody at the bench; I accept the risk." Pass it verbatim with
`s2_bench.py --unattended-waiver "<statement>" --waiver-covers-motion`. It is also the written waiver for unattended
nights and soaks in this period (prompt 66). Observations only a person can make (shaft feel, direction against a mark,
LEDs) are recorded as "not observed: owner away"; an enable uses the released start (`--released`; prompt 57
removed `--released-by-recorded-stop`), never a typed power-up shaft observation.

1. **No waiting.** Ask the owner nothing and wait for nothing. A step that needs the owner (power cycle, cable pull,
   wiring, switch, DIP switch, instrument, observation) goes into the
   [deferred table](README.md#deferred-to-owner-return) with the exact action and the cases it unlocks; those cases are
   NOT RUN with the reason "deferred: owner away". Continue with the rest; the prompt stays Partial.
2. **Nothing persistent, no communication changes.** These object numbers are the CL86-C fixture's (other families
   have no fixture during the absence; their promptsets list their own persistent objects). No `2300:00` write in any value (1 factory restore, 2 save); CL86-C
   has no `1010`/`1011`. No `2203` node ID write (bitrate is DIP-only). No COB-ID write (`1005`, `1014`, `1200:01/02`,
   `1400`-`1403:01`, `1800`-`1803:01`), no PDO map or inhibit write (`1600`-`1603`, `1A00`-`1A03`, `1400`-`1403:02/05`,
   `1800:03`), no RPDO frame on `0x200+node`, no NMT Reset Node or Reset Communication, no NMT Stop to a moving drive. No
   `250A`, `2500=0`, `2501`-`2520`, `605D`, `2202=1`, `60FE`, output functions `2400:0B`-`0F`, input filters
   `2401`-`2407`, and no write to DI0..DI2 or of input codes 4 to 6 (prompt 77 rule 5). No mixed-protocol frames
   (prompt 63 item 5). No eFuse burn.
3. **Allowed in RAM, read back and restored in the same session:** `2201` below a 2.5 MHz command pulse rate
   (rpm x `2201` / 60), `2500` 1 and 2 (no open-loop enable), `6060`, `6081`, `6083`, `6084`, `60FF`, `607A`, DI3..DI6
   under prompt 77 rule 5, TPDO1 transmission type `1800:02` 1 and 255, the event timer `1800:05` 0 and 100, and the
   heartbeat period `1017` (volatile, measured 2026-11-16; end at 1000 ms, the value at the start of the absence).
4. **No state only a power cycle clears.**
   - S21 enable-fault state: never run above about 2.5 MHz command pulse rate (never 3000 rpm at `2201=51200`; FF05
     above about 2.6 MHz left every later enable faulting, [report](../reports/2026-11-13_enable_fault_state.md)).
   - S20 enabled-and-halted state: read `2100` before every HALT run; clear the `0014`/`0015` latch a moving quick
     stop leaves (mode 1 enable, then a stationary quick stop gives `0010`) before a HALT run.
   - No synchronous RPDO start (it can inhibit every later SYNC), no TPDO1 remap, no mode 6 enable (no reviewed
     quick-stop exit), no open-loop (`2500=1`) enable.
   - Reset or flash the MCU only with both axes released and still.
   If a drive still ends in such a state, stop motion on it, record it, leave it out of later cases and continue on the
   other drive.
5. **Snapshot discipline.** Before changing a setting, read it through its own route; restore it on the same node before
   selecting the other node, flashing or ending the session, and read the restore back.
6. **One lead at the bench.** One session alone holds COM15 and active CAN requests; workers do host work only.

**Fixture at the start of the absence (2026-11-19).** S2 on COM15 (ESP-Prog2 `94A990398B40`) with image `d89fddd4`; both
CL86-C drives powered, released and still, mode 1, NMT Pre-operational, TPDO1 type 255, heartbeat 1000 ms (RAM, until
the next power cycle). The PC: sleep and hibernate off, USB selective suspend off, VS Code and extension updates manual,
Windows Update paused until 2026-12-05.

## Read and prepare

1. Read root/applicable parent AGENTS, this contract, the active prompt and the reports for its listed prerequisites. Inspect the current branch, upstream, worktree and incoming changes. Create one new dedicated branch only when starting a new promptset execution; record its identity/base/upstream in PROGRESS and retain it throughout all prompts and resumed sessions. Maintenance of these prompts does not start implementation.
2. Prompt 1 establishes the source map and shared decisions. Thereafter read the active prompt's named sections of [CONVENTIONS.md](CONVENTIONS.md), [DOCUMENTATION.md](DOCUMENTATION.md), [MAINTAINABILITY.md](MAINTAINABILITY.md), [DELEGATION.md](DELEGATION.md) and its [MULTIDRIVE.md](MULTIDRIVE.md) row. Read relevant requirement/architecture/API-map sections and changed contracts, not every historical report. These contracts remain binding; a short reading route does not waive them. In a fresh session, reload the relevant contracts rather than relying on remembered identifiers.
3. For every prompt, refresh MotorControl-RS HEAD and dirty state and read the RS code for this seam; RS is authoritative. Check HEAD and dirty state only for siblings this prompt names, and inspect only the seam it states. Inspect current FieldCore-node read-only for ownership or integration changes. If HEAD, dirty state and seam match the last recorded row, write "unchanged since prompt N" and do not reread. A sibling row must name a behavior adopted or rejected, with path and symbol. This does not relax vendor evidence, safety, uncertainty or verification rules. Record the delta. A symbol search locates evidence; read enough implementation to establish meaning. Unchanged unrelated modules and suites need not be reread or rerun. If a source is unavailable, identify the limit and use the pinned evidence only for its covered seam.
4. For device facts, verify the applicable original PDF/EDS/hash and exact pages. CL86-C is a finished drive, not an identified IC. Keep conflicting firmware/model facts explicit. Check predecessor outputs actually exist and passed; do not assume a report means unfinished code is usable.
5. The [prompts 34-50 repair audit](PROMPTSET_AUDIT_34_50_2026-11-02.md) closed W1 to W8 in [packet 10](../reports/handoff_19_33/packet_10.md). It stays the historical source of its decisions (DEC-1 to DEC-8) and of its W10 and W11 order. The [current disposition](../audit/2026-11-15_auditor_disposition.md) checks each finding against later code and identifies corrections to historical summaries; follow current service contracts for complete correlation premises. Closure qualifies no release command, moving stop, relative wire base or physical scale, so a lifecycle that consumes such a fact keeps its evidence gate.

**Standing bench authorization.** On 2 November 2026 the user stated, for this whole promptset: physical tests are generally allowed; the bench is ready, with two drives in series, free shafts and motors fixed to the table; run everything the work needs; and run as many subagents as needed. So do not stop to ask before a physical test or motion a prompt or packet needs. Check the fixture by what software can see, such as port identity and fresh readback, and report what ran. Ask only for something only the operator can supply: a physical action, an observation software cannot make, or a changed fixture. This does not change the evidence rules: an observed result stays scoped to its specimen and firmware, and one lead alone controls the port and active CAN requests.

## Check facts and correct errors

Use the focused evidence sections named by the prompt. They are research seeds, not approved constants. Reopen the original manufacturer PDF page and EDS section for every affected address, subindex, type, limit, unit, bit, trigger, default and timing; inspect the rendered table when extraction is ambiguous. For public code, record exact repository/commit/path and distinguish protocol implementation from a claim about this drive. Prefer applicable official Lichuan evidence; neither a generic CiA402 library nor a similar model resolves a Lichuan contradiction.

Record each claim as documented, inferred, contradicted, unknown or measured, with model/firmware applicability. Separate factory metadata, live readback and values intentionally chosen by an application. No invented registers, masks, delays, retry intervals, boot/save times or watchdog behavior. A missing documented timing becomes an explicit caller policy or qualification question where appropriate, never a fabricated device guarantee. Do not write EDS defaults during initialization or write a monitoring object because a translated procedure says to "set" it.

When reality disagrees, correct the reviewed ledger first with the old claim, cited evidence and reason; regenerate and fix affected implementation, tests, Doxygen, guides and instructions in the same checked change. Preserve original downloaded bytes and the contradictory source claim. Do not tune tests or timeouts merely to hide a failure. Hardware measurements require the prompt-29 fixture/authorization rules and retained raw traffic; a local observation does not prove all firmware variants. Keep only dependent capability blocked and finish independent work. Include the factual corrections and remaining uncertainty in the prompt report.

## Implement the bounded result

Use the active prompt's objective and owned ledger rows as its scope. Keep code, meaningful tests, Doxygen, affected guides/examples and CHANGELOG together. Follow the concrete RS names and selected CAN conventions. Prefer direct C++11 functions, fixed values and caller-owned contexts; no framework/core I/O, hidden time, heap, retry or speculative generic engine.

Keep one application receive owner and one selected CANopen service backend. Preserve unsolicited events, exact units, raw native errors, terminal-result retention and uncertainty. Preparation/validation publish atomically. A local token cannot correlate an indistinguishable late SDO reply. Queue admission/CAN TX, protocol acknowledgement and motion completion remain different facts. No startup/probe/recovery action silently enables, moves, clears faults, changes settings or replays uncertain work.

Common motion supports stepper and servo intent without forcing CANopen addressing/state, full-step geometry or fixed RPM widths into every profile. N5/EPOS4 and proprietary-frame contrasts remain small test fixtures, not advertised drivers. Preserve original sources/licenses and exclude reference material from core distribution.

Subagents are authorized under DELEGATION. Give disjoint files or read-only reviews and a complete bounded brief; choose only available models/efforts appropriate to complexity. Keep shared contracts and Git under the lead. Strong independent review is useful for arithmetic, state, deadlines, uncertainty and stop behavior. Solo execution remains valid when delegation is unavailable; the lead still reviews the whole result.

## Verify, finish and sync

Run the active prompt's focused acceptance checks and affected existing build/docs/CI gates. Test observable behavior, including errors and unchanged output where required. Do not postpone necessary tests to a later integration prompt. Later audits strengthen already-correct components. Hardware and fake/compile results are separate evidence.

Before marking **Complete**, close every owned deliverable and required check. No TODO, success stub, unimplemented confirmed ledger row, failed required check or silently deferred acceptance item remains in that prompt. Later prompts own only their explicitly named later work. A newly discovered task too large for the current scope requires an explicit revised assignment and traceability; do not quietly move it to a catch-all final audit.

Missing firmware facts, tools, authorization or hardware can block particular work. Record **Blocked** or **Partial**, exact unfulfilled criteria and the next evidence needed; commit and sync verified chunks before moving on. Within the assigned range, continue the next numerically ready independent prompt, then return to earlier blocked work when its evidence arrives. A prerequisite is usable when the specific artifact consumed exists and its checks passed, regardless of that prompt's state. Start when the consumed artifacts are usable and name them in the report. If an artifact is missing, the dependent feature is Blocked. Complete means the prompt's owned scope and checks closed. Record predecessor gates in an Inherited gates column; they do not change the state. Never mark unfinished owned work Complete to unlock another prompt; a usable artifact does not make its producer Complete, and an inherited gate still blocks its dependent feature. No guessed values, success stubs or silent scope reductions qualify.

The owner/evidence path 23-29 deliberately does not require completed firmware-dependent remapping/settings in 18/20-22. Keep those obligations pending while building the real owner, adapter and prompt-29 harness; resume their typed implementation and owner integration when evidence arrives. Prompt 29 permits isolated, reviewed, explicitly authorized experiments without completed motion APIs or the later CLI. Prompts 31-36 get missing device facts through a prompt-29 conditional experiment or manufacturer clarification, not by waiting for prompt 47, which qualifies a finished feature rather than discovering how the drive works. Prompts 47-48 remain pending/blocked without physical evidence; 49-50 close as a scoped software candidate listing all inherited gates. Prompt 18 has no dependents, so its remaining Q17 gates block only their own feature. This is not full-library qualification.

Write `docs/reports/prompt_N.md`: objective/owned scope; source revisions and changes; implemented artifacts and decisions; exact checks/results; documentation; blockers; next work; branch/commits/sync. Update [PROGRESS.md](PROGRESS.md). Update backlog/roadmap/coverage when their facts change; link the authoritative contract rather than copying the report everywhere.

**Commit and sync after every prompt**, and during it when a coherent tested chunk is ready. Keep the same execution branch. The lead stages only owned changes, reviews the diff, commits, pushes and verifies fresh upstream ahead/behind state. Preserve concurrent work; no force-push, empty no-op commits or base-branch merge without authorization. A failed sync stays an explicit blocker. Frequent pushes do not require manually dispatching full CI for small changes; use the root/parent impact-based policy.

When assigned one prompt, finish its handoff and stop. Continue through later prompts without routine confirmation only when the user assigned the whole sequence or a range.
