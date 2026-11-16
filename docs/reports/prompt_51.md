# Prompt 51 host SYNC and synchronous PDO evidence

Status: **Partial**. The finite host producer, owner/adapter/console path and
conditional TPDO1 type setting are implemented and tested. A CL86-C specimen
responded to every SYNC in two short sampling trials. Same-cycle qualification
(Q27), buffered RPDO behavior/withdrawal (Q28), the M09 map and prompt 56
throughput remain distinct gates. No common motion start or sustained 10 ms
qualification is claimed.

The [source review](prompt_51_sources.md) records fresh RS/FieldCore revisions,
original EDS/manual and pinned implementation facts. RS has no SYNC counterpart;
CONVENTIONS records the CANopen-specific names. The [SYNC guide](../sync.md) owns
the implementation, timing and retirement contracts.

## Implemented and reviewed scope

- Pure DLC-zero codec and finite fixed-origin caller-owned schedule, supporting
  60,000 slots at 10 ms; checked arithmetic, exclusive cutoffs and no catch-up.
- Independent owner admission/result/adapter capacity, urgent precedence, final
  local TX evidence, explicit arm epochs, foreign-producer conflict and fault disarm.
- Passive SYNC observations preserve exact, bounded or unknown receive time.
- Shared ProbeSync/console in native, Arduino/IDF and S2 assemblies; passive startup.
- Existing typed `1005` and all eight PDO-type reads reused. Conditional TPDO1
  type 1/255 uses one U8 write plus exact readback and retains uncertain effects.
  S2 `J` and its protocol-6 runner use explicit isolated experiment scope.
- The prompt's former suggestion that an asynchronous `6040` could cancel a
  buffered synchronous RPDO was corrected. That write does not prove withdrawal.
  No RPDO-start path is enabled.

Independent reviewers traced queued TX, cancellation, time, result retention and
source premises. Corrections preserve an owner terminal that wins cancellation,
prevent an old schedule resuming across disarm/rearm, and end a schedule on a
between-slot bus-off/conflict. An unrelated owner failure invalidates the helper
instead of leaving it stuck. Late TX reports remain separate diagnostics.

Integration checks found missing test/package inventory entries and a Python
capability parser limited to prompt 50. The parser now checks the producer's
positive uint8 owner field, with positive and hostile-stream regressions; owner
metadata does not grant a capability.

## Physical fixture and procedure

The new image was flashed through ESP-Prog2 COM15, USB serial `94A990398B40`.
Its ELF SHA-256 is
`77b4c9ffc43737f08d5da378ccb82fb7400f771b8e23a65295028ab562af3106`.
The [boot capture](../bench/20261116-p51-new-ping/rx.txt) matches
[the build/flash record](../bench/20261116-p51-flash/build.json), including exact
input hashes. This was base `63060047` plus the retained source changes, not an
unmodified HEAD build. Startup had the controller closed and transceiver silent.

The existing OPTAK S2/TJA1051 fixture uses RX16/TX17/silent18 and two CL86-C drives
with LC60H2127 motors, nodes 1/2 at 1 Mbit/s. One lead owned the UART. No new
supply, electrical, shaft or independent CAN observation was available.
Fresh [communication records](../bench/20261116-p51-preflight/rx.txt) showed
`1005=80`, all eight PDO types 255, heartbeat zero and the retained default maps.
[Identity](../bench/20261116-p51-identity-state.txt) remains raw
`1009=0C`, `100A=20`, `2000=0400`, `2001=0200`, not a decoded vendor firmware version.
Fresh status/monitor/feedback reads retained `6041=C050`, mode 3, `2100=0015`,
`2101=2102=0`, `606C=0`, and `6064=16846/16845`. These are drive reports,
not independent standstill observations.

Explicit [network setup](../bench/20261116-p51-network-setup/plan.json) wrote
`1017=1000 ms` and requested Operational on both nodes. The subsequent type-1
experiment explicitly requested Pre-operational on node 1, changed only
`1800:02`, read back all nine configuration fields, and requested Operational.
No NMT transition was hidden inside a SYNC or type-setting API. No flash write
occurred during production. No enable, motion, RPDO, fault clear, save or reset
was issued in this stage.

## Trial outcomes

| Trial | Observed result | Evidence and limit |
| --- | --- | --- |
| (a), all PDO types 255, one SYNC | One `LOCAL_TX`; no observed EMCY or changed status/monitor values | [Raw console](../bench/20261116-p51-type255-once.txt), [post-state](../bench/20261116-p51-type255-post-state.txt). No claim about an unobserved internal effect. |
| Type 255 to 1 on node 1 | One U8 write acknowledged; all nine readbacks match | [19-service record](../bench/20261116-p51-type1-setting/rx.txt). This proves stored contents on this specimen, not general firmware support. |
| (b), 20 SYNCs at 100 ms | 20 local TX completions and 20 TPDO1 frames | [Raw capture](../bench/20261116-p51-type1-100ms/rx.txt). Node 1 `181`, DLC 7, payload `50c003ce410000`; zero captured EMCY. |
| (b), 20 SYNCs at 10 ms | 20 local TX completions and 20 TPDO1 frames | [Raw capture](../bench/20261116-p51-type1-10ms/rx.txt). Same payload; zero captured EMCY. A short one-node discovery run, not the required ten-minute/two-axis qualification. |
| Explicit restoration to 255 | One U8 write acknowledged; nine readbacks match | [Restoration](../bench/20261116-p51-restore/rx.txt), followed by [complete communication reads](../bench/20261116-p51-final-communication/rx.txt) of both nodes. |
| (a2), RPDO target buffering; (c), common start | Not run | M09 map/partial-effect contract and Q28 remote buffering/withdrawal evidence are absent. |

The [read-only analysis](../bench/20261116-p51-sync-analysis/analyze.py) reproduces
[counts and timing bounds](../bench/20261116-p51-sync-analysis/summary.json) from
hashed raw captures. Each eight-second passive watch recorded 36 frames: twenty
TPDO1 frames and eight heartbeats per node, with no tap loss, untimed frame or
unprinted frame. Node 2 remained type 255 and emitted no TPDO1 during these watches.
The owner correctly left the new TPDO traffic unqualified (`accepted=false` in
its trace); an experimental response does not bypass the map/provenance contract.

At 100 ms, host enqueue intervals were 99,000..101,000 us; at 10 ms they were
7,996..11,000 us. These are host processing measurements, not physical SYNC wire
jitter. Ordered RX bounds relative to the corresponding enqueue were
255..1343 us and 253..1295 us respectively. Pairing by order is an experiment
comparison, not proof of cycle identity or sampling latency. Receive bounds were
981..1088 us wide and 994..1040 us wide respectively. No independent bus timestamp
or observable cycle marker was available.

The copied watcher retained every RX frame. UART diagnostics had historical
ring overwrites before each debug session; the overwrite counters did not grow
during either trial. At debug-off the 10 ms run still had 31 pending console
trace records. Both runs retained all twenty enqueue and TX-report records, but
the console trace is not claimed to be a lossless general traffic logger.

The attempted `read state errors` after trial (a) was refused by the existing
Q20 read gate before TX. That refusal is retained; it was not bypassed. Supported
status/monitor reads and the passive EMCY capture supplied the stated observations.

## Final bench state and resources

[Final reads](../bench/20261116-p51-final-state.txt) confirm the host producer is
disarmed, inactive, without retained work or a foreign-producer conflict.
Both nodes have all eight PDO types 255 and unchanged maps. Both remain
Operational with heartbeat 1000 ms from the explicit setup; no save was requested
and persistence is unmeasured. Final status, mode, monitors and feedback match the
pre-trial values above. No claim of physical standstill is added.

Final controller counters report zero errors, dropped RX and TX rejection.
Maximum observed owner service gap was 4259 us. Runtime high-water evidence leaves
5604 of 12288 stack bytes free. The static S2 `-Os` check passed with no budget
breach: largest known chain 5952 bytes, largest frame 2000 bytes. It is a lower
bound with incomplete SDK/indirect-call coverage, not the full runtime stack use.

## Verification

- Combined native build passed. Full CTest executed 150 tests: 146 initially
  passed and four integration checks failed. After the fixes above, all four
  passed; initial failure logs remain retained under `.build/p51-full`.
- The updated hostile-stream Python suite passed 37 tests; the PDO runner suite
  passed 57. Focused SYNC, owner, TWAI, PDO configuration and console regressions passed.
- Isolated package consumers passed all 32 commands. Actual test registration
  passed. Strict Doxygen 1.13.2 passed (38 headers, 156 free functions).
- S2 firmware built, flashed and passed the fresh read/setting/SYNC/restoration
  checks above. Ledger, generated declarations, API coverage, prompt integrity
  and code/document diff checks passed on the final pre-commit tree. Raw bench
  logs retain original CRLF and trailing blank lines; their staged bytes were
  compared exactly rather than normalized to satisfy whitespace formatting.
- Required remote CI is checked on the final pushed commit; its result and link
  belong in the handoff. No older green run verifies this change.

## Remaining acceptance criteria and next work

Q27 still needs applicable sample-production/retirement bounds or an observable
marker before assigning a qualified SYNC identity to TPDO samples. The host's
slot number cannot disambiguate delayed same-ID frames. Keep the production
sample gate closed; the narrow type-1 observation is recorded in the ledger.

M09 needs a reviewed RPDO mapping procedure and retained partial-effect rules.
Then a separate no-motion `607A`-only RPDO2 experiment can measure buffering,
last-wins and `1400:05=1000` behavior. Q28 must establish remote pending-trigger
withdrawal before common starts or later periodic SYNC can be admitted after an
uncertain start. An asynchronous SDO stop alone does not establish withdrawal.

Prompt 56 remains the prerequisite for the sustained two-axis 10 ms campaign,
including actual wire timing, missing/duplicate/late samples, bus load and at
least ten minutes. The short trial here cannot close it. Earlier partial prompts
retain their owned gaps. Work stays on main; no release publication is implied.
