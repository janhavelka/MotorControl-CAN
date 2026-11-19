# 06b — Finite profile torque

Execute this stage under the [execution contract](execution_contract.md).
Status: NOT STARTED. Runs after 04 and the run pattern of 06. DS-D11, DS-D12, DS-D13 and DS-D14 belong here; DS-D02
and the execution contract's identity rule govern the vendor object `2406h`, whose write stays withheld (item 4).
This is the library's first torque operation: MULTIDRIVE rows 22 and 35 and ADD_DRIVE's torque rule apply. Use strong independent review for the run, stop and limit logic.

## Work

1. Scope of the API. No common torque operation exists, and CONVENTIONS section 3 forbids placeholder torque APIs.
   Deliver a profile-native `prepareTorque`/`nextTorque`/`advanceTorque` with a concrete `TorqueContext` and native
   units, gated by `requireCapability(..., Capability::TORQUE)`. A common torque intent with physical units is a
   reviewed core change owned outside this promptset; record it in the backlog, do not invent it here.
2. Documented sequence (DS2C p12-13; IBL60 p13-14; IBL80 p13; ISL60 p16): write `6060`=4 and confirm `6061`=4; write
   `6071` target torque, `6072` maximum torque, `6087` torque slope and the model's speed-limit object; then 0x0006,
   0x0007, 0x000F, after which the motor runs. As in 06, the enable words are the start and the operation owns the
   targets, enable, run and stop under the one operation deadline, admitted only from a drive that 04's `released`
   predicate accepts. Write preflight (canopen_shared 02) covers every word of the sequence, mode select included: if
   any row is withheld or unresolved for the bound model, the whole operation is refused with nothing sent.
3. Units (DS-D12, DS-D13). `6071`, `6072` and `6077` are 0.1 % of a reference no source states (DS2C p27-28; `P00-12`
   rated torque is unreachable). `6087` is 0.1 % with no time base. Keep native integers; no newton-metre, current
   or rate conversion exists. `6071` is I16 in every source, so signed targets are encoded; the physical direction
   of a sign is a fixture fact. `6072` is U16 default 1000 (DS2C p27, DS2EDS l.1317) versus I16 in the mode table:
   accept 0..32767 only. `6087` is U32 0..65535 (DS2C p12) versus 0..2147483648 (ISL60 p16): accept 0..65535.
   Require `abs(6071) <= 6072` as host policy, labelled as such.
4. Speed limit by model (DS-D11). The download width is a per-scope write-policy gate (02), not a value range.
   `607F` for DS2 and DS2-Mini: U16 -65535..65535 in the mode table (DS2C p12, MINI p14) versus U32 0..2147483648,
   default 2147483648, pul/s in the object table (DS2C p28, MINI object table) and U32 in DS2EDS. A two-byte and a
   four-byte download are different requests, so the width is unresolved and torque is withheld for these models
   until model-scoped evidence (vendor clarification or a qualified fixture record) resolves it; a successful upload
   of one size or a fake server accepting it resolves nothing. `607F` for ISL60 and ISL80: U32 0..2147483648 in the
   torque mode tables (ISL60 p16, ISL80 p15), U32 in the object tables and ISLEDS; once 02 confirms that agreement in
   the ledger it is a four-byte download, and the operation accepts 1..65535 as a positive limit. `2406h` for IBL60 and
   IBL80: U16 0..65535 RW, unit unstated, absent from IBLEDS (IBL60 p13, IBL80 p13; IBL60 p14 names `6072` in the
   step text, a retained typo). A readable word at that index establishes neither its meaning nor its write effect,
   and the pack review withholds every vendor-object write until live reads of the actual drive, its firmware
   identity and a qualified fixture exist (review, "What a profile can implement"). Its ledger row therefore
   withholds the write, and IBL torque is refused with nothing sent, until that evidence is recorded for the model
   and firmware. After that, the write also needs an outcome the execution contract's table admits for changing
   requests and a fresh successful read of `2406h` on this binding; these are necessary checks, not substitutes for
   the evidence. CONTRADICTED refuses it under prompt 57's vendor-object and motion rules. IBL object tables also
   list `607F`, but their torque sections name `2406h`; do not write `607F` as an IBL torque limit.
5. Required caller inputs. Target torque, maximum torque, slope, speed limit and a positive finite `durationUs` are
   all explicit; no manual or EDS default is written and no field is left at an unread value. Zero torque is a
   setting, not a run.
6. Run and stop. The run counts from the start word's admission, and its budget is checked as in 06 item 5
   (overflow-checked `now + durationUs` plus the stop dispatch budget within the operation deadline, at preparation
   and before the start word). At run expiry yield 04's `QUICK_STOP` under `DS-DEC-2` (no torque-mode stop is
   documented). Torque on a free shaft accelerates until the speed limit acts, and whether either limit object
   actually clamps is unqualified: the API documentation states this, and 12a qualifies it before any physical claim.
   Lost enable or stop words keep `UNKNOWN` execution and a stop obligation; nothing is repeated; local cancel is not
   a stop; host loss produces no documented drive action (DS-D32).
7. Observation. `6077` actual torque is read-only (DS-D14), raw 0.1 %. FunOUT.18 "torque reached" is a DO function set
   by P04 (DS2C p10, p17) and unreachable; no completion is claimed for a torque run beyond the stop's own evidence.
8. Capability facts. Torque is available only for models whose manuals document mode 4 (all six manuals). Brake and
   load facts are caller-declared; the profile never assumes a free shaft. With a declared holding brake, brake
   release on enable is drive-sequenced by inference only (08, DS-D26), so a torque start on a loaded or vertical
   axis is the application's responsibility and the API documentation says so.
9. Torque limits elsewhere. `P07-09`/`P07-10` torque limits (300 %), `P07-40` speed-limit window and `P0A-04` overload
   gain (DS2C p18, p21) are unreachable; the drive may limit torque below `6072` for reasons the host cannot read.
   `6077` reports what the drive measures, not why it differs from `6071`.

## Deliver

`Torque.h`/`Torque.cpp` with `TorqueContext`; `test/lichuan_ds2_torque_test.cpp` covering both speed-limit objects;
owner-backed `ds2 torque` with API/CLI parity; ledger `write_policy` for `6071`, `6072`, `6087`, `607F` (per scope)
and `2406h` (withheld), and the coverage rows naming the models the torque operation admits today (ISL only, pending
02's confirmation) and the evidence each withheld model needs;
a DS2 torque section in the coverage file and the API guide, Doxygen with units and limits, CHANGELOG and the backlog
entry for a common torque intent.

## Acceptance

Tests cover each model's speed-limit object: zero changing TX (mode select included) for DS2 and DS2-Mini while the
`607F` width is unresolved and for IBL while the `2406h` write is withheld, also with a readable `2406h` and an
otherwise admissible outcome; the exact four-byte `607F` download for ISL; `2406h` refused outside IBL; CONTRADICTED
(another group's pair and 0x1000/0x0001) and no current outcome refusing the whole operation;
range and sign checks for every field, missing caller inputs rejected before TX, target-then-enable order, run expiry
yielding one stop, the run-budget cases of 06, lost enable and lost stop, a priority stop, deadline cutoff and no
replay. Lichuan C behavior and the common headers' meanings are
unchanged. Quick and full verification and required CI on the exact
final pushed commit pass. Hardware NOT TESTED. Commit, push and hand off.
