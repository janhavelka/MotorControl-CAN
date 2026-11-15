# Prompt 47 finite campaign, 15 November 2026

This directory retains plans and results from one lead operating COM15. Host
verification is separate from the device observations. The stage report and
per-feature disposition are in [report47](../../reports/prompt_47.md).

## Fixture and authorization

The existing fixture is the OPTAK ESP32-S2 with TJA1051, RX16/TX17/S18, two
CL86-C drives at nodes1/2 and 1Mbit/s, and LC60H2127 motors. ESP-Prog2 USB serial
94A990398B40 is COM15. Previous operator records describe free shafts, motors
bolted to the table, end termination and an independent power cutoff.

On this date the user stated: "if the drives are powered, do the tests, i cant
watch the shafts etc, but do the tests - i take all responsibility". This is
explicit unattended-test authorization. Nobody is observing shaft direction,
travel, standstill, torque or fault LEDs in this campaign. Supply voltage and
nameplate range, electrical margins, independent CAN observation and homing
switch wiring were not supplied. None is inferred from successful communication.

## Finite sequence and evidence rules

1. `00-uart-intake`: five-second passive UART capture, then one firmware ping;
   neither EN nor BOOT is driven. No CAN work.
2. `01-capture-old-image`: five-second/64-frame owner capture with application TX
   gated and S18 requested high. This is not verified physical listen-only.
3. `02-old-baseline` and `03-old-state`: first active upload1000, then DEC-1
   status/monitor/feedback reads on both nodes, no downloads/NMT. Current states
   are historical input to the deliberate MCU flash, not post-flash evidence.
4. `04-flash` and `05-ping`: explicit current-source build/flash and running-image
   comparison. MCU restart is not a drive restart or remote SDO retirement.
5. Repeat bounded receive capture and DEC-1 baseline on the verified image;
   enable raw diagnostic copies explicitly around ordinary runner work. Check
   actual TX/RX pairs and loss counters; tracing is not inherently lossless.
6. Admit each later finite experiment only with a separate retained plan and
   fresh native prerequisites. No save, factory restore, alarm clear, node or
   bitrate change, input reassignment, sweep, automatic replay or hidden enable.

The mode 3 trials used +30 motor rpm, 500 ms ramps and the existing experimental
firmware gate. The 1,000 ms negative requests rejected before downloads because
the setup/enable budget is 11,470 ms. The earlier public-console proposal in
`velocity-plan.json` was not executed as positive motion. The actual retained
[`velocity-native-plan.json`](velocity-native-plan.json) uses a 12,000 ms duration,
5,000 ms polling, a fixed 23,000 ms native deadline and a 60 s host capture bound.
Its own quick stop belongs to that request; the plan documents the existing
conditional S16 fallback separately. Neither run needed that fallback.

Final-image trials 47301/47302 (`38-velocity-node1` and `40-velocity-node2`) each
retained 88 terminals and 264 traffic records with no loss. Each reported 30 rpm
while running, then completed its own quick stop with zero velocity and no
outstanding stop requirement. The exact seven downloads were `60FF=30`,
`6083=500`, `6084=500`, and `6040=6,7,15,11`. No mode write was sent.
The product of requested rate and duration is six motor turns; setup, ramping,
stop response and service gaps prevent treating it as a physical travel bound.
No physical travel was measured. Start/stop qualification flags stay false.
Homing still needs the method's switch, direction, offset and completion evidence.

The final running image is source `bf8a0d9518c6b42ff06d4f29516bf82ee66abf79`,
ELF SHA `9ca7701bb4f20efe9c383fd8fe4929e0314980bd2f2f1899652abcaa687b0399`,
SDK literal `v5.5.5-dirty` (`28-flash-final`, `29-ping-final`).
The [independent final review](final_review.json) checks the raw exchanges;
the [source delta](source-delta.json) records concurrent sibling changes.

Receive intervals and processing/host times are software evidence, not exact
wire timestamps. A matched response proves service communication; a motion
terminal proves only its specified controller evidence. All failures remain in
their original artifacts even if a later, separately planned run passes.
