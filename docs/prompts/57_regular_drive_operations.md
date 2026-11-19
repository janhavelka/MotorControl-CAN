# Prompt 57 Regular enable, release, mode select and firmware identity policy

Apply [COMMON.md](COMMON.md). Prerequisites: [30](30_drive_actions.md), [42](42_cli_motion_routes.md), [45](45_fault_integration.md).

## Objective and boundaries

The user's requirements, verbatim (2026-11-12): "Enable is experiment-only, and there is no 'free the shaft' command -
add these to be regular commands"; "No typed mode switch - do it (write/add prompts) - this is just functionality
(literally a function in the lib to call it, right?)"; "Motion is locked to the exact measured firmware - we dont need
this rule"; "I want all of the above additions/functionality to be still accessed through the lib api so another
firmware can use it".

Added 2026-11-17. The user, verbatim, on the bench's per-boot controlword rule: "whats controlword and whats it used
for"; on the recorded stop a released enable requires: "again - a nonsense complication"; and generally: "In general
we are after good engineering practises and more responsibility on the end user of this firmware with possible
optional settable guards"; and on saving: "remove this restriction, it could be saved", about the measured fact that
`1017` (heartbeat), the TPDO1 type, `2201` and `2500` revert at every power cycle without a save, so every session
starts with setup again ([HIL report](../reports/2026-11-16_hil_campaign.md)). Three complications go:

- The released enable's recorded-stop premise. Today `EnableStart::RELEASED_BY_RECORDED_STOP` (`Actions.h`) asks the
  application to state that a stop it observed left the drive released, `scripts/s2_bench.py` demands that stop's
  evidence directory (`--released-by-recorded-stop`, `check_stop_record`), and so every series after a power cycle
  first runs a 60 rpm HALT velocity run and a halted quick stop only to produce a record. The regular enable instead
  starts from a fresh report showing the released standstill (today's `prepareEnable` rule for that start: mode 1,
  `2100` released, `6041` bits 0..6 `0x50`, `1001`, `603F` and `606C` zero), and the application owns that
  responsibility. The bench runner's record requirement is removed in the 2026-11-17 change; this prompt removes the
  premise from the library gate and its documents.
- The bench's per-boot controlword history rule (`BenchRoute::begin`: a mode write refused after any controlword or
  motion write in this boot, an enable refused after motion writes), which forces one MCU boot per mode change and
  enable. The regular mode select's check is a fresh report showing the drive not enabled; the enable's is the fresh
  released or power-up report. No per-boot history is required.
- The save's mandatory review records. `prepareSave`/`prepareFactoryRestore` (`Persistence.h`; one `2300` write, 2
  saves and 1 restores, `Persistence.cpp`) require a `PersistenceReview` with nonzero `fixtureRecord`,
  `firmwareRecord`, `procedureRecord` and `authorizationRecord`, and the console's `persistence save parameters` only
  consumes a context armed externally (`ProbeCommissioning::armPersistence`), so neither the bench nor an application
  can simply save.

Deliver four regular library operations (mode select, enable, release, save) and one policy change, with the evidence
in the [motion capability report](../reports/2026-11-12_motion_capability_evidence.md) sections d, e and f:

- **Mode select:** `prepareModeSelect` with the existing `nextAction`/`advanceAction`, documented modes 1, 3 and 6
  only, written once while the drive is not enabled, settled when `6061` reads the new mode; never writes `6061`, never
  reads `6060` (WO, D04), no mode change while enabled (Q11). About 130 measured mode writes all succeeded. It replaces
  the raw `6060` writes in the examples (`AxisSession`, bench `MODE_ONE`/`MODE_THREE`). Undocumented codes stay in the
  D17 probe.
- **Enable:** a `MEASURED` enable gate beside CLOSED and ATTENDED_EXPERIMENT, opened by a reviewed record of the
  measured enables (111 succeeded, 2 early failures with known causes), so enable is a regular command. Its start is
  the fresh report alone: the released standstill or the measured power-up report, without a retained stop record or
  a per-boot word history (2026-11-17, above).
- **Release (free the shaft):** `prepareRelease` (the RS name) writes the one word that the release table below
  gives for the fresh predecessor report, and is settled from the drive's report, never by TX. No manual names a
  release word (EN86/ZH86 p12 and OP86 p10 only pair `6040=0` with `6041` low bits 000), so each row is measured, not
  documented, and keeps its scope: two CL86-C specimens, one firmware (`1000=00040192`, `1009=000c`, `100A=0020`).
  Added 2026-11-13 (backlog S21, [report](../reports/2026-11-13_enable_fault_state.md)): release also covers the
  switched-on, stationary, alarm-free state a refused enable leaves, which no library action admitted: the stop
  refuses a stationary report and the clear needs an alarm. Release from mode 1 idle and from the halted mode 3 state
  are bench trials (the halted case may also resolve backlog S20). [Prompt 72](72_enable_fault_guards.md)'s word 7
  guard uses this same table.
- **Save parameters:** a regular library command, `prepareSave` with the review records as optional, settable guards
  instead of mandatory fields, reachable from the console (`persistence save parameters` preparing its own context)
  and from the bench. It is explicit only, never implicit on startup, selection, probe or recovery (AGENTS.md); its
  evidence is the `2300` acknowledgement, a readback of the saved values and, when the user next power-cycles, the
  same values read before any write. Factory restore stays a separate explicit command (`prepareFactoryRestore`,
  `2300=1`). Its backup guard is optional and settable like the save's review guards, which is the user's policy of
  2026-11-17, not a manufacturer statement: when set, a restore without the caller's backup record is refused before
  TX. That record names the configuration subset the application read back and lists what stays unknown (the
  complete persistent set and the factory values, [prompt 39](39_persistence.md)), so no read dump is called a
  complete or restorable image. Guard set or not, the restore keeps its own intent, its uncertain effects and no
  replay. [Prompt 61](61_drive_configuration.md)'s save trials, per-node save count and restore trial use these
  commands.
- **Identity policy:** motion stays refused when the product identity differs (`1000`, `1018:01` vendor, `1018:02`
  product, `2000`), because another product is another profile. A different firmware identity (`1009`, `100A`,
  `1018:03`, `2001`) no longer refuses: the operation runs and its terminal result carries "firmware unqualified" with
  both identities, so qualification stays honest without locking the user out.
  A matching product identity does not prove a CL86-C: the Lichuan DS-C and TL-C EDS files declare the same
  `1000`, `1018:01` and `1018:02` with different vendor objects (backlog D18; TL-C `2402` is a bitrate object). The
  application's declared binding selects the profile and model, the identity read only confirms it, and the family
  promptsets `lichuan_c_extensions` and `lichuan_ds2_canopen` add their model-scoped checks on this one policy.
  Identity outcomes, the one definition both Lichuan family sets use (inputs: the binding's declared model as a
  profile-qualified key, a current identity observation for the binding's node generation and server session,
  the application's candidate set for the interface, and identity references with their DOCUMENTED or MEASURED
  basis):
  - Outcomes:
    - CONFIRMED: a reference of the declared model matches, every other candidate has a reference and none
      matches.
    - CONTRADICTED: a reference of another candidate matches and none of the declared model does.
    - UNCONFIRMED: everything else, including values shared by the declared model and another candidate, a
      candidate without references, no match and fields that could not be read.
    The result carries the outcome, the live fields, every matching model with its reference basis and whether
    a reference of the declared model matched. An outcome exists only for a current observation; the
    invalidators of canopen_shared 02 "Identity and device evidence" clear it. It never selects or changes a
    declaration. Firmware identity (`1009`, `100A`, `1018:03`, `2001`) is reported and never refuses (prompt 57).
  - Vendor-object rule, the same for both sets: on CONTRADICTED every SDO download to the manufacturer area
    `2000h..5FFFh` (principles pack section 1) on that binding is refused before TX. No acceptance, guard
    setting or option overrides it; the application corrects the declaration, candidate set or references
    instead. A model of either set also needs a current outcome before such a download; CL86-C and OL86-C keep
    prompt 57's existing admission (stage 01 register).

Release table: the one predecessor-to-word table that the release's admission, its tests and prompt 72 use. A row
is chosen by a fresh report (`6041`, `2100`, `606C`, `1001`, `603F`, `6061`, and `2500` for the `0x0000` row), never
by retained history.

| Fresh predecessor report | Word | Evidence and scope | Report after the word |
| --- | --- | --- | --- |
| `6041` low bits 111, `2100=0001` (in place; released, alarm and running clear), `606C` zero, `1001` and `603F` zero, mode 1: the power-up report `8437`/`0001` | `0x0006` | Measured as the first word of every power-up enable (21 records, both specimens); node 1's shaft then turned freely by hand ([drive actions](../drive_actions.md), 2026-11-03). The same report after this library's own word 15 cannot be told apart and is admitted on it; the mode 1 idle bench trial measures that case | Low bits 001, `2100` released: `8031`/`0011` |
| `6041` low bits 011, `2100=0001`, `606C` zero, `1001` and `603F` zero, mode 3, open loop (`2500=1`): `8033`/`0001`, as a velocity enable refused at word 7 leaves it | `0x0000` | Measured once: node 2, trial 5991, in the S21 enable-fault state, about 15 minutes after the refusal (trial 5976). Unmeasured: word 6 from this report, and `0x0000` in mode 1, in closed loop or with `606C` nonzero | `8050`/`0011`; the same read gave `606C` 1711, inferred not a rotor speed, so the bench route ended with failure 9 (`why=velocity`) and its alert |
| `6041` low bits 000, `2100=0001` (released clear), `606C` zero, `1001` and `603F` zero, mode 3: `8010`/`0001`, as an `OBJECT_2301` alarm clear leaves it | `0x0080` alone | Measured: trial 5405, then 8 of 8; `0x0000`, `0x0006` and `0x000B` changed nothing there (trials 5402 to 5404) ([limits report](../reports/2026-11-13_velocity_limits_and_fault_recovery.md)). It is the fault-reset bit (ZH86 p12 and OP86 p10 `15->128`; EN86/ZH86 p15), the measured exit from this report only, never a release word elsewhere | `8050`/`0011` |
| `2100` released set with low bits 000 or 001: `8050`/`0011`, the latched `4050`/`0014` and `C050`/`0015`, `8031`/`0011` | None: reported as already released, nothing sent | Already released | Unchanged |
| Any other report: running or `606C` nonzero, an alarm, another mode, the halted mode 3 `9437`/`0001`, S20's `1437`/`0000`, the mode 1 report `0037`/`0000` | None: refused before TX, with the reason | Unmeasured. A moving drive needs a stop, an alarm the alarm clear | Unchanged |

Release settles when the row's report arrives: its `6041` low bits and `2100` released. `606C` is recorded with it, a
nonzero value is reported (as in trial 5991), and release never claims standstill. A refused or unacknowledged word
is not repeated. An explicit, caller-set experiment option admits `0x0000` or `0x0006` from a report outside the
measured rows (never `0x0080` outside its row, never a word with bits 0..2 all set) and marks the result as an
experiment. It overrides only the unmeasured refusal (another mode, loop or predecessor word, or `606C` nonzero with
`2100` running clear, as in S21), never an alarm (the alarm clear's case), `2100` running set (a stop's case) or an
already released report; only a reviewed measured result adds a row. The release bench trials and prompt 72's bench step 2 run
this way.

Boundaries: no implicit enable on startup, selection, probe or recovery; release is not a stop and never claims
standstill; a mode select never enables; uncertainty after an unconfirmed write is kept; the console gets no logic of
its own (it calls these operations).

## Read and audit

For this prompt, refresh MotorControl-RS HEAD and dirty state and read its `prepareEnable`, `prepareRelease`, mode and
identity seams; RS is authoritative for common names. Inspect FieldCore-node read-only for enable/release use. If HEAD,
dirty state and seam match the last recorded row, write "unchanged since prompt N" and do not reread. A sibling row
must name a behavior adopted or rejected, with path and symbol. This does not relax vendor evidence, safety,
uncertainty or verification rules. Reopen EN86/ZH86 p11-12 for `6060`/`6061` and the controlword table, the drive
actions guide, `Actions.h`, `Position.cpp`, `Velocity.cpp`, `Homing.cpp` identity checks, and the bench records the
report cites. Apply [CONVENTIONS.md](CONVENTIONS.md), [MULTIDRIVE.md](MULTIDRIVE.md),
[DOCUMENTATION.md](DOCUMENTATION.md), [MAINTAINABILITY.md](MAINTAINABILITY.md) and [DELEGATION.md](DELEGATION.md).

## Implement

1. `ActionKind::SELECT_MODE` with `prepareModeSelect`, its evidence and terminal result; replace the example raw mode
   writes with it, and replace the bench's per-boot controlword history rule with the fresh not-enabled check.
2. The `MEASURED` enable gate and its reviewed record built from the retained enable evidence, started from the fresh
   released or power-up report alone; `EnableStart::RELEASED_BY_RECORDED_STOP` loses its retained-record premise (or is
   replaced by a released start), and the documents and runners stop asking for a stop record. The released start
   also takes the report a release leaves (`8031`/`0011` after word 6), which today's `0x50` rule refuses. The
   attended experiment gate stays for unmeasured states.
3. `ActionKind::RELEASE` with `prepareRelease` implementing the release table above: each measured row's one word
   and report, the settlement rule, refusal before TX of every other report, and the explicit experiment option. The
   table lives in one place in code and documentation, and prompt 72's guard calls this operation.
4. The identity policy in the position, velocity and homing gates and in enable, with the unqualified-firmware flag
   in every terminal result and in the console output.
5. Console commands `mode AXIS_ID 1|3|6`, `enable AXIS_ID` and `motor-release AXIS_ID` as regular commands, the last
   through `prepareRelease`; it replaces the caps-only `motor-release` row that prompt 42 gated on D-17.
   `release OPERATION_ID` keeps its meaning, freeing a retained result without any CAN frame, and the console never
   infers which meaning was intended from which ID happens to exist. Change the `caps`/help inventory and its JSON
   records, `docs/cli_contract.md`, `docs/api_cli_mapping.md`, the example READMEs,
   `scripts/console_scripts/local_routes.txt`, `test/motion_console_test.cpp` and `test/bench_probe_cli_test.py`
   together; `scripts/bench_probe.py` keeps `release OPERATION_ID`. Bench routes for the release trials.
6. Save parameters with optional review guards through the API, the console and a bench route; factory restore with
   its optional backup guard and the backup scope stated above.

## Verify and exit

Host tests replay the retained enable and mode records, refuse a mode change while enabled and every undocumented
code, prove release never claims standstill, cover product-identity refusal versus firmware-identity flagging, and
cover the save with and without each review guard, a save never sent implicitly, and a refused or unacknowledged
`2300` write reported as uncertain without a repeat. For release they assert the exact wire word for each measured
row of the release table and refusal before TX of every other report (word 6 from `8033`, `0x0000` in mode 1, in
closed loop or with `606C` nonzero, `0x0080` outside `8010`, a moving or alarmed report), an already released report answered without TX, the
experiment option's limits, a lost reply kept UNKNOWN and never resent, and a nonzero `606C` reported beside a
released report. With an axis and a retained operation sharing one numeric ID, `release ID` frees the result and
emits no CAN frame, and `motor-release ID` acts only on the axis through `prepareRelease`. Factory restore is refused
before TX with the backup guard set and no record, admitted with the guard clear, and in both cases keeps its
separate intent and uncertain effects without replay.
Run the native, firmware, stack and documentation gates and CI on the actual final commit. Bench, on both nodes:
release from mode 1 idle (the table's first row); through the experiment option, release from the halted mode 3 state
and `0x0000` from the mode 1 switched-on report (`8033`/`0001`, `606C` zero) that an enable cancelled after word 7
leaves on a healthy drive; also through it, release from `0037`/`0000` (the word-15 report after an enable from a
latched quick stop) and from mode 3 enabled idle, so measured rows can be added; a re-enable after each release, with `6064` before and after word 7; set `1017=1000` and
save on both drives, read it back, and confirm it after the user's next power cycle; then a short regression.

## Documentation and delegation

Update the drive actions, position, velocity and homing guides, the commissioning guide's persistence section, the CLI
contract, coverage, backlog (D04, D17, S20, S21),
CHANGELOG and Doxygen in the same stage. A stronger reviewer challenges the release settlement and the identity
policy; normal coding agents own disjoint tests. The lead integrates and runs combined checks. Record
`docs/reports/prompt_57.md`. Commit and sync on the current branch before handoff.
