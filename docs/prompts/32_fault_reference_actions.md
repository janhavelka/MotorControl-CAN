# Prompt 32 Fault clear and device-reference actions

Apply [COMMON.md](COMMON.md). Prerequisites: [21](21_io_settings.md), [30](30_drive_actions.md), [31](31_stop_actions.md).

## Objective and boundaries

Complete explicit alarm clearing and supported device-position/reference actions using the established action lifecycle and owner. Keep fault clearing, device counter zero, host origin, successful homing, drive release and factory reset visibly different. Factory reset/save belong to prompt 39; homing/reference publication belongs to 36.

Follow [CONVENTIONS](CONVENTIONS.md#3-motion-typed-reads-and-terminal-evidence) and [MULTIDRIVE](MULTIDRIVE.md). Common names express intent; native edge/trigger and reference consequences come only from reviewed firmware evidence.

## Read and audit

Refresh MotorControl-RS, ADS1115 and INA228 revisions and dirty state. Read RS alarm/device-position actions, reference invalidation and uncertainty tests; inspect INA228 hardware-state validity and PCA9555 explicit output actions. Review FieldCore reference generation and retained fault consumers read-only. Recheck original Lichuan fault-clear, monitor/EMCY and `0x2302` position-clear descriptions against the EDS and selected model/firmware.

### Concrete evidence to recheck

Use [motion facts](../audit/LICHUAN_MOTION_FACTS.md) and [object facts](../audit/LICHUAN_OBJECT_FACTS.md) for the distinct reset routes. `0x2301` is U16; EN86 p18 describes 1 clear/0 ineffective, but that does not establish an edge or equivalence to controlword bit7. The bit7 reset prose and full-word `15 -> 128` table also need reconciliation. `0x2302` position clear explicitly uses `0 -> 1` then manual return to 0. It does not prove a physical home, arbitrary set-position support or automatic rebase. Retain raw `0x603F` and post-action evidence; acknowledgement is not proof the fault cause disappeared.

## Implement

1. Implement `prepareClearAlarm` and `prepareSetDevicePosition` only for their evidenced capabilities, through `nextAction`/`advanceAction`. A firmware supporting only zero-clear cannot claim arbitrary position setting; unsupported requested values reject before work.
2. Write a transition table per action: admissible drive/mode states, command/value/edge, rearm requirements, matching service reply, required new observation, terminal outcome and affected evidence. Do not infer edge behavior from a static object value. The table's `15 -> 128` is not permission to enable first: never send `0x000F` or `6,7,15` merely to prepare fault reset. Resolve the current-state reset transition or reject before TX.
3. Retain original error history, EMCY, raw status and failed operation evidence when fault clearing is requested. Acknowledged clear does not prove the fault condition is gone. No automatic retry, resume, enable or motion follows a clear.
4. Treat confirmed or uncertain device counter changes as invalidating dependent axis origin/reference, cached position and prepared targets according to the established generation contract. Do not manufacture a known home reference from a local origin update or a counter reset.
5. Publish outputs atomically and preserve partial/unknown effects. Integrate actions through the same owner and event precedence as other drive work. A late reply may update observation evidence without rewriting the original terminal failure.
6. Close the corresponding native-action rows explicitly. Digital-output actions are already owned by 21, tuning/control groups by 22 and persistence by 39; do not accumulate unrelated “remaining actions” in this prompt.

## Verify and exit

Test supported and unsupported position values, clear/rearm edge sequences, wrong state/mode, active fault, persistent fault after acknowledged clear, lost reply, cancellation, deadline race, reboot and stale generation. Verify original error/EMCY evidence and interrupted operation outcomes survive fault clearing.

Exercise counter-clear success, failure and uncertainty against axis/reference invalidation and later admission. Assert fault-reset traces contain no enabling preamble or subsequent re-enable; no action implies save, factory reset or resumed target. Test direct public API calls independently of CLI and include the owner integration. Exit only when every confirmed assigned action has positive/negative tests, documentation and coverage; unresolved firmware behavior remains precisely gated. Run action, reference and owner regression gates.

## Documentation and delegation

Apply [DOCUMENTATION](DOCUMENTATION.md), [MAINTAINABILITY](MAINTAINABILITY.md) and [DELEGATION](DELEGATION.md). Document fault-clear versus reset and counter-clear versus origin/home with Doxygen for uncertainty and generations. One implementer owns each action. An independent stronger reviewer checks destructive/uncertain effects and edge evidence; a separate agent may add disjoint reference tests.

Record closure in `docs/reports/prompt_32.md`, update user docs/coverage and commit and sync on the existing execution branch. Synchronize complete verified action checkpoints during the prompt when useful; do not mark a blocked action complete.
