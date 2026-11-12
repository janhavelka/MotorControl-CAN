# Python console client

`scripts/bench_probe.py` provides one bounded console session and one explicit
probe or typed read. It uses [the shared console protocol](cli_contract.md),
through the application's existing owner and read routes. Python does not own
a CAN controller or implement a second CANopen driver. Campaigns, repetition
and session orchestration belong to prompt 44.

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
mode is a live result. There is no `--list`, `--count` or automatic recovery mode.

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

Connection, inspection and failure cleanup never imply enable, motion,
reconfiguration, fault clearing or replay. Closing the port sends no cleanup
command. Hardware qualification and previously retained operations remain
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
The reusable API supports the finite read forms and cached/local lookup routes
listed by `supported_command`; it does not accept arbitrary firmware commands.

Request IDs, admitted operation IDs, original command IDs, record kind, native
terminal and deadline are checked independently. Exact decimal strings remain
strings and Python integers remain exact. Trace records are observed without
consuming another operation's evidence. Duplicate/stale records, malformed JSON,
wrong identity, reboot diagnostics, disconnect or framing loss poison the
session permanently. There is no reconnect, retry or automatic result polling.

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
unknown after admission. A native failure stays distinct from stream damage;
a later parsing failure cannot erase an already checked terminal. Native
`uncertain` remains verbatim or null when absent. A serial write attempt proves
neither CAN TX nor protocol acknowledgement.

| Resource | Bound |
| --- | --- |
| Console wire record | 511 bytes including newline; JSON nesting at most 16 levels |
| Client storage | Four pending requests, two retained admitted operations, 128 records per request |
| Input/work | 65536 received bytes per command, 1 MiB per session, 1024 commands, 8192 pump turns per pending request |
| In-memory diagnostics | Latest 64 raw lines and 16 parse failures; dropped raw-line count retained |
| Live JSONL | 2048 events and 2 MiB including a reserved limit notice; 16384 bytes per event |
| Pure summary | 256 records, 262144 serialized ASCII JSON bytes, 16 container levels; execution summary at most 16 operations |

Exhaustion stops further work. These bounds are client/storage limits, not
universal drive or CANopen capacities.

Before each handoff, already-buffered input is routed or rejected under finite
byte/time/turn bounds. Split records for existing work keep its original deadline.
An empty host read cannot prove that device queues are empty or resolve the
missing firmware epoch.

Exit 0 means a valid offline plan/self-test or a successful nonuncertain live
read. Exit 1 means execution, native, identity, dependency or stream failure;
exit 2 means invalid arguments or setup/artifact creation failure. A failed
invocation never becomes a passing campaign. Offline success and
`live-observed` parsing never establish physical completion or qualification.

The [prompt-43 source audit](audit/PROMPT_43_SOURCES.md) records which RS and
sensor runner behaviors were adopted or rejected. [The source map](source_map.md)
locates the client, pure summaries and their focused tests.
