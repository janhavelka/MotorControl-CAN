# Prompt49: user documentation and package audit

Owned software scope: Complete. Physical/inherited gates unchanged.

Date: 2026-11-15. Starting revision: `4ce6656398fcbfd4e1840a4824a5ff2c4630c59a`,
clean and synchronized on `main` / `origin/main` after fetching. Work stays on
main under the user's version0 instruction. No hardware, sibling mutation,
release tag or package publication is part of this audit.

## Scope and findings

The [original assignment](../prompts/49_documentation_package_audit.md) was read
in full with the shared engineering contracts. Independent workers audited
source/package boundaries and support claims, followed installed examples, and
reviewed the final combined changes. The lead checked findings against actual
source/diffs and kept the physical and firmware gates separate.

| Requirement | Result and authoritative paths |
| --- | --- |
| README-only native workflow | Root README now leads with scope, actual install/include/link and a complete exact-preparation consumer. The package harness compiles its actual exported C++/CMake blocks against a relocated install. The installed `position_preview` demonstrates read/move/stop with supplied events and explicit rejection. |
| General file audit | README and docs index reordered for users; CONTRIBUTING, SECURITY, LICENSE, metadata and Doxyfile scope corrected. Formatting policy and root AGENTS retained: no conflicting new workflow. `library.json` remains version authority. |
| Contracts and A-17 | Removed `ownerPrompt` from the generator, public type and all 194 descriptor initializers. Ledger ownership remains. Changed read/network/velocity comments and object tests; no wire value/access/gate changed. Candidate notes and CHANGELOG explain aggregate/layout migration and rebuilding consumers. |
| Coverage | Mechanical checker reconciles 202 EDS entries, 53 manual obligations and 191 public overloads. It retains separate descriptor/read/settings/action/workflow scope, CLI or API-only rationale, tests and pending facts; no raw accessor closes an unfinished operation. |
| Packaging and commands | Actual source ZIP and PlatformIO tar have the same 83 paths/bytes in both the initial and final audit. Core source/install/header/no-catalogue consumers pass. Broken package-relative links now target verified repository files through explicit URLs; shipped-file links stay local and are checked in the ZIP. |
| Licensing/dependency disposition | [Candidate notes](../releases/0.1.0.md) retain the single hash-pinned CANopenNode copy at its current path with Apache-2.0 notices. Optional stack/owner/SDK, GPL reference example, vendor originals/extracts, tools/tests and HIL logs are excluded from the portable core. No reference bytes or rights were changed. The recorded maintainer email is not presented as a verified dedicated security service. |

The literal README walkthrough found `CMAKE_PREFIX_PATH` resolves relative to
the consumer source directory, not its build directory. The wrong consumer path
failed `find_package`; the corrected `../.install/core` succeeds. The nested
position example correctly uses `../../.install/core`. This failure and corrected
run are retained under `.build/p49-readme-follow` rather than hidden by an
absolute-path package test.

Maintained prose no longer says the CLI is absent, physical tests never ran,
angle integration is future work, or configurable channels are permanently two.
Speed guidance distinguishes reported-count fits from physical scale/direction;
an experimental cap is not the admitted measured speed range. Backlog alarm-clear
claims now include the retained November13 mode-3 FF05 subset without qualifying
other faults/modes/firmware. Candidate claims remain conditional.

## Source and evidence review

[Source audit](prompt_49_sources.json) records exact revisions, before/after
dirty state, symbols and working/committed hashes for the requested seams:
RS source/install/export/version/docs, ADS1115/INA228 checker/Doxygen/release,
and prospective FieldCore boundaries. RS acquired an unrelated dirty report
during the audit; it was not inspected or attributed to HEAD. No sibling was
changed or fetched. FieldCore has generic motor health consumers but no CAN
backend; the corrected handoff remains prospective.

The stronger reviewer reopened original EN86 PDF pages11/13/16/17/18 (including
the page11 figure), original EDS sections, the reviewed ledger and retained
units/bench evidence before correcting speed claims. The device evidence map's
status dialect, DO layout, activation, scale and velocity contradictions remain
visible in [device conflicts](../device_conflicts.md) and the ledger. No new
vendor sequence, physical measurement or firmware gate was invented.

## Verification

Focused checks passed before integration:

- Fresh GCC15.1 C++11 object/read/preview build: four CTest gates; 47 generator
  contract cases and generator freshness. Search finds no `ownerPrompt` consumers
  in installed/source/adapter/example/test/script code.
- Package harness: 29 commands, including actual README extraction/build/run,
  exported local links, C++11/C++17, independent public headers, core-only and
  no-catalogue consumers, missing-header negative case and 11 isolated native
  applications. Initial PlatformIO6.1.19 pack matched the exporter byte-for-byte.
- Native README walkthrough: core install, exact example (`1536` command
  increments), and installed read/move/stop preview. Synthetic HALT emits one
  `010F`; missing predecessor is rejected without publication or writes.
- Pinned Arduino3.3.11 S3 and isolated ESP-IDF5.5.5 component builds produce
  ELF/BIN. These are compile checks, not firmware flash or physical qualification.
- Python identity plan, self-test and scenario listing run without a serial port.
  Live syntax was checked against the existing parser/tests; no live invocation
  occurred in this prompt.

Combined verification passed:

```text
python scripts/verify.py --mode full --compiler gcc --build-dir .build/p49-final
```

All 147 selected CTest contracts passed (300.55 s), followed by the 29-command
package audit and strict Doxygen1.13.2. XML inventory includes all37 installed
headers, 150 public free-function names and3859 members; extraction is not empty.
Tools: GCC15.1.0, CMake4.0.1, Ninja1.12.1 and Python3.12.10 on Windows. Local
ASan/UBSan is unavailable on this host; the required Linux CI jobs cover it.
Generated/version/operation, repository, maintained-link and reference-byte
checks pass. No release tags or GitHub releases existed at the audit.

[Verification record](prompt_49_verification.json) retains actual local summaries,
command arguments, package comparison and firmware results. These are working-tree
checks based on the starting revision, not a claim that the old HEAD contains the
changes. Final staged-byte/whitespace checks and synchronization are performed
before commit/push; exact-commit remote CI is confirmed separately at handoff.
The push-triggered workflow includes Windows quick, Linux GCC/Clang sanitizers,
Arduino S3 and standalone IDF S3/S2/board/bench plus isolated component/stack checks.
No additional manual dispatch or release publication is needed.

Working evidence is retained under `.build/p49-package-review`,
`.build/p49-a17`, `.build/p49-native-example` and `.build/p49-readme-follow`.

## Maintainability and remaining scope

Reused the generator, package harness and existing native example; no new
runtime framework, service engine or installed capability. The only public
layout change removes development metadata. Package docs remain checked-in
files; there is no build-time Markdown transformation. Core version is unchanged
at unreleased0.1.0, with the compatibility change explicit.

Prompt18/Q17 and the feature-specific firmware gates remain inherited. Prompts47
and48 remain Partial for physical qualification, related models and unlocalized
serial loss. Earlier missing standalone read rows and unsupported write/action
semantics remain with their existing owners. The [candidate notes](../releases/0.1.0.md)
and [backlog](../backlog.md) retain exact limits. Prompt50 is the next independent
software-candidate review; this audit does not implement it or later feature work.
