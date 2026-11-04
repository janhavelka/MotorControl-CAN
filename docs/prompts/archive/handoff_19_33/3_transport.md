> Executed handoff, retained for traceability. Do not execute this copy. All 22 packets ran between 2 and 4 November 2026. The [execution table](../../../reports/handoff_19_33/README.md) records what each delivered, each packet report records what remains, and the [backlog](../../../backlog.md) rows H03 and H04 track the physical trials still owed. Relative links were relocated; task wording is retained from `2920479`.

# Packet 3: TWAI submission, receive ordering and TX retirement

Execute this packet fully. Apply [COMMON.md](COMMON.md). Prerequisite: packet 2.
Execute W3, DEC-4 and C-03/04/07/13/14/20, reopening original 27 where necessary.

## Read and implement

Inspect `adapters/twai/IdfTwaiDriver.{h,cpp}`, `TwaiAdapter.{h,cpp}`, the moved
owner, `examples/common/FirmwareProbe.cpp`, `EspProbePlatform.cpp` and
`examples/s2_bench/main/main.cpp`. Read pinned IDF5.5.5 source identified by
`docs/adapters/twai_sources.json`, `twai_sdk.md`, `twai_esp32s2.md` and
`twai_receive_time.md`. Refresh RS retirement, ADS owner service and INA validity
callers; they supply patterns, not CAN transmission proof.

The current driver uses `fail_retry_cnt=0`, boolean submission and a receive-pending
guard. Reproduce current behavior with fake driver tests before changing it.
Implement the audit's explicit SENT/DEFERRED/FAILED submission distinction.
DEFERRED has proved no SDK acceptance and retains queued intent; FAILED must
retain whether acceptance/effects are uncertain. Preserve buffer ownership.
Withdraw proved-unsent work without falsely quarantining its channel.

Apply `fail_retry_cnt=-1` only after confirming the selected S2/S3 SDK semantics.
Hardware retry is not an application retry: a no-ACK frame can remain pending
until explicit controller retirement, stall the interface and have uncertain
external effects. Preserve deadlines, retirement evidence and no application
replay. A false return must not invent nontransmission.

Split event polling before owner service from TX promotion afterward. Update all
real callers and tests, including s2_bench. A same-tick RX/TX-report ordering race
still needs explicit handling; do not call it impossible because the loop changed.
Start the applicable response clock/possible-effect boundary at actual SDK
submission, or use positively proved-unsent evidence to avoid false quarantine.
Preserve conservative ingress intervals and deferral of frames whose upper bound
is later than the current owner cutoff. No processing timestamp relabeling.

Keep actual fault reasons distinct from clock faults. Apply the reviewed Classical
DLC9–15-to-eight-byte normalization at the adapter boundary; reject unsupported
FD/invalid formats explicitly. Preserve 11/29-bit and data/RTR framing contracts.

## Test and review

One writer owns driver/adapter/callers. Delegate adversarial timeline tests and
independent SDK-source review. Test arbitration loss, no ACK, RX arriving during
promotion, same-tick reply/TX-done/urgent admission, controller failure, deferred
withdrawal, deadline and recovery with queued bytes. Two-node regression: node 1's
pending frame must not make node 2's never-submitted stop falsely transmitted or
unresolved. Retained terminal results must survive later events.

Use `twai_driver`, `twai_adapter`, `firmware_probe`, `esp_s2_probe_platform`,
`nmt_owner`, `network_receive_time`, owner/recovery suites and the full verifier.
Run actual SDK/firmware builds and required CI. Preserve the meaning of old FAILED
tests; separately add DEFERRED cases rather than deleting adverse assertions.
Packet 11 owns live diagnostic load tests. Do not induce an electrical fault now
or count a host fake as measured arbitration behavior. Update adapter/owner
guidance, original 27 report and packet 3 report, then commit/sync.
