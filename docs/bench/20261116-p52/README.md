# Prompt 52 retained bench evidence

Read [the implementation report](../../reports/prompt_52.md) and
[the independent raw-data analysis](../../reports/prompt_52_measurements.md).
These files include successful, refused, incomplete and inconsistent captures;
a later success never replaces earlier evidence.

The fixture is ESP32-S2 OPTAK_FGI_2025, COM15 / ESP-Prog2 serial
94A990398B40, two CL86-C / LC60H2127 nodes at 1 Mbit/s. Request files retain
actual intent, firmware image, operator waiver and processing-time limits.
No independent shaft observation was available.

## Reading order

- 06-12: disabled mode-display probes on node1 and explicit mode3 restoration.
- 14,21,23: premature velocity-change refusal/capture overflow, enabled-state
  refusal, then one unconfirmed best-effort stop. Node1 remains quarantined
  until an external server-session boundary is established; MCU reset does
  not establish that boundary. The90-second watch27 saw no boot-up.
- 20/30: explicit temporary TPDO1 type1 settings, host SYNC disarmed, no save.
- 31: successful node2 positive30-to60rpm velocity change and original timed stop.
- 41,51/52,60,68,76,84,92,100: position cases1-8, respectively.
- 106/107: completed enable52033, inconsistent duplicate-END capture and cached
  same-trial dump; no repeated motor request.
- 109: longer same-endpoint speed change and moving quick stop from003F.
- 112: restoration refusal at the first authorization before any SDO or write.
- 113/114: corrected image build/flash and matching ping; no drive-session reset.
- 118/123: non-changing typed readiness trials52035/52036, each one END marker.
- 121: corrected explicit node2 restoration, all19services and final type255;
  Operational requested afterward. No save, map change or uncertain-write replay.
- 122: fixed-ID raw readiness refused locally after its identity range was consumed;
  123 used the increasing-ID typed route. No work was sent by the refused request.
- 124: final controller/application counters. No RX loss or CAN errors; diagnostic
  trace overwrites outside a finite acquisition are still visible.


Cached-composition JSON files identify exact source hashes and byte offsets.
Original captures are unchanged, including tail loss and duplicate markers.
The orchestration directory retains the executed one-off host scripts as text,
not installed runners. Each invocation admitted one explicit case, stopped on
any failure, and used existing checked tools. The long-case driver stopped
after the inconsistent enable capture; trial52034 was then requested separately
after cached evidence review, with no repeated enable or changed deadline.

Every host EN pulse was explicit and followed a retained settled node2 quick
stop. Node1's quarantine remained externally recorded across those pulses.
An EN pulse neither reset the drives nor cleared their prior possible effects.
