# Unattended night soak, 2026-11-11 17:45Z to 23:29Z: new console features on hardware, motion soak

The user's written waiver, verbatim: "test all the new features added - do a full night soak without my attendance /
You can do motion tests, speeding up, changing speed on the fly etc / Finish at 9:00 next day in the morning".
Nobody was at the bench; every result is a drive or firmware report, and no physical observation was made. Motion
routes ran through `s2_bench.py --unattended-waiver TEXT --waiver-covers-motion` (new, `8206274`); console arming
lines carry attended=1 under the same waiver. The soak stopped at 23:29Z when the host PC rebooted, so it covered
5.7 h, not the planned night. The S2 kept running through the reboot. At 09:37Z node 1 read released at standstill
and node 2 read unchanged in its held state (below). Evidence: [20261111-night](../bench/20261111-night/NOTE.txt)
(log, results, 167 packed series with a member manifest, tools) and the `20261111T*`/`20261112-final` directories.

## New features on hardware

| Feature (prompt) | Image | Result |
| --- | --- | --- |
| Shared console, local routes (40) | shared OPTAK console image `017b28b` ([run](../bench/20261111T1815Z-s2-console-local-routes/)) | 113 JSON requests over UART0, one final reply each, every outcome as the script expects; 0 malformed, missing, oversize or out of order |
| Console output pressure (40) | same ([burst](../bench/20261111T1821Z-s2-console-burst/)) | 15 lines at once: 5 replies dropped, `consoleDropped` +5 exactly, no input loss |
| Console soak (40) | same ([soak](../bench/20261111T1823Z-s2-console-soak/)) | 20 min, 5662 requests, 46980 streamed records, 0 problems; latency median 31 ms, max 1.33 s (streamed help at 115200) |
| Shared image smoke (28/41) | `idf-s2-optak` smoke ([run](../bench/20261111T1833Z-s2-smoke-run/)) | Passed: typed identity read on both nodes, values as retained. First shared-image run on the drives |
| Typed device routes (41) | bench image `80240dd` with the console ([run](../bench/20261111T1925Z-s2-benchcon-device/)) | `probe`, `read identity/state/monitors/feedback`, `monitor`, `network`, `heartbeat`, `health`, `status`, `stats`, `drv`, `memory`, `load` on real drives; `monitor 500 4` needs an explicit total cutoff above its 1.5 s span (the 1000 ms default ended it DEADLINE, uncertain) |
| Console motion (42) | bench image `cf6ec7c`, `e693f16` | `moveby`, `moveto` (host origin), `stop fast` during a move, finite `velocity` with its own fast stop, and an explicit `stop fast` during a velocity run: 5 move and 10 velocity sessions, every move and stop `OBSERVED`, endpoints exact; a stopped motion stays `CANCELLED`, uncertain, as specified |

Not testable: "changing speed on the fly". No library operation changes speed during a move (immediate update is
refused by design) or a velocity run (one rate per run). Instead each series ran successive moves at different
speeds (6 to 300 rpm) and velocity runs at rising rates (6 to 60 rpm, positive only). Persistence (prompt 39) was not
run: save and factory restore stay outside the authorization.

## Motion soak

151 series on both nodes, 1049 counted motion admissions, one MCU boot per series. Position series: enable from
the previous series' settled quick stop, absolute and relative moves at 6 to 300 rpm with 100 to 1000 ms ramps, a
HALT and a moving or stationary quick stop. Velocity series in mode 3: three runs at rising rates, quick stops,
or a HALT run first. Each series ended released for the next.

- **Arrivals:** 308 completed moves, 238 exactly on target and 70 off by one count (45 at +1, 25 at −1), never more.
  There is no speed dependence. They are rarer with 1000 ms deceleration ramps. Once the completion sample read 8501 and a
  later read 8500, so part of this is settling after the completion report. Round 12 had 23 of 23 exact.
- **Velocity:** 163 runs, 157 with their own stop settled. The other 6, all at 45 to 60 rpm on both nodes, had a first
  stop observation, 28 ms after `000B`, reading `606C` −1 or −3 with `6064` unchanged. The second observation, 4 s
  later, read 0. Only two observations fit the 128-terminal record at the run's 4 s cycle, so the library correctly
  reported the stop unsettled (uncertain, fenced). The drives were released at standstill each time.
- **Other refusals, all before any write:** script plan errors (capacity, the 11470 ms velocity minimum,
  position bounds after positive velocity drift); a start position one count stale after the re-enable's word-7 pull;
  and `MODE_ONE` refused later in a boot (the firmware admits it only first). Two captures were damaged by the known
  bench UART byte loss; their routes completed.

## Fixes made during the night

- `cf6ec7c`, from an independent review before console motion ran: `cancel` during a console velocity
  withdrew the run before its due-time stop, so no stop was ever sent. `ProbeMotion` now refuses it. A refused `T`
  line no longer leaks its bytes into single-byte bench commands, and input loss discards the damaged line.
- `e693f16`: `MODE_THREE` admits the latch a moving quick stop leaves (as `MODE_ONE` and velocity already do). A console
  `stop fast` during a move had left node 2 latched, and the mode change was the only route back.

## Open: node 2 is held enabled

From 20:24Z node 2 has been **enabled and halted in mode 3**, at 6064 18483 with `60FF`=12 and HALT active, not
moving. The HALT run (S0071) started from that latched release, whose 2100 in-place bit stayed clear, and halted with
`6041` 1437 and `2100` 0000. Every halted state measured before read 9437/0001. The library admits only that report,
so the halted-state quick stop, a quick stop and a best-effort quick stop in a new boot were all refused before any
write. The script then ran HALT runs only from releases with in-place set. Node 2 sat out the rest of the soak,
watched read-only every 10 minutes: unchanged every time, and again at 09:37Z. **Releasing it needs the user:** a
drive power cycle, or a reviewed library rule for the 1437/0000 halted report. Backlog S19, S20.
