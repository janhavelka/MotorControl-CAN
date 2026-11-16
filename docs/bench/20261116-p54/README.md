# Prompt 54 qualification campaign

Starting source: `947cc5da8cbb0c24799d8114d8b665b38341921f`, on `main`.
The lead alone opens ESP-Prog2 COM15 (serial `94A990398B40`). Workers review
source and retained evidence; they do not open a second console or CAN adapter.

## Bounds and initial state

The recorded fixture is OPTAK_FGI_2025 ESP32-S2, TJA1051TK/3/1J,
RX16/TX17/silent18, two CL86-C drives with LC60H2127 motors, nodes 1/2,
1 Mbit/s. Standing bench and unattended motion authorization apply. No new
shaft, supply, electrical, wired-stop or independent CAN observation is implied.
The raw identity tuple and each image/configuration must be captured separately.

Initial UART ping confirms the previous protocol14 image, ELF
`9a96eecef07ecfe01166dee0d08995a68aede90daf2a973882398101a206187b`.
The retained trial 52036 is released locally; this sends no drive command.
Node 1's uncertain stop 52012 and temporary TPDO1 type 1 remain unresolved. Its
SDO route must not be reused until the external drive-session boundary is
established. The first plan observes boot messages for 600 seconds, with no
CAN transmission. A requested operator power cycle is not an observed cycle.

## Retained runs and images

| Records | Purpose / result |
| --- | --- |
| `00` through `02` | Initial image, local record release, 600-second passive boot watch. Both nodes reported Operational; no boot-up. |
| `03` through `07` | First unstable-source build refused, stable build/flash and startup measurements. |
| `08` through `14` | Typed reads; raw identity continuation refused locally. Preserve the failed plan and zero-TX record. |
| `15` through `20` | Identity allocator correction, flash/ping, fresh typed reads, raw continuation and node 2 type 1 setup succeed. |
| `21` through `28` | Node 2 enable, interrupted position with its own checked quick stop, explicit mode 3, later velocity refusal by the retained position fence, local releases. |
| `29` through `32` | Initial 100 ms collector failure, local release, corrected complete 100 ms capture. |
| `33` | Initial 10 ms capture: 500 confirmed local SYNC transmissions and 500 native TPDO counts per node; diagnostic print FIFO loses 274 records. Raw capture is incomplete. |
| `34` through `37` | Bounded output-drain correction, matching flash/ping, startup and complete short 10 ms capture. |
| `38` | Nominal 1800-second, 180000-slot 10 ms attempt ended with `MISSED_DEADLINE` after 42988 confirmed local transmissions. All 42988 TPDOs per node and 860 heartbeats were captured without reported loss. The requested 30-minute qualification failed. |
| `39` | Explicit local release of the retained failed SYNC result; no schedule replay. |
| `40` | Nominal 60-second, 600-slot 100 ms baseline passed: 600 local transmissions, 600 raw TPDOs per node, 1320 complete receive records, zero reported capture/controller loss. This does not qualify 30 minutes at 100 ms. |
| `41` | Cached final image/controller/resource/SYNC inspection. Producer disarmed, no active or retained schedule; no new drive read, stop or configuration change. |

The action trials use ELF
`2e0d33ea763bfe22dc9c1856660b388bfd74812f770e905ae48fb108228a4a84`.
The corrected compact-capture image uses ELF
`96d81d97f62471950bfa4589c2bb21cd8bb2eda0f97b150d8bef26ceb4b8c469`.
Their exact source and image hashes are in the respective `15` and `34` build
records. Only `ProbePlatform.{h,cpp}`, `PdoWatch.h` and S2 `main.cpp` differ
between their 236 source hashes; the other 232 match. All 236 current source
files and the actual image/configuration files match the `34` manifest.
No owner, action or motion code changed.
The final image's console/SYNC regression is separate from the earlier image's
action observations; its motion regression remains blocked.

Both last verified TPDO1 types remain 1. Node 2's map was freshly checked in
`20-node2-type1`; node 1's map is historical as stated in the report. No
persistent save was issued. Node 2 was last directly read as released in mode 3,
position 36162 and velocity 0 in `27-velocity-regression`. Later raw TPDOs retain
status 0x4050, mode 3 and position 36162. TPDO1
carries status, mode and position, so later PDOs do not refresh velocity.
Node 2's interrupted position restriction and node 1's unresolved stop remain
externally retained in this handoff even though an MCU restart clears volatile
session state and counters. The durable
`motion-campaign.jsonl` retains six of twelve motion weights; do not start a new
ledger or rebuild a session to evade those histories.

## Finite plan and remaining checks

1. Identify the current image, record passive traffic and any externally
   performed power cycle. Check startup storage, controller counters and stack.
2. On eligible routes only, acquire identity, status, monitor, position,
   velocity, reviewed fault observations and communication/map configuration.
   Keep node 1 blocked if its remote-session premise is missing.
3. Run the established short enable/position/velocity/stop and console regression
   on the exact candidate image. Every admitted motion uses the existing durable
   campaign cap, one finite request and its explicitly planned stop budget.
4. Explicitly configure eligible TPDO1 types through the reviewed typed path.
   Establish a short 100 ms baseline, then a short 10 ms test. Attempt one
   continuous 1800-second, 180000-slot, two-node 10 ms schedule only when its
   prerequisites and bounded collector are ready. A missed slot ends production;
   there is no catch-up, automatic retry or silent change of period.
5. Repeat only the measured prompt 52 change forms with specified positive speeds
   and nonzero ramps. Preserve each original/change/stop outcome. No reverse,
   zero-speed, unknown FIFO or synchronous-RPDO capability is inferred.
6. Exercise independent and group stops with per-axis retained outcomes where
   both routes and motion prerequisites are established. Real wire loss/fault
   injection requires its own bounded fixture/control path; native injection
   alone is not hardware qualification.

M09/Q28 still gate shared quick-stop RPDO, buffered common starts and streaming
setpoints. Wired DI trials require the operator's wired fixture. NMT Stop during
motion needs a dedicated reviewed finite sequence; lifting the console's active
trial exclusion is not a valid experiment. None of these is hidden setup.

The completed scope remains **Partial**. Runs 37/38/40 establish only their
recorded local-transmission and raw-reception outcomes. They do not qualify
same-cycle sampling, wire jitter, mechanical skew, a synchronous set-point
stream, concurrent motion or a shared stop. Further motion remains blocked by
the separate histories above; flashing the diagnostic image is not a motion
reconciliation.

## Evidence interpretation and final reconciliation

Retain exact plan, tool/source/image identity, raw UART bytes, admitted request
identities, controller counters, watch loss, owner service gaps and resource
watermarks. S2 receive bounds and owner processing times are not exact wire
timestamps. Counts and interval statistics do not establish same-cycle sample
identity, motor simultaneity or independent standstill. Any incomplete trace
must remain visibly incomplete even when aggregate counters agree.

Run 38's first-to-last application-owner handoff span is 429.869001 s; its
capture window is 430.292005 s. The 180000-slot plan has a nominal duration of
1800 s, with 1799.99 s between the first and last scheduled instants; it did not
reach either bound. Run 40's passing first-to-last handoff span is 59.899003 s,
with a 60.338013 s capture window. These spans are not wire transmission times.
Payload bits and UART byte rates are not measured CAN bus occupancy.

Final [`41-final-state.txt`](41-final-state.txt) records ELF `96d81d97...`,
CAN running, no trial/watch/capture, SYNC disarmed with no active/retained
schedule, and zero controller loss/error counters. Free heap is 74456 bytes,
largest free block 57344 bytes and minimum free task stack 7624 bytes. This
cached inspection establishes no new drive state or physical standstill.

No save, factory restore, implicit fault clear or automatic motion replay is
part of this campaign. Restore temporary PDO type only as another explicit
checked operation on an eligible route. Leave unknown outcomes, changed
configuration and unavailable qualification in the final report.
