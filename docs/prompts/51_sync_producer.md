# Prompt 51 Host SYNC producer and synchronous PDO operation

Apply [COMMON.md](COMMON.md). Prerequisites: [17](17_pdo_observations.md), [24](24_owner_recovery.md), [38](38_communication_commissioning.md), [42](42_cli_motion_routes.md).

## Objective and boundaries

The user's requirements, verbatim (2026-11-12): "I want to enable the syncing - master (the mcu) will send these -
usual CANbus"; "If there is nothing existing in this regard i still want you to try the general approach of doing sync
commands (and see whether that works or not eventhough not documented)"; "could i control the motors synchronously
(although not with very fast changing - like milisecond changing, eg CSP)".

Deliver the application MCU as the CANopen SYNC producer on classic CAN, with passive SYNC observation, typed SYNC and
PDO communication reads, a conditional synchronous PDO transmission-type setting and the trials that establish what the
Lichuan drives actually do with SYNC. Synchronized operation here means a common SYNC instant for sampling and for
latching set-points or a start trigger at modest rates; it is not CSP or IP, which the Lichuan manuals do not offer
(documented modes 1, 3 and 6; the firmware's accepted set is unmeasured, backlog D17).

Target period, the user's words (2026-11-12): "10ms updating would be enough for all of my purposes". 10 ms is the
design and qualification period of this prompt and of prompt 54: the trials include 10 ms, the producer and owner are
sized for it, and the result states whether 10 ms works for each measured use (sampling, set-point latching, start
trigger). Slower periods are fallbacks, not the goal. Computed budget, to be measured, not assumed: at 1 Mbit/s one
SYNC (DLC 0) is about 0.05 ms and one 8-byte PDO about 0.13 ms of bus time, so one SYNC plus one RPDO and one TPDO
per axis every 10 ms is about 6 % bus load for 2 axes and about 50 % for 19; today's host TX design sends one frame
per service turn, so at a 1 ms turn one period holds at most 10 host frames (the SYNC plus about 9 RPDOs), which is the
host-side limit until a TX change is designed and proven.

Lichuan facts from the SYNC audit (backlog M10, M13): the CANopen manuals only claim "multi-axis high-speed bus
synchronization control" (EN86, ZH86 and ZH57 p3) and describe no SYNC object; the EDS declares `1005` SYNC COB-ID (RW
U32, default `0x80`) and writable transmission types `1400:02`..`1403:02` and `1800:02`..`1803:02` (U8, default 255, no
range), and no `1006`, `1007` or `1019`. A drive can therefore at most consume SYNC: never set `1005` bit 30. Every
synchronous behavior is undocumented for this family and stays an explicit experiment until measured.

Boundaries: one SYNC producer per network; no internal timer, thread or automatic periodic production in the installed
core (the application owns time and calls the producer); TX success is never drive effect; no RPDO is sent before a
reviewed controlword-only RPDO map exists (backlog M09; prompt 18's remaining Q17 gates are consumed when available,
and this prompt must not depend on 18); no NMT state change, save or PDO remap as a side effect.

## Read and audit

For this prompt, refresh MotorControl-RS HEAD and dirty state and read its owner and PDO seams; RS is authoritative for
common names and has no SYNC API, so record that no common name exists. Inspect FieldCore-node read-only for timing and
ownership integration. If HEAD, dirty state and seam match the last recorded row, write "unchanged since prompt N" and do
not reread. A sibling row must name a behavior adopted or rejected, with path and symbol. This does not relax vendor
evidence, safety, uncertainty or verification rules. Read the [multi-axis and SYNC survey](../reports/2026-11-12_multiaxis_sync_survey.md),
the [multi-drive preparation](../reports/2026-11-12_multidrive_preparation.md), the vendored CANopenNode `CO_SYNC` and
`CO_PDO` sources under `experiments/canopen_service_fit/vendor`, the EDS entries above, prompt 17/18/38 reports and
the N5 SYNC material in `docs/reference/multidrive` as design contrast only. Apply [CONVENTIONS.md](CONVENTIONS.md),
[MULTIDRIVE.md](MULTIDRIVE.md), [DOCUMENTATION.md](DOCUMENTATION.md), [MAINTAINABILITY.md](MAINTAINABILITY.md) and
[DELEGATION.md](DELEGATION.md). Record new gates (for example Q27 synchronous TPDO types, Q28 synchronous RPDO latching)
in `device_conflicts.md` and the ledger before implementation depends on them.

## Implement

1. A SYNC frame codec (11-bit COB-ID from the configured `1005` value, default `0x80`, DLC 0; a counter byte only with
   `1019` evidence, which Lichuan lacks) and an explicit owner SYNC service: a bounded application request with its own
   admission class that never consumes ordinary SDO slots and is ordered after urgent stops, sends one frame per
   admitted request at the application's time, reports local TX, refusal and bus-off honestly (a failed SYNC is never
   ignored, unlike CANopenNode and Lely), disarms on bus-off, and refuses while another SYNC producer is observed on the
   bus. A periodic producer state, if the application wants one, schedules on whole multiples of its period from
   supplied time (as Lely does), not by restarting a timer at each send (CANopenNode's drift); it lives in caller-owned
   storage with prepare/next/advance steps whose names follow a CONVENTIONS decision.
2. Passive SYNC observation: a foreign `0x80` frame is reported as a conflict and stops local production until the
   application resolves it; observed SYNC instants carry receive-time bounds for sample provenance.
3. Typed reads of `1005`, `1400:02`..`1403:02` and `1800:02`..`1803:02`, through the reviewed ledger and generator.
4. A conditional transmission-type setting (TPDO first), with prepare/next/advance, whole-candidate validation,
   withdrawal before possible TX, readback and retained partial effects; volatile only, never saved; closed by default
   and opened only by measured evidence or an explicit recorded experiment scope.
5. Synchronized TPDO sampling: samples taken on one SYNC carry that SYNC's identity so several axes' samples can be
   compared as one instant, under prompt 17/38's sample-qualification rules.
6. A synchronous RPDO start trigger, after the M09 map review and prompt 18's artifacts: a controlword-only RPDO on each
   axis with a synchronous type, then one SYNC as the common latch. Experiment scope only, no replay, no automatic stop
   substitute; ordinary per-axis SDO stops remain available and urgent, and a stop is never sent through a synchronous
   RPDO (it would wait for the next SYNC). Hazard (survey, inferred): an armed synchronous RPDO stays pending until the
   next SYNC, so a failed start SYNC leaves any later SYNC able to start the motion; early trials use one-shot SYNC only,
   and the trigger is withdrawn (an asynchronous controlword without the start edge) before any later SYNC.
7. Bench and console: bench routes for the trials below on the S2 image and finite console commands (for example
   `sync once`, `sync period MS COUNT`), never an unbounded producer.
8. Trials, each reported as works, ignored or refused with raw traffic: (a) one SYNC with every PDO at type 255 (expect
   no change and no EMCY); (b) TPDO1 type 1 on one node with SYNC at 10 to 100 ms (one TPDO per SYNC or not, latency),
   then restore 255 without a save; (a2) no-motion RPDO checks: a `607A`-only RPDO2 to test buffering and last-wins,
   and what the live `1400:05` = 1000 RPDO event timer does; (c) a synchronous RPDO controlword on both nodes and one
   SYNC as a common start trigger, start skew compared with the same-turn SDO trigger skew (backlog M07). On the S2
   image no flash write may occur during SYNC production or motion: its TWAI driver runs without the cache-safe ISR.
9. The 10 ms plan (backlog M13), run when items 1 to 6 exist on the bench image: (b) at exactly 10 ms on one node,
   then on both; production jitter of the SYNC against the application's time and the owner turn; one TPDO per SYNC
   per node, with missed, duplicated and late samples counted; bus load and service gaps; at least 10 minutes. What
   10 ms set-point updating means depends on prompt 52's measurements and is tested in prompt 54: in mode 3 a new
   `60FF` per SYNC needs no handshake if F02 shows a mid-run change takes effect; in mode 1 each new `607A` needs the
   bit 4 set-point handshake (a falling and a rising edge, so at least two controlwords per update) and the Lichuan
   holds bit 12 until `000F`, so a 10 ms position stream may be impossible; the trial reports which applies.

## Verify and exit

Host tests cover the codec, admission and capacity, foreign producer conflict, bus-off and recovery, deadline
boundaries, sample provenance and refusal before admission. Run the native, firmware, stack and documentation gates
appropriate to the change, and CI on the actual final commit. Bench trials need the applicable authorization; missing
hardware blocks only the trial, never the host work. A trial that shows "ignored" is a valid result and keeps the
feature closed.

## Documentation and delegation

Update `docs/canopen_services.md`, the CLI contract, coverage, backlog (M10, M13), CHANGELOG and Doxygen in the same
stage. Normal coding agents may own the codec and tests; a stronger reviewer challenges owner admission, conflict and
uncertainty rules. The lead integrates and runs combined checks. Record `docs/reports/prompt_51.md` with the trial
outcomes and remaining gates. Commit and sync on the current branch before handoff.
