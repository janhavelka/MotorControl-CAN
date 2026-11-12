# Prompt 58 Measured records through the library API and the console

Apply [COMMON.md](COMMON.md). Prerequisites: [42](42_cli_motion_routes.md), [57](57_regular_drive_operations.md).

## Objective and boundaries

The user's requirements, verbatim (2026-11-12): "The console can't load the measured records motion needs - add this
functionality (prompt/s)"; "I want all of the above additions/functionality to be still accessed through the lib api
so another firmware can use it". Also the agreed suggestion: "Turn the existing bench data into measured records".

Today motion needs a reviewed record (position completion, velocity, homing, enable) that only firmware code supplies
through `ProbeMotion::set...Prerequisites`; the console refuses by design ([motion capability
report](../reports/2026-11-12_motion_capability_evidence.md) section g). The bench `Y`/`B` arming lines already load
records from text. Deliver a library record format and codec, validation, loading and optional storage through the
API, console routes that call that API, and the first reviewed records built from the retained bench evidence, so the
60 rpm / 20 s experiment limits are replaced by measured ones where the evidence supports it.

Boundaries: a record states its evidence (measured, documented, inferred), the product and firmware identities it was
measured on (prompt 57 policy) and its ranges; loading validates the whole record before it takes effect and never
widens a range beyond what the record states; records stay in RAM unless saved explicitly (NVS, Arduino Preferences or
a native file in the examples), with no automatic restore at boot; a loaded record never enables or moves anything by
itself.

## Read and audit

For this prompt, refresh MotorControl-RS HEAD and dirty state and read its prerequisite and record seams; RS is
authoritative for common names. Inspect FieldCore-node read-only for configuration storage ownership. If HEAD, dirty
state and seam match the last recorded row, write "unchanged since prompt N" and do not reread. A sibling row must
name a behavior adopted or rejected, with path and symbol. This does not relax vendor evidence, safety, uncertainty or
verification rules. Read `ProbeMotion.h`, `ProbeConsole.cpp`, `BenchRoute.cpp` and `BenchConsole.cpp` arming lines, the
position, velocity, homing and enable record types, the bench reports and the night soak. Apply
[CONVENTIONS.md](CONVENTIONS.md), [MULTIDRIVE.md](MULTIDRIVE.md), [DOCUMENTATION.md](DOCUMENTATION.md),
[MAINTAINABILITY.md](MAINTAINABILITY.md) and [DELEGATION.md](DELEGATION.md).

## Implement

1. A pure record codec in the profile: one-line key=value text with a version and a CRC, encode and decode, whole-record
   validation with specific refusal reasons; no heap.
2. API: load, show, use (bind to an axis) and drop a record; example storage hooks for save and restore on ESP-IDF,
   Arduino and native.
3. Console routes `record load|show|use|save|restore|drop` that call the API only.
4. Reviewed records from the retained evidence: enable (prompt 57), position completion, velocity (rounds 7 to 12 and
   the night soak, with their measured speed and duration ranges), each with its evidence directory and review notes;
   unmeasured ranges stay under the attended experiment.

## Verify and exit

Host tests: codec round trip, CRC and version refusal, partial and out-of-range records refused, a record never
widening limits, no automatic restore, the console using only the API. Run the native, firmware, stack and
documentation gates and CI on the actual final commit. Bench: load each reviewed record from the console and run one
short operation per record type.

## Documentation and delegation

Update the position, velocity, homing and drive actions guides, the CLI contract, coverage, CHANGELOG and Doxygen in
the same stage. A stronger reviewer checks every reviewed record against its raw evidence. The lead integrates and
runs combined checks. Record `docs/reports/prompt_58.md`. Commit and sync on the current branch before handoff.
