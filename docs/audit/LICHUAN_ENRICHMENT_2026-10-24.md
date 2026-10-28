# Concrete Lichuan evidence and prompt enrichment

The 50-prompt sequence now has focused concrete device/protocol evidence in 40 assignments, plus a shared mandatory source-check and correction loop. The numbering, prerequisite graph, single-branch workflow and per-prompt commit/sync requirements remain intact. This work prepares the coding assignment; no library or hardware support was implemented.

Start with the [evidence reading map](../prompts/DEVICE_EVIDENCE.md). The complete records are:

- [Object facts](LICHUAN_OBJECT_FACTS.md): all 99 indexes and 202 typed EDS entries, communication records, default PDO mappings, monitoring/settings, digital I/O, all 33 tuning entries, DIP/node setup and persistence.
- [Motion facts](LICHUAN_MOTION_FACTS.md): exact object widths, source ranges/units, controlword values and edges, three status layouts, velocity ordering, four homing methods, fault/reference actions and timing limits.
- [Public implementation audit](LICHUAN_PUBLIC_IMPLEMENTATIONS.md): bounded official/public search, one Lichuan-mentioned example with concrete defects, pinned CANopenNode/canopen-python protocol evidence and 17 host-only byte fixtures.
- [Machine-readable intake](../reference/extracted/LC_StepDriverCAN_V1.1.object_inventory.json): literal EDS fields, hexadecimal addresses, original sections and line spans. It is not the reviewed production ledger.

The [fresh source check](LICHUAN_SOURCE_RECHECK.json) re-downloaded all five vendor PDFs and the EDS from their recorded manufacturer URLs. All six files match the retained hashes. Product/download HTML changed bytes, without establishing changed protocol documentation. The indexed historical English PDF still returns 404. No verified official Lichuan GitHub driver or exact CL86-C firmware match was established by the recorded search.

The public-code pack contains 17 unchanged upstream files and their license texts, a search log and a vector file. The original Lichuan pack gains the complete derived EDS intake. Existing vendor documents and baseline copies were preserved. References are research material and are excluded from the future core package; actual reused/linked code retains its required notices.

## Findings that affect implementation

The most consequential conflict is `0x2400`: official tables disagree about the subindices for output polarity and DO0/1/2, and about output functions 2/3. The Chinese manual contradicts itself. This blocks the dependent write interpretation; it cannot be resolved by choosing a majority or trying both addresses on hardware.

Status bits differ between EN86, ZH86 and OP86. Mode-display `0x6061` must not become writable because of a translated procedure. Velocity sign and startup order need firmware evidence. `0x2201` default 0 and `0x2202` default 10 conflict with the documented working choices. Node object `0x2203` exists, but activation, persistence and DIP precedence are not established. Fault-clear `0x2301` values 0/1 are documented in EN86, while its edge/rearm behavior is not; this differs from explicitly edge-triggered position clear `0x2302`.

Ramp times are milliseconds; approximately 10 ms enable spacing is a procedural example, not a hard response-time bound. SDO/boot/save/stop deadlines, host-loss stopping, actual-position scale and several tuning/filter units remain unknown. Standard quick-stop objects, actual-following-error feedback and an automatic-tuning command cannot be supplied from generic CiA402 or product marketing.

The public Lichuan example uses wrong SDO upload commands, wrong homing widths, shared status caches, unchecked writes and hidden saving during zeroing. Its constants become source-reviewed counterexamples and host regressions, not copied implementation. CANopenNode and canopen-python establish their own protocol behavior; their configured timeouts/retries do not establish Lichuan guarantees.

## Review, workload and verification

Three parallel researchers extracted motion, objects and public code, then independently reviewed the merged prompt additions. Conflicting initial interpretations were checked against original pages, including rendered status and I/O tables. The resulting prompts require the coder to correct the ledger, generated files, implementation, tests, Doxygen and guides together when new hard evidence changes a claim.

The live MotorControl-RS, ADS1115, INA228, SHT3x-main and FieldCore revisions remained unchanged and clean compared with the preceding audit. Relevant RS timing/action/read contracts and I2C read-side-effect/health ownership sources were reread; their paths and full revisions are in the detailed reports. No sibling repository was edited.

Current prompt bodies are 531-767 whitespace-separated words, median 638.5. The [sizing report](PROMPT_SIZING_2026-10-24.md) and [measurements](PROMPT_SIZING.json) include the enrichment and retain the prior split checkpoint. Detailed tables stay in focused references; no prompt gains an unbounded new subsystem or a requirement to implement an unverified object.

Run `python scripts/check_promptset.py --check-git-ref INDEX` before commit and `--check-git-ref HEAD` afterward. The combined checker covers four source packs and 123 artifact hashes, all 50 assignments and their links, and exact EDS intake fields/addresses/widths/line spans/counts. `python scripts/check_promptset_test.py` exercises eight checks, including corrupted intake data and missing rows. [PLANNING_VALIDATION.json](PLANNING_VALIDATION.json) records the final integrity result. These are planning/provenance checks, not a CAN driver build or device qualification.

Whitespace verification accepts CRLF in byte-preserved references and excludes only the two unchanged upstream SRT `.cpp` files, which contain 12 original trailing-space lines. Their exact bytes are checked by the source hashes instead. The maintained files must pass; do not normalize downloaded source to silence a formatting check.
