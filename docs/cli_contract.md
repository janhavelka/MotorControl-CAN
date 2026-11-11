# Shared console contract

Prompt 40 implements the shared local console in `examples/common/ProbeConsole.*`.
The native entry is `examples/native_probe`; Arduino and ESP-IDF compile these
same sources. Device routes remain owned by prompts 41 and 42.

Commands accept printable ASCII and TAB, with CR, LF or CRLF ending a line.
There are at most 255 text bytes and 24 command tokens, plus an optional `@ID`
prefix. An ID is decimal 1..4294967295 without a sign. Invalid, truncated,
overlong or overflow-damaged lines are discarded whole before dispatch. A separate
loss latch remains effective after the diagnostic drop counter saturates.
No quoting or shell escape syntax exists. Exact values use the public
`parseExactNumber` grammar: integers, decimals or fractions; no exponent syntax.

Plain commands produce human text. `@17 version` requests JSONL. Every JSON
record identifies `schema:"motorcontrolcan.console/1"`, `protocol:3` and
`profile:"lichuan_c"`. The version reply additionally identifies
`product:"MotorControl-CAN"` and its generated version. The schema identity is
required: this is not wire-compatible with MotorControl-RS protocol 3.

`id` is a request correlation, `operation_id` identifies admitted asynchronous
work, and `command_id` retains that work's original request. They are independent
uint32 JSON integers. Wide signed/unsigned values, exact fraction components,
timestamps and native coordinates are decimal **strings**. Unknown values are
`null`; known booleans and zero remain `false` and zero. No float round trip is
used for an integer. Binary floating previews are explicitly approximate. Inactive exact-error fractions are `null`, never a default zero-error claim.

Local commands return `type:"reply"`, `command`, `ok` and typed `status` (stable Err label); terminal `detail` preserves the signed call-specific detail.
Asynchronous admission returns `result:"accepted"` and an operation ID. The
terminal `type:"terminal"` retains the original `id`, `command_id`, operation ID,
`outcome`, `status` and `uncertain`. Each admitted operation freezes its human or
JSON format. Other commands cannot change its terminal format. `result ID`
returns the same retained evidence with a fresh lookup `id`, original
`command_id` and operation ID. Pending work returns `result:"pending"`.
Release is explicit and requires that the terminal record has been delivered.
Neither a local ID nor a lookup resolves an ambiguous wire response.

Help and capability lists stream bounded records followed by a final reply.
The canonical compiled inventory specifies grammar, API, capability, side-effect
class, completion shape and owner. Gated rows appear only in `caps`, with the
reason and owner; they are absent from dispatch and working help.

Each complete output record is at most 511 bytes including newline, queued
atomically. Two ordinary and two reserved local-control records bound storage.
The platform's 512-byte ring may write a record over multiple turns, retaining
its suffix. Records are never interleaved. Output pressure can refuse an ordinary
command before mutation; whole refused replies are counted. `cancel` and
`recover` have reserved reply capacity. No command executes when its required
reply capacity is exhausted. Terminal results remain retained under pressure.
Owner service runs before console service and never waits for output.
Release waits for every terminal byte to be accepted by the platform callback;
this is a local delivery boundary, not proof that a remote terminal read it.
Asynchronous request IDs stay reserved through explicit release. Immediate reply
IDs are reserved while held in the console, then become reusable when their
complete record is published to the platform queue. Clients wait for the reply
before reusing an ID. A pressure-refused command is never executed or retried.

`stats.counters` has this fixed positional order: `txFrames`, `rxFrames`,
`txRejected`, `rxDropped`, `rxLost`, `unmatchedFrames`, `queueRejected`,
`resultsRejected`, `cancelled`, `timeouts`, `busOffEvents`, `recoveries`,
`traceOverwritten`, `consoleDropped`. All are uint32 JSON integers. The first
13 and the decimal-string `maxServiceGapUs` come from the owner. `stats reset`
clears owner totals/high-water marks and binding success/failure totals only;
it preserves health streaks, timestamps, errors, faults, generations, history,
uncertainty, retained results, live buffers and console pressure counters.

## Local routes

`help [command]`, `version`, `profile list`, `caps`, `config`, `status`, `health`,
`stats [reset]`, `drv`, `memory`, `result [ID]`, `release ID`, `cancel [ID]`,
`recover`, `wiring [di0..di6|do0..do2 unknown|unconnected|connected]`, `axis` and
`prepare` comprise this increment. The compiled profile inventory lists Lichuan C
only; it claims no detected model, firmware match or hardware qualification.

`config` inspects host facts only. Interface/bitrate configuration comes from the
application, not guessed board settings. `wiring` records host declarations;
labels do not establish terminal existence, function assignment or electrical
level. `stats reset` clears documented counters only. `cancel` cancels local read
work; `recover` requests bounded host recovery, with a caller-console policy of
one second, without stopping/resetting/enabling a drive or replaying work.

The offline axis workspace is separate from selected live bindings. It starts
without geometry, origin or reference evidence. `axis config` inspects it;
`axis config set FIELD VALUE [MAXIMUM]` edits `command`, `gear`, `fullsteps`,
`lead`, `encoder-scale`, `encoder-id`, `encoder-basis`, `encoder-polarity`,
`polarity`, `native-limits`, `soft-limits` or `relative-bases`. Optional scales and
soft limits accept `none`. Encoder basis is `motor`, `load` or `linear`; polarity
is -1 or 1. Limits are inclusive exact integers. Display-unit fields from RS are
absent because this common AxisConfig has no such fields. `axis origin INTEGER`
uses the public origin setter and requires a fresh application-supplied stationary
reference; it never fabricates a position observation or clears a drive counter.

```text
prepare absolute VALUE UNIT FRAME [ROUND [MAX_ERROR [RADIAN_ERROR]]]
prepare relative VALUE UNIT FRAME BASIS [ROUND [MAX_ERROR [RADIAN_ERROR]]]
prepare angle VALUE UNIT FRAME PATH TIE [ROUND [MAX_ERROR [RADIAN_ERROR]]]
```

Units: `steps`, `fullsteps`, `counts`, `turn`, `deg`, `rad`, `mm`. Frames:
`native`, `motor`, `load`. Bases: `actual`, `commanded`, `queued`. Paths:
`positive`, `negative`, `shortest`; ties: `reject`, `positive`, `negative`.
Rounding: `exact`, `nearest` (ties to even), `zero`, `floor`, `ceil`.
Nonzero radians need a positive explicit approximation allowance. All conversion,
limit, rounding and reference decisions use the production public Axis/Units APIs.
Each successful offline configuration edit retires the previous origin, reference
and soft limits. Set the final limits after the scale facts. `axis config` streams
a frozen four-part snapshot (geometry, feedback, limits, provenance), then a final
reply. The geometry key is `command_scale`; it cannot shadow envelope `command`. Scale arrays are `[numerator_string, denominator_string, provenance]`,
where provenance is the public `ScaleSource` numeric enum; unknown scales are
`null`. These are host assumptions unless the application supplies stronger facts.

Build and exercise the real native entry without a controller:

```powershell
cmake -S . -B .build/native -G Ninja -DBUILD_TESTING=OFF -DMOTORCONTROLCAN_BUILD_EXAMPLES=ON -DMOTORCONTROLCAN_BUILD_CANOPENNODE=ON -DMOTORCONTROLCAN_CANOPENNODE_DIR="$PWD/experiments/canopen_service_fit/vendor/CANopenNode"
cmake --build .build/native --target native_probe
@('@1 version','@2 axis config set command 1','@3 prepare absolute 9007199254740993 steps native') | Set-Content -Encoding ascii .build/console.txt
.build/native/examples/native_probe/native_probe.exe --console-fake .build/console.txt
```

On Linux the executable has no `.exe` suffix; use `printf '%s\n'` to create
the same ASCII script. [The native README](../examples/native_probe/README.md)
specifies the 4096-byte batch and 64 KiB capture limits. A nonempty script must
end in CR/LF; EOF never completes a potentially truncated command. File/stdout I/O runs
outside the fake owner service loop. In default Arduino/IDF entries the shared
console runs after the owner turn, even with CAN inactive. An explicit test-only
smoke hook keeps exclusive ownership of its own input protocol and bypasses the
console; the separate S2 evidence parser remains a bench facility.

Bare `reset`, `ping`, `ver`, `?`, `health check`, `help advanced`,
`profile ess_rs`, `profile ess_rs enable`, `stop direct` and `motor ...` are
rejected. No raw object access or placeholder device command is accepted.
