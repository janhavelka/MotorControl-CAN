Execute this packet fully. Apply [COMMON.md](COMMON.md).

# Packet 22 Finite two-node HIL regression and failure closure

Prerequisites: packet 20's absolute lifecycle and the preceding action/evidence packets have usable checked artifacts. Consume packet 21's relative lifecycle only for qualified relative cases; a blocked relative base does not prevent independent absolute/action HIL. Preserve that missing relative obligation explicitly. This packet validates the implemented scope on the recorded fixture; it does not claim complete prompts 47-48 electrical, family or sustained-load qualification.

The user authorizes physical tests and deliberate repeated tests after defects. Use the diagnostic tools from packet 11. The lead alone controls hardware. Keep independent cutoff and observation available throughout; local cancellation, NMT state and an acknowledged stop are not independent physical standstill.

## Read and prepare exact cases

Read original prompts 19-22 and 29-33, their reports, preceding packet reports, `docs/reports/2026-11-02_nmt_pdo_checkpoint.md`, `docs/reports/2026-11-02_pdo_motion_plan.md`, and `docs/hardware_bench.md`. Inspect actual move/action/read/settings, owner, adapter and HIL runner implementations. Verify W1-W8 closure.

Check RS position/stop interruption tests and runner outcomes without copying ESS frames or limits. Refresh the I2C validity/evidence and FieldCore request/result seams under COMMON. Use original Lichuan pages for every transmitted word.

Create a finite campaign record before transmitting. It identifies both physical drives/motors, firmware evidence, node/bitrate, controller/SDK/image, existing I/O assignments, current native targets, command/feedback scale and reference provenance, exact native targets/rates/ramp times, operation deadlines, maximum polls and independent stop/observation method. User-reported LC60H2127 and a paper shaft marker do not establish command increments per turn.

Set a hard campaign budget: at most twelve motion admissions total, at most three trials of any case, and at most sixty seconds per case, always respecting tighter operation and physical limits. Those are host campaign caps, not drive timing guarantees. Record smaller bounds when appropriate. No unbounded sweep, automatic endurance run or adaptive increase of travel/speed after a failure.

The offline absolute native16,1 rpm,1000 ms preview does not establish physical travel. Recorded `607A=5000`, `60FF=120`, mode 0 and feedback 0 are historical, not the current starting state. Use preceding packets' qualified bounds. Reject before TX when state/reference/scale or required stop capability does not match. Do not zero a target, reassign an input or infer a released state to satisfy admission.

## Execute and retain

1. Build and run host gates first. Check `python scripts/verify.py --mode full --compiler gcc --build-dir .build/h22`, then the affected pinned firmware targets through `scripts/check_firmware.py`. Upload through packet 11's tracked helper and generated flash arguments. Record exact source/image hashes and verify the running image. Existing `scripts/s2_bench.py ... --command ping` is inspection only; use the newly implemented typed procedure for motion, never a raw sender or an invented existing command.
2. Begin with cached inspection and reviewed baseline reads. Retain pre-test state, input functions/levels and error history. Confirm actual power-up/holding behavior independently. Keep fault-code/history reads explicit according to their accepted effect policy. No implicit clear, enable, mode change, NMT start or reset follows a baseline.
3. Run one absolute case on node 1 under the separately established explicit enable/mode/stop prerequisites. Record setup acknowledgements, exact trigger history, fresh acceptance, controller completion, final coordinate evidence and independent shaft direction/movement observations. Record mismatches instead of converting controller completion into physical success.
4. When packet 21 and its physical basis gate are ready, run one qualified relative case on node 1 with its explicit actual/commanded basis. Otherwise retain this case as pending and continue independent cases. Do not use queued-base semantics unless separately evidenced. Preserve the difference between intended endpoint, effective quantized target, native displacement and measured movement. A second same-value request is a new edge only when reviewed terminal/rearm state permits it.
5. Exercise the independently qualified stop path during a bounded slow move. Verify the interrupted move retains its own identity/outcome and the stop has its own deadline/result. Repeat-stop and normal-to-fast escalation tests use only implemented, admitted transitions. Never enable first to manufacture a stop predecessor. Do not rely on a diagnostic stop whose predecessor or physical effect remains unqualified.
6. After node 1 cases pass and settle, run a corresponding bounded case on node 2. Check node 1's retained outcome remains unchanged. Exercise one node's work while the other supplies reviewed observations, then a separately admitted urgent stop on the appropriate node. Check capacity/fairness and asynchronous receive; this is not coordinated motion. If simultaneous motion lacks its own bounds, keep the second axis stationary.
7. Retain post-test native state, settings, reference validity, original error history, controller counters, maximum service gap, RX/trace/console loss and task stack high-water mark during actions. Current stack allocation is 12288 bytes; measure the action path rather than recycling the earlier read-only 9580-byte headroom. Count every state-changing admission and every deliberate trial.

## Investigate, correct and retest

For failure, freeze the first terminal and raw record before recovery. Identify possible hardware effects. Stop campaign admissions when physical state or queue retirement is uncertain. Use independent cutoff when required; ask the operator for physical action or measurement when software cannot supply it.

Reproduce timing/queue/late-reply/fault cases with the real owner and fake adapter first. A live fault injection is permitted only when the campaign defines its exact action, independent control and expected bounds; no electrical shorts, random unplugging or forced dangerous load. Simulate lost replies and ambiguous triggers before contemplating an isolated hardware reproduction.

State competing hypotheses and the observation that separates them. Fix the confirmed cause, add a meaningful regression, repeat that same bounded HIL case with fresh identity and proved current state, then a short unaffected read/move/stop regression. Do not erase an uncertain effect by a new epoch, quiet interval, release or MCU reset. No automatic write replay or hidden cleanup retry; a further stop is a separately admitted action. A later pass alone never closes the defect.

## Review and finish

Delegate offline traffic/terminal analysis and adversarial test review to independent workers; the lead verifies their claims against bytes and code. Run focused axis/position/action/reference/read/settings/owner/service regressions, parser tests, full native/package/Doxygen verification and affected firmware checks. Verify required CI on the exact final implementation commit.

Write `docs/reports/handoff_19_33/packet_22.md` with case counts, exact artifact links, defects/causes/fixes/retests, physical observations, unknowns and pending capabilities. Update original prompt reports, coverage, backlog and user documentation without declaring blocked rows complete. Preserve evidence hashes, commit verified changes and sync the same branch. Report the next smallest measurement when a hardware fact remains unresolved; do not manufacture a successful campaign.
