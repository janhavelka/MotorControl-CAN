# Prompt42 motion console and platform parity

Scope: finite motion/action parsing and application integration, retaining the
existing installed operations and sole owner. Entry revision was `7bd63d15` on
`promptset/motorcontrol-can`, synchronized with its upstream; `main` was its
ancestor at `7878f7c`. The user explicitly requests merging this milestone into
`main` and no new branches until version0 is finished. No hardware, ports, flashing,
release tags or publication belongs to this delivery. Existing unrelated untracked
`.vscode` files are preserved.

## Evidence and prerequisites

[The source audit](../audit/PROMPT_42_SOURCES.md) records current RS/FieldCore
revisions, dirty state and actual symbols, and reopened original EN86/ZH86/EDS
facts. Native RPM destinations, ms ramp units, method numbers and zero-only2302
remain distinct. The public hidden2300 save during zeroing is a negative oracle,
not an adopted sequence. No parser chooses a dialect, proves a motor scale,
enables a drive before another action or upgrades fake qualification.
Prompts34-36's conditional operations and the existing W6 `AxisSession` stop path
are the consumed artifacts; their firmware/wiring/physical gates remain inherited.

## Delivered behavior

- One canonical inventory now dispatches simple/detailed absolute, relative and
  wrapped-angle moves, finite velocity, native home, enable/alarm-clear/position-clear
  and normal/fast stop. `velocity read` and `home read` use the existing ordinary
  typed driver read gate. Methods inspection is documentary only. Working grammar,
  effects, results, exact units and examples are in [the CLI contract](../cli_contract.md#finite-motion-and-actions-prompt-42).
- `MotionIntent` is reusable pure application intent. Explicit speed/ramp/scale
  facts replace boot defaults. Simple moves share RS's motor/load/native frame and
  nearest-even half-increment policy; `moveto` applies a supplied host origin.
  Detailed moves retain explicit frame/path/tie/rounding. INTEGER motor-turn/minute
  destination checks reject the in-range wrong267 value before any work; exact
  requests survive preview and final native quantization.
- `ProbeMotion` borrows one actual registered axis/session/live config/reference
  and actual typed prerequisite contexts. Native requests/stepping stay in existing
  `AxisSession` operations. The helper snapshots after the app turn, never receives
  CAN, advances another owner or creates grants. Separate frozen ordinary and
  normal/fast stop projections retain concrete native component evidence.
- Console storage reserves two ordinary correlations plus normal-stop, fast-stop
  and recovery correlations. Each stop owns one complete output slot independently
  of ordinary/control replies. Invalid/refused stop input cannot consume a stop
  reserve. Full output delays complete records and preserves owner progression.
- Normal-to-fast escalation, settlement before stop, one unqualified best-effort
 000B, held-never-sent recovery and UNKNOWN/no-replay reuse the native contract.
  Native stop-chain identity is stable across fallback/recovery and checked against
  hostile external replacement; it is local bookkeeping, never wire correlation.
  Stop, motion failure, cleanup and recovery results cannot overwrite each other.
- Checked idle host configuration and owned home/counter-clear epoch changes follow
  the same owner/session/selection. Historical reads remain stale; reference times
  are not renewed. Current controlword evidence is withdrawn on host edits without
  reopening a session or clearing an uncertainty fence.
- Shared source inventories, independent checker fixtures, exact transcript commands
  and relocated package consumers include all new application files. Installed
  C++11 core and optional SDK boundaries remain unchanged.

`motor-release` is caps-only gated by D-17. `motion stored` is caps-only gated:
native `REUSE_STORED` verifies and updates differences, while stored-only semantics
do not exist. Qualification install/revoke command shapes remain caps-only native
evidence obligations under35/36; an application supplies exact measured records,
never a console all-true switch. Broader Q20, reverse/start/stop, mode6/input/
reference, PDO, communication and persistence source gates remain with their owners.

## Independent review and regressions

Parallel agents independently checked primary sources, native expectations,
stop/uncertainty/correlation and actual SDK inputs. Confirmed defects found during
integration were fixed: malformed stops taking reserved output; cancellation of a
replaced native context; stop projection absorbing a different native chain;
host/native configuration follow; relative displacement versus endpoint formatting;
simple absolute-origin/motor-angle framing; and an unnecessary retained-operation
temporary exceeding the measured chain budget. Tests exercise these observable
failures rather than mirroring implementation helpers. A central registration
failure correctly caught the new third transcript producer argument; both the
checker and its independent failure fixture were updated, without relaxing the gate.

Native fakes cover every working route family, unsupported/invalid/stale cases,
exact numerics, quantization, actual payloads, lost replies, no replay/save/hidden
enable, cancellation settlement, normal/fast congestion, held recovery, late replies,
result lookup/release and hostile replacement. Python parses actual JSONL with
duplicate-key/nonfinite/line-bound rejection, exact string fractions and matching
asynchronous identities. Existing owner/TWAI/S2 platform fakes continue checking
the single receive path, clock wrap, queue pressure and recovery.

## Verification

The retained summaries below contain actual commands, tools, status and logs.
Local full verification passed; exact-commit remote CI is checked at synchronization; a passing
YAML/source-presence check is never represented as execution.

| Gate | Result / evidence |
| --- | --- |
| Native focused motion/read/settings/owner/JSONL | Passed integrated suites. Final motion console:22,933 checks including invalid-input counters; five affected suites passed again after the final diagnostic assertion. New intent and velocity/home suites use the real shared application/session/owner |
| Central full and strict documentation | Passed126/126 native/Python contracts, registration, fresh version/object output and strict Doxygen1.13.2. `.build/p42-full/20261111T161657484020Z-25644-xyxjwonh`, indexed by `.build/p42-full/summary.json`. Windows quick and Linux sanitizer gates also run remotely on the final commit |
| Package/export and isolated consumers | Passed separate full package gate: independent public/application headers, C++11, C++17/no-RTTI, relocated installed/exported/offline consumers and missing-dependency negatives. Same receipt, `16_package.log` |
| Arduino3.3.11 and ESP-IDF5.5.5 | Compile/link plus actual shared-input checks passed for Arduino S3, standalone IDF S3/S2/OPTAK, S2 bench and separate core component. Receipt `.build/p42-firmware/20261111T161657488021Z-54640-1c59w2v_`, indexed by `.build/p42-firmware/summary.json`; no hardware commands |
| S2/S3 `-Os` / `-Og` stack | Strict frame2048 / known-chain6144 gate passed without exemptions; `.build/p42-stack/report.json`: largest frame2016 bytes, largest known chain5888; sizes and incomplete edges retained |
| Repository/generated/planning/diff | Passed actual version/object freshness, test registration, package manifest, promptset working/INDEX integrity and diff checks; original140 artifacts unchanged |

The default SDK entries compile the shared inventory/dispatch and remain passive,
with CAN inactive until explicit local setup. They instantiate no qualified motion
attachment or device grant. A configured application attaches its actual borrowed
session/prerequisites through the same public bridge as a native caller. The shared
IDF S2 image uses UART0/core0; S3 USB/core1 does not substitute for it. Arduino S2
remains blocked by the pinned SDK console conflict. P4's broader adapter/image
work remains the existing backlog. Physical delivery, stopping, service-gap,
high-water, electrical and firmware qualification are unperformed here.

Storage bounds: input256/24 tokens, records512 including NUL, console input budget32
bytes/turn, two ordinary/two control/one-per-stop output slots and five correlations.
The owner retains its previous capacities/work budgets. Native batch stays
4096 input bytes/65536 captured output bytes/2000000 turns. New helper storage is
optional stationary application memory, not automatic core or mandatory SDK storage.
Measured S2/S3 ABI sizes: `ProbeConsole`6744, optional `ProbeMotion`3528, `SimpleMoveIntent`88 and `AxisSession`35752 bytes. The12,288-byte task stack is unchanged. Graph bounds are in the stack receipt; unknown/indirect/
SDK/libc edges remain lower bounds, never runtime stack guarantees.

## Handoff

Commit/push the checked complete milestone on the existing execution branch, verify
automatic required CI on that exact commit, then merge into `main` as explicitly
authorized and verify synchronization/final CI. GitHub main is unprotected with no rulesets (inspected read-only); a safe fast-forward preserves the exact tested SHA. The final handoff identifies actual
SHA/run status; no earlier green commit substitutes. Continue on `main` until version0
is finished, without new branches. Next executable software work is prompt43's
independent Python console/evidence client. Existing hardware/source obligations
remain visible in coverage/backlog rather than being called solved by this parser.
