# Generated Lichuan native declarations and semantic coverage

Generated from `docs/reference/lichuan_objects.json` by `python scripts/generate_lichuan_objects.py`. Do not edit.

This is documentary metadata in `MotorControlCAN::LICHUAN_C`. The EDS family has no installed model/firmware binding. Generated descriptors implement no typed reads, typed settings, actions or workflows. No entry is hardware-qualified. Vendor source material retains its separate licensing and is excluded from the core distribution.

`Objects.h` requires only `Types.h`. `Catalogue.h` is optional descriptive text; minimal consumers neither include it nor link a dictionary. SDO direction, read effect, write qualification and PDO eligibility remain separate. Lichuan 0x2300 save/reset must not be interpreted as Nanotec N5 0x2300.

## Accounting and implementation states

| Measure | Count | Meaning |
| --- | ---: | --- |
| EDS indexes | 99 | Scalars, arrays and records, not implemented functions |
| Explicitly typed EDS entries | 202 | Separate from index count |
| Resolved compact descriptors | 194 | Native type and restrictive policy only |
| Withheld declarations | 4 | Unresolved complete signed type, raw claims retained |
| Excluded reserved entries | 4 | No descriptor or operation route |
| Manual-only semantic items | 53 | Reviewed evidence, semantic functions pending |
| Typed reads | 0 | Assigned semantic prompts remain pending |
| Typed settings | 0 | Assigned semantic prompts remain pending |
| Actions | 0 | Assigned semantic prompts remain pending |
| Workflows | 0 | Assigned semantic prompts remain pending |

Coverage `D/I/H/Q` means documented / semantic implementation / host-tested semantic function / hardware-qualified. Generator and declaration tests do not change a semantic coverage field. `resolved` describes native declaration scope only; other conflicts and every listed qualification gate remain visible. `withheld` and `excluded` entries have no ObjectDescriptor or ObjectMetadata constant.

## Original source identity and applicability

| Source | Title and version | Acquired file / SHA-256 | URL | Applicability evidence |
| --- | --- | --- | --- | --- |
| EN86 | CL86-C/OL86-C CANopen Bus-Type Step Drive User Manual; No explicit printed revision identified; metadata dates do not establish firmware applicability. | docs/reference/vendor/86C_CANopen_user_manual_en.pdf; 401df6c49c54fe56fed7c606ca45718f352235746e2faaeb65be778eef6c4b9f | https://omo-oss-file110.thefastfile.com/portal-saas/pg2024121216272237966/cms/file/86c-canopen bus stepper driver manual.pdf | {"applicability_evidence": "PDF cover page 1; installed firmware match unverified", "models_named": ["CL86-C", "OL86-C"]} |
| ZH86 | CL86-C/OL86-C CANopen user manual; No explicit printed revision identified; metadata dates do not establish firmware applicability. | docs/reference/vendor/86C_CANopen_user_manual_zh.pdf; 3969c914aa291662e8c77941bcfbb0559769a96b75306266370890d03e824d13 | https://omo-oss-file110.thefastfile.com/portal-saas/pg2024121216272237966/cms/file/86c系列-canopen总线驱动使用手册(2).pdf | {"applicability_evidence": "PDF cover page 1; installed firmware match unverified", "models_named": ["CL86-C", "OL86-C"]} |
| OP86 | OL86C and CL86C operating manual; No explicit printed revision identified; metadata dates do not establish firmware applicability. | docs/reference/vendor/CL86C_OL86C_operating_manual_zh.pdf; 4c87679d210c99f4ac4f6ce29e7f2c82b13221fdf81c47ee59a989f708f504b5 | https://omo-oss-file110.thefastfile.com/portal-saas/pg2024121216272237966/cms/file/ol86c和cl86c操作手册(4).pdf | {"applicability_evidence": "PDF cover page 1; installed firmware match unverified", "models_named": ["CL86-C", "OL86-C"]} |
| ZH57 | CL57-C/OL57-C CANopen user manual; No explicit printed revision identified; metadata dates do not establish firmware applicability. | docs/reference/vendor/57C_CANopen_user_manual_zh.pdf; 47ce117cda7498210c09533e7d05dcd83ecb51e64cdd62ba054750b8ed3cf356 | https://omo-oss-file110.thefastfile.com/portal-saas/pg2024121216272237966/cms/file/57c系列-canopen总线驱动使用手册(2).pdf | {"applicability_evidence": "PDF cover page 1; installed firmware match unverified", "models_named": ["CL57-C", "OL57-C"]} |
| OP57 | OL57C and CL57C operating manual; No explicit printed revision identified; metadata dates do not establish firmware applicability. | docs/reference/vendor/CL57C_OL57C_operating_manual_zh.pdf; ce61bb1c1fde3cf689a2e8078b0823eb2734382b7713c0448006017a60396343 | https://omo-oss-file110.thefastfile.com/portal-saas/pg2024121216272237966/cms/file/ol57c和cl57c操作手册(4).pdf | {"applicability_evidence": "PDF cover page 1; installed firmware match unverified", "models_named": ["CL57-C", "OL57-C"]} |
| EDS | LC_StepDriverCAN_V1.1.eds; EDSVersion=4.0; FileVersion=0x01; FileRevision=0x01; creation 2020-12-23; modification 2022-11-11 | docs/reference/vendor/LC_StepDriverCAN_V1.1.eds; dd3c96e7282c012a522dddb5bbdbbf561c1f5c23716b31f12472f45829d4f52c | https://omo-oss-file110.thefastfile.com/portal-saas/pg2024121216272237966/cms/file/lc_stepdrivercan_v1.1.eds | {"applicability_evidence": "DeviceInfo; manufacturer download index, exact model and firmware unverified", "models_named": ["Lichuan CANopen stepper family; exact model/firmware applicability unverified"]} |

## Source scopes

| Scope | Manufacturer, model, dialect and firmware facts |
| --- | --- |
| eds_family_unbound | {"binding": "unverified", "dialect": "LC_StepDriverCAN_V1.1.eds", "family": "Lichuan CANopen stepper drive family", "firmware": null, "id": "eds_family_unbound", "manufacturer": "Lichuan", "models": []} |
| manual_86 | {"binding": "documentary cover only; OP86 internal 57C headers conflict", "dialect": "EN86/ZH86/OP86 claims kept per source", "family": "86C CANopen drives", "firmware": null, "id": "manual_86", "manufacturer": "Lichuan", "models": ["CL86-C", "OL86-C"]} |
| manual_57_contrast | {"binding": "documentary cover only; not an implemented profile", "dialect": "ZH57/OP57 contrast claims kept per source", "family": "57C CANopen drives", "firmware": null, "id": "manual_57_contrast", "manufacturer": "Lichuan", "models": ["CL57-C", "OL57-C"]} |

## Reviewed model binding and capabilities

No installed firmware binding. User-selected model bindings must retain provenance and satisfy only the gates needed by each operation. No write or PDO mapping is approved by an EDS flag alone.

| Model | Reviewed family facts and limitations |
| --- | --- |
| CL86-C | {"binding": "user_selected_unqualified", "capabilities": {"canopen": "documented_unqualified", "cyclic_modes": "not_evidenced", "homing": "documented_unqualified", "position": "documented_unqualified", "servo_motor": "not_evidenced", "torque": "not_evidenced", "velocity": "documented_unqualified"}, "encoder_capability": "documented_present", "firmware": null, "hardware_qualified": false, "implemented": false, "limitations": "Encoder resolution does not establish 6064 scale. OP86 internal 57C headers and supply conflicts limit cross-document applicability.", "model": "CL86-C", "motor_feedback": "Closed-loop stepper; product table claims 1000-line encoder.", "source_refs": [{"id": "EN86", "page": 1}, {"id": "EN86", "page": 3}]} |
| OL86-C | {"binding": "user_selected_unqualified", "capabilities": {"canopen": "documented_unqualified", "cyclic_modes": "not_evidenced", "homing": "documented_unqualified", "position": "documented_unqualified", "servo_motor": "not_evidenced", "torque": "not_evidenced", "velocity": "documented_unqualified"}, "encoder_capability": "explicitly_absent", "firmware": null, "hardware_qualified": false, "implemented": false, "limitations": "Do not advertise measured rotor feedback or closed-loop tuning solely because generic EDS lists it.", "model": "OL86-C", "motor_feedback": "Open-loop stepper; the product table explicitly states no encoder. Reported position is not thereby measured rotor position.", "source_refs": [{"id": "EN86", "page": 1}, {"id": "EN86", "page": 3}]} |
| CL57-C | {"binding": "user_selected_unqualified", "capabilities": {"canopen": "documented_unqualified", "cyclic_modes": "not_evidenced", "homing": "documented_unqualified", "position": "documented_unqualified", "servo_motor": "not_evidenced", "torque": "not_evidenced", "velocity": "documented_unqualified"}, "encoder_capability": "documented_present", "firmware": null, "hardware_qualified": false, "implemented": false, "limitations": "57C status and output labels are separate evidence; no 86C firmware inheritance.", "model": "CL57-C", "motor_feedback": "Closed-loop stepper; product table claims 1000-line encoder.", "source_refs": [{"id": "ZH57", "page": 1}, {"id": "ZH57", "page": 3}]} |
| OL57-C | {"binding": "user_selected_unqualified", "capabilities": {"canopen": "documented_unqualified", "cyclic_modes": "not_evidenced", "homing": "documented_unqualified", "position": "documented_unqualified", "servo_motor": "not_evidenced", "torque": "not_evidenced", "velocity": "documented_unqualified"}, "encoder_capability": "explicitly_absent", "firmware": null, "hardware_qualified": false, "implemented": false, "limitations": "No measured rotor feedback or closed-loop capability inferred from shared dictionary. No 86C firmware inheritance.", "model": "OL57-C", "motor_feedback": "Open-loop stepper; the product table explicitly states no encoder. Reported position is not thereby measured rotor position.", "source_refs": [{"id": "ZH57", "page": 1}, {"id": "ZH57", "page": 3}]} |

## Qualification gates

Gate bits in descriptors use Q01 at bit zero through Q21 at bit twenty. All are open.

| Gate | Feature | Evidence required | Models | Owners |
| --- | --- | --- | --- | --- |
| Q01 | Model and firmware binding / identity | Retain drive label, selected CL/OL model, firmware identification and raw identity reads; compare 1009/100A, 1018:01..03, 2000/2001. Generic EDS IDs cannot select the exact model. A reviewed user selection must retain provenance. Raw identity acquisition may precede exact binding; this gate controls model-specific interpretation/effects, not passive discovery. | ["CL86-C", "OL86-C", "CL57-C", "OL57-C"] | [19, 37] |
| Q02 | 6041 interpreted status and completion | For the exact firmware record raw 6041 together with independently known fault, enabled/released, moving, standstill, homing and positioning states. Distinguish bits 3/7/10/12/14; verify freshness/reset behavior of bits 8/15. Do not combine the three 86C layouts. For 57C, qualify its own cited bit7/14 layout and freshness; resolving 86C alternatives is not a 57C prerequisite. | ["CL86-C", "OL86-C", "CL57-C", "OL57-C"] | [19, 30, 31, 32, 33, 35, 36] |
| Q03 | 60FF signedness and velocity start | Obtain applicable manufacturer clarification or an authorized isolated finite experiment proving positive/negative encoding, target retention, parameter-before-enable order and the actual start transition. No trial of both start recipes on an ordinary bench. | ["CL86-C", "OL86-C", "CL57-C", "OL57-C"] | [30, 35] |
| Q04 | Command / feedback / subdivision scale | Establish nonzero 2201 accepted range and effect, command increments per revolution, independent 6064/606C feedback scale/sign and encoder line/count basis against measured displacement. Record motor, gearing, loop mode and configuration generation. | ["CL86-C", "OL86-C", "CL57-C", "OL57-C"] | [19, 20, 22, 33, 34, 36] |
| Q05 | 2202 / 607D software limits | Read active settings; establish accepted enable codes, coordinate basis, homing prerequisite, boundary inclusion and reaction without relying on EDS default 10. An isolated authorized boundary test needs independent travel protection. | ["CL86-C", "OL86-C", "CL57-C", "OL57-C"] | [20, 33, 36] |
| Q06 | 86C output addresses / functions / polarity | Obtain exact firmware object map for 2400:0B..0F, function values 2/3 and polarity bits for all three terminals; verify 60FE command/mask against identified isolated outputs. Never try both disputed assignment writes. For this selected family, qualify output command/mask clearing and unchanged-bit behavior; the both-bits-one condition is not a complete write truth table. | ["CL86-C", "OL86-C"] | [21] |
| Q07 | 57C output physical identity | Resolve ZH57 p8 DO0/1/2 versus p17 DO1/2/3 by an applicable terminal map and isolated per-bit observation. This cannot resolve 86C firmware. For this selected family, qualify output command/mask clearing and unchanged-bit behavior; the both-bits-one condition is not a complete write truth table. | ["CL57-C", "OL57-C"] | [21] |
| Q08 | Input filter coefficients / defaults | Establish meaning of 2400:0A versus Reserved and 2401..2407 units, equation, accepted bounds and effective timing. Read actual defaults; do not turn a coefficient into milliseconds or write the reserved field. | ["CL86-C", "OL86-C", "CL57-C", "OL57-C"] | [21] |
| Q09 | Tuning / current / feedback applicability | Obtain model-specific admissible values, units, loop prerequisites and effects for the selected 2500..2520 field. Establish motor current ratings separately; encoder/closed-loop controls need an actual feedback-capable model. No inferred autotune or torque interface. | ["CL86-C", "OL86-C", "CL57-C", "OL57-C"] | [22] |
| Q10 | One-byte mode / homing codes | Retain the raw byte; documented positive modes 1/3/6 and methods 17/18/24/29 have identical U8/I8 wire encoding. Obtain evidence before interpreting any high-bit/negative code or default method zero. | ["CL86-C", "OL86-C", "CL57-C", "OL57-C"] | [19, 20, 36] |
| Q11 | Enable / position trigger and ramp boundary | For the current mode/state establish retained-target behavior, enable/release effect, bit-4 trigger/rearm and target-acceptance evidence. Verify zero ramp behavior before accepting it; approximate 10 ms between example transitions is not a response/completion deadline. | ["CL86-C", "OL86-C", "CL57-C", "OL57-C"] | [20, 30, 33, 35] |
| Q12 | Normal / emergency stop and 605D | Obtain current-state stop transitions, applicable 605D option meanings, ramp/deceleration behavior and observable completion. Measure latency separately. Never send 15 merely to prepare 271 or 11; absent 605A/6085 is not a fallback. | ["CL86-C", "OL86-C", "CL57-C", "OL57-C"] | [31] |
| Q13 | Fault-clear effect / rearm | Establish which faults are resettable, 6040 bit-7 assertion/manual-return behavior, 2301 trigger/rearm and equivalence or differences. Record current state; never enable as reset preparation or write 603F. | ["CL86-C", "OL86-C", "CL57-C", "OL57-C"] | [32] |
| Q14 | Position clear / offset reference effects | Verify 2302 zero-to-one clear and manual return, affected command/feedback counters, 607C application point and resulting reference validity. A zero command is not arbitrary position setting, host-origin setting or successful homing. | ["CL86-C", "OL86-C", "CL57-C", "OL57-C"] | [32, 34, 36] |
| Q15 | Save / factory-reset completion and scope | Obtain 2300 trigger/rearm, affected parameter set, acknowledgement versus durable-completion evidence, interrupted-write behavior and restart/route consequences. Values 2 save and 1 factory reset are explicit actions; no guessed wait or automatic replay. | ["CL86-C", "OL86-C", "CL57-C", "OL57-C"] | [39] |
| Q16 | Node / bitrate / configurable routes | Establish DIP precedence and activation/restart/persistence for 2203 and routing fields; identify a supported way to select EDS-only 50 kbit/s before offering it. Verify full route field masks and new/old route retirement with a retained recovery plan. | ["CL86-C", "OL86-C", "CL57-C", "OL57-C"] | [16, 37, 38] |
| Q17 | PDO eligibility / remapping / freshness | Read actual map counts and slots and verify direction/access for the selected model/firmware. Confirm applicability of CiA variable/dynamic mapping and any device-specific NMT/drive restrictions, COB-ID flags, activation and complete readback conditions for the selected remap subset; the standard disable/count-zero/entries/count/enable procedure is documented, not an unknown protocol order. Record persistence effects without assuming volatile-only writes. Gate changes to transmission mode, inhibit/event units, ranges and zero behavior separately for RPDO and TPDO; unchanged settings do not require every timer or SYNC capability. Qualify producer transition and queued RX/TX retirement, distinct route or observable marker before qualifying samples; that separate gate does not prevent confirmation of map contents. Map readback and host generation alone cannot qualify same-ID/same-DLC delayed samples. | ["CL86-C", "OL86-C", "CL57-C", "OL57-C"] | [17, 18] |
| Q18 | Service / network event behavior | Observe actual upload forms, NMT transitions, heartbeat/EMCY/SYNC behavior and reset recovery for the binding. No manufacturer host-loss stop guarantee follows from 1017=0 or absent 1016; obtain applicable drive reaction evidence separately. | ["CL86-C", "OL86-C", "CL57-C", "OL57-C"] | [13, 14, 15, 16, 25] |
| Q19 | Physical electrical fixture | Verify model label and applicable manufacturer electrical limits, board/transceiver/wiring/termination, motor/encoder, input assignments and authorization. Resolve 86C and 57C supply conflicts separately; do not use a document range as bench permission. Resolve only the selected model supply conflict. Establish the independent 60FD bit-to-function layout before decoding it; 2101 is not its mapping source. | ["CL86-C", "OL86-C", "CL57-C", "OL57-C"] | [29, 47, 48] |
| Q20 | Read side effects / polling | For each candidate polled observation establish whether upload consumes or clears state and whether repeated reads affect diagnostics. Preserve an explicit unknown until a manufacturer statement or matched-firmware observation supports non-consuming use. | ["CL86-C", "OL86-C", "CL57-C", "OL57-C"] | [19] |
| Q21 | Homing method / completion / reference | Verify actual switch assignment/polarity, safe direction/travel, method-specific edge behavior, interrupt result, offset and fresh completion evidence before publishing a home reference. Physical inputs are required only for the selected method. | ["CL86-C", "OL86-C", "CL57-C", "OL57-C"] | [36] |

## EDS structure: all indexes

Counts and reserved subentries retain their own explicit native types. A record's SubNumber is structural metadata, not a synthesized child. In particular 0x1018 has only 00..03; no 04 is fabricated.

| Index | Kind | EDS section | Original header fields |
| --- | --- | --- | --- |
| 0x1000 | scalar | EDS [1000] lines 158-166 | {"AccessType": "RO", "DataType": "0x0007", "DefaultValue": "0x40192", "ObjFlags": "0x00000000", "ObjectType": "7", "PDOMapping": "0", "ParameterName": "Device Type"} |
| 0x1001 | scalar | EDS [1001] lines 167-175 | {"AccessType": "RO", "DataType": "0x0005", "DefaultValue": "0", "ObjFlags": "0x00000000", "ObjectType": "7", "PDOMapping": "0", "ParameterName": "Error Register"} |
| 0x1003 | record | EDS [1003] lines 176-181 | {"ObjFlags": "0x00000000", "ObjectType": "9", "ParameterName": "Pre-defined Error Field", "SubNumber": "0x05"} |
| 0x1005 | scalar | EDS [1005] lines 225-233 | {"AccessType": "RW", "DataType": "0x0007", "DefaultValue": "0x80", "ObjFlags": "0x00000000", "ObjectType": "7", "PDOMapping": "0", "ParameterName": "SYNC COB ID"} |
| 0x1009 | scalar | EDS [1009] lines 234-242 | {"AccessType": "RO", "DataType": "0x0006", "DefaultValue": "0x2608", "ObjFlags": "0x00000000", "ObjectType": "7", "PDOMapping": "0", "ParameterName": "hardware version"} |
| 0x100A | scalar | EDS [100A] lines 243-251 | {"AccessType": "RO", "DataType": "0x0006", "DefaultValue": "1", "ObjFlags": "0x00000000", "ObjectType": "7", "PDOMapping": "0", "ParameterName": "software version"} |
| 0x1014 | scalar | EDS [1014] lines 252-260 | {"AccessType": "RW", "DataType": "0x0007", "DefaultValue": "$NODEID+0x80", "ObjFlags": "0x00000000", "ObjectType": "7", "PDOMapping": "0", "ParameterName": "Emergency COB ID"} |
| 0x1017 | scalar | EDS [1017] lines 261-269 | {"AccessType": "RW", "DataType": "0x0006", "DefaultValue": "0", "ObjFlags": "0x00000000", "ObjectType": "7", "PDOMapping": "0", "ParameterName": "Producer Heartbeat Time"} |
| 0x1018 | record | EDS [1018] lines 270-275 | {"ObjFlags": "0x00000000", "ObjectType": "9", "ParameterName": "Identity Object", "SubNumber": "0x04"} |
| 0x1200 | record | EDS [1200] lines 314-319 | {"ObjFlags": "0x00000000", "ObjectType": "9", "ParameterName": "Server SDO Parameter", "SubNumber": "0x03"} |
| 0x1400 | record | EDS [1400] lines 347-352 | {"ObjFlags": "0x00000000", "ObjectType": "9", "ParameterName": "Receive PDO 1 Parameter", "SubNumber": "0x06"} |
| 0x1401 | record | EDS [1401] lines 407-412 | {"ObjFlags": "0x00000000", "ObjectType": "9", "ParameterName": "Receive PDO 2 Parameter", "SubNumber": "0x06"} |
| 0x1402 | record | EDS [1402] lines 467-472 | {"ObjFlags": "0x00000000", "ObjectType": "9", "ParameterName": "Receive PDO 3 Parameter", "SubNumber": "0x06"} |
| 0x1403 | record | EDS [1403] lines 527-532 | {"ObjFlags": "0x00000000", "ObjectType": "9", "ParameterName": "Receive PDO 4 Parameter", "SubNumber": "0x06"} |
| 0x1600 | record | EDS [1600] lines 587-592 | {"ObjFlags": "0x00000000", "ObjectType": "9", "ParameterName": "Receive PDO 1 Mapping", "SubNumber": "0x05"} |
| 0x1601 | record | EDS [1601] lines 639-644 | {"ObjFlags": "0x00000000", "ObjectType": "9", "ParameterName": "Receive PDO 2 Mapping", "SubNumber": "0x05"} |
| 0x1602 | record | EDS [1602] lines 690-695 | {"ObjFlags": "0x00000000", "ObjectType": "9", "ParameterName": "Receive PDO 3 Mapping", "SubNumber": "0x05"} |
| 0x1603 | record | EDS [1603] lines 739-744 | {"ObjFlags": "0x00000000", "ObjectType": "9", "ParameterName": "Receive PDO 4 Mapping", "SubNumber": "0x05"} |
| 0x1800 | record | EDS [1800] lines 788-793 | {"ObjFlags": "0x00000000", "ObjectType": "9", "ParameterName": "Transmit PDO 1 Parameter", "SubNumber": "0x06"} |
| 0x1801 | record | EDS [1801] lines 848-853 | {"ObjFlags": "0x00000000", "ObjectType": "9", "ParameterName": "Transmit PDO 2 Parameter", "SubNumber": "0x06"} |
| 0x1802 | record | EDS [1802] lines 908-913 | {"ObjFlags": "0x00000000", "ObjectType": "9", "ParameterName": "Transmit PDO 3 Parameter", "SubNumber": "0x06"} |
| 0x1803 | record | EDS [1803] lines 968-973 | {"ObjFlags": "0x00000000", "ObjectType": "9", "ParameterName": "Transmit PDO 4 Parameter", "SubNumber": "0x06"} |
| 0x1A00 | record | EDS [1A00] lines 1028-1033 | {"ObjFlags": "0x00000000", "ObjectType": "9", "ParameterName": "Transmit PDO 1 Mapping", "SubNumber": "0x05"} |
| 0x1A01 | record | EDS [1A01] lines 1080-1085 | {"ObjFlags": "0x00000000", "ObjectType": "9", "ParameterName": "Transmit PDO 2 Mapping", "SubNumber": "0x05"} |
| 0x1A02 | record | EDS [1A02] lines 1131-1136 | {"ObjFlags": "0x00000000", "ObjectType": "9", "ParameterName": "Transmit PDO 3 Mapping", "SubNumber": "0x05"} |
| 0x1A03 | record | EDS [1A03] lines 1180-1185 | {"ObjFlags": "0x00000000", "ObjectType": "9", "ParameterName": "Transmit PDO 4 Mapping", "SubNumber": "0x05"} |
| 0x2000 | scalar | EDS [2000] lines 1229-1237 | {"AccessType": "RO", "DataType": "0x0006", "DefaultValue": "1", "ObjFlags": "0x00000000", "ObjectType": "7", "PDOMapping": "1", "ParameterName": "Obj.driverID"} |
| 0x2001 | scalar | EDS [2001] lines 1238-1246 | {"AccessType": "RO", "DataType": "0x0006", "DefaultValue": "0", "ObjFlags": "0x00000000", "ObjectType": "7", "PDOMapping": "1", "ParameterName": "Obj.softversion"} |
| 0x2100 | scalar | EDS [2100] lines 1247-1255 | {"AccessType": "RW", "DataType": "0x0006", "DefaultValue": "8", "ObjFlags": "0x00000000", "ObjectType": "7", "PDOMapping": "1", "ParameterName": "Obj.MonitorWord"} |
| 0x2101 | scalar | EDS [2101] lines 1256-1264 | {"AccessType": "RW", "DataType": "0x0006", "DefaultValue": "0", "ObjFlags": "0x00000000", "ObjectType": "7", "PDOMapping": "1", "ParameterName": "Obj.DiStatus"} |
| 0x2102 | scalar | EDS [2102] lines 1265-1273 | {"AccessType": "RW", "DataType": "0x0006", "DefaultValue": "0", "ObjFlags": "0x00000000", "ObjectType": "7", "PDOMapping": "1", "ParameterName": "Obj.DoStatus"} |
| 0x2200 | scalar | EDS [2200] lines 1274-1282 | {"AccessType": "RW", "DataType": "0x0006", "DefaultValue": "0", "ObjFlags": "0x00000000", "ObjectType": "7", "PDOMapping": "1", "ParameterName": "Obj.DefautDir"} |
| 0x2201 | scalar | EDS [2201] lines 1283-1291 | {"AccessType": "RW", "DataType": "0x0006", "DefaultValue": "0", "ObjFlags": "0x00000000", "ObjectType": "7", "PDOMapping": "1", "ParameterName": "Obj.MicDivide"} |
| 0x2202 | scalar | EDS [2202] lines 1292-1300 | {"AccessType": "RW", "DataType": "0x0006", "DefaultValue": "10", "ObjFlags": "0x00000000", "ObjectType": "7", "PDOMapping": "1", "ParameterName": "Obj.Soft_Limit"} |
| 0x2203 | scalar | EDS [2203] lines 1301-1309 | {"AccessType": "RW", "DataType": "0x0006", "DefaultValue": "0", "ObjFlags": "0x00000000", "ObjectType": "7", "PDOMapping": "1", "ParameterName": "Obj.CAN_ID"} |
| 0x2300 | scalar | EDS [2300] lines 1310-1318 | {"AccessType": "RW", "DataType": "0x0006", "DefaultValue": "0", "ObjFlags": "0x00000000", "ObjectType": "7", "PDOMapping": "1", "ParameterName": "Obj.FactoryPara"} |
| 0x2301 | scalar | EDS [2301] lines 1319-1327 | {"AccessType": "RW", "DataType": "0x0006", "DefaultValue": "0", "ObjFlags": "0x00000000", "ObjectType": "7", "PDOMapping": "1", "ParameterName": "Obj.ClearAlm"} |
| 0x2302 | scalar | EDS [2302] lines 1328-1336 | {"AccessType": "RW", "DataType": "0x0006", "DefaultValue": "0", "ObjFlags": "0x00000000", "ObjectType": "7", "PDOMapping": "1", "ParameterName": "Obj.ClearPos"} |
| 0x2400 | record | EDS [2400] lines 1337-1342 | {"ObjFlags": "0x00000000", "ObjectType": "9", "ParameterName": "Obj.DiDo", "SubNumber": "0x11"} |
| 0x2401 | scalar | EDS [2401] lines 1496-1504 | {"AccessType": "RW", "DataType": "0x0006", "DefaultValue": "50", "ObjFlags": "0x00000000", "ObjectType": "7", "PDOMapping": "1", "ParameterName": "Obj.Di0Filtime"} |
| 0x2402 | scalar | EDS [2402] lines 1505-1513 | {"AccessType": "RW", "DataType": "0x0006", "DefaultValue": "40", "ObjFlags": "0x00000000", "ObjectType": "7", "PDOMapping": "1", "ParameterName": "Obj.Di1Filtime"} |
| 0x2403 | scalar | EDS [2403] lines 1514-1522 | {"AccessType": "RW", "DataType": "0x0006", "DefaultValue": "100", "ObjFlags": "0x00000000", "ObjectType": "7", "PDOMapping": "1", "ParameterName": "Obj.Di2Filtime"} |
| 0x2404 | scalar | EDS [2404] lines 1523-1531 | {"AccessType": "RW", "DataType": "0x0006", "DefaultValue": "4000", "ObjFlags": "0x00000000", "ObjectType": "7", "PDOMapping": "1", "ParameterName": "Obj.Di3Filtime"} |
| 0x2405 | scalar | EDS [2405] lines 1532-1540 | {"AccessType": "RW", "DataType": "0x0006", "DefaultValue": "10", "ObjFlags": "0x00000000", "ObjectType": "7", "PDOMapping": "1", "ParameterName": "Obj.Di4Filtime"} |
| 0x2406 | scalar | EDS [2406] lines 1541-1549 | {"AccessType": "RW", "DataType": "0x0006", "DefaultValue": "2", "ObjFlags": "0x00000000", "ObjectType": "7", "PDOMapping": "1", "ParameterName": "Obj.Di5Filtime"} |
| 0x2407 | scalar | EDS [2407] lines 1550-1558 | {"AccessType": "RW", "DataType": "0x0006", "DefaultValue": "16", "ObjFlags": "0x00000000", "ObjectType": "7", "PDOMapping": "1", "ParameterName": "Obj.Di6Filtime"} |
| 0x2500 | scalar | EDS [2500] lines 1559-1566 | {"AccessType": "RW", "DataType": "0x0006", "ObjFlags": "0x00000000", "ObjectType": "7", "PDOMapping": "1", "ParameterName": "Obj.DrvMode"} |
| 0x2501 | scalar | EDS [2501] lines 1567-1574 | {"AccessType": "RW", "DataType": "0x0006", "ObjFlags": "0x00000000", "ObjectType": "7", "PDOMapping": "1", "ParameterName": "Obj.Encoder"} |
| 0x2502 | scalar | EDS [2502] lines 1575-1582 | {"AccessType": "RW", "DataType": "0x0006", "ObjFlags": "0x00000000", "ObjectType": "7", "PDOMapping": "1", "ParameterName": "Obj.Maxcurrent"} |
| 0x2503 | scalar | EDS [2503] lines 1583-1590 | {"AccessType": "RW", "DataType": "0x0006", "ObjFlags": "0x00000000", "ObjectType": "7", "PDOMapping": "1", "ParameterName": "Obj.ClMaxCper"} |
| 0x2504 | scalar | EDS [2504] lines 1591-1598 | {"AccessType": "RW", "DataType": "0x0006", "ObjFlags": "0x00000000", "ObjectType": "7", "PDOMapping": "1", "ParameterName": "Obj.ClbasCper"} |
| 0x2505 | scalar | EDS [2505] lines 1599-1606 | {"AccessType": "RW", "DataType": "0x0006", "ObjFlags": "0x00000000", "ObjectType": "7", "PDOMapping": "1", "ParameterName": "Obj.OlMaxCper"} |
| 0x2506 | scalar | EDS [2506] lines 1607-1614 | {"AccessType": "RW", "DataType": "0x0006", "ObjFlags": "0x00000000", "ObjectType": "7", "PDOMapping": "1", "ParameterName": "Obj.suo_per"} |
| 0x2507 | scalar | EDS [2507] lines 1615-1622 | {"AccessType": "RW", "DataType": "0x0006", "ObjFlags": "0x00000000", "ObjectType": "7", "PDOMapping": "1", "ParameterName": "Obj.suo_time"} |
| 0x2508 | scalar | EDS [2508] lines 1623-1630 | {"AccessType": "RW", "DataType": "0x0006", "ObjFlags": "0x00000000", "ObjectType": "7", "PDOMapping": "1", "ParameterName": "Obj.Filtable"} |
| 0x2509 | scalar | EDS [2509] lines 1631-1638 | {"AccessType": "RW", "DataType": "0x0006", "ObjFlags": "0x00000000", "ObjectType": "7", "PDOMapping": "1", "ParameterName": "Obj.Filtratio"} |
| 0x250A | scalar | EDS [250A] lines 1639-1646 | {"AccessType": "RW", "DataType": "0x0006", "ObjFlags": "0x00000000", "ObjectType": "7", "PDOMapping": "1", "ParameterName": "Obj.PosAlrm"} |
| 0x250B | scalar | EDS [250B] lines 1647-1654 | {"AccessType": "RW", "DataType": "0x0006", "ObjFlags": "0x00000000", "ObjectType": "7", "PDOMapping": "1", "ParameterName": "Obj.PosError"} |
| 0x250C | scalar | EDS [250C] lines 1655-1662 | {"AccessType": "RW", "DataType": "0x0006", "ObjFlags": "0x00000000", "ObjectType": "7", "PDOMapping": "1", "ParameterName": "Obj.PosinTime"} |
| 0x250D | scalar | EDS [250D] lines 1663-1670 | {"AccessType": "RW", "DataType": "0x0006", "ObjFlags": "0x00000000", "ObjectType": "7", "PDOMapping": "1", "ParameterName": "Obj..AvrFiltime"} |
| 0x250E | scalar | EDS [250E] lines 1671-1678 | {"AccessType": "RW", "DataType": "0x0006", "ObjFlags": "0x00000000", "ObjectType": "7", "PDOMapping": "1", "ParameterName": "Obj.kpscale"} |
| 0x250F | scalar | EDS [250F] lines 1679-1686 | {"AccessType": "RW", "DataType": "0x0006", "ObjFlags": "0x00000000", "ObjectType": "7", "PDOMapping": "1", "ParameterName": "Obj.kp"} |
| 0x2510 | scalar | EDS [2510] lines 1687-1694 | {"AccessType": "RW", "DataType": "0x0006", "ObjFlags": "0x00000000", "ObjectType": "7", "PDOMapping": "1", "ParameterName": "Obj.ki"} |
| 0x2511 | scalar | EDS [2511] lines 1695-1702 | {"AccessType": "RW", "DataType": "0x0006", "ObjFlags": "0x00000000", "ObjectType": "7", "PDOMapping": "1", "ParameterName": "Obj.kc"} |
| 0x2512 | scalar | EDS [2512] lines 1703-1710 | {"AccessType": "RW", "DataType": "0x0006", "ObjFlags": "0x00000000", "ObjectType": "7", "PDOMapping": "1", "ParameterName": "Obj.an_kp1"} |
| 0x2513 | scalar | EDS [2513] lines 1711-1718 | {"AccessType": "RW", "DataType": "0x0006", "ObjFlags": "0x00000000", "ObjectType": "7", "PDOMapping": "1", "ParameterName": "Obj.an_kv1"} |
| 0x2514 | scalar | EDS [2514] lines 1719-1726 | {"AccessType": "RW", "DataType": "0x0006", "ObjFlags": "0x00000000", "ObjectType": "7", "PDOMapping": "1", "ParameterName": "Obj.an_spd1"} |
| 0x2515 | scalar | EDS [2515] lines 1727-1734 | {"AccessType": "RW", "DataType": "0x0006", "ObjFlags": "0x00000000", "ObjectType": "7", "PDOMapping": "1", "ParameterName": "Obj.an_kp2"} |
| 0x2516 | scalar | EDS [2516] lines 1735-1742 | {"AccessType": "RW", "DataType": "0x0006", "ObjFlags": "0x00000000", "ObjectType": "7", "PDOMapping": "1", "ParameterName": "Obj.an_kv2"} |
| 0x2517 | scalar | EDS [2517] lines 1743-1750 | {"AccessType": "RW", "DataType": "0x0006", "ObjFlags": "0x00000000", "ObjectType": "7", "PDOMapping": "1", "ParameterName": "Obj.an_spd2"} |
| 0x2518 | scalar | EDS [2518] lines 1751-1758 | {"AccessType": "RW", "DataType": "0x0006", "ObjFlags": "0x00000000", "ObjectType": "7", "PDOMapping": "1", "ParameterName": "Obj.an_kvff"} |
| 0x2519 | scalar | EDS [2519] lines 1759-1766 | {"AccessType": "RW", "DataType": "0x0006", "ObjFlags": "0x00000000", "ObjectType": "7", "PDOMapping": "1", "ParameterName": "Obj.an_ki"} |
| 0x251A | scalar | EDS [251A] lines 1767-1774 | {"AccessType": "RW", "DataType": "0x0006", "ObjFlags": "0x00000000", "ObjectType": "7", "PDOMapping": "1", "ParameterName": "Obj.v_kp"} |
| 0x251B | scalar | EDS [251B] lines 1775-1782 | {"AccessType": "RW", "DataType": "0x0006", "ObjFlags": "0x00000000", "ObjectType": "7", "PDOMapping": "1", "ParameterName": "Obj.v_ki"} |
| 0x251C | scalar | EDS [251C] lines 1783-1790 | {"AccessType": "RW", "DataType": "0x0006", "ObjFlags": "0x00000000", "ObjectType": "7", "PDOMapping": "1", "ParameterName": "Obj.v_kc"} |
| 0x251D | scalar | EDS [251D] lines 1791-1798 | {"AccessType": "RW", "DataType": "0x0006", "ObjFlags": "0x00000000", "ObjectType": "7", "PDOMapping": "1", "ParameterName": "Obj.v_kvfbd1"} |
| 0x251E | scalar | EDS [251E] lines 1799-1806 | {"AccessType": "RW", "DataType": "0x0006", "ObjFlags": "0x00000000", "ObjectType": "7", "PDOMapping": "1", "ParameterName": "Obj.v_kvfbd2"} |
| 0x251F | scalar | EDS [251F] lines 1807-1814 | {"AccessType": "RW", "DataType": "0x0006", "ObjFlags": "0x00000000", "ObjectType": "7", "PDOMapping": "1", "ParameterName": "Obj.v_kvff"} |
| 0x2520 | scalar | EDS [2520] lines 1815-1822 | {"AccessType": "RW", "DataType": "0x0006", "ObjFlags": "0x00000000", "ObjectType": "7", "PDOMapping": "1", "ParameterName": "Obj.v_kacff"} |
| 0x603F | scalar | EDS [603F] lines 1823-1831 | {"AccessType": "RO", "DataType": "0x0006", "DefaultValue": "0", "ObjFlags": "0x00000000", "ObjectType": "7", "PDOMapping": "1", "ParameterName": "DSP Error Code"} |
| 0x6040 | scalar | EDS [6040] lines 1832-1840 | {"AccessType": "WO", "DataType": "0x0006", "DefaultValue": "0", "ObjFlags": "0x00000000", "ObjectType": "7", "PDOMapping": "1", "ParameterName": "ControlWord"} |
| 0x6041 | scalar | EDS [6041] lines 1841-1849 | {"AccessType": "RO", "DataType": "0x0006", "DefaultValue": "0", "ObjFlags": "0x00000000", "ObjectType": "7", "PDOMapping": "1", "ParameterName": "StatusWord"} |
| 0x605D | scalar | EDS [605D] lines 1850-1858 | {"AccessType": "RW", "DataType": "0x0003", "DefaultValue": "1", "ObjFlags": "0x00000000", "ObjectType": "7", "PDOMapping": "0", "ParameterName": "Halt option code"} |
| 0x6060 | scalar | EDS [6060] lines 1859-1867 | {"AccessType": "WO", "DataType": "0x0005", "DefaultValue": "0", "ObjFlags": "0x00000000", "ObjectType": "7", "PDOMapping": "1", "ParameterName": "Mode of operation"} |
| 0x6061 | scalar | EDS [6061] lines 1868-1876 | {"AccessType": "RO", "DataType": "0x0005", "DefaultValue": "0", "ObjFlags": "0x00000000", "ObjectType": "7", "PDOMapping": "1", "ParameterName": "Mode of operation display"} |
| 0x6064 | scalar | EDS [6064] lines 1877-1885 | {"AccessType": "RO", "DataType": "0x0004", "DefaultValue": "0", "ObjFlags": "0x00000000", "ObjectType": "7", "PDOMapping": "1", "ParameterName": "actual position"} |
| 0x606C | scalar | EDS [606C] lines 1886-1893 | {"AccessType": "RO", "DataType": "0x0004", "ObjFlags": "0x00000000", "ObjectType": "7", "PDOMapping": "1", "ParameterName": "actual speed"} |
| 0x607A | scalar | EDS [607A] lines 1894-1902 | {"AccessType": "RW", "DataType": "0x0004", "DefaultValue": "5000", "ObjFlags": "0x00000000", "ObjectType": "7", "PDOMapping": "1", "ParameterName": "target position"} |
| 0x607C | scalar | EDS [607C] lines 1903-1911 | {"AccessType": "RW", "DataType": "0x0004", "DefaultValue": "0", "ObjFlags": "0x00000000", "ObjectType": "7", "PDOMapping": "1", "ParameterName": "home offset"} |
| 0x607D | record | EDS [607D] lines 1912-1917 | {"ObjFlags": "0x00000000", "ObjectType": "9", "ParameterName": "Soft Limit", "SubNumber": "0x03"} |
| 0x6081 | scalar | EDS [6081] lines 1943-1951 | {"AccessType": "RW", "DataType": "0x0007", "DefaultValue": "60", "ObjFlags": "0x00000000", "ObjectType": "7", "PDOMapping": "1", "ParameterName": "posmode speed"} |
| 0x6083 | scalar | EDS [6083] lines 1952-1960 | {"AccessType": "RW", "DataType": "0x0007", "DefaultValue": "100", "ObjFlags": "0x00000000", "ObjectType": "7", "PDOMapping": "1", "ParameterName": "accel time"} |
| 0x6084 | scalar | EDS [6084] lines 1961-1969 | {"AccessType": "RW", "DataType": "0x0007", "DefaultValue": "100", "ObjFlags": "0x00000000", "ObjectType": "7", "PDOMapping": "1", "ParameterName": "decel time"} |
| 0x6098 | scalar | EDS [6098] lines 1970-1978 | {"AccessType": "RW", "DataType": "0x0005", "DefaultValue": "0", "ObjFlags": "0x00000000", "ObjectType": "7", "PDOMapping": "1", "ParameterName": "homing method"} |
| 0x6099 | record | EDS [6099] lines 1979-1984 | {"ObjFlags": "0x00000000", "ObjectType": "9", "ParameterName": "homing speed/search speed", "SubNumber": "0x03"} |
| 0x609A | scalar | EDS [609A] lines 2012-2020 | {"AccessType": "RW", "DataType": "0x0007", "DefaultValue": "100", "ObjFlags": "0x00000000", "ObjectType": "7", "PDOMapping": "1", "ParameterName": "homing accel/decel time"} |
| 0x60FD | scalar | EDS [60FD] lines 2021-2028 | {"AccessType": "RO", "DataType": "0x0007", "ObjFlags": "0x00000000", "ObjectType": "7", "PDOMapping": "1", "ParameterName": "Digital inputs"} |
| 0x60FE | record | EDS [60FE] lines 2029-2034 | {"ObjFlags": "0x00000000", "ObjectType": "9", "ParameterName": "Digital Outputs", "SubNumber": "0x03"} |
| 0x60FF | scalar | EDS [60FF] lines 2060-2068 | {"AccessType": "RW", "DataType": "0x0004", "DefaultValue": "120", "ObjFlags": "0x00000000", "ObjectType": "7", "PDOMapping": "1", "ParameterName": "velocity speed"} |

## Scalar declaration coverage

The independent native type/access/units columns describe source claims; only resolved rows generate declarations. Source PDOMapping=1 is never a write permission. The default RPDO1 source words 0x60400010, 0x60ff0020, 0x60600008 do not grant SDO reads of WO 6040/6060 or any ungated action. `Pdo.h` generates ten explicitly reviewed passive layout facts and all eight source templates. They support raw-layout validation, not write permission, live configuration, sample provenance, physical units or motion completion. Mode bytes and target velocity retain raw representation while signedness conflicts remain unresolved; every live qualification gate stays open.

| Entry | Role | Native type / bytes / signed | EDS / reviewed SDO access | EDS / reviewed PDO | Declaration | Owner | D/I/H/Q |
| --- | --- | --- | --- | --- | --- | --- | --- |
| 0x1000:00 | value | U32 / 4 / false | RO / RO | 0 / false | resolved | 19 | yes/no/no/no |
| 0x1001:00 | value | U8 / 1 / false | RO / RO | 0 / false | resolved | 19 | yes/no/no/no |
| 0x1003:00 | count | U8 / 1 / false | RO / RO | 0 / false | resolved | 19 | yes/no/no/no |
| 0x1003:01 | value | U32 / 4 / false | RO / RO | 0 / false | resolved | 19 | yes/no/no/no |
| 0x1003:02 | value | U32 / 4 / false | RO / RO | 0 / false | resolved | 19 | yes/no/no/no |
| 0x1003:03 | value | U32 / 4 / false | RO / RO | 0 / false | resolved | 19 | yes/no/no/no |
| 0x1003:04 | value | U32 / 4 / false | RO / RO | 0 / false | resolved | 19 | yes/no/no/no |
| 0x1005:00 | value | U32 / 4 / false | RW / RW | 0 / false | resolved | 38 | yes/no/no/no |
| 0x1009:00 | value | U16 / 2 / false | RO / RO | 0 / false | resolved | 19 | yes/no/no/no |
| 0x100A:00 | value | U16 / 2 / false | RO / RO | 0 / false | resolved | 19 | yes/no/no/no |
| 0x1014:00 | value | U32 / 4 / false | RW / RW | 0 / false | resolved | 38 | yes/no/no/no |
| 0x1017:00 | value | U16 / 2 / false | RW / RW | 0 / false | resolved | 38 | yes/no/no/no |
| 0x1018:00 | count | U8 / 1 / false | RO / RO | 0 / false | resolved | 19 | yes/no/no/no |
| 0x1018:01 | value | U32 / 4 / false | RO / RO | 0 / false | resolved | 19 | yes/no/no/no |
| 0x1018:02 | value | U32 / 4 / false | RO / RO | 0 / false | resolved | 19 | yes/no/no/no |
| 0x1018:03 | value | U32 / 4 / false | RO / RO | 0 / false | resolved | 19 | yes/no/no/no |
| 0x1200:00 | count | U8 / 1 / false | RO / RO | 0 / false | resolved | 13 | yes/no/no/no |
| 0x1200:01 | value | U32 / 4 / false | RW / RW | 0 / false | resolved | 38 | yes/no/no/no |
| 0x1200:02 | value | U32 / 4 / false | RW / RW | 0 / false | resolved | 38 | yes/no/no/no |
| 0x1400:00 | count | U8 / 1 / false | RO / RO | 0 / false | resolved | 17 | yes/no/no/no |
| 0x1400:01 | value | U32 / 4 / false | RW / RW | 0 / false | resolved | 18 | yes/no/no/no |
| 0x1400:02 | value | U8 / 1 / false | RW / RW | 0 / false | resolved | 18 | yes/no/no/no |
| 0x1400:03 | value | U16 / 2 / false | RW / RW | 0 / false | resolved | 18 | yes/no/no/no |
| 0x1400:04 | value | U8 / 1 / false | RW / RO | 0 / false | resolved | 18 | yes/no/no/no |
| 0x1400:05 | value | U16 / 2 / false | RW / RW | 0 / false | resolved | 18 | yes/no/no/no |
| 0x1401:00 | count | U8 / 1 / false | RO / RO | 0 / false | resolved | 17 | yes/no/no/no |
| 0x1401:01 | value | U32 / 4 / false | RW / RW | 0 / false | resolved | 18 | yes/no/no/no |
| 0x1401:02 | value | U8 / 1 / false | RW / RW | 0 / false | resolved | 18 | yes/no/no/no |
| 0x1401:03 | value | U16 / 2 / false | RW / RW | 0 / false | resolved | 18 | yes/no/no/no |
| 0x1401:04 | value | U8 / 1 / false | RW / RO | 0 / false | resolved | 18 | yes/no/no/no |
| 0x1401:05 | value | U16 / 2 / false | RW / RW | 0 / false | resolved | 18 | yes/no/no/no |
| 0x1402:00 | count | U8 / 1 / false | RO / RO | 0 / false | resolved | 17 | yes/no/no/no |
| 0x1402:01 | value | U32 / 4 / false | RW / RW | 0 / false | resolved | 18 | yes/no/no/no |
| 0x1402:02 | value | U8 / 1 / false | RW / RW | 0 / false | resolved | 18 | yes/no/no/no |
| 0x1402:03 | value | U16 / 2 / false | RW / RW | 0 / false | resolved | 18 | yes/no/no/no |
| 0x1402:04 | value | U8 / 1 / false | RW / RO | 0 / false | resolved | 18 | yes/no/no/no |
| 0x1402:05 | value | U16 / 2 / false | RW / RW | 0 / false | resolved | 18 | yes/no/no/no |
| 0x1403:00 | count | U8 / 1 / false | RO / RO | 0 / false | resolved | 17 | yes/no/no/no |
| 0x1403:01 | value | U32 / 4 / false | RW / RW | 0 / false | resolved | 18 | yes/no/no/no |
| 0x1403:02 | value | U8 / 1 / false | RW / RW | 0 / false | resolved | 18 | yes/no/no/no |
| 0x1403:03 | value | U16 / 2 / false | RW / RW | 0 / false | resolved | 18 | yes/no/no/no |
| 0x1403:04 | value | U8 / 1 / false | RW / RO | 0 / false | resolved | 18 | yes/no/no/no |
| 0x1403:05 | value | U16 / 2 / false | RW / RW | 0 / false | resolved | 18 | yes/no/no/no |
| 0x1600:00 | count | U8 / 1 / false | RW / RW | 0 / false | resolved | 18 | yes/no/no/no |
| 0x1600:01 | value | U32 / 4 / false | RW / RW | 0 / false | resolved | 18 | yes/no/no/no |
| 0x1600:02 | value | U32 / 4 / false | RW / RW | 0 / false | resolved | 18 | yes/no/no/no |
| 0x1600:03 | value | U32 / 4 / false | RW / RW | 0 / false | resolved | 18 | yes/no/no/no |
| 0x1600:04 | value | U32 / 4 / false | RW / RW | 0 / false | resolved | 18 | yes/no/no/no |
| 0x1601:00 | count | U8 / 1 / false | RW / RW | 0 / false | resolved | 18 | yes/no/no/no |
| 0x1601:01 | value | U32 / 4 / false | RW / RW | 0 / false | resolved | 18 | yes/no/no/no |
| 0x1601:02 | value | U32 / 4 / false | RW / RW | 0 / false | resolved | 18 | yes/no/no/no |
| 0x1601:03 | value | U32 / 4 / false | RW / RW | 0 / false | resolved | 18 | yes/no/no/no |
| 0x1601:04 | value | U32 / 4 / false | RW / RW | 0 / false | resolved | 18 | yes/no/no/no |
| 0x1602:00 | count | U8 / 1 / false | RW / RW | 0 / false | resolved | 18 | yes/no/no/no |
| 0x1602:01 | value | U32 / 4 / false | RW / RW | 0 / false | resolved | 18 | yes/no/no/no |
| 0x1602:02 | value | U32 / 4 / false | RW / RW | 0 / false | resolved | 18 | yes/no/no/no |
| 0x1602:03 | value | U32 / 4 / false | RW / RW | 0 / false | resolved | 18 | yes/no/no/no |
| 0x1602:04 | value | U32 / 4 / false | RW / RW | 0 / false | resolved | 18 | yes/no/no/no |
| 0x1603:00 | count | U8 / 1 / false | RW / RW | 0 / false | resolved | 18 | yes/no/no/no |
| 0x1603:01 | value | U32 / 4 / false | RW / RW | 0 / false | resolved | 18 | yes/no/no/no |
| 0x1603:02 | value | U32 / 4 / false | RW / RW | 0 / false | resolved | 18 | yes/no/no/no |
| 0x1603:03 | value | U32 / 4 / false | RW / RW | 0 / false | resolved | 18 | yes/no/no/no |
| 0x1603:04 | value | U32 / 4 / false | RW / RW | 0 / false | resolved | 18 | yes/no/no/no |
| 0x1800:00 | count | U8 / 1 / false | RO / RO | 0 / false | resolved | 17 | yes/no/no/no |
| 0x1800:01 | value | U32 / 4 / false | RW / RW | 0 / false | resolved | 18 | yes/no/no/no |
| 0x1800:02 | value | U8 / 1 / false | RW / RW | 0 / false | resolved | 18 | yes/no/no/no |
| 0x1800:03 | value | U16 / 2 / false | RW / RW | 0 / false | resolved | 18 | yes/no/no/no |
| 0x1800:04 | value | U8 / 1 / false | RW / RO | 0 / false | resolved | 18 | yes/no/no/no |
| 0x1800:05 | value | U16 / 2 / false | RW / RW | 0 / false | resolved | 18 | yes/no/no/no |
| 0x1801:00 | count | U8 / 1 / false | RO / RO | 0 / false | resolved | 17 | yes/no/no/no |
| 0x1801:01 | value | U32 / 4 / false | RW / RW | 0 / false | resolved | 18 | yes/no/no/no |
| 0x1801:02 | value | U8 / 1 / false | RW / RW | 0 / false | resolved | 18 | yes/no/no/no |
| 0x1801:03 | value | U16 / 2 / false | RW / RW | 0 / false | resolved | 18 | yes/no/no/no |
| 0x1801:04 | value | U8 / 1 / false | RW / RO | 0 / false | resolved | 18 | yes/no/no/no |
| 0x1801:05 | value | U16 / 2 / false | RW / RW | 0 / false | resolved | 18 | yes/no/no/no |
| 0x1802:00 | count | U8 / 1 / false | RO / RO | 0 / false | resolved | 17 | yes/no/no/no |
| 0x1802:01 | value | U32 / 4 / false | RW / RW | 0 / false | resolved | 18 | yes/no/no/no |
| 0x1802:02 | value | U8 / 1 / false | RW / RW | 0 / false | resolved | 18 | yes/no/no/no |
| 0x1802:03 | value | U16 / 2 / false | RW / RW | 0 / false | resolved | 18 | yes/no/no/no |
| 0x1802:04 | value | U8 / 1 / false | RW / RO | 0 / false | resolved | 18 | yes/no/no/no |
| 0x1802:05 | value | U16 / 2 / false | RW / RW | 0 / false | resolved | 18 | yes/no/no/no |
| 0x1803:00 | count | U8 / 1 / false | RO / RO | 0 / false | resolved | 17 | yes/no/no/no |
| 0x1803:01 | value | U32 / 4 / false | RW / RW | 0 / false | resolved | 18 | yes/no/no/no |
| 0x1803:02 | value | U8 / 1 / false | RW / RW | 0 / false | resolved | 18 | yes/no/no/no |
| 0x1803:03 | value | U16 / 2 / false | RW / RW | 0 / false | resolved | 18 | yes/no/no/no |
| 0x1803:04 | value | U8 / 1 / false | RW / RO | 0 / false | resolved | 18 | yes/no/no/no |
| 0x1803:05 | value | U16 / 2 / false | RW / RW | 0 / false | resolved | 18 | yes/no/no/no |
| 0x1A00:00 | count | U8 / 1 / false | RW / RW | 0 / false | resolved | 18 | yes/no/no/no |
| 0x1A00:01 | value | U32 / 4 / false | RW / RW | 0 / false | resolved | 18 | yes/no/no/no |
| 0x1A00:02 | value | U32 / 4 / false | RW / RW | 0 / false | resolved | 18 | yes/no/no/no |
| 0x1A00:03 | value | U32 / 4 / false | RW / RW | 0 / false | resolved | 18 | yes/no/no/no |
| 0x1A00:04 | value | U32 / 4 / false | RW / RW | 0 / false | resolved | 18 | yes/no/no/no |
| 0x1A01:00 | count | U8 / 1 / false | RW / RW | 0 / false | resolved | 18 | yes/no/no/no |
| 0x1A01:01 | value | U32 / 4 / false | RW / RW | 0 / false | resolved | 18 | yes/no/no/no |
| 0x1A01:02 | value | U32 / 4 / false | RW / RW | 0 / false | resolved | 18 | yes/no/no/no |
| 0x1A01:03 | value | U32 / 4 / false | RW / RW | 0 / false | resolved | 18 | yes/no/no/no |
| 0x1A01:04 | value | U32 / 4 / false | RW / RW | 0 / false | resolved | 18 | yes/no/no/no |
| 0x1A02:00 | count | U8 / 1 / false | RW / RW | 0 / false | resolved | 18 | yes/no/no/no |
| 0x1A02:01 | value | U32 / 4 / false | RW / RW | 0 / false | resolved | 18 | yes/no/no/no |
| 0x1A02:02 | value | U32 / 4 / false | RW / RW | 0 / false | resolved | 18 | yes/no/no/no |
| 0x1A02:03 | value | U32 / 4 / false | RW / RW | 0 / false | resolved | 18 | yes/no/no/no |
| 0x1A02:04 | value | U32 / 4 / false | RW / RW | 0 / false | resolved | 18 | yes/no/no/no |
| 0x1A03:00 | count | U8 / 1 / false | RW / RW | 0 / false | resolved | 18 | yes/no/no/no |
| 0x1A03:01 | value | U32 / 4 / false | RW / RW | 0 / false | resolved | 18 | yes/no/no/no |
| 0x1A03:02 | value | U32 / 4 / false | RW / RW | 0 / false | resolved | 18 | yes/no/no/no |
| 0x1A03:03 | value | U32 / 4 / false | RW / RW | 0 / false | resolved | 18 | yes/no/no/no |
| 0x1A03:04 | value | U32 / 4 / false | RW / RW | 0 / false | resolved | 18 | yes/no/no/no |
| 0x2000:00 | value | U16 / 2 / false | RO / RO | 1 / `null` (not stated) | resolved | 19 | yes/no/no/no |
| 0x2001:00 | value | U16 / 2 / false | RO / RO | 1 / `null` (not stated) | resolved | 19 | yes/no/no/no |
| 0x2100:00 | value | U16 / 2 / false | RW / RO | 1 / `null` (not stated) | resolved | 19 | yes/no/no/no |
| 0x2101:00 | value | U16 / 2 / false | RW / RO | 1 / `null` (not stated) | resolved | 19 | yes/no/no/no |
| 0x2102:00 | value | U16 / 2 / false | RW / RO | 1 / `null` (not stated) | resolved | 19 | yes/no/no/no |
| 0x2200:00 | value | U16 / 2 / false | RW / RW | 1 / `null` (not stated) | resolved | 20 | yes/no/no/no |
| 0x2201:00 | value | U16 / 2 / false | RW / RW | 1 / `null` (not stated) | resolved | 20 | yes/no/no/no |
| 0x2202:00 | value | U16 / 2 / false | RW / RW | 1 / `null` (not stated) | resolved | 20 | yes/no/no/no |
| 0x2203:00 | value | U16 / 2 / false | RW / RW | 1 / `null` (not stated) | resolved | 38 | yes/no/no/no |
| 0x2300:00 | value | U16 / 2 / false | RW / RW | 1 / `null` (not stated) | resolved | 39 | yes/no/no/no |
| 0x2301:00 | value | U16 / 2 / false | RW / RW | 1 / `null` (not stated) | resolved | 32 | yes/no/no/no |
| 0x2302:00 | value | U16 / 2 / false | RW / RW | 1 / `null` (not stated) | resolved | 32 | yes/no/no/no |
| 0x2400:00 | count | U16 / 2 / false | RO / RO | 0 / false | resolved | 21 | yes/no/no/no |
| 0x2400:01 | value | U16 / 2 / false | RW / RW | 1 / `null` (not stated) | resolved | 21 | yes/no/no/no |
| 0x2400:02 | value | U16 / 2 / false | RW / RW | 1 / `null` (not stated) | resolved | 21 | yes/no/no/no |
| 0x2400:03 | value | U16 / 2 / false | RW / RW | 1 / `null` (not stated) | resolved | 21 | yes/no/no/no |
| 0x2400:04 | value | U16 / 2 / false | RW / RW | 1 / `null` (not stated) | resolved | 21 | yes/no/no/no |
| 0x2400:05 | value | U16 / 2 / false | RW / RW | 1 / `null` (not stated) | resolved | 21 | yes/no/no/no |
| 0x2400:06 | value | U16 / 2 / false | RW / RW | 1 / `null` (not stated) | resolved | 21 | yes/no/no/no |
| 0x2400:07 | value | U16 / 2 / false | RW / RW | 1 / `null` (not stated) | resolved | 21 | yes/no/no/no |
| 0x2400:08 | value | U16 / 2 / false | RW / RW | 1 / `null` (not stated) | resolved | 21 | yes/no/no/no |
| 0x2400:09 | reserved | U16 / 2 / false | RW / `null` (not stated) | 1 / false | excluded | 21 | yes/no/no/no |
| 0x2400:0A | reserved | U16 / 2 / false | RW / `null` (not stated) | 1 / false | excluded | 21 | yes/no/no/no |
| 0x2400:0B | reserved | U16 / 2 / false | RW / `null` (not stated) | 1 / false | excluded | 21 | yes/no/no/no |
| 0x2400:0C | value | U16 / 2 / false | RW / RO | 1 / `null` (not stated) | resolved | 21 | yes/no/no/no |
| 0x2400:0D | value | U16 / 2 / false | RW / RO | 1 / `null` (not stated) | resolved | 21 | yes/no/no/no |
| 0x2400:0E | value | U16 / 2 / false | RW / RO | 1 / `null` (not stated) | resolved | 21 | yes/no/no/no |
| 0x2400:0F | value | U16 / 2 / false | RW / RO | 1 / `null` (not stated) | resolved | 21 | yes/no/no/no |
| 0x2400:10 | reserved | U16 / 2 / false | RW / `null` (not stated) | 1 / false | excluded | 21 | yes/no/no/no |
| 0x2401:00 | value | U16 / 2 / false | RW / RO | 1 / `null` (not stated) | resolved | 21 | yes/no/no/no |
| 0x2402:00 | value | U16 / 2 / false | RW / RO | 1 / `null` (not stated) | resolved | 21 | yes/no/no/no |
| 0x2403:00 | value | U16 / 2 / false | RW / RO | 1 / `null` (not stated) | resolved | 21 | yes/no/no/no |
| 0x2404:00 | value | U16 / 2 / false | RW / RO | 1 / `null` (not stated) | resolved | 21 | yes/no/no/no |
| 0x2405:00 | value | U16 / 2 / false | RW / RO | 1 / `null` (not stated) | resolved | 21 | yes/no/no/no |
| 0x2406:00 | value | U16 / 2 / false | RW / RO | 1 / `null` (not stated) | resolved | 21 | yes/no/no/no |
| 0x2407:00 | value | U16 / 2 / false | RW / RO | 1 / `null` (not stated) | resolved | 21 | yes/no/no/no |
| 0x2500:00 | value | U16 / 2 / false | RW / RO | 1 / `null` (not stated) | resolved | 22 | yes/no/no/no |
| 0x2501:00 | value | U16 / 2 / false | RW / RO | 1 / `null` (not stated) | resolved | 22 | yes/no/no/no |
| 0x2502:00 | value | U16 / 2 / false | RW / RO | 1 / `null` (not stated) | resolved | 22 | yes/no/no/no |
| 0x2503:00 | value | U16 / 2 / false | RW / RO | 1 / `null` (not stated) | resolved | 22 | yes/no/no/no |
| 0x2504:00 | value | U16 / 2 / false | RW / RO | 1 / `null` (not stated) | resolved | 22 | yes/no/no/no |
| 0x2505:00 | value | U16 / 2 / false | RW / RO | 1 / `null` (not stated) | resolved | 22 | yes/no/no/no |
| 0x2506:00 | value | U16 / 2 / false | RW / RO | 1 / `null` (not stated) | resolved | 22 | yes/no/no/no |
| 0x2507:00 | value | U16 / 2 / false | RW / RO | 1 / `null` (not stated) | resolved | 22 | yes/no/no/no |
| 0x2508:00 | value | U16 / 2 / false | RW / RO | 1 / `null` (not stated) | resolved | 22 | yes/no/no/no |
| 0x2509:00 | value | U16 / 2 / false | RW / RO | 1 / `null` (not stated) | resolved | 22 | yes/no/no/no |
| 0x250A:00 | value | U16 / 2 / false | RW / RO | 1 / `null` (not stated) | resolved | 22 | yes/no/no/no |
| 0x250B:00 | value | U16 / 2 / false | RW / RO | 1 / `null` (not stated) | resolved | 22 | yes/no/no/no |
| 0x250C:00 | value | U16 / 2 / false | RW / RO | 1 / `null` (not stated) | resolved | 22 | yes/no/no/no |
| 0x250D:00 | value | U16 / 2 / false | RW / RO | 1 / `null` (not stated) | resolved | 22 | yes/no/no/no |
| 0x250E:00 | value | U16 / 2 / false | RW / RO | 1 / `null` (not stated) | resolved | 22 | yes/no/no/no |
| 0x250F:00 | value | U16 / 2 / false | RW / RO | 1 / `null` (not stated) | resolved | 22 | yes/no/no/no |
| 0x2510:00 | value | U16 / 2 / false | RW / RO | 1 / `null` (not stated) | resolved | 22 | yes/no/no/no |
| 0x2511:00 | value | U16 / 2 / false | RW / RO | 1 / `null` (not stated) | resolved | 22 | yes/no/no/no |
| 0x2512:00 | value | U16 / 2 / false | RW / RO | 1 / `null` (not stated) | resolved | 22 | yes/no/no/no |
| 0x2513:00 | value | U16 / 2 / false | RW / RO | 1 / `null` (not stated) | resolved | 22 | yes/no/no/no |
| 0x2514:00 | value | U16 / 2 / false | RW / RO | 1 / `null` (not stated) | resolved | 22 | yes/no/no/no |
| 0x2515:00 | value | U16 / 2 / false | RW / RO | 1 / `null` (not stated) | resolved | 22 | yes/no/no/no |
| 0x2516:00 | value | U16 / 2 / false | RW / RO | 1 / `null` (not stated) | resolved | 22 | yes/no/no/no |
| 0x2517:00 | value | U16 / 2 / false | RW / RO | 1 / `null` (not stated) | resolved | 22 | yes/no/no/no |
| 0x2518:00 | value | U16 / 2 / false | RW / RO | 1 / `null` (not stated) | resolved | 22 | yes/no/no/no |
| 0x2519:00 | value | U16 / 2 / false | RW / RO | 1 / `null` (not stated) | resolved | 22 | yes/no/no/no |
| 0x251A:00 | value | U16 / 2 / false | RW / RO | 1 / `null` (not stated) | resolved | 22 | yes/no/no/no |
| 0x251B:00 | value | U16 / 2 / false | RW / RO | 1 / `null` (not stated) | resolved | 22 | yes/no/no/no |
| 0x251C:00 | value | U16 / 2 / false | RW / RO | 1 / `null` (not stated) | resolved | 22 | yes/no/no/no |
| 0x251D:00 | value | U16 / 2 / false | RW / RO | 1 / `null` (not stated) | resolved | 22 | yes/no/no/no |
| 0x251E:00 | value | U16 / 2 / false | RW / RO | 1 / `null` (not stated) | resolved | 22 | yes/no/no/no |
| 0x251F:00 | value | U16 / 2 / false | RW / RO | 1 / `null` (not stated) | resolved | 22 | yes/no/no/no |
| 0x2520:00 | value | U16 / 2 / false | RW / RO | 1 / `null` (not stated) | resolved | 22 | yes/no/no/no |
| 0x603F:00 | value | U16 / 2 / false | RO / RO | 1 / `null` (not stated) | resolved | 19 | yes/no/no/no |
| 0x6040:00 | value | U16 / 2 / false | WO / WO | 1 / `null` (not stated) | resolved | 30 | yes/no/no/no |
| 0x6041:00 | value | U16 / 2 / false | RO / RO | 1 / `null` (not stated) | resolved | 19 | yes/no/no/no |
| 0x605D:00 | value | I16 / 2 / true | RW / RO | 0 / false | resolved | 31 | yes/no/no/no |
| 0x6060:00 | value | U8 / 1 / false | WO / WO | 1 / `null` (not stated) | withheld | 20 | yes/no/no/no |
| 0x6061:00 | value | U8 / 1 / false | RO / RO | 1 / `null` (not stated) | withheld | 19 | yes/no/no/no |
| 0x6064:00 | value | I32 / 4 / true | RO / RO | 1 / `null` (not stated) | resolved | 19 | yes/no/no/no |
| 0x606C:00 | value | I32 / 4 / true | RO / RO | 1 / `null` (not stated) | resolved | 19 | yes/no/no/no |
| 0x607A:00 | value | I32 / 4 / true | RW / RW | 1 / `null` (not stated) | resolved | 33 | yes/no/no/no |
| 0x607C:00 | value | I32 / 4 / true | RW / RW | 1 / `null` (not stated) | resolved | 32 | yes/no/no/no |
| 0x607D:00 | count | U8 / 1 / false | RO / RO | 0 / false | resolved | 20 | yes/no/no/no |
| 0x607D:01 | value | I32 / 4 / true | RW / RW | 1 / `null` (not stated) | resolved | 20 | yes/no/no/no |
| 0x607D:02 | value | I32 / 4 / true | RW / RW | 1 / `null` (not stated) | resolved | 20 | yes/no/no/no |
| 0x6081:00 | value | U32 / 4 / false | RW / RW | 1 / `null` (not stated) | resolved | 20 | yes/no/no/no |
| 0x6083:00 | value | U32 / 4 / false | RW / RW | 1 / `null` (not stated) | resolved | 20 | yes/no/no/no |
| 0x6084:00 | value | U32 / 4 / false | RW / RW | 1 / `null` (not stated) | resolved | 20 | yes/no/no/no |
| 0x6098:00 | value | U8 / 1 / false | RW / RW | 1 / `null` (not stated) | withheld | 36 | yes/no/no/no |
| 0x6099:00 | count | U8 / 1 / false | RO / RO | 0 / false | resolved | 36 | yes/no/no/no |
| 0x6099:01 | value | U32 / 4 / false | RW / RW | 1 / `null` (not stated) | resolved | 36 | yes/no/no/no |
| 0x6099:02 | value | U32 / 4 / false | RW / RW | 1 / `null` (not stated) | resolved | 36 | yes/no/no/no |
| 0x609A:00 | value | U32 / 4 / false | RW / RW | 1 / `null` (not stated) | resolved | 36 | yes/no/no/no |
| 0x60FD:00 | value | U32 / 4 / false | RO / RO | 1 / `null` (not stated) | resolved | 19 | yes/no/no/no |
| 0x60FE:00 | count | U8 / 1 / false | RO / RO | 0 / false | resolved | 21 | yes/no/no/no |
| 0x60FE:01 | value | U32 / 4 / false | RW / RO | 1 / `null` (not stated) | resolved | 21 | yes/no/no/no |
| 0x60FE:02 | value | U32 / 4 / false | RW / RO | 1 / `null` (not stated) | resolved | 21 | yes/no/no/no |
| 0x60FF:00 | value | I32 / 4 / true | RW / RW | 1 / `null` (not stated) | withheld | 35 | yes/no/no/no |

## Entry evidence and pending semantics

### 0x1000:00

| Fact | Reviewed/source value |
| --- | --- |
| EDS name | Device Type |
| Sources | EDS [1000] lines 158-166 |
| Applicability | eds_family_unbound |
| Declaration disposition | resolved |
| Documentary disposition | supported_evidence |
| Exclusion reason | `null` (not stated) |
| Units | raw identity code |
| Scaling | No exact-model/firmware auto-detection from generic values. |
| Original low / high / default | [null, null, "0x40192"] |
| Effect | Observe identity metadata; not configuration selection. |
| Prerequisites | ["Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.", "Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates."] |
| Edge behavior | Not specified by the retained source. |
| Documentary review | {"gate_ids": ["Q01", "Q20"], "generation_type": "resolved", "read_effect": "not_established", "status": "complete", "type_policy": "Retain exact EDS wire width and signedness; no conversion to another profile type.", "value_policy": "EDS values remain metadata. 1009/100A are U16, not strings; 1018 has only children 00..03.", "write_policy": "not_writable"} |
| Conflicts | [] |
| Owner prompt | 19 |
| Semantic coverage D/I/H/Q | yes/no/no/no |
| Raw source claim | {"AccessType": "RO", "DataType": "0x0007", "DefaultValue": "0x40192", "ObjFlags": "0x00000000", "ObjectType": "7", "PDOMapping": "0", "ParameterName": "Device Type"} |

### 0x1001:00

| Fact | Reviewed/source value |
| --- | --- |
| EDS name | Error Register |
| Sources | EDS [1001] lines 167-175 |
| Applicability | eds_family_unbound |
| Declaration disposition | resolved |
| Documentary disposition | pending_qualification |
| Exclusion reason | `null` (not stated) |
| Units | raw error field |
| Scaling | Error register/history layout and read consumption require evidence. |
| Original low / high / default | [null, null, "0"] |
| Effect | Observe errors/history; no clear command is declared. |
| Prerequisites | ["Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.", "Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates."] |
| Edge behavior | Not specified by the retained source. |
| Documentary review | {"gate_ids": ["Q01", "Q18", "Q20"], "generation_type": "resolved", "read_effect": "not_established", "status": "complete", "type_policy": "Retain exact EDS wire width and signedness; no conversion to another profile type.", "value_policy": "1003 count 0..4, default 0; four U32 entries exist. Preserve raw values, no invented history clear.", "write_policy": "not_writable"} |
| Conflicts | [] |
| Owner prompt | 19 |
| Semantic coverage D/I/H/Q | yes/no/no/no |
| Raw source claim | {"AccessType": "RO", "DataType": "0x0005", "DefaultValue": "0", "ObjFlags": "0x00000000", "ObjectType": "7", "PDOMapping": "0", "ParameterName": "Error Register"} |

### 0x1003:00

| Fact | Reviewed/source value |
| --- | --- |
| EDS name | HighestSubIndex |
| Sources | EDS [1003sub0] lines 182-192 |
| Applicability | eds_family_unbound |
| Declaration disposition | resolved |
| Documentary disposition | pending_qualification |
| Exclusion reason | `null` (not stated) |
| Units | entry count |
| Scaling | Integer record count; retain this entry's own EDS width. |
| Original low / high / default | ["0", "4", "0"] |
| Effect | Observe the declared entry count; not the physical semantics of its children. |
| Prerequisites | ["Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.", "Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates."] |
| Edge behavior | Not specified by the retained source. |
| Documentary review | {"gate_ids": ["Q01", "Q18", "Q20"], "generation_type": "resolved", "read_effect": "not_established", "status": "complete", "type_policy": "Retain exact EDS wire width and signedness; no conversion to another profile type.", "value_policy": "1003 count 0..4, default 0; four U32 entries exist. Preserve raw values, no invented history clear.", "write_policy": "not_writable"} |
| Conflicts | [] |
| Owner prompt | 19 |
| Semantic coverage D/I/H/Q | yes/no/no/no |
| Raw source claim | {"AccessType": "RO", "DataType": "0x0005", "DefaultValue": "0", "HighLimit": "4", "LowLimit": "0", "ObjFlags": "0x00000000", "ObjectType": "7", "PDOMapping": "0", "ParameterName": "HighestSubIndex"} |

### 0x1003:01

| Fact | Reviewed/source value |
| --- | --- |
| EDS name | Pre-defined Error Field_sub1 |
| Sources | EDS [1003sub1] lines 193-200 |
| Applicability | eds_family_unbound |
| Declaration disposition | resolved |
| Documentary disposition | pending_qualification |
| Exclusion reason | `null` (not stated) |
| Units | raw error field |
| Scaling | Error register/history layout and read consumption require evidence. |
| Original low / high / default | [null, null, null] |
| Effect | Observe errors/history; no clear command is declared. |
| Prerequisites | ["Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.", "Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates."] |
| Edge behavior | Not specified by the retained source. |
| Documentary review | {"gate_ids": ["Q01", "Q18", "Q20"], "generation_type": "resolved", "read_effect": "not_established", "status": "complete", "type_policy": "Retain exact EDS wire width and signedness; no conversion to another profile type.", "value_policy": "1003 count 0..4, default 0; four U32 entries exist. Preserve raw values, no invented history clear.", "write_policy": "not_writable"} |
| Conflicts | [] |
| Owner prompt | 19 |
| Semantic coverage D/I/H/Q | yes/no/no/no |
| Raw source claim | {"AccessType": "RO", "DataType": "0x0007", "ObjFlags": "0x00000000", "ObjectType": "7", "PDOMapping": "0", "ParameterName": "Pre-defined Error Field_sub1"} |

### 0x1003:02

| Fact | Reviewed/source value |
| --- | --- |
| EDS name | Pre-defined Error Field_sub2 |
| Sources | EDS [1003sub2] lines 201-208 |
| Applicability | eds_family_unbound |
| Declaration disposition | resolved |
| Documentary disposition | pending_qualification |
| Exclusion reason | `null` (not stated) |
| Units | raw error field |
| Scaling | Error register/history layout and read consumption require evidence. |
| Original low / high / default | [null, null, null] |
| Effect | Observe errors/history; no clear command is declared. |
| Prerequisites | ["Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.", "Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates."] |
| Edge behavior | Not specified by the retained source. |
| Documentary review | {"gate_ids": ["Q01", "Q18", "Q20"], "generation_type": "resolved", "read_effect": "not_established", "status": "complete", "type_policy": "Retain exact EDS wire width and signedness; no conversion to another profile type.", "value_policy": "1003 count 0..4, default 0; four U32 entries exist. Preserve raw values, no invented history clear.", "write_policy": "not_writable"} |
| Conflicts | [] |
| Owner prompt | 19 |
| Semantic coverage D/I/H/Q | yes/no/no/no |
| Raw source claim | {"AccessType": "RO", "DataType": "0x0007", "ObjFlags": "0x00000000", "ObjectType": "7", "PDOMapping": "0", "ParameterName": "Pre-defined Error Field_sub2"} |

### 0x1003:03

| Fact | Reviewed/source value |
| --- | --- |
| EDS name | Pre-defined Error Field_sub3 |
| Sources | EDS [1003sub3] lines 209-216 |
| Applicability | eds_family_unbound |
| Declaration disposition | resolved |
| Documentary disposition | pending_qualification |
| Exclusion reason | `null` (not stated) |
| Units | raw error field |
| Scaling | Error register/history layout and read consumption require evidence. |
| Original low / high / default | [null, null, null] |
| Effect | Observe errors/history; no clear command is declared. |
| Prerequisites | ["Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.", "Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates."] |
| Edge behavior | Not specified by the retained source. |
| Documentary review | {"gate_ids": ["Q01", "Q18", "Q20"], "generation_type": "resolved", "read_effect": "not_established", "status": "complete", "type_policy": "Retain exact EDS wire width and signedness; no conversion to another profile type.", "value_policy": "1003 count 0..4, default 0; four U32 entries exist. Preserve raw values, no invented history clear.", "write_policy": "not_writable"} |
| Conflicts | [] |
| Owner prompt | 19 |
| Semantic coverage D/I/H/Q | yes/no/no/no |
| Raw source claim | {"AccessType": "RO", "DataType": "0x0007", "ObjFlags": "0x00000000", "ObjectType": "7", "PDOMapping": "0", "ParameterName": "Pre-defined Error Field_sub3"} |

### 0x1003:04

| Fact | Reviewed/source value |
| --- | --- |
| EDS name | Pre-defined Error Field_sub4 |
| Sources | EDS [1003sub4] lines 217-224 |
| Applicability | eds_family_unbound |
| Declaration disposition | resolved |
| Documentary disposition | pending_qualification |
| Exclusion reason | `null` (not stated) |
| Units | raw error field |
| Scaling | Error register/history layout and read consumption require evidence. |
| Original low / high / default | [null, null, null] |
| Effect | Observe errors/history; no clear command is declared. |
| Prerequisites | ["Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.", "Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates."] |
| Edge behavior | Not specified by the retained source. |
| Documentary review | {"gate_ids": ["Q01", "Q18", "Q20"], "generation_type": "resolved", "read_effect": "not_established", "status": "complete", "type_policy": "Retain exact EDS wire width and signedness; no conversion to another profile type.", "value_policy": "1003 count 0..4, default 0; four U32 entries exist. Preserve raw values, no invented history clear.", "write_policy": "not_writable"} |
| Conflicts | [] |
| Owner prompt | 19 |
| Semantic coverage D/I/H/Q | yes/no/no/no |
| Raw source claim | {"AccessType": "RO", "DataType": "0x0007", "ObjFlags": "0x00000000", "ObjectType": "7", "PDOMapping": "0", "ParameterName": "Pre-defined Error Field_sub4"} |

### 0x1005:00

| Fact | Reviewed/source value |
| --- | --- |
| EDS name | SYNC COB ID |
| Sources | EDS [1005] lines 225-233 |
| Applicability | eds_family_unbound |
| Declaration disposition | resolved |
| Documentary disposition | pending_qualification |
| Exclusion reason | `null` (not stated) |
| Units | COB-ID field |
| Scaling | U32 route plus flags; accepted flags need profile evidence. |
| Original low / high / default | [null, null, "0x80"] |
| Effect | Changes service/event routing; may lose contact or collide with another route. |
| Prerequisites | ["Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.", "Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates."] |
| Edge behavior | Not specified by the retained source. |
| Documentary review | {"gate_ids": ["Q01", "Q16", "Q20"], "generation_type": "resolved", "read_effect": "not_established", "status": "complete", "type_policy": "Retain exact EDS wire width and signedness; no conversion to another profile type.", "value_policy": "Preserve EDS default/expression; use checked node 1..127 and verified route masks.", "write_policy": "qualification_required"} |
| Conflicts | [] |
| Owner prompt | 38 |
| Semantic coverage D/I/H/Q | yes/no/no/no |
| Raw source claim | {"AccessType": "RW", "DataType": "0x0007", "DefaultValue": "0x80", "ObjFlags": "0x00000000", "ObjectType": "7", "PDOMapping": "0", "ParameterName": "SYNC COB ID"} |

### 0x1009:00

| Fact | Reviewed/source value |
| --- | --- |
| EDS name | hardware version |
| Sources | EDS [1009] lines 234-242 |
| Applicability | eds_family_unbound |
| Declaration disposition | resolved |
| Documentary disposition | supported_evidence |
| Exclusion reason | `null` (not stated) |
| Units | raw identity code |
| Scaling | No exact-model/firmware auto-detection from generic values. |
| Original low / high / default | [null, null, "0x2608"] |
| Effect | Observe identity metadata; not configuration selection. |
| Prerequisites | ["Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.", "Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates."] |
| Edge behavior | Not specified by the retained source. |
| Documentary review | {"gate_ids": ["Q01", "Q20"], "generation_type": "resolved", "read_effect": "not_established", "status": "complete", "type_policy": "Retain exact EDS wire width and signedness; no conversion to another profile type.", "value_policy": "EDS values remain metadata. 1009/100A are U16, not strings; 1018 has only children 00..03.", "write_policy": "not_writable"} |
| Conflicts | [] |
| Owner prompt | 19 |
| Semantic coverage D/I/H/Q | yes/no/no/no |
| Raw source claim | {"AccessType": "RO", "DataType": "0x0006", "DefaultValue": "0x2608", "ObjFlags": "0x00000000", "ObjectType": "7", "PDOMapping": "0", "ParameterName": "hardware version"} |

### 0x100A:00

| Fact | Reviewed/source value |
| --- | --- |
| EDS name | software version |
| Sources | EDS [100A] lines 243-251 |
| Applicability | eds_family_unbound |
| Declaration disposition | resolved |
| Documentary disposition | supported_evidence |
| Exclusion reason | `null` (not stated) |
| Units | raw identity code |
| Scaling | No exact-model/firmware auto-detection from generic values. |
| Original low / high / default | [null, null, "1"] |
| Effect | Observe identity metadata; not configuration selection. |
| Prerequisites | ["Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.", "Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates."] |
| Edge behavior | Not specified by the retained source. |
| Documentary review | {"gate_ids": ["Q01", "Q20"], "generation_type": "resolved", "read_effect": "not_established", "status": "complete", "type_policy": "Retain exact EDS wire width and signedness; no conversion to another profile type.", "value_policy": "EDS values remain metadata. 1009/100A are U16, not strings; 1018 has only children 00..03.", "write_policy": "not_writable"} |
| Conflicts | [] |
| Owner prompt | 19 |
| Semantic coverage D/I/H/Q | yes/no/no/no |
| Raw source claim | {"AccessType": "RO", "DataType": "0x0006", "DefaultValue": "1", "ObjFlags": "0x00000000", "ObjectType": "7", "PDOMapping": "0", "ParameterName": "software version"} |

### 0x1014:00

| Fact | Reviewed/source value |
| --- | --- |
| EDS name | Emergency COB ID |
| Sources | EDS [1014] lines 252-260 |
| Applicability | eds_family_unbound |
| Declaration disposition | resolved |
| Documentary disposition | pending_qualification |
| Exclusion reason | `null` (not stated) |
| Units | COB-ID field |
| Scaling | U32 route plus flags; accepted flags need profile evidence. |
| Original low / high / default | [null, null, "$NODEID+0x80"] |
| Effect | Changes service/event routing; may lose contact or collide with another route. |
| Prerequisites | ["Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.", "Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates."] |
| Edge behavior | Not specified by the retained source. |
| Documentary review | {"gate_ids": ["Q01", "Q16", "Q20"], "generation_type": "resolved", "read_effect": "not_established", "status": "complete", "type_policy": "Retain exact EDS wire width and signedness; no conversion to another profile type.", "value_policy": "Preserve EDS default/expression; use checked node 1..127 and verified route masks.", "write_policy": "qualification_required"} |
| Conflicts | [] |
| Owner prompt | 38 |
| Semantic coverage D/I/H/Q | yes/no/no/no |
| Raw source claim | {"AccessType": "RW", "DataType": "0x0007", "DefaultValue": "$NODEID+0x80", "ObjFlags": "0x00000000", "ObjectType": "7", "PDOMapping": "0", "ParameterName": "Emergency COB ID"} |

### 0x1017:00

| Fact | Reviewed/source value |
| --- | --- |
| EDS name | Producer Heartbeat Time |
| Sources | EDS [1017] lines 261-269 |
| Applicability | eds_family_unbound |
| Declaration disposition | resolved |
| Documentary disposition | pending_qualification |
| Exclusion reason | `null` (not stated) |
| Units | raw producer-time field |
| Scaling | EDS does not specify time unit here; protocol unit is not a tested drive reaction. |
| Original low / high / default | [null, null, "0"] |
| Effect | Configure producer heartbeat only; no consumer/host-loss reaction established. |
| Prerequisites | ["Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.", "Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates."] |
| Edge behavior | Not specified by the retained source. |
| Documentary review | {"gate_ids": ["Q01", "Q18", "Q20"], "generation_type": "resolved", "read_effect": "not_established", "status": "complete", "type_policy": "Retain exact EDS wire width and signedness; no conversion to another profile type.", "value_policy": "Default 0; never infer an active heartbeat or watchdog.", "write_policy": "qualification_required"} |
| Conflicts | [] |
| Owner prompt | 38 |
| Semantic coverage D/I/H/Q | yes/no/no/no |
| Raw source claim | {"AccessType": "RW", "DataType": "0x0006", "DefaultValue": "0", "ObjFlags": "0x00000000", "ObjectType": "7", "PDOMapping": "0", "ParameterName": "Producer Heartbeat Time"} |

### 0x1018:00

| Fact | Reviewed/source value |
| --- | --- |
| EDS name | number of entries |
| Sources | EDS [1018sub0] lines 276-286 |
| Applicability | eds_family_unbound |
| Declaration disposition | resolved |
| Documentary disposition | supported_evidence |
| Exclusion reason | `null` (not stated) |
| Units | entry count |
| Scaling | Integer record count; retain this entry's own EDS width. |
| Original low / high / default | ["1", "4", "3"] |
| Effect | Observe the declared entry count; not the physical semantics of its children. |
| Prerequisites | ["Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.", "Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates."] |
| Edge behavior | Not specified by the retained source. |
| Documentary review | {"gate_ids": ["Q01", "Q20"], "generation_type": "resolved", "read_effect": "not_established", "status": "complete", "type_policy": "Retain exact EDS wire width and signedness; no conversion to another profile type.", "value_policy": "EDS values remain metadata. 1009/100A are U16, not strings; 1018 has only children 00..03.", "write_policy": "not_writable"} |
| Conflicts | [] |
| Owner prompt | 19 |
| Semantic coverage D/I/H/Q | yes/no/no/no |
| Raw source claim | {"AccessType": "RO", "DataType": "0x0005", "DefaultValue": "3", "HighLimit": "4", "LowLimit": "1", "ObjFlags": "0x00000000", "ObjectType": "7", "PDOMapping": "0", "ParameterName": "number of entries"} |

### 0x1018:01

| Fact | Reviewed/source value |
| --- | --- |
| EDS name | Vendor ID |
| Sources | EDS [1018sub1] lines 287-295 |
| Applicability | eds_family_unbound |
| Declaration disposition | resolved |
| Documentary disposition | supported_evidence |
| Exclusion reason | `null` (not stated) |
| Units | raw identity code |
| Scaling | No exact-model/firmware auto-detection from generic values. |
| Original low / high / default | [null, null, "0x00001000"] |
| Effect | Observe identity metadata; not configuration selection. |
| Prerequisites | ["Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.", "Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates."] |
| Edge behavior | Not specified by the retained source. |
| Documentary review | {"gate_ids": ["Q01", "Q20"], "generation_type": "resolved", "read_effect": "not_established", "status": "complete", "type_policy": "Retain exact EDS wire width and signedness; no conversion to another profile type.", "value_policy": "EDS values remain metadata. 1009/100A are U16, not strings; 1018 has only children 00..03.", "write_policy": "not_writable"} |
| Conflicts | [] |
| Owner prompt | 19 |
| Semantic coverage D/I/H/Q | yes/no/no/no |
| Raw source claim | {"AccessType": "RO", "DataType": "0x0007", "DefaultValue": "0x00001000", "ObjFlags": "0x00000000", "ObjectType": "7", "PDOMapping": "0", "ParameterName": "Vendor ID"} |

### 0x1018:02

| Fact | Reviewed/source value |
| --- | --- |
| EDS name | Product code |
| Sources | EDS [1018sub2] lines 296-304 |
| Applicability | eds_family_unbound |
| Declaration disposition | resolved |
| Documentary disposition | supported_evidence |
| Exclusion reason | `null` (not stated) |
| Units | raw identity code |
| Scaling | No exact-model/firmware auto-detection from generic values. |
| Original low / high / default | [null, null, "0x00000001"] |
| Effect | Observe identity metadata; not configuration selection. |
| Prerequisites | ["Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.", "Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates."] |
| Edge behavior | Not specified by the retained source. |
| Documentary review | {"gate_ids": ["Q01", "Q20"], "generation_type": "resolved", "read_effect": "not_established", "status": "complete", "type_policy": "Retain exact EDS wire width and signedness; no conversion to another profile type.", "value_policy": "EDS values remain metadata. 1009/100A are U16, not strings; 1018 has only children 00..03.", "write_policy": "not_writable"} |
| Conflicts | [] |
| Owner prompt | 19 |
| Semantic coverage D/I/H/Q | yes/no/no/no |
| Raw source claim | {"AccessType": "RO", "DataType": "0x0007", "DefaultValue": "0x00000001", "ObjFlags": "0x00000000", "ObjectType": "7", "PDOMapping": "0", "ParameterName": "Product code"} |

### 0x1018:03

| Fact | Reviewed/source value |
| --- | --- |
| EDS name | Revision number |
| Sources | EDS [1018sub3] lines 305-313 |
| Applicability | eds_family_unbound |
| Declaration disposition | resolved |
| Documentary disposition | supported_evidence |
| Exclusion reason | `null` (not stated) |
| Units | raw identity code |
| Scaling | No exact-model/firmware auto-detection from generic values. |
| Original low / high / default | [null, null, "0x00000000"] |
| Effect | Observe identity metadata; not configuration selection. |
| Prerequisites | ["Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.", "Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates."] |
| Edge behavior | Not specified by the retained source. |
| Documentary review | {"gate_ids": ["Q01", "Q20"], "generation_type": "resolved", "read_effect": "not_established", "status": "complete", "type_policy": "Retain exact EDS wire width and signedness; no conversion to another profile type.", "value_policy": "EDS values remain metadata. 1009/100A are U16, not strings; 1018 has only children 00..03.", "write_policy": "not_writable"} |
| Conflicts | [] |
| Owner prompt | 19 |
| Semantic coverage D/I/H/Q | yes/no/no/no |
| Raw source claim | {"AccessType": "RO", "DataType": "0x0007", "DefaultValue": "0x00000000", "ObjFlags": "0x00000000", "ObjectType": "7", "PDOMapping": "0", "ParameterName": "Revision number"} |

### 0x1200:00

| Fact | Reviewed/source value |
| --- | --- |
| EDS name | HighestSubIndex |
| Sources | EDS [1200sub0] lines 320-328 |
| Applicability | eds_family_unbound |
| Declaration disposition | resolved |
| Documentary disposition | supported_evidence |
| Exclusion reason | `null` (not stated) |
| Units | entry count |
| Scaling | Integer record count; retain this entry's own EDS width. |
| Original low / high / default | [null, null, "2"] |
| Effect | Observe the declared entry count; not the physical semantics of its children. |
| Prerequisites | ["Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.", "Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates."] |
| Edge behavior | Not specified by the retained source. |
| Documentary review | {"gate_ids": ["Q01", "Q20"], "generation_type": "resolved", "read_effect": "not_established", "status": "complete", "type_policy": "Retain exact EDS wire width and signedness; no conversion to another profile type.", "value_policy": "Two children; default 2.", "write_policy": "not_writable"} |
| Conflicts | [] |
| Owner prompt | 13 |
| Semantic coverage D/I/H/Q | yes/no/no/no |
| Raw source claim | {"AccessType": "RO", "DataType": "0x0005", "DefaultValue": "2", "ObjFlags": "0x00000000", "ObjectType": "7", "PDOMapping": "0", "ParameterName": "HighestSubIndex"} |

### 0x1200:01

| Fact | Reviewed/source value |
| --- | --- |
| EDS name | COB ID Client to Server |
| Sources | EDS [1200sub1] lines 329-337 |
| Applicability | eds_family_unbound |
| Declaration disposition | resolved |
| Documentary disposition | pending_qualification |
| Exclusion reason | `null` (not stated) |
| Units | COB-ID field |
| Scaling | U32 route plus flags; accepted flags need profile evidence. |
| Original low / high / default | [null, null, "$NODEID+0x600"] |
| Effect | Changes service/event routing; may lose contact or collide with another route. |
| Prerequisites | ["Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.", "Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates."] |
| Edge behavior | Not specified by the retained source. |
| Documentary review | {"gate_ids": ["Q01", "Q16", "Q20"], "generation_type": "resolved", "read_effect": "not_established", "status": "complete", "type_policy": "Retain exact EDS wire width and signedness; no conversion to another profile type.", "value_policy": "Preserve EDS default/expression; use checked node 1..127 and verified route masks.", "write_policy": "qualification_required"} |
| Conflicts | [] |
| Owner prompt | 38 |
| Semantic coverage D/I/H/Q | yes/no/no/no |
| Raw source claim | {"AccessType": "RW", "DataType": "0x0007", "DefaultValue": "$NODEID+0x600", "ObjFlags": "0x00000000", "ObjectType": "7", "PDOMapping": "0", "ParameterName": "COB ID Client to Server"} |

### 0x1200:02

| Fact | Reviewed/source value |
| --- | --- |
| EDS name | COB ID Server to Client |
| Sources | EDS [1200sub2] lines 338-346 |
| Applicability | eds_family_unbound |
| Declaration disposition | resolved |
| Documentary disposition | pending_qualification |
| Exclusion reason | `null` (not stated) |
| Units | COB-ID field |
| Scaling | U32 route plus flags; accepted flags need profile evidence. |
| Original low / high / default | [null, null, "$NODEID+0x580"] |
| Effect | Changes service/event routing; may lose contact or collide with another route. |
| Prerequisites | ["Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.", "Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates."] |
| Edge behavior | Not specified by the retained source. |
| Documentary review | {"gate_ids": ["Q01", "Q16", "Q20"], "generation_type": "resolved", "read_effect": "not_established", "status": "complete", "type_policy": "Retain exact EDS wire width and signedness; no conversion to another profile type.", "value_policy": "Preserve EDS default/expression; use checked node 1..127 and verified route masks.", "write_policy": "qualification_required"} |
| Conflicts | [] |
| Owner prompt | 38 |
| Semantic coverage D/I/H/Q | yes/no/no/no |
| Raw source claim | {"AccessType": "RW", "DataType": "0x0007", "DefaultValue": "$NODEID+0x580", "ObjFlags": "0x00000000", "ObjectType": "7", "PDOMapping": "0", "ParameterName": "COB ID Server to Client"} |

### 0x1400:00

| Fact | Reviewed/source value |
| --- | --- |
| EDS name | HighestSubIndex |
| Sources | EDS [1400sub0] lines 353-361 |
| Applicability | eds_family_unbound |
| Declaration disposition | resolved |
| Documentary disposition | pending_qualification |
| Exclusion reason | `null` (not stated) |
| Units | entry count |
| Scaling | Integer record count; retain this entry's own EDS width. |
| Original low / high / default | [null, null, "5"] |
| Effect | Observe the declared entry count; not the physical semantics of its children. |
| Prerequisites | ["Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.", "Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates."] |
| Edge behavior | Not specified by the retained source. |
| Documentary review | {"gate_ids": ["Q01", "Q17", "Q20"], "generation_type": "resolved", "read_effect": "not_established", "status": "complete", "type_policy": "Retain exact EDS wire width and signedness; no conversion to another profile type.", "value_policy": "Five declared children; count default 5.", "write_policy": "not_writable"} |
| Conflicts | [] |
| Owner prompt | 17 |
| Semantic coverage D/I/H/Q | yes/no/no/no |
| Raw source claim | {"AccessType": "RO", "DataType": "0x0005", "DefaultValue": "5", "ObjFlags": "0x00000000", "ObjectType": "7", "PDOMapping": "0", "ParameterName": "HighestSubIndex"} |

### 0x1400:01

| Fact | Reviewed/source value |
| --- | --- |
| EDS name | COB-ID used by RPDO |
| Sources | EDS [1400sub1] lines 362-370 |
| Applicability | eds_family_unbound |
| Declaration disposition | resolved |
| Documentary disposition | pending_qualification |
| Exclusion reason | `null` (not stated) |
| Units | COB-ID |
| Scaling | Raw protocol field; no invented device timing or flags. |
| Original low / high / default | [null, null, "$NODEID+0x200"] |
| Effect | Changes PDO layout/route/timing; partial reconfiguration and delayed samples can remain ambiguous. |
| Prerequisites | ["Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.", "Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates."] |
| Edge behavior | Not specified by the retained source. |
| Documentary review | {"gate_ids": ["Q01", "Q17", "Q20"], "generation_type": "resolved", "read_effect": "not_established", "status": "complete", "type_policy": "Retain exact EDS wire width and signedness; no conversion to another profile type.", "value_policy": "Keep default $NODEID expression. A raw U32 is not proof every routing/flag combination is supported.", "write_policy": "qualification_required"} |
| Conflicts | [] |
| Owner prompt | 18 |
| Semantic coverage D/I/H/Q | yes/no/no/no |
| Raw source claim | {"AccessType": "RW", "DataType": "0x0007", "DefaultValue": "$NODEID+0x200", "ObjFlags": "0x00000000", "ObjectType": "7", "PDOMapping": "0", "ParameterName": "COB-ID used by RPDO"} |

### 0x1400:02

| Fact | Reviewed/source value |
| --- | --- |
| EDS name | Transmission Type |
| Sources | EDS [1400sub2] lines 371-379 |
| Applicability | eds_family_unbound |
| Declaration disposition | resolved |
| Documentary disposition | pending_qualification |
| Exclusion reason | `null` (not stated) |
| Units | transmission type |
| Scaling | Raw protocol field; no invented device timing or flags. |
| Original low / high / default | [null, null, "255"] |
| Effect | Changes PDO layout/route/timing; partial reconfiguration and delayed samples can remain ambiguous. |
| Prerequisites | ["Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.", "Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates."] |
| Edge behavior | Not specified by the retained source. |
| Documentary review | {"gate_ids": ["Q01", "Q17", "Q20"], "generation_type": "resolved", "read_effect": "not_established", "status": "complete", "type_policy": "Retain exact EDS wire width and signedness; no conversion to another profile type.", "value_policy": "Default 255 only; accepted synchronous/asynchronous types need evidence.", "write_policy": "qualification_required"} |
| Conflicts | [] |
| Owner prompt | 18 |
| Semantic coverage D/I/H/Q | yes/no/no/no |
| Raw source claim | {"AccessType": "RW", "DataType": "0x0005", "DefaultValue": "255", "ObjFlags": "0x00000000", "ObjectType": "7", "PDOMapping": "0", "ParameterName": "Transmission Type"} |

### 0x1400:03

| Fact | Reviewed/source value |
| --- | --- |
| EDS name | Inhibit Time |
| Sources | EDS [1400sub3] lines 380-388 |
| Applicability | eds_family_unbound |
| Declaration disposition | resolved |
| Documentary disposition | pending_qualification |
| Exclusion reason | `null` (not stated) |
| Units | inhibit time |
| Scaling | Raw protocol field; no invented device timing or flags. |
| Original low / high / default | [null, null, "0"] |
| Effect | Changes PDO layout/route/timing; partial reconfiguration and delayed samples can remain ambiguous. |
| Prerequisites | ["Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.", "Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates."] |
| Edge behavior | Not specified by the retained source. |
| Documentary review | {"gate_ids": ["Q01", "Q17", "Q20"], "generation_type": "resolved", "read_effect": "not_established", "status": "complete", "type_policy": "Retain exact EDS wire width and signedness; no conversion to another profile type.", "value_policy": "Default 0; units/effect not specified in this EDS.", "write_policy": "qualification_required"} |
| Conflicts | [] |
| Owner prompt | 18 |
| Semantic coverage D/I/H/Q | yes/no/no/no |
| Raw source claim | {"AccessType": "RW", "DataType": "0x0006", "DefaultValue": "0", "ObjFlags": "0x00000000", "ObjectType": "7", "PDOMapping": "0", "ParameterName": "Inhibit Time"} |

### 0x1400:04

| Fact | Reviewed/source value |
| --- | --- |
| EDS name | Compatibility Entry |
| Sources | EDS [1400sub4] lines 389-397 |
| Applicability | eds_family_unbound |
| Declaration disposition | resolved |
| Documentary disposition | pending_qualification |
| Exclusion reason | `null` (not stated) |
| Units | compatibility entry |
| Scaling | Raw protocol field; no invented device timing or flags. |
| Original low / high / default | [null, null, "0"] |
| Effect | Changes PDO layout/route/timing; partial reconfiguration and delayed samples can remain ambiguous. |
| Prerequisites | ["Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.", "Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates."] |
| Edge behavior | Not specified by the retained source. |
| Documentary review | {"gate_ids": ["Q01", "Q17", "Q20"], "generation_type": "resolved", "read_effect": "not_established", "status": "complete", "type_policy": "Retain exact EDS wire width and signedness; no conversion to another profile type.", "value_policy": "Default 0; no operational purpose established; no generic setter.", "write_policy": "not_writable"} |
| Conflicts | [] |
| Owner prompt | 18 |
| Semantic coverage D/I/H/Q | yes/no/no/no |
| Raw source claim | {"AccessType": "RW", "DataType": "0x0005", "DefaultValue": "0", "ObjFlags": "0x00000000", "ObjectType": "7", "PDOMapping": "0", "ParameterName": "Compatibility Entry"} |

### 0x1400:05

| Fact | Reviewed/source value |
| --- | --- |
| EDS name | Event Timer |
| Sources | EDS [1400sub5] lines 398-406 |
| Applicability | eds_family_unbound |
| Declaration disposition | resolved |
| Documentary disposition | pending_qualification |
| Exclusion reason | `null` (not stated) |
| Units | event timer |
| Scaling | Raw protocol field; no invented device timing or flags. |
| Original low / high / default | [null, null, "1000"] |
| Effect | Changes PDO layout/route/timing; partial reconfiguration and delayed samples can remain ambiguous. |
| Prerequisites | ["Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.", "Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates."] |
| Edge behavior | Not specified by the retained source. |
| Documentary review | {"gate_ids": ["Q01", "Q17", "Q20"], "generation_type": "resolved", "read_effect": "not_established", "status": "complete", "type_policy": "Retain exact EDS wire width and signedness; no conversion to another profile type.", "value_policy": "RPDO default 1000 / TPDO default 100; no drive host-loss reaction follows.", "write_policy": "qualification_required"} |
| Conflicts | [] |
| Owner prompt | 18 |
| Semantic coverage D/I/H/Q | yes/no/no/no |
| Raw source claim | {"AccessType": "RW", "DataType": "0x0006", "DefaultValue": "1000", "ObjFlags": "0x00000000", "ObjectType": "7", "PDOMapping": "0", "ParameterName": "Event Timer"} |

### 0x1401:00

| Fact | Reviewed/source value |
| --- | --- |
| EDS name | HighestSubIndex |
| Sources | EDS [1401sub0] lines 413-421 |
| Applicability | eds_family_unbound |
| Declaration disposition | resolved |
| Documentary disposition | pending_qualification |
| Exclusion reason | `null` (not stated) |
| Units | entry count |
| Scaling | Integer record count; retain this entry's own EDS width. |
| Original low / high / default | [null, null, "5"] |
| Effect | Observe the declared entry count; not the physical semantics of its children. |
| Prerequisites | ["Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.", "Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates."] |
| Edge behavior | Not specified by the retained source. |
| Documentary review | {"gate_ids": ["Q01", "Q17", "Q20"], "generation_type": "resolved", "read_effect": "not_established", "status": "complete", "type_policy": "Retain exact EDS wire width and signedness; no conversion to another profile type.", "value_policy": "Five declared children; count default 5.", "write_policy": "not_writable"} |
| Conflicts | [] |
| Owner prompt | 17 |
| Semantic coverage D/I/H/Q | yes/no/no/no |
| Raw source claim | {"AccessType": "RO", "DataType": "0x0005", "DefaultValue": "5", "ObjFlags": "0x00000000", "ObjectType": "7", "PDOMapping": "0", "ParameterName": "HighestSubIndex"} |

### 0x1401:01

| Fact | Reviewed/source value |
| --- | --- |
| EDS name | COB-ID used by RPDO |
| Sources | EDS [1401sub1] lines 422-430 |
| Applicability | eds_family_unbound |
| Declaration disposition | resolved |
| Documentary disposition | pending_qualification |
| Exclusion reason | `null` (not stated) |
| Units | COB-ID |
| Scaling | Raw protocol field; no invented device timing or flags. |
| Original low / high / default | [null, null, "$NODEID+0x300"] |
| Effect | Changes PDO layout/route/timing; partial reconfiguration and delayed samples can remain ambiguous. |
| Prerequisites | ["Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.", "Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates."] |
| Edge behavior | Not specified by the retained source. |
| Documentary review | {"gate_ids": ["Q01", "Q17", "Q20"], "generation_type": "resolved", "read_effect": "not_established", "status": "complete", "type_policy": "Retain exact EDS wire width and signedness; no conversion to another profile type.", "value_policy": "Keep default $NODEID expression. A raw U32 is not proof every routing/flag combination is supported.", "write_policy": "qualification_required"} |
| Conflicts | [] |
| Owner prompt | 18 |
| Semantic coverage D/I/H/Q | yes/no/no/no |
| Raw source claim | {"AccessType": "RW", "DataType": "0x0007", "DefaultValue": "$NODEID+0x300", "ObjFlags": "0x00000000", "ObjectType": "7", "PDOMapping": "0", "ParameterName": "COB-ID used by RPDO"} |

### 0x1401:02

| Fact | Reviewed/source value |
| --- | --- |
| EDS name | Transmission Type |
| Sources | EDS [1401sub2] lines 431-439 |
| Applicability | eds_family_unbound |
| Declaration disposition | resolved |
| Documentary disposition | pending_qualification |
| Exclusion reason | `null` (not stated) |
| Units | transmission type |
| Scaling | Raw protocol field; no invented device timing or flags. |
| Original low / high / default | [null, null, "255"] |
| Effect | Changes PDO layout/route/timing; partial reconfiguration and delayed samples can remain ambiguous. |
| Prerequisites | ["Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.", "Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates."] |
| Edge behavior | Not specified by the retained source. |
| Documentary review | {"gate_ids": ["Q01", "Q17", "Q20"], "generation_type": "resolved", "read_effect": "not_established", "status": "complete", "type_policy": "Retain exact EDS wire width and signedness; no conversion to another profile type.", "value_policy": "Default 255 only; accepted synchronous/asynchronous types need evidence.", "write_policy": "qualification_required"} |
| Conflicts | [] |
| Owner prompt | 18 |
| Semantic coverage D/I/H/Q | yes/no/no/no |
| Raw source claim | {"AccessType": "RW", "DataType": "0x0005", "DefaultValue": "255", "ObjFlags": "0x00000000", "ObjectType": "7", "PDOMapping": "0", "ParameterName": "Transmission Type"} |

### 0x1401:03

| Fact | Reviewed/source value |
| --- | --- |
| EDS name | Inhibit Time |
| Sources | EDS [1401sub3] lines 440-448 |
| Applicability | eds_family_unbound |
| Declaration disposition | resolved |
| Documentary disposition | pending_qualification |
| Exclusion reason | `null` (not stated) |
| Units | inhibit time |
| Scaling | Raw protocol field; no invented device timing or flags. |
| Original low / high / default | [null, null, "0"] |
| Effect | Changes PDO layout/route/timing; partial reconfiguration and delayed samples can remain ambiguous. |
| Prerequisites | ["Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.", "Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates."] |
| Edge behavior | Not specified by the retained source. |
| Documentary review | {"gate_ids": ["Q01", "Q17", "Q20"], "generation_type": "resolved", "read_effect": "not_established", "status": "complete", "type_policy": "Retain exact EDS wire width and signedness; no conversion to another profile type.", "value_policy": "Default 0; units/effect not specified in this EDS.", "write_policy": "qualification_required"} |
| Conflicts | [] |
| Owner prompt | 18 |
| Semantic coverage D/I/H/Q | yes/no/no/no |
| Raw source claim | {"AccessType": "RW", "DataType": "0x0006", "DefaultValue": "0", "ObjFlags": "0x00000000", "ObjectType": "7", "PDOMapping": "0", "ParameterName": "Inhibit Time"} |

### 0x1401:04

| Fact | Reviewed/source value |
| --- | --- |
| EDS name | Compatibility Entry |
| Sources | EDS [1401sub4] lines 449-457 |
| Applicability | eds_family_unbound |
| Declaration disposition | resolved |
| Documentary disposition | pending_qualification |
| Exclusion reason | `null` (not stated) |
| Units | compatibility entry |
| Scaling | Raw protocol field; no invented device timing or flags. |
| Original low / high / default | [null, null, "0"] |
| Effect | Changes PDO layout/route/timing; partial reconfiguration and delayed samples can remain ambiguous. |
| Prerequisites | ["Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.", "Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates."] |
| Edge behavior | Not specified by the retained source. |
| Documentary review | {"gate_ids": ["Q01", "Q17", "Q20"], "generation_type": "resolved", "read_effect": "not_established", "status": "complete", "type_policy": "Retain exact EDS wire width and signedness; no conversion to another profile type.", "value_policy": "Default 0; no operational purpose established; no generic setter.", "write_policy": "not_writable"} |
| Conflicts | [] |
| Owner prompt | 18 |
| Semantic coverage D/I/H/Q | yes/no/no/no |
| Raw source claim | {"AccessType": "RW", "DataType": "0x0005", "DefaultValue": "0", "ObjFlags": "0x00000000", "ObjectType": "7", "PDOMapping": "0", "ParameterName": "Compatibility Entry"} |

### 0x1401:05

| Fact | Reviewed/source value |
| --- | --- |
| EDS name | Event Timer |
| Sources | EDS [1401sub5] lines 458-466 |
| Applicability | eds_family_unbound |
| Declaration disposition | resolved |
| Documentary disposition | pending_qualification |
| Exclusion reason | `null` (not stated) |
| Units | event timer |
| Scaling | Raw protocol field; no invented device timing or flags. |
| Original low / high / default | [null, null, "1000"] |
| Effect | Changes PDO layout/route/timing; partial reconfiguration and delayed samples can remain ambiguous. |
| Prerequisites | ["Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.", "Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates."] |
| Edge behavior | Not specified by the retained source. |
| Documentary review | {"gate_ids": ["Q01", "Q17", "Q20"], "generation_type": "resolved", "read_effect": "not_established", "status": "complete", "type_policy": "Retain exact EDS wire width and signedness; no conversion to another profile type.", "value_policy": "RPDO default 1000 / TPDO default 100; no drive host-loss reaction follows.", "write_policy": "qualification_required"} |
| Conflicts | [] |
| Owner prompt | 18 |
| Semantic coverage D/I/H/Q | yes/no/no/no |
| Raw source claim | {"AccessType": "RW", "DataType": "0x0006", "DefaultValue": "1000", "ObjFlags": "0x00000000", "ObjectType": "7", "PDOMapping": "0", "ParameterName": "Event Timer"} |

### 0x1402:00

| Fact | Reviewed/source value |
| --- | --- |
| EDS name | HighestSubIndex |
| Sources | EDS [1402sub0] lines 473-481 |
| Applicability | eds_family_unbound |
| Declaration disposition | resolved |
| Documentary disposition | pending_qualification |
| Exclusion reason | `null` (not stated) |
| Units | entry count |
| Scaling | Integer record count; retain this entry's own EDS width. |
| Original low / high / default | [null, null, "5"] |
| Effect | Observe the declared entry count; not the physical semantics of its children. |
| Prerequisites | ["Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.", "Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates."] |
| Edge behavior | Not specified by the retained source. |
| Documentary review | {"gate_ids": ["Q01", "Q17", "Q20"], "generation_type": "resolved", "read_effect": "not_established", "status": "complete", "type_policy": "Retain exact EDS wire width and signedness; no conversion to another profile type.", "value_policy": "Five declared children; count default 5.", "write_policy": "not_writable"} |
| Conflicts | [] |
| Owner prompt | 17 |
| Semantic coverage D/I/H/Q | yes/no/no/no |
| Raw source claim | {"AccessType": "RO", "DataType": "0x0005", "DefaultValue": "5", "ObjFlags": "0x00000000", "ObjectType": "7", "PDOMapping": "0", "ParameterName": "HighestSubIndex"} |

### 0x1402:01

| Fact | Reviewed/source value |
| --- | --- |
| EDS name | COB-ID used by RPDO |
| Sources | EDS [1402sub1] lines 482-490 |
| Applicability | eds_family_unbound |
| Declaration disposition | resolved |
| Documentary disposition | pending_qualification |
| Exclusion reason | `null` (not stated) |
| Units | COB-ID |
| Scaling | Raw protocol field; no invented device timing or flags. |
| Original low / high / default | [null, null, "$NODEID+0x400"] |
| Effect | Changes PDO layout/route/timing; partial reconfiguration and delayed samples can remain ambiguous. |
| Prerequisites | ["Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.", "Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates."] |
| Edge behavior | Not specified by the retained source. |
| Documentary review | {"gate_ids": ["Q01", "Q17", "Q20"], "generation_type": "resolved", "read_effect": "not_established", "status": "complete", "type_policy": "Retain exact EDS wire width and signedness; no conversion to another profile type.", "value_policy": "Keep default $NODEID expression. A raw U32 is not proof every routing/flag combination is supported.", "write_policy": "qualification_required"} |
| Conflicts | [] |
| Owner prompt | 18 |
| Semantic coverage D/I/H/Q | yes/no/no/no |
| Raw source claim | {"AccessType": "RW", "DataType": "0x0007", "DefaultValue": "$NODEID+0x400", "ObjFlags": "0x00000000", "ObjectType": "7", "PDOMapping": "0", "ParameterName": "COB-ID used by RPDO"} |

### 0x1402:02

| Fact | Reviewed/source value |
| --- | --- |
| EDS name | Transmission Type |
| Sources | EDS [1402sub2] lines 491-499 |
| Applicability | eds_family_unbound |
| Declaration disposition | resolved |
| Documentary disposition | pending_qualification |
| Exclusion reason | `null` (not stated) |
| Units | transmission type |
| Scaling | Raw protocol field; no invented device timing or flags. |
| Original low / high / default | [null, null, "255"] |
| Effect | Changes PDO layout/route/timing; partial reconfiguration and delayed samples can remain ambiguous. |
| Prerequisites | ["Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.", "Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates."] |
| Edge behavior | Not specified by the retained source. |
| Documentary review | {"gate_ids": ["Q01", "Q17", "Q20"], "generation_type": "resolved", "read_effect": "not_established", "status": "complete", "type_policy": "Retain exact EDS wire width and signedness; no conversion to another profile type.", "value_policy": "Default 255 only; accepted synchronous/asynchronous types need evidence.", "write_policy": "qualification_required"} |
| Conflicts | [] |
| Owner prompt | 18 |
| Semantic coverage D/I/H/Q | yes/no/no/no |
| Raw source claim | {"AccessType": "RW", "DataType": "0x0005", "DefaultValue": "255", "ObjFlags": "0x00000000", "ObjectType": "7", "PDOMapping": "0", "ParameterName": "Transmission Type"} |

### 0x1402:03

| Fact | Reviewed/source value |
| --- | --- |
| EDS name | Inhibit Time |
| Sources | EDS [1402sub3] lines 500-508 |
| Applicability | eds_family_unbound |
| Declaration disposition | resolved |
| Documentary disposition | pending_qualification |
| Exclusion reason | `null` (not stated) |
| Units | inhibit time |
| Scaling | Raw protocol field; no invented device timing or flags. |
| Original low / high / default | [null, null, "0"] |
| Effect | Changes PDO layout/route/timing; partial reconfiguration and delayed samples can remain ambiguous. |
| Prerequisites | ["Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.", "Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates."] |
| Edge behavior | Not specified by the retained source. |
| Documentary review | {"gate_ids": ["Q01", "Q17", "Q20"], "generation_type": "resolved", "read_effect": "not_established", "status": "complete", "type_policy": "Retain exact EDS wire width and signedness; no conversion to another profile type.", "value_policy": "Default 0; units/effect not specified in this EDS.", "write_policy": "qualification_required"} |
| Conflicts | [] |
| Owner prompt | 18 |
| Semantic coverage D/I/H/Q | yes/no/no/no |
| Raw source claim | {"AccessType": "RW", "DataType": "0x0006", "DefaultValue": "0", "ObjFlags": "0x00000000", "ObjectType": "7", "PDOMapping": "0", "ParameterName": "Inhibit Time"} |

### 0x1402:04

| Fact | Reviewed/source value |
| --- | --- |
| EDS name | Compatibility Entry |
| Sources | EDS [1402sub4] lines 509-517 |
| Applicability | eds_family_unbound |
| Declaration disposition | resolved |
| Documentary disposition | pending_qualification |
| Exclusion reason | `null` (not stated) |
| Units | compatibility entry |
| Scaling | Raw protocol field; no invented device timing or flags. |
| Original low / high / default | [null, null, "0"] |
| Effect | Changes PDO layout/route/timing; partial reconfiguration and delayed samples can remain ambiguous. |
| Prerequisites | ["Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.", "Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates."] |
| Edge behavior | Not specified by the retained source. |
| Documentary review | {"gate_ids": ["Q01", "Q17", "Q20"], "generation_type": "resolved", "read_effect": "not_established", "status": "complete", "type_policy": "Retain exact EDS wire width and signedness; no conversion to another profile type.", "value_policy": "Default 0; no operational purpose established; no generic setter.", "write_policy": "not_writable"} |
| Conflicts | [] |
| Owner prompt | 18 |
| Semantic coverage D/I/H/Q | yes/no/no/no |
| Raw source claim | {"AccessType": "RW", "DataType": "0x0005", "DefaultValue": "0", "ObjFlags": "0x00000000", "ObjectType": "7", "PDOMapping": "0", "ParameterName": "Compatibility Entry"} |

### 0x1402:05

| Fact | Reviewed/source value |
| --- | --- |
| EDS name | Event Timer |
| Sources | EDS [1402sub5] lines 518-526 |
| Applicability | eds_family_unbound |
| Declaration disposition | resolved |
| Documentary disposition | pending_qualification |
| Exclusion reason | `null` (not stated) |
| Units | event timer |
| Scaling | Raw protocol field; no invented device timing or flags. |
| Original low / high / default | [null, null, "1000"] |
| Effect | Changes PDO layout/route/timing; partial reconfiguration and delayed samples can remain ambiguous. |
| Prerequisites | ["Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.", "Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates."] |
| Edge behavior | Not specified by the retained source. |
| Documentary review | {"gate_ids": ["Q01", "Q17", "Q20"], "generation_type": "resolved", "read_effect": "not_established", "status": "complete", "type_policy": "Retain exact EDS wire width and signedness; no conversion to another profile type.", "value_policy": "RPDO default 1000 / TPDO default 100; no drive host-loss reaction follows.", "write_policy": "qualification_required"} |
| Conflicts | [] |
| Owner prompt | 18 |
| Semantic coverage D/I/H/Q | yes/no/no/no |
| Raw source claim | {"AccessType": "RW", "DataType": "0x0006", "DefaultValue": "1000", "ObjFlags": "0x00000000", "ObjectType": "7", "PDOMapping": "0", "ParameterName": "Event Timer"} |

### 0x1403:00

| Fact | Reviewed/source value |
| --- | --- |
| EDS name | HighestSubIndex |
| Sources | EDS [1403sub0] lines 533-541 |
| Applicability | eds_family_unbound |
| Declaration disposition | resolved |
| Documentary disposition | pending_qualification |
| Exclusion reason | `null` (not stated) |
| Units | entry count |
| Scaling | Integer record count; retain this entry's own EDS width. |
| Original low / high / default | [null, null, "5"] |
| Effect | Observe the declared entry count; not the physical semantics of its children. |
| Prerequisites | ["Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.", "Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates."] |
| Edge behavior | Not specified by the retained source. |
| Documentary review | {"gate_ids": ["Q01", "Q17", "Q20"], "generation_type": "resolved", "read_effect": "not_established", "status": "complete", "type_policy": "Retain exact EDS wire width and signedness; no conversion to another profile type.", "value_policy": "Five declared children; count default 5.", "write_policy": "not_writable"} |
| Conflicts | [] |
| Owner prompt | 17 |
| Semantic coverage D/I/H/Q | yes/no/no/no |
| Raw source claim | {"AccessType": "RO", "DataType": "0x0005", "DefaultValue": "5", "ObjFlags": "0x00000000", "ObjectType": "7", "PDOMapping": "0", "ParameterName": "HighestSubIndex"} |

### 0x1403:01

| Fact | Reviewed/source value |
| --- | --- |
| EDS name | COB-ID used by RPDO |
| Sources | EDS [1403sub1] lines 542-550 |
| Applicability | eds_family_unbound |
| Declaration disposition | resolved |
| Documentary disposition | pending_qualification |
| Exclusion reason | `null` (not stated) |
| Units | COB-ID |
| Scaling | Raw protocol field; no invented device timing or flags. |
| Original low / high / default | [null, null, "$NODEID+0x500"] |
| Effect | Changes PDO layout/route/timing; partial reconfiguration and delayed samples can remain ambiguous. |
| Prerequisites | ["Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.", "Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates."] |
| Edge behavior | Not specified by the retained source. |
| Documentary review | {"gate_ids": ["Q01", "Q17", "Q20"], "generation_type": "resolved", "read_effect": "not_established", "status": "complete", "type_policy": "Retain exact EDS wire width and signedness; no conversion to another profile type.", "value_policy": "Keep default $NODEID expression. A raw U32 is not proof every routing/flag combination is supported.", "write_policy": "qualification_required"} |
| Conflicts | [] |
| Owner prompt | 18 |
| Semantic coverage D/I/H/Q | yes/no/no/no |
| Raw source claim | {"AccessType": "RW", "DataType": "0x0007", "DefaultValue": "$NODEID+0x500", "ObjFlags": "0x00000000", "ObjectType": "7", "PDOMapping": "0", "ParameterName": "COB-ID used by RPDO"} |

### 0x1403:02

| Fact | Reviewed/source value |
| --- | --- |
| EDS name | Transmission Type |
| Sources | EDS [1403sub2] lines 551-559 |
| Applicability | eds_family_unbound |
| Declaration disposition | resolved |
| Documentary disposition | pending_qualification |
| Exclusion reason | `null` (not stated) |
| Units | transmission type |
| Scaling | Raw protocol field; no invented device timing or flags. |
| Original low / high / default | [null, null, "255"] |
| Effect | Changes PDO layout/route/timing; partial reconfiguration and delayed samples can remain ambiguous. |
| Prerequisites | ["Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.", "Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates."] |
| Edge behavior | Not specified by the retained source. |
| Documentary review | {"gate_ids": ["Q01", "Q17", "Q20"], "generation_type": "resolved", "read_effect": "not_established", "status": "complete", "type_policy": "Retain exact EDS wire width and signedness; no conversion to another profile type.", "value_policy": "Default 255 only; accepted synchronous/asynchronous types need evidence.", "write_policy": "qualification_required"} |
| Conflicts | [] |
| Owner prompt | 18 |
| Semantic coverage D/I/H/Q | yes/no/no/no |
| Raw source claim | {"AccessType": "RW", "DataType": "0x0005", "DefaultValue": "255", "ObjFlags": "0x00000000", "ObjectType": "7", "PDOMapping": "0", "ParameterName": "Transmission Type"} |

### 0x1403:03

| Fact | Reviewed/source value |
| --- | --- |
| EDS name | Inhibit Time |
| Sources | EDS [1403sub3] lines 560-568 |
| Applicability | eds_family_unbound |
| Declaration disposition | resolved |
| Documentary disposition | pending_qualification |
| Exclusion reason | `null` (not stated) |
| Units | inhibit time |
| Scaling | Raw protocol field; no invented device timing or flags. |
| Original low / high / default | [null, null, "0"] |
| Effect | Changes PDO layout/route/timing; partial reconfiguration and delayed samples can remain ambiguous. |
| Prerequisites | ["Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.", "Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates."] |
| Edge behavior | Not specified by the retained source. |
| Documentary review | {"gate_ids": ["Q01", "Q17", "Q20"], "generation_type": "resolved", "read_effect": "not_established", "status": "complete", "type_policy": "Retain exact EDS wire width and signedness; no conversion to another profile type.", "value_policy": "Default 0; units/effect not specified in this EDS.", "write_policy": "qualification_required"} |
| Conflicts | [] |
| Owner prompt | 18 |
| Semantic coverage D/I/H/Q | yes/no/no/no |
| Raw source claim | {"AccessType": "RW", "DataType": "0x0006", "DefaultValue": "0", "ObjFlags": "0x00000000", "ObjectType": "7", "PDOMapping": "0", "ParameterName": "Inhibit Time"} |

### 0x1403:04

| Fact | Reviewed/source value |
| --- | --- |
| EDS name | Compatibility Entry |
| Sources | EDS [1403sub4] lines 569-577 |
| Applicability | eds_family_unbound |
| Declaration disposition | resolved |
| Documentary disposition | pending_qualification |
| Exclusion reason | `null` (not stated) |
| Units | compatibility entry |
| Scaling | Raw protocol field; no invented device timing or flags. |
| Original low / high / default | [null, null, "0"] |
| Effect | Changes PDO layout/route/timing; partial reconfiguration and delayed samples can remain ambiguous. |
| Prerequisites | ["Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.", "Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates."] |
| Edge behavior | Not specified by the retained source. |
| Documentary review | {"gate_ids": ["Q01", "Q17", "Q20"], "generation_type": "resolved", "read_effect": "not_established", "status": "complete", "type_policy": "Retain exact EDS wire width and signedness; no conversion to another profile type.", "value_policy": "Default 0; no operational purpose established; no generic setter.", "write_policy": "not_writable"} |
| Conflicts | [] |
| Owner prompt | 18 |
| Semantic coverage D/I/H/Q | yes/no/no/no |
| Raw source claim | {"AccessType": "RW", "DataType": "0x0005", "DefaultValue": "0", "ObjFlags": "0x00000000", "ObjectType": "7", "PDOMapping": "0", "ParameterName": "Compatibility Entry"} |

### 0x1403:05

| Fact | Reviewed/source value |
| --- | --- |
| EDS name | Event Timer |
| Sources | EDS [1403sub5] lines 578-586 |
| Applicability | eds_family_unbound |
| Declaration disposition | resolved |
| Documentary disposition | pending_qualification |
| Exclusion reason | `null` (not stated) |
| Units | event timer |
| Scaling | Raw protocol field; no invented device timing or flags. |
| Original low / high / default | [null, null, "1000"] |
| Effect | Changes PDO layout/route/timing; partial reconfiguration and delayed samples can remain ambiguous. |
| Prerequisites | ["Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.", "Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates."] |
| Edge behavior | Not specified by the retained source. |
| Documentary review | {"gate_ids": ["Q01", "Q17", "Q20"], "generation_type": "resolved", "read_effect": "not_established", "status": "complete", "type_policy": "Retain exact EDS wire width and signedness; no conversion to another profile type.", "value_policy": "RPDO default 1000 / TPDO default 100; no drive host-loss reaction follows.", "write_policy": "qualification_required"} |
| Conflicts | [] |
| Owner prompt | 18 |
| Semantic coverage D/I/H/Q | yes/no/no/no |
| Raw source claim | {"AccessType": "RW", "DataType": "0x0006", "DefaultValue": "1000", "ObjFlags": "0x00000000", "ObjectType": "7", "PDOMapping": "0", "ParameterName": "Event Timer"} |

### 0x1600:00

| Fact | Reviewed/source value |
| --- | --- |
| EDS name | HighestSubIndex |
| Sources | EDS [1600sub0] lines 593-603 |
| Applicability | eds_family_unbound |
| Declaration disposition | resolved |
| Documentary disposition | pending_qualification |
| Exclusion reason | `null` (not stated) |
| Units | entry count |
| Scaling | Integer record count; retain this entry's own EDS width. |
| Original low / high / default | ["0", "4", "3"] |
| Effect | Changes PDO layout/route/timing; partial reconfiguration and delayed samples can remain ambiguous. |
| Prerequisites | ["Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.", "Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates."] |
| Edge behavior | Not specified by the retained source. |
| Documentary review | {"gate_ids": ["Q01", "Q17", "Q20"], "generation_type": "resolved", "read_effect": "not_established", "status": "complete", "type_policy": "Retain exact EDS wire width and signedness; no conversion to another profile type.", "value_policy": "Count 0..4; only that many leading slots active. No use of inactive or missing-default slots.", "write_policy": "qualification_required"} |
| Conflicts | [] |
| Owner prompt | 18 |
| Semantic coverage D/I/H/Q | yes/no/no/no |
| Raw source claim | {"AccessType": "RW", "DataType": "0x0005", "DefaultValue": "3", "HighLimit": "4", "LowLimit": "0", "ObjFlags": "0x00000000", "ObjectType": "7", "PDOMapping": "0", "ParameterName": "HighestSubIndex"} |

### 0x1600:01

| Fact | Reviewed/source value |
| --- | --- |
| EDS name | Object 1600_sub1 |
| Sources | EDS [1600sub1] lines 604-612 |
| Applicability | eds_family_unbound |
| Declaration disposition | resolved |
| Documentary disposition | pending_qualification |
| Exclusion reason | `null` (not stated) |
| Units | mapping descriptor |
| Scaling | Raw protocol field; no invented device timing or flags. |
| Original low / high / default | [null, null, "0x60400010"] |
| Effect | Changes PDO layout/route/timing; partial reconfiguration and delayed samples can remain ambiguous. |
| Prerequisites | ["Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.", "Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates."] |
| Edge behavior | Not specified by the retained source. |
| Documentary review | {"gate_ids": ["Q01", "Q17", "Q20"], "generation_type": "resolved", "read_effect": "not_established", "status": "complete", "type_policy": "Retain exact EDS wire width and signedness; no conversion to another profile type.", "value_policy": "U32 descriptor encodes index/subindex/bit width; at most four slots and eight Classical CAN payload bytes.", "write_policy": "qualification_required"} |
| Conflicts | [] |
| Owner prompt | 18 |
| Semantic coverage D/I/H/Q | yes/no/no/no |
| Raw source claim | {"AccessType": "RW", "DataType": "0x0007", "DefaultValue": "0x60400010", "ObjFlags": "0x00000000", "ObjectType": "7", "PDOMapping": "0", "ParameterName": "Object 1600_sub1"} |

### 0x1600:02

| Fact | Reviewed/source value |
| --- | --- |
| EDS name | Object 1600_sub2 |
| Sources | EDS [1600sub2] lines 613-621 |
| Applicability | eds_family_unbound |
| Declaration disposition | resolved |
| Documentary disposition | pending_qualification |
| Exclusion reason | `null` (not stated) |
| Units | mapping descriptor |
| Scaling | Raw protocol field; no invented device timing or flags. |
| Original low / high / default | [null, null, "0x60ff0020"] |
| Effect | Changes PDO layout/route/timing; partial reconfiguration and delayed samples can remain ambiguous. |
| Prerequisites | ["Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.", "Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates."] |
| Edge behavior | Not specified by the retained source. |
| Documentary review | {"gate_ids": ["Q01", "Q17", "Q20"], "generation_type": "resolved", "read_effect": "not_established", "status": "complete", "type_policy": "Retain exact EDS wire width and signedness; no conversion to another profile type.", "value_policy": "U32 descriptor encodes index/subindex/bit width; at most four slots and eight Classical CAN payload bytes.", "write_policy": "qualification_required"} |
| Conflicts | [] |
| Owner prompt | 18 |
| Semantic coverage D/I/H/Q | yes/no/no/no |
| Raw source claim | {"AccessType": "RW", "DataType": "0x0007", "DefaultValue": "0x60ff0020", "ObjFlags": "0x00000000", "ObjectType": "7", "PDOMapping": "0", "ParameterName": "Object 1600_sub2"} |

### 0x1600:03

| Fact | Reviewed/source value |
| --- | --- |
| EDS name | Object 1600_sub3 |
| Sources | EDS [1600sub3] lines 622-630 |
| Applicability | eds_family_unbound |
| Declaration disposition | resolved |
| Documentary disposition | pending_qualification |
| Exclusion reason | `null` (not stated) |
| Units | mapping descriptor |
| Scaling | Raw protocol field; no invented device timing or flags. |
| Original low / high / default | [null, null, "0x60600008"] |
| Effect | Changes PDO layout/route/timing; partial reconfiguration and delayed samples can remain ambiguous. |
| Prerequisites | ["Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.", "Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates."] |
| Edge behavior | Not specified by the retained source. |
| Documentary review | {"gate_ids": ["Q01", "Q17", "Q20"], "generation_type": "resolved", "read_effect": "not_established", "status": "complete", "type_policy": "Retain exact EDS wire width and signedness; no conversion to another profile type.", "value_policy": "U32 descriptor encodes index/subindex/bit width; at most four slots and eight Classical CAN payload bytes.", "write_policy": "qualification_required"} |
| Conflicts | [] |
| Owner prompt | 18 |
| Semantic coverage D/I/H/Q | yes/no/no/no |
| Raw source claim | {"AccessType": "RW", "DataType": "0x0007", "DefaultValue": "0x60600008", "ObjFlags": "0x00000000", "ObjectType": "7", "PDOMapping": "0", "ParameterName": "Object 1600_sub3"} |

### 0x1600:04

| Fact | Reviewed/source value |
| --- | --- |
| EDS name | Object 1600_sub4 |
| Sources | EDS [1600sub4] lines 631-638 |
| Applicability | eds_family_unbound |
| Declaration disposition | resolved |
| Documentary disposition | pending_qualification |
| Exclusion reason | `null` (not stated) |
| Units | mapping descriptor |
| Scaling | Raw protocol field; no invented device timing or flags. |
| Original low / high / default | [null, null, null] |
| Effect | Changes PDO layout/route/timing; partial reconfiguration and delayed samples can remain ambiguous. |
| Prerequisites | ["Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.", "Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates."] |
| Edge behavior | Not specified by the retained source. |
| Documentary review | {"gate_ids": ["Q01", "Q17", "Q20"], "generation_type": "resolved", "read_effect": "not_established", "status": "complete", "type_policy": "Retain exact EDS wire width and signedness; no conversion to another profile type.", "value_policy": "U32 descriptor encodes index/subindex/bit width; at most four slots and eight Classical CAN payload bytes.", "write_policy": "qualification_required"} |
| Conflicts | [] |
| Owner prompt | 18 |
| Semantic coverage D/I/H/Q | yes/no/no/no |
| Raw source claim | {"AccessType": "RW", "DataType": "0x0007", "ObjFlags": "0x00000000", "ObjectType": "7", "PDOMapping": "0", "ParameterName": "Object 1600_sub4"} |

### 0x1601:00

| Fact | Reviewed/source value |
| --- | --- |
| EDS name | HighestSubIndex |
| Sources | EDS [1601sub0] lines 645-655 |
| Applicability | eds_family_unbound |
| Declaration disposition | resolved |
| Documentary disposition | pending_qualification |
| Exclusion reason | `null` (not stated) |
| Units | entry count |
| Scaling | Integer record count; retain this entry's own EDS width. |
| Original low / high / default | ["0", "4", "2"] |
| Effect | Changes PDO layout/route/timing; partial reconfiguration and delayed samples can remain ambiguous. |
| Prerequisites | ["Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.", "Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates."] |
| Edge behavior | Not specified by the retained source. |
| Documentary review | {"gate_ids": ["Q01", "Q17", "Q20"], "generation_type": "resolved", "read_effect": "not_established", "status": "complete", "type_policy": "Retain exact EDS wire width and signedness; no conversion to another profile type.", "value_policy": "Count 0..4; only that many leading slots active. No use of inactive or missing-default slots.", "write_policy": "qualification_required"} |
| Conflicts | [] |
| Owner prompt | 18 |
| Semantic coverage D/I/H/Q | yes/no/no/no |
| Raw source claim | {"AccessType": "RW", "DataType": "0x0005", "DefaultValue": "2", "HighLimit": "4", "LowLimit": "0", "ObjFlags": "0x00000000", "ObjectType": "7", "PDOMapping": "0", "ParameterName": "HighestSubIndex"} |

### 0x1601:01

| Fact | Reviewed/source value |
| --- | --- |
| EDS name | Object 1601_sub1 |
| Sources | EDS [1601sub1] lines 656-664 |
| Applicability | eds_family_unbound |
| Declaration disposition | resolved |
| Documentary disposition | pending_qualification |
| Exclusion reason | `null` (not stated) |
| Units | mapping descriptor |
| Scaling | Raw protocol field; no invented device timing or flags. |
| Original low / high / default | [null, null, "0x607a0020"] |
| Effect | Changes PDO layout/route/timing; partial reconfiguration and delayed samples can remain ambiguous. |
| Prerequisites | ["Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.", "Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates."] |
| Edge behavior | Not specified by the retained source. |
| Documentary review | {"gate_ids": ["Q01", "Q17", "Q20"], "generation_type": "resolved", "read_effect": "not_established", "status": "complete", "type_policy": "Retain exact EDS wire width and signedness; no conversion to another profile type.", "value_policy": "U32 descriptor encodes index/subindex/bit width; at most four slots and eight Classical CAN payload bytes.", "write_policy": "qualification_required"} |
| Conflicts | [] |
| Owner prompt | 18 |
| Semantic coverage D/I/H/Q | yes/no/no/no |
| Raw source claim | {"AccessType": "RW", "DataType": "0x0007", "DefaultValue": "0x607a0020", "ObjFlags": "0x00000000", "ObjectType": "7", "PDOMapping": "0", "ParameterName": "Object 1601_sub1"} |

### 0x1601:02

| Fact | Reviewed/source value |
| --- | --- |
| EDS name | Object 1601_sub2 |
| Sources | EDS [1601sub2] lines 665-673 |
| Applicability | eds_family_unbound |
| Declaration disposition | resolved |
| Documentary disposition | pending_qualification |
| Exclusion reason | `null` (not stated) |
| Units | mapping descriptor |
| Scaling | Raw protocol field; no invented device timing or flags. |
| Original low / high / default | [null, null, "0x60810020"] |
| Effect | Changes PDO layout/route/timing; partial reconfiguration and delayed samples can remain ambiguous. |
| Prerequisites | ["Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.", "Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates."] |
| Edge behavior | Not specified by the retained source. |
| Documentary review | {"gate_ids": ["Q01", "Q17", "Q20"], "generation_type": "resolved", "read_effect": "not_established", "status": "complete", "type_policy": "Retain exact EDS wire width and signedness; no conversion to another profile type.", "value_policy": "U32 descriptor encodes index/subindex/bit width; at most four slots and eight Classical CAN payload bytes.", "write_policy": "qualification_required"} |
| Conflicts | [] |
| Owner prompt | 18 |
| Semantic coverage D/I/H/Q | yes/no/no/no |
| Raw source claim | {"AccessType": "RW", "DataType": "0x0007", "DefaultValue": "0x60810020", "ObjFlags": "0x00000000", "ObjectType": "7", "PDOMapping": "0", "ParameterName": "Object 1601_sub2"} |

### 0x1601:03

| Fact | Reviewed/source value |
| --- | --- |
| EDS name | Object 1601_sub3 |
| Sources | EDS [1601sub3] lines 674-681 |
| Applicability | eds_family_unbound |
| Declaration disposition | resolved |
| Documentary disposition | pending_qualification |
| Exclusion reason | `null` (not stated) |
| Units | mapping descriptor |
| Scaling | Raw protocol field; no invented device timing or flags. |
| Original low / high / default | [null, null, null] |
| Effect | Changes PDO layout/route/timing; partial reconfiguration and delayed samples can remain ambiguous. |
| Prerequisites | ["Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.", "Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates."] |
| Edge behavior | Not specified by the retained source. |
| Documentary review | {"gate_ids": ["Q01", "Q17", "Q20"], "generation_type": "resolved", "read_effect": "not_established", "status": "complete", "type_policy": "Retain exact EDS wire width and signedness; no conversion to another profile type.", "value_policy": "U32 descriptor encodes index/subindex/bit width; at most four slots and eight Classical CAN payload bytes.", "write_policy": "qualification_required"} |
| Conflicts | [] |
| Owner prompt | 18 |
| Semantic coverage D/I/H/Q | yes/no/no/no |
| Raw source claim | {"AccessType": "RW", "DataType": "0x0007", "ObjFlags": "0x00000000", "ObjectType": "7", "PDOMapping": "0", "ParameterName": "Object 1601_sub3"} |

### 0x1601:04

| Fact | Reviewed/source value |
| --- | --- |
| EDS name | Object 1601_sub4 |
| Sources | EDS [1601sub4] lines 682-689 |
| Applicability | eds_family_unbound |
| Declaration disposition | resolved |
| Documentary disposition | pending_qualification |
| Exclusion reason | `null` (not stated) |
| Units | mapping descriptor |
| Scaling | Raw protocol field; no invented device timing or flags. |
| Original low / high / default | [null, null, null] |
| Effect | Changes PDO layout/route/timing; partial reconfiguration and delayed samples can remain ambiguous. |
| Prerequisites | ["Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.", "Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates."] |
| Edge behavior | Not specified by the retained source. |
| Documentary review | {"gate_ids": ["Q01", "Q17", "Q20"], "generation_type": "resolved", "read_effect": "not_established", "status": "complete", "type_policy": "Retain exact EDS wire width and signedness; no conversion to another profile type.", "value_policy": "U32 descriptor encodes index/subindex/bit width; at most four slots and eight Classical CAN payload bytes.", "write_policy": "qualification_required"} |
| Conflicts | [] |
| Owner prompt | 18 |
| Semantic coverage D/I/H/Q | yes/no/no/no |
| Raw source claim | {"AccessType": "RW", "DataType": "0x0007", "ObjFlags": "0x00000000", "ObjectType": "7", "PDOMapping": "0", "ParameterName": "Object 1601_sub4"} |

### 0x1602:00

| Fact | Reviewed/source value |
| --- | --- |
| EDS name | HighestSubIndex |
| Sources | EDS [1602sub0] lines 696-706 |
| Applicability | eds_family_unbound |
| Declaration disposition | resolved |
| Documentary disposition | pending_qualification |
| Exclusion reason | `null` (not stated) |
| Units | entry count |
| Scaling | Integer record count; retain this entry's own EDS width. |
| Original low / high / default | ["0", "4", "0"] |
| Effect | Changes PDO layout/route/timing; partial reconfiguration and delayed samples can remain ambiguous. |
| Prerequisites | ["Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.", "Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates."] |
| Edge behavior | Not specified by the retained source. |
| Documentary review | {"gate_ids": ["Q01", "Q17", "Q20"], "generation_type": "resolved", "read_effect": "not_established", "status": "complete", "type_policy": "Retain exact EDS wire width and signedness; no conversion to another profile type.", "value_policy": "Count 0..4; only that many leading slots active. No use of inactive or missing-default slots.", "write_policy": "qualification_required"} |
| Conflicts | [] |
| Owner prompt | 18 |
| Semantic coverage D/I/H/Q | yes/no/no/no |
| Raw source claim | {"AccessType": "RW", "DataType": "0x0005", "DefaultValue": "0", "HighLimit": "4", "LowLimit": "0", "ObjFlags": "0x00000000", "ObjectType": "7", "PDOMapping": "0", "ParameterName": "HighestSubIndex"} |

### 0x1602:01

| Fact | Reviewed/source value |
| --- | --- |
| EDS name | Object 1602_sub1 |
| Sources | EDS [1602sub1] lines 707-714 |
| Applicability | eds_family_unbound |
| Declaration disposition | resolved |
| Documentary disposition | pending_qualification |
| Exclusion reason | `null` (not stated) |
| Units | mapping descriptor |
| Scaling | Raw protocol field; no invented device timing or flags. |
| Original low / high / default | [null, null, null] |
| Effect | Changes PDO layout/route/timing; partial reconfiguration and delayed samples can remain ambiguous. |
| Prerequisites | ["Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.", "Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates."] |
| Edge behavior | Not specified by the retained source. |
| Documentary review | {"gate_ids": ["Q01", "Q17", "Q20"], "generation_type": "resolved", "read_effect": "not_established", "status": "complete", "type_policy": "Retain exact EDS wire width and signedness; no conversion to another profile type.", "value_policy": "U32 descriptor encodes index/subindex/bit width; at most four slots and eight Classical CAN payload bytes.", "write_policy": "qualification_required"} |
| Conflicts | [] |
| Owner prompt | 18 |
| Semantic coverage D/I/H/Q | yes/no/no/no |
| Raw source claim | {"AccessType": "RW", "DataType": "0x0007", "ObjFlags": "0x00000000", "ObjectType": "7", "PDOMapping": "0", "ParameterName": "Object 1602_sub1"} |

### 0x1602:02

| Fact | Reviewed/source value |
| --- | --- |
| EDS name | Object 1602_sub2 |
| Sources | EDS [1602sub2] lines 715-722 |
| Applicability | eds_family_unbound |
| Declaration disposition | resolved |
| Documentary disposition | pending_qualification |
| Exclusion reason | `null` (not stated) |
| Units | mapping descriptor |
| Scaling | Raw protocol field; no invented device timing or flags. |
| Original low / high / default | [null, null, null] |
| Effect | Changes PDO layout/route/timing; partial reconfiguration and delayed samples can remain ambiguous. |
| Prerequisites | ["Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.", "Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates."] |
| Edge behavior | Not specified by the retained source. |
| Documentary review | {"gate_ids": ["Q01", "Q17", "Q20"], "generation_type": "resolved", "read_effect": "not_established", "status": "complete", "type_policy": "Retain exact EDS wire width and signedness; no conversion to another profile type.", "value_policy": "U32 descriptor encodes index/subindex/bit width; at most four slots and eight Classical CAN payload bytes.", "write_policy": "qualification_required"} |
| Conflicts | [] |
| Owner prompt | 18 |
| Semantic coverage D/I/H/Q | yes/no/no/no |
| Raw source claim | {"AccessType": "RW", "DataType": "0x0007", "ObjFlags": "0x00000000", "ObjectType": "7", "PDOMapping": "0", "ParameterName": "Object 1602_sub2"} |

### 0x1602:03

| Fact | Reviewed/source value |
| --- | --- |
| EDS name | Object 1602_sub3 |
| Sources | EDS [1602sub3] lines 723-730 |
| Applicability | eds_family_unbound |
| Declaration disposition | resolved |
| Documentary disposition | pending_qualification |
| Exclusion reason | `null` (not stated) |
| Units | mapping descriptor |
| Scaling | Raw protocol field; no invented device timing or flags. |
| Original low / high / default | [null, null, null] |
| Effect | Changes PDO layout/route/timing; partial reconfiguration and delayed samples can remain ambiguous. |
| Prerequisites | ["Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.", "Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates."] |
| Edge behavior | Not specified by the retained source. |
| Documentary review | {"gate_ids": ["Q01", "Q17", "Q20"], "generation_type": "resolved", "read_effect": "not_established", "status": "complete", "type_policy": "Retain exact EDS wire width and signedness; no conversion to another profile type.", "value_policy": "U32 descriptor encodes index/subindex/bit width; at most four slots and eight Classical CAN payload bytes.", "write_policy": "qualification_required"} |
| Conflicts | [] |
| Owner prompt | 18 |
| Semantic coverage D/I/H/Q | yes/no/no/no |
| Raw source claim | {"AccessType": "RW", "DataType": "0x0007", "ObjFlags": "0x00000000", "ObjectType": "7", "PDOMapping": "0", "ParameterName": "Object 1602_sub3"} |

### 0x1602:04

| Fact | Reviewed/source value |
| --- | --- |
| EDS name | Object 1602_sub4 |
| Sources | EDS [1602sub4] lines 731-738 |
| Applicability | eds_family_unbound |
| Declaration disposition | resolved |
| Documentary disposition | pending_qualification |
| Exclusion reason | `null` (not stated) |
| Units | mapping descriptor |
| Scaling | Raw protocol field; no invented device timing or flags. |
| Original low / high / default | [null, null, null] |
| Effect | Changes PDO layout/route/timing; partial reconfiguration and delayed samples can remain ambiguous. |
| Prerequisites | ["Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.", "Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates."] |
| Edge behavior | Not specified by the retained source. |
| Documentary review | {"gate_ids": ["Q01", "Q17", "Q20"], "generation_type": "resolved", "read_effect": "not_established", "status": "complete", "type_policy": "Retain exact EDS wire width and signedness; no conversion to another profile type.", "value_policy": "U32 descriptor encodes index/subindex/bit width; at most four slots and eight Classical CAN payload bytes.", "write_policy": "qualification_required"} |
| Conflicts | [] |
| Owner prompt | 18 |
| Semantic coverage D/I/H/Q | yes/no/no/no |
| Raw source claim | {"AccessType": "RW", "DataType": "0x0007", "ObjFlags": "0x00000000", "ObjectType": "7", "PDOMapping": "0", "ParameterName": "Object 1602_sub4"} |

### 0x1603:00

| Fact | Reviewed/source value |
| --- | --- |
| EDS name | HighestSubIndex |
| Sources | EDS [1603sub0] lines 745-755 |
| Applicability | eds_family_unbound |
| Declaration disposition | resolved |
| Documentary disposition | pending_qualification |
| Exclusion reason | `null` (not stated) |
| Units | entry count |
| Scaling | Integer record count; retain this entry's own EDS width. |
| Original low / high / default | ["0", "4", "0"] |
| Effect | Changes PDO layout/route/timing; partial reconfiguration and delayed samples can remain ambiguous. |
| Prerequisites | ["Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.", "Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates."] |
| Edge behavior | Not specified by the retained source. |
| Documentary review | {"gate_ids": ["Q01", "Q17", "Q20"], "generation_type": "resolved", "read_effect": "not_established", "status": "complete", "type_policy": "Retain exact EDS wire width and signedness; no conversion to another profile type.", "value_policy": "Count 0..4; only that many leading slots active. No use of inactive or missing-default slots.", "write_policy": "qualification_required"} |
| Conflicts | [] |
| Owner prompt | 18 |
| Semantic coverage D/I/H/Q | yes/no/no/no |
| Raw source claim | {"AccessType": "RW", "DataType": "0x0005", "DefaultValue": "0", "HighLimit": "4", "LowLimit": "0", "ObjFlags": "0x00000000", "ObjectType": "7", "PDOMapping": "0", "ParameterName": "HighestSubIndex"} |

### 0x1603:01

| Fact | Reviewed/source value |
| --- | --- |
| EDS name | Object 1603_sub1 |
| Sources | EDS [1603sub1] lines 756-763 |
| Applicability | eds_family_unbound |
| Declaration disposition | resolved |
| Documentary disposition | pending_qualification |
| Exclusion reason | `null` (not stated) |
| Units | mapping descriptor |
| Scaling | Raw protocol field; no invented device timing or flags. |
| Original low / high / default | [null, null, null] |
| Effect | Changes PDO layout/route/timing; partial reconfiguration and delayed samples can remain ambiguous. |
| Prerequisites | ["Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.", "Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates."] |
| Edge behavior | Not specified by the retained source. |
| Documentary review | {"gate_ids": ["Q01", "Q17", "Q20"], "generation_type": "resolved", "read_effect": "not_established", "status": "complete", "type_policy": "Retain exact EDS wire width and signedness; no conversion to another profile type.", "value_policy": "U32 descriptor encodes index/subindex/bit width; at most four slots and eight Classical CAN payload bytes.", "write_policy": "qualification_required"} |
| Conflicts | [] |
| Owner prompt | 18 |
| Semantic coverage D/I/H/Q | yes/no/no/no |
| Raw source claim | {"AccessType": "RW", "DataType": "0x0007", "ObjFlags": "0x00000000", "ObjectType": "7", "PDOMapping": "0", "ParameterName": "Object 1603_sub1"} |

### 0x1603:02

| Fact | Reviewed/source value |
| --- | --- |
| EDS name | Object 1603_sub2 |
| Sources | EDS [1603sub2] lines 764-771 |
| Applicability | eds_family_unbound |
| Declaration disposition | resolved |
| Documentary disposition | pending_qualification |
| Exclusion reason | `null` (not stated) |
| Units | mapping descriptor |
| Scaling | Raw protocol field; no invented device timing or flags. |
| Original low / high / default | [null, null, null] |
| Effect | Changes PDO layout/route/timing; partial reconfiguration and delayed samples can remain ambiguous. |
| Prerequisites | ["Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.", "Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates."] |
| Edge behavior | Not specified by the retained source. |
| Documentary review | {"gate_ids": ["Q01", "Q17", "Q20"], "generation_type": "resolved", "read_effect": "not_established", "status": "complete", "type_policy": "Retain exact EDS wire width and signedness; no conversion to another profile type.", "value_policy": "U32 descriptor encodes index/subindex/bit width; at most four slots and eight Classical CAN payload bytes.", "write_policy": "qualification_required"} |
| Conflicts | [] |
| Owner prompt | 18 |
| Semantic coverage D/I/H/Q | yes/no/no/no |
| Raw source claim | {"AccessType": "RW", "DataType": "0x0007", "ObjFlags": "0x00000000", "ObjectType": "7", "PDOMapping": "0", "ParameterName": "Object 1603_sub2"} |

### 0x1603:03

| Fact | Reviewed/source value |
| --- | --- |
| EDS name | Object 1603_sub3 |
| Sources | EDS [1603sub3] lines 772-779 |
| Applicability | eds_family_unbound |
| Declaration disposition | resolved |
| Documentary disposition | pending_qualification |
| Exclusion reason | `null` (not stated) |
| Units | mapping descriptor |
| Scaling | Raw protocol field; no invented device timing or flags. |
| Original low / high / default | [null, null, null] |
| Effect | Changes PDO layout/route/timing; partial reconfiguration and delayed samples can remain ambiguous. |
| Prerequisites | ["Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.", "Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates."] |
| Edge behavior | Not specified by the retained source. |
| Documentary review | {"gate_ids": ["Q01", "Q17", "Q20"], "generation_type": "resolved", "read_effect": "not_established", "status": "complete", "type_policy": "Retain exact EDS wire width and signedness; no conversion to another profile type.", "value_policy": "U32 descriptor encodes index/subindex/bit width; at most four slots and eight Classical CAN payload bytes.", "write_policy": "qualification_required"} |
| Conflicts | [] |
| Owner prompt | 18 |
| Semantic coverage D/I/H/Q | yes/no/no/no |
| Raw source claim | {"AccessType": "RW", "DataType": "0x0007", "ObjFlags": "0x00000000", "ObjectType": "7", "PDOMapping": "0", "ParameterName": "Object 1603_sub3"} |

### 0x1603:04

| Fact | Reviewed/source value |
| --- | --- |
| EDS name | Object 1603_sub4 |
| Sources | EDS [1603sub4] lines 780-787 |
| Applicability | eds_family_unbound |
| Declaration disposition | resolved |
| Documentary disposition | pending_qualification |
| Exclusion reason | `null` (not stated) |
| Units | mapping descriptor |
| Scaling | Raw protocol field; no invented device timing or flags. |
| Original low / high / default | [null, null, null] |
| Effect | Changes PDO layout/route/timing; partial reconfiguration and delayed samples can remain ambiguous. |
| Prerequisites | ["Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.", "Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates."] |
| Edge behavior | Not specified by the retained source. |
| Documentary review | {"gate_ids": ["Q01", "Q17", "Q20"], "generation_type": "resolved", "read_effect": "not_established", "status": "complete", "type_policy": "Retain exact EDS wire width and signedness; no conversion to another profile type.", "value_policy": "U32 descriptor encodes index/subindex/bit width; at most four slots and eight Classical CAN payload bytes.", "write_policy": "qualification_required"} |
| Conflicts | [] |
| Owner prompt | 18 |
| Semantic coverage D/I/H/Q | yes/no/no/no |
| Raw source claim | {"AccessType": "RW", "DataType": "0x0007", "ObjFlags": "0x00000000", "ObjectType": "7", "PDOMapping": "0", "ParameterName": "Object 1603_sub4"} |

### 0x1800:00

| Fact | Reviewed/source value |
| --- | --- |
| EDS name | HighestSubIndex |
| Sources | EDS [1800sub0] lines 794-802 |
| Applicability | eds_family_unbound |
| Declaration disposition | resolved |
| Documentary disposition | pending_qualification |
| Exclusion reason | `null` (not stated) |
| Units | entry count |
| Scaling | Integer record count; retain this entry's own EDS width. |
| Original low / high / default | [null, null, "5"] |
| Effect | Observe the declared entry count; not the physical semantics of its children. |
| Prerequisites | ["Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.", "Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates."] |
| Edge behavior | Not specified by the retained source. |
| Documentary review | {"gate_ids": ["Q01", "Q17", "Q20"], "generation_type": "resolved", "read_effect": "not_established", "status": "complete", "type_policy": "Retain exact EDS wire width and signedness; no conversion to another profile type.", "value_policy": "Five declared children; count default 5.", "write_policy": "not_writable"} |
| Conflicts | [] |
| Owner prompt | 17 |
| Semantic coverage D/I/H/Q | yes/no/no/no |
| Raw source claim | {"AccessType": "RO", "DataType": "0x0005", "DefaultValue": "5", "ObjFlags": "0x00000000", "ObjectType": "7", "PDOMapping": "0", "ParameterName": "HighestSubIndex"} |

### 0x1800:01

| Fact | Reviewed/source value |
| --- | --- |
| EDS name | COB-ID used by TPDO |
| Sources | EDS [1800sub1] lines 803-811 |
| Applicability | eds_family_unbound |
| Declaration disposition | resolved |
| Documentary disposition | pending_qualification |
| Exclusion reason | `null` (not stated) |
| Units | COB-ID |
| Scaling | Raw protocol field; no invented device timing or flags. |
| Original low / high / default | [null, null, "$NODEID+0x180"] |
| Effect | Changes PDO layout/route/timing; partial reconfiguration and delayed samples can remain ambiguous. |
| Prerequisites | ["Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.", "Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates."] |
| Edge behavior | Not specified by the retained source. |
| Documentary review | {"gate_ids": ["Q01", "Q17", "Q20"], "generation_type": "resolved", "read_effect": "not_established", "status": "complete", "type_policy": "Retain exact EDS wire width and signedness; no conversion to another profile type.", "value_policy": "Keep default $NODEID expression. A raw U32 is not proof every routing/flag combination is supported.", "write_policy": "qualification_required"} |
| Conflicts | [] |
| Owner prompt | 18 |
| Semantic coverage D/I/H/Q | yes/no/no/no |
| Raw source claim | {"AccessType": "RW", "DataType": "0x0007", "DefaultValue": "$NODEID+0x180", "ObjFlags": "0x00000000", "ObjectType": "7", "PDOMapping": "0", "ParameterName": "COB-ID used by TPDO"} |

### 0x1800:02

| Fact | Reviewed/source value |
| --- | --- |
| EDS name | Transmission Type |
| Sources | EDS [1800sub2] lines 812-820 |
| Applicability | eds_family_unbound |
| Declaration disposition | resolved |
| Documentary disposition | pending_qualification |
| Exclusion reason | `null` (not stated) |
| Units | transmission type |
| Scaling | Raw protocol field; no invented device timing or flags. |
| Original low / high / default | [null, null, "255"] |
| Effect | Changes PDO layout/route/timing; partial reconfiguration and delayed samples can remain ambiguous. |
| Prerequisites | ["Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.", "Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates."] |
| Edge behavior | Not specified by the retained source. |
| Documentary review | {"gate_ids": ["Q01", "Q17", "Q20"], "generation_type": "resolved", "read_effect": "not_established", "status": "complete", "type_policy": "Retain exact EDS wire width and signedness; no conversion to another profile type.", "value_policy": "Default 255 only; accepted synchronous/asynchronous types need evidence.", "write_policy": "qualification_required"} |
| Conflicts | [] |
| Owner prompt | 18 |
| Semantic coverage D/I/H/Q | yes/no/no/no |
| Raw source claim | {"AccessType": "RW", "DataType": "0x0005", "DefaultValue": "255", "ObjFlags": "0x00000000", "ObjectType": "7", "PDOMapping": "0", "ParameterName": "Transmission Type"} |

### 0x1800:03

| Fact | Reviewed/source value |
| --- | --- |
| EDS name | Inhibit Time |
| Sources | EDS [1800sub3] lines 821-829 |
| Applicability | eds_family_unbound |
| Declaration disposition | resolved |
| Documentary disposition | pending_qualification |
| Exclusion reason | `null` (not stated) |
| Units | inhibit time |
| Scaling | Raw protocol field; no invented device timing or flags. |
| Original low / high / default | [null, null, "0"] |
| Effect | Changes PDO layout/route/timing; partial reconfiguration and delayed samples can remain ambiguous. |
| Prerequisites | ["Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.", "Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates."] |
| Edge behavior | Not specified by the retained source. |
| Documentary review | {"gate_ids": ["Q01", "Q17", "Q20"], "generation_type": "resolved", "read_effect": "not_established", "status": "complete", "type_policy": "Retain exact EDS wire width and signedness; no conversion to another profile type.", "value_policy": "Default 0; units/effect not specified in this EDS.", "write_policy": "qualification_required"} |
| Conflicts | [] |
| Owner prompt | 18 |
| Semantic coverage D/I/H/Q | yes/no/no/no |
| Raw source claim | {"AccessType": "RW", "DataType": "0x0006", "DefaultValue": "0", "ObjFlags": "0x00000000", "ObjectType": "7", "PDOMapping": "0", "ParameterName": "Inhibit Time"} |

### 0x1800:04

| Fact | Reviewed/source value |
| --- | --- |
| EDS name | Compatibility Entry |
| Sources | EDS [1800sub4] lines 830-838 |
| Applicability | eds_family_unbound |
| Declaration disposition | resolved |
| Documentary disposition | pending_qualification |
| Exclusion reason | `null` (not stated) |
| Units | compatibility entry |
| Scaling | Raw protocol field; no invented device timing or flags. |
| Original low / high / default | [null, null, "0"] |
| Effect | Changes PDO layout/route/timing; partial reconfiguration and delayed samples can remain ambiguous. |
| Prerequisites | ["Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.", "Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates."] |
| Edge behavior | Not specified by the retained source. |
| Documentary review | {"gate_ids": ["Q01", "Q17", "Q20"], "generation_type": "resolved", "read_effect": "not_established", "status": "complete", "type_policy": "Retain exact EDS wire width and signedness; no conversion to another profile type.", "value_policy": "Default 0; no operational purpose established; no generic setter.", "write_policy": "not_writable"} |
| Conflicts | [] |
| Owner prompt | 18 |
| Semantic coverage D/I/H/Q | yes/no/no/no |
| Raw source claim | {"AccessType": "RW", "DataType": "0x0005", "DefaultValue": "0", "ObjFlags": "0x00000000", "ObjectType": "7", "PDOMapping": "0", "ParameterName": "Compatibility Entry"} |

### 0x1800:05

| Fact | Reviewed/source value |
| --- | --- |
| EDS name | Event Timer |
| Sources | EDS [1800sub5] lines 839-847 |
| Applicability | eds_family_unbound |
| Declaration disposition | resolved |
| Documentary disposition | pending_qualification |
| Exclusion reason | `null` (not stated) |
| Units | event timer |
| Scaling | Raw protocol field; no invented device timing or flags. |
| Original low / high / default | [null, null, "100"] |
| Effect | Changes PDO layout/route/timing; partial reconfiguration and delayed samples can remain ambiguous. |
| Prerequisites | ["Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.", "Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates."] |
| Edge behavior | Not specified by the retained source. |
| Documentary review | {"gate_ids": ["Q01", "Q17", "Q20"], "generation_type": "resolved", "read_effect": "not_established", "status": "complete", "type_policy": "Retain exact EDS wire width and signedness; no conversion to another profile type.", "value_policy": "RPDO default 1000 / TPDO default 100; no drive host-loss reaction follows.", "write_policy": "qualification_required"} |
| Conflicts | [] |
| Owner prompt | 18 |
| Semantic coverage D/I/H/Q | yes/no/no/no |
| Raw source claim | {"AccessType": "RW", "DataType": "0x0006", "DefaultValue": "100", "ObjFlags": "0x00000000", "ObjectType": "7", "PDOMapping": "0", "ParameterName": "Event Timer"} |

### 0x1801:00

| Fact | Reviewed/source value |
| --- | --- |
| EDS name | HighestSubIndex |
| Sources | EDS [1801sub0] lines 854-862 |
| Applicability | eds_family_unbound |
| Declaration disposition | resolved |
| Documentary disposition | pending_qualification |
| Exclusion reason | `null` (not stated) |
| Units | entry count |
| Scaling | Integer record count; retain this entry's own EDS width. |
| Original low / high / default | [null, null, "5"] |
| Effect | Observe the declared entry count; not the physical semantics of its children. |
| Prerequisites | ["Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.", "Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates."] |
| Edge behavior | Not specified by the retained source. |
| Documentary review | {"gate_ids": ["Q01", "Q17", "Q20"], "generation_type": "resolved", "read_effect": "not_established", "status": "complete", "type_policy": "Retain exact EDS wire width and signedness; no conversion to another profile type.", "value_policy": "Five declared children; count default 5.", "write_policy": "not_writable"} |
| Conflicts | [] |
| Owner prompt | 17 |
| Semantic coverage D/I/H/Q | yes/no/no/no |
| Raw source claim | {"AccessType": "RO", "DataType": "0x0005", "DefaultValue": "5", "ObjFlags": "0x00000000", "ObjectType": "7", "PDOMapping": "0", "ParameterName": "HighestSubIndex"} |

### 0x1801:01

| Fact | Reviewed/source value |
| --- | --- |
| EDS name | COB-ID used by TPDO |
| Sources | EDS [1801sub1] lines 863-871 |
| Applicability | eds_family_unbound |
| Declaration disposition | resolved |
| Documentary disposition | pending_qualification |
| Exclusion reason | `null` (not stated) |
| Units | COB-ID |
| Scaling | Raw protocol field; no invented device timing or flags. |
| Original low / high / default | [null, null, "$NODEID+0x280"] |
| Effect | Changes PDO layout/route/timing; partial reconfiguration and delayed samples can remain ambiguous. |
| Prerequisites | ["Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.", "Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates."] |
| Edge behavior | Not specified by the retained source. |
| Documentary review | {"gate_ids": ["Q01", "Q17", "Q20"], "generation_type": "resolved", "read_effect": "not_established", "status": "complete", "type_policy": "Retain exact EDS wire width and signedness; no conversion to another profile type.", "value_policy": "Keep default $NODEID expression. A raw U32 is not proof every routing/flag combination is supported.", "write_policy": "qualification_required"} |
| Conflicts | [] |
| Owner prompt | 18 |
| Semantic coverage D/I/H/Q | yes/no/no/no |
| Raw source claim | {"AccessType": "RW", "DataType": "0x0007", "DefaultValue": "$NODEID+0x280", "ObjFlags": "0x00000000", "ObjectType": "7", "PDOMapping": "0", "ParameterName": "COB-ID used by TPDO"} |

### 0x1801:02

| Fact | Reviewed/source value |
| --- | --- |
| EDS name | Transmission Type |
| Sources | EDS [1801sub2] lines 872-880 |
| Applicability | eds_family_unbound |
| Declaration disposition | resolved |
| Documentary disposition | pending_qualification |
| Exclusion reason | `null` (not stated) |
| Units | transmission type |
| Scaling | Raw protocol field; no invented device timing or flags. |
| Original low / high / default | [null, null, "255"] |
| Effect | Changes PDO layout/route/timing; partial reconfiguration and delayed samples can remain ambiguous. |
| Prerequisites | ["Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.", "Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates."] |
| Edge behavior | Not specified by the retained source. |
| Documentary review | {"gate_ids": ["Q01", "Q17", "Q20"], "generation_type": "resolved", "read_effect": "not_established", "status": "complete", "type_policy": "Retain exact EDS wire width and signedness; no conversion to another profile type.", "value_policy": "Default 255 only; accepted synchronous/asynchronous types need evidence.", "write_policy": "qualification_required"} |
| Conflicts | [] |
| Owner prompt | 18 |
| Semantic coverage D/I/H/Q | yes/no/no/no |
| Raw source claim | {"AccessType": "RW", "DataType": "0x0005", "DefaultValue": "255", "ObjFlags": "0x00000000", "ObjectType": "7", "PDOMapping": "0", "ParameterName": "Transmission Type"} |

### 0x1801:03

| Fact | Reviewed/source value |
| --- | --- |
| EDS name | Inhibit Time |
| Sources | EDS [1801sub3] lines 881-889 |
| Applicability | eds_family_unbound |
| Declaration disposition | resolved |
| Documentary disposition | pending_qualification |
| Exclusion reason | `null` (not stated) |
| Units | inhibit time |
| Scaling | Raw protocol field; no invented device timing or flags. |
| Original low / high / default | [null, null, "0"] |
| Effect | Changes PDO layout/route/timing; partial reconfiguration and delayed samples can remain ambiguous. |
| Prerequisites | ["Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.", "Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates."] |
| Edge behavior | Not specified by the retained source. |
| Documentary review | {"gate_ids": ["Q01", "Q17", "Q20"], "generation_type": "resolved", "read_effect": "not_established", "status": "complete", "type_policy": "Retain exact EDS wire width and signedness; no conversion to another profile type.", "value_policy": "Default 0; units/effect not specified in this EDS.", "write_policy": "qualification_required"} |
| Conflicts | [] |
| Owner prompt | 18 |
| Semantic coverage D/I/H/Q | yes/no/no/no |
| Raw source claim | {"AccessType": "RW", "DataType": "0x0006", "DefaultValue": "0", "ObjFlags": "0x00000000", "ObjectType": "7", "PDOMapping": "0", "ParameterName": "Inhibit Time"} |

### 0x1801:04

| Fact | Reviewed/source value |
| --- | --- |
| EDS name | Compatibility Entry |
| Sources | EDS [1801sub4] lines 890-898 |
| Applicability | eds_family_unbound |
| Declaration disposition | resolved |
| Documentary disposition | pending_qualification |
| Exclusion reason | `null` (not stated) |
| Units | compatibility entry |
| Scaling | Raw protocol field; no invented device timing or flags. |
| Original low / high / default | [null, null, "0"] |
| Effect | Changes PDO layout/route/timing; partial reconfiguration and delayed samples can remain ambiguous. |
| Prerequisites | ["Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.", "Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates."] |
| Edge behavior | Not specified by the retained source. |
| Documentary review | {"gate_ids": ["Q01", "Q17", "Q20"], "generation_type": "resolved", "read_effect": "not_established", "status": "complete", "type_policy": "Retain exact EDS wire width and signedness; no conversion to another profile type.", "value_policy": "Default 0; no operational purpose established; no generic setter.", "write_policy": "not_writable"} |
| Conflicts | [] |
| Owner prompt | 18 |
| Semantic coverage D/I/H/Q | yes/no/no/no |
| Raw source claim | {"AccessType": "RW", "DataType": "0x0005", "DefaultValue": "0", "ObjFlags": "0x00000000", "ObjectType": "7", "PDOMapping": "0", "ParameterName": "Compatibility Entry"} |

### 0x1801:05

| Fact | Reviewed/source value |
| --- | --- |
| EDS name | Event Timer |
| Sources | EDS [1801sub5] lines 899-907 |
| Applicability | eds_family_unbound |
| Declaration disposition | resolved |
| Documentary disposition | pending_qualification |
| Exclusion reason | `null` (not stated) |
| Units | event timer |
| Scaling | Raw protocol field; no invented device timing or flags. |
| Original low / high / default | [null, null, "100"] |
| Effect | Changes PDO layout/route/timing; partial reconfiguration and delayed samples can remain ambiguous. |
| Prerequisites | ["Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.", "Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates."] |
| Edge behavior | Not specified by the retained source. |
| Documentary review | {"gate_ids": ["Q01", "Q17", "Q20"], "generation_type": "resolved", "read_effect": "not_established", "status": "complete", "type_policy": "Retain exact EDS wire width and signedness; no conversion to another profile type.", "value_policy": "RPDO default 1000 / TPDO default 100; no drive host-loss reaction follows.", "write_policy": "qualification_required"} |
| Conflicts | [] |
| Owner prompt | 18 |
| Semantic coverage D/I/H/Q | yes/no/no/no |
| Raw source claim | {"AccessType": "RW", "DataType": "0x0006", "DefaultValue": "100", "ObjFlags": "0x00000000", "ObjectType": "7", "PDOMapping": "0", "ParameterName": "Event Timer"} |

### 0x1802:00

| Fact | Reviewed/source value |
| --- | --- |
| EDS name | HighestSubIndex |
| Sources | EDS [1802sub0] lines 914-922 |
| Applicability | eds_family_unbound |
| Declaration disposition | resolved |
| Documentary disposition | pending_qualification |
| Exclusion reason | `null` (not stated) |
| Units | entry count |
| Scaling | Integer record count; retain this entry's own EDS width. |
| Original low / high / default | [null, null, "5"] |
| Effect | Observe the declared entry count; not the physical semantics of its children. |
| Prerequisites | ["Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.", "Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates."] |
| Edge behavior | Not specified by the retained source. |
| Documentary review | {"gate_ids": ["Q01", "Q17", "Q20"], "generation_type": "resolved", "read_effect": "not_established", "status": "complete", "type_policy": "Retain exact EDS wire width and signedness; no conversion to another profile type.", "value_policy": "Five declared children; count default 5.", "write_policy": "not_writable"} |
| Conflicts | [] |
| Owner prompt | 17 |
| Semantic coverage D/I/H/Q | yes/no/no/no |
| Raw source claim | {"AccessType": "RO", "DataType": "0x0005", "DefaultValue": "5", "ObjFlags": "0x00000000", "ObjectType": "7", "PDOMapping": "0", "ParameterName": "HighestSubIndex"} |

### 0x1802:01

| Fact | Reviewed/source value |
| --- | --- |
| EDS name | COB-ID used by TPDO |
| Sources | EDS [1802sub1] lines 923-931 |
| Applicability | eds_family_unbound |
| Declaration disposition | resolved |
| Documentary disposition | pending_qualification |
| Exclusion reason | `null` (not stated) |
| Units | COB-ID |
| Scaling | Raw protocol field; no invented device timing or flags. |
| Original low / high / default | [null, null, "$NODEID+0x380"] |
| Effect | Changes PDO layout/route/timing; partial reconfiguration and delayed samples can remain ambiguous. |
| Prerequisites | ["Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.", "Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates."] |
| Edge behavior | Not specified by the retained source. |
| Documentary review | {"gate_ids": ["Q01", "Q17", "Q20"], "generation_type": "resolved", "read_effect": "not_established", "status": "complete", "type_policy": "Retain exact EDS wire width and signedness; no conversion to another profile type.", "value_policy": "Keep default $NODEID expression. A raw U32 is not proof every routing/flag combination is supported.", "write_policy": "qualification_required"} |
| Conflicts | [] |
| Owner prompt | 18 |
| Semantic coverage D/I/H/Q | yes/no/no/no |
| Raw source claim | {"AccessType": "RW", "DataType": "0x0007", "DefaultValue": "$NODEID+0x380", "ObjFlags": "0x00000000", "ObjectType": "7", "PDOMapping": "0", "ParameterName": "COB-ID used by TPDO"} |

### 0x1802:02

| Fact | Reviewed/source value |
| --- | --- |
| EDS name | Transmission Type |
| Sources | EDS [1802sub2] lines 932-940 |
| Applicability | eds_family_unbound |
| Declaration disposition | resolved |
| Documentary disposition | pending_qualification |
| Exclusion reason | `null` (not stated) |
| Units | transmission type |
| Scaling | Raw protocol field; no invented device timing or flags. |
| Original low / high / default | [null, null, "255"] |
| Effect | Changes PDO layout/route/timing; partial reconfiguration and delayed samples can remain ambiguous. |
| Prerequisites | ["Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.", "Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates."] |
| Edge behavior | Not specified by the retained source. |
| Documentary review | {"gate_ids": ["Q01", "Q17", "Q20"], "generation_type": "resolved", "read_effect": "not_established", "status": "complete", "type_policy": "Retain exact EDS wire width and signedness; no conversion to another profile type.", "value_policy": "Default 255 only; accepted synchronous/asynchronous types need evidence.", "write_policy": "qualification_required"} |
| Conflicts | [] |
| Owner prompt | 18 |
| Semantic coverage D/I/H/Q | yes/no/no/no |
| Raw source claim | {"AccessType": "RW", "DataType": "0x0005", "DefaultValue": "255", "ObjFlags": "0x00000000", "ObjectType": "7", "PDOMapping": "0", "ParameterName": "Transmission Type"} |

### 0x1802:03

| Fact | Reviewed/source value |
| --- | --- |
| EDS name | Inhibit Time |
| Sources | EDS [1802sub3] lines 941-949 |
| Applicability | eds_family_unbound |
| Declaration disposition | resolved |
| Documentary disposition | pending_qualification |
| Exclusion reason | `null` (not stated) |
| Units | inhibit time |
| Scaling | Raw protocol field; no invented device timing or flags. |
| Original low / high / default | [null, null, "0"] |
| Effect | Changes PDO layout/route/timing; partial reconfiguration and delayed samples can remain ambiguous. |
| Prerequisites | ["Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.", "Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates."] |
| Edge behavior | Not specified by the retained source. |
| Documentary review | {"gate_ids": ["Q01", "Q17", "Q20"], "generation_type": "resolved", "read_effect": "not_established", "status": "complete", "type_policy": "Retain exact EDS wire width and signedness; no conversion to another profile type.", "value_policy": "Default 0; units/effect not specified in this EDS.", "write_policy": "qualification_required"} |
| Conflicts | [] |
| Owner prompt | 18 |
| Semantic coverage D/I/H/Q | yes/no/no/no |
| Raw source claim | {"AccessType": "RW", "DataType": "0x0006", "DefaultValue": "0", "ObjFlags": "0x00000000", "ObjectType": "7", "PDOMapping": "0", "ParameterName": "Inhibit Time"} |

### 0x1802:04

| Fact | Reviewed/source value |
| --- | --- |
| EDS name | Compatibility Entry |
| Sources | EDS [1802sub4] lines 950-958 |
| Applicability | eds_family_unbound |
| Declaration disposition | resolved |
| Documentary disposition | pending_qualification |
| Exclusion reason | `null` (not stated) |
| Units | compatibility entry |
| Scaling | Raw protocol field; no invented device timing or flags. |
| Original low / high / default | [null, null, "0"] |
| Effect | Changes PDO layout/route/timing; partial reconfiguration and delayed samples can remain ambiguous. |
| Prerequisites | ["Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.", "Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates."] |
| Edge behavior | Not specified by the retained source. |
| Documentary review | {"gate_ids": ["Q01", "Q17", "Q20"], "generation_type": "resolved", "read_effect": "not_established", "status": "complete", "type_policy": "Retain exact EDS wire width and signedness; no conversion to another profile type.", "value_policy": "Default 0; no operational purpose established; no generic setter.", "write_policy": "not_writable"} |
| Conflicts | [] |
| Owner prompt | 18 |
| Semantic coverage D/I/H/Q | yes/no/no/no |
| Raw source claim | {"AccessType": "RW", "DataType": "0x0005", "DefaultValue": "0", "ObjFlags": "0x00000000", "ObjectType": "7", "PDOMapping": "0", "ParameterName": "Compatibility Entry"} |

### 0x1802:05

| Fact | Reviewed/source value |
| --- | --- |
| EDS name | Event Timer |
| Sources | EDS [1802sub5] lines 959-967 |
| Applicability | eds_family_unbound |
| Declaration disposition | resolved |
| Documentary disposition | pending_qualification |
| Exclusion reason | `null` (not stated) |
| Units | event timer |
| Scaling | Raw protocol field; no invented device timing or flags. |
| Original low / high / default | [null, null, "100"] |
| Effect | Changes PDO layout/route/timing; partial reconfiguration and delayed samples can remain ambiguous. |
| Prerequisites | ["Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.", "Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates."] |
| Edge behavior | Not specified by the retained source. |
| Documentary review | {"gate_ids": ["Q01", "Q17", "Q20"], "generation_type": "resolved", "read_effect": "not_established", "status": "complete", "type_policy": "Retain exact EDS wire width and signedness; no conversion to another profile type.", "value_policy": "RPDO default 1000 / TPDO default 100; no drive host-loss reaction follows.", "write_policy": "qualification_required"} |
| Conflicts | [] |
| Owner prompt | 18 |
| Semantic coverage D/I/H/Q | yes/no/no/no |
| Raw source claim | {"AccessType": "RW", "DataType": "0x0006", "DefaultValue": "100", "ObjFlags": "0x00000000", "ObjectType": "7", "PDOMapping": "0", "ParameterName": "Event Timer"} |

### 0x1803:00

| Fact | Reviewed/source value |
| --- | --- |
| EDS name | HighestSubIndex |
| Sources | EDS [1803sub0] lines 974-982 |
| Applicability | eds_family_unbound |
| Declaration disposition | resolved |
| Documentary disposition | pending_qualification |
| Exclusion reason | `null` (not stated) |
| Units | entry count |
| Scaling | Integer record count; retain this entry's own EDS width. |
| Original low / high / default | [null, null, "5"] |
| Effect | Observe the declared entry count; not the physical semantics of its children. |
| Prerequisites | ["Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.", "Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates."] |
| Edge behavior | Not specified by the retained source. |
| Documentary review | {"gate_ids": ["Q01", "Q17", "Q20"], "generation_type": "resolved", "read_effect": "not_established", "status": "complete", "type_policy": "Retain exact EDS wire width and signedness; no conversion to another profile type.", "value_policy": "Five declared children; count default 5.", "write_policy": "not_writable"} |
| Conflicts | [] |
| Owner prompt | 17 |
| Semantic coverage D/I/H/Q | yes/no/no/no |
| Raw source claim | {"AccessType": "RO", "DataType": "0x0005", "DefaultValue": "5", "ObjFlags": "0x00000000", "ObjectType": "7", "PDOMapping": "0", "ParameterName": "HighestSubIndex"} |

### 0x1803:01

| Fact | Reviewed/source value |
| --- | --- |
| EDS name | COB-ID used by TPDO |
| Sources | EDS [1803sub1] lines 983-991 |
| Applicability | eds_family_unbound |
| Declaration disposition | resolved |
| Documentary disposition | pending_qualification |
| Exclusion reason | `null` (not stated) |
| Units | COB-ID |
| Scaling | Raw protocol field; no invented device timing or flags. |
| Original low / high / default | [null, null, "$NODEID+0x480"] |
| Effect | Changes PDO layout/route/timing; partial reconfiguration and delayed samples can remain ambiguous. |
| Prerequisites | ["Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.", "Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates."] |
| Edge behavior | Not specified by the retained source. |
| Documentary review | {"gate_ids": ["Q01", "Q17", "Q20"], "generation_type": "resolved", "read_effect": "not_established", "status": "complete", "type_policy": "Retain exact EDS wire width and signedness; no conversion to another profile type.", "value_policy": "Keep default $NODEID expression. A raw U32 is not proof every routing/flag combination is supported.", "write_policy": "qualification_required"} |
| Conflicts | [] |
| Owner prompt | 18 |
| Semantic coverage D/I/H/Q | yes/no/no/no |
| Raw source claim | {"AccessType": "RW", "DataType": "0x0007", "DefaultValue": "$NODEID+0x480", "ObjFlags": "0x00000000", "ObjectType": "7", "PDOMapping": "0", "ParameterName": "COB-ID used by TPDO"} |

### 0x1803:02

| Fact | Reviewed/source value |
| --- | --- |
| EDS name | Transmission Type |
| Sources | EDS [1803sub2] lines 992-1000 |
| Applicability | eds_family_unbound |
| Declaration disposition | resolved |
| Documentary disposition | pending_qualification |
| Exclusion reason | `null` (not stated) |
| Units | transmission type |
| Scaling | Raw protocol field; no invented device timing or flags. |
| Original low / high / default | [null, null, "255"] |
| Effect | Changes PDO layout/route/timing; partial reconfiguration and delayed samples can remain ambiguous. |
| Prerequisites | ["Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.", "Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates."] |
| Edge behavior | Not specified by the retained source. |
| Documentary review | {"gate_ids": ["Q01", "Q17", "Q20"], "generation_type": "resolved", "read_effect": "not_established", "status": "complete", "type_policy": "Retain exact EDS wire width and signedness; no conversion to another profile type.", "value_policy": "Default 255 only; accepted synchronous/asynchronous types need evidence.", "write_policy": "qualification_required"} |
| Conflicts | [] |
| Owner prompt | 18 |
| Semantic coverage D/I/H/Q | yes/no/no/no |
| Raw source claim | {"AccessType": "RW", "DataType": "0x0005", "DefaultValue": "255", "ObjFlags": "0x00000000", "ObjectType": "7", "PDOMapping": "0", "ParameterName": "Transmission Type"} |

### 0x1803:03

| Fact | Reviewed/source value |
| --- | --- |
| EDS name | Inhibit Time |
| Sources | EDS [1803sub3] lines 1001-1009 |
| Applicability | eds_family_unbound |
| Declaration disposition | resolved |
| Documentary disposition | pending_qualification |
| Exclusion reason | `null` (not stated) |
| Units | inhibit time |
| Scaling | Raw protocol field; no invented device timing or flags. |
| Original low / high / default | [null, null, "0"] |
| Effect | Changes PDO layout/route/timing; partial reconfiguration and delayed samples can remain ambiguous. |
| Prerequisites | ["Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.", "Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates."] |
| Edge behavior | Not specified by the retained source. |
| Documentary review | {"gate_ids": ["Q01", "Q17", "Q20"], "generation_type": "resolved", "read_effect": "not_established", "status": "complete", "type_policy": "Retain exact EDS wire width and signedness; no conversion to another profile type.", "value_policy": "Default 0; units/effect not specified in this EDS.", "write_policy": "qualification_required"} |
| Conflicts | [] |
| Owner prompt | 18 |
| Semantic coverage D/I/H/Q | yes/no/no/no |
| Raw source claim | {"AccessType": "RW", "DataType": "0x0006", "DefaultValue": "0", "ObjFlags": "0x00000000", "ObjectType": "7", "PDOMapping": "0", "ParameterName": "Inhibit Time"} |

### 0x1803:04

| Fact | Reviewed/source value |
| --- | --- |
| EDS name | Compatibility Entry |
| Sources | EDS [1803sub4] lines 1010-1018 |
| Applicability | eds_family_unbound |
| Declaration disposition | resolved |
| Documentary disposition | pending_qualification |
| Exclusion reason | `null` (not stated) |
| Units | compatibility entry |
| Scaling | Raw protocol field; no invented device timing or flags. |
| Original low / high / default | [null, null, "0"] |
| Effect | Changes PDO layout/route/timing; partial reconfiguration and delayed samples can remain ambiguous. |
| Prerequisites | ["Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.", "Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates."] |
| Edge behavior | Not specified by the retained source. |
| Documentary review | {"gate_ids": ["Q01", "Q17", "Q20"], "generation_type": "resolved", "read_effect": "not_established", "status": "complete", "type_policy": "Retain exact EDS wire width and signedness; no conversion to another profile type.", "value_policy": "Default 0; no operational purpose established; no generic setter.", "write_policy": "not_writable"} |
| Conflicts | [] |
| Owner prompt | 18 |
| Semantic coverage D/I/H/Q | yes/no/no/no |
| Raw source claim | {"AccessType": "RW", "DataType": "0x0005", "DefaultValue": "0", "ObjFlags": "0x00000000", "ObjectType": "7", "PDOMapping": "0", "ParameterName": "Compatibility Entry"} |

### 0x1803:05

| Fact | Reviewed/source value |
| --- | --- |
| EDS name | Event Timer |
| Sources | EDS [1803sub5] lines 1019-1027 |
| Applicability | eds_family_unbound |
| Declaration disposition | resolved |
| Documentary disposition | pending_qualification |
| Exclusion reason | `null` (not stated) |
| Units | event timer |
| Scaling | Raw protocol field; no invented device timing or flags. |
| Original low / high / default | [null, null, "100"] |
| Effect | Changes PDO layout/route/timing; partial reconfiguration and delayed samples can remain ambiguous. |
| Prerequisites | ["Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.", "Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates."] |
| Edge behavior | Not specified by the retained source. |
| Documentary review | {"gate_ids": ["Q01", "Q17", "Q20"], "generation_type": "resolved", "read_effect": "not_established", "status": "complete", "type_policy": "Retain exact EDS wire width and signedness; no conversion to another profile type.", "value_policy": "RPDO default 1000 / TPDO default 100; no drive host-loss reaction follows.", "write_policy": "qualification_required"} |
| Conflicts | [] |
| Owner prompt | 18 |
| Semantic coverage D/I/H/Q | yes/no/no/no |
| Raw source claim | {"AccessType": "RW", "DataType": "0x0006", "DefaultValue": "100", "ObjFlags": "0x00000000", "ObjectType": "7", "PDOMapping": "0", "ParameterName": "Event Timer"} |

### 0x1A00:00

| Fact | Reviewed/source value |
| --- | --- |
| EDS name | HighestSubIndex |
| Sources | EDS [1A00sub0] lines 1034-1044 |
| Applicability | eds_family_unbound |
| Declaration disposition | resolved |
| Documentary disposition | pending_qualification |
| Exclusion reason | `null` (not stated) |
| Units | entry count |
| Scaling | Integer record count; retain this entry's own EDS width. |
| Original low / high / default | ["0", "4", "3"] |
| Effect | Changes PDO layout/route/timing; partial reconfiguration and delayed samples can remain ambiguous. |
| Prerequisites | ["Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.", "Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates."] |
| Edge behavior | Not specified by the retained source. |
| Documentary review | {"gate_ids": ["Q01", "Q17", "Q20"], "generation_type": "resolved", "read_effect": "not_established", "status": "complete", "type_policy": "Retain exact EDS wire width and signedness; no conversion to another profile type.", "value_policy": "Count 0..4; only that many leading slots active. No use of inactive or missing-default slots.", "write_policy": "qualification_required"} |
| Conflicts | [] |
| Owner prompt | 18 |
| Semantic coverage D/I/H/Q | yes/no/no/no |
| Raw source claim | {"AccessType": "RW", "DataType": "0x0005", "DefaultValue": "3", "HighLimit": "4", "LowLimit": "0", "ObjFlags": "0x00000000", "ObjectType": "7", "PDOMapping": "0", "ParameterName": "HighestSubIndex"} |

### 0x1A00:01

| Fact | Reviewed/source value |
| --- | --- |
| EDS name | Object 1A00_sub1 |
| Sources | EDS [1A00sub1] lines 1045-1053 |
| Applicability | eds_family_unbound |
| Declaration disposition | resolved |
| Documentary disposition | pending_qualification |
| Exclusion reason | `null` (not stated) |
| Units | mapping descriptor |
| Scaling | Raw protocol field; no invented device timing or flags. |
| Original low / high / default | [null, null, "0x60410010"] |
| Effect | Changes PDO layout/route/timing; partial reconfiguration and delayed samples can remain ambiguous. |
| Prerequisites | ["Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.", "Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates."] |
| Edge behavior | Not specified by the retained source. |
| Documentary review | {"gate_ids": ["Q01", "Q17", "Q20"], "generation_type": "resolved", "read_effect": "not_established", "status": "complete", "type_policy": "Retain exact EDS wire width and signedness; no conversion to another profile type.", "value_policy": "U32 descriptor encodes index/subindex/bit width; at most four slots and eight Classical CAN payload bytes.", "write_policy": "qualification_required"} |
| Conflicts | [] |
| Owner prompt | 18 |
| Semantic coverage D/I/H/Q | yes/no/no/no |
| Raw source claim | {"AccessType": "RW", "DataType": "0x0007", "DefaultValue": "0x60410010", "ObjFlags": "0x00000000", "ObjectType": "7", "PDOMapping": "0", "ParameterName": "Object 1A00_sub1"} |

### 0x1A00:02

| Fact | Reviewed/source value |
| --- | --- |
| EDS name | Object 1A00_sub2 |
| Sources | EDS [1A00sub2] lines 1054-1062 |
| Applicability | eds_family_unbound |
| Declaration disposition | resolved |
| Documentary disposition | pending_qualification |
| Exclusion reason | `null` (not stated) |
| Units | mapping descriptor |
| Scaling | Raw protocol field; no invented device timing or flags. |
| Original low / high / default | [null, null, "0x60fd0020"] |
| Effect | Changes PDO layout/route/timing; partial reconfiguration and delayed samples can remain ambiguous. |
| Prerequisites | ["Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.", "Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates."] |
| Edge behavior | Not specified by the retained source. |
| Documentary review | {"gate_ids": ["Q01", "Q17", "Q20"], "generation_type": "resolved", "read_effect": "not_established", "status": "complete", "type_policy": "Retain exact EDS wire width and signedness; no conversion to another profile type.", "value_policy": "U32 descriptor encodes index/subindex/bit width; at most four slots and eight Classical CAN payload bytes.", "write_policy": "qualification_required"} |
| Conflicts | [] |
| Owner prompt | 18 |
| Semantic coverage D/I/H/Q | yes/no/no/no |
| Raw source claim | {"AccessType": "RW", "DataType": "0x0007", "DefaultValue": "0x60fd0020", "ObjFlags": "0x00000000", "ObjectType": "7", "PDOMapping": "0", "ParameterName": "Object 1A00_sub2"} |

### 0x1A00:03

| Fact | Reviewed/source value |
| --- | --- |
| EDS name | Object 1A00_sub3 |
| Sources | EDS [1A00sub3] lines 1063-1071 |
| Applicability | eds_family_unbound |
| Declaration disposition | resolved |
| Documentary disposition | pending_qualification |
| Exclusion reason | `null` (not stated) |
| Units | mapping descriptor |
| Scaling | Raw protocol field; no invented device timing or flags. |
| Original low / high / default | [null, null, "0x60610008"] |
| Effect | Changes PDO layout/route/timing; partial reconfiguration and delayed samples can remain ambiguous. |
| Prerequisites | ["Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.", "Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates."] |
| Edge behavior | Not specified by the retained source. |
| Documentary review | {"gate_ids": ["Q01", "Q17", "Q20"], "generation_type": "resolved", "read_effect": "not_established", "status": "complete", "type_policy": "Retain exact EDS wire width and signedness; no conversion to another profile type.", "value_policy": "U32 descriptor encodes index/subindex/bit width; at most four slots and eight Classical CAN payload bytes.", "write_policy": "qualification_required"} |
| Conflicts | [] |
| Owner prompt | 18 |
| Semantic coverage D/I/H/Q | yes/no/no/no |
| Raw source claim | {"AccessType": "RW", "DataType": "0x0007", "DefaultValue": "0x60610008", "ObjFlags": "0x00000000", "ObjectType": "7", "PDOMapping": "0", "ParameterName": "Object 1A00_sub3"} |

### 0x1A00:04

| Fact | Reviewed/source value |
| --- | --- |
| EDS name | Object 1A00_sub4 |
| Sources | EDS [1A00sub4] lines 1072-1079 |
| Applicability | eds_family_unbound |
| Declaration disposition | resolved |
| Documentary disposition | pending_qualification |
| Exclusion reason | `null` (not stated) |
| Units | mapping descriptor |
| Scaling | Raw protocol field; no invented device timing or flags. |
| Original low / high / default | [null, null, null] |
| Effect | Changes PDO layout/route/timing; partial reconfiguration and delayed samples can remain ambiguous. |
| Prerequisites | ["Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.", "Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates."] |
| Edge behavior | Not specified by the retained source. |
| Documentary review | {"gate_ids": ["Q01", "Q17", "Q20"], "generation_type": "resolved", "read_effect": "not_established", "status": "complete", "type_policy": "Retain exact EDS wire width and signedness; no conversion to another profile type.", "value_policy": "U32 descriptor encodes index/subindex/bit width; at most four slots and eight Classical CAN payload bytes.", "write_policy": "qualification_required"} |
| Conflicts | [] |
| Owner prompt | 18 |
| Semantic coverage D/I/H/Q | yes/no/no/no |
| Raw source claim | {"AccessType": "RW", "DataType": "0x0007", "ObjFlags": "0x00000000", "ObjectType": "7", "PDOMapping": "0", "ParameterName": "Object 1A00_sub4"} |

### 0x1A01:00

| Fact | Reviewed/source value |
| --- | --- |
| EDS name | HighestSubIndex |
| Sources | EDS [1A01sub0] lines 1086-1096 |
| Applicability | eds_family_unbound |
| Declaration disposition | resolved |
| Documentary disposition | pending_qualification |
| Exclusion reason | `null` (not stated) |
| Units | entry count |
| Scaling | Integer record count; retain this entry's own EDS width. |
| Original low / high / default | ["0", "4", "2"] |
| Effect | Changes PDO layout/route/timing; partial reconfiguration and delayed samples can remain ambiguous. |
| Prerequisites | ["Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.", "Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates."] |
| Edge behavior | Not specified by the retained source. |
| Documentary review | {"gate_ids": ["Q01", "Q17", "Q20"], "generation_type": "resolved", "read_effect": "not_established", "status": "complete", "type_policy": "Retain exact EDS wire width and signedness; no conversion to another profile type.", "value_policy": "Count 0..4; only that many leading slots active. No use of inactive or missing-default slots.", "write_policy": "qualification_required"} |
| Conflicts | [] |
| Owner prompt | 18 |
| Semantic coverage D/I/H/Q | yes/no/no/no |
| Raw source claim | {"AccessType": "RW", "DataType": "0x0005", "DefaultValue": "2", "HighLimit": "4", "LowLimit": "0", "ObjFlags": "0x00000000", "ObjectType": "7", "PDOMapping": "0", "ParameterName": "HighestSubIndex"} |

### 0x1A01:01

| Fact | Reviewed/source value |
| --- | --- |
| EDS name | Object 1A01_sub1 |
| Sources | EDS [1A01sub1] lines 1097-1105 |
| Applicability | eds_family_unbound |
| Declaration disposition | resolved |
| Documentary disposition | pending_qualification |
| Exclusion reason | `null` (not stated) |
| Units | mapping descriptor |
| Scaling | Raw protocol field; no invented device timing or flags. |
| Original low / high / default | [null, null, "0x60640020"] |
| Effect | Changes PDO layout/route/timing; partial reconfiguration and delayed samples can remain ambiguous. |
| Prerequisites | ["Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.", "Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates."] |
| Edge behavior | Not specified by the retained source. |
| Documentary review | {"gate_ids": ["Q01", "Q17", "Q20"], "generation_type": "resolved", "read_effect": "not_established", "status": "complete", "type_policy": "Retain exact EDS wire width and signedness; no conversion to another profile type.", "value_policy": "U32 descriptor encodes index/subindex/bit width; at most four slots and eight Classical CAN payload bytes.", "write_policy": "qualification_required"} |
| Conflicts | [] |
| Owner prompt | 18 |
| Semantic coverage D/I/H/Q | yes/no/no/no |
| Raw source claim | {"AccessType": "RW", "DataType": "0x0007", "DefaultValue": "0x60640020", "ObjFlags": "0x00000000", "ObjectType": "7", "PDOMapping": "0", "ParameterName": "Object 1A01_sub1"} |

### 0x1A01:02

| Fact | Reviewed/source value |
| --- | --- |
| EDS name | Object 1A01_sub2 |
| Sources | EDS [1A01sub2] lines 1106-1114 |
| Applicability | eds_family_unbound |
| Declaration disposition | resolved |
| Documentary disposition | pending_qualification |
| Exclusion reason | `null` (not stated) |
| Units | mapping descriptor |
| Scaling | Raw protocol field; no invented device timing or flags. |
| Original low / high / default | [null, null, "0x606c0020"] |
| Effect | Changes PDO layout/route/timing; partial reconfiguration and delayed samples can remain ambiguous. |
| Prerequisites | ["Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.", "Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates."] |
| Edge behavior | Not specified by the retained source. |
| Documentary review | {"gate_ids": ["Q01", "Q17", "Q20"], "generation_type": "resolved", "read_effect": "not_established", "status": "complete", "type_policy": "Retain exact EDS wire width and signedness; no conversion to another profile type.", "value_policy": "U32 descriptor encodes index/subindex/bit width; at most four slots and eight Classical CAN payload bytes.", "write_policy": "qualification_required"} |
| Conflicts | [] |
| Owner prompt | 18 |
| Semantic coverage D/I/H/Q | yes/no/no/no |
| Raw source claim | {"AccessType": "RW", "DataType": "0x0007", "DefaultValue": "0x606c0020", "ObjFlags": "0x00000000", "ObjectType": "7", "PDOMapping": "0", "ParameterName": "Object 1A01_sub2"} |

### 0x1A01:03

| Fact | Reviewed/source value |
| --- | --- |
| EDS name | Object 1A01_sub3 |
| Sources | EDS [1A01sub3] lines 1115-1122 |
| Applicability | eds_family_unbound |
| Declaration disposition | resolved |
| Documentary disposition | pending_qualification |
| Exclusion reason | `null` (not stated) |
| Units | mapping descriptor |
| Scaling | Raw protocol field; no invented device timing or flags. |
| Original low / high / default | [null, null, null] |
| Effect | Changes PDO layout/route/timing; partial reconfiguration and delayed samples can remain ambiguous. |
| Prerequisites | ["Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.", "Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates."] |
| Edge behavior | Not specified by the retained source. |
| Documentary review | {"gate_ids": ["Q01", "Q17", "Q20"], "generation_type": "resolved", "read_effect": "not_established", "status": "complete", "type_policy": "Retain exact EDS wire width and signedness; no conversion to another profile type.", "value_policy": "U32 descriptor encodes index/subindex/bit width; at most four slots and eight Classical CAN payload bytes.", "write_policy": "qualification_required"} |
| Conflicts | [] |
| Owner prompt | 18 |
| Semantic coverage D/I/H/Q | yes/no/no/no |
| Raw source claim | {"AccessType": "RW", "DataType": "0x0007", "ObjFlags": "0x00000000", "ObjectType": "7", "PDOMapping": "0", "ParameterName": "Object 1A01_sub3"} |

### 0x1A01:04

| Fact | Reviewed/source value |
| --- | --- |
| EDS name | Object 1A01_sub4 |
| Sources | EDS [1A01sub4] lines 1123-1130 |
| Applicability | eds_family_unbound |
| Declaration disposition | resolved |
| Documentary disposition | pending_qualification |
| Exclusion reason | `null` (not stated) |
| Units | mapping descriptor |
| Scaling | Raw protocol field; no invented device timing or flags. |
| Original low / high / default | [null, null, null] |
| Effect | Changes PDO layout/route/timing; partial reconfiguration and delayed samples can remain ambiguous. |
| Prerequisites | ["Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.", "Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates."] |
| Edge behavior | Not specified by the retained source. |
| Documentary review | {"gate_ids": ["Q01", "Q17", "Q20"], "generation_type": "resolved", "read_effect": "not_established", "status": "complete", "type_policy": "Retain exact EDS wire width and signedness; no conversion to another profile type.", "value_policy": "U32 descriptor encodes index/subindex/bit width; at most four slots and eight Classical CAN payload bytes.", "write_policy": "qualification_required"} |
| Conflicts | [] |
| Owner prompt | 18 |
| Semantic coverage D/I/H/Q | yes/no/no/no |
| Raw source claim | {"AccessType": "RW", "DataType": "0x0007", "ObjFlags": "0x00000000", "ObjectType": "7", "PDOMapping": "0", "ParameterName": "Object 1A01_sub4"} |

### 0x1A02:00

| Fact | Reviewed/source value |
| --- | --- |
| EDS name | HighestSubIndex |
| Sources | EDS [1A02sub0] lines 1137-1147 |
| Applicability | eds_family_unbound |
| Declaration disposition | resolved |
| Documentary disposition | pending_qualification |
| Exclusion reason | `null` (not stated) |
| Units | entry count |
| Scaling | Integer record count; retain this entry's own EDS width. |
| Original low / high / default | ["0", "4", "0"] |
| Effect | Changes PDO layout/route/timing; partial reconfiguration and delayed samples can remain ambiguous. |
| Prerequisites | ["Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.", "Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates."] |
| Edge behavior | Not specified by the retained source. |
| Documentary review | {"gate_ids": ["Q01", "Q17", "Q20"], "generation_type": "resolved", "read_effect": "not_established", "status": "complete", "type_policy": "Retain exact EDS wire width and signedness; no conversion to another profile type.", "value_policy": "Count 0..4; only that many leading slots active. No use of inactive or missing-default slots.", "write_policy": "qualification_required"} |
| Conflicts | [] |
| Owner prompt | 18 |
| Semantic coverage D/I/H/Q | yes/no/no/no |
| Raw source claim | {"AccessType": "RW", "DataType": "0x0005", "DefaultValue": "0", "HighLimit": "4", "LowLimit": "0", "ObjFlags": "0x00000000", "ObjectType": "7", "PDOMapping": "0", "ParameterName": "HighestSubIndex"} |

### 0x1A02:01

| Fact | Reviewed/source value |
| --- | --- |
| EDS name | Object 1A02_sub1 |
| Sources | EDS [1A02sub1] lines 1148-1155 |
| Applicability | eds_family_unbound |
| Declaration disposition | resolved |
| Documentary disposition | pending_qualification |
| Exclusion reason | `null` (not stated) |
| Units | mapping descriptor |
| Scaling | Raw protocol field; no invented device timing or flags. |
| Original low / high / default | [null, null, null] |
| Effect | Changes PDO layout/route/timing; partial reconfiguration and delayed samples can remain ambiguous. |
| Prerequisites | ["Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.", "Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates."] |
| Edge behavior | Not specified by the retained source. |
| Documentary review | {"gate_ids": ["Q01", "Q17", "Q20"], "generation_type": "resolved", "read_effect": "not_established", "status": "complete", "type_policy": "Retain exact EDS wire width and signedness; no conversion to another profile type.", "value_policy": "U32 descriptor encodes index/subindex/bit width; at most four slots and eight Classical CAN payload bytes.", "write_policy": "qualification_required"} |
| Conflicts | [] |
| Owner prompt | 18 |
| Semantic coverage D/I/H/Q | yes/no/no/no |
| Raw source claim | {"AccessType": "RW", "DataType": "0x0007", "ObjFlags": "0x00000000", "ObjectType": "7", "PDOMapping": "0", "ParameterName": "Object 1A02_sub1"} |

### 0x1A02:02

| Fact | Reviewed/source value |
| --- | --- |
| EDS name | Object 1A02_sub2 |
| Sources | EDS [1A02sub2] lines 1156-1163 |
| Applicability | eds_family_unbound |
| Declaration disposition | resolved |
| Documentary disposition | pending_qualification |
| Exclusion reason | `null` (not stated) |
| Units | mapping descriptor |
| Scaling | Raw protocol field; no invented device timing or flags. |
| Original low / high / default | [null, null, null] |
| Effect | Changes PDO layout/route/timing; partial reconfiguration and delayed samples can remain ambiguous. |
| Prerequisites | ["Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.", "Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates."] |
| Edge behavior | Not specified by the retained source. |
| Documentary review | {"gate_ids": ["Q01", "Q17", "Q20"], "generation_type": "resolved", "read_effect": "not_established", "status": "complete", "type_policy": "Retain exact EDS wire width and signedness; no conversion to another profile type.", "value_policy": "U32 descriptor encodes index/subindex/bit width; at most four slots and eight Classical CAN payload bytes.", "write_policy": "qualification_required"} |
| Conflicts | [] |
| Owner prompt | 18 |
| Semantic coverage D/I/H/Q | yes/no/no/no |
| Raw source claim | {"AccessType": "RW", "DataType": "0x0007", "ObjFlags": "0x00000000", "ObjectType": "7", "PDOMapping": "0", "ParameterName": "Object 1A02_sub2"} |

### 0x1A02:03

| Fact | Reviewed/source value |
| --- | --- |
| EDS name | Object 1A02_sub3 |
| Sources | EDS [1A02sub3] lines 1164-1171 |
| Applicability | eds_family_unbound |
| Declaration disposition | resolved |
| Documentary disposition | pending_qualification |
| Exclusion reason | `null` (not stated) |
| Units | mapping descriptor |
| Scaling | Raw protocol field; no invented device timing or flags. |
| Original low / high / default | [null, null, null] |
| Effect | Changes PDO layout/route/timing; partial reconfiguration and delayed samples can remain ambiguous. |
| Prerequisites | ["Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.", "Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates."] |
| Edge behavior | Not specified by the retained source. |
| Documentary review | {"gate_ids": ["Q01", "Q17", "Q20"], "generation_type": "resolved", "read_effect": "not_established", "status": "complete", "type_policy": "Retain exact EDS wire width and signedness; no conversion to another profile type.", "value_policy": "U32 descriptor encodes index/subindex/bit width; at most four slots and eight Classical CAN payload bytes.", "write_policy": "qualification_required"} |
| Conflicts | [] |
| Owner prompt | 18 |
| Semantic coverage D/I/H/Q | yes/no/no/no |
| Raw source claim | {"AccessType": "RW", "DataType": "0x0007", "ObjFlags": "0x00000000", "ObjectType": "7", "PDOMapping": "0", "ParameterName": "Object 1A02_sub3"} |

### 0x1A02:04

| Fact | Reviewed/source value |
| --- | --- |
| EDS name | Object 1A02_sub4 |
| Sources | EDS [1A02sub4] lines 1172-1179 |
| Applicability | eds_family_unbound |
| Declaration disposition | resolved |
| Documentary disposition | pending_qualification |
| Exclusion reason | `null` (not stated) |
| Units | mapping descriptor |
| Scaling | Raw protocol field; no invented device timing or flags. |
| Original low / high / default | [null, null, null] |
| Effect | Changes PDO layout/route/timing; partial reconfiguration and delayed samples can remain ambiguous. |
| Prerequisites | ["Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.", "Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates."] |
| Edge behavior | Not specified by the retained source. |
| Documentary review | {"gate_ids": ["Q01", "Q17", "Q20"], "generation_type": "resolved", "read_effect": "not_established", "status": "complete", "type_policy": "Retain exact EDS wire width and signedness; no conversion to another profile type.", "value_policy": "U32 descriptor encodes index/subindex/bit width; at most four slots and eight Classical CAN payload bytes.", "write_policy": "qualification_required"} |
| Conflicts | [] |
| Owner prompt | 18 |
| Semantic coverage D/I/H/Q | yes/no/no/no |
| Raw source claim | {"AccessType": "RW", "DataType": "0x0007", "ObjFlags": "0x00000000", "ObjectType": "7", "PDOMapping": "0", "ParameterName": "Object 1A02_sub4"} |

### 0x1A03:00

| Fact | Reviewed/source value |
| --- | --- |
| EDS name | HighestSubIndex |
| Sources | EDS [1A03sub0] lines 1186-1196 |
| Applicability | eds_family_unbound |
| Declaration disposition | resolved |
| Documentary disposition | pending_qualification |
| Exclusion reason | `null` (not stated) |
| Units | entry count |
| Scaling | Integer record count; retain this entry's own EDS width. |
| Original low / high / default | ["0", "4", "0"] |
| Effect | Changes PDO layout/route/timing; partial reconfiguration and delayed samples can remain ambiguous. |
| Prerequisites | ["Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.", "Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates."] |
| Edge behavior | Not specified by the retained source. |
| Documentary review | {"gate_ids": ["Q01", "Q17", "Q20"], "generation_type": "resolved", "read_effect": "not_established", "status": "complete", "type_policy": "Retain exact EDS wire width and signedness; no conversion to another profile type.", "value_policy": "Count 0..4; only that many leading slots active. No use of inactive or missing-default slots.", "write_policy": "qualification_required"} |
| Conflicts | [] |
| Owner prompt | 18 |
| Semantic coverage D/I/H/Q | yes/no/no/no |
| Raw source claim | {"AccessType": "RW", "DataType": "0x0005", "DefaultValue": "0", "HighLimit": "4", "LowLimit": "0", "ObjFlags": "0x00000000", "ObjectType": "7", "PDOMapping": "0", "ParameterName": "HighestSubIndex"} |

### 0x1A03:01

| Fact | Reviewed/source value |
| --- | --- |
| EDS name | Object 1A03_sub1 |
| Sources | EDS [1A03sub1] lines 1197-1204 |
| Applicability | eds_family_unbound |
| Declaration disposition | resolved |
| Documentary disposition | pending_qualification |
| Exclusion reason | `null` (not stated) |
| Units | mapping descriptor |
| Scaling | Raw protocol field; no invented device timing or flags. |
| Original low / high / default | [null, null, null] |
| Effect | Changes PDO layout/route/timing; partial reconfiguration and delayed samples can remain ambiguous. |
| Prerequisites | ["Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.", "Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates."] |
| Edge behavior | Not specified by the retained source. |
| Documentary review | {"gate_ids": ["Q01", "Q17", "Q20"], "generation_type": "resolved", "read_effect": "not_established", "status": "complete", "type_policy": "Retain exact EDS wire width and signedness; no conversion to another profile type.", "value_policy": "U32 descriptor encodes index/subindex/bit width; at most four slots and eight Classical CAN payload bytes.", "write_policy": "qualification_required"} |
| Conflicts | [] |
| Owner prompt | 18 |
| Semantic coverage D/I/H/Q | yes/no/no/no |
| Raw source claim | {"AccessType": "RW", "DataType": "0x0007", "ObjFlags": "0x00000000", "ObjectType": "7", "PDOMapping": "0", "ParameterName": "Object 1A03_sub1"} |

### 0x1A03:02

| Fact | Reviewed/source value |
| --- | --- |
| EDS name | Object 1A03_sub2 |
| Sources | EDS [1A03sub2] lines 1205-1212 |
| Applicability | eds_family_unbound |
| Declaration disposition | resolved |
| Documentary disposition | pending_qualification |
| Exclusion reason | `null` (not stated) |
| Units | mapping descriptor |
| Scaling | Raw protocol field; no invented device timing or flags. |
| Original low / high / default | [null, null, null] |
| Effect | Changes PDO layout/route/timing; partial reconfiguration and delayed samples can remain ambiguous. |
| Prerequisites | ["Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.", "Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates."] |
| Edge behavior | Not specified by the retained source. |
| Documentary review | {"gate_ids": ["Q01", "Q17", "Q20"], "generation_type": "resolved", "read_effect": "not_established", "status": "complete", "type_policy": "Retain exact EDS wire width and signedness; no conversion to another profile type.", "value_policy": "U32 descriptor encodes index/subindex/bit width; at most four slots and eight Classical CAN payload bytes.", "write_policy": "qualification_required"} |
| Conflicts | [] |
| Owner prompt | 18 |
| Semantic coverage D/I/H/Q | yes/no/no/no |
| Raw source claim | {"AccessType": "RW", "DataType": "0x0007", "ObjFlags": "0x00000000", "ObjectType": "7", "PDOMapping": "0", "ParameterName": "Object 1A03_sub2"} |

### 0x1A03:03

| Fact | Reviewed/source value |
| --- | --- |
| EDS name | Object 1A03_sub3 |
| Sources | EDS [1A03sub3] lines 1213-1220 |
| Applicability | eds_family_unbound |
| Declaration disposition | resolved |
| Documentary disposition | pending_qualification |
| Exclusion reason | `null` (not stated) |
| Units | mapping descriptor |
| Scaling | Raw protocol field; no invented device timing or flags. |
| Original low / high / default | [null, null, null] |
| Effect | Changes PDO layout/route/timing; partial reconfiguration and delayed samples can remain ambiguous. |
| Prerequisites | ["Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.", "Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates."] |
| Edge behavior | Not specified by the retained source. |
| Documentary review | {"gate_ids": ["Q01", "Q17", "Q20"], "generation_type": "resolved", "read_effect": "not_established", "status": "complete", "type_policy": "Retain exact EDS wire width and signedness; no conversion to another profile type.", "value_policy": "U32 descriptor encodes index/subindex/bit width; at most four slots and eight Classical CAN payload bytes.", "write_policy": "qualification_required"} |
| Conflicts | [] |
| Owner prompt | 18 |
| Semantic coverage D/I/H/Q | yes/no/no/no |
| Raw source claim | {"AccessType": "RW", "DataType": "0x0007", "ObjFlags": "0x00000000", "ObjectType": "7", "PDOMapping": "0", "ParameterName": "Object 1A03_sub3"} |

### 0x1A03:04

| Fact | Reviewed/source value |
| --- | --- |
| EDS name | Object 1A03_sub4 |
| Sources | EDS [1A03sub4] lines 1221-1228 |
| Applicability | eds_family_unbound |
| Declaration disposition | resolved |
| Documentary disposition | pending_qualification |
| Exclusion reason | `null` (not stated) |
| Units | mapping descriptor |
| Scaling | Raw protocol field; no invented device timing or flags. |
| Original low / high / default | [null, null, null] |
| Effect | Changes PDO layout/route/timing; partial reconfiguration and delayed samples can remain ambiguous. |
| Prerequisites | ["Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.", "Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates."] |
| Edge behavior | Not specified by the retained source. |
| Documentary review | {"gate_ids": ["Q01", "Q17", "Q20"], "generation_type": "resolved", "read_effect": "not_established", "status": "complete", "type_policy": "Retain exact EDS wire width and signedness; no conversion to another profile type.", "value_policy": "U32 descriptor encodes index/subindex/bit width; at most four slots and eight Classical CAN payload bytes.", "write_policy": "qualification_required"} |
| Conflicts | [] |
| Owner prompt | 18 |
| Semantic coverage D/I/H/Q | yes/no/no/no |
| Raw source claim | {"AccessType": "RW", "DataType": "0x0007", "ObjFlags": "0x00000000", "ObjectType": "7", "PDOMapping": "0", "ParameterName": "Object 1A03_sub4"} |

### 0x2000:00

| Fact | Reviewed/source value |
| --- | --- |
| EDS name | Obj.driverID |
| Sources | EDS [2000] lines 1229-1237; EN86 PDF page 17; ZH86 PDF page 18 |
| Applicability | eds_family_unbound |
| Declaration disposition | resolved |
| Documentary disposition | supported_evidence |
| Exclusion reason | `null` (not stated) |
| Units | raw identity code |
| Scaling | No exact-model/firmware auto-detection from generic values. |
| Original low / high / default | [null, null, "1"] |
| Effect | Observe identity metadata; not configuration selection. |
| Prerequisites | ["Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.", "Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates."] |
| Edge behavior | Not specified by the retained source. |
| Documentary review | {"gate_ids": ["Q01", "Q20"], "generation_type": "resolved", "read_effect": "not_established", "status": "complete", "type_policy": "Retain exact EDS wire width and signedness; no conversion to another profile type.", "value_policy": "EDS values remain metadata. 1009/100A are U16, not strings; 1018 has only children 00..03.", "write_policy": "not_writable"} |
| Conflicts | [] |
| Owner prompt | 19 |
| Semantic coverage D/I/H/Q | yes/no/no/no |
| Raw source claim | {"AccessType": "RO", "DataType": "0x0006", "DefaultValue": "1", "ObjFlags": "0x00000000", "ObjectType": "7", "PDOMapping": "1", "ParameterName": "Obj.driverID"} |

### 0x2001:00

| Fact | Reviewed/source value |
| --- | --- |
| EDS name | Obj.softversion |
| Sources | EDS [2001] lines 1238-1246; EN86 PDF page 17; ZH86 PDF page 18 |
| Applicability | eds_family_unbound |
| Declaration disposition | resolved |
| Documentary disposition | supported_evidence |
| Exclusion reason | `null` (not stated) |
| Units | raw identity code |
| Scaling | No exact-model/firmware auto-detection from generic values. |
| Original low / high / default | [null, null, "0"] |
| Effect | Observe identity metadata; not configuration selection. |
| Prerequisites | ["Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.", "Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates."] |
| Edge behavior | Not specified by the retained source. |
| Documentary review | {"gate_ids": ["Q01", "Q20"], "generation_type": "resolved", "read_effect": "not_established", "status": "complete", "type_policy": "Retain exact EDS wire width and signedness; no conversion to another profile type.", "value_policy": "EDS values remain metadata. 1009/100A are U16, not strings; 1018 has only children 00..03.", "write_policy": "not_writable"} |
| Conflicts | [] |
| Owner prompt | 19 |
| Semantic coverage D/I/H/Q | yes/no/no/no |
| Raw source claim | {"AccessType": "RO", "DataType": "0x0006", "DefaultValue": "0", "ObjFlags": "0x00000000", "ObjectType": "7", "PDOMapping": "1", "ParameterName": "Obj.softversion"} |

### 0x2100:00

| Fact | Reviewed/source value |
| --- | --- |
| EDS name | Obj.MonitorWord |
| Sources | EDS [2100] lines 1247-1255; EN86 PDF page 17; ZH86 PDF page 18 |
| Applicability | eds_family_unbound |
| Declaration disposition | resolved |
| Documentary disposition | conservative_read_only |
| Exclusion reason | `null` (not stated) |
| Units | bit field |
| Scaling | Native monitor bits; no CiA402 mask substitution. |
| Original low / high / default | [null, null, "8"] |
| Effect | Read monitor only despite EDS RW. |
| Prerequisites | ["Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.", "Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates."] |
| Edge behavior | Not specified by the retained source. |
| Documentary review | {"gate_ids": ["Q01", "Q02", "Q20"], "generation_type": "resolved", "read_effect": "not_established", "status": "complete", "type_policy": "Retain exact EDS wire width and signedness; no conversion to another profile type.", "value_policy": "Bits 0 in-position, 1 homed, 2 running, 3 alarm, 4 released (0 enabled, 1 released), 5 positive and 6 negative software limit.", "write_policy": "not_writable"} |
| Conflicts | ["C06"] |
| Owner prompt | 19 |
| Semantic coverage D/I/H/Q | yes/no/no/no |
| Raw source claim | {"AccessType": "RW", "DataType": "0x0006", "DefaultValue": "8", "ObjFlags": "0x00000000", "ObjectType": "7", "PDOMapping": "1", "ParameterName": "Obj.MonitorWord"} |

### 0x2101:00

| Fact | Reviewed/source value |
| --- | --- |
| EDS name | Obj.DiStatus |
| Sources | EDS [2101] lines 1256-1264; EN86 PDF page 17; ZH86 PDF page 18 |
| Applicability | eds_family_unbound |
| Declaration disposition | resolved |
| Documentary disposition | conservative_read_only |
| Exclusion reason | `null` (not stated) |
| Units | bit field |
| Scaling | Native monitor bits; no CiA402 mask substitution. |
| Original low / high / default | [null, null, "0"] |
| Effect | Read monitor only despite EDS RW. |
| Prerequisites | ["Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.", "Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates."] |
| Edge behavior | Not specified by the retained source. |
| Documentary review | {"gate_ids": ["Q01", "Q20"], "generation_type": "resolved", "read_effect": "not_established", "status": "complete", "type_policy": "Retain exact EDS wire width and signedness; no conversion to another profile type.", "value_policy": "Bits 0..6 logical DI0..6 active state; not physical voltage.", "write_policy": "not_writable"} |
| Conflicts | ["C06"] |
| Owner prompt | 19 |
| Semantic coverage D/I/H/Q | yes/no/no/no |
| Raw source claim | {"AccessType": "RW", "DataType": "0x0006", "DefaultValue": "0", "ObjFlags": "0x00000000", "ObjectType": "7", "PDOMapping": "1", "ParameterName": "Obj.DiStatus"} |

### 0x2102:00

| Fact | Reviewed/source value |
| --- | --- |
| EDS name | Obj.DoStatus |
| Sources | EDS [2102] lines 1265-1273; EN86 PDF page 17; ZH86 PDF page 18 |
| Applicability | eds_family_unbound |
| Declaration disposition | resolved |
| Documentary disposition | conservative_read_only |
| Exclusion reason | `null` (not stated) |
| Units | bit field |
| Scaling | Native monitor bits; no CiA402 mask substitution. |
| Original low / high / default | [null, null, "0"] |
| Effect | Read monitor only despite EDS RW. |
| Prerequisites | ["Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.", "Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates."] |
| Edge behavior | Not specified by the retained source. |
| Documentary review | {"gate_ids": ["Q01", "Q20"], "generation_type": "resolved", "read_effect": "not_established", "status": "complete", "type_policy": "Retain exact EDS wire width and signedness; no conversion to another profile type.", "value_policy": "Bits 0..2 logical DO0..2 active state; not 60FE bits16..18.", "write_policy": "not_writable"} |
| Conflicts | ["C06"] |
| Owner prompt | 19 |
| Semantic coverage D/I/H/Q | yes/no/no/no |
| Raw source claim | {"AccessType": "RW", "DataType": "0x0006", "DefaultValue": "0", "ObjFlags": "0x00000000", "ObjectType": "7", "PDOMapping": "1", "ParameterName": "Obj.DoStatus"} |

### 0x2200:00

| Fact | Reviewed/source value |
| --- | --- |
| EDS name | Obj.DefautDir |
| Sources | EDS [2200] lines 1274-1282; EN86 PDF page 18; ZH86 PDF page 18 |
| Applicability | eds_family_unbound |
| Declaration disposition | resolved |
| Documentary disposition | pending_qualification |
| Exclusion reason | `null` (not stated) |
| Units | direction selector |
| Scaling | Physical positive direction not established. |
| Original low / high / default | [null, null, "0"] |
| Effect | Named default direction; actual effect on CAN motion is not established by the retained labels. |
| Prerequisites | ["Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.", "Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates."] |
| Edge behavior | Not specified by the retained source. |
| Documentary review | {"gate_ids": ["Q01", "Q04", "Q20"], "generation_type": "resolved", "read_effect": "not_established", "status": "complete", "type_policy": "Retain exact EDS wire width and signedness; no conversion to another profile type.", "value_policy": "Manual labels 0 Pulse Sign and 1 Pulse /Sign; retain raw values until actual direction is qualified.", "write_policy": "qualification_required"} |
| Conflicts | [] |
| Owner prompt | 20 |
| Semantic coverage D/I/H/Q | yes/no/no/no |
| Raw source claim | {"AccessType": "RW", "DataType": "0x0006", "DefaultValue": "0", "ObjFlags": "0x00000000", "ObjectType": "7", "PDOMapping": "1", "ParameterName": "Obj.DefautDir"} |

### 0x2201:00

| Fact | Reviewed/source value |
| --- | --- |
| EDS name | Obj.MicDivide |
| Sources | EDS [2201] lines 1283-1291; EN86 PDF page 18; ZH86 PDF page 18; EN86 PDF page 11 |
| Applicability | eds_family_unbound |
| Declaration disposition | resolved |
| Documentary disposition | pending_qualification |
| Exclusion reason | `null` (not stated) |
| Units | subdivision |
| Scaling | Command/feedback/encoder scales remain separate. |
| Original low / high / default | [null, null, "0"] |
| Effect | Changes command subdivision and invalidates dependent coordinate evidence. |
| Prerequisites | ["Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.", "Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates."] |
| Edge behavior | Not specified by the retained source. |
| Documentary review | {"gate_ids": ["Q01", "Q04", "Q20"], "generation_type": "resolved", "read_effect": "not_established", "status": "complete", "type_policy": "Retain exact EDS wire width and signedness; no conversion to another profile type.", "value_policy": "EDS default 0; manual summary 0..65535 versus detailed 400..51200. Do not choose a union or initialize from zero.", "write_policy": "qualification_required"} |
| Conflicts | ["C12"] |
| Owner prompt | 20 |
| Semantic coverage D/I/H/Q | yes/no/no/no |
| Raw source claim | {"AccessType": "RW", "DataType": "0x0006", "DefaultValue": "0", "ObjFlags": "0x00000000", "ObjectType": "7", "PDOMapping": "1", "ParameterName": "Obj.MicDivide"} |

### 0x2202:00

| Fact | Reviewed/source value |
| --- | --- |
| EDS name | Obj.Soft_Limit |
| Sources | EDS [2202] lines 1292-1300; EN86 PDF page 18; ZH86 PDF page 18 |
| Applicability | eds_family_unbound |
| Declaration disposition | resolved |
| Documentary disposition | pending_qualification |
| Exclusion reason | `null` (not stated) |
| Units | enable selector |
| Scaling | Native enumerated choice. |
| Original low / high / default | [null, null, "10"] |
| Effect | Changes software-limit enforcement. |
| Prerequisites | ["Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.", "Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates."] |
| Edge behavior | Not specified by the retained source. |
| Documentary review | {"gate_ids": ["Q01", "Q05", "Q20"], "generation_type": "resolved", "read_effect": "not_established", "status": "complete", "type_policy": "Retain exact EDS wire width and signedness; no conversion to another profile type.", "value_policy": "Manual 0 disabled / 1 effective after homing; EDS default 10 is not an accepted option.", "write_policy": "qualification_required"} |
| Conflicts | ["C13"] |
| Owner prompt | 20 |
| Semantic coverage D/I/H/Q | yes/no/no/no |
| Raw source claim | {"AccessType": "RW", "DataType": "0x0006", "DefaultValue": "10", "ObjFlags": "0x00000000", "ObjectType": "7", "PDOMapping": "1", "ParameterName": "Obj.Soft_Limit"} |

### 0x2203:00

| Fact | Reviewed/source value |
| --- | --- |
| EDS name | Obj.CAN_ID |
| Sources | EDS [2203] lines 1301-1309; EN86 PDF page 18; ZH86 PDF page 18 |
| Applicability | eds_family_unbound |
| Declaration disposition | resolved |
| Documentary disposition | pending_qualification |
| Exclusion reason | `null` (not stated) |
| Units | node selector |
| Scaling | Native U16; CANopen unicast node remains 1..127. |
| Original low / high / default | [null, null, "0"] |
| Effect | Changes software node setting; route/activation/persistence unknown. |
| Prerequisites | ["Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.", "Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates."] |
| Edge behavior | Not specified by the retained source. |
| Documentary review | {"gate_ids": ["Q01", "Q16", "Q20"], "generation_type": "resolved", "read_effect": "not_established", "status": "complete", "type_policy": "Retain exact EDS wire width and signedness; no conversion to another profile type.", "value_policy": "Manual 0..127 includes zero; all-off DIP selects user parameter, not a unicast node zero.", "write_policy": "qualification_required"} |
| Conflicts | [] |
| Owner prompt | 38 |
| Semantic coverage D/I/H/Q | yes/no/no/no |
| Raw source claim | {"AccessType": "RW", "DataType": "0x0006", "DefaultValue": "0", "ObjFlags": "0x00000000", "ObjectType": "7", "PDOMapping": "1", "ParameterName": "Obj.CAN_ID"} |

### 0x2300:00

| Fact | Reviewed/source value |
| --- | --- |
| EDS name | Obj.FactoryPara |
| Sources | EDS [2300] lines 1310-1318; EN86 PDF page 15; EN86 PDF page 18; ZH86 PDF page 15; ZH86 PDF page 18 |
| Applicability | eds_family_unbound |
| Declaration disposition | resolved |
| Documentary disposition | pending_qualification |
| Exclusion reason | `null` (not stated) |
| Units | action code |
| Scaling | Not a harmless setting. |
| Original low / high / default | [null, null, "0"] |
| Effect | Save parameters or restore factory parameters. |
| Prerequisites | ["Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.", "Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates."] |
| Edge behavior | Trigger/rearm and completion timing not established; do not infer a rising edge. |
| Documentary review | {"gate_ids": ["Q01", "Q15", "Q20"], "generation_type": "resolved", "read_effect": "not_established", "status": "complete", "type_policy": "Retain exact EDS wire width and signedness; no conversion to another profile type.", "value_policy": "0 ineffective; 1 factory reset; 2 save.", "write_policy": "qualification_required"} |
| Conflicts | [] |
| Owner prompt | 39 |
| Semantic coverage D/I/H/Q | yes/no/no/no |
| Raw source claim | {"AccessType": "RW", "DataType": "0x0006", "DefaultValue": "0", "ObjFlags": "0x00000000", "ObjectType": "7", "PDOMapping": "1", "ParameterName": "Obj.FactoryPara"} |

### 0x2301:00

| Fact | Reviewed/source value |
| --- | --- |
| EDS name | Obj.ClearAlm |
| Sources | EDS [2301] lines 1319-1327; EN86 PDF page 18 |
| Applicability | eds_family_unbound |
| Declaration disposition | resolved |
| Documentary disposition | pending_qualification |
| Exclusion reason | `null` (not stated) |
| Units | action code |
| Scaling | Not a harmless setting. |
| Original low / high / default | [null, null, "0"] |
| Effect | Explicit alarm-clear action, not equivalent to controlword reset without evidence. |
| Prerequisites | ["Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.", "Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates."] |
| Edge behavior | Trigger/rearm and completion timing not established; do not infer a rising edge. |
| Documentary review | {"gate_ids": ["Q01", "Q13", "Q20"], "generation_type": "resolved", "read_effect": "not_established", "status": "complete", "type_policy": "Retain exact EDS wire width and signedness; no conversion to another profile type.", "value_policy": "0 ineffective; 1 effective in EN86; ZH86 omits the entry.", "write_policy": "qualification_required"} |
| Conflicts | [] |
| Owner prompt | 32 |
| Semantic coverage D/I/H/Q | yes/no/no/no |
| Raw source claim | {"AccessType": "RW", "DataType": "0x0006", "DefaultValue": "0", "ObjFlags": "0x00000000", "ObjectType": "7", "PDOMapping": "1", "ParameterName": "Obj.ClearAlm"} |

### 0x2302:00

| Fact | Reviewed/source value |
| --- | --- |
| EDS name | Obj.ClearPos |
| Sources | EDS [2302] lines 1328-1336; EN86 PDF page 15; EN86 PDF page 18; ZH86 PDF page 15; ZH86 PDF page 18 |
| Applicability | eds_family_unbound |
| Declaration disposition | resolved |
| Documentary disposition | pending_qualification |
| Exclusion reason | `null` (not stated) |
| Units | action code |
| Scaling | Not a harmless setting. |
| Original low / high / default | [null, null, "0"] |
| Effect | Clear current position to zero; invalidate dependent reference evidence. |
| Prerequisites | ["Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.", "Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates."] |
| Edge behavior | Rising 0 -&gt; 1, followed by explicit manual return to 0. |
| Documentary review | {"gate_ids": ["Q01", "Q14", "Q20"], "generation_type": "resolved", "read_effect": "not_established", "status": "complete", "type_policy": "Retain exact EDS wire width and signedness; no conversion to another profile type.", "value_policy": "0 to 1 clears current position; explicitly return to zero manually.", "write_policy": "qualification_required"} |
| Conflicts | [] |
| Owner prompt | 32 |
| Semantic coverage D/I/H/Q | yes/no/no/no |
| Raw source claim | {"AccessType": "RW", "DataType": "0x0006", "DefaultValue": "0", "ObjFlags": "0x00000000", "ObjectType": "7", "PDOMapping": "1", "ParameterName": "Obj.ClearPos"} |

### 0x2400:00

| Fact | Reviewed/source value |
| --- | --- |
| EDS name | HighestSubIndex |
| Sources | EDS [2400sub0] lines 1343-1351 |
| Applicability | eds_family_unbound |
| Declaration disposition | resolved |
| Documentary disposition | supported_evidence |
| Exclusion reason | `null` (not stated) |
| Units | entry count |
| Scaling | Integer record count; retain this entry's own EDS width. |
| Original low / high / default | [null, null, "16"] |
| Effect | Observe the declared entry count; not the physical semantics of its children. |
| Prerequisites | ["Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.", "Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates."] |
| Edge behavior | Not specified by the retained source. |
| Documentary review | {"gate_ids": ["Q01", "Q20"], "generation_type": "resolved", "read_effect": "not_established", "status": "complete", "type_policy": "Retain exact EDS wire width and signedness; no conversion to another profile type.", "value_policy": "Default 16; explicitly U16 in EDS.", "write_policy": "not_writable"} |
| Conflicts | [] |
| Owner prompt | 21 |
| Semantic coverage D/I/H/Q | yes/no/no/no |
| Raw source claim | {"AccessType": "RO", "DataType": "0x0006", "DefaultValue": "16", "ObjFlags": "0x00000000", "ObjectType": "7", "PDOMapping": "0", "ParameterName": "HighestSubIndex"} |

### 0x2400:01

| Fact | Reviewed/source value |
| --- | --- |
| EDS name | Obj.Dilevel |
| Sources | EDS [2400sub1] lines 1352-1360; EN86 PDF page 8; EN86 PDF page 18; ZH86 PDF page 8; ZH86 PDF page 18 |
| Applicability | eds_family_unbound |
| Declaration disposition | resolved |
| Documentary disposition | pending_qualification |
| Exclusion reason | `null` (not stated) |
| Units | polarity bit field |
| Scaling | Bits0..6 correspond to DI0..6. |
| Original low / high / default | [null, null, "0"] |
| Effect | Changes logical input polarity; protective signals may change meaning. |
| Prerequisites | ["Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.", "Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates."] |
| Edge behavior | Not specified by the retained source. |
| Documentary review | {"gate_ids": ["Q01", "Q19", "Q20"], "generation_type": "resolved", "read_effect": "not_established", "status": "complete", "type_policy": "Retain exact EDS wire width and signedness; no conversion to another profile type.", "value_policy": "Manual 0 normal / 1 inverted for each bit; other bits unspecified.", "write_policy": "qualification_required"} |
| Conflicts | [] |
| Owner prompt | 21 |
| Semantic coverage D/I/H/Q | yes/no/no/no |
| Raw source claim | {"AccessType": "RW", "DataType": "0x0006", "DefaultValue": "0", "ObjFlags": "0x00000000", "ObjectType": "7", "PDOMapping": "1", "ParameterName": "Obj.Dilevel"} |

### 0x2400:02

| Fact | Reviewed/source value |
| --- | --- |
| EDS name | Obj.Di0 |
| Sources | EDS [2400sub2] lines 1361-1369; EN86 PDF page 8; EN86 PDF page 18; ZH86 PDF page 8; ZH86 PDF page 19 |
| Applicability | eds_family_unbound |
| Declaration disposition | resolved |
| Documentary disposition | pending_qualification |
| Exclusion reason | `null` (not stated) |
| Units | input function code |
| Scaling | One assignment for DI0. |
| Original low / high / default | [null, null, "0"] |
| Effect | Reassigns a physical input function, possibly a limit/stop/enable input. |
| Prerequisites | ["Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.", "Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates."] |
| Edge behavior | Not specified by the retained source. |
| Documentary review | {"gate_ids": ["Q01", "Q19", "Q20"], "generation_type": "resolved", "read_effect": "not_established", "status": "complete", "type_policy": "Retain exact EDS wire width and signedness; no conversion to another profile type.", "value_policy": "0 undefined; 1 origin; 2 positive limit; 3 negative limit; 4 enable/MF; 5 stop; 6 emergency stop; 7..13 user-defined0..6. Factory defaults conflict, read active values.", "write_policy": "qualification_required"} |
| Conflicts | ["C14"] |
| Owner prompt | 21 |
| Semantic coverage D/I/H/Q | yes/no/no/no |
| Raw source claim | {"AccessType": "RW", "DataType": "0x0006", "DefaultValue": "0", "ObjFlags": "0x00000000", "ObjectType": "7", "PDOMapping": "1", "ParameterName": "Obj.Di0"} |

### 0x2400:03

| Fact | Reviewed/source value |
| --- | --- |
| EDS name | Obj.Di1 |
| Sources | EDS [2400sub3] lines 1370-1378; EN86 PDF page 8; EN86 PDF page 18; ZH86 PDF page 8; ZH86 PDF page 19 |
| Applicability | eds_family_unbound |
| Declaration disposition | resolved |
| Documentary disposition | pending_qualification |
| Exclusion reason | `null` (not stated) |
| Units | input function code |
| Scaling | One assignment for DI1. |
| Original low / high / default | [null, null, "0"] |
| Effect | Reassigns a physical input function, possibly a limit/stop/enable input. |
| Prerequisites | ["Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.", "Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates."] |
| Edge behavior | Not specified by the retained source. |
| Documentary review | {"gate_ids": ["Q01", "Q19", "Q20"], "generation_type": "resolved", "read_effect": "not_established", "status": "complete", "type_policy": "Retain exact EDS wire width and signedness; no conversion to another profile type.", "value_policy": "0 undefined; 1 origin; 2 positive limit; 3 negative limit; 4 enable/MF; 5 stop; 6 emergency stop; 7..13 user-defined0..6. Factory defaults conflict, read active values.", "write_policy": "qualification_required"} |
| Conflicts | ["C14"] |
| Owner prompt | 21 |
| Semantic coverage D/I/H/Q | yes/no/no/no |
| Raw source claim | {"AccessType": "RW", "DataType": "0x0006", "DefaultValue": "0", "ObjFlags": "0x00000000", "ObjectType": "7", "PDOMapping": "1", "ParameterName": "Obj.Di1"} |

### 0x2400:04

| Fact | Reviewed/source value |
| --- | --- |
| EDS name | Obj.Di2 |
| Sources | EDS [2400sub4] lines 1379-1387; EN86 PDF page 8; EN86 PDF page 18; ZH86 PDF page 8; ZH86 PDF page 19 |
| Applicability | eds_family_unbound |
| Declaration disposition | resolved |
| Documentary disposition | pending_qualification |
| Exclusion reason | `null` (not stated) |
| Units | input function code |
| Scaling | One assignment for DI2. |
| Original low / high / default | [null, null, "0"] |
| Effect | Reassigns a physical input function, possibly a limit/stop/enable input. |
| Prerequisites | ["Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.", "Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates."] |
| Edge behavior | Not specified by the retained source. |
| Documentary review | {"gate_ids": ["Q01", "Q19", "Q20"], "generation_type": "resolved", "read_effect": "not_established", "status": "complete", "type_policy": "Retain exact EDS wire width and signedness; no conversion to another profile type.", "value_policy": "0 undefined; 1 origin; 2 positive limit; 3 negative limit; 4 enable/MF; 5 stop; 6 emergency stop; 7..13 user-defined0..6. Factory defaults conflict, read active values.", "write_policy": "qualification_required"} |
| Conflicts | ["C14"] |
| Owner prompt | 21 |
| Semantic coverage D/I/H/Q | yes/no/no/no |
| Raw source claim | {"AccessType": "RW", "DataType": "0x0006", "DefaultValue": "0", "ObjFlags": "0x00000000", "ObjectType": "7", "PDOMapping": "1", "ParameterName": "Obj.Di2"} |

### 0x2400:05

| Fact | Reviewed/source value |
| --- | --- |
| EDS name | Obj.Di3 |
| Sources | EDS [2400sub5] lines 1388-1396; EN86 PDF page 8; EN86 PDF page 18; ZH86 PDF page 8; ZH86 PDF page 19 |
| Applicability | eds_family_unbound |
| Declaration disposition | resolved |
| Documentary disposition | pending_qualification |
| Exclusion reason | `null` (not stated) |
| Units | input function code |
| Scaling | One assignment for DI3. |
| Original low / high / default | [null, null, "0"] |
| Effect | Reassigns a physical input function, possibly a limit/stop/enable input. |
| Prerequisites | ["Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.", "Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates."] |
| Edge behavior | Not specified by the retained source. |
| Documentary review | {"gate_ids": ["Q01", "Q19", "Q20"], "generation_type": "resolved", "read_effect": "not_established", "status": "complete", "type_policy": "Retain exact EDS wire width and signedness; no conversion to another profile type.", "value_policy": "0 undefined; 1 origin; 2 positive limit; 3 negative limit; 4 enable/MF; 5 stop; 6 emergency stop; 7..13 user-defined0..6. Factory defaults conflict, read active values.", "write_policy": "qualification_required"} |
| Conflicts | [] |
| Owner prompt | 21 |
| Semantic coverage D/I/H/Q | yes/no/no/no |
| Raw source claim | {"AccessType": "RW", "DataType": "0x0006", "DefaultValue": "0", "ObjFlags": "0x00000000", "ObjectType": "7", "PDOMapping": "1", "ParameterName": "Obj.Di3"} |

### 0x2400:06

| Fact | Reviewed/source value |
| --- | --- |
| EDS name | Obj.Di4 |
| Sources | EDS [2400sub6] lines 1397-1405; EN86 PDF page 8; EN86 PDF page 18; ZH86 PDF page 8; ZH86 PDF page 19 |
| Applicability | eds_family_unbound |
| Declaration disposition | resolved |
| Documentary disposition | pending_qualification |
| Exclusion reason | `null` (not stated) |
| Units | input function code |
| Scaling | One assignment for DI4. |
| Original low / high / default | [null, null, "0"] |
| Effect | Reassigns a physical input function, possibly a limit/stop/enable input. |
| Prerequisites | ["Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.", "Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates."] |
| Edge behavior | Not specified by the retained source. |
| Documentary review | {"gate_ids": ["Q01", "Q19", "Q20"], "generation_type": "resolved", "read_effect": "not_established", "status": "complete", "type_policy": "Retain exact EDS wire width and signedness; no conversion to another profile type.", "value_policy": "0 undefined; 1 origin; 2 positive limit; 3 negative limit; 4 enable/MF; 5 stop; 6 emergency stop; 7..13 user-defined0..6. Factory defaults conflict, read active values.", "write_policy": "qualification_required"} |
| Conflicts | [] |
| Owner prompt | 21 |
| Semantic coverage D/I/H/Q | yes/no/no/no |
| Raw source claim | {"AccessType": "RW", "DataType": "0x0006", "DefaultValue": "0", "ObjFlags": "0x00000000", "ObjectType": "7", "PDOMapping": "1", "ParameterName": "Obj.Di4"} |

### 0x2400:07

| Fact | Reviewed/source value |
| --- | --- |
| EDS name | Obj.Di5 |
| Sources | EDS [2400sub7] lines 1406-1414; EN86 PDF page 8; EN86 PDF page 18; ZH86 PDF page 8; ZH86 PDF page 19 |
| Applicability | eds_family_unbound |
| Declaration disposition | resolved |
| Documentary disposition | pending_qualification |
| Exclusion reason | `null` (not stated) |
| Units | input function code |
| Scaling | One assignment for DI5. |
| Original low / high / default | [null, null, "0"] |
| Effect | Reassigns a physical input function, possibly a limit/stop/enable input. |
| Prerequisites | ["Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.", "Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates."] |
| Edge behavior | Not specified by the retained source. |
| Documentary review | {"gate_ids": ["Q01", "Q19", "Q20"], "generation_type": "resolved", "read_effect": "not_established", "status": "complete", "type_policy": "Retain exact EDS wire width and signedness; no conversion to another profile type.", "value_policy": "0 undefined; 1 origin; 2 positive limit; 3 negative limit; 4 enable/MF; 5 stop; 6 emergency stop; 7..13 user-defined0..6. Factory defaults conflict, read active values.", "write_policy": "qualification_required"} |
| Conflicts | [] |
| Owner prompt | 21 |
| Semantic coverage D/I/H/Q | yes/no/no/no |
| Raw source claim | {"AccessType": "RW", "DataType": "0x0006", "DefaultValue": "0", "ObjFlags": "0x00000000", "ObjectType": "7", "PDOMapping": "1", "ParameterName": "Obj.Di5"} |

### 0x2400:08

| Fact | Reviewed/source value |
| --- | --- |
| EDS name | Obj.Di6 |
| Sources | EDS [2400sub8] lines 1415-1423; EN86 PDF page 8; EN86 PDF page 18; ZH86 PDF page 8; ZH86 PDF page 19 |
| Applicability | eds_family_unbound |
| Declaration disposition | resolved |
| Documentary disposition | pending_qualification |
| Exclusion reason | `null` (not stated) |
| Units | input function code |
| Scaling | One assignment for DI6. |
| Original low / high / default | [null, null, "0"] |
| Effect | Reassigns a physical input function, possibly a limit/stop/enable input. |
| Prerequisites | ["Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.", "Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates."] |
| Edge behavior | Not specified by the retained source. |
| Documentary review | {"gate_ids": ["Q01", "Q19", "Q20"], "generation_type": "resolved", "read_effect": "not_established", "status": "complete", "type_policy": "Retain exact EDS wire width and signedness; no conversion to another profile type.", "value_policy": "0 undefined; 1 origin; 2 positive limit; 3 negative limit; 4 enable/MF; 5 stop; 6 emergency stop; 7..13 user-defined0..6. Factory defaults conflict, read active values.", "write_policy": "qualification_required"} |
| Conflicts | [] |
| Owner prompt | 21 |
| Semantic coverage D/I/H/Q | yes/no/no/no |
| Raw source claim | {"AccessType": "RW", "DataType": "0x0006", "DefaultValue": "0", "ObjFlags": "0x00000000", "ObjectType": "7", "PDOMapping": "1", "ParameterName": "Obj.Di6"} |

### 0x2400:09

| Fact | Reviewed/source value |
| --- | --- |
| EDS name | rsvd1 |
| Sources | EDS [2400sub9] lines 1424-1432 |
| Applicability | eds_family_unbound |
| Declaration disposition | excluded |
| Documentary disposition | excluded |
| Exclusion reason | EDS names this entry Reserved; no typed public access is approved |
| Units | unknown |
| Scaling | Unknown. |
| Original low / high / default | [null, null, "0"] |
| Effect | Reserved in EDS; conflicting manual alias does not authorize access. |
| Prerequisites | ["Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.", "Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates."] |
| Edge behavior | Not specified by the retained source. |
| Documentary review | {"gate_ids": ["Q01", "Q06", "Q20"], "generation_type": "excluded", "read_effect": "not_established", "status": "complete", "type_policy": "Retain exact EDS wire width and signedness; no conversion to another profile type.", "value_policy": "No typed public access; preserve raw source only.", "write_policy": "reserved"} |
| Conflicts | [] |
| Owner prompt | 21 |
| Semantic coverage D/I/H/Q | yes/no/no/no |
| Raw source claim | {"AccessType": "RW", "DataType": "0x0006", "DefaultValue": "0", "ObjFlags": "0x00000000", "ObjectType": "7", "PDOMapping": "1", "ParameterName": "rsvd1"} |

### 0x2400:0A

| Fact | Reviewed/source value |
| --- | --- |
| EDS name | rsvd2 |
| Sources | EDS [2400subA] lines 1433-1441; EN86 PDF page 8; EN86 PDF page 18; ZH86 PDF page 8; ZH86 PDF page 19 |
| Applicability | eds_family_unbound |
| Declaration disposition | excluded |
| Documentary disposition | excluded |
| Exclusion reason | EDS names this entry Reserved; no typed public access is approved |
| Units | unknown |
| Scaling | Unknown. |
| Original low / high / default | [null, null, "0"] |
| Effect | Reserved in EDS; conflicting manual alias does not authorize access. |
| Prerequisites | ["Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.", "Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates."] |
| Edge behavior | Not specified by the retained source. |
| Documentary review | {"gate_ids": ["Q01", "Q08", "Q20"], "generation_type": "excluded", "read_effect": "not_established", "status": "complete", "type_policy": "Retain exact EDS wire width and signedness; no conversion to another profile type.", "value_policy": "No typed public access; preserve raw source only.", "write_policy": "reserved"} |
| Conflicts | ["C15"] |
| Owner prompt | 21 |
| Semantic coverage D/I/H/Q | yes/no/no/no |
| Raw source claim | {"AccessType": "RW", "DataType": "0x0006", "DefaultValue": "0", "ObjFlags": "0x00000000", "ObjectType": "7", "PDOMapping": "1", "ParameterName": "rsvd2"} |

### 0x2400:0B

| Fact | Reviewed/source value |
| --- | --- |
| EDS name | rsvd3 |
| Sources | EDS [2400subB] lines 1442-1450; EN86 PDF page 8; EN86 PDF page 18; ZH86 PDF page 8; ZH86 PDF page 19 |
| Applicability | eds_family_unbound |
| Declaration disposition | excluded |
| Documentary disposition | excluded |
| Exclusion reason | EDS names this entry Reserved; no typed public access is approved |
| Units | unknown |
| Scaling | Unknown. |
| Original low / high / default | [null, null, "0"] |
| Effect | Reserved in EDS; conflicting manual alias does not authorize access. |
| Prerequisites | ["Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.", "Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates."] |
| Edge behavior | Not specified by the retained source. |
| Documentary review | {"gate_ids": ["Q01", "Q06", "Q20"], "generation_type": "excluded", "read_effect": "not_established", "status": "complete", "type_policy": "Retain exact EDS wire width and signedness; no conversion to another profile type.", "value_policy": "No typed public access; preserve raw source only.", "write_policy": "reserved"} |
| Conflicts | ["C01"] |
| Owner prompt | 21 |
| Semantic coverage D/I/H/Q | yes/no/no/no |
| Raw source claim | {"AccessType": "RW", "DataType": "0x0006", "DefaultValue": "0", "ObjFlags": "0x00000000", "ObjectType": "7", "PDOMapping": "1", "ParameterName": "rsvd3"} |

### 0x2400:0C

| Fact | Reviewed/source value |
| --- | --- |
| EDS name | Obj.Dolevel |
| Sources | EDS [2400subC] lines 1451-1459; EN86 PDF page 8; EN86 PDF page 18; ZH86 PDF page 8; ZH86 PDF page 19 |
| Applicability | eds_family_unbound |
| Declaration disposition | resolved |
| Documentary disposition | pending_qualification |
| Exclusion reason | `null` (not stated) |
| Units | output polarity/function code |
| Scaling | 2400:0B..0F meanings conflict. |
| Original low / high / default | [null, null, "0"] |
| Effect | May reassign output function/polarity, including brake output. |
| Prerequisites | ["Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.", "Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates."] |
| Edge behavior | Not specified by the retained source. |
| Documentary review | {"gate_ids": ["Q01", "Q06", "Q20", "Q19"], "generation_type": "resolved", "read_effect": "not_established", "status": "complete", "type_policy": "Retain exact EDS wire width and signedness; no conversion to another profile type.", "value_policy": "Do not union addresses. Functions 0 undefined,1 alarm,4 in-position,5 brake release,9..11 user-defined0..2; 2/3 conflict,6..8 undocumented. Only bits0/1 of polarity are described despite three outputs.", "write_policy": "not_writable"} |
| Conflicts | ["C01", "C02", "C14"] |
| Owner prompt | 21 |
| Semantic coverage D/I/H/Q | yes/no/no/no |
| Raw source claim | {"AccessType": "RW", "DataType": "0x0006", "DefaultValue": "0", "ObjFlags": "0x00000000", "ObjectType": "7", "PDOMapping": "1", "ParameterName": "Obj.Dolevel"} |

### 0x2400:0D

| Fact | Reviewed/source value |
| --- | --- |
| EDS name | Obj.Do0 |
| Sources | EDS [2400subD] lines 1460-1468; EN86 PDF page 8; EN86 PDF page 18; ZH86 PDF page 8; ZH86 PDF page 19 |
| Applicability | eds_family_unbound |
| Declaration disposition | resolved |
| Documentary disposition | pending_qualification |
| Exclusion reason | `null` (not stated) |
| Units | output polarity/function code |
| Scaling | 2400:0B..0F meanings conflict. |
| Original low / high / default | [null, null, "0"] |
| Effect | May reassign output function/polarity, including brake output. |
| Prerequisites | ["Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.", "Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates."] |
| Edge behavior | Not specified by the retained source. |
| Documentary review | {"gate_ids": ["Q01", "Q06", "Q20", "Q19"], "generation_type": "resolved", "read_effect": "not_established", "status": "complete", "type_policy": "Retain exact EDS wire width and signedness; no conversion to another profile type.", "value_policy": "Do not union addresses. Functions 0 undefined,1 alarm,4 in-position,5 brake release,9..11 user-defined0..2; 2/3 conflict,6..8 undocumented. Only bits0/1 of polarity are described despite three outputs.", "write_policy": "not_writable"} |
| Conflicts | ["C01", "C02", "C14"] |
| Owner prompt | 21 |
| Semantic coverage D/I/H/Q | yes/no/no/no |
| Raw source claim | {"AccessType": "RW", "DataType": "0x0006", "DefaultValue": "0", "ObjFlags": "0x00000000", "ObjectType": "7", "PDOMapping": "1", "ParameterName": "Obj.Do0"} |

### 0x2400:0E

| Fact | Reviewed/source value |
| --- | --- |
| EDS name | Obj.Do1 |
| Sources | EDS [2400subE] lines 1469-1477; EN86 PDF page 8; EN86 PDF page 18; ZH86 PDF page 8; ZH86 PDF page 19 |
| Applicability | eds_family_unbound |
| Declaration disposition | resolved |
| Documentary disposition | pending_qualification |
| Exclusion reason | `null` (not stated) |
| Units | output polarity/function code |
| Scaling | 2400:0B..0F meanings conflict. |
| Original low / high / default | [null, null, "0"] |
| Effect | May reassign output function/polarity, including brake output. |
| Prerequisites | ["Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.", "Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates."] |
| Edge behavior | Not specified by the retained source. |
| Documentary review | {"gate_ids": ["Q01", "Q06", "Q20", "Q19"], "generation_type": "resolved", "read_effect": "not_established", "status": "complete", "type_policy": "Retain exact EDS wire width and signedness; no conversion to another profile type.", "value_policy": "Do not union addresses. Functions 0 undefined,1 alarm,4 in-position,5 brake release,9..11 user-defined0..2; 2/3 conflict,6..8 undocumented. Only bits0/1 of polarity are described despite three outputs.", "write_policy": "not_writable"} |
| Conflicts | ["C01", "C02", "C04"] |
| Owner prompt | 21 |
| Semantic coverage D/I/H/Q | yes/no/no/no |
| Raw source claim | {"AccessType": "RW", "DataType": "0x0006", "DefaultValue": "0", "ObjFlags": "0x00000000", "ObjectType": "7", "PDOMapping": "1", "ParameterName": "Obj.Do1"} |

### 0x2400:0F

| Fact | Reviewed/source value |
| --- | --- |
| EDS name | Obj.Do2 |
| Sources | EDS [2400subF] lines 1478-1486; EN86 PDF page 8; EN86 PDF page 18; ZH86 PDF page 8; ZH86 PDF page 19 |
| Applicability | eds_family_unbound |
| Declaration disposition | resolved |
| Documentary disposition | pending_qualification |
| Exclusion reason | `null` (not stated) |
| Units | output polarity/function code |
| Scaling | 2400:0B..0F meanings conflict. |
| Original low / high / default | [null, null, "0"] |
| Effect | May reassign output function/polarity, including brake output. |
| Prerequisites | ["Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.", "Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates."] |
| Edge behavior | Not specified by the retained source. |
| Documentary review | {"gate_ids": ["Q01", "Q06", "Q20"], "generation_type": "resolved", "read_effect": "not_established", "status": "complete", "type_policy": "Retain exact EDS wire width and signedness; no conversion to another profile type.", "value_policy": "Do not union addresses. Functions 0 undefined,1 alarm,4 in-position,5 brake release,9..11 user-defined0..2; 2/3 conflict,6..8 undocumented. Only bits0/1 of polarity are described despite three outputs.", "write_policy": "not_writable"} |
| Conflicts | ["C01", "C02"] |
| Owner prompt | 21 |
| Semantic coverage D/I/H/Q | yes/no/no/no |
| Raw source claim | {"AccessType": "RW", "DataType": "0x0006", "DefaultValue": "0", "ObjFlags": "0x00000000", "ObjectType": "7", "PDOMapping": "1", "ParameterName": "Obj.Do2"} |

### 0x2400:10

| Fact | Reviewed/source value |
| --- | --- |
| EDS name | rsvd4 |
| Sources | EDS [2400sub10] lines 1487-1495 |
| Applicability | eds_family_unbound |
| Declaration disposition | excluded |
| Documentary disposition | excluded |
| Exclusion reason | EDS names this entry Reserved; no typed public access is approved |
| Units | unknown |
| Scaling | Unknown. |
| Original low / high / default | [null, null, "0"] |
| Effect | Reserved in EDS; conflicting manual alias does not authorize access. |
| Prerequisites | ["Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.", "Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates."] |
| Edge behavior | Not specified by the retained source. |
| Documentary review | {"gate_ids": ["Q01", "Q06", "Q20"], "generation_type": "excluded", "read_effect": "not_established", "status": "complete", "type_policy": "Retain exact EDS wire width and signedness; no conversion to another profile type.", "value_policy": "No typed public access; preserve raw source only.", "write_policy": "reserved"} |
| Conflicts | [] |
| Owner prompt | 21 |
| Semantic coverage D/I/H/Q | yes/no/no/no |
| Raw source claim | {"AccessType": "RW", "DataType": "0x0006", "DefaultValue": "0", "ObjFlags": "0x00000000", "ObjectType": "7", "PDOMapping": "1", "ParameterName": "rsvd4"} |

### 0x2401:00

| Fact | Reviewed/source value |
| --- | --- |
| EDS name | Obj.Di0Filtime |
| Sources | EDS [2401] lines 1496-1504; EN86 PDF page 8; EN86 PDF page 18; ZH86 PDF page 19 |
| Applicability | eds_family_unbound |
| Declaration disposition | resolved |
| Documentary disposition | pending_qualification |
| Exclusion reason | `null` (not stated) |
| Units | filter coefficient |
| Scaling | No time unit or equation established. |
| Original low / high / default | [null, null, "50"] |
| Effect | Changes input filtering, possibly protective-signal timing. |
| Prerequisites | ["Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.", "Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates."] |
| Edge behavior | Not specified by the retained source. |
| Documentary review | {"gate_ids": ["Q01", "Q08", "Q20"], "generation_type": "resolved", "read_effect": "not_established", "status": "complete", "type_policy": "Retain exact EDS wire width and signedness; no conversion to another profile type.", "value_policy": "EDS defaults 50/40/100/4000/10/2/16 versus manual all2; no accepted range.", "write_policy": "not_writable"} |
| Conflicts | ["C16"] |
| Owner prompt | 21 |
| Semantic coverage D/I/H/Q | yes/no/no/no |
| Raw source claim | {"AccessType": "RW", "DataType": "0x0006", "DefaultValue": "50", "ObjFlags": "0x00000000", "ObjectType": "7", "PDOMapping": "1", "ParameterName": "Obj.Di0Filtime"} |

### 0x2402:00

| Fact | Reviewed/source value |
| --- | --- |
| EDS name | Obj.Di1Filtime |
| Sources | EDS [2402] lines 1505-1513; EN86 PDF page 8; EN86 PDF page 18; ZH86 PDF page 19 |
| Applicability | eds_family_unbound |
| Declaration disposition | resolved |
| Documentary disposition | pending_qualification |
| Exclusion reason | `null` (not stated) |
| Units | filter coefficient |
| Scaling | No time unit or equation established. |
| Original low / high / default | [null, null, "40"] |
| Effect | Changes input filtering, possibly protective-signal timing. |
| Prerequisites | ["Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.", "Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates."] |
| Edge behavior | Not specified by the retained source. |
| Documentary review | {"gate_ids": ["Q01", "Q08", "Q20"], "generation_type": "resolved", "read_effect": "not_established", "status": "complete", "type_policy": "Retain exact EDS wire width and signedness; no conversion to another profile type.", "value_policy": "EDS defaults 50/40/100/4000/10/2/16 versus manual all2; no accepted range.", "write_policy": "not_writable"} |
| Conflicts | ["C16"] |
| Owner prompt | 21 |
| Semantic coverage D/I/H/Q | yes/no/no/no |
| Raw source claim | {"AccessType": "RW", "DataType": "0x0006", "DefaultValue": "40", "ObjFlags": "0x00000000", "ObjectType": "7", "PDOMapping": "1", "ParameterName": "Obj.Di1Filtime"} |

### 0x2403:00

| Fact | Reviewed/source value |
| --- | --- |
| EDS name | Obj.Di2Filtime |
| Sources | EDS [2403] lines 1514-1522; EN86 PDF page 8; EN86 PDF page 19; ZH86 PDF page 19 |
| Applicability | eds_family_unbound |
| Declaration disposition | resolved |
| Documentary disposition | pending_qualification |
| Exclusion reason | `null` (not stated) |
| Units | filter coefficient |
| Scaling | No time unit or equation established. |
| Original low / high / default | [null, null, "100"] |
| Effect | Changes input filtering, possibly protective-signal timing. |
| Prerequisites | ["Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.", "Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates."] |
| Edge behavior | Not specified by the retained source. |
| Documentary review | {"gate_ids": ["Q01", "Q08", "Q20"], "generation_type": "resolved", "read_effect": "not_established", "status": "complete", "type_policy": "Retain exact EDS wire width and signedness; no conversion to another profile type.", "value_policy": "EDS defaults 50/40/100/4000/10/2/16 versus manual all2; no accepted range.", "write_policy": "not_writable"} |
| Conflicts | ["C16"] |
| Owner prompt | 21 |
| Semantic coverage D/I/H/Q | yes/no/no/no |
| Raw source claim | {"AccessType": "RW", "DataType": "0x0006", "DefaultValue": "100", "ObjFlags": "0x00000000", "ObjectType": "7", "PDOMapping": "1", "ParameterName": "Obj.Di2Filtime"} |

### 0x2404:00

| Fact | Reviewed/source value |
| --- | --- |
| EDS name | Obj.Di3Filtime |
| Sources | EDS [2404] lines 1523-1531; EN86 PDF page 8; EN86 PDF page 19; ZH86 PDF page 19 |
| Applicability | eds_family_unbound |
| Declaration disposition | resolved |
| Documentary disposition | pending_qualification |
| Exclusion reason | `null` (not stated) |
| Units | filter coefficient |
| Scaling | No time unit or equation established. |
| Original low / high / default | [null, null, "4000"] |
| Effect | Changes input filtering, possibly protective-signal timing. |
| Prerequisites | ["Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.", "Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates."] |
| Edge behavior | Not specified by the retained source. |
| Documentary review | {"gate_ids": ["Q01", "Q08", "Q20"], "generation_type": "resolved", "read_effect": "not_established", "status": "complete", "type_policy": "Retain exact EDS wire width and signedness; no conversion to another profile type.", "value_policy": "EDS defaults 50/40/100/4000/10/2/16 versus manual all2; no accepted range.", "write_policy": "not_writable"} |
| Conflicts | ["C16"] |
| Owner prompt | 21 |
| Semantic coverage D/I/H/Q | yes/no/no/no |
| Raw source claim | {"AccessType": "RW", "DataType": "0x0006", "DefaultValue": "4000", "ObjFlags": "0x00000000", "ObjectType": "7", "PDOMapping": "1", "ParameterName": "Obj.Di3Filtime"} |

### 0x2405:00

| Fact | Reviewed/source value |
| --- | --- |
| EDS name | Obj.Di4Filtime |
| Sources | EDS [2405] lines 1532-1540; EN86 PDF page 8; EN86 PDF page 19; ZH86 PDF page 19 |
| Applicability | eds_family_unbound |
| Declaration disposition | resolved |
| Documentary disposition | pending_qualification |
| Exclusion reason | `null` (not stated) |
| Units | filter coefficient |
| Scaling | No time unit or equation established. |
| Original low / high / default | [null, null, "10"] |
| Effect | Changes input filtering, possibly protective-signal timing. |
| Prerequisites | ["Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.", "Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates."] |
| Edge behavior | Not specified by the retained source. |
| Documentary review | {"gate_ids": ["Q01", "Q08", "Q20"], "generation_type": "resolved", "read_effect": "not_established", "status": "complete", "type_policy": "Retain exact EDS wire width and signedness; no conversion to another profile type.", "value_policy": "EDS defaults 50/40/100/4000/10/2/16 versus manual all2; no accepted range.", "write_policy": "not_writable"} |
| Conflicts | ["C16"] |
| Owner prompt | 21 |
| Semantic coverage D/I/H/Q | yes/no/no/no |
| Raw source claim | {"AccessType": "RW", "DataType": "0x0006", "DefaultValue": "10", "ObjFlags": "0x00000000", "ObjectType": "7", "PDOMapping": "1", "ParameterName": "Obj.Di4Filtime"} |

### 0x2406:00

| Fact | Reviewed/source value |
| --- | --- |
| EDS name | Obj.Di5Filtime |
| Sources | EDS [2406] lines 1541-1549; EN86 PDF page 8; EN86 PDF page 19; ZH86 PDF page 19 |
| Applicability | eds_family_unbound |
| Declaration disposition | resolved |
| Documentary disposition | pending_qualification |
| Exclusion reason | `null` (not stated) |
| Units | filter coefficient |
| Scaling | No time unit or equation established. |
| Original low / high / default | [null, null, "2"] |
| Effect | Changes input filtering, possibly protective-signal timing. |
| Prerequisites | ["Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.", "Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates."] |
| Edge behavior | Not specified by the retained source. |
| Documentary review | {"gate_ids": ["Q01", "Q08", "Q20"], "generation_type": "resolved", "read_effect": "not_established", "status": "complete", "type_policy": "Retain exact EDS wire width and signedness; no conversion to another profile type.", "value_policy": "EDS defaults 50/40/100/4000/10/2/16 versus manual all2; no accepted range.", "write_policy": "not_writable"} |
| Conflicts | ["C16"] |
| Owner prompt | 21 |
| Semantic coverage D/I/H/Q | yes/no/no/no |
| Raw source claim | {"AccessType": "RW", "DataType": "0x0006", "DefaultValue": "2", "ObjFlags": "0x00000000", "ObjectType": "7", "PDOMapping": "1", "ParameterName": "Obj.Di5Filtime"} |

### 0x2407:00

| Fact | Reviewed/source value |
| --- | --- |
| EDS name | Obj.Di6Filtime |
| Sources | EDS [2407] lines 1550-1558; EN86 PDF page 8; EN86 PDF page 19; ZH86 PDF page 19 |
| Applicability | eds_family_unbound |
| Declaration disposition | resolved |
| Documentary disposition | pending_qualification |
| Exclusion reason | `null` (not stated) |
| Units | filter coefficient |
| Scaling | No time unit or equation established. |
| Original low / high / default | [null, null, "16"] |
| Effect | Changes input filtering, possibly protective-signal timing. |
| Prerequisites | ["Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.", "Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates."] |
| Edge behavior | Not specified by the retained source. |
| Documentary review | {"gate_ids": ["Q01", "Q08", "Q20"], "generation_type": "resolved", "read_effect": "not_established", "status": "complete", "type_policy": "Retain exact EDS wire width and signedness; no conversion to another profile type.", "value_policy": "EDS defaults 50/40/100/4000/10/2/16 versus manual all2; no accepted range.", "write_policy": "not_writable"} |
| Conflicts | ["C16"] |
| Owner prompt | 21 |
| Semantic coverage D/I/H/Q | yes/no/no/no |
| Raw source claim | {"AccessType": "RW", "DataType": "0x0006", "DefaultValue": "16", "ObjFlags": "0x00000000", "ObjectType": "7", "PDOMapping": "1", "ParameterName": "Obj.Di6Filtime"} |

### 0x2500:00

| Fact | Reviewed/source value |
| --- | --- |
| EDS name | Obj.DrvMode |
| Sources | EDS [2500] lines 1559-1566; EN86 PDF page 19; ZH86 PDF page 19 |
| Applicability | eds_family_unbound |
| Declaration disposition | resolved |
| Documentary disposition | pending_qualification |
| Exclusion reason | `null` (not stated) |
| Units | loop selector |
| Scaling | No unqualified conversion or range inferred. |
| Original low / high / default | [null, null, null] |
| Effect | Changes native loop/current/tuning behavior; not a generic torque or autotune command. |
| Prerequisites | ["Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.", "Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates."] |
| Edge behavior | Not specified by the retained source. |
| Documentary review | {"gate_ids": ["Q01", "Q09", "Q20"], "generation_type": "resolved", "read_effect": "not_established", "status": "complete", "type_policy": "Retain exact EDS wire width and signedness; no conversion to another profile type.", "value_policy": "0 ineffective, 1 open loop, 2 closed loop; distinct from 6060 motion mode. No EDS default or limits.", "write_policy": "not_writable"} |
| Conflicts | [] |
| Owner prompt | 22 |
| Semantic coverage D/I/H/Q | yes/no/no/no |
| Raw source claim | {"AccessType": "RW", "DataType": "0x0006", "ObjFlags": "0x00000000", "ObjectType": "7", "PDOMapping": "1", "ParameterName": "Obj.DrvMode"} |

### 0x2501:00

| Fact | Reviewed/source value |
| --- | --- |
| EDS name | Obj.Encoder |
| Sources | EDS [2501] lines 1567-1574; EN86 PDF page 19; ZH86 PDF page 19 |
| Applicability | eds_family_unbound |
| Declaration disposition | resolved |
| Documentary disposition | pending_qualification |
| Exclusion reason | `null` (not stated) |
| Units | encoder resolution |
| Scaling | No unqualified conversion or range inferred. |
| Original low / high / default | [null, null, null] |
| Effect | Changes native loop/current/tuning behavior; not a generic torque or autotune command. |
| Prerequisites | ["Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.", "Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates."] |
| Edge behavior | Not specified by the retained source. |
| Documentary review | {"gate_ids": ["Q01", "Q09", "Q04", "Q20"], "generation_type": "resolved", "read_effect": "not_established", "status": "complete", "type_policy": "Retain exact EDS wire width and signedness; no conversion to another profile type.", "value_policy": "Line/count/quadrature basis not specified. No EDS default or limits.", "write_policy": "not_writable"} |
| Conflicts | [] |
| Owner prompt | 22 |
| Semantic coverage D/I/H/Q | yes/no/no/no |
| Raw source claim | {"AccessType": "RW", "DataType": "0x0006", "ObjFlags": "0x00000000", "ObjectType": "7", "PDOMapping": "1", "ParameterName": "Obj.Encoder"} |

### 0x2502:00

| Fact | Reviewed/source value |
| --- | --- |
| EDS name | Obj.Maxcurrent |
| Sources | EDS [2502] lines 1575-1582; EN86 PDF page 19; ZH86 PDF page 19 |
| Applicability | eds_family_unbound |
| Declaration disposition | resolved |
| Documentary disposition | pending_qualification |
| Exclusion reason | `null` (not stated) |
| Units | mA |
| Scaling | No unqualified conversion or range inferred. |
| Original low / high / default | [null, null, null] |
| Effect | Changes native loop/current/tuning behavior; not a generic torque or autotune command. |
| Prerequisites | ["Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.", "Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates."] |
| Edge behavior | Not specified by the retained source. |
| Documentary review | {"gate_ids": ["Q01", "Q09", "Q20"], "generation_type": "resolved", "read_effect": "not_established", "status": "complete", "type_policy": "Retain exact EDS wire width and signedness; no conversion to another profile type.", "value_policy": "Maximum peak current, mA, not RMS. Admissible range, activation and motor ratings are not supplied. No EDS default or limits.", "write_policy": "not_writable"} |
| Conflicts | [] |
| Owner prompt | 22 |
| Semantic coverage D/I/H/Q | yes/no/no/no |
| Raw source claim | {"AccessType": "RW", "DataType": "0x0006", "ObjFlags": "0x00000000", "ObjectType": "7", "PDOMapping": "1", "ParameterName": "Obj.Maxcurrent"} |

### 0x2503:00

| Fact | Reviewed/source value |
| --- | --- |
| EDS name | Obj.ClMaxCper |
| Sources | EDS [2503] lines 1583-1590; EN86 PDF page 19; ZH86 PDF page 19 |
| Applicability | eds_family_unbound |
| Declaration disposition | resolved |
| Documentary disposition | pending_qualification |
| Exclusion reason | `null` (not stated) |
| Units | percent |
| Scaling | No unqualified conversion or range inferred. |
| Original low / high / default | [null, null, null] |
| Effect | Changes native loop/current/tuning behavior; not a generic torque or autotune command. |
| Prerequisites | ["Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.", "Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates."] |
| Edge behavior | Not specified by the retained source. |
| Documentary review | {"gate_ids": ["Q01", "Q09", "Q20"], "generation_type": "resolved", "read_effect": "not_established", "status": "complete", "type_policy": "Retain exact EDS wire width and signedness; no conversion to another profile type.", "value_policy": "Closed-loop maximum current ratio; range/basis not established. No EDS default or limits.", "write_policy": "not_writable"} |
| Conflicts | [] |
| Owner prompt | 22 |
| Semantic coverage D/I/H/Q | yes/no/no/no |
| Raw source claim | {"AccessType": "RW", "DataType": "0x0006", "ObjFlags": "0x00000000", "ObjectType": "7", "PDOMapping": "1", "ParameterName": "Obj.ClMaxCper"} |

### 0x2504:00

| Fact | Reviewed/source value |
| --- | --- |
| EDS name | Obj.ClbasCper |
| Sources | EDS [2504] lines 1591-1598; EN86 PDF page 19; ZH86 PDF page 19 |
| Applicability | eds_family_unbound |
| Declaration disposition | resolved |
| Documentary disposition | pending_qualification |
| Exclusion reason | `null` (not stated) |
| Units | percent |
| Scaling | No unqualified conversion or range inferred. |
| Original low / high / default | [null, null, null] |
| Effect | Changes native loop/current/tuning behavior; not a generic torque or autotune command. |
| Prerequisites | ["Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.", "Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates."] |
| Edge behavior | Not specified by the retained source. |
| Documentary review | {"gate_ids": ["Q01", "Q09", "Q20"], "generation_type": "resolved", "read_effect": "not_established", "status": "complete", "type_policy": "Retain exact EDS wire width and signedness; no conversion to another profile type.", "value_policy": "Closed-loop base current ratio; range/basis not established. No EDS default or limits.", "write_policy": "not_writable"} |
| Conflicts | [] |
| Owner prompt | 22 |
| Semantic coverage D/I/H/Q | yes/no/no/no |
| Raw source claim | {"AccessType": "RW", "DataType": "0x0006", "ObjFlags": "0x00000000", "ObjectType": "7", "PDOMapping": "1", "ParameterName": "Obj.ClbasCper"} |

### 0x2505:00

| Fact | Reviewed/source value |
| --- | --- |
| EDS name | Obj.OlMaxCper |
| Sources | EDS [2505] lines 1599-1606; EN86 PDF page 19; ZH86 PDF page 19 |
| Applicability | eds_family_unbound |
| Declaration disposition | resolved |
| Documentary disposition | pending_qualification |
| Exclusion reason | `null` (not stated) |
| Units | percent |
| Scaling | No unqualified conversion or range inferred. |
| Original low / high / default | [null, null, null] |
| Effect | Changes native loop/current/tuning behavior; not a generic torque or autotune command. |
| Prerequisites | ["Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.", "Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates."] |
| Edge behavior | Not specified by the retained source. |
| Documentary review | {"gate_ids": ["Q01", "Q09", "Q20"], "generation_type": "resolved", "read_effect": "not_established", "status": "complete", "type_policy": "Retain exact EDS wire width and signedness; no conversion to another profile type.", "value_policy": "Open-loop maximum current ratio; range/basis not established. No EDS default or limits.", "write_policy": "not_writable"} |
| Conflicts | [] |
| Owner prompt | 22 |
| Semantic coverage D/I/H/Q | yes/no/no/no |
| Raw source claim | {"AccessType": "RW", "DataType": "0x0006", "ObjFlags": "0x00000000", "ObjectType": "7", "PDOMapping": "1", "ParameterName": "Obj.OlMaxCper"} |

### 0x2506:00

| Fact | Reviewed/source value |
| --- | --- |
| EDS name | Obj.suo_per |
| Sources | EDS [2506] lines 1607-1614; EN86 PDF page 19; ZH86 PDF page 20 |
| Applicability | eds_family_unbound |
| Declaration disposition | resolved |
| Documentary disposition | pending_qualification |
| Exclusion reason | `null` (not stated) |
| Units | percent |
| Scaling | No unqualified conversion or range inferred. |
| Original low / high / default | [null, null, null] |
| Effect | Changes native loop/current/tuning behavior; not a generic torque or autotune command. |
| Prerequisites | ["Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.", "Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates."] |
| Edge behavior | Not specified by the retained source. |
| Documentary review | {"gate_ids": ["Q01", "Q09", "Q20"], "generation_type": "resolved", "read_effect": "not_established", "status": "complete", "type_policy": "Retain exact EDS wire width and signedness; no conversion to another profile type.", "value_policy": "Standstill current ratio; range/basis not established. No EDS default or limits.", "write_policy": "not_writable"} |
| Conflicts | [] |
| Owner prompt | 22 |
| Semantic coverage D/I/H/Q | yes/no/no/no |
| Raw source claim | {"AccessType": "RW", "DataType": "0x0006", "ObjFlags": "0x00000000", "ObjectType": "7", "PDOMapping": "1", "ParameterName": "Obj.suo_per"} |

### 0x2507:00

| Fact | Reviewed/source value |
| --- | --- |
| EDS name | Obj.suo_time |
| Sources | EDS [2507] lines 1615-1622; EN86 PDF page 19; ZH86 PDF page 20 |
| Applicability | eds_family_unbound |
| Declaration disposition | resolved |
| Documentary disposition | pending_qualification |
| Exclusion reason | `null` (not stated) |
| Units | ms |
| Scaling | No unqualified conversion or range inferred. |
| Original low / high / default | [null, null, null] |
| Effect | Changes native loop/current/tuning behavior; not a generic torque or autotune command. |
| Prerequisites | ["Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.", "Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates."] |
| Edge behavior | Not specified by the retained source. |
| Documentary review | {"gate_ids": ["Q01", "Q09", "Q20"], "generation_type": "resolved", "read_effect": "not_established", "status": "complete", "type_policy": "Retain exact EDS wire width and signedness; no conversion to another profile type.", "value_policy": "Standstill-current timing; range and exact transition not established. No EDS default or limits.", "write_policy": "not_writable"} |
| Conflicts | [] |
| Owner prompt | 22 |
| Semantic coverage D/I/H/Q | yes/no/no/no |
| Raw source claim | {"AccessType": "RW", "DataType": "0x0006", "ObjFlags": "0x00000000", "ObjectType": "7", "PDOMapping": "1", "ParameterName": "Obj.suo_time"} |

### 0x2508:00

| Fact | Reviewed/source value |
| --- | --- |
| EDS name | Obj.Filtable |
| Sources | EDS [2508] lines 1623-1630; EN86 PDF page 19; ZH86 PDF page 20 |
| Applicability | eds_family_unbound |
| Declaration disposition | resolved |
| Documentary disposition | pending_qualification |
| Exclusion reason | `null` (not stated) |
| Units | enable code |
| Scaling | No unqualified conversion or range inferred. |
| Original low / high / default | [null, null, null] |
| Effect | Changes native loop/current/tuning behavior; not a generic torque or autotune command. |
| Prerequisites | ["Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.", "Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates."] |
| Edge behavior | Not specified by the retained source. |
| Documentary review | {"gate_ids": ["Q01", "Q09", "Q20"], "generation_type": "resolved", "read_effect": "not_established", "status": "complete", "type_policy": "Retain exact EDS wire width and signedness; no conversion to another profile type.", "value_policy": "Low-pass enable, factory parameter; no Boolean option set established. No EDS default or limits.", "write_policy": "not_writable"} |
| Conflicts | [] |
| Owner prompt | 22 |
| Semantic coverage D/I/H/Q | yes/no/no/no |
| Raw source claim | {"AccessType": "RW", "DataType": "0x0006", "ObjFlags": "0x00000000", "ObjectType": "7", "PDOMapping": "1", "ParameterName": "Obj.Filtable"} |

### 0x2509:00

| Fact | Reviewed/source value |
| --- | --- |
| EDS name | Obj.Filtratio |
| Sources | EDS [2509] lines 1631-1638; EN86 PDF page 19; ZH86 PDF page 20 |
| Applicability | eds_family_unbound |
| Declaration disposition | resolved |
| Documentary disposition | pending_qualification |
| Exclusion reason | `null` (not stated) |
| Units | coefficient |
| Scaling | No unqualified conversion or range inferred. |
| Original low / high / default | [null, null, null] |
| Effect | Changes native loop/current/tuning behavior; not a generic torque or autotune command. |
| Prerequisites | ["Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.", "Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates."] |
| Edge behavior | Not specified by the retained source. |
| Documentary review | {"gate_ids": ["Q01", "Q09", "Q20"], "generation_type": "resolved", "read_effect": "not_established", "status": "complete", "type_policy": "Retain exact EDS wire width and signedness; no conversion to another profile type.", "value_policy": "Filter coefficient; equation not supplied. No EDS default or limits.", "write_policy": "not_writable"} |
| Conflicts | [] |
| Owner prompt | 22 |
| Semantic coverage D/I/H/Q | yes/no/no/no |
| Raw source claim | {"AccessType": "RW", "DataType": "0x0006", "ObjFlags": "0x00000000", "ObjectType": "7", "PDOMapping": "1", "ParameterName": "Obj.Filtratio"} |

### 0x250A:00

| Fact | Reviewed/source value |
| --- | --- |
| EDS name | Obj.PosAlrm |
| Sources | EDS [250A] lines 1639-1646; EN86 PDF page 19; ZH86 PDF page 20 |
| Applicability | eds_family_unbound |
| Declaration disposition | resolved |
| Documentary disposition | pending_qualification |
| Exclusion reason | `null` (not stated) |
| Units | position threshold |
| Scaling | No unqualified conversion or range inferred. |
| Original low / high / default | [null, null, null] |
| Effect | Changes native loop/current/tuning behavior; not a generic torque or autotune command. |
| Prerequisites | ["Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.", "Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates."] |
| Edge behavior | Not specified by the retained source. |
| Documentary review | {"gate_ids": ["Q01", "Q09", "Q04", "Q20"], "generation_type": "resolved", "read_effect": "not_established", "status": "complete", "type_policy": "Retain exact EDS wire width and signedness; no conversion to another profile type.", "value_policy": "Position-deviation alarm threshold; scale/range not supplied. No EDS default or limits.", "write_policy": "not_writable"} |
| Conflicts | [] |
| Owner prompt | 22 |
| Semantic coverage D/I/H/Q | yes/no/no/no |
| Raw source claim | {"AccessType": "RW", "DataType": "0x0006", "ObjFlags": "0x00000000", "ObjectType": "7", "PDOMapping": "1", "ParameterName": "Obj.PosAlrm"} |

### 0x250B:00

| Fact | Reviewed/source value |
| --- | --- |
| EDS name | Obj.PosError |
| Sources | EDS [250B] lines 1647-1654; EN86 PDF page 19; ZH86 PDF page 20 |
| Applicability | eds_family_unbound |
| Declaration disposition | resolved |
| Documentary disposition | pending_qualification |
| Exclusion reason | `null` (not stated) |
| Units | position threshold |
| Scaling | No unqualified conversion or range inferred. |
| Original low / high / default | [null, null, null] |
| Effect | Changes native loop/current/tuning behavior; not a generic torque or autotune command. |
| Prerequisites | ["Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.", "Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates."] |
| Edge behavior | Not specified by the retained source. |
| Documentary review | {"gate_ids": ["Q01", "Q09", "Q04", "Q20"], "generation_type": "resolved", "read_effect": "not_established", "status": "complete", "type_policy": "Retain exact EDS wire width and signedness; no conversion to another profile type.", "value_policy": "Position-completion threshold; scale/range not supplied. No EDS default or limits.", "write_policy": "not_writable"} |
| Conflicts | [] |
| Owner prompt | 22 |
| Semantic coverage D/I/H/Q | yes/no/no/no |
| Raw source claim | {"AccessType": "RW", "DataType": "0x0006", "ObjFlags": "0x00000000", "ObjectType": "7", "PDOMapping": "1", "ParameterName": "Obj.PosError"} |

### 0x250C:00

| Fact | Reviewed/source value |
| --- | --- |
| EDS name | Obj.PosinTime |
| Sources | EDS [250C] lines 1655-1662; EN86 PDF page 19; ZH86 PDF page 20 |
| Applicability | eds_family_unbound |
| Declaration disposition | resolved |
| Documentary disposition | pending_qualification |
| Exclusion reason | `null` (not stated) |
| Units | time field |
| Scaling | No unqualified conversion or range inferred. |
| Original low / high / default | [null, null, null] |
| Effect | Changes native loop/current/tuning behavior; not a generic torque or autotune command. |
| Prerequisites | ["Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.", "Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates."] |
| Edge behavior | Not specified by the retained source. |
| Documentary review | {"gate_ids": ["Q01", "Q09", "Q20"], "generation_type": "resolved", "read_effect": "not_established", "status": "complete", "type_policy": "Retain exact EDS wire width and signedness; no conversion to another profile type.", "value_policy": "Position-completion time; unit/range not supplied. No EDS default or limits.", "write_policy": "not_writable"} |
| Conflicts | [] |
| Owner prompt | 22 |
| Semantic coverage D/I/H/Q | yes/no/no/no |
| Raw source claim | {"AccessType": "RW", "DataType": "0x0006", "ObjFlags": "0x00000000", "ObjectType": "7", "PDOMapping": "1", "ParameterName": "Obj.PosinTime"} |

### 0x250D:00

| Fact | Reviewed/source value |
| --- | --- |
| EDS name | Obj..AvrFiltime |
| Sources | EDS [250D] lines 1663-1670; EN86 PDF page 19; ZH86 PDF page 20 |
| Applicability | eds_family_unbound |
| Declaration disposition | resolved |
| Documentary disposition | pending_qualification |
| Exclusion reason | `null` (not stated) |
| Units | native tuning value |
| Scaling | No unqualified conversion or range inferred. |
| Original low / high / default | [null, null, null] |
| Effect | Changes native loop/current/tuning behavior; not a generic torque or autotune command. |
| Prerequisites | ["Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.", "Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates."] |
| Edge behavior | Not specified by the retained source. |
| Documentary review | {"gate_ids": ["Q01", "Q09", "Q20"], "generation_type": "resolved", "read_effect": "not_established", "status": "complete", "type_policy": "Retain exact EDS wire width and signedness; no conversion to another profile type.", "value_policy": "Factory filter/gain meaning is incomplete. No EDS default or limits.", "write_policy": "not_writable"} |
| Conflicts | [] |
| Owner prompt | 22 |
| Semantic coverage D/I/H/Q | yes/no/no/no |
| Raw source claim | {"AccessType": "RW", "DataType": "0x0006", "ObjFlags": "0x00000000", "ObjectType": "7", "PDOMapping": "1", "ParameterName": "Obj..AvrFiltime"} |

### 0x250E:00

| Fact | Reviewed/source value |
| --- | --- |
| EDS name | Obj.kpscale |
| Sources | EDS [250E] lines 1671-1678; EN86 PDF page 19; ZH86 PDF page 20 |
| Applicability | eds_family_unbound |
| Declaration disposition | resolved |
| Documentary disposition | pending_qualification |
| Exclusion reason | `null` (not stated) |
| Units | native tuning value |
| Scaling | No unqualified conversion or range inferred. |
| Original low / high / default | [null, null, null] |
| Effect | Changes native loop/current/tuning behavior; not a generic torque or autotune command. |
| Prerequisites | ["Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.", "Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates."] |
| Edge behavior | Not specified by the retained source. |
| Documentary review | {"gate_ids": ["Q01", "Q09", "Q20"], "generation_type": "resolved", "read_effect": "not_established", "status": "complete", "type_policy": "Retain exact EDS wire width and signedness; no conversion to another profile type.", "value_policy": "Factory filter/gain meaning is incomplete. No EDS default or limits.", "write_policy": "not_writable"} |
| Conflicts | [] |
| Owner prompt | 22 |
| Semantic coverage D/I/H/Q | yes/no/no/no |
| Raw source claim | {"AccessType": "RW", "DataType": "0x0006", "ObjFlags": "0x00000000", "ObjectType": "7", "PDOMapping": "1", "ParameterName": "Obj.kpscale"} |

### 0x250F:00

| Fact | Reviewed/source value |
| --- | --- |
| EDS name | Obj.kp |
| Sources | EDS [250F] lines 1679-1686; EN86 PDF page 19; ZH86 PDF page 20 |
| Applicability | eds_family_unbound |
| Declaration disposition | resolved |
| Documentary disposition | pending_qualification |
| Exclusion reason | `null` (not stated) |
| Units | native tuning value |
| Scaling | No unqualified conversion or range inferred. |
| Original low / high / default | [null, null, null] |
| Effect | Changes native loop/current/tuning behavior; not a generic torque or autotune command. |
| Prerequisites | ["Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.", "Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates."] |
| Edge behavior | Not specified by the retained source. |
| Documentary review | {"gate_ids": ["Q01", "Q09", "Q20"], "generation_type": "resolved", "read_effect": "not_established", "status": "complete", "type_policy": "Retain exact EDS wire width and signedness; no conversion to another profile type.", "value_policy": "Factory filter/gain meaning is incomplete. No EDS default or limits.", "write_policy": "not_writable"} |
| Conflicts | [] |
| Owner prompt | 22 |
| Semantic coverage D/I/H/Q | yes/no/no/no |
| Raw source claim | {"AccessType": "RW", "DataType": "0x0006", "ObjFlags": "0x00000000", "ObjectType": "7", "PDOMapping": "1", "ParameterName": "Obj.kp"} |

### 0x2510:00

| Fact | Reviewed/source value |
| --- | --- |
| EDS name | Obj.ki |
| Sources | EDS [2510] lines 1687-1694; EN86 PDF page 19; ZH86 PDF page 20 |
| Applicability | eds_family_unbound |
| Declaration disposition | resolved |
| Documentary disposition | pending_qualification |
| Exclusion reason | `null` (not stated) |
| Units | native tuning value |
| Scaling | No unqualified conversion or range inferred. |
| Original low / high / default | [null, null, null] |
| Effect | Changes native loop/current/tuning behavior; not a generic torque or autotune command. |
| Prerequisites | ["Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.", "Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates."] |
| Edge behavior | Not specified by the retained source. |
| Documentary review | {"gate_ids": ["Q01", "Q09", "Q20"], "generation_type": "resolved", "read_effect": "not_established", "status": "complete", "type_policy": "Retain exact EDS wire width and signedness; no conversion to another profile type.", "value_policy": "Factory filter/gain meaning is incomplete. No EDS default or limits.", "write_policy": "not_writable"} |
| Conflicts | [] |
| Owner prompt | 22 |
| Semantic coverage D/I/H/Q | yes/no/no/no |
| Raw source claim | {"AccessType": "RW", "DataType": "0x0006", "ObjFlags": "0x00000000", "ObjectType": "7", "PDOMapping": "1", "ParameterName": "Obj.ki"} |

### 0x2511:00

| Fact | Reviewed/source value |
| --- | --- |
| EDS name | Obj.kc |
| Sources | EDS [2511] lines 1695-1702; EN86 PDF page 19; ZH86 PDF page 20 |
| Applicability | eds_family_unbound |
| Declaration disposition | resolved |
| Documentary disposition | pending_qualification |
| Exclusion reason | `null` (not stated) |
| Units | native tuning value |
| Scaling | No unqualified conversion or range inferred. |
| Original low / high / default | [null, null, null] |
| Effect | Changes native loop/current/tuning behavior; not a generic torque or autotune command. |
| Prerequisites | ["Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.", "Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates."] |
| Edge behavior | Not specified by the retained source. |
| Documentary review | {"gate_ids": ["Q01", "Q09", "Q20"], "generation_type": "resolved", "read_effect": "not_established", "status": "complete", "type_policy": "Retain exact EDS wire width and signedness; no conversion to another profile type.", "value_policy": "Factory filter/gain meaning is incomplete. No EDS default or limits.", "write_policy": "not_writable"} |
| Conflicts | [] |
| Owner prompt | 22 |
| Semantic coverage D/I/H/Q | yes/no/no/no |
| Raw source claim | {"AccessType": "RW", "DataType": "0x0006", "ObjFlags": "0x00000000", "ObjectType": "7", "PDOMapping": "1", "ParameterName": "Obj.kc"} |

### 0x2512:00

| Fact | Reviewed/source value |
| --- | --- |
| EDS name | Obj.an_kp1 |
| Sources | EDS [2512] lines 1703-1710; EN86 PDF page 19; ZH86 PDF page 20 |
| Applicability | eds_family_unbound |
| Declaration disposition | resolved |
| Documentary disposition | pending_qualification |
| Exclusion reason | `null` (not stated) |
| Units | native tuning value |
| Scaling | No unqualified conversion or range inferred. |
| Original low / high / default | [null, null, null] |
| Effect | Changes native loop/current/tuning behavior; not a generic torque or autotune command. |
| Prerequisites | ["Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.", "Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates."] |
| Edge behavior | Not specified by the retained source. |
| Documentary review | {"gate_ids": ["Q01", "Q09", "Q20"], "generation_type": "resolved", "read_effect": "not_established", "status": "complete", "type_policy": "Retain exact EDS wire width and signedness; no conversion to another profile type.", "value_policy": "Factory filter/gain meaning is incomplete. No EDS default or limits.", "write_policy": "not_writable"} |
| Conflicts | [] |
| Owner prompt | 22 |
| Semantic coverage D/I/H/Q | yes/no/no/no |
| Raw source claim | {"AccessType": "RW", "DataType": "0x0006", "ObjFlags": "0x00000000", "ObjectType": "7", "PDOMapping": "1", "ParameterName": "Obj.an_kp1"} |

### 0x2513:00

| Fact | Reviewed/source value |
| --- | --- |
| EDS name | Obj.an_kv1 |
| Sources | EDS [2513] lines 1711-1718; EN86 PDF page 19; ZH86 PDF page 20 |
| Applicability | eds_family_unbound |
| Declaration disposition | resolved |
| Documentary disposition | pending_qualification |
| Exclusion reason | `null` (not stated) |
| Units | native tuning value |
| Scaling | No unqualified conversion or range inferred. |
| Original low / high / default | [null, null, null] |
| Effect | Changes native loop/current/tuning behavior; not a generic torque or autotune command. |
| Prerequisites | ["Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.", "Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates."] |
| Edge behavior | Not specified by the retained source. |
| Documentary review | {"gate_ids": ["Q01", "Q09", "Q20"], "generation_type": "resolved", "read_effect": "not_established", "status": "complete", "type_policy": "Retain exact EDS wire width and signedness; no conversion to another profile type.", "value_policy": "Factory filter/gain meaning is incomplete. No EDS default or limits.", "write_policy": "not_writable"} |
| Conflicts | [] |
| Owner prompt | 22 |
| Semantic coverage D/I/H/Q | yes/no/no/no |
| Raw source claim | {"AccessType": "RW", "DataType": "0x0006", "ObjFlags": "0x00000000", "ObjectType": "7", "PDOMapping": "1", "ParameterName": "Obj.an_kv1"} |

### 0x2514:00

| Fact | Reviewed/source value |
| --- | --- |
| EDS name | Obj.an_spd1 |
| Sources | EDS [2514] lines 1719-1726; EN86 PDF page 19; ZH86 PDF page 20 |
| Applicability | eds_family_unbound |
| Declaration disposition | resolved |
| Documentary disposition | pending_qualification |
| Exclusion reason | `null` (not stated) |
| Units | native tuning value |
| Scaling | No unqualified conversion or range inferred. |
| Original low / high / default | [null, null, null] |
| Effect | Changes native loop/current/tuning behavior; not a generic torque or autotune command. |
| Prerequisites | ["Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.", "Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates."] |
| Edge behavior | Not specified by the retained source. |
| Documentary review | {"gate_ids": ["Q01", "Q09", "Q20"], "generation_type": "resolved", "read_effect": "not_established", "status": "complete", "type_policy": "Retain exact EDS wire width and signedness; no conversion to another profile type.", "value_policy": "Factory filter/gain meaning is incomplete. No EDS default or limits.", "write_policy": "not_writable"} |
| Conflicts | [] |
| Owner prompt | 22 |
| Semantic coverage D/I/H/Q | yes/no/no/no |
| Raw source claim | {"AccessType": "RW", "DataType": "0x0006", "ObjFlags": "0x00000000", "ObjectType": "7", "PDOMapping": "1", "ParameterName": "Obj.an_spd1"} |

### 0x2515:00

| Fact | Reviewed/source value |
| --- | --- |
| EDS name | Obj.an_kp2 |
| Sources | EDS [2515] lines 1727-1734; EN86 PDF page 19; ZH86 PDF page 20 |
| Applicability | eds_family_unbound |
| Declaration disposition | resolved |
| Documentary disposition | pending_qualification |
| Exclusion reason | `null` (not stated) |
| Units | native tuning value |
| Scaling | No unqualified conversion or range inferred. |
| Original low / high / default | [null, null, null] |
| Effect | Changes native loop/current/tuning behavior; not a generic torque or autotune command. |
| Prerequisites | ["Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.", "Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates."] |
| Edge behavior | Not specified by the retained source. |
| Documentary review | {"gate_ids": ["Q01", "Q09", "Q20"], "generation_type": "resolved", "read_effect": "not_established", "status": "complete", "type_policy": "Retain exact EDS wire width and signedness; no conversion to another profile type.", "value_policy": "Factory filter/gain meaning is incomplete. No EDS default or limits.", "write_policy": "not_writable"} |
| Conflicts | [] |
| Owner prompt | 22 |
| Semantic coverage D/I/H/Q | yes/no/no/no |
| Raw source claim | {"AccessType": "RW", "DataType": "0x0006", "ObjFlags": "0x00000000", "ObjectType": "7", "PDOMapping": "1", "ParameterName": "Obj.an_kp2"} |

### 0x2516:00

| Fact | Reviewed/source value |
| --- | --- |
| EDS name | Obj.an_kv2 |
| Sources | EDS [2516] lines 1735-1742; EN86 PDF page 19; ZH86 PDF page 20 |
| Applicability | eds_family_unbound |
| Declaration disposition | resolved |
| Documentary disposition | pending_qualification |
| Exclusion reason | `null` (not stated) |
| Units | native tuning value |
| Scaling | No unqualified conversion or range inferred. |
| Original low / high / default | [null, null, null] |
| Effect | Changes native loop/current/tuning behavior; not a generic torque or autotune command. |
| Prerequisites | ["Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.", "Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates."] |
| Edge behavior | Not specified by the retained source. |
| Documentary review | {"gate_ids": ["Q01", "Q09", "Q20"], "generation_type": "resolved", "read_effect": "not_established", "status": "complete", "type_policy": "Retain exact EDS wire width and signedness; no conversion to another profile type.", "value_policy": "Factory filter/gain meaning is incomplete. No EDS default or limits.", "write_policy": "not_writable"} |
| Conflicts | [] |
| Owner prompt | 22 |
| Semantic coverage D/I/H/Q | yes/no/no/no |
| Raw source claim | {"AccessType": "RW", "DataType": "0x0006", "ObjFlags": "0x00000000", "ObjectType": "7", "PDOMapping": "1", "ParameterName": "Obj.an_kv2"} |

### 0x2517:00

| Fact | Reviewed/source value |
| --- | --- |
| EDS name | Obj.an_spd2 |
| Sources | EDS [2517] lines 1743-1750; EN86 PDF page 20; ZH86 PDF page 20 |
| Applicability | eds_family_unbound |
| Declaration disposition | resolved |
| Documentary disposition | pending_qualification |
| Exclusion reason | `null` (not stated) |
| Units | native tuning value |
| Scaling | No unqualified conversion or range inferred. |
| Original low / high / default | [null, null, null] |
| Effect | Changes native loop/current/tuning behavior; not a generic torque or autotune command. |
| Prerequisites | ["Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.", "Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates."] |
| Edge behavior | Not specified by the retained source. |
| Documentary review | {"gate_ids": ["Q01", "Q09", "Q20"], "generation_type": "resolved", "read_effect": "not_established", "status": "complete", "type_policy": "Retain exact EDS wire width and signedness; no conversion to another profile type.", "value_policy": "Factory filter/gain meaning is incomplete. No EDS default or limits.", "write_policy": "not_writable"} |
| Conflicts | [] |
| Owner prompt | 22 |
| Semantic coverage D/I/H/Q | yes/no/no/no |
| Raw source claim | {"AccessType": "RW", "DataType": "0x0006", "ObjFlags": "0x00000000", "ObjectType": "7", "PDOMapping": "1", "ParameterName": "Obj.an_spd2"} |

### 0x2518:00

| Fact | Reviewed/source value |
| --- | --- |
| EDS name | Obj.an_kvff |
| Sources | EDS [2518] lines 1751-1758; EN86 PDF page 20; ZH86 PDF page 20 |
| Applicability | eds_family_unbound |
| Declaration disposition | resolved |
| Documentary disposition | pending_qualification |
| Exclusion reason | `null` (not stated) |
| Units | native tuning value |
| Scaling | No unqualified conversion or range inferred. |
| Original low / high / default | [null, null, null] |
| Effect | Changes native loop/current/tuning behavior; not a generic torque or autotune command. |
| Prerequisites | ["Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.", "Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates."] |
| Edge behavior | Not specified by the retained source. |
| Documentary review | {"gate_ids": ["Q01", "Q09", "Q20"], "generation_type": "resolved", "read_effect": "not_established", "status": "complete", "type_policy": "Retain exact EDS wire width and signedness; no conversion to another profile type.", "value_policy": "Factory filter/gain meaning is incomplete. No EDS default or limits.", "write_policy": "not_writable"} |
| Conflicts | [] |
| Owner prompt | 22 |
| Semantic coverage D/I/H/Q | yes/no/no/no |
| Raw source claim | {"AccessType": "RW", "DataType": "0x0006", "ObjFlags": "0x00000000", "ObjectType": "7", "PDOMapping": "1", "ParameterName": "Obj.an_kvff"} |

### 0x2519:00

| Fact | Reviewed/source value |
| --- | --- |
| EDS name | Obj.an_ki |
| Sources | EDS [2519] lines 1759-1766; EN86 PDF page 20; ZH86 PDF page 20 |
| Applicability | eds_family_unbound |
| Declaration disposition | resolved |
| Documentary disposition | pending_qualification |
| Exclusion reason | `null` (not stated) |
| Units | native tuning value |
| Scaling | No unqualified conversion or range inferred. |
| Original low / high / default | [null, null, null] |
| Effect | Changes native loop/current/tuning behavior; not a generic torque or autotune command. |
| Prerequisites | ["Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.", "Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates."] |
| Edge behavior | Not specified by the retained source. |
| Documentary review | {"gate_ids": ["Q01", "Q09", "Q20"], "generation_type": "resolved", "read_effect": "not_established", "status": "complete", "type_policy": "Retain exact EDS wire width and signedness; no conversion to another profile type.", "value_policy": "Factory filter/gain meaning is incomplete. No EDS default or limits.", "write_policy": "not_writable"} |
| Conflicts | [] |
| Owner prompt | 22 |
| Semantic coverage D/I/H/Q | yes/no/no/no |
| Raw source claim | {"AccessType": "RW", "DataType": "0x0006", "ObjFlags": "0x00000000", "ObjectType": "7", "PDOMapping": "1", "ParameterName": "Obj.an_ki"} |

### 0x251A:00

| Fact | Reviewed/source value |
| --- | --- |
| EDS name | Obj.v_kp |
| Sources | EDS [251A] lines 1767-1774 |
| Applicability | eds_family_unbound |
| Declaration disposition | resolved |
| Documentary disposition | pending_qualification |
| Exclusion reason | `null` (not stated) |
| Units | native tuning value |
| Scaling | No unqualified conversion or range inferred. |
| Original low / high / default | [null, null, null] |
| Effect | Unknown physical effect; a symbolic EDS name is not sufficient evidence for a tuning write. |
| Prerequisites | ["Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.", "Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates."] |
| Edge behavior | Not specified by the retained source. |
| Documentary review | {"gate_ids": ["Q01", "Q09", "Q20"], "generation_type": "resolved", "read_effect": "not_established", "status": "complete", "type_policy": "Retain exact EDS wire width and signedness; no conversion to another profile type.", "value_policy": "EDS-only symbolic name; no documented physical meaning. No EDS default or limits.", "write_policy": "not_writable"} |
| Conflicts | [] |
| Owner prompt | 22 |
| Semantic coverage D/I/H/Q | yes/no/no/no |
| Raw source claim | {"AccessType": "RW", "DataType": "0x0006", "ObjFlags": "0x00000000", "ObjectType": "7", "PDOMapping": "1", "ParameterName": "Obj.v_kp"} |

### 0x251B:00

| Fact | Reviewed/source value |
| --- | --- |
| EDS name | Obj.v_ki |
| Sources | EDS [251B] lines 1775-1782 |
| Applicability | eds_family_unbound |
| Declaration disposition | resolved |
| Documentary disposition | pending_qualification |
| Exclusion reason | `null` (not stated) |
| Units | native tuning value |
| Scaling | No unqualified conversion or range inferred. |
| Original low / high / default | [null, null, null] |
| Effect | Unknown physical effect; a symbolic EDS name is not sufficient evidence for a tuning write. |
| Prerequisites | ["Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.", "Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates."] |
| Edge behavior | Not specified by the retained source. |
| Documentary review | {"gate_ids": ["Q01", "Q09", "Q20"], "generation_type": "resolved", "read_effect": "not_established", "status": "complete", "type_policy": "Retain exact EDS wire width and signedness; no conversion to another profile type.", "value_policy": "EDS-only symbolic name; no documented physical meaning. No EDS default or limits.", "write_policy": "not_writable"} |
| Conflicts | [] |
| Owner prompt | 22 |
| Semantic coverage D/I/H/Q | yes/no/no/no |
| Raw source claim | {"AccessType": "RW", "DataType": "0x0006", "ObjFlags": "0x00000000", "ObjectType": "7", "PDOMapping": "1", "ParameterName": "Obj.v_ki"} |

### 0x251C:00

| Fact | Reviewed/source value |
| --- | --- |
| EDS name | Obj.v_kc |
| Sources | EDS [251C] lines 1783-1790 |
| Applicability | eds_family_unbound |
| Declaration disposition | resolved |
| Documentary disposition | pending_qualification |
| Exclusion reason | `null` (not stated) |
| Units | native tuning value |
| Scaling | No unqualified conversion or range inferred. |
| Original low / high / default | [null, null, null] |
| Effect | Unknown physical effect; a symbolic EDS name is not sufficient evidence for a tuning write. |
| Prerequisites | ["Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.", "Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates."] |
| Edge behavior | Not specified by the retained source. |
| Documentary review | {"gate_ids": ["Q01", "Q09", "Q20"], "generation_type": "resolved", "read_effect": "not_established", "status": "complete", "type_policy": "Retain exact EDS wire width and signedness; no conversion to another profile type.", "value_policy": "EDS-only symbolic name; no documented physical meaning. No EDS default or limits.", "write_policy": "not_writable"} |
| Conflicts | [] |
| Owner prompt | 22 |
| Semantic coverage D/I/H/Q | yes/no/no/no |
| Raw source claim | {"AccessType": "RW", "DataType": "0x0006", "ObjFlags": "0x00000000", "ObjectType": "7", "PDOMapping": "1", "ParameterName": "Obj.v_kc"} |

### 0x251D:00

| Fact | Reviewed/source value |
| --- | --- |
| EDS name | Obj.v_kvfbd1 |
| Sources | EDS [251D] lines 1791-1798 |
| Applicability | eds_family_unbound |
| Declaration disposition | resolved |
| Documentary disposition | pending_qualification |
| Exclusion reason | `null` (not stated) |
| Units | native tuning value |
| Scaling | No unqualified conversion or range inferred. |
| Original low / high / default | [null, null, null] |
| Effect | Unknown physical effect; a symbolic EDS name is not sufficient evidence for a tuning write. |
| Prerequisites | ["Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.", "Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates."] |
| Edge behavior | Not specified by the retained source. |
| Documentary review | {"gate_ids": ["Q01", "Q09", "Q20"], "generation_type": "resolved", "read_effect": "not_established", "status": "complete", "type_policy": "Retain exact EDS wire width and signedness; no conversion to another profile type.", "value_policy": "EDS-only symbolic name; no documented physical meaning. No EDS default or limits.", "write_policy": "not_writable"} |
| Conflicts | [] |
| Owner prompt | 22 |
| Semantic coverage D/I/H/Q | yes/no/no/no |
| Raw source claim | {"AccessType": "RW", "DataType": "0x0006", "ObjFlags": "0x00000000", "ObjectType": "7", "PDOMapping": "1", "ParameterName": "Obj.v_kvfbd1"} |

### 0x251E:00

| Fact | Reviewed/source value |
| --- | --- |
| EDS name | Obj.v_kvfbd2 |
| Sources | EDS [251E] lines 1799-1806 |
| Applicability | eds_family_unbound |
| Declaration disposition | resolved |
| Documentary disposition | pending_qualification |
| Exclusion reason | `null` (not stated) |
| Units | native tuning value |
| Scaling | No unqualified conversion or range inferred. |
| Original low / high / default | [null, null, null] |
| Effect | Unknown physical effect; a symbolic EDS name is not sufficient evidence for a tuning write. |
| Prerequisites | ["Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.", "Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates."] |
| Edge behavior | Not specified by the retained source. |
| Documentary review | {"gate_ids": ["Q01", "Q09", "Q20"], "generation_type": "resolved", "read_effect": "not_established", "status": "complete", "type_policy": "Retain exact EDS wire width and signedness; no conversion to another profile type.", "value_policy": "EDS-only symbolic name; no documented physical meaning. No EDS default or limits.", "write_policy": "not_writable"} |
| Conflicts | [] |
| Owner prompt | 22 |
| Semantic coverage D/I/H/Q | yes/no/no/no |
| Raw source claim | {"AccessType": "RW", "DataType": "0x0006", "ObjFlags": "0x00000000", "ObjectType": "7", "PDOMapping": "1", "ParameterName": "Obj.v_kvfbd2"} |

### 0x251F:00

| Fact | Reviewed/source value |
| --- | --- |
| EDS name | Obj.v_kvff |
| Sources | EDS [251F] lines 1807-1814 |
| Applicability | eds_family_unbound |
| Declaration disposition | resolved |
| Documentary disposition | pending_qualification |
| Exclusion reason | `null` (not stated) |
| Units | native tuning value |
| Scaling | No unqualified conversion or range inferred. |
| Original low / high / default | [null, null, null] |
| Effect | Unknown physical effect; a symbolic EDS name is not sufficient evidence for a tuning write. |
| Prerequisites | ["Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.", "Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates."] |
| Edge behavior | Not specified by the retained source. |
| Documentary review | {"gate_ids": ["Q01", "Q09", "Q20"], "generation_type": "resolved", "read_effect": "not_established", "status": "complete", "type_policy": "Retain exact EDS wire width and signedness; no conversion to another profile type.", "value_policy": "EDS-only symbolic name; no documented physical meaning. No EDS default or limits.", "write_policy": "not_writable"} |
| Conflicts | [] |
| Owner prompt | 22 |
| Semantic coverage D/I/H/Q | yes/no/no/no |
| Raw source claim | {"AccessType": "RW", "DataType": "0x0006", "ObjFlags": "0x00000000", "ObjectType": "7", "PDOMapping": "1", "ParameterName": "Obj.v_kvff"} |

### 0x2520:00

| Fact | Reviewed/source value |
| --- | --- |
| EDS name | Obj.v_kacff |
| Sources | EDS [2520] lines 1815-1822 |
| Applicability | eds_family_unbound |
| Declaration disposition | resolved |
| Documentary disposition | pending_qualification |
| Exclusion reason | `null` (not stated) |
| Units | native tuning value |
| Scaling | No unqualified conversion or range inferred. |
| Original low / high / default | [null, null, null] |
| Effect | Unknown physical effect; a symbolic EDS name is not sufficient evidence for a tuning write. |
| Prerequisites | ["Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.", "Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates."] |
| Edge behavior | Not specified by the retained source. |
| Documentary review | {"gate_ids": ["Q01", "Q09", "Q20"], "generation_type": "resolved", "read_effect": "not_established", "status": "complete", "type_policy": "Retain exact EDS wire width and signedness; no conversion to another profile type.", "value_policy": "EDS-only symbolic name; no documented physical meaning. No EDS default or limits.", "write_policy": "not_writable"} |
| Conflicts | [] |
| Owner prompt | 22 |
| Semantic coverage D/I/H/Q | yes/no/no/no |
| Raw source claim | {"AccessType": "RW", "DataType": "0x0006", "ObjFlags": "0x00000000", "ObjectType": "7", "PDOMapping": "1", "ParameterName": "Obj.v_kacff"} |

### 0x603F:00

| Fact | Reviewed/source value |
| --- | --- |
| EDS name | DSP Error Code |
| Sources | EDS [603F] lines 1823-1831; EN86 PDF page 16; ZH86 PDF page 16 |
| Applicability | eds_family_unbound |
| Declaration disposition | resolved |
| Documentary disposition | conservative_read_only |
| Exclusion reason | `null` (not stated) |
| Units | raw error code |
| Scaling | Preserve unknown codes. |
| Original low / high / default | [null, null, "0"] |
| Effect | Read active drive error; never clear by writing this object. |
| Prerequisites | ["Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.", "Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates."] |
| Edge behavior | Not specified by the retained source. |
| Documentary review | {"gate_ids": ["Q01", "Q20", "Q13"], "generation_type": "resolved", "read_effect": "not_established", "status": "complete", "type_policy": "Retain exact EDS wire width and signedness; no conversion to another profile type.", "value_policy": "Manual FF00..FFFF, FF01 overcurrent, FF05 position deviation; EDS default0 remains metadata.", "write_policy": "not_writable"} |
| Conflicts | ["C07"] |
| Owner prompt | 19 |
| Semantic coverage D/I/H/Q | yes/no/no/no |
| Raw source claim | {"AccessType": "RO", "DataType": "0x0006", "DefaultValue": "0", "ObjFlags": "0x00000000", "ObjectType": "7", "PDOMapping": "1", "ParameterName": "DSP Error Code"} |

### 0x6040:00

| Fact | Reviewed/source value |
| --- | --- |
| EDS name | ControlWord |
| Sources | EDS [6040] lines 1832-1840; EN86 PDF page 11; EN86 PDF page 12; EN86 PDF page 15; EN86 PDF page 16; ZH86 PDF page 12; ZH86 PDF page 15; ZH86 PDF page 17; OP86 PDF page 9; OP86 PDF page 10; OP86 PDF page 13 |
| Applicability | eds_family_unbound |
| Declaration disposition | resolved |
| Documentary disposition | pending_qualification |
| Exclusion reason | `null` (not stated) |
| Units | controlword bits |
| Scaling | Mode and current-state dependent. |
| Original low / high / default | [null, null, "0"] |
| Effect | Enable/release, trigger motion, stop or reset through separate explicit operations. |
| Prerequisites | ["Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.", "Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates."] |
| Edge behavior | Bit4 rising target trigger in position mode; bit7 assertion/manual return for reset, edge unspecified. |
| Documentary review | {"gate_ids": ["Q01", "Q11", "Q12", "Q13", "Q03"], "generation_type": "resolved", "pdo_observation": {"directions": ["RPDO"], "live_qualification_required": true, "representation": "UNSIGNED", "role": "RAW", "source_refs": [{"id": "EDS", "line_end": 1840, "line_start": 1832, "section": "6040"}, {"id": "EN86", "page": 11}, {"id": "EN86", "page": 12}, {"id": "EN86", "page": 15}, {"id": "EN86", "page": 16}, {"id": "ZH86", "page": 12}, {"id": "ZH86", "page": 15}, {"id": "ZH86", "page": 17}, {"id": "OP86", "page": 9}, {"id": "OP86", "page": 10}, {"id": "OP86", "page": 13}, {"id": "EDS", "line_end": 612, "line_start": 604, "section": "1600sub1"}]}, "read_effect": "not_established", "status": "complete", "type_policy": "Retain exact EDS wire width and signedness; no conversion to another profile type.", "value_policy": "6,7,15 enable example;15-&gt;271 normal stop;15-&gt;11 emergency stop;15-&gt;128 reset. Never enable as stop/reset preamble.", "write_policy": "qualification_required"} |
| Conflicts | ["C08", "C17"] |
| Owner prompt | 30 |
| Semantic coverage D/I/H/Q | yes/no/no/no |
| Raw source claim | {"AccessType": "WO", "DataType": "0x0006", "DefaultValue": "0", "ObjFlags": "0x00000000", "ObjectType": "7", "PDOMapping": "1", "ParameterName": "ControlWord"} |

### 0x6041:00

| Fact | Reviewed/source value |
| --- | --- |
| EDS name | StatusWord |
| Sources | EDS [6041] lines 1841-1849; EN86 PDF page 12; ZH86 PDF page 12; OP86 PDF page 10; ZH57 PDF page 12; OP57 PDF page 10 |
| Applicability | eds_family_unbound |
| Declaration disposition | resolved |
| Documentary disposition | conservative_read_only |
| Exclusion reason | `null` (not stated) |
| Units | statusword bits |
| Scaling | Three 86C layouts; no merged mask. |
| Original low / high / default | [null, null, "0"] |
| Effect | Observe raw status; interpreted fields depend on selected evidence. |
| Prerequisites | ["Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.", "Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates."] |
| Edge behavior | Not specified by the retained source. |
| Documentary review | {"gate_ids": ["Q01", "Q02", "Q20"], "generation_type": "resolved", "pdo_observation": {"directions": ["TPDO"], "live_qualification_required": true, "representation": "UNSIGNED", "role": "RAW", "source_refs": [{"id": "EDS", "line_end": 1849, "line_start": 1841, "section": "6041"}, {"id": "EN86", "page": 12}, {"id": "ZH86", "page": 12}, {"id": "OP86", "page": 10}, {"id": "ZH57", "page": 12}, {"id": "OP57", "page": 10}, {"id": "EDS", "line_end": 1053, "line_start": 1045, "section": "1A00sub1"}]}, "read_effect": "not_established", "status": "complete", "type_policy": "Retain exact EDS wire width and signedness; no conversion to another profile type.", "value_policy": "See status decision table; preserve raw word and per-field validity.", "write_policy": "not_writable"} |
| Conflicts | ["C05"] |
| Owner prompt | 19 |
| Semantic coverage D/I/H/Q | yes/no/no/no |
| Raw source claim | {"AccessType": "RO", "DataType": "0x0006", "DefaultValue": "0", "ObjFlags": "0x00000000", "ObjectType": "7", "PDOMapping": "1", "ParameterName": "StatusWord"} |

### 0x605D:00

| Fact | Reviewed/source value |
| --- | --- |
| EDS name | Halt option code |
| Sources | EDS [605D] lines 1850-1858; EN86 PDF page 16; ZH86 PDF page 17 |
| Applicability | eds_family_unbound |
| Declaration disposition | resolved |
| Documentary disposition | pending_qualification |
| Exclusion reason | `null` (not stated) |
| Units | halt option code |
| Scaling | I16 native; options not established. |
| Original low / high / default | [null, null, "1"] |
| Effect | Halt/stop behavior cannot be derived from default1. |
| Prerequisites | ["Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.", "Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates."] |
| Edge behavior | Not specified by the retained source. |
| Documentary review | {"gate_ids": ["Q01", "Q12", "Q20"], "generation_type": "resolved", "read_effect": "not_established", "status": "complete", "type_policy": "Retain exact EDS wire width and signedness; no conversion to another profile type.", "value_policy": "EDS default1; no usable option-value semantics. 605A/6085 absent.", "write_policy": "not_writable"} |
| Conflicts | [] |
| Owner prompt | 31 |
| Semantic coverage D/I/H/Q | yes/no/no/no |
| Raw source claim | {"AccessType": "RW", "DataType": "0x0003", "DefaultValue": "1", "ObjFlags": "0x00000000", "ObjectType": "7", "PDOMapping": "0", "ParameterName": "Halt option code"} |

### 0x6060:00

| Fact | Reviewed/source value |
| --- | --- |
| EDS name | Mode of operation |
| Sources | EDS [6060] lines 1859-1867; EN86 PDF page 11; EN86 PDF page 12; EN86 PDF page 16; ZH86 PDF page 11; ZH86 PDF page 17 |
| Applicability | eds_family_unbound |
| Declaration disposition | withheld |
| Documentary disposition | pending_qualification |
| Exclusion reason | `null` (not stated) |
| Units | mode code |
| Scaling | Raw byte; U8 EDS versus I8 manual. |
| Original low / high / default | [null, null, "0"] |
| Effect | Explicitly request motion mode. |
| Prerequisites | ["Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.", "Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates."] |
| Edge behavior | Not specified by the retained source. |
| Documentary review | {"gate_ids": ["Q01", "Q10", "Q11"], "generation_type": "withheld", "pdo_observation": {"directions": ["RPDO"], "live_qualification_required": true, "representation": "RAW", "role": "RAW", "source_refs": [{"id": "EDS", "line_end": 1867, "line_start": 1859, "section": "6060"}, {"id": "EN86", "page": 11}, {"id": "EN86", "page": 12}, {"id": "EN86", "page": 16}, {"id": "ZH86", "page": 11}, {"id": "ZH86", "page": 17}, {"id": "EDS", "line_end": 630, "line_start": 622, "section": "1600sub3"}]}, "read_effect": "not_established", "status": "complete", "type_policy": "Retain raw U8 byte; decode only documented positive codes shared by U8/I8. No negative-mode inference.", "value_policy": "Documented codes 0 undefined,1 position,3 velocity,6 homing; undefined0 is not an operating mode. No negative/other operating codes approved.", "write_policy": "qualification_required"} |
| Conflicts | ["C08", "C10"] |
| Owner prompt | 20 |
| Semantic coverage D/I/H/Q | yes/no/no/no |
| Raw source claim | {"AccessType": "WO", "DataType": "0x0005", "DefaultValue": "0", "ObjFlags": "0x00000000", "ObjectType": "7", "PDOMapping": "1", "ParameterName": "Mode of operation"} |

### 0x6061:00

| Fact | Reviewed/source value |
| --- | --- |
| EDS name | Mode of operation display |
| Sources | EDS [6061] lines 1868-1876; EN86 PDF page 11; EN86 PDF page 12; EN86 PDF page 16; ZH86 PDF page 11; ZH86 PDF page 17 |
| Applicability | eds_family_unbound |
| Declaration disposition | withheld |
| Documentary disposition | conservative_read_only |
| Exclusion reason | `null` (not stated) |
| Units | mode code |
| Scaling | Raw byte; U8 EDS versus I8 manual. |
| Original low / high / default | [null, null, "0"] |
| Effect | Observe actual mode; never write translated set instruction. |
| Prerequisites | ["Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.", "Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates."] |
| Edge behavior | Not specified by the retained source. |
| Documentary review | {"gate_ids": ["Q01", "Q10", "Q20"], "generation_type": "withheld", "pdo_observation": {"directions": ["TPDO"], "live_qualification_required": true, "representation": "RAW", "role": "RAW", "source_refs": [{"id": "EDS", "line_end": 1876, "line_start": 1868, "section": "6061"}, {"id": "EN86", "page": 11}, {"id": "EN86", "page": 12}, {"id": "EN86", "page": 16}, {"id": "ZH86", "page": 11}, {"id": "ZH86", "page": 17}, {"id": "EDS", "line_end": 1071, "line_start": 1063, "section": "1A00sub3"}]}, "read_effect": "not_established", "status": "complete", "type_policy": "Retain raw U8 byte; decode only documented positive codes shared by U8/I8. No negative-mode inference.", "value_policy": "Documented codes 0 undefined,1 position,3 velocity,6 homing; undefined0 is not an operating mode. No negative/other operating codes approved.", "write_policy": "not_writable"} |
| Conflicts | ["C09", "C10"] |
| Owner prompt | 19 |
| Semantic coverage D/I/H/Q | yes/no/no/no |
| Raw source claim | {"AccessType": "RO", "DataType": "0x0005", "DefaultValue": "0", "ObjFlags": "0x00000000", "ObjectType": "7", "PDOMapping": "1", "ParameterName": "Mode of operation display"} |

### 0x6064:00

| Fact | Reviewed/source value |
| --- | --- |
| EDS name | actual position |
| Sources | EDS [6064] lines 1877-1885; EN86 PDF page 3; EN86 PDF page 16; ZH86 PDF page 3; ZH86 PDF page 17; ZH57 PDF page 3; ZH57 PDF page 17 |
| Applicability | eds_family_unbound |
| Declaration disposition | resolved |
| Documentary disposition | conservative_read_only |
| Exclusion reason | `null` (not stated) |
| Units | raw actual-position count |
| Scaling | Feedback scale/sign independent of command pulse/rpm conventions. |
| Original low / high / default | [null, null, "0"] |
| Effect | Observe feedback value; open-loop model does not establish measured rotor position. |
| Prerequisites | ["Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.", "Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates."] |
| Edge behavior | Not specified by the retained source. |
| Documentary review | {"gate_ids": ["Q01", "Q04", "Q20"], "generation_type": "resolved", "pdo_observation": {"directions": ["TPDO"], "live_qualification_required": true, "representation": "SIGNED", "role": "ACTUAL", "source_refs": [{"id": "EDS", "line_end": 1885, "line_start": 1877, "section": "6064"}, {"id": "EN86", "page": 3}, {"id": "EN86", "page": 16}, {"id": "ZH86", "page": 3}, {"id": "ZH86", "page": 17}, {"id": "ZH57", "page": 3}, {"id": "ZH57", "page": 17}, {"id": "EDS", "line_end": 1105, "line_start": 1097, "section": "1A01sub1"}]}, "read_effect": "not_established", "status": "complete", "type_policy": "Retain exact EDS wire width and signedness; no conversion to another profile type.", "value_policy": "I32 wire value; no physical conversion or accepted magnitude inferred.", "write_policy": "not_writable"} |
| Conflicts | [] |
| Owner prompt | 19 |
| Semantic coverage D/I/H/Q | yes/no/no/no |
| Raw source claim | {"AccessType": "RO", "DataType": "0x0004", "DefaultValue": "0", "ObjFlags": "0x00000000", "ObjectType": "7", "PDOMapping": "1", "ParameterName": "actual position"} |

### 0x606C:00

| Fact | Reviewed/source value |
| --- | --- |
| EDS name | actual speed |
| Sources | EDS [606C] lines 1886-1893; EN86 PDF page 3; EN86 PDF page 16; ZH86 PDF page 3; ZH86 PDF page 17; ZH57 PDF page 3; ZH57 PDF page 17 |
| Applicability | eds_family_unbound |
| Declaration disposition | resolved |
| Documentary disposition | conservative_read_only |
| Exclusion reason | `null` (not stated) |
| Units | raw actual-velocity value |
| Scaling | Feedback scale/sign independent of command pulse/rpm conventions. |
| Original low / high / default | [null, null, null] |
| Effect | Observe feedback value; open-loop model does not establish measured rotor position. |
| Prerequisites | ["Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.", "Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates."] |
| Edge behavior | Not specified by the retained source. |
| Documentary review | {"gate_ids": ["Q01", "Q04", "Q20"], "generation_type": "resolved", "pdo_observation": {"directions": ["TPDO"], "live_qualification_required": true, "representation": "SIGNED", "role": "ACTUAL", "source_refs": [{"id": "EDS", "line_end": 1893, "line_start": 1886, "section": "606C"}, {"id": "EN86", "page": 3}, {"id": "EN86", "page": 16}, {"id": "ZH86", "page": 3}, {"id": "ZH86", "page": 17}, {"id": "ZH57", "page": 3}, {"id": "ZH57", "page": 17}, {"id": "EDS", "line_end": 1114, "line_start": 1106, "section": "1A01sub2"}]}, "read_effect": "not_established", "status": "complete", "type_policy": "Retain exact EDS wire width and signedness; no conversion to another profile type.", "value_policy": "I32 wire value; no physical conversion or accepted magnitude inferred.", "write_policy": "not_writable"} |
| Conflicts | [] |
| Owner prompt | 19 |
| Semantic coverage D/I/H/Q | yes/no/no/no |
| Raw source claim | {"AccessType": "RO", "DataType": "0x0004", "ObjFlags": "0x00000000", "ObjectType": "7", "PDOMapping": "1", "ParameterName": "actual speed"} |

### 0x607A:00

| Fact | Reviewed/source value |
| --- | --- |
| EDS name | target position |
| Sources | EDS [607A] lines 1894-1902; EN86 PDF page 11; EN86 PDF page 16; ZH86 PDF page 11; ZH86 PDF page 17 |
| Applicability | eds_family_unbound |
| Declaration disposition | resolved |
| Documentary disposition | pending_qualification |
| Exclusion reason | `null` (not stated) |
| Units | command pulses |
| Scaling | Command scale must be bound separately from feedback. |
| Original low / high / default | [null, null, "5000"] |
| Effect | Sets position target; trigger and retained-target behavior belong to motion. |
| Prerequisites | ["Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.", "Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates."] |
| Edge behavior | Not specified by the retained source. |
| Documentary review | {"gate_ids": ["Q01", "Q04", "Q11", "Q20"], "generation_type": "resolved", "pdo_observation": {"directions": ["RPDO"], "live_qualification_required": true, "representation": "SIGNED", "role": "COMMANDED", "source_refs": [{"id": "EDS", "line_end": 1902, "line_start": 1894, "section": "607A"}, {"id": "EN86", "page": 11}, {"id": "EN86", "page": 16}, {"id": "ZH86", "page": 11}, {"id": "ZH86", "page": 17}, {"id": "EDS", "line_end": 664, "line_start": 656, "section": "1601sub1"}]}, "read_effect": "not_established", "status": "complete", "type_policy": "Retain exact EDS wire width and signedness; no conversion to another profile type.", "value_policy": "I32, documented -1000000..1000000; EDS default5000 is not an initial move. I3C dictionary label is malformed, use EDS and position table I32.", "write_policy": "qualification_required"} |
| Conflicts | ["C21"] |
| Owner prompt | 33 |
| Semantic coverage D/I/H/Q | yes/no/no/no |
| Raw source claim | {"AccessType": "RW", "DataType": "0x0004", "DefaultValue": "5000", "ObjFlags": "0x00000000", "ObjectType": "7", "PDOMapping": "1", "ParameterName": "target position"} |

### 0x607C:00

| Fact | Reviewed/source value |
| --- | --- |
| EDS name | home offset |
| Sources | EDS [607C] lines 1903-1911; EN86 PDF page 13; EN86 PDF page 16; ZH86 PDF page 13; ZH86 PDF page 17 |
| Applicability | eds_family_unbound |
| Declaration disposition | resolved |
| Documentary disposition | pending_qualification |
| Exclusion reason | `null` (not stated) |
| Units | raw native home offset |
| Scaling | Offset units, scale and reference application point not established. |
| Original low / high / default | [null, null, "0"] |
| Effect | Changes native home offset and dependent reference evidence. |
| Prerequisites | ["Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.", "Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates."] |
| Edge behavior | Not specified by the retained source. |
| Documentary review | {"gate_ids": ["Q01", "Q14", "Q21", "Q20"], "generation_type": "resolved", "read_effect": "not_established", "status": "complete", "type_policy": "Retain exact EDS wire width and signedness; no conversion to another profile type.", "value_policy": "I32, documented -1000000..1000000; no arbitrary set-position effect.", "write_policy": "qualification_required"} |
| Conflicts | [] |
| Owner prompt | 32 |
| Semantic coverage D/I/H/Q | yes/no/no/no |
| Raw source claim | {"AccessType": "RW", "DataType": "0x0004", "DefaultValue": "0", "ObjFlags": "0x00000000", "ObjectType": "7", "PDOMapping": "1", "ParameterName": "home offset"} |

### 0x607D:00

| Fact | Reviewed/source value |
| --- | --- |
| EDS name | HighestSubIndex |
| Sources | EDS [607Dsub0] lines 1918-1926 |
| Applicability | eds_family_unbound |
| Declaration disposition | resolved |
| Documentary disposition | supported_evidence |
| Exclusion reason | `null` (not stated) |
| Units | entry count |
| Scaling | Integer record count; retain this entry's own EDS width. |
| Original low / high / default | [null, null, "2"] |
| Effect | Observe the declared entry count; not the physical semantics of its children. |
| Prerequisites | ["Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.", "Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates."] |
| Edge behavior | Not specified by the retained source. |
| Documentary review | {"gate_ids": ["Q01", "Q20"], "generation_type": "resolved", "read_effect": "not_established", "status": "complete", "type_policy": "Retain exact EDS wire width and signedness; no conversion to another profile type.", "value_policy": "Count2.", "write_policy": "not_writable"} |
| Conflicts | [] |
| Owner prompt | 20 |
| Semantic coverage D/I/H/Q | yes/no/no/no |
| Raw source claim | {"AccessType": "RO", "DataType": "0x0005", "DefaultValue": "2", "ObjFlags": "0x00000000", "ObjectType": "7", "PDOMapping": "0", "ParameterName": "HighestSubIndex"} |

### 0x607D:01

| Fact | Reviewed/source value |
| --- | --- |
| EDS name | Soft MIN Limit |
| Sources | EDS [607Dsub1] lines 1927-1934; EN86 PDF page 16; ZH86 PDF page 17 |
| Applicability | eds_family_unbound |
| Declaration disposition | resolved |
| Documentary disposition | pending_qualification |
| Exclusion reason | `null` (not stated) |
| Units | raw native software-limit coordinate |
| Scaling | Units and coordinate scale are not established. |
| Original low / high / default | [null, null, null] |
| Effect | Changes negative/positive software travel bound. |
| Prerequisites | ["Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.", "Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates."] |
| Edge behavior | Not specified by the retained source. |
| Documentary review | {"gate_ids": ["Q01", "Q05", "Q20"], "generation_type": "resolved", "read_effect": "not_established", "status": "complete", "type_policy": "Retain exact EDS wire width and signedness; no conversion to another profile type.", "value_policy": "I32; operating bounds/defaults absent. Do not copy 607A/607C ranges. Minimum/maximum ordering and enforcement require qualification.", "write_policy": "qualification_required"} |
| Conflicts | [] |
| Owner prompt | 20 |
| Semantic coverage D/I/H/Q | yes/no/no/no |
| Raw source claim | {"AccessType": "RW", "DataType": "0x0004", "ObjFlags": "0x00000000", "ObjectType": "7", "PDOMapping": "1", "ParameterName": "Soft MIN Limit"} |

### 0x607D:02

| Fact | Reviewed/source value |
| --- | --- |
| EDS name | Soft MAX Limit |
| Sources | EDS [607Dsub2] lines 1935-1942; EN86 PDF page 17; ZH86 PDF page 17 |
| Applicability | eds_family_unbound |
| Declaration disposition | resolved |
| Documentary disposition | pending_qualification |
| Exclusion reason | `null` (not stated) |
| Units | raw native software-limit coordinate |
| Scaling | Units and coordinate scale are not established. |
| Original low / high / default | [null, null, null] |
| Effect | Changes negative/positive software travel bound. |
| Prerequisites | ["Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.", "Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates."] |
| Edge behavior | Not specified by the retained source. |
| Documentary review | {"gate_ids": ["Q01", "Q05", "Q20"], "generation_type": "resolved", "read_effect": "not_established", "status": "complete", "type_policy": "Retain exact EDS wire width and signedness; no conversion to another profile type.", "value_policy": "I32; operating bounds/defaults absent. Do not copy 607A/607C ranges. Minimum/maximum ordering and enforcement require qualification.", "write_policy": "qualification_required"} |
| Conflicts | [] |
| Owner prompt | 20 |
| Semantic coverage D/I/H/Q | yes/no/no/no |
| Raw source claim | {"AccessType": "RW", "DataType": "0x0004", "ObjFlags": "0x00000000", "ObjectType": "7", "PDOMapping": "1", "ParameterName": "Soft MAX Limit"} |

### 0x6081:00

| Fact | Reviewed/source value |
| --- | --- |
| EDS name | posmode speed |
| Sources | EDS [6081] lines 1943-1951; EN86 PDF page 11; EN86 PDF page 17; ZH86 PDF page 11; ZH86 PDF page 17 |
| Applicability | eds_family_unbound |
| Declaration disposition | resolved |
| Documentary disposition | pending_qualification |
| Exclusion reason | `null` (not stated) |
| Units | rpm |
| Scaling | Profile-specific U32 command speed; not generic common-axis width. |
| Original low / high / default | [null, null, "60"] |
| Effect | Sets position-profile speed. |
| Prerequisites | ["Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.", "Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates."] |
| Edge behavior | Not specified by the retained source. |
| Documentary review | {"gate_ids": ["Q01", "Q11", "Q20"], "generation_type": "resolved", "pdo_observation": {"directions": ["RPDO"], "live_qualification_required": true, "representation": "UNSIGNED", "role": "COMMANDED", "source_refs": [{"id": "EDS", "line_end": 1951, "line_start": 1943, "section": "6081"}, {"id": "EN86", "page": 11}, {"id": "EN86", "page": 17}, {"id": "ZH86", "page": 11}, {"id": "ZH86", "page": 17}, {"id": "EDS", "line_end": 673, "line_start": 665, "section": "1601sub2"}]}, "read_effect": "not_established", "status": "complete", "type_policy": "Retain exact EDS wire width and signedness; no conversion to another profile type.", "value_policy": "Documented 0..3000 rpm; zero-motion behavior is not established.", "write_policy": "qualification_required"} |
| Conflicts | [] |
| Owner prompt | 20 |
| Semantic coverage D/I/H/Q | yes/no/no/no |
| Raw source claim | {"AccessType": "RW", "DataType": "0x0007", "DefaultValue": "60", "ObjFlags": "0x00000000", "ObjectType": "7", "PDOMapping": "1", "ParameterName": "posmode speed"} |

### 0x6083:00

| Fact | Reviewed/source value |
| --- | --- |
| EDS name | accel time |
| Sources | EDS [6083] lines 1952-1960; EN86 PDF page 11; EN86 PDF page 17; ZH86 PDF page 11; ZH86 PDF page 17 |
| Applicability | eds_family_unbound |
| Declaration disposition | resolved |
| Documentary disposition | pending_qualification |
| Exclusion reason | `null` (not stated) |
| Units | ms |
| Scaling | Native ramp duration, not acceleration units. |
| Original low / high / default | [null, null, "100"] |
| Effect | Changes acceleration/deceleration ramp time. |
| Prerequisites | ["Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.", "Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates."] |
| Edge behavior | Not specified by the retained source. |
| Documentary review | {"gate_ids": ["Q01", "Q11", "Q20"], "generation_type": "resolved", "read_effect": "not_established", "status": "complete", "type_policy": "Retain exact EDS wire width and signedness; no conversion to another profile type.", "value_policy": "U32; documented 0..2000 ms; zero behavior unqualified. EDS default100 not initialization.", "write_policy": "qualification_required"} |
| Conflicts | [] |
| Owner prompt | 20 |
| Semantic coverage D/I/H/Q | yes/no/no/no |
| Raw source claim | {"AccessType": "RW", "DataType": "0x0007", "DefaultValue": "100", "ObjFlags": "0x00000000", "ObjectType": "7", "PDOMapping": "1", "ParameterName": "accel time"} |

### 0x6084:00

| Fact | Reviewed/source value |
| --- | --- |
| EDS name | decel time |
| Sources | EDS [6084] lines 1961-1969; EN86 PDF page 11; EN86 PDF page 17; ZH86 PDF page 11; ZH86 PDF page 17 |
| Applicability | eds_family_unbound |
| Declaration disposition | resolved |
| Documentary disposition | pending_qualification |
| Exclusion reason | `null` (not stated) |
| Units | ms |
| Scaling | Native ramp duration, not acceleration units. |
| Original low / high / default | [null, null, "100"] |
| Effect | Changes acceleration/deceleration ramp time. |
| Prerequisites | ["Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.", "Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates."] |
| Edge behavior | Not specified by the retained source. |
| Documentary review | {"gate_ids": ["Q01", "Q11", "Q20"], "generation_type": "resolved", "read_effect": "not_established", "status": "complete", "type_policy": "Retain exact EDS wire width and signedness; no conversion to another profile type.", "value_policy": "U32; documented 0..2000 ms; zero behavior unqualified. EDS default100 not initialization.", "write_policy": "qualification_required"} |
| Conflicts | [] |
| Owner prompt | 20 |
| Semantic coverage D/I/H/Q | yes/no/no/no |
| Raw source claim | {"AccessType": "RW", "DataType": "0x0007", "DefaultValue": "100", "ObjFlags": "0x00000000", "ObjectType": "7", "PDOMapping": "1", "ParameterName": "decel time"} |

### 0x6098:00

| Fact | Reviewed/source value |
| --- | --- |
| EDS name | homing method |
| Sources | EDS [6098] lines 1970-1978; EN86 PDF page 13; EN86 PDF page 14; EN86 PDF page 17; ZH86 PDF page 13; ZH86 PDF page 14; ZH86 PDF page 17 |
| Applicability | eds_family_unbound |
| Declaration disposition | withheld |
| Documentary disposition | pending_qualification |
| Exclusion reason | `null` (not stated) |
| Units | homing method code |
| Scaling | Raw byte; U8 EDS versus I8 dictionary. |
| Original low / high / default | [null, null, "0"] |
| Effect | Selects a physical homing procedure. |
| Prerequisites | ["Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.", "Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates."] |
| Edge behavior | Not specified by the retained source. |
| Documentary review | {"gate_ids": ["Q01", "Q10", "Q21", "Q20"], "generation_type": "withheld", "read_effect": "not_established", "status": "complete", "type_policy": "Retain raw byte; positive codes17/18/24/29 are wire-identical in U8/I8. Do not infer negative methods.", "value_policy": "Documented 17 negative limit,18 positive limit,24 positive origin,29 negative origin. Default0 not a supported method.", "write_policy": "qualification_required"} |
| Conflicts | ["C22"] |
| Owner prompt | 36 |
| Semantic coverage D/I/H/Q | yes/no/no/no |
| Raw source claim | {"AccessType": "RW", "DataType": "0x0005", "DefaultValue": "0", "ObjFlags": "0x00000000", "ObjectType": "7", "PDOMapping": "1", "ParameterName": "homing method"} |

### 0x6099:00

| Fact | Reviewed/source value |
| --- | --- |
| EDS name | HighestSubIndex |
| Sources | EDS [6099sub0] lines 1985-1993 |
| Applicability | eds_family_unbound |
| Declaration disposition | resolved |
| Documentary disposition | supported_evidence |
| Exclusion reason | `null` (not stated) |
| Units | entry count |
| Scaling | Integer record count; retain this entry's own EDS width. |
| Original low / high / default | [null, null, "2"] |
| Effect | Observe the declared entry count; not the physical semantics of its children. |
| Prerequisites | ["Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.", "Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates."] |
| Edge behavior | Not specified by the retained source. |
| Documentary review | {"gate_ids": ["Q01", "Q20"], "generation_type": "resolved", "read_effect": "not_established", "status": "complete", "type_policy": "Retain exact EDS wire width and signedness; no conversion to another profile type.", "value_policy": "Count2.", "write_policy": "not_writable"} |
| Conflicts | [] |
| Owner prompt | 36 |
| Semantic coverage D/I/H/Q | yes/no/no/no |
| Raw source claim | {"AccessType": "RO", "DataType": "0x0005", "DefaultValue": "2", "ObjFlags": "0x00000000", "ObjectType": "7", "PDOMapping": "0", "ParameterName": "HighestSubIndex"} |

### 0x6099:01

| Fact | Reviewed/source value |
| --- | --- |
| EDS name | homing speed |
| Sources | EDS [6099sub1] lines 1994-2002; EN86 PDF page 13; EN86 PDF page 14; EN86 PDF page 17; ZH86 PDF page 13; ZH86 PDF page 14; ZH86 PDF page 17 |
| Applicability | eds_family_unbound |
| Declaration disposition | resolved |
| Documentary disposition | pending_qualification |
| Exclusion reason | `null` (not stated) |
| Units | rpm |
| Scaling | U32 homing speed fields, not 16-bit values. |
| Original low / high / default | [null, null, "120"] |
| Effect | Sets search/creep speed for selected method. |
| Prerequisites | ["Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.", "Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates."] |
| Edge behavior | Not specified by the retained source. |
| Documentary review | {"gate_ids": ["Q01", "Q21", "Q20"], "generation_type": "resolved", "read_effect": "not_established", "status": "complete", "type_policy": "Retain exact EDS wire width and signedness; no conversion to another profile type.", "value_policy": "Documented 0..3000 rpm; sub2 is creep speed (rpm/prose), not a time. Zero behavior requires evidence.", "write_policy": "qualification_required"} |
| Conflicts | [] |
| Owner prompt | 36 |
| Semantic coverage D/I/H/Q | yes/no/no/no |
| Raw source claim | {"AccessType": "RW", "DataType": "0x0007", "DefaultValue": "120", "ObjFlags": "0x00000000", "ObjectType": "7", "PDOMapping": "1", "ParameterName": "homing speed"} |

### 0x6099:02

| Fact | Reviewed/source value |
| --- | --- |
| EDS name | homing search speed |
| Sources | EDS [6099sub2] lines 2003-2011; EN86 PDF page 13; EN86 PDF page 14; EN86 PDF page 17; ZH86 PDF page 13; ZH86 PDF page 14; ZH86 PDF page 17 |
| Applicability | eds_family_unbound |
| Declaration disposition | resolved |
| Documentary disposition | pending_qualification |
| Exclusion reason | `null` (not stated) |
| Units | rpm |
| Scaling | U32 homing speed fields, not 16-bit values. |
| Original low / high / default | [null, null, "60"] |
| Effect | Sets search/creep speed for selected method. |
| Prerequisites | ["Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.", "Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates."] |
| Edge behavior | Not specified by the retained source. |
| Documentary review | {"gate_ids": ["Q01", "Q21", "Q20"], "generation_type": "resolved", "read_effect": "not_established", "status": "complete", "type_policy": "Retain exact EDS wire width and signedness; no conversion to another profile type.", "value_policy": "Documented 0..3000 rpm; sub2 is creep speed (rpm/prose), not a time. Zero behavior requires evidence.", "write_policy": "qualification_required"} |
| Conflicts | ["C23"] |
| Owner prompt | 36 |
| Semantic coverage D/I/H/Q | yes/no/no/no |
| Raw source claim | {"AccessType": "RW", "DataType": "0x0007", "DefaultValue": "60", "ObjFlags": "0x00000000", "ObjectType": "7", "PDOMapping": "1", "ParameterName": "homing search speed"} |

### 0x609A:00

| Fact | Reviewed/source value |
| --- | --- |
| EDS name | homing accel/decel time |
| Sources | EDS [609A] lines 2012-2020; EN86 PDF page 13; EN86 PDF page 14; EN86 PDF page 17; ZH86 PDF page 13; ZH86 PDF page 14; ZH86 PDF page 17 |
| Applicability | eds_family_unbound |
| Declaration disposition | resolved |
| Documentary disposition | pending_qualification |
| Exclusion reason | `null` (not stated) |
| Units | ms |
| Scaling | Native ramp duration, not acceleration units. |
| Original low / high / default | [null, null, "100"] |
| Effect | Changes acceleration/deceleration ramp time. |
| Prerequisites | ["Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.", "Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates."] |
| Edge behavior | Not specified by the retained source. |
| Documentary review | {"gate_ids": ["Q01", "Q11", "Q21", "Q20"], "generation_type": "resolved", "read_effect": "not_established", "status": "complete", "type_policy": "Retain exact EDS wire width and signedness; no conversion to another profile type.", "value_policy": "U32; documented 0..2000 ms; zero behavior unqualified. EDS default100 not initialization.", "write_policy": "qualification_required"} |
| Conflicts | [] |
| Owner prompt | 36 |
| Semantic coverage D/I/H/Q | yes/no/no/no |
| Raw source claim | {"AccessType": "RW", "DataType": "0x0007", "DefaultValue": "100", "ObjFlags": "0x00000000", "ObjectType": "7", "PDOMapping": "1", "ParameterName": "homing accel/decel time"} |

### 0x60FD:00

| Fact | Reviewed/source value |
| --- | --- |
| EDS name | Digital inputs |
| Sources | EDS [60FD] lines 2021-2028; EN86 PDF page 17; ZH86 PDF page 17 |
| Applicability | eds_family_unbound |
| Declaration disposition | resolved |
| Documentary disposition | pending_qualification |
| Exclusion reason | `null` (not stated) |
| Units | input bit field |
| Scaling | No union of terminal numbering with 2101. |
| Original low / high / default | [null, null, null] |
| Effect | Observe digital inputs. |
| Prerequisites | ["Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.", "Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates."] |
| Edge behavior | Not specified by the retained source. |
| Documentary review | {"gate_ids": ["Q01", "Q19", "Q20"], "generation_type": "resolved", "pdo_observation": {"directions": ["TPDO"], "live_qualification_required": true, "representation": "UNSIGNED", "role": "RAW", "source_refs": [{"id": "EDS", "line_end": 2028, "line_start": 2021, "section": "60FD"}, {"id": "EN86", "page": 17}, {"id": "ZH86", "page": 17}, {"id": "EDS", "line_end": 1062, "line_start": 1054, "section": "1A00sub2"}]}, "read_effect": "not_established", "status": "complete", "type_policy": "Retain exact EDS wire width and signedness; no conversion to another profile type.", "value_policy": "Retain raw U32. Bit-to-function/terminal layout is not established by the cited dictionary rows; do not copy 2101 layout.", "write_policy": "not_writable"} |
| Conflicts | [] |
| Owner prompt | 19 |
| Semantic coverage D/I/H/Q | yes/no/no/no |
| Raw source claim | {"AccessType": "RO", "DataType": "0x0007", "ObjFlags": "0x00000000", "ObjectType": "7", "PDOMapping": "1", "ParameterName": "Digital inputs"} |

### 0x60FE:00

| Fact | Reviewed/source value |
| --- | --- |
| EDS name | HighestSubIndex |
| Sources | EDS [60FEsub0] lines 2035-2043 |
| Applicability | eds_family_unbound |
| Declaration disposition | resolved |
| Documentary disposition | supported_evidence |
| Exclusion reason | `null` (not stated) |
| Units | entry count |
| Scaling | Integer record count; retain this entry's own EDS width. |
| Original low / high / default | [null, null, "2"] |
| Effect | Observe the declared entry count; not the physical semantics of its children. |
| Prerequisites | ["Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.", "Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates."] |
| Edge behavior | Not specified by the retained source. |
| Documentary review | {"gate_ids": ["Q01", "Q20"], "generation_type": "resolved", "read_effect": "not_established", "status": "complete", "type_policy": "Retain exact EDS wire width and signedness; no conversion to another profile type.", "value_policy": "Count2.", "write_policy": "not_writable"} |
| Conflicts | [] |
| Owner prompt | 21 |
| Semantic coverage D/I/H/Q | yes/no/no/no |
| Raw source claim | {"AccessType": "RO", "DataType": "0x0005", "DefaultValue": "2", "ObjFlags": "0x00000000", "ObjectType": "7", "PDOMapping": "0", "ParameterName": "HighestSubIndex"} |

### 0x60FE:01

| Fact | Reviewed/source value |
| --- | --- |
| EDS name | Physical outputs |
| Sources | EDS [60FEsub1] lines 2044-2051; EN86 PDF page 17; ZH86 PDF page 17; OP86 PDF page 7; ZH57 PDF page 8; ZH57 PDF page 17 |
| Applicability | eds_family_unbound |
| Declaration disposition | resolved |
| Documentary disposition | pending_qualification |
| Exclusion reason | `null` (not stated) |
| Units | output bit field |
| Scaling | Native bits16/17/18, physical labels model-specific. |
| Original low / high / default | [null, null, null] |
| Effect | Commands output levels or changes enabling mask, potentially including brake. |
| Prerequisites | ["Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.", "Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates."] |
| Edge behavior | Not specified by the retained source. |
| Documentary review | {"gate_ids": ["Q01", "Q06", "Q07", "Q20"], "generation_type": "resolved", "read_effect": "not_established", "status": "complete", "type_policy": "Retain exact EDS wire width and signedness; no conversion to another profile type.", "value_policy": "Sub1 physical outputs, sub2 bit mask; OP86 requires both corresponding bits1. ZH57 labels conflict (DO0/1/2 versus DO1/2/3).", "write_policy": "not_writable"} |
| Conflicts | ["C03"] |
| Owner prompt | 21 |
| Semantic coverage D/I/H/Q | yes/no/no/no |
| Raw source claim | {"AccessType": "RW", "DataType": "0x0007", "ObjFlags": "0x00000000", "ObjectType": "7", "PDOMapping": "1", "ParameterName": "Physical outputs"} |

### 0x60FE:02

| Fact | Reviewed/source value |
| --- | --- |
| EDS name | Bit Mask |
| Sources | EDS [60FEsub2] lines 2052-2059; EN86 PDF page 17; ZH86 PDF page 17; OP86 PDF page 7; ZH57 PDF page 8; ZH57 PDF page 17 |
| Applicability | eds_family_unbound |
| Declaration disposition | resolved |
| Documentary disposition | pending_qualification |
| Exclusion reason | `null` (not stated) |
| Units | output bit field |
| Scaling | Native bits16/17/18, physical labels model-specific. |
| Original low / high / default | [null, null, null] |
| Effect | Commands output levels or changes enabling mask, potentially including brake. |
| Prerequisites | ["Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.", "Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates."] |
| Edge behavior | Not specified by the retained source. |
| Documentary review | {"gate_ids": ["Q01", "Q06", "Q07", "Q20"], "generation_type": "resolved", "read_effect": "not_established", "status": "complete", "type_policy": "Retain exact EDS wire width and signedness; no conversion to another profile type.", "value_policy": "Sub1 physical outputs, sub2 bit mask; OP86 requires both corresponding bits1. ZH57 labels conflict (DO0/1/2 versus DO1/2/3).", "write_policy": "not_writable"} |
| Conflicts | ["C03"] |
| Owner prompt | 21 |
| Semantic coverage D/I/H/Q | yes/no/no/no |
| Raw source claim | {"AccessType": "RW", "DataType": "0x0007", "ObjFlags": "0x00000000", "ObjectType": "7", "PDOMapping": "1", "ParameterName": "Bit Mask"} |

### 0x60FF:00

| Fact | Reviewed/source value |
| --- | --- |
| EDS name | velocity speed |
| Sources | EDS [60FF] lines 2060-2068; EN86 PDF page 13; EN86 PDF page 17; ZH86 PDF page 13; ZH86 PDF page 17; OP86 PDF page 11 |
| Applicability | eds_family_unbound |
| Declaration disposition | withheld |
| Documentary disposition | pending_qualification |
| Exclusion reason | `null` (not stated) |
| Units | rpm command |
| Scaling | EDS I32 versus procedure U32; no negative-speed approval. |
| Original low / high / default | [null, null, "120"] |
| Effect | Sets velocity target; may start on enable under Chinese sequence. |
| Prerequisites | ["Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.", "Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates."] |
| Edge behavior | Not specified by the retained source. |
| Documentary review | {"gate_ids": ["Q01", "Q03", "Q20"], "generation_type": "withheld", "pdo_observation": {"directions": ["RPDO"], "live_qualification_required": true, "representation": "RAW", "role": "COMMANDED", "source_refs": [{"id": "EDS", "line_end": 2068, "line_start": 2060, "section": "60FF"}, {"id": "EN86", "page": 13}, {"id": "EN86", "page": 17}, {"id": "ZH86", "page": 13}, {"id": "ZH86", "page": 17}, {"id": "OP86", "page": 11}, {"id": "EDS", "line_end": 621, "line_start": 613, "section": "1600sub2"}]}, "read_effect": "not_established", "status": "complete", "type_policy": "Preserve EDS I32 and contradictory U32 claim. Decode raw wire data; no typed signed motion until binding resolves Q03.", "value_policy": "Documentary common positive interval0..3000; negative encoding/start order unqualified.", "write_policy": "qualification_required"} |
| Conflicts | ["C11", "C17"] |
| Owner prompt | 35 |
| Semantic coverage D/I/H/Q | yes/no/no/no |
| Raw source claim | {"AccessType": "RW", "DataType": "0x0004", "DefaultValue": "120", "ObjFlags": "0x00000000", "ObjectType": "7", "PDOMapping": "1", "ParameterName": "velocity speed"} |

## Manual-only inventory

All source-reviewed manual items remain accounted for without declarations or implemented semantic functions. No second dictionary is synthesized.

### mode_selection

| Fact | Value |
| --- | --- |
| key | mode_selection |
| kind | semantic_function |
| scope_id | manual_86 |
| claim | 6060 modes 1 position, 3 velocity, 6 homing; 6061 is observed mode. The translated set instruction for 6061 is unresolved |
| related_entries | ["0x6060:00", "0x6061:00"] |
| source_refs | [{"id": "EN86", "page": 11}, {"id": "ZH86", "page": 11}] |
| confidence | documented_manual |
| conflict_ids | ["C08", "C09", "C10"] |
| owner_prompt | 20 |
| disposition | reviewed |
| coverage | {"documented": true, "hardware_qualified": false, "host_tested": false, "implemented": false} |
| review | {"decision": "Select6060 only; observe6061 under19. Typed groups never write6061.", "gate_ids": ["Q01", "Q10", "Q11"], "status": "complete", "write_policy": "qualification_required"} |

### enable_transition

| Fact | Value |
| --- | --- |
| key | enable_transition |
| kind | semantic_function |
| scope_id | manual_86 |
| claim | Controlword 6, 7, 15 is a state-transition example with approximately 10 ms between writes, not a universal startup script |
| related_entries | ["0x6040:00"] |
| source_refs | [{"id": "EN86", "page": 12}, {"id": "ZH86", "page": 12}, {"id": "OP86", "page": 10}, {"id": "OP86", "page": 9}] |
| confidence | documented_manual |
| conflict_ids | ["C08"] |
| owner_prompt | 30 |
| disposition | reviewed |
| coverage | {"documented": true, "hardware_qualified": false, "host_tested": false, "implemented": false} |
| review | {"decision": "State-transition example; no startup enable and no enable as stop/reset preamble.", "gate_ids": ["Q01", "Q11"], "status": "complete", "write_policy": "qualification_required"} |

### position_parameters

| Fact | Value |
| --- | --- |
| key | position_parameters |
| kind | semantic_function |
| scope_id | manual_86 |
| claim | Position target and profile velocity/ramp tables state native limits and milliseconds; pulse graph does not prove feedback scale |
| related_entries | ["0x607A:00", "0x6081:00", "0x6083:00", "0x6084:00"] |
| source_refs | [{"id": "EN86", "page": 11}, {"id": "ZH86", "page": 11}, {"id": "OP86", "page": 9}] |
| confidence | documented_manual |
| conflict_ids | ["C21"] |
| owner_prompt | 33 |
| disposition | reviewed |
| coverage | {"documented": true, "hardware_qualified": false, "host_tested": false, "implemented": false} |
| review | {"decision": "Documented position intent and bit meanings retained; qualify trigger, fresh completion and independent scale.", "gate_ids": ["Q01", "Q04", "Q11", "Q02"], "status": "complete", "write_policy": "qualification_required"} |

### position_trigger

| Fact | Value |
| --- | --- |
| key | position_trigger |
| kind | semantic_function |
| scope_id | manual_86 |
| claim | Controlword bit 4 rising edge is described as a new-target trigger |
| related_entries | ["0x6040:00"] |
| source_refs | [{"id": "EN86", "page": 12}, {"id": "ZH86", "page": 12}] |
| confidence | documented_manual |
| conflict_ids | [] |
| owner_prompt | 33 |
| disposition | reviewed |
| coverage | {"documented": true, "hardware_qualified": false, "host_tested": false, "implemented": false} |
| review | {"decision": "Documented position intent and bit meanings retained; qualify trigger, fresh completion and independent scale.", "gate_ids": ["Q01", "Q04", "Q11", "Q02"], "status": "complete", "write_policy": "qualification_required"} |

### position_update_policy

| Fact | Value |
| --- | --- |
| key | position_update_policy |
| kind | semantic_function |
| scope_id | manual_86 |
| claim | Controlword bit 5 distinguishes after-current-curve and immediate update |
| related_entries | ["0x6040:00"] |
| source_refs | [{"id": "EN86", "page": 12}, {"id": "ZH86", "page": 12}] |
| confidence | documented_manual |
| conflict_ids | [] |
| owner_prompt | 33 |
| disposition | reviewed |
| coverage | {"documented": true, "hardware_qualified": false, "host_tested": false, "implemented": false} |
| review | {"decision": "Documented position intent and bit meanings retained; qualify trigger, fresh completion and independent scale.", "gate_ids": ["Q01", "Q04", "Q11", "Q02"], "status": "complete", "write_policy": "qualification_required"} |

### position_coordinate_mode

| Fact | Value |
| --- | --- |
| key | position_coordinate_mode |
| kind | semantic_function |
| scope_id | manual_86 |
| claim | Controlword bit 6 distinguishes absolute and relative examples |
| related_entries | ["0x6040:00"] |
| source_refs | [{"id": "EN86", "page": 12}, {"id": "ZH86", "page": 12}] |
| confidence | documented_manual |
| conflict_ids | [] |
| owner_prompt | 33 |
| disposition | reviewed |
| coverage | {"documented": true, "hardware_qualified": false, "host_tested": false, "implemented": false} |
| review | {"decision": "Documented position intent and bit meanings retained; qualify trigger, fresh completion and independent scale.", "gate_ids": ["Q01", "Q04", "Q11", "Q02"], "status": "complete", "write_policy": "qualification_required"} |

### position_transition_examples

| Fact | Value |
| --- | --- |
| key | position_transition_examples |
| kind | semantic_function |
| scope_id | manual_86 |
| claim | Examples 15 to 31/95/63/127 combine trigger, coordinate and update bits; current state remains a prerequisite |
| related_entries | ["0x6040:00"] |
| source_refs | [{"id": "EN86", "page": 12}, {"id": "ZH86", "page": 12}, {"id": "OP86", "page": 10}] |
| confidence | documented_manual |
| conflict_ids | [] |
| owner_prompt | 33 |
| disposition | reviewed |
| coverage | {"documented": true, "hardware_qualified": false, "host_tested": false, "implemented": false} |
| review | {"decision": "Documented position intent and bit meanings retained; qualify trigger, fresh completion and independent scale.", "gate_ids": ["Q01", "Q04", "Q11", "Q02"], "status": "complete", "write_policy": "qualification_required"} |

### normal_stop

| Fact | Value |
| --- | --- |
| key | normal_stop |
| kind | semantic_function |
| scope_id | manual_86 |
| claim | 15 to 271 is called stop in Chinese/operating tables; missing from EN86 transition table; 605D options and stop effect remain unresolved |
| related_entries | ["0x6040:00", "0x605D:00"] |
| source_refs | [{"id": "ZH86", "page": 12}, {"id": "OP86", "page": 10}, {"id": "EN86", "page": 12}] |
| confidence | documented_manual |
| conflict_ids | [] |
| owner_prompt | 31 |
| disposition | reviewed |
| coverage | {"documented": true, "hardware_qualified": false, "host_tested": false, "implemented": false} |
| review | {"decision": "15-&gt;271 example is not a script beginning with15.", "gate_ids": ["Q01", "Q12"], "status": "complete", "write_policy": "qualification_required"} |

### emergency_stop

| Fact | Value |
| --- | --- |
| key | emergency_stop |
| kind | semantic_function |
| scope_id | manual_86 |
| claim | 15 to 11 is called emergency stop; no stop latency or safety rating is established |
| related_entries | ["0x6040:00"] |
| source_refs | [{"id": "EN86", "page": 12}, {"id": "ZH86", "page": 12}, {"id": "OP86", "page": 10}] |
| confidence | documented_manual |
| conflict_ids | [] |
| owner_prompt | 31 |
| disposition | reviewed |
| coverage | {"documented": true, "hardware_qualified": false, "host_tested": false, "implemented": false} |
| review | {"decision": "15-&gt;11 example is not a safety-rated stop or a script beginning with15.", "gate_ids": ["Q01", "Q12"], "status": "complete", "write_policy": "qualification_required"} |

### status_interpretation

| Fact | Value |
| --- | --- |
| key | status_interpretation |
| kind | semantic_function |
| scope_id | manual_86 |
| claim | Statusword bit definitions differ among the 86C manuals; preserve raw status and each source interpretation |
| related_entries | ["0x6041:00"] |
| source_refs | [{"id": "EN86", "page": 12}, {"id": "ZH86", "page": 12}, {"id": "OP86", "page": 10}] |
| confidence | documented_manual |
| conflict_ids | ["C05"] |
| owner_prompt | 19 |
| disposition | reviewed |
| coverage | {"documented": true, "hardware_qualified": false, "host_tested": false, "implemented": false} |
| review | {"decision": "Keep raw status and three distinct 86C interpretations.", "gate_ids": ["Q01", "Q02"], "status": "complete", "write_policy": "not_writable"} |

### velocity_parameters

| Fact | Value |
| --- | --- |
| key | velocity_parameters |
| kind | semantic_function |
| scope_id | manual_86 |
| claim | Procedure table calls target velocity U32 0..3000 rpm while dictionary/EDS call it I32; ramps are milliseconds |
| related_entries | ["0x60FF:00", "0x6083:00", "0x6084:00"] |
| source_refs | [{"id": "EN86", "page": 13}, {"id": "ZH86", "page": 13}, {"id": "OP86", "page": 11}] |
| confidence | documented_manual |
| conflict_ids | ["C11"] |
| owner_prompt | 35 |
| disposition | reviewed |
| coverage | {"documented": true, "hardware_qualified": false, "host_tested": false, "implemented": false} |
| review | {"decision": "Common positive range does not resolve negative motion or start order.", "gate_ids": ["Q01", "Q03", "Q11"], "status": "complete", "write_policy": "qualification_required"} |

### velocity_start_order

| Fact | Value |
| --- | --- |
| key | velocity_start_order |
| kind | semantic_function |
| scope_id | manual_86 |
| claim | EN86 enables before target parameters; ZH86 and OP86 specify parameters before 6 to 7 to 15 |
| related_entries | ["0x6040:00", "0x60FF:00"] |
| source_refs | [{"id": "EN86", "page": 13}, {"id": "ZH86", "page": 13}, {"id": "OP86", "page": 11}] |
| confidence | documented_manual |
| conflict_ids | ["C17"] |
| owner_prompt | 35 |
| disposition | reviewed |
| coverage | {"documented": true, "hardware_qualified": false, "host_tested": false, "implemented": false} |
| review | {"decision": "No universal start recipe selected.", "gate_ids": ["Q01", "Q03"], "status": "complete", "write_policy": "qualification_required"} |

### homing_parameters

| Fact | Value |
| --- | --- |
| key | homing_parameters |
| kind | semantic_function |
| scope_id | manual_86 |
| claim | Methods 17/18/24/29, two U32 speeds and U32 ramp time, plus home offset are described; physical prerequisites remain method-specific |
| related_entries | ["0x6098:00", "0x6099:01", "0x6099:02", "0x609A:00", "0x607C:00"] |
| source_refs | [{"id": "EN86", "page": 13}, {"id": "ZH86", "page": 13}, {"id": "OP86", "page": 11}] |
| confidence | documented_manual |
| conflict_ids | ["C22", "C23"] |
| owner_prompt | 36 |
| disposition | reviewed |
| coverage | {"documented": true, "hardware_qualified": false, "host_tested": false, "implemented": false} |
| review | {"decision": "Retain method-specific direction, trigger and switch requirements; no reference publication from stale completion.", "gate_ids": ["Q01", "Q21", "Q02", "Q14", "Q10"], "status": "complete", "write_policy": "qualification_required"} |

### homing_negative_limit_17

| Fact | Value |
| --- | --- |
| key | homing_negative_limit_17 |
| kind | semantic_function |
| scope_id | manual_86 |
| claim | Method 17 searches negative limit, withdraws positive and creeps negative toward limit |
| related_entries | ["0x6098:00"] |
| source_refs | [{"id": "EN86", "page": 13}, {"id": "ZH86", "page": 13}, {"id": "OP86", "page": 11}] |
| confidence | documented_manual |
| conflict_ids | [] |
| owner_prompt | 36 |
| disposition | reviewed |
| coverage | {"documented": true, "hardware_qualified": false, "host_tested": false, "implemented": false} |
| review | {"decision": "Retain method-specific direction, trigger and switch requirements; no reference publication from stale completion.", "gate_ids": ["Q01", "Q21", "Q02", "Q14"], "status": "complete", "write_policy": "qualification_required"} |

### homing_positive_limit_18

| Fact | Value |
| --- | --- |
| key | homing_positive_limit_18 |
| kind | semantic_function |
| scope_id | manual_86 |
| claim | Method 18 searches positive limit, withdraws negative and creeps positive toward limit |
| related_entries | ["0x6098:00"] |
| source_refs | [{"id": "EN86", "page": 14}, {"id": "ZH86", "page": 14}, {"id": "OP86", "page": 12}] |
| confidence | documented_manual |
| conflict_ids | [] |
| owner_prompt | 36 |
| disposition | reviewed |
| coverage | {"documented": true, "hardware_qualified": false, "host_tested": false, "implemented": false} |
| review | {"decision": "Retain method-specific direction, trigger and switch requirements; no reference publication from stale completion.", "gate_ids": ["Q01", "Q21", "Q02", "Q14"], "status": "complete", "write_policy": "qualification_required"} |

### homing_positive_origin_24

| Fact | Value |
| --- | --- |
| key | homing_positive_origin_24 |
| kind | semantic_function |
| scope_id | manual_86 |
| claim | Method 24 searches positive origin, withdraws negative and creeps positive toward origin |
| related_entries | ["0x6098:00"] |
| source_refs | [{"id": "EN86", "page": 14}, {"id": "ZH86", "page": 14}, {"id": "OP86", "page": 12}] |
| confidence | documented_manual |
| conflict_ids | [] |
| owner_prompt | 36 |
| disposition | reviewed |
| coverage | {"documented": true, "hardware_qualified": false, "host_tested": false, "implemented": false} |
| review | {"decision": "Retain method-specific direction, trigger and switch requirements; no reference publication from stale completion.", "gate_ids": ["Q01", "Q21", "Q02", "Q14"], "status": "complete", "write_policy": "qualification_required"} |

### homing_negative_origin_29

| Fact | Value |
| --- | --- |
| key | homing_negative_origin_29 |
| kind | semantic_function |
| scope_id | manual_86 |
| claim | Method 29 searches negative origin and creeps positive until leaving switch |
| related_entries | ["0x6098:00"] |
| source_refs | [{"id": "EN86", "page": 14}, {"id": "ZH86", "page": 14}, {"id": "OP86", "page": 12}] |
| confidence | documented_manual |
| conflict_ids | [] |
| owner_prompt | 36 |
| disposition | reviewed |
| coverage | {"documented": true, "hardware_qualified": false, "host_tested": false, "implemented": false} |
| review | {"decision": "Retain method-specific direction, trigger and switch requirements; no reference publication from stale completion.", "gate_ids": ["Q01", "Q21", "Q02", "Q14"], "status": "complete", "write_policy": "qualification_required"} |

### homing_trigger_and_interrupt

| Fact | Value |
| --- | --- |
| key | homing_trigger_and_interrupt |
| kind | semantic_function |
| scope_id | manual_86 |
| claim | Controlword bit 4 transition 0 to 1 starts homing; 1 to 0 is described as stop/interruption, with resulting reference unresolved |
| related_entries | ["0x6040:00"] |
| source_refs | [{"id": "EN86", "page": 15}, {"id": "ZH86", "page": 15}, {"id": "OP86", "page": 13}] |
| confidence | documented_manual |
| conflict_ids | [] |
| owner_prompt | 36 |
| disposition | reviewed |
| coverage | {"documented": true, "hardware_qualified": false, "host_tested": false, "implemented": false} |
| review | {"decision": "Retain method-specific direction, trigger and switch requirements; no reference publication from stale completion.", "gate_ids": ["Q01", "Q21", "Q02", "Q14"], "status": "complete", "write_policy": "qualification_required"} |

### homing_status

| Fact | Value |
| --- | --- |
| key | homing_status |
| kind | semantic_function |
| scope_id | manual_86 |
| claim | Manuals call statusword bit 8 homing completion; speed/running bit claims differ |
| related_entries | ["0x6041:00"] |
| source_refs | [{"id": "EN86", "page": 15}, {"id": "ZH86", "page": 15}, {"id": "OP86", "page": 13}] |
| confidence | documented_manual |
| conflict_ids | ["C05"] |
| owner_prompt | 36 |
| disposition | reviewed |
| coverage | {"documented": true, "hardware_qualified": false, "host_tested": false, "implemented": false} |
| review | {"decision": "Retain method-specific direction, trigger and switch requirements; no reference publication from stale completion.", "gate_ids": ["Q01", "Q21", "Q02", "Q14"], "status": "complete", "write_policy": "qualification_required"} |

### clear_current_position

| Fact | Value |
| --- | --- |
| key | clear_current_position |
| kind | semantic_function |
| scope_id | manual_86 |
| claim | 2302 0 to 1 clears current position, followed by manual return to zero; coordinate/reference effects need review |
| related_entries | ["0x2302:00"] |
| source_refs | [{"id": "EN86", "page": 15}, {"id": "ZH86", "page": 15}, {"id": "OP86", "page": 13}] |
| confidence | documented_manual |
| conflict_ids | [] |
| owner_prompt | 32 |
| disposition | reviewed |
| coverage | {"documented": true, "hardware_qualified": false, "host_tested": false, "implemented": false} |
| review | {"decision": "Explicit2302 rising edge followed by manual return0; reference effects require evidence.", "gate_ids": ["Q01", "Q14"], "status": "complete", "write_policy": "qualification_required"} |

### save_parameters

| Fact | Value |
| --- | --- |
| key | save_parameters |
| kind | semantic_function |
| scope_id | manual_86 |
| claim | 2300 value 2 is described as saving parameters through SDO; completion time is not documented |
| related_entries | ["0x2300:00"] |
| source_refs | [{"id": "EN86", "page": 15}, {"id": "ZH86", "page": 15}, {"id": "OP86", "page": 13}] |
| confidence | documented_manual |
| conflict_ids | [] |
| owner_prompt | 39 |
| disposition | reviewed |
| coverage | {"documented": true, "hardware_qualified": false, "host_tested": false, "implemented": false} |
| review | {"decision": "2300=2 explicit persistent action; SDO ACK does not prove durability.", "gate_ids": ["Q01", "Q15"], "status": "complete", "write_policy": "qualification_required"} |

### restore_factory_parameters

| Fact | Value |
| --- | --- |
| key | restore_factory_parameters |
| kind | semantic_function |
| scope_id | manual_86 |
| claim | 2300 value 1 is called restore factory parameters, zero ineffective; resulting settings/state need review |
| related_entries | ["0x2300:00"] |
| source_refs | [{"id": "EN86", "page": 18}, {"id": "ZH86", "page": 18}] |
| confidence | documented_manual |
| conflict_ids | [] |
| owner_prompt | 39 |
| disposition | reviewed |
| coverage | {"documented": true, "hardware_qualified": false, "host_tested": false, "implemented": false} |
| review | {"decision": "2300=1 explicit factory reset; no implicit save/restart.", "gate_ids": ["Q01", "Q15"], "status": "complete", "write_policy": "qualification_required"} |

### fault_reset_controlword

| Fact | Value |
| --- | --- |
| key | fault_reset_controlword |
| kind | semantic_function |
| scope_id | manual_86 |
| claim | Controlword bit 7 assertion and 15 to 128 example are separate reset claims; manual return to zero is described, but no edge trigger or pulse width is specified. |
| related_entries | ["0x6040:00"] |
| source_refs | [{"id": "EN86", "page": 15}, {"id": "ZH86", "page": 12}, {"id": "OP86", "page": 13}] |
| confidence | documented_manual |
| conflict_ids | [] |
| owner_prompt | 32 |
| disposition | reviewed |
| coverage | {"documented": true, "hardware_qualified": false, "host_tested": false, "implemented": false} |
| review | {"decision": "Set bit7, manually return0; trigger edge/pulse width not specified.", "gate_ids": ["Q01", "Q13"], "status": "complete", "write_policy": "qualification_required"} |

### fault_reset_vendor_object

| Fact | Value |
| --- | --- |
| key | fault_reset_vendor_object |
| kind | semantic_function |
| scope_id | manual_86 |
| claim | 2301 value 1 is described as effective fault reset, zero ineffective; equivalence to controlword reset is unproven |
| related_entries | ["0x2301:00"] |
| source_refs | [{"id": "EN86", "page": 18}] |
| confidence | documented_manual |
| conflict_ids | [] |
| owner_prompt | 32 |
| disposition | reviewed |
| coverage | {"documented": true, "hardware_qualified": false, "host_tested": false, "implemented": false} |
| review | {"decision": "2301 action kept separate from6040 reset; no replay.", "gate_ids": ["Q01", "Q13"], "status": "complete", "write_policy": "qualification_required"} |

### motion_monitor

| Fact | Value |
| --- | --- |
| key | motion_monitor |
| kind | semantic_function |
| scope_id | manual_86 |
| claim | 2100 bits 0..6 describe in-position, homed, running, alarm, released and two software limits; EDS says RW |
| related_entries | ["0x2100:00"] |
| source_refs | [{"id": "EN86", "page": 17}, {"id": "ZH86", "page": 18}] |
| confidence | documented_manual |
| conflict_ids | ["C06"] |
| owner_prompt | 19 |
| disposition | reviewed |
| coverage | {"documented": true, "hardware_qualified": false, "host_tested": false, "implemented": false} |
| review | {"decision": "Read-only observation with raw values retained; do not infer non-consuming upload or write-to-clear.", "gate_ids": ["Q01", "Q20"], "status": "complete", "write_policy": "not_writable"} |

### digital_input_observation

| Fact | Value |
| --- | --- |
| key | digital_input_observation |
| kind | semantic_function |
| scope_id | manual_86 |
| claim | 2101 bits 0..6 describe logical input active states; physical voltage interpretation is separate |
| related_entries | ["0x2101:00"] |
| source_refs | [{"id": "EN86", "page": 17}, {"id": "ZH86", "page": 18}] |
| confidence | documented_manual |
| conflict_ids | ["C06"] |
| owner_prompt | 19 |
| disposition | reviewed |
| coverage | {"documented": true, "hardware_qualified": false, "host_tested": false, "implemented": false} |
| review | {"decision": "Read-only observation with raw values retained; do not infer non-consuming upload or write-to-clear.", "gate_ids": ["Q01", "Q20"], "status": "complete", "write_policy": "not_writable"} |

### digital_output_observation

| Fact | Value |
| --- | --- |
| key | digital_output_observation |
| kind | semantic_function |
| scope_id | manual_86 |
| claim | 2102 bits 0..2 describe logical output active states, distinct from 60FE bits 16..18 |
| related_entries | ["0x2102:00", "0x60FE:01"] |
| source_refs | [{"id": "EN86", "page": 17}, {"id": "ZH86", "page": 18}] |
| confidence | documented_manual |
| conflict_ids | ["C06"] |
| owner_prompt | 19 |
| disposition | reviewed |
| coverage | {"documented": true, "hardware_qualified": false, "host_tested": false, "implemented": false} |
| review | {"decision": "Read-only observation with raw values retained; do not infer non-consuming upload or write-to-clear.", "gate_ids": ["Q01", "Q20"], "status": "complete", "write_policy": "not_writable"} |

### default_direction

| Fact | Value |
| --- | --- |
| key | default_direction |
| kind | semantic_function |
| scope_id | manual_86 |
| claim | 2200 labels Pulse Sign and Pulse /Sign; positive shaft direction remains unresolved |
| related_entries | ["0x2200:00"] |
| source_refs | [{"id": "EN86", "page": 18}, {"id": "ZH86", "page": 18}] |
| confidence | documented_manual |
| conflict_ids | [] |
| owner_prompt | 20 |
| disposition | reviewed |
| coverage | {"documented": true, "hardware_qualified": false, "host_tested": false, "implemented": false} |
| review | {"decision": "Keep native direction selector separate from physical sign.", "gate_ids": ["Q01", "Q04"], "status": "complete", "write_policy": "qualification_required"} |

### subdivision_and_position_scale

| Fact | Value |
| --- | --- |
| key | subdivision_and_position_scale |
| kind | semantic_function |
| scope_id | manual_86 |
| claim | 2201 has conflicting documented ranges; command, feedback and encoder-count scaling remain unresolved |
| related_entries | ["0x2201:00"] |
| source_refs | [{"id": "EN86", "page": 11}, {"id": "EN86", "page": 18}, {"id": "ZH86", "page": 18}] |
| confidence | documented_manual |
| conflict_ids | ["C12"] |
| owner_prompt | 20 |
| disposition | reviewed |
| coverage | {"documented": true, "hardware_qualified": false, "host_tested": false, "implemented": false} |
| review | {"decision": "No union range or feedback-scale inference.", "gate_ids": ["Q01", "Q04"], "status": "complete", "write_policy": "qualification_required"} |

### software_limit_enable

| Fact | Value |
| --- | --- |
| key | software_limit_enable |
| kind | semantic_function |
| scope_id | manual_86 |
| claim | 2202 options 0/1 and 607D limits are documented; EDS default 10 conflicts |
| related_entries | ["0x2202:00", "0x607D:01", "0x607D:02"] |
| source_refs | [{"id": "EN86", "page": 16}, {"id": "EN86", "page": 18}, {"id": "ZH86", "page": 18}, {"id": "EN86", "page": 17}] |
| confidence | documented_manual |
| conflict_ids | ["C13"] |
| owner_prompt | 20 |
| disposition | reviewed |
| coverage | {"documented": true, "hardware_qualified": false, "host_tested": false, "implemented": false} |
| review | {"decision": "Only0/1 documentary meanings; default10 not an accepted initialization.", "gate_ids": ["Q01", "Q05"], "status": "complete", "write_policy": "qualification_required"} |

### software_node_setting

| Fact | Value |
| --- | --- |
| key | software_node_setting |
| kind | semantic_function |
| scope_id | manual_86 |
| claim | 2203 range includes 0; DIP precedence, activation and reset/save behavior are not established |
| related_entries | ["0x2203:00"] |
| source_refs | [{"id": "EN86", "page": 18}, {"id": "ZH86", "page": 18}] |
| confidence | documented_manual |
| conflict_ids | [] |
| owner_prompt | 38 |
| disposition | reviewed |
| coverage | {"documented": true, "hardware_qualified": false, "host_tested": false, "implemented": false} |
| review | {"decision": "Zero is not a unicast node; commissioning explicit.", "gate_ids": ["Q01", "Q16"], "status": "complete", "write_policy": "qualification_required"} |

### input_polarity

| Fact | Value |
| --- | --- |
| key | input_polarity |
| kind | semantic_function |
| scope_id | manual_86 |
| claim | 2400:01 bits 0..6 invert logical DI states according to manual; physical wiring remains separate |
| related_entries | ["0x2400:01"] |
| source_refs | [{"id": "EN86", "page": 18}, {"id": "ZH86", "page": 18}] |
| confidence | documented_manual |
| conflict_ids | [] |
| owner_prompt | 21 |
| disposition | reviewed |
| coverage | {"documented": true, "hardware_qualified": false, "host_tested": false, "implemented": false} |
| review | {"decision": "Logical state and physical wiring are distinct.", "gate_ids": ["Q01", "Q19"], "status": "complete", "write_policy": "qualification_required"} |

### input_function_assignment

| Fact | Value |
| --- | --- |
| key | input_function_assignment |
| kind | semantic_function |
| scope_id | manual_86 |
| claim | 2400:02..08 describe origin, positive/negative limit, enable, stop, emergency stop and user inputs |
| related_entries | ["0x2400:02", "0x2400:03", "0x2400:04", "0x2400:05", "0x2400:06", "0x2400:07", "0x2400:08"] |
| source_refs | [{"id": "EN86", "page": 8}, {"id": "EN86", "page": 18}, {"id": "ZH86", "page": 19}, {"id": "OP86", "page": 7}, {"id": "ZH86", "page": 8}] |
| confidence | documented_manual |
| conflict_ids | ["C14"] |
| owner_prompt | 21 |
| disposition | reviewed |
| coverage | {"documented": true, "hardware_qualified": false, "host_tested": false, "implemented": false} |
| review | {"decision": "Documented function codes retained; never silently reassign protective inputs.", "gate_ids": ["Q01", "Q19"], "status": "complete", "write_policy": "qualification_required"} |

### input_filter

| Fact | Value |
| --- | --- |
| key | input_filter |
| kind | semantic_function |
| scope_id | manual_86 |
| claim | Manuals call 2400:0A an input filter coefficient and list 2401..2407 per-input filters; no time unit/equation established |
| related_entries | ["0x2400:0A", "0x2401:00", "0x2402:00", "0x2403:00", "0x2404:00", "0x2405:00", "0x2406:00", "0x2407:00"] |
| source_refs | [{"id": "EN86", "page": 18}, {"id": "ZH86", "page": 19}, {"id": "OP86", "page": 6}] |
| confidence | documented_manual |
| conflict_ids | ["C15", "C16"] |
| owner_prompt | 21 |
| disposition | reviewed |
| coverage | {"documented": true, "hardware_qualified": false, "host_tested": false, "implemented": false} |
| review | {"decision": "Reserved conflict blocks0A; no invented time unit.", "gate_ids": ["Q01", "Q08"], "status": "complete", "write_policy": "qualification_required"} |

### output_polarity

| Fact | Value |
| --- | --- |
| key | output_polarity |
| kind | semantic_function |
| scope_id | manual_86 |
| claim | Manuals disagree with EDS on 2400:0B versus 0C; tables describe only bits 0/1 despite three outputs |
| related_entries | ["0x2400:0B", "0x2400:0C"] |
| source_refs | [{"id": "EN86", "page": 8}, {"id": "EN86", "page": 18}, {"id": "ZH86", "page": 19}, {"id": "OP86", "page": 6}, {"id": "ZH86", "page": 8}] |
| confidence | documented_manual |
| conflict_ids | ["C01"] |
| owner_prompt | 21 |
| disposition | reviewed |
| coverage | {"documented": true, "hardware_qualified": false, "host_tested": false, "implemented": false} |
| review | {"decision": "No selected address or third polarity bit.", "gate_ids": ["Q01", "Q06"], "status": "complete", "write_policy": "qualification_required"} |

### output_function_assignment

| Fact | Value |
| --- | --- |
| key | output_function_assignment |
| kind | semantic_function |
| scope_id | manual_86 |
| claim | 2400:0C..0F meanings and function numbers 2/3 conflict across source tables |
| related_entries | ["0x2400:0C", "0x2400:0D", "0x2400:0E", "0x2400:0F"] |
| source_refs | [{"id": "EN86", "page": 8}, {"id": "EN86", "page": 18}, {"id": "ZH86", "page": 19}, {"id": "OP86", "page": 7}, {"id": "ZH86", "page": 8}] |
| confidence | documented_manual |
| conflict_ids | ["C01", "C02", "C14"] |
| owner_prompt | 21 |
| disposition | reviewed |
| coverage | {"documented": true, "hardware_qualified": false, "host_tested": false, "implemented": false} |
| review | {"decision": "Preserve2/3 conflict and address alternatives.", "gate_ids": ["Q01", "Q06", "Q19"], "status": "complete", "write_policy": "qualification_required"} |

### user_output_command_and_mask

| Fact | Value |
| --- | --- |
| key | user_output_command_and_mask |
| kind | semantic_function |
| scope_id | manual_86 |
| claim | OP86 describes 60FE:01 physical outputs and :02 mask at bits 16..18; both corresponding bits must be 1 to command an output. Terminal identity remains unqualified. |
| related_entries | ["0x60FE:01", "0x60FE:02"] |
| source_refs | [{"id": "OP86", "page": 7}] |
| confidence | documented_manual |
| conflict_ids | [] |
| owner_prompt | 21 |
| disposition | reviewed |
| coverage | {"documented": true, "hardware_qualified": false, "host_tested": false, "implemented": false} |
| review | {"decision": "Both corresponding bits required perOP86; terminal identity still gated.", "gate_ids": ["Q01", "Q06"], "status": "complete", "write_policy": "qualification_required"} |

### brake_output_wiring

| Fact | Value |
| --- | --- |
| key | brake_output_wiring |
| kind | semantic_function |
| scope_id | manual_86 |
| claim | DO2/function 5 relay example says PA_01E while same-page table says PA_02E; not a new CANopen object |
| related_entries | ["0x2400:0E"] |
| source_refs | [{"id": "OP86", "page": 6}] |
| confidence | documented_manual |
| conflict_ids | ["C04"] |
| owner_prompt | 21 |
| disposition | reviewed |
| coverage | {"documented": true, "hardware_qualified": false, "host_tested": false, "implemented": false} |
| review | {"decision": "PA label is not an extra CAN object; no automatic brake action.", "gate_ids": ["Q01", "Q06", "Q19"], "status": "complete", "write_policy": "qualification_required"} |

### drive_loop_mode

| Fact | Value |
| --- | --- |
| key | drive_loop_mode |
| kind | semantic_function |
| scope_id | manual_86 |
| claim | 2500 0 ineffective, 1 open loop, 2 closed loop is distinct from 6060 motion mode |
| related_entries | ["0x2500:00", "0x6060:00"] |
| source_refs | [{"id": "EN86", "page": 19}, {"id": "ZH86", "page": 19}] |
| confidence | documented_manual |
| conflict_ids | [] |
| owner_prompt | 22 |
| disposition | reviewed |
| coverage | {"documented": true, "hardware_qualified": false, "host_tested": false, "implemented": false} |
| review | {"decision": "0 ineffective/1open/2closed distinct from motion mode.", "gate_ids": ["Q01", "Q09"], "status": "complete", "write_policy": "qualification_required"} |

### encoder_resolution

| Fact | Value |
| --- | --- |
| key | encoder_resolution |
| kind | semantic_function |
| scope_id | manual_86 |
| claim | 2501 names encoder resolution without line/count/quadrature basis |
| related_entries | ["0x2501:00"] |
| source_refs | [{"id": "EN86", "page": 19}, {"id": "ZH86", "page": 19}] |
| confidence | documented_manual |
| conflict_ids | [] |
| owner_prompt | 22 |
| disposition | reviewed |
| coverage | {"documented": true, "hardware_qualified": false, "host_tested": false, "implemented": false} |
| review | {"decision": "No inferred line/count/quadrature scale.", "gate_ids": ["Q01", "Q09", "Q04"], "status": "complete", "write_policy": "qualification_required"} |

### current_settings

| Fact | Value |
| --- | --- |
| key | current_settings |
| kind | semantic_function |
| scope_id | manual_86 |
| claim | 2502 is mA, 2503..2506 percent ratios and 2507 milliseconds; motor ratings remain independent |
| related_entries | ["0x2502:00", "0x2503:00", "0x2504:00", "0x2505:00", "0x2506:00", "0x2507:00"] |
| source_refs | [{"id": "EN86", "page": 19}, {"id": "ZH86", "page": 19}, {"id": "ZH86", "page": 20}] |
| confidence | documented_manual |
| conflict_ids | [] |
| owner_prompt | 22 |
| disposition | reviewed |
| coverage | {"documented": true, "hardware_qualified": false, "host_tested": false, "implemented": false} |
| review | {"decision": "mA/percent/ms native fields; no generic torque interface.", "gate_ids": ["Q01", "Q09"], "status": "complete", "write_policy": "qualification_required"} |

### tuning_settings

| Fact | Value |
| --- | --- |
| key | tuning_settings |
| kind | semantic_function |
| scope_id | manual_86 |
| claim | 2508..2519 describe filters, thresholds and gains; EDS-only 251A..2520 have no reviewed physical meaning |
| related_entries | ["0x2508:00", "0x2509:00", "0x250A:00", "0x250B:00", "0x250C:00", "0x250D:00", "0x250E:00", "0x250F:00", "0x2510:00", "0x2511:00", "0x2512:00", "0x2513:00", "0x2514:00", "0x2515:00", "0x2516:00", "0x2517:00", "0x2518:00", "0x2519:00", "0x251A:00", "0x251B:00", "0x251C:00", "0x251D:00", "0x251E:00", "0x251F:00", "0x2520:00"] |
| source_refs | [{"id": "EN86", "page": 19}, {"id": "EN86", "page": 20}, {"id": "ZH86", "page": 20}] |
| confidence | documented_manual |
| conflict_ids | [] |
| owner_prompt | 22 |
| disposition | reviewed |
| coverage | {"documented": true, "hardware_qualified": false, "host_tested": false, "implemented": false} |
| review | {"decision": "Keep each opaque field visible; no autotune inferred.", "gate_ids": ["Q01", "Q09"], "status": "complete", "write_policy": "qualification_required"} |

### error_codes

| Fact | Value |
| --- | --- |
| key | error_codes |
| kind | semantic_function |
| scope_id | manual_86 |
| claim | 603F table names FF01 overcurrent and FF05 excessive position deviation within FF00..FFFF |
| related_entries | ["0x603F:00"] |
| source_refs | [{"id": "EN86", "page": 16}, {"id": "ZH86", "page": 16}] |
| confidence | documented_manual |
| conflict_ids | ["C07"] |
| owner_prompt | 19 |
| disposition | reviewed |
| coverage | {"documented": true, "hardware_qualified": false, "host_tested": false, "implemented": false} |
| review | {"decision": "Read-only observation with raw values retained; do not infer non-consuming upload or write-to-clear.", "gate_ids": ["Q01", "Q20", "Q13"], "status": "complete", "write_policy": "not_writable"} |

### led_alarm_and_reset_class

| Fact | Value |
| --- | --- |
| key | led_alarm_and_reset_class |
| kind | semantic_function |
| scope_id | manual_86 |
| claim | Per-five-second LED counts 1..5 distinguish power-restart faults from resettable faults |
| related_entries | [] |
| source_refs | [{"id": "EN86", "page": 20}, {"id": "ZH86", "page": 20}, {"id": "OP86", "page": 14}] |
| confidence | documented_manual |
| conflict_ids | [] |
| owner_prompt | 19 |
| disposition | reviewed |
| coverage | {"documented": true, "hardware_qualified": false, "host_tested": false, "implemented": false} |
| review | {"decision": "Read-only observation with raw values retained; do not infer non-consuming upload or write-to-clear.", "gate_ids": ["Q01", "Q20"], "status": "complete", "write_policy": "not_writable"} |

### dip_node_selection

| Fact | Value |
| --- | --- |
| key | dip_node_selection |
| kind | semantic_function |
| scope_id | manual_86 |
| claim | SW1..5 encode 1..31; all OFF selects user-defined node, not node zero |
| related_entries | ["0x2203:00"] |
| source_refs | [{"id": "EN86", "page": 10}, {"id": "ZH86", "page": 10}, {"id": "OP86", "page": 8}] |
| confidence | documented_manual |
| conflict_ids | [] |
| owner_prompt | 38 |
| disposition | reviewed |
| coverage | {"documented": true, "hardware_qualified": false, "host_tested": false, "implemented": false} |
| review | {"decision": "All-off selects user parameter, not node0.", "gate_ids": ["Q01", "Q16"], "status": "complete", "write_policy": "qualification_required"} |

### dip_bitrate_selection

| Fact | Value |
| --- | --- |
| key | dip_bitrate_selection |
| kind | semantic_function |
| scope_id | manual_86 |
| claim | SW6/SW7 choose 125/250/500/1000 kbit/s; EDS advertises 50 kbit/s without manual DIP choice |
| related_entries | [] |
| source_refs | [{"id": "EN86", "page": 9}, {"id": "ZH86", "page": 9}, {"id": "OP86", "page": 7}] |
| confidence | documented_manual |
| conflict_ids | ["C20"] |
| owner_prompt | 38 |
| disposition | reviewed |
| coverage | {"documented": true, "hardware_qualified": false, "host_tested": false, "implemented": false} |
| review | {"decision": "Four documented DIP choices;50k remains EDS-only.", "gate_ids": ["Q01", "Q16"], "status": "complete", "write_policy": "qualification_required"} |

### termination_and_cabling

| Fact | Value |
| --- | --- |
| key | termination_and_cabling |
| kind | semantic_function |
| scope_id | manual_86 |
| claim | SW8 ON connects 120 ohms; RJ45 pins 1/2/3 are CAN_H/CAN_L/GND; cable lengths are recommendations |
| related_entries | [] |
| source_refs | [{"id": "EN86", "page": 5}, {"id": "EN86", "page": 9}, {"id": "ZH86", "page": 9}, {"id": "OP86", "page": 7}] |
| confidence | documented_manual |
| conflict_ids | [] |
| owner_prompt | 47 |
| disposition | reviewed |
| coverage | {"documented": true, "hardware_qualified": false, "host_tested": false, "implemented": false} |
| review | {"decision": "Physical fixture requirement; not firmware functionality.", "gate_ids": ["Q01", "Q19"], "status": "complete", "write_policy": "not_writable"} |

### electrical_input_output_contract

| Fact | Value |
| --- | --- |
| key | electrical_input_output_contract |
| kind | semantic_function |
| scope_id | manual_86 |
| claim | Manuals describe seven 24 V inputs, DICOM wiring, DOCOM 0 V and three outputs; actual fixture unknown |
| related_entries | ["0x2400:01"] |
| source_refs | [{"id": "EN86", "page": 6}, {"id": "EN86", "page": 8}, {"id": "OP86", "page": 4}] |
| confidence | documented_manual |
| conflict_ids | [] |
| owner_prompt | 47 |
| disposition | reviewed |
| coverage | {"documented": true, "hardware_qualified": false, "host_tested": false, "implemented": false} |
| review | {"decision": "Seven inputs/three outputs described; actual wiring unknown.", "gate_ids": ["Q01", "Q19"], "status": "complete", "write_policy": "not_writable"} |

### product_protocol_and_geometry

| Fact | Value |
| --- | --- |
| key | product_protocol_and_geometry |
| kind | semantic_function |
| scope_id | manual_86 |
| claim | Cover/product table names CiA301/CiA402.V2, three motion modes, 32-axis claim and dual RJ45; no universal CAN assumptions follow |
| related_entries | [] |
| source_refs | [{"id": "EN86", "page": 3}, {"id": "ZH86", "page": 3}] |
| confidence | documented_manual |
| conflict_ids | ["C18"] |
| owner_prompt | 37 |
| disposition | reviewed |
| coverage | {"documented": true, "hardware_qualified": false, "host_tested": false, "implemented": false} |
| review | {"decision": "CANopen stepper product claim; no shared silicon or exact identity from EDS.", "gate_ids": ["Q01"], "status": "complete", "write_policy": "not_writable"} |

### supply_voltage_applicability

| Fact | Value |
| --- | --- |
| key | supply_voltage_applicability |
| kind | semantic_function |
| scope_id | manual_86 |
| claim | Supply ranges conflict between product table and wiring/terminal pages; no bench default follows |
| related_entries | [] |
| source_refs | [{"id": "EN86", "page": 3}, {"id": "EN86", "page": 6}, {"id": "ZH86", "page": 3}, {"id": "ZH86", "page": 6}] |
| confidence | documented_manual |
| conflict_ids | ["C19"] |
| owner_prompt | 47 |
| disposition | reviewed |
| coverage | {"documented": true, "hardware_qualified": false, "host_tested": false, "implemented": false} |
| review | {"decision": "No selected supply range until applicable product evidence.", "gate_ids": ["Q01", "Q19"], "status": "complete", "write_policy": "not_writable"} |

### status_interpretation_57

| Fact | Value |
| --- | --- |
| key | status_interpretation_57 |
| kind | semantic_function |
| scope_id | manual_57_contrast |
| claim | ZH57 statusword table assigns bit 7 to alarm/ready; this 57C model contrast does not establish a CL86-C status mask |
| related_entries | ["0x6041:00"] |
| source_refs | [{"id": "ZH57", "page": 12}] |
| confidence | documented_manual |
| conflict_ids | [] |
| owner_prompt | 19 |
| disposition | reviewed |
| coverage | {"documented": true, "hardware_qualified": false, "host_tested": false, "implemented": false} |
| review | {"decision": "57C alarm bit7 does not resolve86C; retain separate model scope.", "gate_ids": ["Q01", "Q02"], "status": "complete", "write_policy": "not_writable"} |

### output_terminal_labels_57

| Fact | Value |
| --- | --- |
| key | output_terminal_labels_57 |
| kind | semantic_function |
| scope_id | manual_57_contrast |
| claim | ZH57 p8 identifies 60FE bits 16..18 as DO0..DO2, while p17 calls them DO1..DO3; preserve physical identity conflict |
| related_entries | ["0x60FE:01", "0x60FE:02"] |
| source_refs | [{"id": "ZH57", "page": 8}, {"id": "ZH57", "page": 17}] |
| confidence | contradicted |
| conflict_ids | ["C03"] |
| owner_prompt | 21 |
| disposition | reviewed |
| coverage | {"documented": true, "hardware_qualified": false, "host_tested": false, "implemented": false} |
| review | {"decision": "No silent renumbering or fourth output.", "gate_ids": ["Q01", "Q07"], "status": "complete", "write_policy": "qualification_required"} |

### supply_voltage_applicability_57

| Fact | Value |
| --- | --- |
| key | supply_voltage_applicability_57 |
| kind | semantic_function |
| scope_id | manual_57_contrast |
| claim | ZH57 product and terminal pages give different DC supply ranges; neither sets a bench default |
| related_entries | [] |
| source_refs | [{"id": "ZH57", "page": 3}, {"id": "ZH57", "page": 6}] |
| confidence | contradicted |
| conflict_ids | ["C24"] |
| owner_prompt | 47 |
| disposition | reviewed |
| coverage | {"documented": true, "hardware_qualified": false, "host_tested": false, "implemented": false} |
| review | {"decision": "Separate57C supply conflict; no cross-family bench default.", "gate_ids": ["Q01", "Q19"], "status": "complete", "write_policy": "not_writable"} |

## Conflicting source claims

Documentary policy does not select a firmware winner. Claims stay side by side.

### C01: 2400 output polarity and DO assignment addresses

| Source | Claim |
| --- | --- |
| {"id": "EDS", "line_end": 1450, "line_start": 1442, "section": "2400subB"} | 0B Reserved |
| {"id": "EDS", "line_end": 1459, "line_start": 1451, "section": "2400subC"} | 0C output polarity |
| {"id": "EDS", "line_end": 1468, "line_start": 1460, "section": "2400subD"} | 0D DO0 assignment |
| {"id": "EDS", "line_end": 1477, "line_start": 1469, "section": "2400subE"} | 0E DO1 assignment |
| {"id": "EDS", "line_end": 1486, "line_start": 1478, "section": "2400subF"} | 0F DO2 assignment |
| {"id": "EN86", "page": 18} | 0B polarity; 0C..0E DO assignments |
| {"id": "ZH86", "page": 19} | 0C polarity; 0D..0F DO assignments |
| {"id": "EN86", "page": 8} | 0B polarity; 0C..0E DO assignments |
| {"id": "ZH86", "page": 8} | 0B polarity; 0C..0E DO assignments |

Disposition: unresolved. Review: {"consequence": "Only dependent fields/actions are gated; raw evidence and unrelated operations remain available for later implementation.", "disposition": "Gate output assignment/polarity writes; preserve every conflicting address. Reserved0B stays excluded.", "gate_ids": ["Q06"], "status": "complete"}.

### C02: DO function values 2 and 3

| Source | Claim |
| --- | --- |
| {"id": "EN86", "page": 8} | 2 running; 3 homing complete |
| {"id": "EN86", "page": 18} | 2 homing complete; 3 drive status |
| {"id": "ZH86", "page": 19} | 2 homing complete; 3 drive status |
| {"id": "ZH86", "page": 8} | 2 running; 3 homing complete |
| {"id": "OP86", "page": 7} | 2 running; 3 homing complete |

Disposition: unresolved. Review: {"consequence": "Only dependent fields/actions are gated; raw evidence and unrelated operations remain available for later implementation.", "disposition": "Gate function2/3 interpretation; other documented codes do not resolve this pair.", "gate_ids": ["Q06"], "status": "complete"}.

### C03: 57C 60FE physical terminal labels

| Source | Claim |
| --- | --- |
| {"id": "ZH57", "page": 8} | bits 16..18: DO0, DO1, DO2 |
| {"id": "ZH57", "page": 17} | bits 16..18: DO1, DO2, DO3 |

Disposition: unresolved. Review: {"consequence": "Only dependent fields/actions are gated; raw evidence and unrelated operations remain available for later implementation.", "disposition": "Keep 57C terminal labels unresolved; no effect on86C binding without separate evidence.", "gate_ids": ["Q07"], "status": "complete"}.

### C04: Brake example parameter label for DO2

| Source | Claim |
| --- | --- |
| {"id": "OP86", "page": 6} | instruction labels DO2 PA_01E |
| {"id": "OP86", "page": 6} | same-page table labels DO2 PA_02E / 2400:0E |

Disposition: unresolved. Review: {"consequence": "Only dependent fields/actions are gated; raw evidence and unrelated operations remain available for later implementation.", "disposition": "Keep both PA labels; neither is a new CANopen address or reliable brake mapping.", "gate_ids": ["Q06", "Q19"], "status": "complete"}.

### C05: 6041 statusword bit interpretation

| Source | Claim |
| --- | --- |
| {"id": "EN86", "page": 12} | alarm bit7, zero-speed bit10, target-effective bit12 |
| {"id": "ZH86", "page": 12} | alarm bit3, running bit14 |
| {"id": "OP86", "page": 10} | alarm bit7, running bit14 |

Disposition: unresolved. Review: {"consequence": "Only dependent fields/actions are gated; raw evidence and unrelated operations remain available for later implementation.", "disposition": "Keep raw status and three86C dialects; select no merged status mask.", "gate_ids": ["Q02"], "status": "complete"}.

### C06: 2100..2102 monitor object SDO access

| Source | Claim |
| --- | --- |
| {"id": "EDS", "line_end": 1255, "line_start": 1247, "section": "2100"} | 2100 RW |
| {"id": "EDS", "line_end": 1264, "line_start": 1256, "section": "2101"} | 2101 RW |
| {"id": "EDS", "line_end": 1273, "line_start": 1265, "section": "2102"} | 2102 RW |
| {"id": "EN86", "page": 17} | 2100..2102 RO monitor table |

Disposition: unresolved. Review: {"consequence": "Only dependent fields/actions are gated; raw evidence and unrelated operations remain available for later implementation.", "disposition": "Conservative read-only monitor surface; EDS RW never grants a write.", "gate_ids": ["Q20"], "status": "complete"}.

### C07: 603F error-code access

| Source | Claim |
| --- | --- |
| {"id": "EDS", "line_end": 1831, "line_start": 1823, "section": "603F"} | RO |
| {"id": "EN86", "page": 16} | RW in manual dictionary |

Disposition: unresolved. Review: {"consequence": "Only dependent fields/actions are gated; raw evidence and unrelated operations remain available for later implementation.", "disposition": "Conservative read-only error code; reset uses explicit action, never603F write.", "gate_ids": ["Q13", "Q20"], "status": "complete"}.

### C08: 6040 and 6060 command access

| Source | Claim |
| --- | --- |
| {"id": "EDS", "line_end": 1840, "line_start": 1832, "section": "6040"} | 6040 WO |
| {"id": "EDS", "line_end": 1867, "line_start": 1859, "section": "6060"} | 6060 WO |
| {"id": "EN86", "page": 11} | 6040 and 6060 shown RW |

Disposition: unresolved. Review: {"consequence": "Only dependent fields/actions are gated; raw evidence and unrelated operations remain available for later implementation.", "disposition": "Use write-only command access as conservative intersection; no SDO read-modify-write. PDO eligibility is separate.", "gate_ids": ["Q11"], "status": "complete"}.

### C09: 6061 observed mode versus translated set instruction

| Source | Claim |
| --- | --- |
| {"id": "EDS", "line_end": 1876, "line_start": 1868, "section": "6061"} | 6061 RO |
| {"id": "EN86", "page": 11} | mode display RO in object table |
| {"id": "EN86", "page": 12} | procedure says set 6061 |
| {"id": "EN86", "page": 16} | 6061 RW in manual dictionary |

Disposition: unresolved. Review: {"consequence": "Only dependent fields/actions are gated; raw evidence and unrelated operations remain available for later implementation.", "disposition": "6061 is observation only; reject translated instruction to write it.", "gate_ids": ["Q10"], "status": "complete"}.

### C10: 6060 and 6061 signedness

| Source | Claim |
| --- | --- |
| {"id": "EDS", "line_end": 1867, "line_start": 1859, "section": "6060"} | 6060 U8 |
| {"id": "EDS", "line_end": 1876, "line_start": 1868, "section": "6061"} | 6061 U8 |
| {"id": "EN86", "page": 16} | manual dictionary INTEGER8 |

Disposition: unresolved. Review: {"consequence": "Only dependent fields/actions are gated; raw evidence and unrelated operations remain available for later implementation.", "disposition": "Retain raw byte and documented positive codes only; high-bit/negative interpretation remains unknown.", "gate_ids": ["Q10"], "status": "complete"}.

### C11: 60FF target-velocity signedness

| Source | Claim |
| --- | --- |
| {"id": "EDS", "line_end": 2068, "line_start": 2060, "section": "60FF"} | I32 |
| {"id": "EN86", "page": 13} | procedure table U32 0..3000 rpm |
| {"id": "EN86", "page": 17} | dictionary I32 |

Disposition: unresolved. Review: {"consequence": "Only dependent fields/actions are gated; raw evidence and unrelated operations remain available for later implementation.", "disposition": "Retain I32 metadata and U32 claim; no negative-speed or start-order conclusion.", "gate_ids": ["Q03"], "status": "complete"}.

### C12: 2201 subdivision range and EDS default

| Source | Claim |
| --- | --- |
| {"id": "EDS", "line_end": 1291, "line_start": 1283, "section": "2201"} | default 0, no limits |
| {"id": "EN86", "page": 11} | 0..65535 in object table |
| {"id": "EN86", "page": 18} | 400..51200 in detailed setting |

Disposition: unresolved. Review: {"consequence": "Only dependent fields/actions are gated; raw evidence and unrelated operations remain available for later implementation.", "disposition": "No union range; zero metadata is not an initialization or valid physical scale.", "gate_ids": ["Q04"], "status": "complete"}.

### C13: 2202 software-limit enable options versus default

| Source | Claim |
| --- | --- |
| {"id": "EDS", "line_end": 1300, "line_start": 1292, "section": "2202"} | default 10 |
| {"id": "EN86", "page": 18} | only values 0 and 1 described |

Disposition: unresolved. Review: {"consequence": "Only dependent fields/actions are gated; raw evidence and unrelated operations remain available for later implementation.", "disposition": "Document0/1 meanings but no initialization from10.", "gate_ids": ["Q05"], "status": "complete"}.

### C14: I/O assignment factory values

| Source | Claim |
| --- | --- |
| {"id": "EDS", "line_end": 1369, "line_start": 1361, "section": "2400sub2"} | DI0 default0 |
| {"id": "EDS", "line_end": 1378, "line_start": 1370, "section": "2400sub3"} | DI1 default0 |
| {"id": "EDS", "line_end": 1387, "line_start": 1379, "section": "2400sub4"} | DI2 default0 |
| {"id": "EDS", "line_end": 1468, "line_start": 1460, "section": "2400subD"} | DO0 default0 |
| {"id": "EN86", "page": 8} | DI0=1,DI1=2,DI2=3,DO0=1 defaults |
| {"id": "ZH86", "page": 8} | DI0=1,DI1=2,DI2=3,DO0=1 defaults |

Disposition: unresolved. Review: {"consequence": "Only dependent fields/actions are gated; raw evidence and unrelated operations remain available for later implementation.", "disposition": "Read actual assignments with provenance; never restore either claimed default automatically.", "gate_ids": ["Q19"], "status": "complete"}.

### C15: 2400:0A reserved name versus filter function

| Source | Claim |
| --- | --- |
| {"id": "EDS", "line_end": 1441, "line_start": 1433, "section": "2400subA"} | rsvd2, RW U16 |
| {"id": "EN86", "page": 18} | input filter coefficient |

Disposition: unresolved. Review: {"consequence": "Only dependent fields/actions are gated; raw evidence and unrelated operations remain available for later implementation.", "disposition": "Keep0A reserved/excluded until an applicable map establishes a filter function.", "gate_ids": ["Q08"], "status": "complete"}.

### C16: 2401..2407 input filter defaults

| Source | Claim |
| --- | --- |
| {"id": "EDS", "line_end": 1504, "line_start": 1496, "section": "2401"} | Filter default 50 |
| {"id": "EDS", "line_end": 1513, "line_start": 1505, "section": "2402"} | Filter default 40 |
| {"id": "EDS", "line_end": 1522, "line_start": 1514, "section": "2403"} | Filter default 100 |
| {"id": "EDS", "line_end": 1531, "line_start": 1523, "section": "2404"} | Filter default 4000 |
| {"id": "EDS", "line_end": 1540, "line_start": 1532, "section": "2405"} | Filter default 10 |
| {"id": "EDS", "line_end": 1549, "line_start": 1541, "section": "2406"} | Filter default 2 |
| {"id": "EDS", "line_end": 1558, "line_start": 1550, "section": "2407"} | Filter default 16 |
| {"id": "EN86", "page": 8} | all seven input filters 2 |

Disposition: unresolved. Review: {"consequence": "Only dependent fields/actions are gated; raw evidence and unrelated operations remain available for later implementation.", "disposition": "Raw filter observations only; no default writes or invented time unit.", "gate_ids": ["Q08"], "status": "complete"}.

### C17: Velocity setup and enable order

| Source | Claim |
| --- | --- |
| {"id": "EN86", "page": 13} | enable then parameters and start bits 4..6 |
| {"id": "ZH86", "page": 13} | parameters before 6 to 7 to 15, start on enable |
| {"id": "OP86", "page": 11} | parameters before enable |

Disposition: unresolved. Review: {"consequence": "Only dependent fields/actions are gated; raw evidence and unrelated operations remain available for later implementation.", "disposition": "No universal velocity recipe; qualify order and retained-target behavior.", "gate_ids": ["Q03"], "status": "complete"}.

### C18: OP86 cover versus internal model headers

| Source | Claim |
| --- | --- |
| {"id": "OP86", "page": 1} | cover identifies CL86-C/OL86-C |
| {"id": "OP86", "page": 3} | internal header identifies CL57-C/OL57-C |

Disposition: unresolved. Review: {"consequence": "Only dependent fields/actions are gated; raw evidence and unrelated operations remain available for later implementation.", "disposition": "OP86 cover does not make its57C headers firmware proof. Keep attribution per page.", "gate_ids": ["Q01"], "status": "complete"}.

### C19: 86C supply range

| Source | Claim |
| --- | --- |
| {"id": "EN86", "page": 3} | 18..80 VAC or 36..110 VDC product table |
| {"id": "EN86", "page": 6} | 24..50 VDC power terminal |

Disposition: unresolved. Review: {"consequence": "Only dependent fields/actions are gated; raw evidence and unrelated operations remain available for later implementation.", "disposition": "No bench voltage selected from conflicting86C pages.", "gate_ids": ["Q19"], "status": "complete"}.

### C20: 50 kbit/s metadata versus DIP choices

| Source | Claim |
| --- | --- |
| {"id": "EDS", "line_end": 35, "line_start": 12, "section": "DeviceInfo"} | BaudRate_50=1 |
| {"id": "EN86", "page": 9} | SW6/SW7 list 125,250,500,1000 kbit/s only |

Disposition: unresolved. Review: {"consequence": "Only dependent fields/actions are gated; raw evidence and unrelated operations remain available for later implementation.", "disposition": "Advertise only four documented DIP choices as documentary facts;50k selection remains unknown.", "gate_ids": ["Q16"], "status": "complete"}.

### C21: 607A printed datatype typo

| Source | Claim |
| --- | --- |
| {"id": "EDS", "line_end": 1902, "line_start": 1894, "section": "607A"} | I32 |
| {"id": "EN86", "page": 16} | I3C in manual dictionary |

Disposition: unresolved. Review: {"consequence": "Only dependent fields/actions are gated; raw evidence and unrelated operations remain available for later implementation.", "disposition": "Interpret target as I32 using EDS607A and EN86p11 position table; preserve malformed I3C claim. This is documentary normalization, not firmware measurement.", "gate_ids": ["Q04"], "status": "complete"}.

### C22: 6098 homing method signedness

| Source | Claim |
| --- | --- |
| {"id": "EDS", "line_end": 1978, "line_start": 1970, "section": "6098"} | U8 |
| {"id": "EN86", "page": 17} | I8 in manual dictionary |

Disposition: unresolved. Review: {"consequence": "Only dependent fields/actions are gated; raw evidence and unrelated operations remain available for later implementation.", "disposition": "Only positive documented methods17/18/24/29; no interpretation of negative/high-bit values.", "gate_ids": ["Q10", "Q21"], "status": "complete"}.

### C23: 6099:02 creep parameter wording

| Source | Claim |
| --- | --- |
| {"id": "EN86", "page": 13} | creep time(rpm) in table |
| {"id": "EN86", "page": 14} | creep speed in prose |

Disposition: unresolved. Review: {"consequence": "Only dependent fields/actions are gated; raw evidence and unrelated operations remain available for later implementation.", "disposition": "Interpret6099:02 as U32 creep speed in rpm using procedure prose and table unit; preserve time wording as source defect.", "gate_ids": ["Q21"], "status": "complete"}.

### C24: 57C supply range contrast

| Source | Claim |
| --- | --- |
| {"id": "ZH57", "page": 3} | 24..48 VDC product table |
| {"id": "ZH57", "page": 6} | 24..50 VDC power terminal |

Disposition: unresolved. Review: {"consequence": "Only dependent fields/actions are gated; raw evidence and unrelated operations remain available for later implementation.", "disposition": "No bench voltage selected from conflicting57C pages.", "gate_ids": ["Q19"], "status": "complete"}.

## Gaps, anomalies and nonobject exclusions

Address gaps are absent source entries, not reserved objects or implied support.

| First absent index | Last absent index |
| --- | --- |
| 0x1002 | 0x1002 |
| 0x1004 | 0x1004 |
| 0x1006 | 0x1008 |
| 0x100B | 0x1013 |
| 0x1015 | 0x1016 |
| 0x1019 | 0x11FF |
| 0x1201 | 0x13FF |
| 0x1404 | 0x15FF |
| 0x1604 | 0x17FF |
| 0x1804 | 0x19FF |
| 0x1A04 | 0x1FFF |
| 0x2002 | 0x20FF |
| 0x2103 | 0x21FF |
| 0x2204 | 0x22FF |
| 0x2303 | 0x23FF |
| 0x2408 | 0x24FF |
| 0x2521 | 0x603E |
| 0x6042 | 0x605C |
| 0x605E | 0x605F |
| 0x6062 | 0x6063 |
| 0x6065 | 0x606B |
| 0x606D | 0x6079 |
| 0x607B | 0x607B |
| 0x607E | 0x6080 |
| 0x6082 | 0x6082 |
| 0x6085 | 0x6097 |
| 0x609B | 0x60FC |

### Source anomalies

[{"disposition": "reviewed", "id": "A01", "observed": "1018:00 HighLimit is 4 and DefaultValue is 3, while 1018 declares only subindices 00..03; no 04 serial entry exists", "owner_prompt": 19, "review": {"decision": "Count HighLimit4 does not create sub04. Retain actual declared children00..03; read live count before any supported traversal.", "gate_ids": ["Q01"], "status": "complete"}, "scope_id": "eds_family_unbound", "source_refs": [{"id": "EDS", "line_end": 275, "line_start": 270, "section": "1018"}, {"id": "EDS", "line_end": 286, "line_start": 276, "section": "1018sub0"}], "subject": "1018 count header and declared children"}]

### Manual index inventory

{"additional_canopen_addresses": [], "excluded_nonobject_labels": [{"label": "6000h", "reason": "dictionary group heading, not an object"}, {"label": "PA_...", "reason": "manufacturer parameter labels, not separate CANopen indexes"}, {"label": "MODBUS register columns", "reason": "alternate protocol address labels, not CANopen object keys"}], "method": "Compare explicit CANopen index/subindex references in the five original manuals with EDS sections; continued table headings checked manually", "not_declared_examples": ["0x1006", "0x100C", "0x100D", "0x1010", "0x1011", "0x1016", "0x605A", "0x6085", "0x6502"]}

### Original accounting

{"array_headers": 0, "child_entries": 126, "index_gap_ranges": 27, "manual_only_items": 53, "object_indexes": 99, "pdo_mapping_counts": {"0": 112, "1": 90}, "record_headers": 23, "reserved_entries": 4, "scalar_access_counts": {"RO": 34, "RW": 166, "WO": 2}, "scalar_entries": 202, "scalar_roots": 76, "scalar_type_counts": {"I16": 1, "I32": 7, "U16": 91, "U32": 61, "U8": 42}}
