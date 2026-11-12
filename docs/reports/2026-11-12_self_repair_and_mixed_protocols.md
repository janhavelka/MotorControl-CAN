# Self-repair, retries and mixed protocols on one CAN bus (2026-11-12)

Read-only research on `main` at `2c5d832`. Nothing was built, flashed or sent, and no file other than this report
was changed. The working tree holds another worker's uncommitted edits (tests, `docs/prompts/52_motion_changes.md`,
`docs/prompts/53_multi_axis.md`, `docs/tuning.md`); this report uses none of them. All `adapters/` and other `docs/`
line numbers are at `2c5d832`.

Sibling state, recorded read-only:

| Repository | HEAD | Branch | Dirty state | Seam read |
| --- | --- | --- | --- | --- |
| FieldCore-node | `5ad9292d` | `rodtester` | Clean | RS485 owner, device modules, motor module, guidelines |
| MotorControl-RS | `5938f1d` | `main` | 6 modified bench scripts and tests, outside this seam | Retry and replay rules in `docs/architecture.md` |
| ESP-IDF | 5.5.5 (`C:\pio\packages\framework-espidf\version.txt`) | | Hashes match `docs/adapters/twai_sources.json` | TWAI legacy and node drivers, HAL v1, S2 LL |

The user's four requests, verbatim, are in the task: self-repair of a stuck read/write, a simple procedure for "one
frame no drive answers", a bounded retry/replay/fault-clear function, and non-CANopen traffic on the same bus.

**Marks.** **D**: documented by the cited source. **M**: measured on this bench (report cited). **I**: inference,
our reading or general CAN knowledge with no retained source. **P**: proposal of this report.

## Short answer

1. **A stuck SDO costs an MCU restart today because the only way back is switched off.** Any unclear exchange
   quarantines the channel. The only reopening, the drain marker, needs two premises that default to false and that
   no real fixture sets (Q18). Four restarts were needed on 2026-11-04 (backlog D14). The library also never sends
   the standard SDO abort `0x05040000` on a timeout.
2. **The industry standard is simple.** CANopenNode and python-canopen send the abort `0x05040000` on timeout, put
   the channel back to idle and reuse it at once. They filter late replies by command, index/subindex and toggle bit.
   They silently accept the remaining risk: a late reply for the *same* object can be taken as the next reply.
   **Recommendation:** use that standard, plus two cheap guards we already have code for. Wait a guard time, then run
   the existing drain-marker read of a *different* harmless object, automatically under an application policy. The
   channel then repairs itself in about guard + 5 ms, with no restart. This needs one recorded owner decision: accept
   the CiA 301 premise "one request, one reply, in order" for the default SDO channel (an amendment to DEC-3). The
   Q18 bench measurement then confirms or refutes it.
3. **"One frame no drive answers" happens only when no other node is listening at all.** On CAN every receiving node
   acknowledges every correct frame, whoever it is addressed to. So one missing drive, with another drive present,
   never stalls the bus: its SDO just times out. A no-ACK means all drives are off or the cable is cut. On 2026-11-03
   both drives went silent within 10 ms. The S2 then retried one frame for 4.9 h with TEC fixed at 128, and no bus-off
   (M).
4. **An error-passive no-ACK retransmission never reaches bus-off.** Under ISO 11898-1, an error-passive transmitter's
   ACK error does not raise TEC. This was measured here (TEC 128, REC 0, about 7,250 `ack_err` per second for 4.9 h).
   ESP-IDF's sources do not state the rule. Standard bus-off auto-recovery therefore never fires in this case.
5. **The real hazard is late delivery, not the stall.** While nobody listens, no axis can be reached anyway. But the
   stuck frame is delivered the moment any node returns, possibly hours late, possibly a write. **Recommendation:**
   give every frame a TX lifetime (an application setting). On expiry, an automatic, bounded, logged controller
   restart deletes and recreates the TWAI node; this is the existing recovery code. Restarts back off, and the
   firmware listens for boot-up or heartbeat. Use the same path for a real bus-off (ODrive documents the same
   practice) and for RX ring overflow. No frame is ever resent.
6. **Axes stay independent through per-node presence.** Each node has its own failure streak: DEGRADED after a few
   misses, OFFLINE after more (FieldCore uses 2 and 5). Work for an OFFLINE node fails fast, one slow probe continues,
   and the node is online again at the first success or boot-up. Where `1017` is commissioned, use a per-node
   heartbeat consumer as CANopenNode does. Never reset a node on loss, which is Lely's default.
7. **Retry policy: application-owned and classified per operation, as the RS rule says.**
   - Reads: retry up to two times after a resync.
   - Setting writes: retry only through a readback.
   - Quick stop `000B`: repeating it is safe.
   - Motion starts, enable, homing, save, NMT: never replayed. A relative move of +10 mm resent once becomes +20 mm.
   - Fault clear: optional and off by default, count-limited, and it never enables.
8. **FieldCore's RS485 pattern fits the policy, not the mechanics.** These parts map to CAN: per-device timeout,
   bounded read retries that never extend the deadline, a miss counter, back online on the next success, urgent stop
   first, automatic transport re-init without replay, and a protocol-free owner. These do not: RS485 is half-duplex
   master/slave with time-shared protocols, while CAN is multi-master, with hardware ACK and retransmission and
   separation by identifier.
9. **Non-CANopen traffic can share the bus, and on CAN this is simpler than RS485 time-sharing.** It needs three
   things:
   - An identifier plan checked at configuration. It must be disjoint from every CANopen node's identifiers and avoid
     the CiA 301 restricted IDs. The clean 11-bit block is `0x680-0x6DF`, or use 29-bit frames.
   - An RX route that sends frames in the plan to the protocol module.
   - A raw TX kind ranked below stops.

   The traps are devices that ignore the frame format. moteus ignores the 11/29-bit flag for IDs up to `0x7FFF`.
   ODrive documents only specific node-ID pairs (for example CANopen nodes 32 and up). Whether the Lichuan ignores
   29-bit frames is unknown; one harmless bench test settles it.

## 1. SDO timeout handling

### 1.1 This library today (code read)

**What happens on a timeout**
- No abort is sent. `SdoClient::expire` calls `retire` and then `finish(TIMEOUT)` (`adapters/canopennode/SdoClient.cpp:528-538`).
- The CANopenNode engine's own timers are never advanced. The comment reads "Native per-step timers are intentionally not advanced" (`SdoClient.cpp:569-570`; `docs/canopen_services.md:299-301`). The engine therefore never reaches its own `CO_SDO_AB_TIMEOUT` path.

**When the channel becomes unresolved**
- `finish` marks it unresolved when the request was possibly transmitted and did not settle, on an unwithdrawn transport error, or on reboot (`SdoClient.cpp:508-511`). It also retains the index/subindex pair (`:514`).
- Admission refuses an unresolved channel (`SdoClient.cpp:321-323`).
- Boot-up (`:932-946`), invalidation (`:948-958`) and any frame on an idle, terminal or quarantined route (`:689-703`, "Observed old/unsolicited traffic disproves the external reuse premise") also quarantine.
- The contract says "Release, quiet time, host epochs, a client abort or construction do not requalify it" (`docs/canopen_services.md:373-375`).

**The only reopening**
- The drain marker: `checkDrain` requires `premises.soleClient && premises.orderedServer` (`SdoClient.cpp:324-340`). Both default to false (`SdoClient.h:28-34`). On success, `reopenAfterDrain` (`:353-361`) reopens the channel.
- DEC-3 rule 1 says "The application requests it. Nothing triggers it automatically" (`docs/prompts/PROMPTSET_AUDIT_34_50_2026-11-02.md:219-222`).
- The contract adds: "No retained Lichuan or CiA source establishes the second. ... It stays open under Q18, so no real fixture sets it and real routes stay blocked" (`docs/canopen_services.md:409`).

**What it costs**
- Backlog D14 (`docs/backlog.md:102`): on 2026-11-04 four events each cost an MCU restart:
  - a read cut at a stop's deadline;
  - the client aborts of two-byte `60FE`/`607D` replies (D16);
  - a best-effort stop.
- H04 (`docs/backlog.md:227`) and the limitations report, item 2.5 (`docs/reports/2026-11-12_library_limitations.md:86-92`).

**Timing (M)**
- Measured reply time per SDO step: 4.81 to 5.92 ms, mean 4.89 ms, over 1,377,097 steps (`docs/reports/2026-11-03_overnight_soak.md:30`).
- The bench response timeout is 100 ms (`examples/common/ProbeDevice.cpp:19`).

### 1.2 CANopenNode (pinned `9b8beed8`, v4.0)

Code read; paths are relative to `experiments/canopen_service_fit/vendor/CANopenNode/301/`.

| Topic | What the code does | Evidence |
| --- | --- | --- |
| Timeout | Timer counts the gap since the last frame sent or received, not the whole transfer. On expiry: `abortCode = CO_SDO_AB_TIMEOUT; state = CO_SDO_ST_ABORT` | `CO_SDOclient.c:775-780` |
| Abort frame | `0x80`, index, subindex, code; sent with `(void)CO_CANsend`, then `state = CO_SDO_ST_IDLE` even if the send failed | `CO_SDOclient.c:1009-1020`; code `0x05040000` "SDO protocol timed out" at `CO_SDOserver.h:299` |
| Reuse | Immediate. The initiate functions do not check the state; they overwrite the multiplexer, clear the timer and drop any pending received frame. No guard time anywhere | `CO_SDOclient.c:365-397`, `:1052-1080`, close `:1771-1775` |
| Idle filter | Frames are ignored while idle; only the first unprocessed frame is kept, and an abort overwrites it | `CO_SDOclient.c:62-64` |
| Multiplexer check | Initiate responses must match index/subindex; a mismatch aborts with `0x06040043` | `CO_SDOclient.c:588-599`, `:1218-1229` |
| Toggle check | Segment toggle is checked; a mismatch aborts with `0x05030000` | `CO_SDOclient.c:627-631`, `:1271-1275` |
| Server abort | Any `0x80` ends the transfer, with no multiplexer check | `CO_SDOclient.c:576-581`, `:1206-1211` |
| Late same-object reply | Accepted as the new transfer's result: there is no transfer identity (I, code reading) | `CO_SDOclient.c:604-605`, `:1241-1242` |

### 1.3 python-canopen (pinned `14b3336b`)

Code read in `docs/reference/public_code/canopen_python/canopen/sdo/`.

**Settings:** `RESPONSE_TIMEOUT = 0.3` s and `MAX_RETRIES = 1` (`client.py:23`, `:26`). By default there is one
attempt and no retry.

**On the final timeout:**
- It sends `abort(ABORT_TIMED_OUT)`, which is `0x05040000` (`client.py:97-100`; `constants.py:39`).
- The abort's index/subindex are left at zero, with the comment "TODO: Is it necessary to include index and subindex?" (`client.py:103-108`).

**Late replies:**
- The response queue is flushed only once, before the request (`client.py:88-90`).
- Upload initiate checks the index (`client.py:262-266`).
- Download initiate checks only the command byte (`client.py:367-371`), so a late `0x60` for any object is accepted.
- A server abort is accepted with no multiplexer check (`client.py:80-83`).

### 1.4 Lely and CiA 301

**Lely:** no source exists in the repository and none was found on disk. Its SDO timeout handling is not
established here. Only its heartbeat-loss default is recorded: it resets the failing node, boots it and starts it,
with no application involvement (`docs/reports/2026-11-12_multiaxis_sync_survey.md:106`, D through that survey). The
library rejects that default (`:122`).

**CiA 301:** the only retained statements are CANopenNode's comments on the abort frame layout (`CO_SDOserver.h:96-101`),
the abort codes (`:290-299`) and the toggle encoding (`:126-169`). No retained source states that a server answers
one request at a time and in order (`docs/canopen_services.md:409`). Classic SDO carries no transaction tag. The
premise is protocol shape plus common implementation (I).

### 1.5 The remaining risk, and how the stacks accept it

The residual case: a reply arrives after the client's timeout, for the same object and with the same command class.
If the next request for that object is already out, the old reply settles it.

**Read example (I):** a read of `6064` times out at 100 ms and its reply comes at 120 ms. A new `6064` read sent at
101 ms returns the old position. That is wrong but bounded.

**Write example (I):** a write of `607A` times out. A new write of `607A` with another target goes out. The late
`60 7A 60 00` from the first write confirms the second, which may never have arrived, and the drive moves to the old
target.

**How the stacks accept it (I):**
- CANopenNode and python-canopen add nothing against it.
- In their usual use the client timeout (python 0.3 s) is far above the server's reply time, so a reply after timeout
  is rare.
- In this library it is more common, because stop deadlines cut reads short (D14 trial 213).

Three cheap guards remove the case under the in-order premise (P):
- A guard time after the unclear end.
- A marker read of a *different* object. A late old reply cannot match it, and under the in-order premise the old
  reply must come before the marker's reply.
- The rule that a marker never reuses a retained or last-completed pair.

The last two already exist (`SdoClient.cpp:324-340`, `:672-688`).

## 2. A TX frame nobody ACKs, bus-off, and node presence

### 2.1 CAN facts

**ACK (I):** ACK comes from *any* receiver that received the frame correctly, not from the addressee. This is
general CAN knowledge, recorded without a retained ISO copy in `docs/prompts/PROMPTSET_AUDIT_34_50_2026-11-02.md:249`
("ACK may come from any node") and `docs/adapters/twai_sdk.md:194-200`. So a no-ACK means no other node received the
frame. With two drives on the bus, one drive off still leaves the other acknowledging.

**The 2026-11-03 event (M)** (`docs/reports/2026-11-03_unattended_bench.md:77-78`):
- "Node 2's next request was still acknowledged. Then node 2 never replied, and the next frame got no ACK at all.
  Both drives went silent within the same 10 ms."
- The S2 retried one SDO upload for 4.9 h at about 7,250 `ack_err` per second. TEC rose to 128 and stayed there, REC
  stayed 0, and no bus-off occurred.
- "Every request for the other node timed out before transmission" (`docs/adapters/twai_sdk.md:43`), because the
  adapter keeps one SDK frame in flight for all axes.

**Error-passive and bus-off (I plus M):**
- Under ISO 11898-1, an error-passive transmitter whose ACK error is the only error does not raise TEC. A lone
  retransmitting node therefore stays error-passive at TEC 128 and never reaches bus-off. This is recorded at
  `docs/adapters/twai_sdk.md:41` and matches the measurement above.
- ESP-IDF's TWAI sources do not mention the exception (searched `*twai*` for ack error, fault confinement, 11898).
- Bus-off needs other errors, for example bit errors on a stuck or miswired bus (`docs/adapters/twai_sdk.md:49`).

### 2.2 ESP-IDF 5.5.5 TWAI (code read)

Paths are relative to `components/` in the framework.

| Topic | What the code does | Evidence |
| --- | --- | --- |
| Ordinary TX | `tr` only; retransmits with no limit. No TX-done until ACK | `hal/twai_hal_v1.c:405`; `hal/esp32s2/include/hal/twai_ll.h:282-285` |
| TX timeout | None in either driver. The timeouts only wait for queue space | legacy `driver/twai/twai.c:759`; node API `esp_driver_twai/include/esp_twai.h:117` |
| Single-shot | Any `fail_retry_cnt` other than -1 selects it (`tr`+`at`). "Will not automatically retry transmission upon an error (e.g., due to an acknowledgement error)"; it then raises a TX failure | `twai_hal_v1.c:375`; LL `:290-303`; `twai.c:192-194`, `:741` |
| Abort | `twai_ll_set_cmd_abort_tx` exists in the LL only ("Frames already being TX are not aborted"). No driver or HAL code calls it | LL `:320-323` |
| Clear queue | `twai_clear_transmit_queue` clears only the software queue; the frame in hardware keeps retrying | `twai.c:957-959` |
| What stops a pending TX | Entering reset mode: "Any TX is immediately halted on entering reset mode" (`twai_stop`, `twai_node_disable`), or bus-off | `twai_hal_v1.c:167-171`, `:210-211` |
| Bus-off recovery | Started by the application in both drivers. Needs 128 occurrences of 11 recessive bits. The node API restarts the next queued frame after recovery | `twai.c:882-906`; `esp_twai_onchip.c:545-555`, `:550`, `:245-253`; `twai.h:443-445` |

**This project's TWAI adapter (code read):**
- It configures endless retransmission: `fail_retry_cnt = -1` with SDK queue depth 1 (`adapters/twai/IdfTwaiDriver.cpp:92-95`).
- Only one frame is active at a time: `promote` returns while any slot is ACTIVE (`adapters/twai/TwaiAdapter.cpp:152-154`).
- Error-state changes other than BUS_OFF are ignored (`IdfTwaiDriver.cpp:315-318`), and error flags are only counted (`:322-328`).
- The header states the stall: "a no-ACK frame stays ACTIVE and stalls this interface until an application-started owner recovery deletes the node" (`TwaiAdapter.h:80-82`).
- One RX ring drop latches TX inhibition until recovery: submit refuses at `IdfTwaiDriver.cpp:172`; the inhibit is set at `TwaiAdapter.cpp:123-126`; the ring has 16 entries (`IdfTwaiDriver.h:24-26`).
- Recovery deletes and recreates the node (`TwaiAdapter.cpp:196-222`). It deliberately avoids `twai_node_recover` because that call restarts old queued frames (`docs/adapters/twai_sdk.md:67-68`).

**Hazards the project already recorded (D):**
- "A frame past its deadline can still be ACKed and take effect arbitrarily late, including on a rebooted drive" (`docs/adapters/twai_sdk.md:46`).
- "Retired does not mean undelivered" (`:47`).
- A late error can deliver a frame twice (`:48`, I).

### 2.3 Stacks and vendors on TX failure and bus-off

**CANopenNode (code read):**
- There is no TX timeout or abort API. `CO_CANsend` only sets `bufferFull` when the buffer is busy (`CO_driver.h:240-244`, `:569-570`).
- The error status has warning, passive and bus-off bits with thresholds 96, 128 and 256 (`CO_driver.h:430-447`).
- Callers ignore send results: the heartbeat producer at `CO_NMT_Heartbeat.c:191`; a failed TPDO waits for its next trigger (`docs/reference/public_code/canopennode/301/CO_PDO.c:1339-1344`).

**python-canopen (code read):** it resends a request once only on a local `CanError`, and by default not at all
(`client.py:50-63`). TX timeout and bus-off belong to python-can and the OS driver (I, not retained).

**ODrive (D):** "if autobaud is disabled: Restarts its CAN interface. If the fault condition persists, the CAN
interface is restarted repeatedly" (`docs/reference/other_can/odrive_can_protocol_0.6.12.html:1316-1325`).

**General practice (I, no retained source):** Linux SocketCAN `restart-ms` and AUTOSAR bus-off recovery both restart
the controller automatically after a delay, with backoff, and both report it.

### 2.4 Node presence keeps axes independent

**CANopenNode heartbeat consumer** (`docs/reference/public_code/canopennode/301/CO_HBconsumer.h`), D from the header:
- Each node has its own state, timer and receive flag (`:62-78`).
- "Monitoring starts after the reception of the first HeartBeat (not bootup)" (`:50-51`).
- On timeout it sends an emergency (`:48-49`) and calls a callback on the active-to-timeout change (`:203`). A boot-up callback exists (`:217`).
- The code changes only the local node's NMT state, and only when it is configured to (`CO_NMT_Heartbeat.c:224-236`). It sends nothing to the remote node. The survey records "Reported only: it sends no NMT and retries nothing" (`2026-11-12_multiaxis_sync_survey.md:110`).

**python-canopen:** it keeps NMT state per node, sets boot-up to pre-operational, and has no heartbeat timeout
(`canopen/nmt.py:113-132`).

**Lely (D through the survey):** a lost node is reset and restarted automatically (`:106`). This library rejects
that, and rightly, because it is an NMT reset on loss.

**Lichuan (D):** `1017` (heartbeat producer) defaults to 0. There is no `1016` (consumer) and no `6007` (reaction to
host loss) (`docs/backlog.md:98`; survey `:69`, `:113`, `:132`). Without commissioning, the host's presence evidence
is SDO results plus boot-up frames.

**This library (D):** health is READY, DEGRADED on a failure streak, or OFFLINE on a heartbeat expiry or the TX
interlock (`docs/can_owner.md:551-558`). A quarantined node leaves the other nodes running
(`docs/can_owner.md:446-449`; `docs/canopen_services.md:377-379`). The shared in-flight frame is the one place
where one node's frame blocks another node's frame.

## 3. FieldCore-node RS485 (HEAD `5ad9292d`, read-only)

Root `c:\Users\Honza\Documents\Projects\FieldCore-node\`. Spot-checked lines are marked with an asterisk.

| Question | What it does | Evidence |
| --- | --- | --- |
| Owner | One `Rs485Task` driven by one worker; foreground code only enqueues into an 8-cell FIFO. One physical transaction at a time, and one command per device | `src/rs485/Rs485Diagnostics.cpp:168-171`, `:874-912`; `src/rs485/Rs485RuntimeIngress.h:214-260`; `src/rs485/Rs485Task.cpp:479-484`, `:916-919` |
| Urgent stop | Never queued. Serviced at the top of every poll, interrupts an active transport and cancels the ordinary motor lane | `Rs485Task.cpp:676-686`*, `:795-802`* |
| Timeout | Per device: `Rs485InstanceRuntimeConfig{address, baud, responseTimeoutMs, retries}`. Range 100..5000 ms; SHZK 1000 ms, VibWire 1500 ms | `include/TunnelMonitor/rs485/Rs485DeviceBinding.h:12-17`*; `TunnelMonitorSettingCatalog.h:82-87` |
| Retry | In the device module, not the owner or codec. Backoff `75 << attempt` ms (75, 150, 300), at most 3 (SHZK default 2, VibWire 0). Retries never extend the command deadline. Backend errors and deadline expiry are never retried | `src/rs485/ShzkDeviceModule.cpp:476-482`*, `:429-444`, `:478-479`; `docs/reports/archive/shzk_retry_review_20260912.md:46-48` |
| Motor | "A motor never inherits the sensor automatic retry policy": retries other than 0 are refused. No fault-clear operation | `src/devices/rs485_motor/Rs485MotorModule.cpp:111-117`*; `docs/guidelines/rodtester/integration.md:367` |
| Uncertain motor result | Any transmitted byte sets `executionUnknown`. Motion is then inhibited until a qualified fresh ConfirmedStopped read, newer than the inhibit and younger than 500 ms | `Rs485Task.cpp:1815`, `:709-712`, `:837-855` |
| Offline/online | Only zero-byte terminal timeouts count as misses: stale (Absent/Degraded) at 2, fault at 5. Any Ok, including a probe, resets to Present/OK. Probes continue at the poll interval | `ShzkDeviceModule.cpp:558-577`*, `:535-545`*; `TunnelMonitorBuildProfile.h:48-49`; `docs/guidelines/rs485_devices.md:535-545`* |
| Cost of an offline device | Each attempt holds the single bus for its full timeout. With no devices present, only 6 of 600 probes were accepted | `docs/reports/hil-testing/condensed/archive/rs485_runtime_admission_postfix_20260815.md:57-62` |
| Late bytes | RX is cleared before every transmit. There is no wire sequence number, so a same-shape late reply after the clear would be accepted (I) | `Rs485Task.cpp:1368-1374`* |
| Two protocols, one bus | The owner is protocol-free: "does not contain SHZK, VibWire, Modbus, CRLF, address, parser ... policy". Each transaction carries its own baud, completion mode and echo policy. Protocols interleave per transaction by round-robin. There is no cross-device address or baud check | `docs/guidelines/rs485_devices.md:153-157`*; `include/TunnelMonitor/rs485/Rs485OwnerTransaction.h:100-119`*; `src/product/tunnelmonitor/TunnelMonitorSettingsPolicy.cpp:104-113` |
| Transport recovery | UART re-init is retried every 5000 ms, automatically and without replay. "Ordinary timeout, parser error, absence, and configured-retry exhaustion do not recreate or automatically recover the owner" | `Rs485Diagnostics.cpp:800-801`, `:835-853`; `docs/guidelines/rs485_devices.md:450-453`* |

**What maps to CAN (P):**
- Per-device timeout. This already exists as the per-request SDO response timeout.
- Bounded read retries that never extend the deadline.
- Retries in the module or policy layer, not the codec or owner.
- A per-device miss streak with two thresholds, back online on the next success.
- Probes continue while offline.
- Urgent stop first.
- Automatic transport re-init that replays nothing.
- "Uncertain motor result inhibits motion until a fresh confirmed-stopped read".
- A protocol-free owner with protocol modules.

**What does not map (I):**
- **Bus time:** RS485 is half-duplex master/slave, so a silent device costs the bus its full timeout. On CAN a
  request frame leaves in about 130 µs, other nodes keep transmitting, and waiting costs no bus time. Skipping an
  offline node is therefore for clarity, not for bus time.
- **What retries are for:** RS485 retries also cover CRC and noise errors. CAN controllers already retransmit corrupted
  frames in hardware, so a CAN software retry is only for a lost or late *reply*.
- **Late data:** RS485 can clear RX before each transmit. CAN cannot, because other nodes' frames keep arriving; the
  CAN equivalent is the guard time plus the different-object marker.
- **How protocols share:** RS485 protocols share by time and may change baud per transaction. CAN protocols share by
  identifier and format at the same moment, at one bitrate. Every node receives and ACKs every frame and must ignore
  foreign ones.
- **Address checks:** RS485 has no address-collision check, which is a gap there. On CAN a collision means a
  misinterpreted command, so the check is mandatory.
- **ACK:** RS485 has no ACK, so the no-ACK stall has no RS485 equivalent.

## 4. Retry and fault-clear policy

**Constraints**
- The core has no automatic retries (`AGENTS.md:57`).
- Applications own retries and health policy (`AGENTS.md:58`).
- "Never import sensor retry policy into motor commands" (`AGENTS.md:53`).
- No implicit fault clearing, NMT reset or re-enable. Preserve uncertainty and never replay motion (`AGENTS.md:75`, `:77`).
- MotorControl-RS, which is authoritative, says "Only explicitly classified operations may be retried under
  application policy. Reconnection must not replay motion" (`MotorControl-RS/docs/architecture.md:415-419`; table at
  `:133`).
- FieldCore's motor module refuses retries (above).

So the policy is an **owner/adapter-level, application-configured layer** with an explicit class per operation. The
portable core only reports outcomes (P).

| Class | Rule | Why | Example |
| --- | --- | --- | --- |
| Read (DEC-1 accepted objects only) | After the channel resyncs, retry up to R times (R configured, 0..3) within the original deadline | No changing effect is accepted for these objects; a fresh reply is just as good | `6041` read lost in a stop deadline |
| Setting write (absolute value, no trigger meaning) | If uncertain: resync, then read back. Equal to target: `CONFIRMED_BY_READBACK`, with the write count unknown (1 or 2). Equal to the old value: write again, at most once. Anything else: `UNKNOWN`, stop | The final value is what matters for an absolute setting, and the readback is a fresh exchange with a different command class (`0x4x` against `0x60`) | `2202` setting while released |
| Quick stop `000B` | Repeat every Ts until a fresh read shows the stopped or released state or the deadline passes, at most K times. The existing best-effort route (DEC-2) is the carrier | `000B` has the quick-stop bit low, so no CiA 402 transition from it enables (I) | Stop sent while the reply was lost |
| HALT `010F`, any word with bits 0-3 set as enable | Never repeated | `010F` from Switched On would enable with halt (I): "never enable merely to prepare a stop" (`AGENTS.md:79`) | |
| Motion start, set-point trigger, velocity start, homing start, enable sequence, save, NMT, node or bitrate change | **Never replayed.** Uncertain means `UNKNOWN` plus one reconciling read, then motion is inhibited until the application decides (FieldCore pattern) | A resend repeats or restarts motion | See below |

**Why a motion start is never resent (concrete).** A relative move of +10 mm writes the target, then sets the
controlword's new-set-point bit. The trigger write times out. The drive may have received it and started moving,
with only the ACK lost. A resend adds a second +10 mm: the axis travels 20 mm and can hit an end stop.

An absolute move is no safer:
- The trigger needs a bit-4 edge, so a resend first clears bit 4, which interacts with the running set-point.
- If the operator stopped the axis in the meantime, a resend starts motion after a stop.

The same holds for a velocity start after a stop, and for an enable after a fault reaction. These are general CiA
402 mechanics (I). The Lichuan trigger rules are in `docs/position_parameters.md` and prompt 33.

**Fault clear (P).** The only clear the library has is `prepareClearAlarm`. It writes `2301` 1 then 0, and only for
`603F=FF05` on a released drive in mode 1 (`docs/fault_reference_actions.md:3-7`, `:24-30`). It has never run on a
specimen, because no alarm has occurred (`:11-13`). The controlword bit 7 reset is not implemented (`:31`).

An optional automatic clear may use exactly that action and nothing else:
- Off by default.
- At most one clear per configured window and K per drive boot.
- Only while the drive reports released and no operation is active.
- It never sends an enable word and never resumes. The application must enable explicitly afterwards.
- If the same alarm returns within the window, there is no further clear; the policy reports and stops.

**PLCopen (I, no source in the repository).** `MC_Reset` is an explicit, edge-triggered application call, not an
automatic action. Where `MC_Power` is still on, it can return the axis to an enabled standstill. This proposal is
stricter: the clear never re-enables.

## 5. Mixed protocols on one CAN bus

### 5.1 How CAN separates protocols (I)

All nodes see all frames and ACK the correct ones. Each node keeps or ignores a frame by identifier and format (11-bit
or 29-bit). There is no master time slot. Arbitration gives the bus to the lowest identifier. A standard frame beats
an extended frame with the same 11-bit base, because the SRR and IDE bits are recessive in the extended frame.
Mixed protocols therefore need disjoint identifier sets *and* receivers that ignore what is not theirs. Protocols
take turns only through arbitration priority.

### 5.2 CANopen identifiers (D, CANopenNode, `CO_driver.h:396-414`; restricted list `:418-427`)

| Range | Use |
| --- | --- |
| `0x000` | NMT |
| `0x001` | Global fail-safe command (CANopenNode list) |
| `0x080` | SYNC |
| `0x081-0x0FF` | EMCY (`0x80` + node) |
| `0x100` | TIME |
| `0x181-0x1FF`, `0x281-0x2FF`, `0x381-0x3FF`, `0x481-0x4FF` | Default TPDO1..4 |
| `0x201-0x27F`, `0x301-0x37F`, `0x401-0x47F`, `0x501-0x57F` | Default RPDO1..4 |
| `0x581-0x5FF` / `0x601-0x67F` | SDO server-to-client / client-to-server |
| `0x701-0x77F` | Heartbeat, boot-up, node guarding |
| `0x7E4` / `0x7E5` | LSS slave / master |
| Restricted by CiA 301 | `0x000-0x07F`, `0x101-0x180`, `0x581-0x5FF`, `0x601-0x67F`, `0x6E0-0x6FF`, `0x701-0x7FF`: "shall not be used for SYNC, TIME, EMCY, PDO and SDO" |

**Free 11-bit identifiers (I, computed from the table):**
- `0x680-0x6DF` (96 IDs) is neither predefined nor restricted. It ranks below all SDO, PDO, EMCY, SYNC and NMT traffic
  and above heartbeat and LSS.
- The PDO slots of node IDs that are not on the bus are free only under a plan that reserves every node ID ever used.
  PDO COB-IDs are also reconfigurable, so the check must use the configured COB-IDs, not the defaults.
- Node-0 base IDs (`0x200`, `0x280`, ...) and the restricted ranges should be avoided.

### 5.3 29-bit frames beside 11-bit CANopen

**What is documented (D):**
- CANopenNode's receive filter takes an 11-bit identifier (`CO_driver.h:526-530`, `:224-225`).
- It rejects COB-ID writes with bits 11..29 set: "bits 11...29 must be zero" (`docs/reference/public_code/canopennode/301/CO_PDO.c:542-546`; `CO_SDOclient.c:174-176`).
- This library's SDO path rejects extended frames on SDO routes (`SdoClient.cpp:662`). The PDO path matches the format bit (`adapters/canopennode/Pdo.cpp:30-32`, `:150`).
- The library's frame type keeps 11/29-bit distinct (`include/MotorControlCAN/can/Frame.h:19-22`).

**What is not established:**
- No retained source says that CiA 301 devices ignore 29-bit frames. That is the usual result of a standard-ID-only
  filter (I).
- Whether the Lichuan's firmware masks an extended ID down to 11 bits is unknown (U). It needs the bench test in
  item 9.

### 5.4 Other protocols' traps (D)

**ODrive CANSimple** uses 11-bit IDs (`odrive_can_protocol_0.6.12.html:918`) and documents an interoperability table
(`:1333-1350`): "ODrives can coexist with CANopen devices on the same bus if caution is taken with the node ID
assignment".
- With CANopen nodes 32..127, only ODrive node IDs `0x10`, `0x18`, `0x20`, `0x28` are valid.
- Every listed combination requires CANopen node IDs of 32 or more. The bench nodes 1 and 2 would have to move, and
  a node-ID change is an explicit commissioning action.

**moteus:** "The CAN frame must be an extended one if the ID would be greater than 0x7fff, but otherwise whether or
not a frame is extended is not considered" (`docs/reference/other_can/moteus_can.md:25-33`).
- Every 11-bit CANopen frame is therefore also a moteus frame, read as source = high byte and destination = low byte.
  For example, `0x601`, the SDO request to node 1, would reach a moteus at ID 1 (I).
- A nonzero 13-bit `can_prefix` (`moteus_protocol.h:80`; `moteus_transport.py:346`) forces IDs above `0x7FFF`, so
  the frames must be extended (I).

**VESC** uses only 29-bit extended IDs (`docs/reference/other_can/vesc_comm_can.md:26`). Against CANopen it is
separated by format, if every CANopen device ignores extended frames.

**CAN FD:** FD frames on a bus with Classical-only nodes cause errors. ODrive firmware 0.6.10/0.6.11 "could run into
spurious bus-off events when CAN-FD frames were present" (`odrive html:1268-1276`).

### 5.5 What the owner would need (P)

The audit already sets the direction:
- "One application owner receives every frame and routes it to the selected protocol instance ... Mixed protocol
  buses require a documented identifier-allocation/conflict policy and compatible physical bitrate/frame
  capabilities" (`docs/audit/OTHER_CAN_PROTOCOLS.md:203-209`).
- The `Protocol::PROPRIETARY` label exists with no code (`include/MotorControlCAN/Endpoint.h:16-19`).
- `AGENTS.md:59` asks for an "explicit identifier/broadcast/format compatibility analysis".

Today the owner already passes every frame, accepted or not, to an optional `observe` copy callback
(`adapters/owner/CanBusOwner.cpp:625-666`). It counts frames that no protocol accepted as `unmatchedFrames` (`:646`).
TX has only the SDO and NMT kinds (`adapters/owner/CanBusTypes.h:60-68`).

The smallest addition:
- **Identifier plan (fixed capacity).** Entries are (first ID, last ID, 11/29-bit, protocol instance, flags:
  format-blind, broadcast IDs). The plan is checked once at configuration against:
  - every configured CANopen node's default and configured COB-IDs;
  - the global CANopen IDs (`0x000`, `0x080`, `0x100`, `0x7E4/5`);
  - the CiA 301 restricted ranges.

  A device flagged format-blind (moteus with prefix 0) is refused beside 11-bit CANopen. The plan is part of
  configuration evidence, and a change invalidates as a rebinding does.
- **RX route.** A frame not accepted by CANopen dispatch that matches a plan entry goes to that protocol's bounded
  receive callback, as a copy, once. Frames outside the plan stay unmatched diagnostics.
- **RAW TX kind.** It has its own fixed slots and an admission class ranked below CANopen urgent stops and the
  protocol's own stops, and above ordinary SDO. The owner treats it as fire-and-forget. The protocol module decides
  what confirmation means, and the owner never retries. The TX lifetime and controller restart apply to it as to any
  frame.
- **Shared fault domain.** A no-ACK, bus-off or controller restart invalidates both protocols' evidence and is
  reported to each module. Bitrate and FD capability are one bus-wide fact.

## 6. Recommended designs: the smallest safe mechanism per topic

**6.1 SDO self-repair (resync).**
- **Abort:** after a timeout with possible TX, stage one CiA 301 client abort `0x05040000` carrying the request's
  index/subindex, within a small cleanup budget. It is cleanup only and settles nothing.
- **Guard:** the channel stays unresolved as today. When the application's `SdoResyncPolicy` is enabled, the owner:
  1. waits until local TX on the route is retired;
  2. waits a guard time G after the unclear end and after the last frame on the route's response COB-ID. A late frame
     restarts G and is retained as a pair. G is an application setting at or above the response timeout; 100 ms on
     this bench is about 17 times the measured maximum reply.
- **Marker:** the owner then sends the existing drain marker, one expedited read of a DEC-1 object (`1000:00`, with
  `1018:01` as the alternate). It must differ from the retained and last-completed pairs, and its value is compared
  with the commissioned identity.
- **Results:**
  - A matching reply reopens the route (existing `reopenAfterDrain`).
  - A timeout counts as a presence miss, with backoff.
  - A wrong value invalidates the binding: a different drive.
  - `drainRefused` (overflow, a frame with no multiplexer, an unexpected frame after a reopen) contradicts the
    premise. The route then stays blocked until an explicit operator action, such as an NMT reset communication of
    that node or an MCU restart, and the event is reported.
- **Premises:** they come from a recorded owner decision (DEC-3a). `soleClient` is a configuration statement: no PC
  tool on that channel at the same time. `orderedServer` is the accepted CiA 301 premise, to be measured under Q18.

**6.2 "Nobody ACKs": TX lifetime and automatic controller restart.**
- **Detection:** the TWAI adapter starts a lifetime clock T_tx at SDK submission. T_tx is an application setting,
  sized from the measured maximum submit-to-done time under load. The S2 already reports `ack_err` and error-passive,
  which can make the detection earlier and more certain.
- **Action:** on expiry, the owner policy starts the existing recovery (delete and recreate the node). The frame is
  reported retired with effects possible. Its channel becomes unresolved and is resynced later by 6.1.
- **Interface state:** the interface enters NO_LISTENER. Ordinary admissions fail fast, and urgent stops stay
  admissible. A received frame (boot-up, heartbeat) or one ACKed probe ends the state.
- **Backoff:** restarts back off (doubling up to a cap, at most N per minute). The minimum is well above the
  128 x 11-bit bus-off recovery time, to stay as polite as standard recovery.
- **Same path elsewhere:** bus-off, RX ring overflow and adapter faults use the same path. Nothing is resent, no
  device command is sent, and every restart is logged.

**6.3 Per-node presence.**
- Per binding: DEGRADED after M consecutive attempted-service timeouts, OFFLINE after N (configured; FieldCore uses 2
  and 5).
- While OFFLINE, ordinary work fails fast with no TX. One marker probe runs per period P.
- The node is online again at the first success or after boot-up, which also resyncs the channel.
- Heartbeat consumption is used where `1017` was commissioned. There is never a reset on loss.

**6.4 Retry policy.** The `OperationRetryPolicy` in the owner/adapter layer follows the class table in section 4.
- The core reports classes and outcomes only.
- The policy runs only after transport recovery (6.2) and channel resync (6.1). That is the link between "no drive
  answers" and "no automatic retry".
- Fault clear is a separate opt-in sub-policy and never enables.

**6.5 Mixed protocols.** The identifier plan, RX route and RAW TX kind of section 5.5, applied as a configuration
check plus routing. There is no CANopen emulation and no shared state between the protocols.

## 7. Prompt items for a future implementation prompt

Numbers are local to this list; the lead assigns prompt numbers. Each item keeps the core free of retries and
clocks.

1. **Decisions and evidence records (docs only).**
   - Record the CAN ACK and error-passive facts with their marks, and the 2026-11-03 measurement.
   - Record DEC-3a: the CiA 301 premise for the default SDO channel, and sole client as a configuration statement.
   - Record DEC-4a: TX lifetime and automatic controller restart.
   - Record DEC-9: retry classes, plus fault clear off by default.
   - Checks: `python scripts/check_promptset.py`, `git diff --check`.
2. **Timeout abort.**
   - On TIMEOUT after possible TX, stage one `0x80` abort with code `0x05040000` and the request's index/subindex,
     with its own cleanup deadline. It never reopens a route or settles an effect.
   - Host tests:
     - The abort bytes are exact.
     - No abort when TX was never possible.
     - The abort is handed off at most once.
     - It changes no quarantine state.
     - Other channels are unaffected.
3. **SDO resync policy** (owner layer: `SdoResyncPolicy{enabled, guardUs, markers[2], expected values, maxAttempts, backoff}`).
   - Triggers: TIMEOUT, CORRELATION_LOST, unclean abort, boot-up, controller restart. Reuse `admitDrainMarker` and
     `reopenAfterDrain`.
   - Host tests:
     - A late frame inside G restarts G and is retained.
     - A same-object late reply during the marker is ignored.
     - Any other object fails the marker.
     - Overflow refuses markers.
     - A wrong marker value invalidates the binding.
     - A reopen invalidates the node's reservations.
     - A disabled policy never sends.
     - The second node keeps its rate throughout.
     - The counterexample: with G = 0 and the same object, a late `60` confirms a later write. The policy refuses
       G = 0 and same-object markers.
   - Bench:
     - Read a DEC-1 object on node 1 with a 1 ms response timeout, against about 4.9 ms measured. The late reply is
       retained, the resync reopens, and reads continue.
     - Repeat 1,000 times; count reopen failures.
     - Replay the 2026-11-04 cases: a stop deadline cutting a read must no longer need a restart.
4. **Q18 measurement** (bench, before DEC-3a is called measured).
   - Cases: a slow write then a read; a second request 0.5 ms after a first on one node; a search for duplicates.
   - Record the S2's RX with TEC/REC, then use the independent listen-only observer when available. The user asked to
     wait for it for prompt 47 (H04).
   - Outcome: confirm or refute `orderedServer` for this firmware.
5. **TX lifetime and automatic restart** (`adapters/twai` plus owner policy `ControllerRestartPolicy{txLifetimeUs, minBackoffUs, maxBackoffUs, maxPerMinute}`).
   - Host tests with the fake driver:
     - Expiry starts the recovery.
     - The frame is retired with effects possible and its route unresolved.
     - No frame is resubmitted.
     - TX-done just before expiry causes no restart.
     - TX-done racing the stop is handled.
     - Backoff and rate limits hold.
     - NO_LISTENER fails ordinary work fast and admits stops.
     - Bus-off and RX overflow take the same path.
   - Bench: one physical action. Unplug the CAN connector at the S2 for 10 s with the drives powered and released,
     then plug it back. Expect:
     - `ack_err` at once and a restart within T_tx;
     - no stale frame after the replug (observer optional);
     - both nodes resynced, reads continuing, and no MCU restart.
   - This also answers packet 3 question 4: whether disable stops a retrying frame.
   - Also measure the submit-to-done distribution under the current load, to size T_tx.
6. **Per-node presence.**
   - Owner health thresholds M and N, fail-fast while OFFLINE, and a probe period P.
   - Host tests: two nodes, one silent fake. The other node's latency and counts are unchanged. Back online after one
     success or boot-up.
   - Bench: configure an absent node ID (for example 3) beside nodes 1 and 2. Its SDO frames are ACKed by the
     drives, its requests time out, and it goes OFFLINE while nodes 1 and 2 keep their read rate. No physical action
     is needed. This demonstrates "one missing drive never blocks the others".
7. **Retry policy** (`OperationRetryPolicy` per operation class, section 4).
   - Host tests:
     - Uncertain relative move: exactly one trigger frame on the wire, outcome `UNKNOWN`, motion inhibited until a
       fresh stopped read.
     - Setting write with a lost ACK: readback confirms it, with one write on the wire.
     - Setting write with a lost request: readback returns the old value, then one retry.
     - Stop repeat: every frame is `000B`.
     - Retries never extend a deadline, and attempt counts are recorded.
   - Bench, attended-experiment scope:
     - A setting write with a forced short timeout on a released drive.
     - A low-speed move with a forced short trigger timeout: no replay, and the reconciliation read shows the
       position.
     - A stop repeat at low speed.
8. **Optional fault clear.**
   - Off by default. Only `prepareClearAlarm` (FF05), only while released and idle, at most one per window and K per
     drive boot. It never enables.
   - Host tests: the limits, refusal of other codes, no enable frame, and no action when off.
   - Bench only if an FF05 alarm occurs.
9. **Mixed-protocol foundation** (owner: identifier plan, RX route, RAW TX kind).
   - Host tests:
     - Overlap with node ranges, configured COB-IDs or restricted ranges is refused.
     - A format-blind flag is refused beside 11-bit CANopen.
     - A 29-bit plan is accepted.
     - Plan frames reach only their callback; CANopen frames never do.
     - Stop ordering holds.
     - A restart notifies every module.
   - Bench, drives released, two cases:
     - (a) A 29-bit frame whose low 11 bits are `0x601`, with a `40 00 10 00` upload payload, which is harmless if
       misread. Expect no reply on `0x581`.
     - (b) 11-bit frames in `0x680-0x6DF`. Expect no EMCY, an unchanged statusword and zero error counters.
   - Never use an NMT-looking payload.
10. **Documentation.**
    - Files: `docs/can_owner.md`, `docs/canopen_services.md`, `docs/adapters/twai_sdk.md`, README, CHANGELOG.
    - Backlog D14, H04 and T04 notes, and limitation items 2.1, 2.5, 7.3 and 8.3, updated with what is host-tested
      against what is measured.

## 8. Not established

- The Lichuan's SDO server order and duplicate behaviour (Q18). Whether its firmware ignores extended frames.
- Lely's SDO client timeout handling: there is no source here.
- Whether TX-done fires at bus-off on the node API (`docs/adapters/twai_sdk.md:49`).
- The submit-to-done maximum under PDO load.
- Whether deleting the node resets TEC immediately (I: the clock reset suggests it does).
- The PLCopen and AUTOSAR statements are general knowledge, not retained sources.
