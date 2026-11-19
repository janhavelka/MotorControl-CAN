# 03 — Typed reads, identity gate and capabilities

Execute this stage under the [execution contract](execution_contract.md).
Status: NOT STARTED. Runs after 02b. NT-D01, D02, D03, D14, D26, D27 and D29 gate individual
interpretations. Reads change nothing; a successful read never qualifies a device.

## Work

1. `prepareIdentity`: 1000h, 1008h, 1009h, 100Ah, 1018h:01-04 and 4040h (serial
   string). Use segmented upload for strings; if the selected backend path cannot,
   use the per-character subindex form only where N5 p123 documents it. Publish raw
   values first, then the identity verdict of the execution contract: qualified row;
   firmware or hardware unqualified (FIR-v1650 06720000h, FIR-v2538 09EA0000h, CL4-E
   FIR-v2315 090B0000h, an unlisted build, a missing or out-of-range hardware
   declaration), with the declared and observed identities and the row's behavior
   gates (13); or product contradiction (1000h, 1018h:01 or 1018h:02 against the
   declared model, prompt 57 "Identity policy"). Only a product contradiction blocks
   every dependent operation, leaving the identity reads; an unqualified verdict
   blocks only its gated operations, and objects its evidence says are absent are
   not requested. Report the 1018h:03 high word as a decimal number labelled
   "inferred firmware number", never as a gate. The verdict is held in an
   application record keyed to binding, generation and server session, and is
   invalidated by boot-up, NMT reset, bus-off or controller restart, rebinding, a
   configuration generation change or a contradicting identity read (canopen_shared
   02 "Identity and device evidence").
2. Declared facts. The caller binds a declared model and its nameplate hardware
   revision (1009h is "0", NT-D29). The verdict compares the declaration with the
   admission line's range (02 item 5): missing, unparseable or below the minimum is
   reported as hardware unqualified and refuses nothing. A product code that belongs
   to another model refuses; for PD4-C either documented code of the declared variant is
   accepted and the observed one is recorded (NT-D14). No identity value selects a
   profile or model; the declared binding selects and identity only confirms
   (MULTIDRIVE row 57, backlog D18).
3. `prepareConfig` in reviewed groups, each with its own observation and generation:
   - Motor and feedback: 3202h (bit 0 closed loop, 1 VoS, 2 brake control, 3 current
     reduction, 4 auto alignment, 5 torque mode, 6 BLDC, 7 slow speed; N5 pp297-298),
     3203h, 33A0h:01 where present, 2030h, 2031h, 6075h, 6073h, 203Bh:01/02, 608Fh,
     6090h, 60E6h, 60EBh and 2059h where the model has it.
   - Units: 60A8h, 60A9h, 6091h, 6092h, 6096h, 6097h, 60A2h, 604Ch, 607Eh.
   - Reactions: 605Ah-605Eh, 3700h, 3701h, 6007h, 1029h:01/02.
   - Supervision: 1016h:01, 1017h, 100Ch, 100Dh, 1F80h, 1007h. With the Reactions
     group this stage publishes the typed read-only `Supervision` snapshot that 09b
     item 1 specifies (moved here so that 04's enable can report it; 09b adds the
     setters): factory values decode as "no host-loss supervision active".
   - Control paths: 2290h, 2292h, 2300h, 2301h, 2302h, 60F2h, 4015h:01/02 on PD4-C,
     and the RPDO valid bits, transmission types and mapping entries (1400h-1407h:01
     and :02, 1600h-1607h) as raw observations.
   - Limits and modes: 607Dh, 607Bh, 3240h, 6502h, 6060h, 6098h; 1020h stamp.
   An object absent from the declared model's scope is not requested and reports
   "not in model". 2005h is decoded per NT-D04: 81h-86h and 88h as documented,
   78h and 87h raw with both source claims.
4. `prepareState`: 6041h, 6061h, 6064h, 606Ch, 603Fh, 1001h, 60FDh and 2301h. Decode
   the CiA 402 state with the masks of N5 p58 (004Fh and 006Fh forms) and keep the
   raw word. Nanotec bit meanings stay profile-local: bit 9 "Remote" is always 1 and
   proves no control (NT-D26); bit 15 means closed loop active and is meaningful only
   in Operation enabled (N5 p53, p348); bits 10-13 are mode specific and are decoded
   only with the mode that was active when the word was sampled.
5. Capabilities. The caller supplies `Capabilities` (`MotorKind::STEPPER` or `BLDC`,
   `FeedbackKind::NONE`, `ENCODER` or `HALL`, position, velocity, torque, brake,
   calibration). The profile checks them against readback and refuses a
   contradiction: 1000h bits 16-23 (2 BLDC, 4 stepper, 6 both, N5 p159) and 3202h
   bits 0 and 6. Rules from N5 p46 and p96: torque (PT, CST) and homing on block only
   in closed loop; closed loop needs a completed auto setup; IP, CSP and CSV are "not
   advisable" in open loop and the host refuses them there. In open loop the actual
   position follows the commanded field unless 3203h selects an encoder for display
   (N5 p300), so no measured-position claim follows.
6. Exclusive-control facts for 04b: publish typed `ControlPaths`, bound to binding,
   boot and control-path generations (Plug & Drive enabled; NanoJ 2300h bit 0 and
   2301h RUN/ERR; each valid RPDO with its transmission type and the command objects
   it maps from 04b item 1's list; 6061h; PD4-C 4015h:01/02). This stage only
   observes; admission is 04b item 1's.

## Deliver

`profiles/nanotec_canopen/Reads.h/.cpp` with `ReadContext`, `getIdentity`,
`getConfig`, `getStateBlock` and typed observations; `IdentityVerdict`,
`ControlPaths`, the read-only `Supervision` snapshot and capability checks;
`test/nanotec_canopen_reads_test.cpp`;
owner-backed console routes `nanotec read identity|config|state` with parity tests.
Lichuan `Reads` is a structural example only; no Lichuan field or decision is reused.

## Acceptance

Tests cover exact read plans per model scope, absent-object handling, string
uploads, every identity verdict including the three named unqualified rows with
their gates, an unlisted build, missing and below-minimum hardware declarations with
an otherwise matching electronic identity (reported unqualified, nothing refused), a
product contradiction (dependent work refused, zero writes, identity reads only),
both PD4-C codes, the
`Supervision` snapshot with factory values reported as unsupervised, contradiction
refusals for each capability rule,
1003h-style empty aborts, malformed and partial sequences, deadlines, stale
generations and unchanged outputs on failure. Read effects come from the ledger and
set `RetryClass::REPEATABLE_READ` only where every upload is reviewed non-changing.
Quick verification, firmware builds and CI on the exact final pushed commit pass.
Hardware NOT RUN; no readiness, calibration or firmware qualification follows from a
good reply.
