# Prompt 52: changes during motion

Current status: **Partial**. The [independent 52/54 audit](audit_52_54.md)
corrects the earlier Complete verdict: initial Q26 ramp-change meaning remains
unmeasured. Bare ramp writes are not an edge-applied active-ramp experiment.
The audit also fixes a queued change refusal that could suppress the run's stop.
The dated implementation and hardware results below remain historical evidence.

Scope: bounded typed operations, console routes and the initial physical
experiments required here. Ordinary Q22-Q26 capabilities remain closed unless
the caller supplies an applicable reviewed record. Finished-operation
qualification belongs to prompt 54. The final verification and fixture state
are recorded below; initial observations do not establish family-wide support.

The [source review](prompt_52_sources.md) records original documents, sibling
revisions and selected contracts. Source-gate checkpoint `6775a7ff` was committed
and synchronized on main before implementation. No sibling was changed.
The final refresh still found RS `e56d4c0a` and FieldCore `c40ea834` clean.
Three parallel workers used the available inherited model/effort for bounded
source/measurement review, harness/tests and independent ownership review.
The lead integrated shared files and alone controlled Git and the physical port.

## Implemented scope

- Existing VelocityContext now owns one checked positive-native60FF change.
  Intent, prepared rate/error, service evidence, first failure and post-change
  observations are retained. Preparation-time stopDue and final deadline do not
  move. Wrong sign, zero, unresolved writes, stops, stale generation and
  insufficient remaining budget reject before admission.
- Position replacement keeps distinct original/replacement contexts and IDs,
  per-field stored qualification, exact target/error and one outstanding
  set-point. Bare, falling-edge, immediate and buffered forms have separate
  evidence gates. Typed target replacement currently extends the original
  absolute endpoint in the same direction; shorter-ahead targets remain an
  explicit bench experiment. No inferred target FIFO or genericbit12 handshake.
- AxisSession owns handoff and preserves original terminal evidence, stops and
  uncertainty. Stop capacity is unchanged. Original and replacement console
  results have separate retained slots and explicit release ordering.
- Console velocity-change, move-speed, move-ramps and absolute-retarget routes
  use the same direct APIs. Python readers validate exact request identity,
  approximate versus exact error evidence and retained move relations.
- Protocol14 adds disabled mode-display probes and finite p/v experiments.
  One owner, checked codecs, explicit trial/campaign and finite stop budgets;
  no hidden replay, enable, setting or traffic filter.

## Physical evidence

Fixture: OPTAK_FGI_2025 ESP32-S2, TJA1051TK/3/1J, TX17/RX16/S18,
ESP-Prog2 COM15 serial94A990398B40, two CL86-C/LC60H2127 nodes1/2 at1Mbit/s.
Standing unattended authorization was retained in each motion request; shafts
were not independently watched. Raw identity on both specimens remains
1000=00040192,1009=0C,100A=20,1018:01/02/03=1000/1/0,
2000=0400,2001=0200. These are raw identifiers, not a decoded firmware version.

| Image | Build/flash record | ELF SHA256 |
| --- | --- | --- |
| 01, first mode/velocity trials | [01-flash](../bench/20261116-p52/01-flash/build.json) | `6c5f19ac67dc806831d4106b7db8ee1e6d889c27a6e929491843395235b55d60` |
| 02, successful velocity change | [15-flash](../bench/20261116-p52/15-flash/build.json) | `92d5f9c3e18d30af1c6d96d3cdf65c0ae895d25584ecc03e9f71179245bf1fc0` |
| 03, position experiments | [32-flash](../bench/20261116-p52/32-flash/build.json) | `7e64e3d4f8177d96591830a3c83694b8128e3a123d6c9b7106df724ce0d51e8c` |
| 04, final diagnostic fixes and read/PDO regression | [113-final-flash](../bench/20261116-p52/113-final-flash/build.json) | `9a96eecef07ecfe01166dee0d08995a68aede90daf2a973882398101a206187b` |

Each image was checked against a fresh ping. The build records pin dirty-source
hashes and base `6775a7ff`; none is falsely attributed to clean HEAD. Subsequent
host projection/printer fixes do not retroactively change these image identities.

| Trial/record | Observed result | Limit |
| --- | --- | --- |
| Node1 mode trials52001-52006 | Each6060 value2/4/7/8/9/10 acknowledged and appeared in6061;606C stayed0,6064 stayed16846; no fault reported | Acceptance/display only; no enable in those modes |
| Node1 mode52007 | Explicit6060=3 restoration acknowledged/displayed | No inferred mode capability |
| Node1 velocity52010 |30rpm run and original HALT completed; change refused before any changed60FF TX | Harness attempted before a fresh RUNNING cycle; capture also overflowed |
| Node1 velocity52011 | Refused before motion because HALT left the drive enabled | Correct prerequisite refusal; no automatic release or enable |
| Node1 stop52012 | Explicit DEC-2 quick-stop transmitted once; execution remains unknown | No prior retained6040 predecessor; route deliberately quarantined, no reopening from the matching reply |
| Node2 velocity52013 |30-to60rpm change acknowledged, later606C=60; original timed quick-stop completed with fresh controller stop evidence | One positive specimen response, no physical speed/ramp or family-wide qualification |
| Node2 position cases 1-4/8 | Bare speed, ramp and target writes acknowledged without an observed change to the active curve; clearing bit4 cleared bit12 while the trajectory continued | Sparse feedback cannot rule out a short transient or establish next-curve effects |
| Node2 immediate speed, cases 5 | Short same-endpoint trial and longer trial52034 retained the fresh bit12 handshake; the longer trial reported56/60/60/58/60rpm and a moving quick stop from003F | Controller observations only; no independent shaft measurement |
| Node2 immediate target, case 6 | New endpoint ahead was used; trajectory crossed the old endpoint without the buffered case's sampled slowdown | One same-direction positive extension; no reversal or shorter-target qualification |
| Node2 buffered target, case 7 | Deceleration near the old endpoint then acceleration toward the new endpoint; explicit stop interrupted the second curve | No queue-depth, full-buffer or final new-target completion claim |

The [independent measurement report](prompt_52_measurements.md) owns the exact
download bytes, operation/step identities, processing windows and per-cycle
`6041` bits 10/12, `2100`, `6064` and `606C`. All eight position cases have a
recorded disposition. Bare changes that were ignored remain closed as active
trajectory capabilities. In velocity mode, observed `C437` has bit10 set while
`606C` reports motion: that bit is not a generic standstill indication.

Mode captures are folders06 through12 under
[the retained bench directory](../bench/20261116-p52/).
[Failed52010](../bench/20261116-p52/14-velocity-change-node1/rx.txt),
[refused52011](../bench/20261116-p52/21-velocity-change-node1/rx.txt),
[uncertain stop52012](../bench/20261116-p52/23-quick-stop/rx.txt) and
[successful52013](../bench/20261116-p52/31-velocity-change-node2/rx.txt)
retain exact requests, raw traffic and failed evidence.

The52010 copied record filled448 entries:264 were TPDO1 ID181;912 later
copies were dropped. No CAN TEC/REC fault was reported. This is application
capture pressure, not evidence of CAN wire loss. Explicit reviewed TPDO1
transmission-type1 settings on [node1](../bench/20261116-p52/20-type1-setting/rx.txt)
and [node2](../bench/20261116-p52/30-node2-type1-setting/rx.txt), with hostSYNC
disarmed, removed that unsolicited traffic for subsequent SDO measurements.
No map, event timer or persistence command was changed.

Trial52013 retained96 terminal results and322 traffic records, zero trial
loss/drops, TEC/REC0, unchanged free heap19692 bytes, largest block7680,
stack watermark7828 bytes and maximum service gap5999us. Its stopDue was
329301009us, stop requested329302006us and final deadline337301009us;
settlement finished334420010us. One late tail-ring overwrite occurred while
the held record was printed; it is outside the completed trial and remains
reported, not erased. Sequential SDO samples are not simultaneous measurements.

Node1 remains quarantined pending an external power cycle. The requested
[90-second passive watch](../bench/20261116-p52/27-boot-watch/rx.txt) saw180
normal heartbeats and no boot-up. An MCU restart or elapsed silence does not
establish a fresh remote session. Its type1 setting remains explicitly recorded
until a verified restoration or power-cycle readback.

The final image was checked against [114-final-ping](../bench/20261116-p52/114-final-ping/rx.txt).
Node2's explicit [restoration](../bench/20261116-p52/121-restore-node2/rx.txt)
completed all 19 transfers: baseline type1, one acknowledged `1800:02=255`,
then exact readback. NMT Operational was explicitly requested afterward; no
save or map change was made. [Final trial52036](../bench/20261116-p52/123-final-readiness/rx.txt)
retains 22 successful uploads, one END marker, `6041=4050`, `2100=0014`,
mode1, two `6064=35981` / `606C=0` reports and `1001=0` / `603F=0`.
These are controller reports, not independent physical standstill. The earlier
fixed-ID raw-read route refused after later experiments consumed its ID space
([122](../bench/20261116-p52/122-final-state/rx.txt)); it sent no work. The final
read used the ordinary increasing-ID typed readiness route instead.

Final counters report zero dropped RX, CAN errors, TX rejection and RX-loss
notifications, stackFree8468 bytes, max owner gap5022us. Global traceOver131 and
console queue refusals3180 are retained: the former includes periods without an
active acquisition and the latter is bounded output backpressure, not erased
as a successful-trial statistic. The finite records name their own loss counts.

The board remains on image04, CAN active and host SYNC disarmed, with trial52036
retained and no active operation. Node1 remains quarantined with temporary
TPDO1 type1; it was never accessed through SDO after the uncertain stop52012.

## Corrections found during implementation

1. The v harness confused a final enable observation with a fresh RUNNING poll.
   A long-poll/enable-window regression reproduced the premature refusal.
   It now waits for a new complete, fresh cycle before its single attempt.
2. Retained original moves were incorrectly validated as replacements after
   handoff. Role-specific validation now permits the original terminal query
   and rejects malformed, swapped or unrelated relation IDs.
3. The extended help JSON exceeded its512-byte buffer. Factored grammar and a
   maximum-request-ID regression preserve every route. The inventory scanner
   now balances braces outside quoted strings instead of ending on help text.
4. Approximate velocity-change conversion no longer serializes a false exact
   zero error; it emits exact/error or approximate/error_bound consistently.
   Positive-native changes also respect negative engineering polarity.
5. Stack measurement found common dispatch frames inflated by change-only
   locals. Focused direct helpers reduce those frames; final measurement below.
6. Parameter-only experiments need a narrowly scoped preservation of the last
   acknowledged controlword. An exact accepted transfer ticket now preserves
   it only after a matching successful acknowledgement, current operation and
   epochs, with no stop supersession. Generic raw admission stays conservative.
7. A failed replacement falling-edge write retained its native abort in the
   context but omitted it from console results. Both immediate and buffered
   regressions now retain `06090030` through terminal and cached results, while
   preserving the original result and uncertainty and sending no trigger/retry.
8. The bench dump loop continued after queuing END. RX could grow the tail
   before its next turn, moving END to the next cursor and producing a second
   marker. Formatting now returns the last-line fact, and the loop stops only
   after that whole line queues. The regression reproduces tail growth and
   checks the retained completion flag and output-capacity failure.
9. PDO restoration used stale operation/grant counters after motion routes.
   The first failed attempt stopped at authorization before any SDO or write.
   All addressed PDO commands now refresh their local counters from the same
   service high-water marks, preserving overflow guards and channel isolation.
   The native test proves stale-grant refusal without TX and subsequent explicit
   19-step restoration. The final image repeats the real interleaving with
   readiness reads followed by restoration, then more readiness reads.

The initial ramp-up capture ended before its dump finished. The same retained
trial was dumped again without CAN traffic; the original prefix plus cached
body validates, with hashes/offsets in
[ramp-up-cached-analysis.json](../bench/20261116-p52/ramp-up-cached-analysis.json).
Both originals remain unchanged. Enable52033 separately exposed duplicate END
markers; its original capture remains inconsistent and its cached same-trial
record is cross-referenced in
[enable-cached-analysis.json](../bench/20261116-p52/enable-cached-analysis.json).
Neither issue caused a repeated motor command. Post-terminal tail loss is
reported independently of the complete retained operation prefix.

## Verification

The final local full GCC verifier passed all159 CTests, isolated package
consumers and strict Doxygen after the diagnostic fixes. Its retained
[summary](../bench/20261116-p52/verification/local-full.json), test/package/docs
logs and [stack report](../bench/20261116-p52/verification/stack.json) identify
the tools and commands. Focused tests also passed after the final corrections. The strict S2/S3 stack check passed at both `-Os`
and `-Og`, with unchanged 2048-byte frame and 6144-byte chain limits and no
allowance file. Known chains are lower bounds, excluding unmeasured SDK and
indirect frames; runtime high-water values are retained separately.

The operation inventory accounts for 203 public overloads, 202 EDS rows and 53
manual rows. Ledger/generator, prompt integrity, repository, package, diff and
original-byte checks are required alongside the focused change suites. The
final push runs the five required workflow jobs on its exact commit; the final
handoff names that commit and result. An earlier green checkpoint is not its
verification.

Source/documentation staged diff checks pass with CRLF recognized as an end of
line. Preserved compiler/Doxygen stdout has nine blank-at-EOF notices; those
acquired bytes were retained rather than rewritten for whitespace cosmetics.
All482 retained bench files match their manifest in both the working tree and
staged Git blobs. The promptset INDEX check also passes for the original vendor
and baseline artifacts.

## Remaining qualification and next work

The initial physical measurements are distinct from prompt 54's finished-route
qualification and repeatability. Sparse sequential SDO samples cannot establish
physical acceleration, a universal ramp formula, acceptance latency, queue
capacity, independent standstill or displacement. Per-field gates retain those
limits. Other models/firmware remain unqualified. Prompt 59 owns initial reverse
and range behavior; F15 mid-run sign changes, reversal and zero stay refused.

Node1 requires an externally established fresh server session before further
SDO work or restoration of its temporary TPDO1 type. Host reset and normal
heartbeat are insufficient. This operator-dependent obligation is separate from
the completed node2 experiments and remains in the current bench handoff.

Work stays on `main`; the source checkpoint was synchronized before the
implementation. The final commit contains the code, tests, documentation,
generated updates and retained successful/failed measurements together. No
release tag, package publication, sibling edit or other drive profile is implied.
