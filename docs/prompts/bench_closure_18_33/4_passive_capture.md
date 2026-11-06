# Packet 4 Add explicit bounded silent capture

Apply [COMMON](COMMON.md). Prerequisite [3](3_s2_shared_firmware.md)'s board and
silent-control contract. Originals27-29. This packet implements capture
readiness; it cannot establish physical TX retirement with one observer watching
its own software events.

## Inspect

Read originals27/29, `adapters/twai/IdfTwaiDriver.*`,
`examples/common/FirmwareProbe.*`, `examples/s2_bench/Soak.h` and `Soak.cpp`
(`SoakTwaiTap`), `PdoWatch.*`, `main/main.cpp`, `docs/adapters/twai_sdk.md` and
the SDK sources identified by its pinned audit. Refresh RS diagnostic
copy/owner seams and INA228 hardware-validity handling. FieldCore remains
read-only for ownership/invalidation.

Existing PdoWatch receives copies from an active controller; calling it a
passive software observer does not prove the transceiver sends no ACK.

## Implement

1. Add one explicit finite capture mode using the same owner, driver and copied
   RX trace. Prefer the current application mode representation. No independent
   receive task, second TWAI instance or generic adapter-selection framework.
   Require a duration/frame capacity and expose overflow and end reason.
2. Refuse entry while ordinary, urgent, NMT or recovery work remains active or
   queued. Use established retirement evidence, not a GPIO change as proof
   queued writes disappeared. Refuse when the required retirement is unknown.
   Entry and exit cannot resolve an ambiguous SDO channel or replay its work.
3. While capture-only is active, reject every path to transmission, including
   SDO cleanup/abort, NMT, PDO/SYNC, urgent work and automatic controller retry
   if the selected SDK/hardware mode does not prevent it. Reconcile hardware
   listen-only and transceiver silent behavior against the actual SDK/NXP
   contract. A mode name or a successful GPIO call is not physical proof.
4. Preserve normal bounded asynchronous dispatch and raw copies. Record receive
   interval bounds, clock domain, controller state/counters, commanded silent
   level, overflow, image identity and begin/end boundaries. Unknown frames are
   diagnostic data. Stop-capture/cache inspection/startup issue no CAN commands.
5. Fake-test entry with held/uncertain TX, failures changing GPIO/controller
   mode, all rejected send paths, late RX, finite expiry, overflow and repeated
   exit. Ensure unrelated retained terminals survive capture and failed entry.

## Verify and finish

Run owner/TWAI/platform/watch and runner tests affected, the real S2 image build
and Doxygen. Execute a bounded live mode-transition/passivity smoke check only
after state prerequisites hold. A silent S2 with silent drives can legitimately
capture nothing; do not call zero frames proof that capture or silence works.
Do not secretly start drive PDOs inside capture entry. Any known producer
configuration is a separate explicit admitted experiment.

Keep physical no-ACK/no-TX verification and remote-session drain obligations
open until a verified independent observer is connected. A PCAN unit mentioned
as available is not yet an integrated observer. Do not send malformed DLC
traffic to these drives just to fill a test matrix.

One writer owns mode/controller mutation; delegate negative send-path tests.
Use stronger review of old queued writes across entry/exit. Write packet_4,
update27/29 and T04's exact software-versus-wire distinction, commit and sync.
