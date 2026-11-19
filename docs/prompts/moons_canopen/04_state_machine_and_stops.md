# 04 — State machine, enable, release, fault reset and stops

Execute this stage under the [execution contract](execution_contract.md).
Status: NOT STARTED. Requires 02b-03. MO-D05, MO-D06, MO-D19, MO-D24, MO-D28 and MO-D33 are owned here.
Reuse prompt 57's common enable, release and mode-select names and its firmware identity policy where landed.

UM21 reproduces the CiA 402 state machine (UM21 p15-p16) but implements a subset. Each word below is an
explicit caller operation through `prepareEnable`, `prepareRelease`, `prepareClearAlarm`, `prepareStop` and
the concrete `ActionContext`; nothing runs on binding, probe, read or recovery.

## Work

1. Enable: the documented sequence is 0x0006, 0x0007, 0x000F (UM21 p19, p88); velocity, torque and Q examples
   enable with halt set, 0x010F, and start by clearing halt (UM21 p32, p34, p35). Each step confirms the target
   state by a fresh statusword read, never by the SDO download response alone. The enable-with-halt variant is
   an explicit option of the request. AMP10 p21's single 0x001F enable-and-start is the 2010 layout and is not
   used here. Never enable to prepare a stop, a fault reset or a mode change.
2. Enable prerequisites: read the enable input function 0x2617 (STF) or 0x220D (SSDC) and the interlock bit 3
   of 0x60FD first (UM21 p63, p75, p87). A configured enable input can block or remove enabling independently
   of the controlword; its default is 3 (general purpose) in UM21 but 0 in the EDS (MO-D24), so read it, never
   assume it. Report 0x200B bit 0 "Motor Enabled" (UM21 p67) as vendor evidence separate from CiA 402 state.
3. Mode select: write 0x6060 (write-only) and confirm through 0x6061 within the operation's deadline (UM21
   p35: 0x6061 updates "when the current operation mode is accepted"). Admit 1, 3, 6 and, on SSDC only, 4.
   -1 and -2 are refused here (stage 10). UM21 does not state in which drive state or motion state a mode
   change is accepted (principles CP-D24); require an explicit caller policy (disabled, or host-established
   standstill) and keep the drive's own rule an open question for 14a.
4. Release: which word removes motor current is not documented. Shutdown 0x0006, disable operation 0x0007 and
   disable voltage 0x0000 reach different states, and the power-stage meaning of Switched on differs between
   vendors (principles CP-D11); 0x605B and 0x605C reactions are undocumented (MO-D33). AMP10 p26's NMT "Drive
   Disabled" state does not exist (MO-D28); NMT never releases. Implement `prepareRelease` with one reviewed
   word, report 0x200B bit 0 and the CiA 402 state as separate evidence, and keep "shaft free" unobserved until
   14a. Brake sequencing by 0x2607/0x220C (UM21 p75, p83) is drive-managed and not described against the state
   machine; report the configured brake mode, claim no brake action.
5. Fault reset (`FaultResetMethod`, a MOONS'-local enum): `CONTROLWORD_EDGE` writes a word with bit 7 = 0 that
   does not enable (0x0000), then 0x0080; UM21 p16 says the reset happens only "if no fault exists currently",
   so success needs a fresh statusword without bit 3, and the state afterwards is Switch on disabled.
   `DSP_CLEAR_ALARM` writes 0x2006 = 1, which clears 0x603F and 0x200F (UM21 p66); it is a separate vendor
   action with its own evidence. Clearing 0x2217 misc flags (write 0, UM21 p79) and 0x1003 history (sub 0 = 0,
   UM21 p42) are distinct explicit operations, never side effects of a reset.
6. Fault classification: 0x603F is a vendor bit field (UM21 p56); which bits disable the motor is marked only
   in AMP10 p20 for the 2010 generation (MO-D19). Publish named bits and the CiA 402 fault bit; claim no
   severity or automatic disable for UM21. No 0x605E exists (MO-D33), so the fault reaction is unknown.
7. Quick stop: the command clears controlword bit 2 (0x000B from Operation enabled; principles s12). Its
   reaction follows 0x605A, which supports only option codes 1 and 2 "at this moment" (UM21 p58); both end in
   Switch on disabled (UM21 p16 transition 12), so the drive does not hold position afterwards. The power-up
   value of 0x605A is undocumented and its EDS default 0 (disable drive function) is not a supported value
   (MO-D05; AMP10 p16's "2 & 6 only" applies to the 2010 layout, stage 13). Admit a quick stop with a ramp
   claim only after a fresh 0x605A read of 1 or 2 in this node epoch. Provide an explicit setter for 1 or 2
   with readback; 0x605A is in no save list (UM21 p45-p46), so assume it is lost at reset and re-read it.
   0x6085 is the code-2 ramp in profile-acceleration units (UM21 p62; units per stage 05).
8. Halt (controlword bit 8) has no option code: 0x605D is referenced but absent (MO-D06). Record the documented
   per-mode effects: profile position stops in position control and **continues the set-point when halt is
   released** (UM21 p21), so halt is a pause and a later halt release can restart motion; homing stops
   (UM21 p22); profile velocity and torque per stage 06; Q programs stop immediately and restart from the
   beginning (AMP10 p48). The halt ramp source on UM21 is unknown. A halt-based stop retains "pending set-point"
   and is never released implicitly by any stop, recovery or reset path.
9. Stop kinds and seam S3: define the MOONS' stop kinds (quick stop, halt, disable voltage) as profile-local
   values with their documented reactions; do not reuse Lichuan's `StopKind`. Decide from items 7-8 whether an
   exact reviewed best-effort shape exists for a binding (object, size, data and ledger review ID under
   canopen_shared 02's S3 rules), for example quick stop 0x000B only while 0x605A was read as 1 or 2 in this
   node epoch; otherwise the binding keeps no shape and `admitBestEffortStop` stays `INVALID` with its reason.
   NMT stop only stops PDO exchange (UM21 p14).
10. Owner interaction: urgent stops use `admitUrgent`; stop while queued, in flight or waiting; an interrupted
    operation retains `UNKNOWN` execution and the stop result is correlated separately. Cancel is not a stop.

## Deliver

Profile `Actions.h/.cpp`, `ActionContext`, `FaultResetMethod` and the MOONS' stop kinds; the quick-stop option
setter; `test/moons_canopen_actions_test.cpp`; console `enable`, `motor-release`, `alarm-clear`, `stop` routes
for MOONS' bindings with parity tests; S3 registration update; coverage, ledger, Doxygen and report.

## Acceptance

Tests cover each documented transition word, refused transitions, enable blocked by the input function or
interlock, mode-accept timeout, partial enable, reset with a remaining fault, both reset methods, quick stop
with 0x605A unread, 0, 1, 2 and out-of-range, halt with a pending set-point, stop under queue pressure, late
replies, stale generations and no replay. ACKNOWLEDGED never becomes OBSERVED without a fresh state read.
Quick and full verify, firmware builds and exact-commit CI pass. Release, brake and quick-stop physical effects
stay open for 14a. Hardware: NOT RUN.
