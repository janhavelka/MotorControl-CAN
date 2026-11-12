# Prompt 64 Common axis interface for several drive brands

Apply [COMMON.md](COMMON.md). Prerequisites: [46](46_consumer_contrasts.md), [53](53_multi_axis.md).

## Objective and boundaries

The user's requirement, verbatim (2026-11-12): "lichuan functions - i wanted the lib to be capable of multi-brand
communication (we are building for lichuan, but enable others too) - is it prepared for that too?"; and "I want all of
the above additions/functionality to be still accessed through the lib api so another firmware can use it".

Today the design is prepared but the code is not: common units, coordinates, result words and capability facts are
shared, while every action (enable, release, move, velocity, home, stop, mode) is a Lichuan function and the
manufacturer and family lists have one entry each ([multi-drive decision](../multidrive_decision.md), limitations
report item 1.1). Deliver one common axis interface over profiles: an application calls the same typed operations for
any supported drive, the profile translates them to its native objects and words, and an unsupported capability is
refused before any work is admitted. Lichuan is the only production profile; the second real brand follows
[ADD_DRIVE.md](ADD_DRIVE.md) when it is selected.

Boundaries: static selection through a small explicit per-profile table, no heap, no runtime plugin system, no
placeholder production profile and no empty folders for candidates. Common names and meanings come from MotorControl-RS.
Native units, words and evidence stay in the profile; the common layer never assumes a node ID, an object dictionary,
NMT, a statusword, RPM, subdivision or encoder presence. A test-only fake profile and the N5/EPOS4 contrast fixtures
prove the interface; they are not support claims.

## Read and audit

For this prompt, refresh MotorControl-RS HEAD and dirty state and read its multi-profile common types, operation
vocabulary and consumers; RS is authoritative. Inspect FieldCore-node read-only for how a consumer selects and drives a
motor module. If HEAD, dirty state and seam match the last recorded row, write "unchanged since prompt N" and do not
reread. A sibling row must name a behavior adopted or rejected, with path and symbol. This does not relax vendor
evidence, safety, uncertainty or verification rules. Read the multi-drive decision, [MULTIDRIVE.md](MULTIDRIVE.md),
ADD_DRIVE.md, the prompt 46 report and contrast fixtures (`test/multidrive_contract_test.cpp`), `Discovery.h`,
`Position.h`, `Velocity.h`, `Actions.h` and the example `AxisSession`. Apply [CONVENTIONS.md](CONVENTIONS.md),
[DOCUMENTATION.md](DOCUMENTATION.md), [MAINTAINABILITY.md](MAINTAINABILITY.md) and [DELEGATION.md](DELEGATION.md).

## Implement

1. A decision record: which operations are common (enable, release, mode select, absolute and relative move, finite
   velocity, homing, stop kinds, typed reads), which stay profile-only, and the shape of the per-profile table
   (function pointers or a switch over a closed profile enum), with the reasons.
2. The common operation requests and results in exact units, each carrying the profile's native evidence; capability
   refusal before admission with a specific reason.
3. The Lichuan profile behind that table, with no behavior change; its existing typed functions stay available for
   native use.
4. A per-axis session API in the library (moved or trimmed from the example `AxisSession`) that drives any profile
   through the table and one owner, so firmware other than the examples gets the full feature set; the examples then
   consume it.
5. A test-only second profile that differs in units, words and capabilities, plus the N5/EPOS4 fixtures, run through
   the same session and owner tests.

## Verify and exit

Host tests: identical Lichuan traffic before and after the table, capability refusals without traffic, the fake
profile through every common operation, mixed profiles under one owner with independent stops. Run the native,
package, firmware, stack and documentation gates and CI on the actual final commit.

## Documentation and delegation

Update the architecture, multi-drive, profile contract and application guides, ADD_DRIVE.md, coverage, CHANGELOG and
Doxygen in the same stage. A stronger reviewer challenges what is common versus profile-only; normal coding agents own
disjoint test files. The lead integrates and runs combined checks. Record `docs/reports/prompt_64.md`. Commit and sync
on the current branch before handoff.
