# Configuration evidence: persistence, node ID, bitrate, RPDO, gains and currents

Date: 2026-11-12. Research for three user requests (quoted in each section), plus one
documentation section in [tuning.md](../tuning.md). Checkout `main` at `2c5d832`. No build,
no hardware, no CAN traffic and no Git change was made for this report. Vendor tools were not
obtained or run.

Labels used below: **documented** (an original vendor PDF page or EDS line says it),
**measured** (a retained bench record on our two CL86-C specimens, one firmware),
**inferred** (my reasoning, not stated anywhere), **unknown** (no source and no measurement).

Sources reopened for this report. Pages marked (image) were rendered and read as images; the
others were checked by text extraction, which is enough for the running text cited from them:

- [EN86](../reference/vendor/86C_CANopen_user_manual_en.pdf) PDF pages 8, 9, 10, 15, 16, 18, 19 and 20 (image); 3, 4 and 5 (text).
- [ZH86](../reference/vendor/86C_CANopen_user_manual_zh.pdf) PDF pages 18, 19 and 20 (image); 8, 9, 10 and 15 (text).
- [OP86](../reference/vendor/CL86C_OL86C_operating_manual_zh.pdf) PDF page 4 (image); 2, 3, 7, 8 and 13 (text). PDF pages 3 to 14 all carry a CL57-C/OL57-C header, so they are not independent 86C evidence.
- A keyword search of all five retained manuals (EN86, ZH86, OP86, ZH57, OP57) for save, restart, software, debugging, RS232, bitrate and station-number wording.
- The [EDS](../reference/vendor/LC_StepDriverCAN_V1.1.eds): `[DeviceInfo]` lines 12-35, `[1400]`..`[1403]` 347-586, `[1600]`..`[1603]` 587-787, `[2203]` 1301-1308, `[2300]` 1310-1317 and `[2500]`..`[2520]` 1559-1822.
- The retained manufacturer [product page](../reference/vendor/CL86-C_manufacturer_product.html) and [download index](../reference/vendor/CANopen_manufacturer_download_index.html) snapshots.

Sibling audit. MotorControl-RS HEAD `5938f1d`, dirty (three `scripts/bench_*.py` files
modified, outside these seams). `Persistence.h/.cpp`, `Communication.h/.cpp` and
`DriverSettings.h` are unchanged since `cd27b12`, the revision prompt 39 recorded. Adopted for
the node-ID proposal: `include/MotorControlRS/profiles/ess_rs/Communication.h`
`CommunicationPrerequisites` (`stationaryQualified`, `effectsQualified`, `routeBackQualified`,
`addressDipOffQualified`) and `prepareCommunicationConfirmation` (at most two explicit
confirmations, on the old or the requested route). RS's address is a Modbus address, so this is a
design precedent, not a CAN fact. Kept for the tuning proposal: `src/profiles/ess_rs/DriverSettings.cpp`
`validateIntermediateCurrentTuple`, which refuses a current write without a qualified ceiling.
FieldCore-node HEAD `5ad9292d`, clean, unchanged since prompt 39. It still rejects runtime address
and baud changes in `Rs485MotorModule.cpp` `applyRuntimeConfig` (lines 115-116).

## 1. Save parameters and restore factory settings

User: "Save and factory restore never ran on a drive - add this to the promptsets todo when the
code is in place (when no prompt implements it - add such prompt)".

### What exists

- **Core API** (prompt 39): [Persistence.h](../../include/MotorControlCAN/profiles/lichuan_c/Persistence.h)
  `prepareSave`, `prepareFactoryRestore`, `nextPersistence`, `notePersistenceTransmission`,
  `advancePersistence` and `verifyPersistence`. One attempt per context. ACK is protocol success
  only. A lost reply keeps the possible effect. `restart`, `rearm` and `completeSavedSet` are
  tri-state and fixed at `UNKNOWN`. Factory restore also needs backup and route-back records.
  `verifyPersistence` gives survival bits only for `6081/6083/6084`, and only after an
  independently recorded drive power cycle with no intervening writes.
- **Typed 2300 read**: `DriverReadGroup::PERSISTENCE` in `DriverSettings.h`, outside DEC-1, so each
  upload needs a one-use review grant.
- **Host tests**: `test/persistence_test.cpp` (4515 checks) and
  `test/configuration_restore_test.cpp` (7008 checks), per [prompt 39](prompt_39.md). The console
  bridge is tested in `test/probe_commissioning_app_test.cpp`.
- **Console route** (prompt 41): `persistence save parameters [TIMEOUT_MS]` and
  `persistence restore factory settings [TIMEOUT_MS]` (`examples/common/ProbeConsole.cpp`
  lines 133-138). Each consumes a `PersistenceContext` that the application has already prepared
  and armed through `ProbeCommissioning::armPersistence`. "The ordinary console has no
  arming/grant command" ([cli_contract.md](../cli_contract.md), "Explicit reviewed commissioning").
- **Bench path: none.** No firmware image attaches `ProbeCommissioning`. Only the tests call
  `attachCommissioning`. The S2 bench has no persistence line, while heartbeat has its
  `HeartbeatCommission` (`H`) route. The 2026-11-11 night soak states "Persistence (prompt 39) was
  not run: save and factory restore stay outside the authorization"
  ([report](2026-11-12_night_soak.md)).

So the code is in place for a host application. A bench run still needs a new bench route that
acquires the prerequisites and arms the context.

### Documented

- `2300:00` is U16 RW with EDS default 0 and `PDOMapping=1` (EDS lines 1310-1317).
- EN86/ZH86 PDF p18: 0 noneffective, 1 restore factory settings, 2 save current parameters.
- EN86/ZH86 PDF p15 and OP86 PDF p13 (57C header): "When 2 is written into Index 2300h-00 through
  SDO, the current parameters of the drive will be saved. This operation is often used to save
  parameters such as homing velocity, acceleration/deceleration and homing mode." These are
  examples, not a list.
- `1010`/`1011` are absent from the EDS (gap `100B..1013`). No generic save/load signature applies.
- EN86/ZH86 PDF p20: LED pattern "four flashes every 5 s" is a memory read/write error,
  "Drive damaged, replace the drive", listed as resettable.

### Measured (related, not a save)

With no save sent, these writes were gone after one drive power cycle, one cycle each:

- `1017` raw 1000, on node 1 on 2026-11-02 and on both nodes on 2026-11-09 ([commissioning](../commissioning.md), "Measured on the bench, 2026-11-09").
- The TPDO1 map3-to-map2 change, on node 1 ([checkpoint](2026-11-02_nmt_pdo_checkpoint.md)).
- The `2400:01` polarity write, on node 1, trial 403 ([2026-11-04](2026-11-04_bench.md)).
- `6083=6084=500` on node 2. It read 500 after a move on 2026-11-03 (trial 122,
  [report](2026-11-03_unattended_bench.md)) and 100 after the next power cycle (trial 202).

Both nodes power up with `6081=60` and `6083=6084=100` (trials 201 and 202, 2026-11-04).
Inferred: the drive loads a stored set at power-up, and these writes were not in it. Whether
`2300=2` stores them is unknown.

### Unknown

- Completion: whether the SDO reply comes before or after the flash write, and how long the write takes.
- Rearm: whether `2300` reads back 2 or 0 afterwards, and whether a second save needs a 0 first.
- Whether a save or a factory restore reboots the drive (boot-up frame), stops communication or changes the NMT state.
- The saved set: `1017`, `2203`, the PDO records, `2400`, `2500..2520`.
- Flash endurance, and the effect of power loss during a save.
- The factory values. They are not the EDS defaults: the live TPDO1 inhibit and event words and the RPDO1 map already differ from the EDS ([2026-11-06](2026-11-06_pdo_stream.md)).
- Whether a factory restore changes the loop mode `2500`, the current `2502`, the DI functions (limits, stop) or the direction `2200`.

### Who owns the hardware trial

| Prompt | What it says | Owns the trial? |
| --- | --- | --- |
| 39 | Keeps Q15 device effects "assigned here"; no hardware was used | Owns the open evidence, not a bench step |
| 41 | Delivered the console route; console has no arming | No |
| 47 | "Failure/load/persistence ... campaigns belong to prompt 48" | No |
| 48 | Step 3: "Test reviewed persistence and restart behavior ... Count every save and cap the count" | Partly: names the topic, has no bench route or concrete steps, and waits for 47 (Pending, behind 44 and 45) |

No prompt implements the bench route or a concrete trial. The user's condition for adding a
prompt is met. Proposal P1 below. The lead can then make 48 step 3 consume P1's results.

### Planning constraint

A survival check needs a drive power cycle. On 2026-11-10 the user said not to ask for power
cycles ("dont ask me to switch on/off the drives"). So P1 must either wait passively for the next
natural power-up (the S2 PDO watch sees the boot-up frames and sends nothing), or batch every
power-cycle check into one request that the lead puts to the user. An NMT Reset Node is not a
substitute: its parameter-reload effect is unknown, `verifyPersistence` rightly does not accept
it, and no retained record shows one sent to these drives.

### Proposal P1: persistence bench route and Q15 trials

Prerequisites: prompts 39, 41, 38 (for the `K` backup reads) and 20 (motion-parameter settings),
plus the S2 bench image. P1 does not depend on 47.

Implementation:

1. A bench route `PersistenceBench` in `examples/s2_bench`, built like `HeartbeatCommission`. It
   takes the reservation, then runs the fresh prerequisite reads (identity, STATUS/MONITORS,
   MOTION_PARAMETERS) and one `2300` read, each under its own one-use grant. It calls `prepareSave`
   or `prepareFactoryRestore` with the exact review records, calls `notePersistenceTransmission`
   before handoff and `advancePersistence` on the terminal, then reads `2300` once more. It prints
   ACK or abort, the admission-to-ACK interval (receive bounds), `attempts`, `attemptOrdinal`,
   `effectsPossible` and `uncertain`. No retry, no 0 write, no NMT, no power cycle.
2. A per-specimen save counter kept by the runner on the host, passed as `attemptOrdinal`. The
   runner refuses past the campaign cap. Suggested cap: 4 saves per node per campaign. This is
   project policy, not a device limit.
3. Backup for factory restore: the `K` 95-word read plus the driver, I/O, control, tuning,
   identity and state groups, written to the campaign directory. The runner refuses a factory
   restore without a complete backup of the same node and firmware from the same session.
4. A verification line: fresh post-power-up reads, then `verifyPersistence` with the power-cycle
   record (the boot-up capture or the user's relayed statement) and a no-write record taken from
   the runner's TX log.
5. Decide whether the bench console also attaches `ProbeCommissioning`, so that
   `persistence save parameters` works there. Optional; the bench line is enough.

Host tests:

- Replay tests through the fake owner: ACK, abort, lost reply (uncertain, never a second save),
  boot-up while waiting, deadline, refusal with an alarm or running bit, refusal without a backup.
- The `2300` readback is kept raw. No 0 write. No retry.
- `verifyPersistence` with and without a power-cycle record. Re-applied values never count as survival.
- Runner: the counter survives runs, the cap refuses, and a factory restore without a backup refuses.

Bench trials. Node 1 first; node 2 only after node 1's result is known.

- **S0, baseline.** Drive powered, released, stationary, no alarm (`2100` bits 2 and 3 clear),
  Pre-operational. Read identity, `6081/6083/6084` (expect 60/100/100), `2300`, `1017` and `K`.
  The passive watch runs throughout (boot-up, EMCY, heartbeat).
- **S1, save.** Write `6083` from 100 to 200 through the typed setting, with readback. Save once.
  Record the ACK and its interval. Read `2300`. Watch 30 s for EMCY, boot-up or a heartbeat gap.
  Read `6083` again.
- **S2, survival.** At the one power cycle (see the planning constraint), wait for boot-up. Read
  identity, state, `6081/6083/6084`, `2300` and `1017` before any write, then run
  `verifyPersistence`. `6083=200` means the save took effect; 100 means it did not, or was not
  stored. Record either result; never repeat the save to "fix" it.
- **S3, return.** Write `6083` back to 100 and save once more (count 2). Check it at the next power
  cycle; this can share node 2's S2.
- **S4, saved-set probe (optional, separate review).** Before one save, also set `1017` to 1000
  with the existing workflow, then see whether it survives the power cycle. Blocker: there is no
  typed `1017` 1000-to-0 transition. A saved 1000 could not be written back. Add the reverse
  transition first, or accept a permanent 1 s heartbeat.
- **S5, factory restore (last, separate decision).** Only after S1 to S3, with a complete backup,
  on a released drive. The route back is the DIP position (node 1, 1 Mbit/s). It is inferred to
  survive, because the bitrate is DIP-only and these drives answer at their DIP node while
  `2203=0`. Write `2300=1` once. Watch for boot-up and EMCY. Re-read everything and compare with
  the backup. Re-apply only the fields that have typed writes. A changed field without one
  (`2200`, `2201`, `2502..2520`, the PDO maps other than TPDO1, `1017` other than 0 to 1000)
  stays changed. The library cannot restore it. So the user must accept that risk, or the vendor
  tool must be available, before S5 runs.

Stop rules, every trial:

- Never save or restore while the drive is enabled or moving.
- After a lost reply, do not repeat. Keep the result uncertain and reconcile by reads only.
- On an abort, record the full code and stop.
- On an unexpected boot-up, an EMCY, a `2100` alarm, or a `2300` readback other than 0 or 2: stop. Do not clear faults. Re-baseline before going on.
- Never write `2300=0`, unless a trial shows the drive needs it; then it becomes a visible step.
- Never use a factory restore as test setup.
- If nothing answers on the DIP route after a restore: listen passively, send no scan with writes and no broadcast NMT, and tell the user. That is a failure that needs them.

## 2. Node ID, bitrate and command PDOs

User: "No node ID, bitrate or command-PDO setup - is this somewhere in the roadmap? If not add it
and make prompt/s for it".

### Node ID

- **Documented.** `2203:00` "CAN ID" is U16 RW, EDS default 0, `PDOMapping=1` (EDS lines
  1301-1308). Its range is "0~127" (EN86/ZH86 PDF p18).
- **Documented.** DIP SW1..SW5 select node 1..31, weights 1, 2, 4, 8, 16. All five OFF is
  "User-defined" (EN86/ZH86 PDF p10; OP86 PDF p8).
- **Inferred, not stated:** "User-defined" means the node comes from `2203`. That would make
  nodes 32..127 reachable only through `2203`. EN86 p3 claims "Max.: 32 axes".
- **Measured.** Both drives answer on `0x581`/`0x582` while `2203` reads 0, with SW1 or SW2 ON as
  the user reported ([intake](2026-11-02_s2_bench_intake.md)). That is consistent with DIP
  precedence when any switch is ON. It does not prove precedence.
- **Unknown.** Activation (immediate, NMT reset or power cycle). Whether `2203` needs a save.
  What all-OFF does with a stored 0. Whether the `$NODEID`-based PDO, EMCY and SDO COB-IDs follow
  a new node ID.
- No object reports the DIP position. An all-OFF state can only be attested by the operator.
- **Library today** (prompt 38): `CommunicationGroup::NODE_SETTING` reads `2203` raw. It ran on
  both nodes on 2026-11-09. There is no setter. `CommunicationRequest` says "no bitrate, route,
  node or persistence setter" (`Communication.h`). `2203` stays Q16 gated.

### Bitrate

- **Documented.** SW6/SW7 select 125k (ON/ON), 250k (OFF/ON), 500k (ON/OFF) and 1M (OFF/OFF)
  (EN86/ZH86 PDF p9; OP86 PDF p7). No object sets the bitrate in either manual or the EDS.
  `LSS_Supported=0` (EDS line 31).
- **Conflict C20.** The EDS sets `BaudRate_50=1`, but no DIP setting gives 50 kbit/s.
- **Unknown.** Whether the drive reads the DIP switches only at power-up.
- **Measured.** The fixture runs at 1 Mbit/s (SW6/SW7 OFF, user-reported).
- A bitrate change is therefore physical work: every drive's switches, the host controller, and
  the power cycle the drives may need. It is not a library setter. The library's part is the
  host side: reopen the controller at the new bitrate, listen, then probe the configured nodes.
  Prompt 37's discovery already uses the configured bitrate and never changes it.

### Command PDOs (RPDO)

- **Documented, EDS only.** `1400..1403`: `:00` RO U8 5; `:01` COB-ID RW U32, default
  `$NODEID+0x200/0x300/0x400/0x500`; `:02` type RW U8, default 255; `:03` inhibit RW U16,
  default 0; `:04` RW U8; `:05` event timer RW U16, default 1000. EN86 PDF p15-16 do not list
  `1400..1403`.
- **Documented, EDS and EN86 PDF p15-16.** `1600..1603`: `:00` RW U8 with limits 0..4; `:01..04`
  RW U32. EDS defaults: 1600 maps `6040`, `60FF`, `6060`; 1601 maps `607A`, `6081`; 1602 and 1603
  are empty. `Granularity=0x08`, four RPDOs, no dynamic channels.
- **Documented.** The EDS marks almost every object PDO-mappable, `2300`, `2203`, `2301` and
  `2302` included. A reviewed allowlist must therefore refuse them. The EDS flag is no permission.
- **Measured, both nodes** (2026-11-06 and 2026-11-09). RPDO1 to RPDO4 are valid at
  `0x200..0x500+node`, type 255, inhibit 0, event raw 1000. RPDO1 maps `6040:00` (16 bits),
  `6060:00` (8), `6098:00` (8) and `607A:00` (32), so a frame on `0x201` could command motion.
  RPDO2..4 are empty. No RPDO has ever been sent.
- **Unknown.** The RPDO event-timer meaning. CiA 301 describes deadline monitoring (general claim,
  service unconfirmed), so a drive might react after RPDOs stop. Whether a remap needs
  Pre-operational. Persistence.
- **Library today.** Prompt 38 reads all 16 records (40 mapping and 48 communication fields).
  Prompt 18 writes only TPDO1: map3 to map2, and the `1800:05` event timer raw 0/100
  ([PdoConfiguration.h](../../include/MotorControlCAN/profiles/lichuan_c/PdoConfiguration.h)).
  The owner sends only SDO and NMT frames (`TransportTxId::Kind` in `adapters/owner/CanBusTypes.h`).
  The host cannot send an RPDO.

### What the roadmap has

- Prompt 18 (Partial): its objective is general remapping, but only TPDO1 exists. Backlog M09
  ("Review a controlword-only RPDO map before any RPDO start or stop") and F16 ("Deliver changes
  by RPDO", design only) name it as owner.
- Prompt 51 (Pending): "no RPDO is sent before a reviewed controlword-only RPDO map exists (backlog
  M09 ...)", and "this prompt must not depend on 18". Its trials (a2) and (c) send RPDOs, but no
  step delivers the RPDO map configuration.
- Prompt 53 (Pending): the shared quick-stop RPDO M16 needs a controlword-only map and one shared COB-ID.
- Prompt 38 (Partial): "explicit typed workflows for supported node/communication settings".
  Node-ID mutation is left Partial under Q16 with no step and no trial. Bitrate is "a bench step,
  not a software change".

So node ID and RPDO configuration are named but not planned. Bitrate correctly has no software
setter, and has no procedure either. Three proposals follow.

### Proposal P2: node setting (2203) change

Prerequisites: 38, 37 and P1 (only if a save turns out to be needed).

Implementation:

1. A typed `NodeSettingRequest` (candidate 1..127) and `prepareNodeSetting` in `Communication.h`.
   CL86-C only. Refuse 0, anything over 127, and any node already configured on the same bus.
   Required inputs: the reservation; a fresh `2203` baseline; a fresh clean STATUS/MONITORS read
   (not running, no alarm); the drive released (project policy); and records for the fixture,
   firmware, procedure, authorization, DIP attestation (all-OFF, or 1..31) and route back. This
   follows RS `CommunicationPrerequisites`.
2. One `2203` download, then a `2203` readback on the old route at the next configuration epoch.
   The result is a stored value only. Activation, restart and save stay tri-state `UNKNOWN`.
3. `prepareNodeSettingConfirmation`, as RS `prepareCommunicationConfirmation`: at most two
   explicit identity uploads, on the old or the new route, each with its own deadline, after the
   application has rebound its host channel. Never a scan. Never a write on the new route. A reply
   on the old route never proves activation.
4. Host rebinding stays explicit application work. Invalidate the old binding and its PDO routes.
   After activation, read all 16 PDO COB-IDs, `1014` and `1200:01/02`, because their `$NODEID`
   relation is unknown.
5. Nothing at startup, discovery, selection or recovery touches `2203`.

Host tests: the candidate matrix; a missing DIP attestation or route-back record; a node
collision; a lost ACK; an ACK followed by silence; a reply on the new route before any restart
(recorded, never adopted); a boot-up on the candidate node; confirmations using up their two
slots; no retry, scan or automatic rebind.

Bench trials. Node 1 only, attended for the physical steps.

- **N0.** The user attests or photographs both drives' DIP switches. Read `2203` (measured 0).
- **N1, no physical action.** With the DIP still at node 1, write `2203=5` (unused) and read it
  back. Check that node 1 still answers on `0x601/0x581`. Send one probe upload to node 5 (no
  write); expect no answer. At the next power cycle, re-read `2203` to see whether it survived
  without a save.
- **N2, physical, separate decision.** Save first if N1 showed it is needed (counts against P1's
  cap). Power off; the user sets all five node switches OFF; power on. Listen passively for boot-up
  on `0x705`, `0x701` or anything else. Then one probe on the candidate, identity reads and the
  COB-ID reads. No motion.
- **N3, restore.** Power off, DIP back to node 1, power on, confirm on node 1. Write `2203=0`, and
  save if N2 saved.
- **Stop rules.** If neither the old nor the candidate node answers after power-up: stop and put
  the DIP back. No scan with writes, no broadcast NMT. Never change `2203` during motion.

### Proposal P2b: bitrate commissioning procedure (documentation plus host path, no setter)

1. Document the physical procedure: set every drive's SW6/SW7, then the host controller at the
   same bitrate, then the power cycle. Refuse 50 kbit/s (C20).
2. Host path: the application closes and reopens its controller at the new bitrate, then runs a
   passive listen and the configured probes. Check that the TWAI adapter can re-initialize at
   another bitrate without an MCU restart. Host tests for a refused or mismatched bitrate.
3. Optional attended trial B1: every drive at 500 kbit/s, host at 500 kbit/s, passive listen and
   probe, then back to 1 Mbit/s. Low value on this fixture; run only if a user needs it.

### Proposal P3: RPDO configuration, controlword-only map first (M09)

Prerequisites: 17, 18 (the TPDO1 lifecycle to generalize), 38, 24.

Implementation:

1. Generalize `Tpdo1ConfigurationContext` into a reviewed RPDO lifecycle. Do not build a second
   engine. Requests: `MAP_CONTROLWORD_ONLY` (count 1, `6040:00` 16 bits, COB-ID and type 255
   kept, inhibit and event raw kept), `DISABLE` and `ENABLE` (`:01` bit 31), and later a COB-ID
   change from a reviewed list for M16, with collision checks against every configured node's
   SDO, PDO, EMCY, heartbeat, NMT and SYNC identifiers.
2. Allowlist: `6040:00` only at first. Refuse `2300`, `2203`, `2301`, `2302`, `25xx`, `1xxx` and
   every other unreviewed object, even where the EDS marks it mappable. Total at most 64 bits.
3. Order. This is the CiA 301 procedure (general claim, service unconfirmed on Lichuan), in the
   same order as the measured TPDO1 change: in Pre-operational, set `:01` bit 31, `1600+k:00=0`,
   the slots, `:00=n`, clear bit 31; then read back `1400+k:01/02/03/05` and `1600+k:00..04`. Keep
   the TPDO1 rules: withdraw dependent maps before possible TX, record `failedStep`, no rollback,
   no replay.
4. Baseline: the exact measured live records. The RPDO1 map is `6040/6060/6098/607A`; RPDO2..4
   are empty.
5. Host RPDO send. The owner has no RPDO TX kind. Either P3 adds a one-shot explicit RPDO
   transmission (exact COB-ID and payload, application-timed, TX evidence kept apart from drive
   effect) and prompt 51 then reuses it, or P3 stops at configuration and 51 delivers frames.
   Recommended: the one-shot kind in P3, so that M09 and M16 have a tested path.

Host tests: a failure at every step; partial maps; readback mismatch; NMT not Pre-operational;
refused objects; total length; COB-ID collisions; no rollback or replay; samples never qualified;
persistence unknown.

Bench trials. Node 1 first, then node 2.

- **R1, configuration only.** RPDO4 (empty, `0x500+node`) gets `[6040:00 16 bits]`, with readback.
  No RPDO frame is sent. At the next power cycle, check whether it reverted (TPDO1 did).
- **R2, needs the RPDO send.** In Operational, send one RPDO `000B` to a released drive. The SDO
  `000B` on a released drive changed nothing (trial 307), so expect no change. Then send one
  RPDO `000B` to an enabled, stationary drive. The SDO `000B` released such a drive (trials 124
  and 208). Observe `6041`/`2100` by SDO. Then watch at least 2 s for an event-timer reaction
  (EMCY or alarm) after the last RPDO.
- **Stop rules.** Never send `010F`, `001F` or any motion word by RPDO in P3. Never map `607A`.
  If the drive moves or faults, send the existing urgent SDO quick stop and end the campaign.
  Undo with `DISABLE` or the power-cycle reversion; no save.
- **R3.** The shared COB-ID for M16 stays in prompts 53 and 54.

## 3. Gains and currents

User: "Gains and currents are read-only - ok, document it well and document where and how to change it".

### Objects

All 33 entries `2500..2520:00` are U16 RW and PDO-mappable in the EDS (lines 1559-1822), with no
default and no limits. Meanings come from EN86 PDF p19-20 and ZH86 PDF p19-20. The values are the
identical specimen readings of 2026-11-04 (trials 309 and 310).

| Index | Manual meaning | Unit and range evidence | Specimens | Library |
| --- | --- | --- | --- | --- |
| 2500 | Drive operation mode: 0 noneffective, 1 open, 2 closed loop | 0..2 documented | 2 | Typed write; never run on a drive |
| 2501 | Encoder resolution | No unit, no range | 4000 | Read only |
| 2502 | Max. peak current | mA; no range; EN86 p3 maximum output current 8 A | 8400 | Read only |
| 2503, 2504, 2505 | Closed-loop maximum, closed-loop base, open-loop maximum current ratio | %; no base, range or ordering | 100, 40, 100 | Read only |
| 2506, 2507 | Current ratio for locking; locking current time | % and ms; no range, zero meaning or start condition | 40, 200 | Read only |
| 2508, 2509, 250D | Low-pass enable and coefficient, mean filter coefficient | None; "factory default, adjustment not required generally" | 0, 5, 512 | Read only |
| 250A, 250B, 250C | Out-of-tolerance alarm threshold, positioning completion threshold and time | None; same note | 4000, 20, 10 | Read only |
| 250E..2511 | Current loop ratio, Kp, Ki, Kc | None; same note | 6144, 1228, 40, 20 | Read only |
| 2512..2519 | LA velocity Kp1, Kv1, node 1, Kp2, Kv2, node 2, Kvf, position Ki | None; same note | 2560, 114, 25, 2560, 1024, 100, 20, 4 | Read only |
| 251A..2520 | EDS-only names `v_kp` to `v_kacff` | Names only | 12001, 1000, 1024, 1228, 8, 0, 0 | Read only |

New observation for this report: `2502` reads 8400 on both drives, and with the documented mA
unit that is 8.4 A, above the 8 A maximum output current on EN86 PDF p3. No source explains this.
It could be a peak rating, another convention or another unit. Unknown.

No other current object exists. `6073`, `6075` and similar CiA 402 objects are absent from the EDS
(gap `606D..6079`). The DIP switches set node, bitrate and termination only (EN86 PDF p9).

### What the library does and why

`Tuning.h` reads the four groups (`prepareTuningRead`, `getTuning`). Console:
`tuning current-holding|filter-position|loop-gains|eds-gains read`; bench `readiness-tuning`.
Only `2500` has a write (`prepareControlSettings`, console `control set 0|1|2`). No firmware image
attaches the settings session that `control set` needs, and packet 14's T1 has no bench route.
Writes to `2501..2520` are withheld because no source gives an admissible domain for any of them,
and the installed motor's rated phase current is not established
([tuning.md](../tuning.md), "Decisions per subgroup"). RS does the same:
`validateIntermediateCurrentTuple` refuses a current write without a qualified ceiling.

### How a user can change them today

1. **The vendor's debugging software, over the RS232 port.** Documented: EN86 PDF p3 ("Serial
   port debugging: RS232 interface"), p4 and p5 (pinout). EN86/ZH86 PDF p8: DI/DO functions "can be
   configured through the CANopen bus or the upper computer debugging software"; EN86's table there
   has a "MODBUS address (decimal)" column. OP86 PDF p4, a page under a CL57-C/OL57-C header: "CANopen 调试软件可以找销售或售后工程师索要，通讯转接线可以找销售咨询"
   (the CANopen debugging software can be requested from sales or after-sales engineers; ask sales
   about the adapter cable). The retained download index has a 调试软件 (debugging software) menu,
   but its CANopen list names only the EDS and the manuals. Checked: every dictionary row has a
   "Register address", and where both tables list a row they agree (for example `2400:01` is MODBUS
   32 on p8 and register `0x0020` on p18). Inferred: the tool reads and writes those registers, so
   `2502` would be register `0x0052`. Not documented: that the tool edits `2502..2520`, whether it
   saves, or which firmware it supports. The project has not obtained or run it.
2. **A raw SDO download from the user's own code.** The SDO layer can carry one: `SdoTransfer` is
   "not an object-access permission" (`SdoTransfer.h`), and the reference `SdoClient`
   (`adapters/canopennode/SdoClient.cpp`, `checkWork`) admits a download on a confirmed channel.
   But no typed check, test, console line or bench route covers these words. The S2 bench's raw
   `W` line writes only PDO-record fields, and the firmware's download dispatcher needs a typed
   caller's notice. Nothing in this project has written these words. This path is unsupported and
   untested, and not recommended.
3. **DIP switches.** None for current or gains on CL86-C.

### What a typed write needs (per row or group, prompt 22)

1. The manufacturer's range, unit and convention for the installed firmware: peak or RMS for
   `2502` (and why 8400 exceeds 8 A), the percent base and ordering for `2503..2506`, delay or
   duration for `2507`, the scale and safe domain for each gain.
2. The installed motor's rated phase current, from its nameplate or datasheet.
3. A whole-tuple check before any current write, as in RS `validateIntermediateCurrentTuple`.
4. One attended trial per group on one specimen: a single write, readback, a phase-current probe,
   an unloaded shaft, the power cutoff at hand, a return to the original value, then a power cycle
   to learn whether the change was volatile.
5. The same lifecycle as `2500`: whole-candidate validation, withdrawal at possible TX, readback,
   retained uncertainty, no save.

A practical way to get items 1 and 2: ask Lichuan for the debugging software, its parameter
document, and the current settings that fit the installed motor.

## 4. Documentation change

Added "Changing gains and currents" at the end of [tuning.md](../tuning.md): the object table, how
to change each today with the risks, what a typed write needs, and a link to this report. It uses
no in-page anchor links, because `docs/tuning.md` is in the Doxyfile `INPUT`.

## 5. Roadmap coverage

| Part | Prompt | Coverage |
| --- | --- | --- |
| Save/restore core API and host tests | 39 | Fully |
| Save/restore console route | 41 | Fully (needs an externally armed context) |
| Bench route that arms a persistence context | None | Not at all (P1) |
| Save and factory-restore hardware trial | 48 step 3 (39 keeps the evidence) | Partly: topic only, behind 47 (P1) |
| Q15 completion, rearm, saved set, endurance | 39 open, 48 | Partly |
| `2203` typed read | 38 | Fully (run on both nodes) |
| `2203` typed change and confirmation | 38 | Partly: objective only, no step (P2) |
| Node-ID activation trial | None | Not at all (P2) |
| Bitrate software setter | None, correctly | Not applicable: DIP only |
| Bitrate procedure and host reopen path | None | Not at all (P2b) |
| RPDO record reads | 38 | Fully (run on both nodes) |
| RPDO configuration writes | 18 (M09, F16) | Partly: TPDO1 only (P3) |
| Controlword-only RPDO map (M09) | 18; consumed by 51 and 53 | Partly: backlog row only (P3) |
| Host RPDO transmission | 51 (implied by its trials) | Partly: no explicit step (P3 or 51) |
| Shared quick-stop RPDO COB-ID (M16) | 53, 54 | Partly: needs P3 first |
| Tuning reads `2500..2520` | 22 | Fully (read on both nodes) |
| `2500` typed write | 22 | Fully in software; T1/T2 owed (H03), no bench route |
| `2501..2520` writes | 22 | Partly: gated rows with named next evidence; no step obtains manufacturer data |
| How to change gains and currents today | This report and tuning.md | Fully (documentation) |
| Persisting a tuning change | 39, P1 | Partly |

Numbering. Prompts 55 to 67 are being allocated by the concurrent planning session (62, 64, 65
and 67 are untracked files today). The lead should give P1, P2, P2b and P3 their numbers and
update SEQUENCE.json, PROGRESS, the backlog (Q15, Q16, Q17, M09, D09) and the roadmap together.
