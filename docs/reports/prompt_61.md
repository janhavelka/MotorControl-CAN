# Prompt 61 report: node setting, bitrate procedure, command-PDO maps and save trials

State: In progress; stage (a) complete, stage (b) next. Run by the unattended Claude loop of 2026-11-23
([COMMON "Owner absence", Schedule](../prompts/COMMON.md#owner-absence-20-november-to-about-4-december-2026)).

## Starting point

- Starting revision: `1766583e` on `main` (equal to `origin/main`).
- Prompt: [61_drive_configuration.md](../prompts/61_drive_configuration.md), read in full.
- Consumed artifacts: prompt 38 (Partial: heartbeat commissioning, node mutation open), prompt 39 (Partial: `prepareSave`,
  `prepareFactoryRestore`, `verifyPersistence`, Q15 device effects unknown), prompts 41 and 42 (Complete: device and
  motion console routes), prompt 57 (the optional save and restore guards, the S2 save route `Ts`). Prompt 18's TPDO1
  artifacts are consumed where they exist; this prompt does not depend on its Q17 gates.

## Source audit (stage (a), 2026-11-27)

| Sibling | HEAD and state | Behavior adopted or rejected (path, symbol) |
| --- | --- | --- |
| MotorControl-RS | `403eed08` (2026-11-27), clean; the persistence seam unchanged since prompt 57 (last change `f894e43`, 2026-11-19); the communication seam read here first | Adopted: `prepareCommunicationConfirmation` (`include/MotorControlRS/profiles/ess_rs/Communication.h:107`, `Communication.cpp:110-128`): read-only, after a terminal write, at most two slots, the exact before or requested candidate with its own deadline, no scan, switch or replay, the application switches the host first; an echo on the old address is not activation (`Communication.cpp:192, 211`, `ADDRESS_ACK_UNRESOLVED`); `CommunicationPrerequisites` (`Communication.h:44`) as the shape of explicit stationary, effect and route-back qualification. Rejected: `save=REQUIRED` for an address change and `restart=REQUIRED` for baud (`Communication.cpp:89-98`; ESS source facts, no CL86-C source states either, so MCC keeps them UNKNOWN); the mandatory complete backup for a restore (`Persistence.h:30-122`; MCC keeps prompt 57's optional settable guard, the user's policy of 2026-11-17); the application's fixed limit of two saves (`examples/probe_cli/PersistenceApp.h:7`, `PERSISTENCE_MAX_INVOCATIONS`; MCC uses an optional per-node cap, 4 for the campaign); the counter outside the library is adopted |
| FieldCore-node | `453db51f`, clean, unchanged since prompt 60 | Adopted: node, bitrate and saves are external commissioning the application owns; nothing runs at startup (`include/TunnelMonitor/device/Rs485MotorModule.h:10-14`, "Commissioning supplies these facts explicitly"; `src/devices/rs485_motor/Rs485MotorModule.cpp:113-116` refuses an address or baud change at runtime). It never calls a save or a communication setter and has no CAN integration |

## Baseline

- Persistence (`lichuan_c/Persistence.h`): optional guards (`:33-41`), `PersistenceReview::backupRecord` as a `uint32`
  id only (`:59`), `PersistenceRecord` with identity, state and `6081`/`6083`/`6084` (`:73-81`), `validatePersistenceReview`
  (`:165`), `prepareSave` (`:189`), `prepareFactoryRestore` (`:212`), `next`/`note`/`advancePersistence` (`:223-254`),
  `verifyPersistence` (`:265`); the caller keeps `attemptOrdinal`.
- Communication (`Communication.h`): heartbeat only (`prepareCommunication` `:216`, `prepareCommunicationConfirmation`
  `:231`); `CommunicationGroup::NODE_SETTING` read (`:19`, `prepareCommunicationRead` `:67`); no node, bitrate or
  route setter (`:113-116`).
- PDO configuration (`PdoConfiguration.h`): TPDO1 only (`prepareTpdo1Configuration` `:156, :205`, the `next`/`note`/
  `advancePdoConfiguration` engine `:226-262`, `getTpdo1Configuration` `:276, :291`). `canopen/Pdo.h` decodes
  (`decodePdo` `:126`, `PdoObject::rpdo` `:49`) but has no encoder; the owner transmits only SDO, NMT and SYNC
  (`adapters/owner/CanBusTypes.h:214-218`); TWAI allows one `start` per adapter lifetime (`TwaiAdapter.h:142-153`).
- Examples: `ProbeCommissioning.h` (`PersistencePlan` `:20`, `armPersistence` `:80`, `requestPersistence` `:101`,
  `startPersistence` `:117`); the console `persistence` route (`ProbeConsole.cpp:137`, `:2616-2694`) counts attempts
  once per console, not per node, with no cap (`persistenceAttempts_`, `:2689`); the S2 save route `Ts`
  (`BenchRoute.h:240-250, :417`, `SaveWork` `:775`, `STEP_SAVE` `:1255`) needs `--persistent`, is capped by the shared
  `RouteCaps::settingsPerCase` (3 in `campaign()`, `:699-712`), reads no `2300` before or after, and has never run; no
  factory-restore, node or RPDO route exists; `scripts/s2_bench.py` has no save command.

## Evidence (stage (a); original pages read as images, EDS rows)

| Fact | Source | Note |
| --- | --- | --- |
| `2300:00` U16 RW: 0 no effect, 1 restore factory, 2 save; "When 2 is written into 2300h-00 through SDO, the current parameters will be saved" | EN86/ZH86 p18 table, p15; EDS `[2300]` (default 0, PDOMapping=1) | Completion, reboot, the saved set and endurance are not stated anywhere; p20: LED 4 flashes per 5 s = memory read/write error. OP86 p13 carries a CL57-C header and is not independent |
| `2203` "CAN ID" U16 RW 0..127 | EN86/ZH86 p18; EDS `[2203]` (default 0, PDOMapping=1, no limits) | DIP SW1-5 select 1..31, all OFF = user-defined (p10); DIP precedence and activation not stated; p3 "Max 32 axes" |
| Bitrate by DIP SW6/SW7 only: 125k ON/ON, 250k OFF/ON, 500k ON/OFF, 1M OFF/OFF; SW8 termination | EN86/ZH86 p9 | No bitrate object; EDS `LSS_Supported=0` and `BaudRate_50=1` with no DIP setting (C20) |
| RPDO `1400`-`1403` (COB-ID default `$NODEID+0x200/0x300/0x400/0x500`, type 255, inhibit, event timer 1000), `1600`-`1603` (0..4 entries) | EDS | Neither manual mentions `1400`, `1800` or `1017` |
| Writable objects the EDS marks PDO-mappable that must never enter an RPDO here | EDS | Configuration and commands `2203`, `2300`, `2301`, `2302`, `2200`-`2202`; I/O `2400:01..10`, `2401`-`2407`; tuning and mode `2500`-`2520`; motion `6060`, `6098`, `607A`, `607C`, `607D`, `6081`, `6083`, `6084`, `6099`, `609A`, `60FE`, `60FF`. Access conflicts: EDS `2100`-`2102` RW against manual RO; EDS `6060` WO and `6061` RO against EN86 p16 RW |

Bench records: no `2300` download was ever sent; `2203` was only read (17 uploads, all 0); writes of `1017`, the
TPDO1 map, `2400:01` and `6083` were lost at a power cycle without a save; the live RPDO1 map is
`6040`/`6060`/`6098`/`607A` (RPDO2-4 empty, event timer 1000) and no RPDO was ever sent; DIP settings are the user's
report (SW1 on node 1, SW2 on node 2, SW6/SW7 OFF = 1 Mbit/s), never photographed, while `2203` reads 0.

Correction: the prompt's facts block and the configuration evidence report say no bench route exists; prompt 57 added
the S2 save route `Ts` (above). Stage (b) corrects the prompt text with the old claim, the evidence and the
replacement.

## Trials under the owner-absence rules

| Trial | Disposition | Reason |
| --- | --- | --- |
| S0 (reads and a passive watch) | Allowed unattended | Reads only; decided in stage (d) once the bench reads exist |
| S1 (save), S2, S3 (power cycle, read back, restore the old value and save) | Deferred to the owner's return | A `2300` write is persistent; a power cycle is the owner's (README rows "all", "39, 57, 61") |
| S5 (factory restore) | Deferred; the owner decides | Persistent and destructive (README row "Prompt 61") |
| N0 (DIP attestation), N1 to N3 (node write, activation, DIP change) | `2203` reads allowed; the rest deferred | A node write changes communication and its activation is undocumented (README row "38, 61") |
| B1 (bitrate by DIP) | Deferred | Physical DIP change and power cycle (README row "Prompt 61") |
| R1 (configure RPDO4), R2 (an RPDO `000B` to a released and an enabled stationary drive, then the event-timer reaction) | Deferred | PDO reconfiguration and RPDO frames change communication (README row "51, 54, 61"; R2 is added to it in stage (b)) |

## Lead decisions (stage (a))

1. The prompt ends Partial: this host-only stage delivers the software, tests and documents; every trial except S0
   stays NOT RUN with its owner step recorded.
2. Saves: the library keeps no counter (as RS); the examples count saves per node with an optional settable cap, 4 for
   the bench campaign (the prompt's value; prompt 57's shared `settingsPerCase` of 3 is replaced by a per-node save
   cap); the bench runner sums saves across runs from its retained ledger. `Ts` reads `2300` before and after its
   write; `2300=0` is never written; no retry.
3. The backup before a factory restore is a record of the typed groups actually read, with its unknown fields listed
   and `complete` always false; after a restore the examples compare, re-apply only fields with typed writes and report
   every other changed field as changed. The guard stays optional and settable (prompt 57).
4. Node setting: `prepareNodeSetting` (1..127; refuses 0 and any node already configured on the interface) needs a
   released drive, the operator's DIP attestation record and a recorded route back; one `2203` write with readback on
   the old route; confirmation by RS's two-slot exact-candidate rule; an acknowledgement on the old route is not
   activation; activation and whether a save is needed stay UNKNOWN; never a scan.
5. Bitrate: the DIP procedure is documented; the examples get a host reopen path (adapter shutdown, a new adapter and
   owner epoch, since TWAI starts once per lifetime); 50 kbit/s is refused; no drive-side setter.
6. Command PDOs: `prepareRpdoConfiguration` reuses the existing PDO configuration engine (overloaded `next`, `note`,
   `advancePdoConfiguration`), starting with a controlword-only map (`6040` alone); the allowlist admits only `6040` in
   this prompt and refuses every object listed in the evidence table and all `1xxx`; COB-ID collision checks against
   every configured identifier of every node on the interface; the measured TPDO1 write order with full readback.
7. RPDO send: a one-shot host RPDO transmission kind in the owner, admitted only for a route whose confirmed map is
   controlword-only, with the urgent SDO quick stop as the fallback on any problem; no motion words by RPDO. This is a
   cross-module owner and adapter change, so its slice gets full verification and CI.
8. Console commands for each call the API only; the bench routes stay explicit and capped.

## Implementer brief for stage (b)

One implementer (Opus, xhigh) owns all slices; the lead verifies each slice on a short clean worktree and commits it.

- b1 Persistence: per-node save counter with the optional cap (examples), the backup record with unknown fields, `2300`
  reads around `Ts`, a factory-restore bench route with the guard set, the compare and typed re-apply helper, runner
  lines; host tests (ACK, abort, lost reply, boot-up during a save, cap set and clear, guard set and clear, a backup
  never reported complete, never `2300=0`, no retry).
- b2 Node setting: the API of decision 4 (`prepare`, `next`, `note`, `advance`, confirmation, typed result), refusals,
  console line, host tests.
- b3 Bitrate: the DIP procedure in the commissioning guide, the example reopen path, the 50 kbit/s refusal, tests.
- b4 RPDO configuration: decision 6, allowlist and collision refusals, tests.
- b5 RPDO send: decision 7 in the owner and adapters, console, tests (refused without a confirmed controlword-only map).
- b6 Documents: commissioning, persistence and CANopen services guides, CLI contract, coverage, backlog (D09, M09) and
  device conflicts (Q13, Q15 to Q17), CHANGELOG, Doxygen; the prompt's bench-route correction; R2 in the deferred
  table.

Stage (c): a stronger reviewer (Fable, high) challenges the save, node-change and RPDO allowlist rules; a second
reviewer checks the code and the owner change.
