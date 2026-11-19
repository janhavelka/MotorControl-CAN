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
