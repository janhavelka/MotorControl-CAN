# Contributing

Read [AGENTS.md](AGENTS.md), the [current handoff](docs/prompts/PROGRESS.md) and
the assigned numbered prompt before editing. The working package currently
contains status, endpoint identity and Classical CAN frame validation. Synthetic
motion/profile tests do not establish implemented drive support.

## Build and check a change

Developer verification needs Python 3.10+, CMake 3.19+, Ninja and a native C/C++
compiler. Full verification also requires Doxygen **1.13.2**. The
[verification guide](docs/verification.md) gives setup, compiler/sanitizer options,
commands, output paths and the maintained current/planned check table.

```text
python scripts/verify.py --mode quick
python scripts/verify.py --mode full
```

Use a new ignored build directory for a clean build, for example
`--build-dir ".build/review clean"`. This works from PowerShell and Linux shells;
quote paths with spaces. Run the affected focused tests while editing, then the
appropriate integrated mode. The full mode exercises independent package
consumers and strict documentation. Required missing tools produce a nonzero
unavailable result. A previous passing revision does not verify changed source.

Ordinary source and installed consumers need only the C++ toolchain; follow the
[native package guide](docs/getting_started.md). Keep their dependency boundary
independent of Python, research documents, SDKs and CANopenNode. New public
headers must compile independently and participate in both source and isolated
installed consumers. Keep strict flags private to verification targets.

## Source, tests and generated files

Use small functions, explicit states and caller-owned bounded storage in the
portable C++11 core. The [maintainability contract](docs/prompts/MAINTAINABILITY.md)
and [conventions](docs/prompts/CONVENTIONS.md) define names, ownership, units and
failure publication. `.clang-format` uses an LLVM-derived four-space style,
attached braces and a 100-column target consistent with the current core and RS
source. The I2C siblings use a different two-space style. Apply formatting to new
code or changed regions; do not reformat immutable evidence or unrelated source.
Run `clang-format --dry-run --Werror` on changed handwritten C++ files when that
tool is available; formatting is a contributor aid, not a required current gate.

Add native contracts as `test/*_test.cpp` and host contracts as
`test/*_test.py`. Register every test explicitly in `test/CMakeLists.txt`,
including its mode label and bounded timeout. Retained checker suites in
`scripts/*_test.py` are registered there too. The repository checker detects
unregistered test files; file discovery alone does not run a test. Test
observable success, failure, unchanged outputs and relevant boundary behavior.
Keep N5/EPOS4 and synthetic operations in test support.

`library.json` is the version authority. After an intentional version change run
`python scripts/generate_version.py sync`, review and commit `Version.h`, then
run `python scripts/generate_version.py check`. Fix documentation of generated
declarations in the generator rather than editing generated output. Catalogue
generation is a later stage with its own reviewed ledger and check mode.

Document public declarations in their headers, including parameters, validity,
units, ownership, failure output and evidence limits where applicable. Extend
the maintained guide and CHANGELOG with public behavior changes. Run
`doxygen Doxyfile`; missing parameter comments, undocumented public members and
unresolved references fail the gate. New installed headers must be included in
the package allowlist/install rules, independent-header consumers and API
documentation inventory. Avoid copied examples that drift from compiled usage.

## Changes, evidence and handoff

Keep related implementation, tests, generated output and documentation together.
Preserve unrelated files/hunks. The promptset uses one execution branch across
numbered prompts; the lead owns Git integration. Follow the root instructions
for explicit staging, checked commits, normal pushes and final upstream checks.

`.build/`, `.install/`, `build/` and `dist/` are disposable development output.
The verifier retains per-command logs and a machine-readable summary in its
build directory; preserve relevant failures before cleanup. Original vendor
files, manifests, copied snapshots and retained audit/bench evidence remain in
the repository and keep their separate provenance/licenses. Never classify
those files as build cache. Check changed protected artifacts with
`python scripts/check_promptset.py --check-git-ref INDEX` before committing and
with `--check-git-ref HEAD` afterward.

Record exact revisions, checks, unavailable tools and evidence gaps in the stage
report and current handoff. Run required CI on the final commit for a build,
workflow or integration milestone when the service is available. Documentation
wording changes need local checks; they do not require a manual CI dispatch per
push. Host verification does not authorize hardware use or qualify a motor.
