# Prompt 54: extension qualification

This campaign starts at `947cc5da8cbb0c24799d8114d8b665b38341921f` on `main`.
Hardware qualification remains **Partial**. Results below distinguish the
implemented diagnostic tools, native tests and actual specimen observations.
The [campaign directory](../bench/20261116-p54/README.md) owns the fixture,
finite plans and raw records. No host test establishes physical standstill.

## Sources and initial boundary

MotorControl-RS is `e56d4c0af6a7d678da11bb672a1b94126812b3a1`, clean.
Read `scripts/bench_session.py::{snapshot,cached_failure,run_recorded}`,
`scripts/bench_motion.py::{profile_snapshot,phase_plan,_run_phase}` and
`scripts/bench_probe.py::{observe_stopped,cleanup_stop_attempts,move_campaign}`.
Adopt bounded scenario artifacts and separate primary/cleanup outcomes.
Reject recovery followed by another stop attempt as an automatic CAN policy:
an uncertain write is never replayed by this campaign.

FieldCore-node is `c40ea8341edf8d665460b902cafefbe857a7f73f`, clean.
The ownership/integration seam is unchanged since prompt 53; it was not reread.
Neither sibling was modified. EN86/ZH86 original network and motion pages and
the retained EDS were reopened; no generic CiA402 behavior fills a firmware gap.

The initial image is the retained prompt 52 image. Both drives emitted regular
Operational heartbeats during a 600-second passive watch, with no boot-up.
Node 1's unconfirmed stop 52012 remains unresolved. An MCU reset cannot clear it.
No SDO, motion, RPDO or reconfiguration is admitted to that node in this state.
Independent global SYNC and raw reception do not reuse its SDO channel.

Node 1's last verified TPDO1 setting is the prompt 52 `20-type1-setting` record:
type 1, COB-ID 0x181, map `6041:16,6061:8,6064:32`. This is historical configuration,
not the EDS default map or a fresh qualification after an external reset.
Node 2 was explicitly restored to type 255 in prompt 52. Q27 same-cycle sample
provenance, M09 controlword-only mapping and Q28 remote buffered-trigger
withdrawal remain open.

## Diagnostic implementation

- `ProbeSync::timing()` and cached `sync timing` count consumed owner terminals,
  confirmed local transmissions, application-owner handoffs, intervals and delays.
  Results survive release/refusal until the next successful schedule preparation.
  These are not wire transmission times or mechanical skew.
- `P<seconds>,c` selects bounded compact receive records, with independent full
  receive bounds, sequence numbers and CRC16. Legacy watch formatting remains
  available. The finite `scripts/sync_watch.py` runner retains raw UART bytes,
  controller snapshots, exact schedule/terminal identity and loss accounting.
  Per-cycle raw SYNC TX and independent wire bus-load measurements are unavailable;
  those limitations remain separate from complete raw RX capture.
- Explicit `Tn` / `velocity-nmt` uses the existing finite velocity operation and
  one owner. It admits addressed NMT Stop and then Pre-operational inside a
  natural polling gap, without moving the original stop time or deadline.
  The next native poll records status, monitor, position and velocity. The run's
  own stop retains its independent outcome. No NMT response implies motor stop.
- Optional `--allow-changes` on console arming supplies only fresh, fixture-bound
  experimental prerequisites. It never marks firmware behavior measured and
  ordinary rearming closes the previous permission. No exact position-feedback
  relation is invented to permit target extensions.
- The flash tool accepts the two reviewed esptool versions 4.12.0 and 4.13.0,
  retaining the actual version and refusing mismatched write-tool output.

## Remaining qualification

| Requirement | Boundary retained until measured |
| --- | --- |
| Two-node 30-minute 10 ms sampling | Run 38 failed after 42,988 of 180,000 slots. The 100 ms / 60 s baseline passed; sustained 10 ms remains unqualified. Raw counts do not close Q27 or prove wire or sampling jitter. |
| Synchronous set-point stream/start | Not opened by prompt 52; M09/Q28 and the measured handshake remain prerequisites. |
| Concurrent motion, independent/group stop, HM1-HM9 | Node 1 needs an externally established fresh server session and reacquisition. Host fault/loss/refusal tests are not physical injections. |
| Motion changes across speeds/ramps | Node 2's interrupted position fence and node 1's unresolved session block the next runs. Prepared finite plans and native tests are not physical qualification. |
| Shared RPDO stop and skew | M09/Q28 remain unresolved; no guessed sender. |
| Wired DI5/DI6 stop | No wired signal fixture is established. |
| Shaft/supply/independent CAN observation | Not available during this unattended campaign; user authorization does not create those measurements. |

## Verification and retained outcomes

The action-tested image is ELF
`2e0d33ea763bfe22dc9c1856660b388bfd74812f770e905ae48fb108228a4a84`,
base `947cc5da` plus the exact source hashes in
[`15-flash-id-fix/build.json`](../bench/20261116-p54/15-flash-id-fix/build.json).
The subsequent ping confirms that image, route protocol15 and PDO protocol7.
The output-drain correction is then flashed as ELF
`96d81d97f62471950bfa4589c2bb21cd8bb2eda0f97b150d8bef26ceb4b8c469`
with the exact source/image manifest in `34-flash-drain-fix` and matching ping35.
All 236 source hashes were compared: 232 match between the images, and only
`ProbePlatform.{h,cpp}`, `PdoWatch.h` and S2 `main.cpp` differ. All 236 current
source files and the actual ELF, BIN, bootloader, partition and sdkconfig files
match the `34-flash-drain-fix` manifest.
The motion fence prevents a new motion regression on this final image; its
console and SYNC checks are reported separately.

| Specimen check | Actual result and evidence |
| --- | --- |
| Node 2 identity and type 1 setup | `20-node2-type1`: 26 raw uploads and 19 typed transition services succeeded. Identity: device type 262546, hardware 12, software 32, vendor 4096, product 1, revision 0, model 1024, firmware 512. The one-byte version replies are raw specimen observations, not changes to the EDS dictionary. TPDO1 map is `6041:16,6061:8,6064:32`, type 1, event timer 0; explicit Operational follows. |
| Node 2 enable | `21-enable-node2`, trial 54003: 49 service terminals, acknowledged words 6/7/15 and new controller observations. No independent torque/shaft observation. |
| Node 2 position and its own quick stop | `23-small-move`, trial 54004: target 37580 from 35980, 6 rpm, ramps 500 ms. The explicitly planned interrupt leaves the original move uncertain/fenced. Its separate quick stop succeeds with three new observations; final reported position 36162, velocity 0, status 0x4050, monitor 0x0014. The stop does not turn the original move into a success. |
| Node 2 mode and later admission | `25-mode3` changes the released drive explicitly to mode 3. `27-velocity-regression`, trial 54006, is refused before motion writes because the interrupted position fence remains. This is expected current behavior, not a successful velocity test. |
| Runtime storage | Action-tested image, `17-startup`: free heap 105764 bytes before CAN, 105200 after CAN, 79620 after console; reserved evidence arena 38592 bytes. Minimum free task stack reported 5960 bytes during the position/stop run; maximum owner service gap 6004 us. The final image's `36-startup` repeats those heap/arena sizes; SYNC runs 37/38/40 report minimum free task stack 7624 bytes. These are observed paths, not worst-case stack or board capacity. |

The position fence is deliberate: `AxisSession::afterMove` retains a trigger
without confirmed move completion for the session object's lifetime.
`afterConfirmedEnd` clears velocity/home stop obligations but does not prove
retirement of the position target/curve. Existing interruption tests require
this distinction. Mode changes, close/open and a new image cannot be used as
workarounds. Further node 2 motion needs an independently justified trajectory
retirement boundary; node 1 separately needs its unresolved server-session
boundary. Consequently M14 and the motion-change/multi-axis campaign remain
unperformed here despite the new native-tested diagnostic route.

The first SDK build was deliberately rejected by the tool's source-stability
check when two headers changed during compilation; no image from that run was
flashed. The subsequent stable ESP-IDF 5.5.5 build passed. Native and strict stack
review findings are retained with their regressions; final commands and hardware
results are recorded below.

## Anomalies investigated

- The new wide SYNC formatter inflated `ProbeConsole::dispatchDevice` by 112
  bytes, making a known `-Og` call chain 6256 bytes against the 6144-byte limit. A narrow
  non-inlined formatter restores the ordinary chain; limits were not changed.
- NMT heartbeat acceptance could cross the fixed stage cutoff during a service
  gap. Both processing time and receive upper bound now must precede the cutoff;
  direct boundary-jump regressions retain failure for earlier-but-late-processed
  and actually late observations.
- Actual mixed NMT/SDO native transcripts exposed the offline parser assigning
  NMT TX reports to SDO windows. The parser now respects the transport kind and
  complete printed local identity. Raw exact duplicate records remain errors.
- A refused NMT experiment has no admitted stage times or native NMT work.
  The parser incorrectly required prepared bounds for this failed candidate.
  A real native Pre-operational refusal transcript now tests the exact empty
  shape; forged bounds, samples or work still fail validation.
- Live raw identity after typed readiness failed before TX with stale operation
  identity (`13-node2-type1`). The single-byte branch did not refresh the retained
  producer counters used by the PDO-line branch. The same helper now refreshes
  addressed commands, and raw grant ranges use the existing authoritative
  history. A real raw/PDO native regression preserves the zero-TX failure and
  verifies continuation without resetting state or replaying an uncertain write.
- ESP-IDF reported `v5.5.5-dirty`. The 42 flagged files exactly match index and HEAD,
  and all 27 recursive submodules match; cached index sizes correspond to CRLF
  checkout sizes while actual/HEAD bytes are LF. This metadata condition, not a
  TWAI/UART patch, caused the suffix. The full byte audit is retained in
  [sdk_provenance.json](../bench/20261116-p54/sdk_provenance.json).
- The first 100 ms SYNC capture (`29-sync-100ms`) exposed two host-collector
  errors: a normal stopping receipt was parsed as a numbered data record, and
  successful local transmission was rejected because remote drive effects stay
  unknown. Both have retained-transcript regressions. The capture is incomplete;
  the corrected explicit trial `32-sync-100ms-fixed` captures all 50 TPDOs per
  node, with complete final accounting and no reported loss.
- The first 10 ms capture (`33-sync-10ms-short`) confirms 500 local SYNC
  transmissions and counts 500 TPDOs per node, but drops 274 diagnostic records.
  The 32-record print FIFO fills while the 16-byte output budget drains about
  8 kB/s at the observed 2 ms turn; the compact stream needs about 9.1 kB/s.
  CAN receive/tap loss remains zero. This is a diagnostic throughput failure,
  not evidence of 274 missing CAN frames or a passing qualification run.
  The correction keeps the ordinary 16-byte drain and permits at most three
  further output-only 16-byte passes during a compact watch. Native tests
  reproduce the backlog using the actual platform queue and retain loss under
  a blocked sink. No buffer size, CAN service or SDK callback changed.
  The new image's short run 37 captures 500 TPDOs per node and all 1011 receive
  frames with zero printed/tap loss and unchanged zero CAN loss/error counters.

## Sustained SYNC result and next measurement

| Exact final-image run | Native transmissions / TPDOs per node | Captured receive frames | Result |
| --- | --- | --- | --- |
| `37-sync-10ms-drain-fixed`: nominal 5 s, 500 slots | 500 / 500 | 1011, no reported capture loss | Short capture passed |
| `38-sync-10ms-30min`: nominal 1800 s, 180000 slots requested | 42988 / 42988 | 86836, no reported capture loss | `MISSED_DEADLINE`; 137012 requested slots were not executed |
| `40-sync-100ms-60s`: nominal 60 s, 600 slots | 600 / 600 | 1320, no reported capture loss | 100 ms baseline passed for these 600 slots; no 30-minute claim |

The requested 30-minute 10 ms qualification **failed**. Run 38's actual
first-to-last handoff span is 429.869001 s and its raw capture window 430.292005 s.
All 42988 TPDOs from each node and 430 heartbeats per node were captured with
valid CRCs, continuous record numbering and complete final accounting. All 44
controller snapshots report zero dropped frames, errors, TX rejections and RX
loss, with running=true and inhibited=false. This is complete evidence of the
shortened failed run, not complete coverage of the requested 180000 slots.

Nominal duration counts slot windows: 180000 slots at 10 ms cover a nominal
1800 s schedule, but the first slot is due at the origin and the last at
origin + 1799.99 s. Actual first-to-last handoff spacing is reported separately.
Run 40's 600 local transmissions and 600 raw TPDOs per node passed at 100 ms,
with 1320 complete receive records and unchanged zero controller loss/error
counters. Its first-to-last handoff span is 59.899003 s, raw capture window
60.338013 s and host elapsed time 60.89 s. This is a bounded passing baseline,
not sustained 30-minute qualification at the slower period.

Slot 42987 expired at 460194676 us. The helper consumed its confirmed result at
460195400 us, 724 us late. The last handoff at 460187431 us plus the aggregate maximum
owner report delay of 5999 us bounds that owner completion at or before 460193430 us;
it is not an individually measured completion timestamp. Thus at least 1970 us
elapsed between owner completion and helper consumption. The core's documented
processing-time cutoff correctly retains `MISSED_DEADLINE` and the confirmed
TX evidence. A native regression covers consumption before, exactly at and
724 us after the cutoff. No deadline was extended and no catch-up frame sent.

The exact cost of the intervening output/evidence/watch/console phases or
preemption is unmeasured. There is no idle callback between owner and helper in
this assembly; the explicit task idle is later. Prompt 56's next concrete
measurement is last-slot owner completion and helper consumption plus bounded
phase timings, followed by its reviewed scheduling changes. Then repeat the
same 10 ms / 1800 s request. A later pass alone does not explain this failure.

Run 38 owner handoff gaps are 6998..12005 us; received upper-bound gaps are
6896..13441 us for node 1 and 6917..13434 us for node 2. Maximum owner service gap is
4002 us; minimum reported free task stack is 7624 bytes. These are software and
driver-bound measurements, not CAN wire jitter or same-cycle sample identity.
Per-cycle raw TX timestamps, measured wire bus occupancy, physical movement
and stop/start skew remain unavailable. Optional owner-trace overwrites and
output enqueue refusals remain counted separately from the loss-free compact
receive stream; they are not silently classified as CAN loss.

## Verification, handoff and remaining work

The first exact-commit CI run (`f974e4b1`) caught a late documentation mistake:
the exported SYNC guide linked a repository-only report with a relative path.
The existing package gate correctly rejected the missing packaged target after
the sanitizer tests passed. The guide now uses the repository URL, preserving
the package boundary. The full package/consumer gate passed after this
correction (`.build/p54-package-linkfix`), including the README workflow,
independent headers, relocated install and isolated native applications.

- `python scripts/verify.py --mode full --compiler gcc --build-dir .build/p54-final-drain`
  passed: 166 contract/native/Python CTest checks, generated-file checks,
  package/relocated consumers and strict Doxygen. The final focused SYNC timing
  regression was rebuilt and passed afterward in the same native build; it
  changes tests only.
- Strict S2 stack measurement at both `-Og` and `-Os` passed with no allow-file:
  maximum known chains 6144/5968 bytes under the 6144-byte limit. Unknown SDK,
  indirect and task-entry paths remain explicitly unmeasured by that tool.
- The actual pinned SDK build, verified flash and matching ping passed.
  Independent reviewers checked ownership/timing, raw identity continuity,
  evidence interpretation and the bounded diagnostic correction.
- Promptset, artifact hashes, staged/committed provenance and diff checks are
  required before synchronization. Required CI is checked on the final commit;
  its result is reported in the handoff, not inferred from an older green run.

Final cached inspection [`41-final-state.txt`](../bench/20261116-p54/41-final-state.txt)
shows the final image, no trial/watch/capture,
SYNC disarmed with no active or retained schedule, CAN running, zero controller
loss/error counters, free heap 74456 bytes and largest block 57344 bytes. Both drives'
last verified TPDO1 types remain 1; this cached inspection performs no new drive
read or configuration check. No save or hidden restoration ran. Neither original
motion restriction is cleared. The original motion ledger still accounts for
six of twelve motion weights despite the MCU's volatile counter resetting on flash.

Prompt 54 remains **Partial**. Resume motion only after the specific external
session/trajectory-retirement boundaries and fresh prerequisites are established.
M09/Q28 still gate synchronous RPDO starts/streams and shared stops; a wired DI
fixture is needed for DI5/6. Concurrent node 1/2 motion, group fault/loss injection,
M14 and physical skew/standstill have no new hardware qualification here.
Work stays on `main`; routine commit and upstream synchronization are authorized.
