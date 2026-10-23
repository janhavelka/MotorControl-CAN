# MotorControl-RS quick verification evidence

These are byte-preserved logs from the read-only source audit on 2026-10-23.
They verify **MotorControl-RS**, not the proposed MotorControl-CAN library.

| Item | Result |
| --- | --- |
| Source HEAD | `5da4ba7f13be457ea7bf9706ffd7d546821669c7` |
| Source working tree | Clean before and after verification and evidence copying |
| Workflow | Source repository `scripts/verify.py --mode quick` |
| Aggregate | **PASS**, 15 workflow steps with exit code zero |
| Repository CTest suite | **78/78 native/Python checks passed** |
| Clean source-package consumer | **3/3 checks passed** |
| Clean installed-package consumer | **3/3 checks passed** |
| Other workflow steps | Offline reference checks, builds/install and codec-only symbol checks passed |
| Hardware, firmware builds, remote CI | Not run in this audit |
| Host tools | Windows, Python 3.12.10, MinGW GCC 15.1.0, CMake/CTest 4.0.1, Ninja |

The three consumer checks run in each consumption mode; do not describe them
as six additional unique behavioral test suites. Likewise, 15 workflow steps
is an orchestration count, not the number of tests. Historical release documents
refer to their own 77-test snapshot; this branch revision registered 78.

Original command, run from the source repository:

```powershell
$env:PYTHONDONTWRITEBYTECODE='1'
python scripts/verify.py --mode quick --build-dir C:/Users/Honza/AppData/Local/Temp/motorcontrol-rs-audit-20261023
```

[summary.json](summary.json) contains the verifier's result, per-step timing,
exact logs and locally generated package hash. The 15 numbered `.log` files are
complete original logs. [MANIFEST.json](MANIFEST.json) records source revision,
command, tool versions and per-file byte lengths/SHA-256 values. Its `files`
entries use directory-relative `source` and `path`, with `bytes` and `sha256`.
Validate all 16 evidence files against that manifest without changing them.

Absolute paths inside logs are retained as original execution evidence. They
are not dependencies of MotorControl-CAN or assertions that temporary build
directories will remain available. No binaries, ZIP package, temporary copied
source, build tree or firmware image is included here. The ZIP hash in the
summary identifies the audit's generated source package; it is not the original
v1.0.0 release hash and is not a hardware qualification claim.

The [source audit](../../MOTORCONTROL_RS_AUDIT.md) explains the checks and their
limits. The [selective source baseline](../../../baseline/README.md) records
the same HEAD and preserves API/architecture reference files.
