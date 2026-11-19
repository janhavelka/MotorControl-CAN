# 08 — Settings and dangerous persistent settings

Execute this stage under the [execution contract](execution_contract.md).
Status: NOT STARTED. Run after 03. MK-D10, D18, D21, D23, D27, D29 and D34 are central. Bus settings (8AH-8DH, 4CH),
restart and factory restore belong to [10](10_bus_settings_and_restart.md).

## Work

1. One `SettingsContext` (`prepareDriverRead`, `prepareDriverSettings`, `nextDriver`, `advanceDriver`, `getDriver`) for
   prior read, whole-candidate validation, write, readback and partial evidence. Stages 07, 09 and 10 reuse it. Prior
   read and readback use 00H only for codes 03 or 12A established as readable (MK-D10); a two-byte `FF FF` reply is
   ambiguous (MK-D34). Without readback a write ends acknowledged-only, never verified.
2. Persistence: menu settings save automatically and take effect immediately (MK109 p.9); there is no save command
   (MK-D21), so `prepareSave` is unsupported with that reason. Treat every write as persistent unless the page documents
   a volatile form. Flash endurance is undocumented: no cyclic writes, no write-for-coverage loops, no automatic
   restoration. Retry class is `IDEMPOTENT_SETTING` only for a side-effect-free absolute value with established
   readback; otherwise `NOT_REPEATABLE`.
3. Groups, each one tested and committed block, validated against the declared board (2 = 42D, 4 = 57D) and dialect:

   | Block | Codes | Policy |
   | --- | --- | --- |
   | Motor and current | 83H, 9BH, 84H, 89H | 83H saved form replies 1, unsaved form (trailing 00) replies 2 (MK109 p.42); prefer the unsaved form unless persistence is requested. Maximum 3000 mA (42D) or 5200 mA (57D), and the manual discourages the maximum. 9BH 0..8 = 10..90 % applies to OPEN and CLOSE only (p.43). 84H 1..255; 256 refused (MK-D29). Microstep changes invalidate pulse scales and speed mappings. |
   | Work mode | 82H | Per dialect values (V1_0_0 mode 3 is CR_CAN, MK100 p.15). Changing to CR_* removes bus motion control; changing loop type changes the speed limit (p.23, p.78). Invalidates enable, motion and completion evidence. |
   | Pulse-interface only | 85H, 86H | En level and direction apply to the pulse interface (p.24, p.44); bus motion is unaffected. Explicit only. |
   | Display and keys | 87H, 8FH | 8FH 00 and 01 are both labelled "Key lock" in MK109 p.48 while MK106 p.27 gives 01 lock, 00 unlock (MK-D23): refuse until resolved. |
   | Protection | 88H, 9DH, 95H | 88H stall protection default off (p.53). 9DH enable, Tim (about 15 ms units) and Errors (28000 per turn) (p.54); MK106 bit 1 triggered En homing (MK-D27), so bit 1 is always written 0. 95H enable and threshold (65535 per turn, default 200) decides when "complete" is sent (p.50): a change invalidates 05's completion qualification. |
   | Host-silence protection | 98H | Four-byte ms, 0 off (default) (p.50). Only an explicit application request writes it; the library never sets a default, never enables it implicitly and never feeds it. Feeding frames, stop type, enable after trip, reporting, persistence and signedness are unknown (MK-D18); bit 31 set is refused. A binding with nonzero 98H records `hostSilenceConfigured`. |
   | Tuning | 96H, 97H | Sub 00 Kp/Ki, sub 01 Kd/Kv, uint16 0..1024 (p.51-52); read with `00 96 00` (p.51). Native values, no unit conversion. |
   | I/O | 36H, 9FH, 99H | 36H mask/value bits (p.63). 9FH turns IN_1 into an output on 28D/35D/42D (p.62); 99H OUT_2 divider on 57D only (p.64). Physical load scope is the application's. |
   | User ID | 42H | Four bytes, persistent (p.39). The only host-assignable identity mark; an application may use it to confirm a binding, explicitly. |

4. Calibration 80H rotates the motor and requires no load (MK109 p.41, p.115): an explicit `MOTION_LIKE` commissioning
   action, never automatic; replies 0 calibrating, 1 success, 2 fail; F1H 6 while calibrating; afterwards 40H's
   calibration flag is read, not assumed. It invalidates position evidence.
5. **Never written:** FFH (C8 or CA), 9AH, 50H, 0FH. Read and report their state where 00H permits. FFH and 9AH make the
   drive move at every power-on (MK109 p.27, p.85); report them as startup hazards in capability output and the
   startup guide. A later disabling write needs an explicit owner decision and an amendment to this stage.
6. 4AH synchronization flag is written only through 09's operation; 01H reports are 09's.
7. Model applicability: refuse 99H on board 2, 9FH on board 4, currents above the board maximum, and any code the
   declared dialect lacks. Keep raw unknown observed values inspectable without making them setters.
8. Quiescence: whether a setting applies during motion is undocumented, except the unsaved 83H form described for use
   "during operation" (MK109 p.42). Other writes require no admitted motion, homing or calibration on that address and
   a caller-declared stationary state recorded in the evidence; a fresh 32H of 0 alone is not proof.
9. Invalidation, recorded per descriptor and tested: 84H invalidates pulse scales and speed and acceleration mappings;
   82H invalidates enable, motion, completion and the work-mode gate; 95H invalidates completion qualification; 88H and
   9DH change stall semantics; 90H, 94H and 9EH set 07's `limitHomingRequired` and invalidate the home reference.
10. Restoration records: a typed snapshot of read values per code that an application can store and later apply
    explicitly, excluding every motion, homing, calibration, restart and never-written command. Restoration is never
    automatic and never replays a trigger; a snapshot with unreadable codes says which values it lacks.

## Deliver

`Settings.h/.cpp`, `SettingsContext`, typed descriptors per block, `test/mks_servo_can_settings_test.cpp` (one section
per block), owner-backed app and console routes (`mks set ...`, `mks get ...`, `mks calibrate`), API/CLI parity,
coverage rows per code with its persistence class, report and PROGRESS row.

## Acceptance

Per block: exact frames, accepted and refused values, model and dialect refusals, prior read and readback where
established, acknowledged-only result where not, status 0 rejection, lost reply with execution unknown and no repeat,
invalidation of dependent scales and completion qualification, saved versus unsaved 83H replies, and refused FFH, 9AH,
50H, 0FH and 8FH. No automatic save or restoration. The full matrix of contract 02 "Registration acceptance" for this
major change and CI on the exact final pushed commit pass for each committed block. Physical current, I/O and
calibration checks need actual fixtures: hardware NOT RUN. Commit and push each block.
