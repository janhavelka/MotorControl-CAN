# 01 — Evidence and command ledger

Execute this stage under the [execution contract](execution_contract.md).
Status: NOT STARTED. The research inputs exist; do not redownload unchanged originals or mistake transcription for an
implemented profile. No hardware is needed.

## Work

1. Run [canopen_shared 01](../canopen_shared/01_existing_code_and_tooling.md) with its section "A non-CANopen CAN
   protocol instead". Record HEAD, dirty state, PROGRESS rows 55, 57, 63, 64, 65, 73, 75 and 77 and the current claims
   of seams S1-S10 in the [shared README](../canopen_shared/README.md) table; open each reused symbol with its callers
   (`Endpoint.h`, `Capabilities.h`, `Units.h`, `Axis.h` `NativeRateBasis`, `Native.h` `RampKind`, `ActionOperation.h`
   `AcknowledgementSupport`/`ServiceEvidence`/`RetryClass`, `can/Frame.h`, `CanBusTypes.h` `Transport`). Refresh
   MotorControl-RS and record adopted or rejected behavior. Note FieldCore-node's CAN consumer boundary read-only.
2. Verify the pack under canopen_shared 01 "Intake rules" (pack): manifest hashes, page counts (MK109 118, MK109A
   106, MK106 60, MK100 43) and the extract page markers, with `python scripts/check_promptset.py` on the working tree
   and `--check-git-ref INDEX` before the commit and `HEAD` after. The checker discovers the manifest; do not edit
   `reference_manifests`. Requiring the pack by name needs a required-pack entry with a negative test. For every row
   below reopen the rendered original page; extracted tables lose columns and DLC cells.
3. Create `docs/reference/mks_servo_can_commands.json`, the family's other-protocol ledger under canopen_shared 01
   "Intake rules": the CANopen ledger's lossless accounting (`key`, `source_refs`, `units`, `scaling`, `effect`,
   hash-guarded `conflicts`, `coverage` with independent `documented`, `implemented`, `host_tested`,
   `hardware_qualified`, and `review` with `read_effect`, `write_policy`, `gate_ids`), plus `manual_only` and an
   `owner_prompt` naming this set's stage IDs, over the protocol's own commands; no EDS, PDO or object-dictionary
   fields. Per function-code row record:
   - `key`, `code`, the manual's name, `kind` (read, setting, action, motion, stop, report, bus, restart);
   - frame format: standard 11-bit data frame, RTR and FD never, big-endian byte order (MK109 p.16, p.30);
   - request layout per dialect: field, byte offset, width, signedness, unit and `scaling`, documented range,
     reserved bits;
   - reply phases (immediate, unsolicited completion, streamed) with every status value's meaning per phase, the
     layout-derived DLC and the printed DLC where they differ (MK-D04..D07);
   - acknowledgement or no-ack and reply suppression: broadcast, group, 8CH, silent mode, 4BH (MK109 p.30-31, p.47,
     p.58, p.107); watchdog and periodic reports where they exist: 98H host-silence time, 01H period (p.50, p.36);
   - persistence evidence: `documented_saved`, `documented_volatile` (only 83H's trailing-00 form, MK109 p.42, and
     MK109A's 0FH, MK109A p.45), `presumed_persistent` (menu equivalents, MK109 p.9) or `unknown` (42H, 98H, 01H, 4AH,
     8CH, 95H, 9DH, 96H/97H and F3H, review section 11); a separate `persistence_policy` handles the last two as
     persistent, and the policy never becomes evidence (42H is `unknown`, handled as persistent; 0FH is
     `documented_volatile` with write policy `never`);
   - `effect` (side effects): motion, shaft release, enable change, coordinate change, restart, communication loss,
     power-on behavior; bus-mode-only flag (MK109 p.78);
   - availability rows `MK109`, `MK109A`, `MK106`, `MK100` (documented, absent, differs) and `since` from the version
     notes (MK109 p.1-3, MKNOTES), recording MK-D03 dates side by side;
   - `source_refs` (source ID and page), `conflict_ids`, `retry_class` and `write_policy` (`never`, `explicit`,
     `explicit_persistent`, `explicit_motion`, `explicit_restart`);
   - `multi_target` per form and dialect: whether that exact form may go to ID 0 or a group ID (09 step 6), with the
     reason and pages; default not allowed. 4BH is reviewed only for 09 step 7.
   Cover every code in review section 4: reads 00H, 01H, 30H-35H, 38H-3BH, 3EH, 40H, 42H, F1H; settings 80H, 82H-89H,
   8AH-8DH, 8FH, 4CH, 0FH, 95H-98H, 9BH, 9DH, 3DH, 3FH, 41H, 42H write, 50H, 36H, 9FH, 99H, 9EH, 90H, 94H, 9AH, 92H, 4AH;
   motion F3H, F7H (both forms), F6H (5- and 8-byte), FFH, FDH, FEH, F4H, F5H, 91H (both forms), 4BH; the documented
   speed-0 stop forms of F6H, FDH, FEH, F4H and F5H (MK109 p.84, p.90, p.94, p.98, p.102), each as its own operation
   with its own statuses (0-2 for F6H, 0-3 for the others), and no invented stop row for any other code; the PID
   read shape `00 96 00` (MK109 p.51). Menu-only functions
   (GoHome, Restore key, About, Next-key restore) and MK106-only behavior (En-triggered homing and stall release, MK106
   p.10, p.33-34) go in `manual_only`.
4. Create `docs/reference/mks_servo_can_operations.json` (schema 2 with Lichuan's keys `families` with `header`,
   `functions`, `tests` and `console`, `ledger_bindings` and `python_plans`; canopen_shared 01 "Intake rules", operation
   map and tools) referencing ledger keys only and copying no device facts, and
   `docs/reference/mks_servo_can_vectors.json` with every worked example frame per manual: page, ID, the bytes,
   printed DLC and check byte exactly as printed (`vendor`, never repaired; a labelled repair is a separate `derived`
   vector), `dialect`, `direction` (request or reply), `use`: `build_parse` (a
   supported transmissible form in that dialect), `parse_only` (replies and observations), `not_built` (documented
   but never written or outside an implemented table: 0FH, 50H, MK106-only layouts) or `malformed` (MK-D07 nine-byte
   frames, MK-D26 RS485 frames), and a check-byte record: `checksum_domain` (`sum8`, `fixed_6b`, or `not_applicable`
   with the reason, as for RS485 frames) and `checksum_expected` (`match`, `mismatch` with its discrepancy ID and
   source, or `not_applicable`). The checker recomputes each vector's check byte in its domain and compares it with
   that record, whatever its use.
5. Seam S7, before the first MKS public header (stage 02), under canopen_shared 01 "Intake rules" (seam S7): claim it
   in the shared README table, or extend the split another family landed. Tools:
   `scripts/check_mks_servo_can_ledger.py`, `scripts/generate_mks_servo_can_commands.py` (`--check` never writes;
   outputs private `Commands.h`, optional `Catalogue.h` and `docs/reference/mks_servo_can_generated.md`; names per
   [README](README.md) open question 2) and `scripts/check_mks_servo_can_operations.py`. Scope
   `check_lichuan_operations.py` to common and `lichuan_c` headers and the MKS checker to `profiles/mks_servo_can/`;
   test that the header sets are disjoint, their union is every public header with public functions, and an
   unclassified common, Lichuan or MKS function still fails. Register each checker in `scripts/verify.py`,
   `scripts/check_repository.py` (`CHECKS`, file lists) and CTest; add LF rules for generated headers to
   `.gitattributes`.
6. Record the unit derivations as inferences with pages, for 05 and 06 to confirm: 1.0.1+ speed code is motor rpm at
   16/32/64 microsteps, a `MOTOR_TURNS` rate basis; MK109 p.78 gives 8 (×2) and 128 (÷8) as examples of the 16/MStep
   rule for other settings, its extension to other powers of two is an inference and non-powers of two are unstated
   (MK-D14), so exact common-unit mapping stays limited to 16/32/64 (review section 16) until a scoped ledger
   resolution; V1.0.0 speed code times
   100 is command pulses per second (from rpm = code × 6000/(MStep × 200), MK100 p.22), a `COMMAND_INCREMENTS` basis;
   1.0.1+ acc gives 1 rpm per (256 − acc) × 50 µs (MK109 p.79); V1.0.0 acc adds acc codes per 10 ms (MK100 p.23); pulses
   are MStep × 200 per turn and encoder counts 16384 per turn (MK109 p.17); angle-error scales 51200, 65535 and 28000
   per turn (MK-D28).
7. Record decisions: profile names; dialect identifiers `V1_0_9_SEP2026`, `V1_0_9_NOV2025`, `V1_0_0` and the declaration
   contract (02 lands the type, 02b binds it); the probe read per dialect with its read effect (40H for the V1.0.9
   tables; for `V1_0_0`, which has no 40H, one documented read such as 30H), which 02b implements; whether MK106
   becomes a fourth table (default: ledger rows only); supported board codes 2 and 4; the
   never-written list; the initial implementable subset; and whether MKS reads may be `REPEATABLE_READ` by a recorded
   risk acceptance (Lichuan DEC-1 does not transfer). Without that owner decision, reads stay `NOT_REPEATABLE`.

### Discrepancy owners

| IDs | Topic | Resolving stage |
| --- | --- | --- |
| MK-D01, D03, D05, D31, D33 | Two V1.0.9 editions, history dates, 40H layout, MK106 headers, missing manuals | 03; 12A Q2 |
| MK-D02 | Direction sign of commands, 31H/35H and 32H | 05, 06; 12A Q4 |
| MK-D04, D06, D07 | Printed DLC and example length errors | 02; 12A Q3 |
| MK-D08 | Uplink check byte in fixed-0x6B mode | 02, 10; 12A Q5 |
| MK-D09 | Bad check byte, wrong DLC or unknown code | 02b, 09; 12A Q14 |
| MK-D10, D34 | Readable set of 00H; FF FF sentinel | 03, 08; 12A Q3 |
| MK-D11 | Scope of 8CH respond-off | 09; 12A Q6 |
| MK-D12 | Repeated 4BH | 09; 12A Q12 |
| MK-D13 | Enable state after power-on or restart | 04, 10; 12A Q8 |
| MK-D14, D15 | Speed scaling outside 16/32/64 microsteps; MK106 600 rpm example | 05, 06 |
| MK-D16, D25, D37 | Left-switch release, Hm_Ma defaults, single-turn zero arming power-on homing | 07; 12A Q13 |
| MK-D17, D36 | Replies after stall, silence or F7H; shared run/stop codes | 04, 09; 12A Q9a |
| MK-D18 | 98H feeding frames, stop type, enable, persistence, signedness | 08; 12A Q7 |
| MK-D19, D30 | Absolute-move zeroing; 31H, 35H and 33H relation | 05, 07; 12A Q8 |
| MK-D20, D22 | CAN ID menu range; 3FH restored scope | 10; 12A Q10, Q11 |
| MK-D21, D23, D27, D28, D29 | Persistence scope, 8FH values, 9DH bit 1, angle scales, 84H value 256 | 08; 12A Q11 |
| MK-D24 | Board revisions and MOSFET ratings | 03, 12B |
| MK-D38 | V1.0.0 39H printed `int16_t` with a 0..FFFF per turn text | 02, 03 |
| MK-D26, D32, D35 | RS485 frames in CAN manuals, silent mode, identical request and reply | 02, 02b, 10 |

## Deliver

- The command ledger, operation map and vector file; the three checkers with registered negative tests in
  `test/mks_servo_can_inventory_test.py`: missing code, tampered conflict hash, altered generated output, unclassified
  public function, promoted gate, a dialect row with no page source, a vector whose recomputed check byte disagrees
  with its recorded expectation, a `mismatch` without a discrepancy ID, a `build_parse` vector for a never-written or
  absent form, a `multi_target` allowance without a page, a persistence class promoted from `unknown` or
  `presumed_persistent` to a documented class without a page, and a public header claimed by two checkers or by none.
  The S7 claim and landed commit, tested interface and consumer gate in the shared README table.
- `docs/mks_servo_can_coverage.md` with one planned row per common operation of contract 02 "Capabilities and the
  common operation mapping" (reads, enable, release, mode select, absolute, relative and angle move, finite velocity,
  homing, each stop kind, fault clear, save, restore): native items by ledger key, staging and trigger order,
  acknowledgement and completion evidence, identity consumed, stop interaction, `RetryClass`, gates and tests, each NOT
  IMPLEMENTED or unsupported with its reason (save: no native save command, MK-D21). Link it from `docs/coverage.md`
  only when code lands.
- `docs/prompts/mks_servo_can/PROGRESS.md` with every stage including 02b, 12A and 12B, software status, a separate
  hardware column, inherited gates and handoff; `docs/reports/mks_servo_can_01.md`.

## Acceptance

Every documented code and form of all four manuals has a ledger row with pages and dialect rows; prose-only functions
are in `manual_only`; every MK-D has an owner stage; no unresolved row is writable merely because a manual lists it.
Every vector's recomputed check byte matches its recorded expectation. Checker negative tests fail as intended;
promptset, ledger and quick verification pass. The check registration is a major change: verify CI on the exact final
pushed commit. No device API, firmware or physical support is claimed. Commit and push the owned files and record the
handoff.
