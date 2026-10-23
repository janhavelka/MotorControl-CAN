# Primary references for other CAN motor protocols

This reference-only pack preserves official sources for the
[non-CANopen protocol audit](../../audit/OTHER_CAN_PROTOCOLS.md). It is not a
driver implementation or a claim of supported hardware. None of these files
belongs in the MotorControl-CAN build, installed headers or exported core.
Do not execute the downloaded Python/C/C++ files.

[SOURCES.json](SOURCES.json) schema version 1 contains a `sources` array with
`id`, `title`, directory-relative `path`, original `url`, `resolved_url`,
`retrieved_on`, retrieval status, exact `revision`, byte count, SHA-256,
applicability and upstream-rights notes. All originals were retrieved on
2026-10-23. Preserve their bytes; an intentional refresh must update provenance
and the associated protocol review. No changing `main`/`master` URL is used for
the retained GitHub source files.

| Source group | Pinned identity | Local files |
| --- | --- | --- |
| ODrive CANSimple | Official documentation version 0.6.12 | [Protocol HTML](odrive_can_protocol_0.6.12.html) |
| moteus | `8f747f4ac448d117c1390b77ca2d80c27e751a56` | [CAN protocol](moteus_can.md), [registers](moteus_registers.md), [C++ protocol schema](moteus_protocol.h), [client frame construction](moteus_transport.py), [control modes](moteus_control_modes.md), [timeout mode](moteus_timeout.md) |
| VESC native CAN | `abc9217fb2b7ee3821f1742bc32b269173643319` | [Protocol guide](vesc_comm_can.md), [CAN implementation](vesc_comm_can.c), [timeout implementation](vesc_timeout.c), [version declarations](vesc_conf_general.h) |

There are 11 retained original files totaling 371,308 bytes. The VESC source
declares firmware 7.01, but this is a commit snapshot, not a release-tag or
hardware-identification assertion. The moteus commit likewise must be matched
to actual firmware/register-map identity before support is claimed.

The originals retain upstream-relative hyperlinks, documentation-site syntax
and dependencies. Some are absent from this selective pack. Resolve them
against the URL and revision in the manifest; do not rewrite originals to make
this directory look like a complete project. Maintained MotorControl-CAN
documents outside the originals must resolve their own links.

The audit records the moteus extended-ID prose discrepancy (`0x7fff` in the
protocol text versus `0x7ff` in official client frame construction) and the
distinction between VESC timeout prose and actual recognized setpoint handlers.
Do not silently normalize those source differences.

Original upstream licenses and notices remain applicable. In particular, the
VESC source carries GPL notices; it is protocol research material, not code
relicensed under the MotorControl-CAN MIT license. Consult source licenses
before any implementation reuse or third-party dependency decision.
