# Fresh independent audit of prompts 50 and 51

Date: 2026-11-16. Starting commit: `b6d230a73d61195b12a2424d6ff1e844972ea4be`.
The initial `main` worktree was clean and a fresh fetch showed no divergence from
`origin/main`. This audit stays on main under the version0 instruction.

## Review and findings

The lead reread both full original prompts and compared their attachments to
the maintained instructions. Three parallel reviewers separately inspected
core/application timing, owner/adapter retirement, and scope/package/source
evidence. They then cross-reviewed the two runtime fixes. The lead inspected
actual declarations, implementations and diffs and independently ran the
pre-fix reproducers. Reviews used the available inherited runtime; the scope
reviewer could not independently identify its exact model/effort. No worker
changed Git, siblings or hardware. Lead-only physical work is recorded below.

| Confirmed finding | Consequence and correction | Regression |
| --- | --- | --- |
| Passive SYNC observation short-circuited native receive dispatch | An accepted custom heartbeat at `0x80` could hide boot-up and let an old queued SDO transmit. Always run native dispatch once, while retaining the conservative SYNC conflict. Local production remains refused for colliding routes. | `sync_owner_test`: boot-up advances node generation, invalidates the reservation and sends zero frames; a synthetic one-byte PDO still publishes; foreign SYNC disarms while ordinary heartbeat/SDO continues. |
| `ProbeSync::cancel` accepted time older than the owner | The helper returned success/CANCELLED while the owner rejected cancellation, leaving queued SYNC eligible to transmit. Check owner and schedule time before any mutation. | `probe_sync_console_test::helperTimeBoundaries`: stale cancel rejects unchanged; timely cancellation retires the actual queued request and produces zero TX. |
| `ProbeSync::service` consumed a future owner terminal | A helper call at time 1 consumed the owner's time-2 terminal and published success in the past. Apply the same monotonic guard before reading/consuming the terminal. | The same boundary test preserves the pending context at time 1 and consumes success exactly once at time 2. |
| P50 called a dirty RS checkout clean | The retained before/after arrays each list 14 modified/untracked paths. Correct the report and disposition; retain original arrays and inspected-byte hashes. | Fresh source comparison and the unchanged historical source records. |
| P51 inferred producer incapability from missing EDS objects | Absence of `1006/1007/1019` does not prove a drive cannot produce SYNC. State unestablished capability and retain this milestone's consumer-only policy/no bit30 write. | Original EDS sections and manuals; no runtime capability broadened. |
| `unmatchedFrames` documentation omitted passive SYNC acceptance | Align the header and owner guide with the actual counter: neither native dispatch nor passive SYNC accepted the frame. | Existing foreign-SYNC counter checks and the added ordinary-traffic regression. |

The old native reproducers showed `generation=11, sequence_live=1, sent=1`
after a custom-route boot-up; `cancelled=1` followed by `tx_after=1`; and
`helper_time=1, owner_time=2` with success. The repaired tests exercise those
observable contracts rather than mirroring the new conditions. No second
receiver, scheduler, service engine, hidden retry or production fallback was added.

## Sources, coverage and retained evidence

[Fresh source record](audit_50_51_sources.json) identifies the current RS,
ADS1115, INA228 and FieldCore revisions, working-byte hashes and the specific
behavior adopted or rejected. Siblings were read-only. RS advanced, but the
named files retained their P50 hashes; ownership/result seams were reopened.
The lead independently checked raw `1005`, `1800:02`, absent producer/counter
objects and pinned `CO_SYNC`/`CO_RPDO_process` mechanics. Original manufacturer
PDF tables were independently sampled by the evidence reviewer.

All 49 historical report-inventory hashes match. Reviewed accounting remains
99 EDS indexes, 202 scalars, 53 manual obligations and 198 public overloads.
Only Lichuan C is installed; N5, EPOS4 and proprietary fixtures remain design
contrasts. The four missing standalone reads (`605D`, `2301`, `2302`, `607A`)
stay visible with prompts31–33; this audit does not relabel them implemented.

The reviewer reproduced the retained P51 analyses: each short type-1 trial
contains 20 local TX reports and 20 TPDO1 frames, with zero EMCY/tap drops.
All 223 non-manifest firmware input hashes match baseline source bytes or their
CRLF checkouts. The remaining `library.json` hash matches `63060047`; the later
change only exported the SYNC guide. These observations establish neither
physical same-cycle sampling nor buffered start/withdrawal semantics.
The retained one-node, 20-frame 10 ms capture predates prompt56 and is
exploratory evidence. It does not satisfy P51's gated 10 ms qualification criterion.

## Integrated verification

Focused suites and reciprocal code review passed. The first full native run
passed 149/150 tests; the retained-capture grammar check found the newly retained
truncated refusal capture below. Its exact negative fixture now must remain
incomplete, hash-identical and without a SYNC schedule/terminal. No production
parser rule or original byte is relaxed to call that failed attempt successful.
After that fixture registration, all 150 native/Python tests passed in the fresh
full verifier under `.build/audit5051-rechecked`. The evidence suite contains
142 tests. Independent package verification passed all 32 commands, including
documented PowerShell use, relocated install and isolated C++11 consumers.
Strict Doxygen 1.13.2 passed and extracted 38 headers, 4017 members and 156 free
functions. Ledger/generated freshness, operation coverage, repository/prompt
integrity and code/document diff checks passed. All 51 newly staged bench files
were compared byte-for-byte with their working originals; raw capture whitespace
was preserved rather than normalized.

The fresh S2 `-Os` stack check passed: largest frame 2000 bytes, known chain
5952 bytes, against 2048/6144 limits. This remains a lower bound excluding
unknown/indirect/SDK paths. Exact final-commit CI and synchronization are checked
after commit; their revision and run URL belong in the final handoff.

## Fresh physical regression

Lead-only COM15 operation used the recorded ESP-Prog2 serial `94A990398B40`,
OPTAK S2/TJA1051 fixture and two CL86-C/LC60H2127 nodes at 1 Mbit/s. No new
electrical or shaft observation is claimed. Source facts establish the object
widths; type255/no-PDO-after-one-SYNC was a previous specimen observation, not
a general firmware guarantee. The regression issued no motion, NMT, drive
configuration, save or reset command.

- The [first build](../bench/20261116-audit5051-flash/build.json) refused flashing
  because a header comment changed during compilation. The stable-input
  [final flash](../bench/20261116-audit5051-flash-final/build.json) passed and
  [boot identity](../bench/20261116-audit5051-boot/rx.txt) matched ELF
  `c5c579929c7d16b8a58f013338ff58669113fb6ed1a5e6658769db346efdd0ff`.
  Startup was passive: CAN off, silent pin high and no trial active.
- The [read session](../bench/20261116-audit5051-reads/session.json) contains
  26 raw uploads and 95 typed communication uploads per node, all successful.
  Both nodes reported `1005=80`, heartbeat1000ms and all eight PDO types255.
  Raw `6041=C050` and `2100=0015` matched prior records. The multi-record
  `s2_pdo.py analyze` reports no unknown/malformed lines; the single-record
  `s2_evidence.py` is not the validator for this combined session.
- The [first SYNC attempt](../bench/20261116-audit5051-sync/rx.txt) was refused
  before admission because the lead had omitted the bench console's explicit
  local `o` setup. Its helper ended with a partial background JSON trace; keep
  the capture incomplete. After [local setup](../bench/20261116-audit5051-console-setup.txt),
  a separately requested [single SYNC](../bench/20261116-audit5051-sync-final/rx.txt)
  returned `LOCAL_TX`, count1, with drive effect still unknown. The eight-second
  watch recorded sixteen heartbeats, no TPDO/EMCY and zero tap drops/unprinted
  frames. Result release and explicit disarm succeeded.
- [Final local state](../bench/20261116-audit5051-final-state.txt) is CAN running,
  not inhibited, SYNC disarmed with no retained/active schedule or conflict.
  Controller errors, RX drops/loss and rejected TX are zero. Maximum observed
  owner gap is 4003us and runtime stack free is 7096/12288 bytes. Diagnostic
  retention is bounded: trace overwrite872, console rejection4858 and two
  unmatched frames are retained facts, not a lossless-interface claim. The
  dedicated watch above had its own zero-loss accounting.

Custom-route boot-up and stale helper-time failures were reproduced with native
fakes, not by reconfiguring the real drive or injecting foreign traffic. The
short physical regression does not qualify same-cycle sampling or motion.

## Scope and handoff

P50 remains a completed **scoped software review**, not whole-library completion.
P51 remains **Partial**: Q27 qualified sample association, Q28 remote buffering
and withdrawal, M09 reviewed RPDO mapping, and the sustained two-axis 10 ms
campaign remain open. Prompt56 throughput is a prerequisite for that campaign.
Host counters cannot replace missing wire markers or remote retirement bounds.

Use the [README](../../README.md) for installed native consumption,
[SYNC guide](../sync.md) for finite producer/owner semantics and
[S2 bench guide](../../examples/s2_bench/README.md) for explicit firmware routes.
The next P51 work is the reviewed no-motion RPDO map/buffering experiment and
sample-provenance evidence, followed by the throughput-qualified campaign.
No release tag, publication, sibling change or new motion capability is claimed.
