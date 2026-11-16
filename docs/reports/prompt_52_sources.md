# Prompt 52 source review

Reviewed 2026-11-16 from main `4c444225b37c315181638f8b2db3dc67100c7fa8`.
This review establishes source claims, not hardware behavior.

| Source | Revision/state | Adopted or rejected behavior |
| --- | --- | --- |
| MotorControl-RS | `e56d4c0af6a7d678da11bb672a1b94126812b3a1`, clean | `include/MotorControlRS/profiles/ess_rs/Position.h`, `StoredPositionQualification` and `src/profiles/ess_rs/Position.cpp::prepareNative`: independently frozen original profile, configuration and qualification age. Incoming target cannot establish its own witness. RS `INTERRUPT_CURRENT` is not only-if-busy. RS stored-profile retrigger cannot justify a CAN mid-run parameter effect. |
| RS relation and speed | same revision | `examples/probe_cli/MoveRelation.h`: retain original/replacement IDs and observation supersession. CAN puts this small association with common move evidence. Reject RS serial addresses, uint16 RPM and register frames; retain exact CAN rate conversion. |
| FieldCore-node | `c40ea8341edf8d665460b902cafefbe857a7f73f`, clean | Ownership/configuration seam unchanged since prompt51: `MotorControl.h` and `Rs485MotorModule.cpp` consume explicit operation/generation evidence. No FieldCore integration or replacement API is claimed. |

Manufacturer review reopened original EN86/ZH86 p12?13 and EDS, not only
extracted text. The original files and hashes remain in
[the manifest](../reference/SOURCES.json). EN86 SHA256 starts401df6c4;
ZH86 starts3969c914; EDS startsdd3c. The complete hashes remain authoritative
in the manifest and ledger. Independent source review also compared the original
Nanotec N5 p68?70 handshake as a contrast, never a Lichuan implementation.

| Fact | Source disposition |
| --- | --- |
| `607A` I32, documented ?1000000 pulses | EN86/ZH86 p12; EDS `[607A]`. Acknowledgement does not establish live application. |
| `6081` U32,0..3000rpm; `6083/6084` U32,0..2000ms | EN86/ZH86 p12; corresponding EDS sections. No physical ramp formula during a change; zero ramp stays gated. |
| `6040` bit4 rising; bit5 immediate/after-current-curve; bit6 relative | EN86/ZH86 p12. Arrows start from000F; words003F/007F do not establish falling-edge behavior, buffering depth or fresh completion. EDS marks6040 WO: no read-modify-write. |
| `60FF` | EDS I32/default120; manual mode table U32/0..3000rpm. Positive changes use the common encoding but do not resolve reverse/sign-change semantics. EN86 and ZH86 startup orders remain separate. |
| `6060/6061` | EDS U8 versus manual I8; selected positive probe bytes agree.6061 is read-only. A displayed candidate mode does not qualify it. |
| N5 | Its bit9, mode-entry behavior, bit12 exceptions and buffer rule are manufacturer-specific; none transfers to CL86-C. |

Q22?Q26 were added before dependent change behavior to the canonical ledger and
[conflict table](../device_conflicts.md). Existing November3 acceptance/rearm
traces and November13 velocity ladders are useful baselines, but none measured
mid-curve changes. In particular a post-completion000F does not establish the
same edge during a curve. No measurement claim is inferred from host tests.

The independent ownership review found two integration hazards to test: an
acknowledged non-controlword change can otherwise discard the owner's last
controlword and prevent the scheduled stop; a replacement that never transmits
its own trigger can still leave the original move in progress. Both require
retained evidence through the real owner, not a profile-only fake handshake.
