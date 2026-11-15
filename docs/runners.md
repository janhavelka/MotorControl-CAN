# Python console client and finite campaigns

`scripts/bench_probe.py` provides one bounded console session and one explicit
probe or typed read. It uses [the shared console protocol](cli_contract.md),
through the application's existing owner and read routes. Python does not own
a CAN controller or implement a second CANopen driver. `bench_motion`,
`bench_repeat` and `bench_scenarios` add finite public-command campaigns through
the shared `bench_session` owner. The private `_bench_evidence` module interprets
supplied records through helpers exported by `bench_probe`.

## Setup and offline use

Python 3.10 or newer is sufficient for imports, help, plans, self-test and pure
evidence summaries. Only live serial use needs the optional pinned dependency:

```text
python -m pip install pyserial==3.5
```

Run from the repository root:

```text
python scripts/bench_probe.py --help
python scripts/bench_probe.py --plan-only
python scripts/bench_probe.py --plan-only --scenario identity
python scripts/bench_probe.py --plan-only --scenario state --state-block feedback
python scripts/bench_probe.py --self-test
```

`--plan-only` prints JSON without importing serial, opening a port or creating
files. Omitted fixture facts remain null. Its default `--scenario quick` plans
one probe; it is not a sweep or repeated campaign. `--self-test` exercises the
client with a scripted host stream and prints `host-tested` evidence. Neither
mode is a live result. `bench_probe` has no `--list`, `--count` or automatic
recovery mode.

## One live invocation

Supply every fixture fact explicitly. In the following syntax, replace the
uppercase tokens with established values; none is a working default:

```text
python scripts/bench_probe.py --port SERIAL_PORT --baud CONSOLE_BAUD --profile lichuan_c --node NODE --can-bitrate CAN_BITS_PER_SECOND --expected-library-version MAJOR.MINOR.PATCH --log NEW_LOG.jsonl --scenario probe
```

`--port` identifies the serial console. `--baud` is its serial rate, 1..4000000;
`--can-bitrate` is the expected cached CAN interface rate, 1..1000000 bits/s.
They configure and check different interfaces. `--node` is an existing CANopen
host binding, 1..127. The client selects it with `useaddr`, then checks the
cached selected node and bitrate. It does not write a drive node or bitrate,
discover routes or construct a missing binding. `--profile lichuan_c` and an
exact `--expected-library-version MAJOR.MINOR.PATCH` are required. These are
library/profile expectations, not a drive model or firmware qualification.

All arguments are checked before opening a port. `--log` and
`NEW_LOG.jsonl.summary.json` must both be new files in an existing directory.
The client validates `version`, `profile list`, `caps` and `config` before
selection or the requested read. It requires the CAN schema, product, exact
library version, profile, CANopen protocol and implemented read capabilities.
Unknown model/firmware matches and `hardware_qualified:false` remain explicit.

| Option | Meaning |
| --- | --- |
| `--scenario quick` or `probe` | One `probe TIMEOUT_MS`, uploading `1000:00`. DEC-1 accepts its unknown read effect; read-only intent does not establish harmlessness. |
| `--scenario identity` | One nine-field `read identity TIMEOUT_MS`; raw identity does not prove model/firmware support or exclude collisions. |
| `--scenario state --state-block status\|monitors\|feedback` | One selected state block. Default block is `status`; original field bounds and currentness remain visible. |
| `--scenario config` | One `read config TIMEOUT_MS`. The ordinary route is Q20-gated and returns refusal before admission/traffic; successful configuration evidence is not implemented by this client. |
| `--request-timeout-ms` | Application read budget 1..60000 ms, default 1000; this is host policy, not vendor timing. |
| `--timeout` | Immutable host deadline in seconds, finite `(0,60]`, default 5; handshake shares one overall deadline. A host deadline is not confirmed motor stop. |
| `--startup` | Bounded initial banner collection in seconds, finite `(0,60]`, default 0.2. |
| `--release` | Send a separate correlated release after a checked admitted terminal. Omitted by default, so firmware retains the result. |
| `--trace-raw` | Explicitly enable public `debug raw` copies after handshake/selection and before the read; retain traces through one `debug off` on a usable stream. |

A requested release refusal makes the invocation fail; it does not erase or
reinterpret the primary read terminal. Ctrl-C retains the known terminal or
interrupted/unknown evidence in the summary and exits nonzero. Both port and log
closure are attempted before a passing summary is published. Closure failures
are recorded in `close_errors` without replacing an earlier primary error.
Summary write/flush/close failures return execution status 1 and report the
primary classification/reason plus the separate artifact error on bounded stderr.
The raw transcript remains available; completion prints only after summary closure.
The client does not reopen or retry the failed summary medium.

Connection, inspection and failure cleanup never imply enable, motion,
reconfiguration, fault clearing or replay. Closing the port sends no cleanup
command. Requested raw tracing is disabled before closure while the stream remains usable.
Hardware qualification and previously retained operations remain
application/fixture concerns.

## Reusing Console

Import `bench_probe` with the repository's `scripts` directory on Python's module
search path. `Console(port, on_event=..., clock=..., sleeper=...)` borrows one
nonblocking stream: `read(size)` returns at most `size` bytes, and `write(data)`
is one bounded handoff. The caller owns stream lifetime, scheduling, clock and
evidence callback. Callbacks must remain bounded and must not reenter Console.

Call `drain_startup(seconds)` and
`identify(expected_version="MAJOR.MINOR.PATCH", profile="lichuan_c")` first.
`begin(command, timeout_s=...)` returns a caller-retained `Command`; it confirms
neither admission nor device success. `pump()` dispatches available records,
and `wait(handle)` waits under the original cutoff. `command(...)` combines
begin/wait. `release(handle)` explicitly releases a checked admitted terminal.
Its returned reply reports refusal. The `wait(..., release=True)` and
`command(..., release=True)` conveniences raise `BenchError` on that refusal,
leaving the original terminal and unreleased handle available.
Until an empty input boundary completes trailing-byte drainage, a known terminal
still has its original finite deadline/work limits. Afterwards its native handle
is retained: later commands cannot consume that result's byte/turn budget, and
another `wait(handle)` returns the cached terminal without servicing unrelated
traffic. Global session limits and explicit release ownership remain unchanged.
The reusable API supports the finite public forms listed by `supported_command`,
including the campaign forms described in [the scenario catalogue](bench_scenarios.md).
It does not accept arbitrary firmware commands.

Request IDs, admitted operation IDs, original command IDs, record kind, native
terminal and deadline are checked independently. Exact decimal strings remain
strings and Python integers remain exact. Trace records are observed without
consuming another operation's evidence. Duplicate/stale records, malformed JSON,
wrong identity, reboot diagnostics, disconnect or framing loss poison the
session permanently. There is no reconnect, retry or automatic result polling.

Ctrl-C during startup, service, write handoff or evidence publication also fences
the stream, including consumed-but-unparsed bytes and returned-but-unvalidated
write counts.
The original interrupt and any checked terminal survive; scenario cleanup cannot
transmit on that damaged stream. Fixture JSON exceeding parser or contract nesting
limits is an input error (exit2), before artifacts or a serial port are opened.

Schema 1 supplies no firmware boot/session ID or firmware source commit.
`firmware_epoch` is therefore null. Each Console has a separate local
`host_epoch`, which rejects foreign handles but proves no firmware reset or
queue retirement. A fresh Python object or randomized request ID cannot make a
late old device response new evidence. After interruption, retain the known
terminal or unknown outcome and establish the actual fixture/session boundary
before any separate invocation.

## Evidence, limits and exit codes

Live JSONL records include intent, raw received bytes as hex, parsed events and
identities, with supplied host elapsed time on Console events. The initial
invocation records explicit configuration and intended read even if handshake
fails. The sibling summary JSON preserves checked records, native
status/detail/outcome/abort, acceptance, uncertainty and host provenance. The
console prints a short result and artifact paths. It captures no environment
or unrelated host files. Keep application-provided command/fixture metadata
appropriate for the retained artifacts.

`_bench_evidence.summarize_operation` and `summarize_execution` are pure supplied
record projections. They own bounded copies and perform no I/O, clock access,
correlation or device-state inference. Evidence kinds are `planned`, `skipped`,
`host-tested` and `live-observed`. Classifications distinguish local refusal,
no admission, native rejection, pending/terminal evidence, unknown handoff and
unknown after admission. A checked pending-result lookup preserves its admitted
operation ID without claiming a terminal outcome or known native uncertainty.
A native failure stays distinct from stream damage;
a later parsing failure cannot erase an already checked terminal. Native
`uncertain` remains verbatim or null when absent. A serial write attempt proves
neither CAN TX nor protocol acknowledgement.

| Resource | Bound |
| --- | --- |
| Console wire record | 511 bytes including newline; JSON nesting at most 16 levels |
| Client storage | Four ordinary/cached pending requests plus one reserved normal-stop and one fast-stop request; two ordinary admitted operations plus one per stop kind; 128 records per request |
| Input/work | 65536 received bytes per command, 1 MiB per session, 1024 commands; startup, preflight and all requests have a deadline-derived allowance of at least 8192 and at most 65536 turns |
| In-memory diagnostics | Latest 64 raw lines and 16 parse failures; dropped raw-line count retained |
| Live JSONL | 2048 events and 2 MiB including a reserved limit notice; 16384 bytes per event |
| Pure summary | 256 records, 262144 serialized ASCII JSON bytes, 16 container levels; execution summary at most 16 operations |

Evidence write/flush failure and capacity exhaustion permanently fence the log,
even if its sink later recovers. `events` and `bytes` count completely written
and flushed records, including the limit notice; partial failed writes are not
counted as confirmed records. Session evidence exposes independent `failed` and
`exhausted` flags. Failure through the Console callback forbids further commands.
These bounds are client/storage limits, not
universal drive or CANopen capacities.

Before each handoff, already-buffered input is routed or rejected under finite
byte/time/turn bounds. Split records for existing work keep its original deadline.
The candidate deadline is checked again after intent evidence and before serial
handoff; an already expired request sends nothing. More idle turns do not renew
the immutable timeout, and frozen clocks still terminate at the finite work limit.
An empty host read cannot prove that device queues are empty or resolve the
missing firmware epoch.

Exit 0 means a valid offline plan/self-test or a successful nonuncertain live
read. Exit 1 means execution, native, identity, dependency or stream failure;
exit 2 means invalid arguments or setup/artifact creation failure. A failed
invocation never becomes a passing campaign. Offline success and
`live-observed` parsing never establish physical completion or qualification.

The [prompt-43 source audit](audit/PROMPT_43_SOURCES.md) records which RS and
sensor client behaviors were adopted or rejected. The
[campaign audit](audit/PROMPT_44_SOURCES.md) checks the finite runners and cleanup
differences. [The source map](source_map.md) locates implementations and tests.

## Finite campaign tools

The six-file family shares one parser and evidence path:

| File | Usable role |
| --- | --- |
| `bench_probe.py` | Reusable `Console`, checked commands, fixture validation and one probe/read invocation |
| `bench_motion.py` | One explicitly selected finite action/motion/settings scenario; read-only `quick` default |
| `bench_repeat.py` | Position-only `write`/`verify` comparison, 1..20 pairs, default one pair, one planned final stop |
| `bench_scenarios.py` | Named plan construction and execution; only runner with `--list`; read-only `quick` default |
| `bench_session.py` | Owns the port, `Console`, session identity and evidence artifacts; reusable functions and the same named-scenario CLI |
| `_bench_evidence.py` | Private pure evidence interpretation behind `bench_probe`; no port or command execution |

Run these plans without serial, fixture files or artifact creation:

```text
python scripts/bench_scenarios.py --list
python scripts/bench_scenarios.py --plan-only
python scripts/bench_motion.py --plan-only --scenario position --kind relative --target 100 --rpm 60 --ramp-up-ms 100 --ramp-down-ms 200 --setup verify --stop-policy fast
python scripts/bench_repeat.py --plan-only --count 3 --target 100 --rpm 60 --ramp-up-ms 100 --ramp-down-ms 200 --stop-policy fast
```

The campaign and session CLIs accept `--plan-only`. It returns a distinct `planned`
verdict and leaves absent fixture facts unobserved. Motion/settings plans still
require their explicit campaign parameters. The
[scenario catalogue](bench_scenarios.md) specifies each parameter, command,
capability and finite deadline. Repeat plans exactly `2*count` ordinary relative
moves and reports their aggregate requested displacement. It accepts no arbitrary
scenario, persistence action or restoration. `verify` may write differences;
there is no available stored-only policy.

Live campaign invocations use `--out NEW_PREFIX`, while the single-invocation
`bench_probe` interface above retains `--log NEW_LOG.jsonl`. For example:

```text
python scripts/bench_scenarios.py --scenario quick --port SERIAL_PORT --baud CONSOLE_BAUD --profile lichuan_c --node NODE --can-bitrate CAN_BITS_PER_SECOND --expected-library-version MAJOR.MINOR.PATCH --out NEW_PREFIX
```

Each live campaign requires explicit port, console baud, profile, node, CAN
bitrate, expected library version and output prefix. Device campaigns additionally
require `--fixture PATH`. Nothing opens a connection on import. The session
identifies and selects the existing host route, then its first active upload is
the DEC-1 accepted `probe`. `quick` follows it with one identity read. It does not
use a Q20-gated configuration read as a startup baseline. Missing native
prerequisites remain a named refusal; no runner enables a drive or writes
settings to make admission succeed.

| Shared campaign/session option | Meaning and bound |
| --- | --- |
| `--port`, `--baud`, `--profile`, `--node`, `--can-bitrate`, `--expected-library-version` | Same explicit interface/version meanings and numeric ranges as the one-invocation client; all required live |
| `--out` | Required live artifact prefix in an existing directory; no `--log` alias |
| `--fixture` | Supplied strict JSON prerequisite record, at most 16384 bytes and 16 container levels |
| `--firmware` | Optional supplied image file, at most 64 MiB; retain path, byte count and SHA256 only |
| `--timeout` | Per-command immutable host deadline, finite `(0,60]` seconds, default 15; velocity requires more than its duration plus the application's 10-second cutoff |
| `--startup` | Bounded initial banner collection, finite `(0,60]` seconds, default 0.2 |
| `--request-timeout-ms` | Explicit probe/identity application budget, 1..60000 ms, default 1000 |
| `--trace-raw` | Enable owner diagnostic copies after cached setup and before the first DEC-1 probe; disable after scenario cleanup and final snapshots on a usable stream |

`--timeout` is a command deadline, not an overall campaign duration. A campaign
has a finite command count and each command keeps its own cutoff. These are
host policies and existing application budgets, never invented device timing.
There is no campaign `--release` flag: successful admitted terminals are copied
into evidence and explicitly released before proceeding. Failed or uncertain
results remain retained; release is local result ownership, not a motor action.

`--trace-raw` uses the same Console, owner and bounded transcript. It sends only
the public local diagnostic commands `debug raw` and `debug off`; no second CAN
receiver is introduced. The transcript preserves `trace` records, exact payload
hex, identifiers/format, `processed_us` decimal strings and host receive times.
Before debug off, a fixed 0.2-second observation window keeps raw diagnostics
enabled. Current firmware copies at most 24 owner diagnostics per service turn
into a separate 80-entry application buffer (1920 bytes of compact records),
independent of UART pressure, and serializes one record per turn. All diagnostic
kinds remain represented. Debug off captures the remaining bounded owner queue,
stops capture and reports `trace_pending`: buffered records that follow its FIFO
reply. The runner waits for that declared count under the existing command timeout,
then observes another fixed 0.2-second window. Older firmware lacking this field
gets only the fixed observation window. Empty reads do not end these waits.
Each uses the supplied monotonic clock, at most 65536 received bytes and a finite
turn allowance even when the clock freezes. Missing declared records or partial
lines at cutoff fail with retained diagnostics. Metadata appears under `raw_trace`
as `before_disable_tail`, `declared_pending_tail` and `after_disable_tail`.
The debug replies retain owner `trace_overwritten` and application `trace_dropped`.
An increase, regression or saturated counter sets `loss_observed:true` and fails
requested tracing while preserving `workload_verified` and native results. No retry
is added. `coverage_complete` remains false: review each trial's required TX/RX
pairs and drop evidence. These software timestamps and counts establish neither
empty remote queues nor exact wire timing. Controller/drop counters remain in the
cached snapshots. Existing record, byte, time and evidence limits apply, so
trace pressure can fail a campaign instead of silently losing required evidence.
The summary's `raw_trace` records enable/disable replies, refusal or skipped cleanup.
If the stream is poisoned, disconnected or its evidence sink failed, no debug-off
command is sent; the unresolved local trace setting remains explicit. A healthy
disable refusal fails the session while preserving any primary failure or uncertain
motion result. Trace disable never stops a motor. A separate trace-enable invocation
before starting a runner can leave queued structured output at startup and is not
the supported procedure.

## Supplied fixture records

A device campaign's `--fixture` is a JSON object supplied for this invocation.
It declares `protocol:"canopen"`, `profile:"lichuan_c"`, nonempty `model`,
`firmware` and `dialect` strings, and a `capabilities` object with explicit
boolean values. Enable the capability for the selected scenario and, when
planned, `stop`. Each required evidence section is an object with a nonempty
`evidence` string pointing to the relevant retained source/fixture evidence.

| Scenario | Required evidence sections |
| --- | --- |
| Position | `command_scale`, `reference`, `io`, `stop`, `configuration` |
| Velocity | `command_scale`, `io`, `stop`, `configuration` |
| Homing | `command_scale`, `reference`, `io`, `stop`, `configuration` |
| Stop | `stop` |
| Settings | `configuration` |

`stop.method` must equal the requested `normal` or `fast` policy. Homing also
requires `io.home_inputs_permit:true` and an integer `io.method` matching the
requested method. These are declarations of supplied provenance, validated before
port access. They neither prove a match to the connected drive nor grant native
application qualification. Keep effective map, heartbeat, scale/reference,
configuration, I/O, firmware/dialect and relevant evidence references in these
sections; unknown facts remain unknown. The ordinary application still checks
current native prerequisites and the same Q20 acquisition gate.
`heartbeat` and `pdo_map` keys must be present, explicitly null when unavailable.
I/O evidence records effective protective assignments; ordinary motion does not
inherently require a connected homing switch.

## Campaign failures and cleanup

Each scenario owns its preplanned cleanup in `run(console, record)`. The shared
session owns connection and evidence lifetime. Position/home plan one separate
stop; finite velocity uses its native stop and plans a separate stop only on
failure. Repeat plans one stop for the complete comparison. Settings never
restore or save automatically. A stop is attempted at most once when motion
handoff is possible and framing/evidence remain usable. A pure prerequisite
refusal sends no cleanup stop. A poisoned stream records skipped cleanup and
sends no further command.

`cleanup_stop_attempts` retains the distinct attempt or its skipped reason.
Successful cleanup cannot erase a failed or uncertain primary operation; failed
cleanup makes otherwise successful requested work fail. Port-close and evidence
failures also prevent a passing session. A stop terminal establishes only its
reported evidence. It proves no physical standstill, restoration or electrical
timing. There is no automatic recovery, reconnect, command replay or fault clear.

## Session artifacts and report package

`--out NEW_PREFIX` reserves `NEW_PREFIX.json`, `NEW_PREFIX.jsonl` and
`NEW_PREFIX.zip` before opening the port. A supplied suffix is replaced with
these suffixes, so use a new plain prefix for each run. The parent directory must
exist. Any existing artifact rejects the invocation without opening the port or
overwriting earlier evidence. A reservation failure removes only this call's
new empty reservations.

The summary records scenario intent, supplied fixture facts, library/profile
identity, selected route, command/operation identities, native terminal evidence,
cleanup, host deadlines, elapsed time and bounded client/drop diagnostics. Before
and successful after-snapshots use cached `config`, `status`, `health`, `stats`,
`drv` and `memory`; cached configuration is distinct from Q20-gated device
configuration acquisition. Summaries keep the latest 16 operations, separate
total/failure counts and the first primary failure. Repeated trial evidence stays
bounded by the repeat count. Summary JSON is limited to 512 KiB.
Operation ID allocation and receipt delivery are separate: reserved stop output
can precede an earlier ordinary receipt. The client retains at most1024 seen IDs
for the session and rejects reuse or epoch regression on newer requests.
Caller inputs are limited to4096 encoded bytes. Device fixture sessions retain
the cached four-part axis listing when the firmware advertises it; a detached
offline workspace is not promoted to native scale or reference qualification.

The transcript streams the ordinary Console events and raw received bytes as
hex under the same 2048-event/2-MiB bounds as `bench_probe`. It retains unsuccessful
stop evidence and has no unrelated host inventory. SDK identity and firmware
epoch remain null when the console supplies none. A supplied firmware hash does
not attest the flashed image; the image itself is excluded from the report.
Physical observation remains absent unless independently supplied and retained.

The ZIP contains only `manifest.json`, `summary.json` and `transcript.jsonl`, in
that sorted order. The manifest records byte counts and SHA256 hashes of the
exact summary/transcript bytes. Entries use `ZIP_STORED`, fixed permissions and
the timestamp 1980-01-01 00:00:00. Identical input artifacts produce identical ZIP
bytes; two live runs naturally differ in their evidence and times. Packaging
scans no directories and adds no firmware, environment dump or unrelated files.

Session verdicts distinguish `planned`, injected `host-tested`, checked
`live-observed`, `failed` and `interrupted` evidence. A live console result never
claims physical qualification. Exit 0 means a valid plan or successful requested
console workload, exit 1 an execution/cleanup failure, and exit 2 invalid input
or setup failure. A package/evidence-write failure returns nonzero; an incomplete
artifact set is not a passing session, even if one file contains earlier
successful work. Retain failed artifacts and select a new output prefix for a
separate invocation.
Packaging failure truncates this attempt's archive, so it cannot retain an older
passing summary. The loose summary is rewritten as failed where storage permits;
an unavailable evidence medium can leave incomplete artifacts and a nonzero exit.
