# Application of the Nanotec CANopen promptset audit, 19 November 2026

Each finding of [audit `aud-20261119-175227-c23b91`](audit.md) was checked against this
set's files (unchanged from the audited `eb3dae18` to `1043608b`), the
[reference review](../../reference/nanotec_canopen/review.md), the original pages where a
device claim is involved (read through the pack's page-marked extracts of N5 3.3.0 and
PD4-C 3.3.0), and the shared contract as revised in `1043608b`
([canopen_shared audit application](../canopen_shared/audit_application.md), "Round 2").
Following the user's direction of 2026-11-19, every verified high finding is fixed and
the two medium ones were cheap and clearly right; nothing outside the findings was added.
Where the shared contract now states the rule, the family stage consumes it by section
instead of restating it. Only files in this directory changed; no source, test, shared
or main prompt file was edited, nothing was compiled and no port was opened.

Dispositions: 15 accepted (F1-F15), none accepted in part, rejected or deferred. Two
lead decisions of the same day then adjusted F4 and F10; see "Lead decisions" below.

## Findings

| Finding | Disposition | Evidence checked | Change (where) |
| --- | --- | --- | --- |
| F1 S3 deny before the first binding | Accepted | 02b line 8 (S1, S2, S6, S10 only); canopen_shared README S3 row and 02 CANopen-only S3 bullet (default deny lands in the S2 commit, tested after timeout and rebind; Round 2 item 3) | 02b intro, item 2 (S3 default deny in the S2 commit, claimed or extended, with direct `admitBestEffortStop` tests on a Nanotec binding, an unresolved profile, after timeout and rebind, and a same-bytes transfer refused per binding), Deliver (S3 claim row), Acceptance; 04 status and item 8 (opt-in of the shape only); execution contract "Owner and bus rules"; README stage table and seam paragraph; 01 item 1 |
| F2 Enable depends on later stages | Accepted (the first alternative) | 04 lines 21-23; 03 line 58; 04b line 4 and item 4 (auto setup uses enable); 09b item 1 ("snapshot from 03"); COMMON usable-prerequisite rule | 04b status and Deliver: item 1, the pure predicate, lands with its tests right after 03 and before 04 exposes enable; items 2-5 after 04. 03 item 3 publishes the read-only `Supervision` snapshot that 09b item 1 specifies (09b keeps the setters); 04 status and item 2 consume both (the snapshot is reported, not a gate, per 09b item 4); README stage table |
| F3 No admissible starting state | Accepted | N5 p57 (rendered transition table: shutdown 2, 6, 8; switch on 3; disable voltage 7, 9, 10, 12; quick stop 11; disable operation 5; enable 4; enable after quick stop 16; torque at the motor only in Operation enabled, Quick stop active, Fault reaction active), p58 (masks, fault reset 15), p347 (HALT valid in PP, VL, PV, PT, IP), p76 (clearing halt resumes motion); canopen_shared 02 "Operation admission", state transitions (Round 2 item 8) | 04: new "Source-state and intent table" (first word or nothing per fresh state for enable, the three release kinds, HALT, QUICK_STOP, fault reset and mode select; already enabled sends nothing; Quick stop active needs the separate enable-after-quick-stop); item 2 uses it; item 3 per-state release; item 4 HALT is the whole word 010Fh in 6061h 1-4 only (IP with 06c, homing by bit 4 in 07, others refused) and QUICK_STOP is sent only from Operation enabled (CP-D10), replacing "from other states its effect is undocumented and the operation says so"; item 6 already-selected mode sends nothing; Acceptance first-frame tests per cell and mode |
| F4 Command paths undecided | Accepted | 03 item 6; 04b item 1; 09 item 3 (RPDO1 maps 6040h and 6060h by default); N5 p283 (2300h bit 0 ON, start reloads up to 200 ms, power-on autostart sets 2300h to 1), p284 (2301h RUN, ERR); PD4C p33 (with special drive modes on, the state machine follows digital input 1 and 6040h state requests have no effect), pp326-327 (4015h:01 0 off, 2 on); PDI p5-6, p14 | 04b item 1 rewritten as an admission table (NanoJ, Plug & Drive, RPDOs mapping command objects, vendor modes with PD4-C 4015h, other writers) with observation, admitting evidence, refusal and invalidators; one single-master declaration; a controlword RPDO refuses in every NMT state without it; rules for handing authority away (control-path generation advanced before NanoJ start, Plug & Drive on or auto setup; older work sends no further step; RUN = 0 while loading is not exclusion) and the narrow NanoJ-stop and Plug & Drive-off routes that do not need the predicate; items 3 and 5; Acceptance; 03 items 3 and 6 read 4015h, 2300h bit 0 and RPDO transmission types; 04 item 4 stops need no exclusive control and report overwriting paths |
| F5 S13 harmful effect | Accepted | canopen_shared 02 S13 bullet (whole declared population, known-harmful effects, revocation, the three test classes; Round 2 item 1); 09 item 6 | 09 item 6 consumes the shared S13 rule and states the Nanotec classification (harmless synchronous TPDOs and RPDOs without command objects; harmful valid synchronous RPDO mapping a command object, unless an assigned 06c with S14 owns it; unknown unread or stale maps); NMT state never turns harmful into harmless; Acceptance lists the shared cases through the real producer with a non-target node and the README landing record |
| F6 S11 landing is not runtime evidence | Accepted | 09b items 2-3 and Acceptance; 1016h layout (time bits 0-15, producer node 16-23, N5 p177 per review NT-D24); prompt 56 measured service budget | 09b item 3: the setter needs the producer's current owner status on the binding's interface (scheduled and transmitting in the current owner generation, matching and reserved host node ID, consumer time above period plus measured worst service gap); S11's landing must define missed-service, TX retirement, identifier reservation and bus-off or reconfiguration invalidation; configured, transmitting and demonstrated reaction stay separate; item 2 refers to it; Acceptance adds absent, wrong-node, wrong-interface, stale, interrupted and margin cases with S11 compiled in |
| F7 Save standstill | Accepted | N5 p114 (standstill, no start while saving), p115 (completion by readback of 1; 1010h:01 excludes CANopen and Ethernet), p300 (606Ch reflects a sensor only where 3203h assigns one for velocity); 04 item 3 (release is not standstill) | 10 item 5: one save gate for every category and the 1010h:0Ah store of item 3 (fresh state outside Operation enabled, Quick stop active and Fault reaction active; a caller standstill declaration, since documented objects cannot establish standstill; a measured nonzero 606Ch refuses; exclusive control and no other operation of the library admitted, in flight or unresolved; starts refused until completion or unknown terminal, stops admitted and recorded; after a lost reply conflicting work stays refused until a read returns 1); item 6 binds acceptance to snapshot, category, binding and configuration generation; Acceptance cases |
| F8 1020h without a baseline | Accepted (medium, cheap) | N5 p116 (master writes the stamp before 1010h:01; any savable write, including 1010h:0xh except :01, resets both to 0; zero means changed since load), pp180-181 (date in days since 1984, time in ms) | 10 item 8: raw values plus a three-way interpretation (zero: changed since load or never stamped; matching a baseline written by this application before a confirmed 1010h:01: unchanged; otherwise UNKNOWN); the stamp write is an explicit step of a save-all; never proof of values; Acceptance cases including a category save and a declared cfg.txt |
| F9 First-contact routes | Accepted | 12A lines 7-8, 14-15, 26, 32-33; 10 items 2, 4, 7; canopen_shared 02 "First-contact questions" (Round 2 item 5); `docs/device_conflicts.md` "Conditional experiment prerequisites"; N5 p37 (switch position 1 recovery) | 12A rewritten: an evidence-path column per question (production route with prerequisites, vendor evidence or a separately assigned reviewed experiment); Q7 after 05; Q5, the PDO part of Q11 and Q12 NOT RUN through production APIs; original-value snapshots and restoration boundary instead of a presumed 88h; an observed bitrate is specimen behavior. 10 items 2 and 7 say those refusals are never opened for an experiment |
| F10 Model admission | Accepted | 02 items 1-2; 13 row "CL4-E -5VDI"; execution contract identity bullets; review section 1 and manual covers (N5 p1 W007, C5E p1 W006, CL4E p1 W002, PD4C p1 W006, checked in the extracts); NT-D29 | Execution contract: one admission table with the hardware declaration compared with the row minimum; 02 item 2 keeps -5VDI outside the scope until stage 13 stores its EDS; item 5 becomes the admission table; 03 items 1-2 and Acceptance test missing and below-minimum declarations with a matching electronic identity; README scope row. As first applied, an out-of-range declaration and the FIR-v1650/v2538 rows refused by name; lead decision 2 replaced that with "unqualified" reporting and stage 13 behavior gates |
| F11 Active closed loop | Accepted | N5 p53 (encoder commutation stays open loop until the index passes; bit 15 shows closed loop in Operation enabled), p348 (CLA bit 15), p46 per review (PT, CST and homing on block need closed loop) | 06 item 6: PT needs a fresh, same-session bit 15 = 1 in Operation enabled after the last boot-up and motor-settings write, before the mode change or first nonzero 6071h; Acceptance before/after and stale cases; 07 item 2 and Acceptance apply it to homing on block |
| F12 Cyclic RPDO transmission | Accepted | canopen_shared README S14 row and 02 S14 bullet (Round 2 item 2); 06 items 7-8 | 06 item 8: 06c runs only with S14 landed or consumed and its consumer gate passed, no family raw transmitter or S9 substitute, otherwise `UNSUPPORTED`; status line, README stage table and seam paragraph, 01 item 1 and 12 item 2 name S14 only for an assigned 06c |
| F13 Package entries late | Accepted | canopen_shared 01 intake rule "Seam S7" and 02 package bullet (install rule, `HEADERS`, exact archive set and per-header compile in the first header's commit; Round 2 item 4); 02 Deliver; 02b Deliver | 02 new item 10 (package entries and consumers in the generated headers' commit), Deliver (tooling commits only where each registered checker has its inputs), Acceptance; 02b Deliver extends the entries with `Reads.h`; README stage table |
| F14 Console before S4 | Accepted (medium, cheap) | 11 item 2; execution contract "Owner and bus rules"; canopen_shared README "Seams" | 11 item 2: before S4 only the native API and owner tests exist, console routes, common dispatch and parity stay Blocked, no second session or temporary `ProbeApp`, return work listed in stage order; item 1 and Acceptance follow; the contract's Blocked list covers 02b-11 |
| F15 Bench disposition | Accepted (medium, cheap) | 11 Acceptance; canopen_shared 02 "Bench steps and owner absence"; execution contract "Hardware"; AGENTS owner-absence line; COMMON owner-absence rule (deferred row with action and unlocked cases, NOT RUN "deferred: owner away") | 11 Acceptance: no flashing or port, an independently authorized Lichuan run may be cited as Lichuan-only evidence; execution contract "Hardware": no frame to the CL86-C nodes, and owner-dependent steps (connecting and recording a Nanotec fixture included) get one deferred-table row now, with no port access or waiver extension |

## Unresolved questions

1. **Principles input.** Answered by the repository: the pack is committed in `e02ab04f`
   (`docs/reference/canopen_principles/`; its Kollmorgen AKD manual is remote-only). The
   README and execution contract now link it instead of calling it working-tree-only.
2. **IP and bit 12.** Answered by N5 p89: in IP, bit 4 activates interpolation, bit 12
   means "IP mode active" and bit 10 is target reached or speed 0 under halt; "target
   followed" is the CSP/CSV/CST meaning (N5 pp90-94). Stage 06 item 7 now states the
   difference and shares no completion rule.
3. **Boot readiness.** Answered in part. N5 p132 and p252 (2007h:01 suppresses the boot
   loader's boot-up) and p275 (2084h delay) show that the boot-up count varies, and the
   manuals give no boot duration (review section 11). Stage 09 item 8 no longer counts:
   readiness is a fresh identity read answered after the last observed boot-up, and each
   later boot-up invalidates it. Whether the boot loader answers SDO is not documented; a
   reply from it would be invalidated by the firmware's boot-up. Spacing stays 12A Q2's
   measurement.
4. **NanoJ autostart, PDO save category, heartbeat arming and order.** Not answered by the
   files: NT-D32 (N5 p283 against FWUPD p6), NT-D09 (N5 pp111-112 against p132) and NT-D24
   (N5 p164, p177, p182, p345) stay open. Closing paths are now explicit in 12A: Q3 observes
   autostart read-only through 03; the PDO part of Q11 needs vendor evidence or a reviewed
   experiment; Q10 needs S11, 09b's runtime producer check and a reviewed plan. Host tests
   establish only refusal, uncertainty and invalidation.

## Lead decisions (2026-11-19)

1. **Single-master guard settable.** Following the owner's engineering direction of
   2026-11-17 (optional, settable guards; the user owns the responsibility), the
   single-master declaration of 04b item 1 is a guard that is on by default and
   requires the declaration; the application may switch it off per binding. With it
   off, only the declaration-dependent conditions drop; the observed rows (NanoJ,
   vendor modes and 4015h, unread maps, a valid RPDO mapping 2291h with Plug & Drive
   on) still refuse. Every result that consumed the predicate reports the guard
   setting and the declaration. Changed: 04b item 1 (guard paragraph and table rows)
   and Acceptance.
2. **No firmware lock.** Following the owner's product direction of 2026-11-12 and
   prompt 57's "Identity policy", FIR-v1650, FIR-v2538, CL4-E FIR-v2315, unlisted builds
   and a missing or out-of-range hardware declaration are not refused by name. They run
   as firmware or hardware unqualified with both identities in every result; only the
   operations that need an object or behavior the evidence shows that generation lacks
   refuse, through named behavior gates (stage 13's new table: for FIR-v1650 host-loss
   setters, 1029h setter, limit reaction, 3203h, mode change in Operation enabled,
   active-closed-loop operations, slow speed, Plug & Drive; for FIR-v2538 COB-ID and
   2007h:02 writes and the changed EDS entries). A product identity contradiction
   (1000h, 1018h:01, 1018h:02) still refuses as prompt 57 defines. Changed: execution
   contract identity bullets ("No firmware lock"); 02 items 2, 5, 6 and Acceptance; 03
   items 1-2 and Acceptance; 13 status, behavior section with the gate table, Work,
   Deliver and Acceptance; 04b item 2; 05 item 1; 09 item 4; 09b facts; 10 item 3; 11
   items 5-6; 12 item 1; 12A Q1 and Q13; README scope rows and stage table.

Remaining for a later decision: no route switches off PD4-C special drive modes
(4015h) or leaves mode -1, so such a unit stays refused by 04b item 1; models without a
stored EDS (CL4-E -5VDI, the other PD4-C steppers) stay outside the scope until stage
13, a model-scope question that decision 2 did not address.

## Checks

`python scripts/check_promptset.py` reports no error for this set; `git diff --check` is
clean for it. Nothing was compiled; no hardware or port was used.

## Round 2: application of audit `aud-20261119-184316-126cde`, 19 November 2026

Each finding of the [round 2 audit](audit.md) was checked against this set's files
(unchanged from the audited `edcf9621` to `a7656efb`), the
[reference review](../../reference/nanotec_canopen/review.md), the original pages where a
device claim is involved (the page-marked extracts of N5 3.3.0 in the multidrive pack and
of the N5 2.0.3 (FIR-v1650), FIR-v2039 update and PD4-C 3.3.0 documents in this pack), and
the shared contract and prompt 57 as revised in `a7656efb`
([canopen_shared audit application](../canopen_shared/audit_application.md), "Round 3",
themes 1 to 6). Findings F1, F5, F6 (S3 part), F8 and F10 are those themes: the Nanotec
stages now consume the shared rule by citation and add only Nanotec facts. F2, F3, F4,
F6 (halt part), F7 and F9 are family-specific. Only files in this directory changed;
nothing was compiled and no port was opened.

Dispositions: 10 accepted (F1-F10), none accepted in part, rejected or deferred.

### Round 2 findings

| Finding | Disposition | Evidence checked | Change (where) |
| --- | --- | --- | --- |
| F1 S13 node-0 NMT | Accepted | canopen_shared 02 S13 bullet and README row as of `a7656efb` (gate at the owner's NMT admission; default deny in the S2 commit; per-command NMT effects; NMT-driven revocation; theme 5); 09 item 6 and 11 item 4 at the audited revision; N5 p118 (service activity per NMT state: PDO only in Operational, only NMT in Stopped), p119 (the five commands, node 0 is broadcast), p110 and pp112-113 (saved values, 6040h and 6060h included, reload at reset), p131 (RPDO1 maps 6040h and 6060h), p250 (1F80h) | 02b item 2, intro, Deliver and Acceptance: S13's default deny (no `armSync`, `admitSync` or node-0 NMT on an interface with a Nanotec binding) lands in the S2 commit, with tests; 09 item 6: per-command Nanotec effect table (Start, Enter Pre-operational, Stop, Reset Communication, Reset Node with its condition per node; Reset Node refused while NT-D32 is open), revocation of the SYNC approval before an admitted NMT command, targeted NMT kept as its own changing request; 09 Acceptance: the shared Start cases and node-0 cases with zero NMT TX; 11 item 4: the recorded analysis admits no broadcast; execution contract "Owner and bus rules"; README stage table and seam paragraph; 01 item 1 |
| F2 Mode select and enable bypass later gates | Accepted | 04 items 2 and 6; 06 items 6 and 8; N5 p359 (6060h values, 0 = no mode assigned), p348 (bit 15 meaningful only in Operation enabled), p76 and p77 (VL and PV run toward the target speed when not halted), p82 (homing starts on bit 4) | 04 item 6: a host mode table for mode select and for enable with a mode active (0 enable only; 1, 2, 3 and 6 once their stage lands; 4 only inside 06b's operation, enable in 4 refused; 7-10 only with an assigned 06c; -1 and -2 refused; others refused); VL and PV enable needs a fresh zero target (host policy); item 2 uses the table; Acceptance tests the generic routes with zero TX; 06 item 6 states that 04's table routes mode 4 through its check; 06 item 8 keeps 7-10 unavailable; README stage table |
| F3 Auto setup path | Accepted | 04b items 1 and 4; 04 item 6; N5 pp40-42 (prerequisites, steps, the p42 example 0006h, 6060h = FEh, 0007h, 000Fh, 6061h = FEh, 001Fh, wait for bit 12, 0000h; automatic save to 1010h:05/06; restart required), p96 (bit 4 rising starts, bit 12 aligned, bit 10 indexed) | 04b item 4 rewritten: its own sequence, not 04's mode select or enable; admission (declarations, identity, preflight, one deadline, predicate before the generation advance, admissible start states without Operation enabled); operation-local authority after the generation advance; steps with per-step confirmation; coordinate invalidation before the trigger; per-step failure (lost mode reply, lost enable word, lost trigger reply, deadline, fault, stop) ending `UNKNOWN` with ordinary control refused until a boot-up; observed completion distinguished from a possible persistent change. Correction: the old text ended the sequence with "bit 4 = 0"; the p42 example's final word is 0000h. 04b item 1 names the operation-local authorization; Acceptance cases |
| F4 FIR-v1650 Plug & Drive lockout and bit 15 | Accepted | 13 behavior section and gate table; 04b item 1; 03 items 3-4; N5 pp280-282 (2290h, 2291h and 2292h all "FIR-v1748-B531667"); N5 2.0.3 extract (no 2290h, 2291h or 2292h anywhere); FWUPD p13 (6041h bit 15: FIR-v1650 "Closed Loop Available", FIR-v2039 "Closed Loop Active") | 13: three applicability states (present, absent by the row's evidence and decided on explicitly, unread, which never becomes absent) and per-row typed decoding; `FIR1650_PLUG_AND_DRIVE` refuses only `preparePdiControl`, the predicate decides on the documented absence without an upload; `FIR1650_CLOSED_LOOP_ACTIVE` notes the decoding; Acceptance. 04b item 1 Plug & Drive row admits on that absence and refuses an unread 2290h where it exists; Acceptance. 03 item 3 "not in this generation" and unread; item 4 bit 15 per row; Acceptance. Execution contract identity bullet |
| F5 Identity refusal versus the stop exemption | Accepted | Prompt 57 "Identity policy" as of `a7656efb` (motion rule with the stop exemption and its own checks, an NMT command never that stop, "Admission by outcome", vendor-object rule); canopen_shared 02 "Identity and device evidence" (theme 1) | Execution contract: product identity is prompt 57's policy (outcomes, admission by outcome, stop exemption covering 04's `HALT`, `QUICK_STOP` and the opted-in S3 shape; release, fault reset, mode select, NanoJ stop, Plug & Drive off and NMT are changing requests); 03 item 1 computes prompt 57's outcome over 1000h, 1018h:01 and 1018h:02 and reports the firmware row beside it; 04 item 4: the exemption with the stop's own reads (6041h, 6061h, 6040h, 603Fh, 605Ah or 605Dh) and table, queued stops not withdrawn, other pending steps retired, a refused stop naming the independent stop path; 04b item 1: NanoJ stop and Plug & Drive off are vendor downloads, never exempt; 12A Q1; 13; Acceptance in 03 and 04 (both stop entry points under each refusing outcome) |
| F6 Halted and unresolved predecessors | Accepted | N5 p57 (commands defined by bits 7, 3, 2, 1, 0; quick stop transition 11 only), p58 (fault reset by rising edge), p60 and p347 (bit 8 triggers a halt with 605Dh in PP, VL, PV, PT, IP), p68, p76, p77, p80 and p89 (clearing bit 8 restarts motion toward the target in those modes), p82, pp91-93 and p96 (no bit 8 function in homing, CSP, CSV, CST, auto setup); no page gives a precedence between halt removal and transition 11; principles pack CP-D10; canopen_shared 02 S3 bullet (theme 4) | Halt decision: the quick-stop word preserves bit 8 when the predecessor has it set (010Bh), because 000Bh would combine a documented restart with the stop. 04 item 1 (bit 8 kept in words sent from a halted Operation enabled), item 4, the table split into unhalted and halted Operation enabled rows (release words 0107h, 0106h, 0100h; HALT already reached; enable-after-quick-stop keeps bit 8); item 8 consumes the shared S3 rule with the per-state review from those pages, candidate shape 010Bh, 000Bh rejected; Acceptance; README stage table |
| F7 Category save bypasses the PDO gate | Accepted | 10 items 5 and 7; 12A Q11; N5 p110 (only whole categories are saved; saved values reload at switch-on or reset), pp111-112 (PDO objects in communication), p132 (remap saves with application, NT-D09); C5E p108 and CL4E p106 (cfg.txt wins at boot); review section 10 | 10 item 5: a PDO-persistence condition on the category write for 1010h:01, :02 and :03 (the RAM PDO configuration must be the one loaded at the last boot-up this owner observed: boot-up seen in the owner session, no PDO write by this library since and none unresolved, no cfg.txt; guard-off reported), otherwise zero TX; item 7 says the gate is on the category write, not on a "PDO save" label; Acceptance (remap, unresolved partial remap, no observed boot-up, cfg.txt, save-all; 04-06 unaffected); 09 item 4; 12A Q11 |
| F8 Timed runs and their stop | Accepted | canopen_shared 02 "Operation admission" timed-runs bullet as of `a7656efb` (theme 3); 06 items 3 and 6 | 06 item 3 consumes the shared timed-run design and names the Nanotec stop rows (04's `HALT` in 6061h 2-4 and `QUICK_STOP`, halted or not) that exist for every state the run reaches; the urgent stop is the application's separate operation; item 6 refers to it; Acceptance adds the shared cases |
| F9 Q2 and Q13 routes | Accepted (medium, cheap) | 03 item 3 plan; 09 items 4 and 8, Deliver; 12A Q2 and Q13; N5 pp251-252 (2007h:01), p275 (2084h); review rows for 2007h:02 (EDS N5-1-2/2538 only, NT-D02) | 03 item 3: a Boot group (2007h:01, 2084h; 2007h:02 only on the FIR-v2538 row), read effects reviewed in 02; Acceptance tests omission per scope; 12A Q2 and Q13 name 03's groups and 09's observer and `PdoConfig` readback and run after 09 |
| F10 Registration without an identity outcome | Accepted (medium, cheap) | canopen_shared 02 "Registration acceptance" as of `a7656efb` (theme 6); 02b intro, item 3 and Acceptance; 03 Acceptance | 02b intro and Acceptance: registration delivers no identity outcome, so every identity-dependent write and motion route is shown unavailable with zero changing TX and the probe runs only its reviewed read; 03 Acceptance closes the identity gate with the shared cases on the admission check every later route calls, before any such route is callable |

### Round 2 unresolved questions

1. **Boot-loader SDO and readiness.** Not answered by the files. N5 p132 and pp251-252
   document only the boot loader's own boot-up message and its suppression by
   2007h:01; no page documents whether the boot loader answers SDO or gives a positive
   firmware-readiness marker, and the manuals give no boot duration (review section 11).
   09 item 8's rule stands (readiness only from an identity read answered after the last
   observed boot-up, each later boot-up invalidating it), with the residual risk that a
   missed firmware boot-up and a boot-loader reply cannot be told apart from documents.
   12A Q2 now records, as a fixture fact, whether an identity read between two boot-ups
   is answered and with which values.
2. **Heartbeat delivery bound and the queued 1016h write.** Not answered by the files.
   09b item 3 checks the producer's owner status at admission and requires the consumer
   time above the period plus the measured worst service gap (prompt 56); neither file
   bounds TX queueing or arbitration delay to the drive, and the check is not repeated
   at the 1016h TX. Both belong to the S11 landing and the prompt 56 measurement; lead
   decision 2 below records them as an open request on seam S11.

### Round 2 lead decisions (2026-11-19)

1. **PDO-persistence check as a settable guard.** The F7 check (a boot-up observed in
   the owner session, no PDO write since, no cfg.txt, for 1010h:01, :02 and :03) stays,
   as a guard on by default and settable per binding, following prompt 77 item 2 (an
   admitted result names a guard that was off) and the owner's engineering direction
   of 2026-11-17 (explicit saves allowed, optional settable guards, user
   responsibility). With the guard off the save result names the guard, states that a
   PDO map change may be persisted (NT-D09) and carries the readback of 1400h-1407h,
   1600h-1607h, 1800h-1807h and 1A00h-1A07h taken in the operation before the save.
   Changed: 10 items 5 and 7 and Acceptance; 09 item 4; 12A Q11 (runs with the guard
   on).
2. **S11 delivery bound and producer recheck.** Recorded as an open request on seam
   S11, which the lead adds to canopen_shared; no stage change beyond citing it. Changed:
   09b item 3 cites the open request.
3. **Halt bit and 000Bh.** The lead adds a shared state-transitions rule to canopen_shared
   02 (a stop word never clears a set halt bit unless the family's evidence shows that
   the quick stop takes precedence); the 010Bh row already satisfies it. Changed: 04
   item 4, the halted table row and item 8 cite canopen_shared 02 "State transitions".

### Round 2 checks

`python scripts/check_promptset.py` reports no error for this set; `git diff --check` is
clean for it. Nothing was compiled; no hardware or port was used.

## Round 3: application of audit `aud-20261119-192232-a1c214`, 19 November 2026

Each finding of the [round 3 audit](audit.md) was checked against this set's files
(unchanged from the audited `6ff80ed1` to `c67e48dd`), the
[reference review](../../reference/nanotec_canopen/review.md), the page-marked extract
of N5 3.3.0 in the multidrive pack where a device claim is involved, prompt 57, prompt
77, and the shared contract as revised in `c4e5574f`
([canopen_shared audit application](../canopen_shared/audit_application.md), "Round 4").
F1, F2 and F3 are that round's themes 3, 4 and 1: the stages now consume the shared rule
by citation and add only the Nanotec facts. F4-F9 are family-specific. Only files in
this directory changed; nothing was compiled and no port was opened.

Dispositions: 8 accepted (F1-F8), 1 accepted in part (F9), none rejected or deferred.

### Round 3 findings

| Finding | Disposition | Evidence checked | Change (where) |
| --- | --- | --- | --- |
| F1 NMT bootstrap exception | Accepted | Prompt 57 "Admission by outcome" (bootstrap reads and the targeted Enter Pre-operational on a fresh heartbeat or node-guarding Stopped report, every outcome except CONTRADICTED; otherwise the node is reported stranded); canopen_shared audit application round 4 theme 3; contract lines 98-100 restated the rule without the command; N5 p118 (only NMT in Stopped), NT-D17 (heartbeat or guarding in Stopped not shown) | Execution contract: "Admission by outcome" consumed by reference, its bootstrap command named and assigned to 09; 09 item 6: the owner's targeted NMT route applies prompt 57's rule by reference, the Stopped report may never arrive on this device (N5 p118, NT-D17), then the node is reported stranded, nothing sends the command implicitly; 09 Acceptance: admitted on fresh Stopped with no outcome, refused under CONTRADICTED and with stale or missing state evidence, never sent by recovery or probe |
| F2 S11 delivery bound and handoff recheck | Accepted | canopen_shared README S11 row (TX delay under load counted; producer rechecked when the `1016h` write is handed to the controller) and audit application round 4 theme 4; 09b item 3 and Acceptance at the audited revision | 09b item 3: both checks are prerequisites of every nonzero 1016h:01 write (consumer time above period plus S11's delivery bound, of which the prompt 56 gap is one part; a producer lost while the write waited withdraws it with nothing sent); the nonzero setter stays BLOCKED until S11 provides them; the "open request" text is gone. Acceptance: a consumer time that passes the service gap but not the delivery bound, and a producer invalidated between admission and handoff, each with zero 1016h TX. README stage table |
| F3 Stop path after a lost start | Accepted | canopen_shared 02 "Operation admission", timed runs (a stop row is not a stop path; S3 shape or a declared independent stop, else refused before the first changing TX) and audit application round 4 theme 1; execution contract (S10 `NONE` until markers are reviewed); 04 item 8 (S3 opt-in per binding) | 06 item 3: the unresolved-route branch with the S3 opt-in caveat and the declared independent stop named in the result; 06 item 6 applies it to PT; Acceptance: refusal with neither, a lost start reply with the S3 shape (run retired, at most one 010Bh frame, uncertainty kept, no standstill), admission with a declared stop; README stage table |
| F4 Retained target on an enabled mode change | Accepted | N5 p58 ("set or changed at any time"), FWUPD p6 (from FIR-v2039 mode change in Operation enabled allowed; a command starts at once if bit 4 is set), N5 pp76-79 (VL ramp generator follows 6042h; PV accelerates to 60FFh once the mode is selected and Operation enabled is reached), pp80-81 (PT, 6071h), pp112-113 per 10 item 6 (6042h, 60FFh, 6071h savable). No page states what an enabled mode change does with a retained nonzero target | 04 item 6: entering VL, PV or PT in Operation enabled needs a fresh zero destination target read under 04b item 1's predicate, with the order (state reads, target read, 6060h as the only changing step, 6061h), refusal on a nonzero or unread target whatever bit 8 shows, no write of another mode's target, a lost 6060h reply leaving the mode UNKNOWN; table rows 2/3 and 4; 06 item 6: PT entry order with a fresh zero 6071h; Acceptance in 04 and 06; README stage table |
| F5 Set halt bit reported as attained | Accepted | N5 p60 and p347 (bit 8 halts in PP, VL, PV, PT, IP only), p82, pp91-93, p96 (no bit 8 function in homing, CSP, CSV, CST, auto setup); p57 (commands defined by bits 7, 3, 2, 1, 0: 0100h is disable voltage, transition 12 from Quick stop active); canopen_shared 02 "State transitions" | 04 item 1: a set bit is a requested halt in those five modes, never reported as attained; item 3: Quick stop active release keeps bit 8 (0100h); table: the bit-set Operation enabled `HALT` cell reports "requested" in 6061h 1-4 without an attained-halt or standstill claim and refuses in other modes; the Quick stop active `DISABLE_VOLTAGE` cell sends 0000h or 0100h by the fresh bit 8; Acceptance asserts results as well as frames; README stage table |
| F6 Limit admission before the first motion route | Accepted (the first alternative) | Prompt 77 items 1-2 (assigned limit active, stale input or undecodable assignment refuses before TX; both directions until the sign is measured; guard on by default, settable per axis; stops never refused); N5 p66 (limit stores the position, sets bit 7, runs 3701h; factory -1 no reaction; opposite travel always possible), p97 and p411 (60FDh bits 0-1 report the NLS and PLS functions), p98 (3240h:01 bits 0-1 switch them on); 03 reads 3240h (config group) and 60FDh (`prepareState`) | 07 item 8: a pure limit-admission predicate over 03's fresh reads with Nanotec input facts and prompt 77's policy, landed in its own commit right after 03 and before 05's move (status line, Deliver `Limits.h/.cpp` and its test); 05 status and item 5, 06 items 3 and 6 consume it; Acceptance in 05, 06 and 07 (predicate cases in item 8's commit, route refusals with zero TX in each consumer, stops admitted); README stage table |
| F7 PDO-persistence guard claim | Accepted (the claim narrowed, both guards kept settable) | 10 item 5 at the audited revision; 04b item 1 (single-master guard settable per binding); N5 p110 (saved values reload at boot); round 2 lead decision 1 | 10 item 5: the guard protects against this library's own PDO changes only; RAM holding the boot-loaded configuration is stated only with both guards on and only as far as the declaration holds; with the single-master guard off the result says another writer's change is not excluded and carries the map readback. 09 item 4 says the same. 12A Q11 runs with both guards on and compares 09's `PdoConfig` readback before the save with one taken after the boot-up. Acceptance: an out-of-band map change with only the single-master guard off, and both guards on |
| F8 COB-ID reservation and activation | Accepted | N5 p127 and p129 (PDO 5-8 CAN-IDs free "with the restriction that it not be used by other services"; a change takes effect only after the controller or communication is restarted), p110 (saved values reload at switch-on or reset), p119 (Reset Communication resets the CANopen settings), NT-D09 and 10 item 7 (PDO save gated), 09 item 5 (delayed samples), canopen_shared 02 S13 and S14 (COB-ID reserved on the interface) | 09 item 4: a COB-ID change consumes 11 item 4's identifier plan at its admission, the candidate reserved beside the old ID, configured, active and pending values kept apart, readback before activation never wire proof, whether a RAM-only change survives its own activation recorded as undocumented, both IDs reserved and their routes and the node's S13 approval unqualified until an observed boot-up and fresh readback; 11 item 4 reruns the check per change; 09 Acceptance; README stage table |
| F9 Boot loader and readiness | Accepted in part | N5 p132 and pp251-252 (2007h:01 BL Config, savable in category CANopen, preset 0; with 1 only the firmware sends a boot-up), pp246-249 (1F50h, 1F51h, 1F57h: no documented value that tells the loader from the firmware), p344 (4042h: versions and fieldbuses only), p275 (2084h); review section 11 (no boot duration) | 09 item 8: responding server, identity outcome and firmware readiness kept separate; an identity reply after the last observed boot-up re-establishes the outcome, not readiness; READY only with 2007h:01 = 1 read after an earlier boot-up and not changed by this library since, or a recorded fixture observation for that model and firmware row (12A Q2); otherwise UNKNOWN in every result; no delay, retry, count or build list makes it READY. Not accepted: UNKNOWN does not refuse changing work by itself (owner direction: no firmware lock; on factory settings 2007h:01 = 0 it would refuse every changing operation after each boot-up); an application may require READY as its policy, as 09b item 4. 09 Acceptance; 12A Q2; README stage table. See the lead question below |

### Round 3 unresolved questions

1. **Boot loader versus firmware.** Not answered by the files. N5 p132 and pp251-252
   document only the boot loader's boot-up and its suppression; 1F50h, 1F51h, 1F57h
   (pp246-249) and 4042h (p344) give no state that distinguishes the loader; no page
   says whether the loader answers SDO. 09 item 8 therefore admits only two
   distinguishing observations (2007h:01 = 1 in force, or a recorded fixture
   observation for the row through 12A Q2) and otherwise reports readiness UNKNOWN.
2. **Retained targets and COB-ID activation.** Not answered by the files. N5 p58, FWUPD
   p6 and N5 pp76-81 give no disposition of a retained 6042h, 60FFh or 6071h on an
   enabled mode change, so 04 item 6 and 06 item 6 now require a fresh zero target and
   write none. N5 p127 and p129 put COB-ID activation at a restart, while p110 and p119
   reload saved values or reset the CANopen settings then; whether a RAM-only change is
   the one activated is undocumented, so 09 item 4 keeps configured, pending and active
   values apart and takes the active value only from a readback after an observed
   boot-up.

### Round 3 open for the lead

- F9: whether readiness UNKNOWN should become a settable guard (on by default would
  refuse every changing operation after each boot-up on factory settings until the
  application switches it off, sets and saves 2007h:01 = 1, or records 12A Q2) or stay
  a reported fact with application policy, as applied.

### Round 3 checks

`python scripts/check_promptset.py` reports no error for this set; `git diff --check` is
clean for it. Nothing was compiled; no hardware or port was used.

### Round 3 lead decisions

1. F9 readiness: UNKNOWN firmware readiness is reported and refuses nothing; refusing would block factory
   units (2007h:01 = 0) after every boot-up, against the owner's no-lock direction. An application may
   refuse on it; no guessed delay counts as evidence.
2. canopen_shared 02 changed after this round (commit `328651b6`): with an unknown halt state a stop word
   sets bit 8 and is never refused for that reason alone; homing is a host-ended run with a caller-supplied
   latest interruption time. Stage 04's 010Bh rows and 07's homing already agree; the next round checks the
   wording.

## Round 4: application of audit `aud-20261119-195033-e8234b`, 19 November 2026

Each finding of the [round 4 audit](audit.md) was checked against this set's files
(unchanged from the audited `f864cbb9` to `c138e959`), the
[reference review](../../reference/nanotec_canopen/review.md), the page-marked extracts
of N5 3.3.0 (multidrive pack), N5 2.0.3 (FIR-v1650) and the FIR-v2039 update
instructions (this pack) where a device claim is involved, prompt 57, and the shared
contract as revised in `328651b6` and `c138e959`
([canopen_shared audit application](../canopen_shared/audit_application.md), "Round 4",
themes 6-11). F1 falls under themes 6, 9 and 10, F2 under theme 7 and F6 in part
under theme 11: the stages consume those rules by citation and add only Nanotec facts. Round 3
lead decision 2 said that 04's 010Bh rows and 07's homing already agreed with the
amended rules; F1 and F2 show they did not, and that claim is withdrawn. Only files in
this directory changed; nothing was compiled and no port was opened. The owner's
direction holds: the settable guards stay settable, readiness UNKNOWN stays a reported,
nonblocking fact (round 3 lead decision 1), and no firmware row is refused by name.

Dispositions: 10 accepted (F1-F10), none accepted in part, rejected or deferred.

### Round 4 findings

| Finding | Disposition | Evidence checked | Change (where) |
| --- | --- | --- | --- |
| F1 Unknown halt refuses a stop | Accepted | 04 table paragraph at the audited revision ("an undecodable or failed read refuses every column"); canopen_shared 02 "State transitions" as of `c138e959` (unknown halt: a stop word sets bit 8, never refused for that reason; a stop word reviewed from every reachable state is admitted without a fresh report; halt evidence follows the words sent); 04 item 8's review of 010Bh (N5 p57, p58; bit 8 halts only in PP, VL, PV, PT, IP, N5 p60, p347, no special function in homing, CSP, CSV, CST, auto setup, N5 p82, pp91-93, p96) | 04 table paragraph: halt state only from a fresh 6040h read after the last possibly sent word; unknown-halt branch with fresh 6041h and 6061h (`HALT` 010Fh in 6061h 1-4, `QUICK_STOP` 010Bh, result records halt unknown); every-state branch on a usable route with an aborted, malformed or stale 6041h or 6061h (`QUICK_STOP` 010Bh, drive state unknown; `HALT` refused); a failed 605Ah or 605Dh read refuses neither; non-stop columns still refuse; item 4's refusal sentence narrowed to `HALT` without a mode and an unresolved route without S3; Acceptance tests both branches on a usable route with no S3 shape, apart from the S3 case. 06 item 3: 010Bh with unknown halt, and 04's every-state branch as the stop path on a resolved route with a stale, missing or alarmed report |
| F2 Homing interruption unbounded | Accepted | 07 items 3-4 and Acceptance; N5 p82 (homing: only bit 4 special, "performed until either the reference position is reached or bit 4 is reset"), p347; 04 item 4 (homing excluded from `HALT`) and item 8 (bits 4-6 = 0 end a running homing); canopen_shared 02 "Operation admission" timed runs as of `c138e959` (homing clause, stop path on unresolved and on stale-report resolved routes) | 07 item 4: every moving method is a host-ended run (caller latest interruption time, optional travel bound from fresh 6064h, stop budget checked at preparation, transfers capped at interruption due); `prepareHomeInterrupt`, a separate `NOT_REPEATABLE` operation sending 000Fh, or 010Fh with bit 8 set or unknown, only from Operation enabled in mode 6; 04's `QUICK_STOP` also ends homing; stop path checked before the trigger (S3 shape or declared stop for an unresolved route, 04's every-state branch for a stale report), otherwise refused; `needsStop` and UNKNOWN from the trigger's possible TX; method 35 not a run. Deliver and Acceptance (the shared cases, the interruption word, travel bound). 04 item 4 names the operation; execution contract: the prompt 57 stop exemption covers 07's homing interruption. README row 07 |
| F3 Identifier check scheduled late | Accepted (the second alternative) | 09 item 4 and Acceptance; 11 status ("runs after the implemented subsets of 02b-10") and item 4; COMMON usable-prerequisite rule | 09 item 4: the COB-ID change is BLOCKED (zero TX, reason named) until 11 item 4's check and atomic reservation have landed with their tests; valid-bit steps, remapping and readback stay available; Acceptance: BLOCKED refusal now, the COB-ID cases in the commit that lands the check. 11 item 4: the reusable per-interface check with atomic old and candidate reservation lands first in its own tested commit, unblocks 09 and runs 09's COB-ID cases; topology analysis and coexistence tests stay in 11. 10 Acceptance: the COB-ID-write case once unblocked. README rows 09 and 11 |
| F4 Targeted NMT admission | Accepted | 09 item 6 table and text, Acceptance; prompt 57 (an NMT command to the binding is a changing request, never a stop; "Admission by outcome" with the bootstrap command); N5 p118 (Stopped: only NMT; PDO only in Operational), p119; NT-D32 | 09 item 6: a targeted command is admitted only under prompt 57 and the same table row evaluated on the addressed node from fresh reads (Stop and Reset Communication need a fresh state without torque, Start and Enter Pre-operational the map condition, Reset Node refused while NT-D32 is open, a command without a row refused); the bootstrap Enter Pre-operational stays the one exception, needing no map condition because leaving Stopped for Pre-operational activates no PDO (N5 p118); Acceptance: targeted refusals on torque, unread or stale states, Reset Node and an unadmitted command RPDO, each with zero NMT TX. README row 09 |
| F5 Readiness and activation evidence | Accepted | 09 items 4 and 8, Acceptance; 12A Q2; N5 p110 (saved values reload at switch-on or reset), p132 and pp251-252 (2007h:01 savable in category CANopen, preset 0; with 1 only the firmware sends a boot-up), p127 and p129 (COB-ID change active after a restart); AGENTS.md lines 73 and 79; round 3 lead decision 1 | 09 item 8: READY needs 2007h:01 = 1 in force at that boot-up, shown by a read after the preceding boot-up with no 2007h write, CANopen save or 1011h restore by this library across both, under the single-master declaration; a RAM-only 1, or the guard off, is not that evidence; a single unanswered identity request is fixture evidence only; UNKNOWN still refuses nothing. Item 4: a readback after a boot-up activates a pending COB-ID only with READY for that boot-up, or for one TPDO route with a frame on the new identifier with the confirmed length; otherwise pending, both IDs reserved, nothing else refused. Acceptance: RAM-only suppression lost at reboot, guard off, a silent request, a pending change after an UNKNOWN boot-up, TPDO activation by frame. 12A Q2: fixture evidence only |
| F6 Save exclusion and NMT | Accepted (verified, not only probable: 10 item 5 named no NMT, restart or restore route) | 10 items 5 and 8 (2800h:01 restart, 1011h restore); 09 item 6 Reset Communication row (fresh state without torque, which a save also needs); N5 pp114-115 (standstill, fieldbus may be disturbed, a few seconds); canopen_shared 02 S13 as of `c138e959` (NMT rechecked at hand-off; an in-flight save is a conflicting operation) | 10 item 5: the reservation also covers persistence and NMT operations; it refuses the restart, restore and other saves and is visible at the owner's NMT admission (targeted or node-0 commands including the node refused; one admitted earlier retired at hand-off); an externally observed boot-up during an in-flight or unresolved save ends it UNKNOWN, never success; stops stay admitted; Acceptance cases. 09 item 6: both NMT admissions rechecked at hand-off, the save reservation named. README row 10 |
| F7 VL and PV start write | Accepted | 06 items 1, 3, 4; 04 item 4 and the host mode table rows 2 and 3; N5 pp76-77 (VL and PV: bit 8 = 1 stops the motor, a 1-to-0 transition accelerates to the target; the VL ramp generator follows the target), p60, p347 | 06a: start rows after item 4 (start only from Operation enabled with 6061h already 2 or 3; fresh 6041h, 6061h, 6040h and target; settings confirmed before the start-capable write; unhalted with a zero target: the target write starts; halted: target, matching readback, then 000Fh; unhalted nonzero target, unknown halt, unread target and other predecessors refused; `needsStop` from the target write's possible TX; stop due in staging, lost replies); Acceptance. README row 06 |
| F8 Positive stop test at registration | Accepted (medium, cheap) | 02b item 2 and Acceptance; 04 items 4 and 8; canopen_shared 02 "Registration acceptance" | 02b Acceptance: isolated profiles, caches and results, Lichuan stops unchanged and a Nanotec stop request refused with zero TX; the positive case moves to 04. 04 Acceptance: a Lichuan and a Nanotec axis each stopped by its own family's stop, the S3 opt-in tested separately. README row 04 |
| F9 Save preflight and absent objects | Accepted (medium, cheap) | 10 items 5-6; 13 FIR-v1650 row, applicability states and `FIR1650_FEEDBACK_SELECTION`; N5V2 extract (no 2290h, 3203h, 3701h or 6007h anywhere in the text) and its application-category list (N5V2 pp101-102: 6040h, 6042h, 6060h, 6071h, 607Ah, 60FEh, 60FFh, 2300h, 2800h, 3700h, none of 2290h, 3701h, 6007h); N5 p300 | 10 item 5: 3203h follows stage 13 applicability (not requested where absent, so the gate does not refuse the save and no 606Ch veto applies, the result saying so; unread where present refuses, naming it). Item 6: the snapshot is row-scoped (absent objects listed "not in this generation" and never requested; a failed read of a present one refuses; an unqualified row's category membership reported unverified). Acceptance cases |
| F10 First-contact restoration | Accepted (medium, cheap) | 12A Work paragraph, Q6, Q7, Q9 and Acceptance; 07 item 3 (method 35 invalidates origin and reference generations); canopen_shared 02 "First-contact questions" | 12A Work: a restoration boundary per changing question; settings with an admitted write route restored to the snapshot through it and read back; drive state, mode, controlword, position and coordinate references not replayed (no-torque state only through 04's release); a change no route undoes runs only with prior acceptance in the fixture record, otherwise NOT RUN; never a raw controlword. Q9: an accepted new reference or a defined re-reference procedure, otherwise NOT RUN, and 07 item 4's interruption time and stop path. Acceptance |

### Round 4 unresolved questions

1. **Loader versus firmware, and COB-ID activation.** Answered in part. The only
   documented observation that excludes the boot loader's boot-up is 2007h:01 = 1 in
   force at that boot-up (N5 p252); N5 p132, pp246-249 (1F50h, 1F51h, 1F57h) and p344
   (4042h) give no marker, and no page says whether the loader answers SDO. 09 item 8
   now requires the persisted value (read after the preceding boot-up, no write, save
   or restore across, single-master declaration in force), so a RAM-only value does
   not count. COB-ID activation: N5 p127 and p129 put it at a restart, and no object
   reports the active identifier apart from the configured one. 09 item 4 accepts a
   readback in a session that 09 item 8 reports READY, or, for a TPDO, a frame on the
   new identifier with the confirmed length; an RPDO has no drive-side marker, so its
   change stays pending without READY. A model-specific positive marker remains a
   fixture or vendor question.
2. **Legacy absence on C5-E and PD4-C.** Answered in part by this pack. The FIR-v2039
   update instructions cover the product groups C5, C5-E, N5, CL3-E, NP5 and PDx-C
   from FIR-v1650 (FWUPD p1) and list 1006h, 1016h, 1019h, 1F80h, 2290h-2292h, 3203h
   and 3701h among the objects new in FIR-v2039 (FWUPD pp11-12): group-scoped support
   for those absences, including the 2290h absence on which 04b's Plug & Drive row
   admits without an upload. The same list names 1029h, which N5V2 documents (NT-D31),
   so it is not exact. 6007h is not in that list; its absence on C5-E and PD4-C rests
   on N5V2 alone. The stock C5-E 2.0.1 and PD4-C 2.0.2 manuals are remote-only and stay
   stage 13's missing evidence. No stage change: absence there drives only refusals,
   read omissions and, after F9, a save snapshot that reports its membership
   unverified.

### Round 4 lead decisions (2026-11-19)

1. **Homing interruption is a stop.** `prepareHomeInterrupt` counts as an explicitly
   requested stop under prompt 57's exemption: it is reviewed and cannot enable or start
   motion. It keeps its own route, generation and source-state checks. Changed: 07 item 4
   states the reason; the execution contract's exemption bullet (already applied) stands.
2. **COB-ID changes stay BLOCKED** until 11 item 4's identifier check lands as its own
   first commit, which is equivalent to landing it before 09's writes. Confirmed as
   applied; no further change.
3. **Caller declaration for a pending COB-ID.** Following the owner's engineering
   direction of 2026-11-17 (user responsibility, settable), the application may declare
   the new identifier active after a power cycle or Reset Communication that it
   records; the route then becomes active, and every result that uses it names the
   declaration. READY or an observed TPDO frame still activates without a declaration,
   and readiness UNKNOWN still refuses nothing. This replaces the pending-only outcome on
   factory settings noted while applying F5. Changed: 09 item 4 and Acceptance.
4. **VL and PV runs do not enable or select a mode.** The application composes 04's
   operations first. Confirmed as applied; no further change.

### Round 4 checks

`python scripts/check_promptset.py` reports no error for this set; `git diff --check` is
clean for it. Nothing was compiled; no hardware or port was used.
