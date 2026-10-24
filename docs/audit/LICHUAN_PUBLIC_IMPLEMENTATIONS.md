# Public implementation evidence for Lichuan CANopen

Audited 2026-10-24. This is source research for the 50-prompt implementation plan.
No library code was implemented, no downloaded program was executed, and no CAN
device was contacted. Original files, exact commits, licenses and SHA256 values
are in [the public-code source pack](../reference/public_code/SOURCES.json).

## Search result and applicability

The search did not establish an official Lichuan GitHub implementation for
CL86-C/OL86-C or CL57-C/OL57-C. This is a bounded search result, not proof that no
such code exists. Manufacturer product/download pages remain the preferred
device evidence. The official Chinese download index includes an **EtherCAT**
example; its presence does not establish a Classical CAN example.

The GitHub connector successfully searched repositories and code, fetched trees,
resolved commits and fetched full files. Exact raw bytes were then downloaded
from commit-pinned GitHub raw URLs for reproducible local hashes. The
[search log](../reference/public_code/search_results.json) records queries and
returned URLs, including false positives and references to this repository.
Searches included `Lichuan motor`, `CL86`, `CL86-C`, `CL86C`, `CL57-C`, `CL57C`,
`LC_StepDriverCAN`, `xlichuan`, and English/Chinese CANopen combinations. Web
searches also covered official Lichuan product/download pages and CANopen examples.
Do not use this repository's indexed research as independent confirmation of itself.

| Candidate | Inspected evidence | Decision |
| --- | --- | --- |
| [SwinRoverTeam/SRT_CAN_Motor](https://github.com/SwinRoverTeam/SRT_CAN_Motor/tree/30ec0a617fdbbcb6d17b42edc85c5210344f6796) | `2026 Season/Example/main.cpp` calls the CANopen devices Lichuan. `SRT_OpenCan` uses the relevant object addresses. Exact drive model, firmware, EDS and successful test trace are absent from inspected files. | Useful concrete implementation and defect evidence; not a CL86-C authority. Four original files retained, including GPL-3.0 license. |
| [amba-pro/motor_liCHuan](https://github.com/amba-pro/motor_liCHuan/blob/9c431ea437b084419cec0a6ba30d9a2052e970aa/README.md) | LC10E-100W with SOEM, EtherCAT process image and CSP. | Different model and physical protocol. Its encoder scale, objects, PDO sizes and 1 ms cycle must not become CL86-C facts. |
| [Tesla900/LichuanServoTest](https://github.com/Tesla900/LichuanServoTest/blob/7c5c4092fde2affe47c8a8d2b391392739181878/README.md) | README identifies DC servo RS485/Modbus. | Different protocol. |
| [pureskywalk/LichuanMotor](https://github.com/pureskywalk/LichuanMotor/tree/c14a490e921efb1b0aa357aa5de4ce5a3d71ccbb) | Tree contains ISL-PR 485 manuals, `src/main/modbus.ts`, serial/Modbus controls and a Windows installer. | No verified manufacturer ownership; not a CANopen implementation reference. Installer was not downloaded or run. |
| Other search hits | CL57R/CL86RS libraries, pulse/direction CL57T/CL86T projects, EtherCAT projects, unrelated names and marketplace text. | A similar part name or shared CiA402 object does not establish applicability. |

The manufacturer also now lists [CL2-86C](https://en.xlichuan.com/product-xq/318.html)
and [CL2-57C](https://en.xlichuan.com/product-xq/316.html), with absolute-encoder
options and a different I/O description. Those are separate candidates. Do not
merge their encoder resolution, seven-DI layout, USB interface or later object
dictionary into the original CL86-C profile without a matched manual and firmware.

## The relevant public example contains defects

All line references in this section refer to the pinned local
[SRT_OpenCan.cpp](../reference/public_code/srt/SRT_OpenCan.cpp), its
[header](../reference/public_code/srt/SRT_OpenCan.h), and
[example main](../reference/public_code/srt/main.cpp). These findings are source
observations; they do not claim that a particular drive accepted or rejected the
example's traffic.

| Location | Concrete finding | Required treatment in the prompts |
| --- | --- | --- |
| CPP 57-78 | `send_sdo_read` constructs request bytes `0x4F`, `0x4B` or `0x43` from expected response width. | Upload-initiate request is `0x40`. Those other forms belong to size-indicated expedited responses. Add an independent request-vector regression in prompt 11. A permissive device accepting reserved bits would not make the request correct. |
| CPP 3-7, 15-36 | Status/controlword caches are file-global, shared by every motor object. Upload parsing tests only command class, not expedited/size flags. | Node/channel/generation-specific state and checked full response shape. A two-motor interleave and segmented response must not populate another axis or become a fabricated statusword. Prompts 11, 13, 23, 25. |
| CPP 40-43 | Write-size selection falls through to four bytes for any size other than one or two. | Reject invalid typed widths before publishing a frame; protocol codec can explicitly support its documented 1-4-byte range. Do not accept arbitrary sizes through a fallback. |
| CPP 81-109 | Enable sends 6, 7, 15 with fixed 10 ms waits. Moves call stop and enable implicitly and ignore most return values. | Every dependent write requires its own SDO/drive evidence. Keep enable/stop explicit and maintain uncertainty on an unconfirmed write. A sleep cannot acknowledge a command. Prompts 13-14, 30, 33. |
| CPP 93-108 | Gear ratio multiplies position, speed **and ramp duration** without checked bounds. | Use exact dimensional conversions. Millisecond ramp duration does not acquire a gear multiplier merely because target position/speed are referred to another shaft. Check signed coordinates, overflow and actual profile units. Prompts 9-10, 33-35. |
| CPP 112-118 | Stop writes 271 (`0x010F`); `Estop` writes 11 (`0x000B`) and sets a local latch. | These values are comparison leads, not verified stopping guarantees or safety-rated emergency stop. Reconcile controlword/options and actual stopping observation with the Lichuan sources. Prompt 31. |
| CPP 120-134; main 105-118 | Running query spins up to 10 ms without calling receive dispatch and returns false on missing reply. Example receives in its ordinary loop. | Service receive/events continuously; timeout is unknown, not stopped. Bit 14 must remain subject to the EN/ZH statusword conflict. Prompts 3, 23, 25, 30. |
| CPP 146-152 | Homing writes `0x6099:01/:02` and `0x609A:00` as two bytes, and method `0x6098:00` as two bytes. | Existing vendor EDS says U32 for the first three and U8 for method. EN86 homing table likewise uses U32/U8; its later I8 method entry is a signedness conflict, still one byte. Add wrong-width tests. Prompt 36. |
| CPP 150 | Homing ramp value 5000 is sent. | EN86 section 3.4.1 states 0-2000 ms; the example is not justification to widen that range. Preserve any source conflict and establish firmware scope. Prompt 36. |
| CPP 159-163 | A controlword read is followed by a 15 ms wait and hardcoded trigger; function returns true without a checked response or completion. | A request being emitted is not acceptance or homing completion. No stale controlword read/modify/write or unconditional success. Prompt 36. |
| CPP 166-175 | Zeroing writes `0x2302` high then low, waits 30 ms each, and unconditionally saves via `0x2300=2`. | The 30 ms waits are example policy, not documented device timing. Separate zero/reference action from explicit persistence and record the required edge; never silently save. Prompts 32 and 39. |
| CPP 177-182 | Fault reset sends `0x0080` immediately followed by zero. | Preserve edge semantics, verified state and uncertainty. Do not infer minimum/maximum edge time or successful alarm clearing. Prompt 32. |
| Header 4-5; main 30-43, 96-101, 131 | Core header includes Arduino/TWAI; send may block 500 ms; startup enables all CANopen motors. | This is not the portable-core or startup contract. Keep SDK/time in the adapter, bound owner service gaps, and leave startup non-actuating. Prompts 6, 23, 27-28. |
| Main 12-26, 105-118 | ODrive and CANopen share a bus based partly on disjoint node numbers. | Different protocol node IDs do not prove disjoint wire identifiers or broadcast behavior. Use existing ODrive protocol evidence to review full routes; do not copy this selection rule. Prompts 4, 23, 46. |

The inspected example provides leads for `0x6040`, `0x6041`, `0x6060`, `0x607A`,
`0x607C`, `0x6081`, `0x6083`, `0x6084`, `0x6098`, `0x6099`, `0x609A`, `0x2300`
and `0x2302`. It provides no independent firmware identification, object-access
conformance, alarm map, PDO qualification, heartbeat loss behavior, persistence
duration, motion completion trace or response-time bound. Do not fill those gaps
from this example.

## Pinned protocol implementation facts

[CANopenNode](https://github.com/CANopenNode/CANopenNode/tree/9b8beed8367241e96ac03f916cd5a500bcb2cf23)
and [canopen-python](https://github.com/canopen-python/canopen/tree/14b3336bc973023e82325426adf6d71046ebcf55)
are primary sources for their own CANopen implementations. They are not Lichuan
firmware sources or substitutes for a device manual. The pack preserves bounded
source files and original Apache-2.0/MIT licenses. Existing protocol standards and
the selected stack's configuration still need review in prompt 5.

| Fact | Exact pinned source | Scope |
| --- | --- | --- |
| SDO header is command byte, little-endian index, subindex. Upload initiate is `0x40`. | Python `constants.py` 5-20; `client.py` 252-255; CANopenNode `CO_SDOclient.c` 1542-1545. | Protocol encoding. |
| Expedited sized download commands for 1/2/3/4 bytes are `0x2F/0x2B/0x27/0x23`; sized upload responses are `0x4F/0x4B/0x47/0x43`. | Python `client.py` 269-275, 373-377; `constants.py` 8-30. | Derived from the command/unused-byte bit formula; not evidence of a specific device object or transfer choice. |
| Download acknowledgement `0x60` must match current index/subindex. | CANopenNode `CO_SDOclient.c` 587-615. | Also bind channel/node and transaction state. A repeated same-object write still has indistinguishable late acknowledgements. |
| `0x80` carries a little-endian 32-bit abort code in bytes 4-7. | CANopenNode `CO_SDOclient.c` 1206-1211; Python `client.py` 78-83. | Preserve unknown raw codes; an SDO abort is separate from a drive alarm or EMCY. |
| Expedited upload with size flag clear is valid; small objects can also use segmented initiation. | CANopenNode `CO_SDOclient.c` 1231-1256; Python `client.py` 269-281. | Do not treat expected U16 width as proof the server must answer `0x4B`. A size-unspecified reply carries four raw bytes; typed interpretation requires reviewed metadata. |
| NMT command payload is command/node; node zero is broadcast. Commands start/stop/pre-op/reset-node/reset-communication are `01/02/80/81/82` hex. | CANopenNode `CO_NMT_Heartbeat.h` 51-56, 81-88; Python `nmt.py` 149-150 sends CAN-ID zero. | NMT intent is not observed state or CiA402 stop. |
| Heartbeat payload state values are 0 initializing/boot-up, 4 stopped, 5 operational, 127 pre-op. | CANopenNode `CO_NMT_Heartbeat.h` 58-75; Python `nmt.py` 228, 262. | Boot-up and periodic heartbeat have different evidence effects. |
| CANopenNode heartbeat monitor distinguishes unconfigured, unknown, active and timeout; monitoring begins after first heartbeat, not boot-up. | `CO_HBconsumer.h` 48-66. | Stack behavior. The host can have its own explicitly configured startup deadline; no missing heartbeat proves offline while producer is disabled. |
| First four default TPDO IDs are `0x180/280/380/480 + node`; RPDO IDs `0x200/300/400/500 + node`. | `CO_PDO.h` 82-86. | Defaults only; read actual COB-ID and disable/format flags. Never overwrite active configuration from EDS defaults. |
| PDO mapping descriptor packs index in upper 16 bits, subindex in next 8 and bit count in low 8. | `CO_PDO.c` 89-95, 337-344. | `0x60410010` means statusword/sub0/16 bits. Width, access, byte order, sum and actual map require independent checks. |
| Disable PDO, clear mapping count, write entries, restore count, enable PDO is the documented CANopenNode remap order. | `CO_PDO.h` 93-103. | Its dynamic remap configuration can allow operational remapping; this does not establish Lichuan support/state requirements. Use an explicit reviewed device sequence. |
| CANopenNode exposes transmission types 0, 1-240, 254, 255. TPDO inhibit-time conversion is value x 100 microseconds; event-time conversion is value x 1000 microseconds. | `CO_PDO.h` 141-145; `CO_PDO.c` 1027-1034, 1162-1163. | Protocol/stack timing units. Do not conflate these with Lichuan ramp milliseconds or assume every type is implemented by the drive. |
| Emergency payload begins with 16-bit error code and 8-bit error register. | `CO_Emergency.h` 69-78. | CANopenNode uses byte 3 for its own error-condition index and bytes 4-7 for its argument. Keep all five manufacturer bytes opaque for Lichuan until its source defines them. |
| Generic CiA402 state masks include `(0x6F,0x27)` for operation enabled and `(0x4F,0x08)` for fault. | Python `p402.py` 40-49. | Comparison evidence only. Never apply these masks to contradictory CL86-C status documentation without qualification. |

### Abort-code seeds

Python `canopen/sdo/constants.py` 38-68 preserves a useful complete set of named
codes. Important host fixtures are `0x05030000` toggle, `0x05040000` timeout,
`0x05040001` invalid command, `0x06010000` unsupported access, `0x06010001`
read of write-only, `0x06010002` write of read-only, `0x06020000` absent object,
`0x06040041` cannot PDO-map, `0x06040042` PDO length, `0x06070010/12/13`
type-length mismatch/high/low, `0x06090011` absent subindex,
`0x06090030/31/32` invalid/high/low value, `0x08000020` application transfer/store,
`0x08000021` local control and `0x08000022` device state. This is a decoder seed,
not a claim that CL86-C emits each code. Unknown codes must survive unchanged.

### Timing facts and limits

Do not install any timing constant merely because a public library contains it.
The source ledger must distinguish **device requirement**, **protocol unit**,
**application deadline**, **example delay**, and **measured observation**.

| Constant | Actual meaning | Not established |
| --- | --- | --- |
| SRT 10, 15 and 30 ms | Sleeps/busy-wait choices in the example. | CL86-C required gap, maximum response, save time or edge width. |
| SRT 500 ms | TWAI enqueue/blocking API timeout in its example. | Wire completion, SDO success or permissible owner service gap. |
| Python SDO `RESPONSE_TIMEOUT=0.3`, `MAX_RETRIES=1`, `PAUSE_BEFORE_SEND=0`, `RETRY_DELAY=0.1` | Configurable host policy at `client.py` 23-32; retry loops at 50-64 and 87-101. At the pinned default, one attempt is made by each loop; raising retries can replay requests. | Device timing guarantee or permission to replay uncertain motor writes. Audit both layers and adapter retransmission behavior. |
| Python CiA402 0.4/0.5/0.8/0.2/30 seconds | Generic fault/mode/state/TPDO/homing deadlines in `p402.py` 207-212. | CL86-C transition or homing bounds. |
| CANopenNode caller-provided `SDOtimeoutTime_ms` | Converted to microseconds at `CO_SDOclient.c` 377, 1058 and accumulated from caller elapsed time. | A manufacturer timeout. Caller scheduling, queue age and end-to-end operation deadline remain separate. |
| PDO inhibit x100 us and event x1000 us | Object-unit conversions in CANopenNode. | Motion ramp time or drive response latency. |

## Host-only byte fixtures and prompt use

[protocol_vectors.json](../reference/public_code/protocol_vectors.json) contains
17 reviewed, source-derived fixtures. They are synthetic bytes, not captured
traffic and not instructions to transmit. Node 5 is an example, not this user's
node assignment. The file includes `0x6041` upload, U8/U16/U32 downloads, I32
negative encoding, acknowledgement, abort, size-unspecified/segmented upload,
NMT, heartbeat, boot-up, mapping descriptors and the two concrete bad examples.

For example, a default-channel statusword upload request at node 5 is CAN-ID
`0x605`, DLC 8, `40 41 60 00 00 00 00 00`. A synthetic two-byte reply is
`0x585`, DLC 8, `4B 41 60 00 34 12 00 00`; it yields raw `0x1234`, without a
qualified meaning for its state bits. A four-byte ramp value 100 at `0x6083:00`
encodes `23 83 60 00 64 00 00 00`. Source-backed data width is essential even
when the numeric value fits one byte.

Prompt 11 owns basic encoding/rejection fixtures. Prompts 13-14 own late/wrong
channel/index/subindex, abort, valid unsupported segmentation, timeout and
uncertain-write settlement. Prompts 15-18 own network/PDO event interpretation
and configuration. Prompts 23-25 own multi-node isolation and continuous dispatch.
Prompts 30-36 must independently reconcile the manufacturer action/state/units
before applying any object sequence. Prompt 39 keeps parameter persistence
explicit. Prompts 43-48 must not conceal retries or turn synthetic fixtures into
hardware qualification.

Before using a finding, reopen its original manual page, EDS section and pinned
code. Match object index/subindex, type, access, units, limits, effect and firmware
scope. If sources disagree, preserve both statements, identify the affected
capability and obtain the precise evidence needed to resolve it. Correct the
reviewed ledger, generated declarations, public function, tests and documentation
together. A copied constant, AI assertion, passing fake test or working example
for another drive is not proof. Missing hard data stays missing; it must never be
replaced by a guessed default, a longer sleep or automatic retry.

## Ecosystem refresh and verification

The relevant sibling checkouts were read-only and clean during this audit:
MotorControl-RS `bd831c859daa9c8c07b8d27a2dd879bb4e750511`, ADS1115
`235af3e8c722661a89a44766a4ccd903a654f6fe`, INA228
`b28c5ea69bcb4d26b351c52c24d3c6fae9fd282b`, and FieldCore-node
`a002abae533b0aa767c235c2197d3b548bdcdd83`. Targeted reads covered RS `Status.h`
and `src/profiles/ess_rs/Velocity.cpp` 127-150, ADS1115 conversion readback/polling
around 880-904, INA228 hardware synchronization checks around 3060-3077, and
FieldCore `contracts/Health.h`. Their existing validation/status/evidence
separation fits the planned contract. Sensor polling/retry policy and third-party
Arduino motion flow do not change the RS common motion vocabulary or become
automatic motor retries. RS bounded velocity still begins its duration at
successful preparation, with setup time consuming that duration.

Local work consists of 17 unchanged upstream files with hashes/licenses, one
bounded search log and 17 synthetic vector records. All 19 artifact byte/hash
records and all 17 vector lengths were checked. Independent standard-library
packing checked the upload request, U32 ramp, negative I32 target, mapping
descriptors and abort value. At worker handoff, `python scripts/check_promptset.py`
passed with four reference packs and 122 hashed artifacts; `git diff --check`
passed. The lead repeats combined and staged/committed artifact checks after final
integration. No upstream project test suite or CAN firmware was built; these
findings are source audit results, not compilation or hardware claims.
