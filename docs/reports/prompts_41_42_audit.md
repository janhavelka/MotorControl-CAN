# Independent audit of prompts 41 and 42, 2026-11-15

Audited entry: `635cb95e7816fb57073212bd584e944799b0f009` on `main`, tracking
`origin/main`, with a clean tracked tree and no incoming commits after fetch.
Both original prompts were read in full. The lead reviewed actual code/diffs;
three parallel reviewers independently examined device routes, motion/stop
ownership and requirement/source/coverage boundaries. No sibling repository,
vendor reference, generated catalogue, device permission or hardware was changed.
The [source audit](../audit/PROMPTS_41_42_REAUDIT_SOURCES.md) records refreshed
RS/FieldCore revisions, primary evidence and scope decisions.
The device and coverage reviewers used `gpt-6.1-sol` at high effort; the stronger
motion/stop reviewer used `gpt-6-astra` at high effort and later ran firmware and
stack gates. Only the lead changed production code, integrated checks and Git.

## Confirmed findings and corrections

| Finding at the entry revision | Correction and regression |
| --- | --- |
| A foreign `DeviceEvidence` collector could route `capture-read` into a different app/owner. | One graph check includes app, owner, service storage and optional collector. Both attachment paths and capture-plan admission reject mismatches. The regression tests foreign app/owner combinations and a successful same-graph nine-field capture. |
| Active discovery changed the public host selection to the last scanned target, including an initially unselected app. | `requestDiscoveryRead` passes its explicit target through the existing read preparation/reservation path. Progress uses the retained read target; selected-target getters retain their original checks. Tests preserve an absent or different selection every turn, through completion/cancellation/release, and reject non-discovery read kinds before work. |
| Discovery terminal/lookup records omitted cancellation, interface-change and deadline flags. | Freeze those independent summary flags alongside the existing collection outcome. A passive scan can normally exhaust its deadline; finding outcomes remain separate. Tests compare terminal and retained lookup for all three endings with no traffic. |
| Settings/PDO summaries replaced native failure causes/details with masks; communication/persistence records also omitted the original status detail. | Preserve additive `native_outcome_code` and `status_detail` fields, retaining existing summary/mask fields and full unsigned aborts. Tests cover timeout, cancellation, success and frozen lookup after native storage release. |
| Canonical result/release/cancel metadata described only the earlier read/recovery subset. Console driver/control success branches lacked direct route tests. | Correct inventory descriptions; test actual parser-to-typed-API payload units, readback/getters, retained correlation and whole-candidate refusal before traffic or operation consumption. |

The independent stronger motion review found no new defect in dimension checks,
exact coordinates, zero-without-save, no enabling, lost trigger/late reply,
stop settlement/recovery/escalation or retained motion/stop correlation. Existing
`probe_motion_actions` tests already cover qualified forward/reverse velocity and
homing; these were verified rather than reported as missing from another file.
Newer RS features outside the original assignments were not imported. Existing
Q20, model/firmware, DO, native velocity, homing and release gates remain explicit.

## Verification

The final fresh full run passes with GCC15.1, CMake4.0.1, Python3.12.10 and
Doxygen1.13.2:

```text
python scripts/verify.py --mode full --compiler gcc --build-dir .build/audit4142-full-final
142/142 registered CTest contracts passed; separate package gate passed;
strict documentation and public XML contracts passed.
```

This includes the real native console, JSONL/client/campaign regressions,
independent headers, source/export and relocated installed consumers,
C++11/C++17/no-RTTI, isolated native applications and negative dependency tests.
Focused integrated checks also pass, including actual settings/commissioning
success, cancellation and lost-reply vectors. Version/catalogue freshness,
repository/registration, planning integrity and diff checks pass. Full logs and
command/tool/status metadata are retained in
`.build/audit4142-full-final/20261114T231504506415Z-47280-9apggk2n/`.

The initial foreign-graph regression failed against the original code. One early
full run also mixed pre-correction objects with the final direct-attachment test;
an incremental rebuild confirmed stale objects, and the unchanged repository
contract separately exceeded its90-second limit during concurrent firmware work.
It passed in isolation (19 tests,12.214s). The final full run used frozen code and
passed all contracts in130.49s without changing tests, timeouts or gates. Failed
attempts remain under `.build/audit4142-full/`.

| Current-tree compile gate | Result and evidence |
| --- | --- |
| Arduino S3 | PASS,113 actual compiler entries, PlatformIO6.1.19/Arduino3.3.11 |
| Standalone IDF S3 | PASS,537 entries, ESP-IDF5.5.5, all43 original SDK hashes |
| Shared console IDF S2 | PASS,515 entries, same application/owner/console inventory |
| Isolated IDF component | PASS,501 entries, separate staging and real link |
| Strict target stack gate | PASS, S2/S3 at `-Os`/`-Og`, no flag drift or baseline allowances |

Firmware gates verified target/configuration, linked ELF/BIN and exact compiler
inputs. All31 shared inputs match and all107 production source/header hashes
remained unchanged. Summaries are below `.build/audit4142-firmware-*`, aggregated
in `.build/audit4142-firmware-results.json`. IDF commands use the existing
hash-exact `C:/pio/packages/framework-espidf` and isolated `C:/esp/v5.5.5` tools;
the earlier `C:/esp` SDK checkout was rejected for CRLF source bytes. It was not
modified and that rejection was not converted into a pass. The stack report is
`.build/audit4142-stack/report.json`: maximum frame2016/2048 bytes, maximum
measured chain6064/6144 bytes. Chains remain lower bounds, not runtime watermarks.

The handoff records the final checkpoint, synchronization and exact-commit
automatic CI result. Linux GCC/Clang ASan/UBSan are remote gates, unavailable
locally on Windows; an earlier green run is not used to verify the checkpoint.

No physical test or flashing occurred. Native/fake checks and firmware compilation
do not qualify motion, wiring, watchdog behavior or another firmware specimen.
The next planned implementation stage remains prompt45.
