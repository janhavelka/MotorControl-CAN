# Prompt 65 Optional library package for the bus code and the axis API

Apply [COMMON.md](COMMON.md). Prerequisites: [49](49_documentation_package_audit.md), [64](64_common_axis_interface.md).

## Objective and boundaries

The user's requirement, verbatim (2026-11-12): "I want all of the above additions/functionality to be still accessed
through the lib api so another firmware can use it - the example serves as a working example of all the features and
for testing purposes."

Today the installed package exports only `include/` and `src/` (`library.json`): units, codecs and profile
preparation. The bus owner, the CANopenNode SDO adapter, the TWAI adapters, the capacity header and the per-axis
session are compiled from the repository by the examples, so another firmware cannot install them (limitations report
item 1.2). Deliver them as optional, versioned library components that an application installs and uses without the
examples, while the portable core package stays as it is.

Boundaries: the portable core stays C++11, heap-free and free of SDK, stack and platform includes; the optional
components carry their own dependencies (CANopenNode, ESP-IDF TWAI) and license notices (CANopenNode is Apache-2.0);
vendor manuals and EDS files stay out of every distributable package; one capacity set per program (Capacity.h); the
examples must use only the published API, so nothing works only inside an example.

## Read and audit

For this prompt, refresh MotorControl-RS HEAD and dirty state and read its packaging and component layout; RS is
authoritative for ecosystem conventions. Inspect FieldCore-node read-only for how it consumes libraries. If HEAD,
dirty state and seam match the last recorded row, write "unchanged since prompt N" and do not reread. A sibling row
must name a behavior adopted or rejected, with path and symbol. This does not relax vendor evidence, safety,
uncertainty or verification rules. Read `library.json`, the CMake install/export rules, `test/package_test.py`, the
installed consumer, the IDF component consumer, the prompt 49 report and the licence files. Apply
[CONVENTIONS.md](CONVENTIONS.md), [MULTIDRIVE.md](MULTIDRIVE.md), [DOCUMENTATION.md](DOCUMENTATION.md),
[MAINTAINABILITY.md](MAINTAINABILITY.md) and [DELEGATION.md](DELEGATION.md).

## Implement

1. A component layout decision: core (unchanged), CANopen services and owner, TWAI adapter per platform, and the axis
   session API from prompt 64, with the public headers of each and what stays example-only (board configuration, CLI,
   bench fixture).
2. CMake install components and exported targets, a PlatformIO library manifest per component or one manifest with
   documented optional parts, and an ESP-IDF component, each with the capacity options.
3. Isolated consumers for each component built from the installed or packed artifacts only, with no repository
   checkout, examples or reference tree.
4. Examples switched to the published headers and targets; any helper an example needs becomes API or stays a
   documented example detail.
5. Strict Doxygen for every published header, API stability notes, and the licence and notice files per package.

## Verify and exit

Run the package tests for every component, the isolated consumers on the host and in the IDF and PlatformIO builds,
the native, firmware, stack and documentation gates, and CI on the actual final commit. The package contents must
exclude vendor material.

## Documentation and delegation

Update the README, installation and integration guides, CHANGELOG and Doxygen in the same stage. Normal coding agents
may own disjoint package manifests and consumers; a stronger reviewer checks the public surface and licences. The lead
integrates and runs combined checks. Record `docs/reports/prompt_65.md`. Commit and sync on the current branch before
handoff.
