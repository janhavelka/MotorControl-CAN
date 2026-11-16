# HIL campaign after the power cycle, 2026-11-16

Report: [2026-11-16_hil_campaign.md](../../reports/2026-11-16_hil_campaign.md). The lead alone used COM15 (serial
`94A990398B40`); one subagent researched the console path read-only.

## Bounds

- The user's request: "do all HIL testing regarding new implementation and finish up all tests that are missing and
  are able to run now.. Motor power cycled". The standing bench authorization of 2026-11-02 applies; motion routes
  carry that text through `--unattended-waiver` (the runner's recorded-authorization field).
- Fixture as in [prompt 54](../20261116-p54/README.md): OPTAK_FGI_2025 ESP32-S2, TJA1051, two CL86-C with LC60H2127
  motors, nodes 1 and 2, 1 Mbit/s. No shaft, supply, wired-stop or independent CAN observation.
- Image `17386514` (ELF `20cc8ea2527993d8aa2acaa31be3508cd8b15bde598cb8e736ed7779439f0d54`), flashed at 09:15Z
  ([upload](../20261116T0915Z-s2-upload/), [ping](00-ping/)).
- Prompt 54's two restrictions (node 1's unresolved stop 52012, node 2's interrupted-position fence) were retired by
  the user's drive power cycle: both drives read the power-up report at 09:19Z (series S0001, S0002). A new
  campaign ledger per series (one MCU boot each) follows the 2026-11-13 ladder practice; the prompt 54 ledger was not
  reused or reset.
- Configuration changed and not saved: `1017=1000` on both nodes (heartbeat; volatile), TPDO1 type 1 during the
  campaign (restored, see the report), `2201`/`2500` as the report states. No save, factory restore or reset.

## Layout

| Path | Content |
| --- | --- |
| `00-ping` | Image check against the upload |
| `10-network` to `15-operational-sync` | NMT/heartbeat/TPDO1 setup plans' sessions (`s2_pdo.py`), raw |
| `16-sync-10ms-short`, `17-sync-20ms-30min` | `sync_watch.py` captures, raw |
| `S0001-…` to `S0092-…` | One MCU boot per series: route records (`s2_bench.py`), console sessions, `sched-*` single-port console sessions with `rx.bin`, `transcript.txt`, `records.jsonl`, `summary.json`, the series' campaign ledger; packed in `packed/` (lossless tar.xz, `MANIFEST.json` with every member's SHA-256) |
| `night.log` | The lead's log of every route, key record lines and decisions |
| `ladder.jsonl`, `results.jsonl`, `state.json` | Driver notes, route log and state (trial counter from 56000) |
| `run-*.out` | Driver console output per phase |
| `plans/` | The `s2_pdo.py` plans |
| `tools/` | The drivers, copied for provenance: `hil.py` (the 2026-11-13 ladder driver pointed here), `hilstep.py` (steps: velocity/position change, M14, state, loop, subdivision), `conhil.py` and `conhil2.py` (console sessions), `sched.py` (single-port scheduled console executor), `hilsum.py` (record summaries), `hil_p*.sh` (phase programs), `pack_hil.py` |

## Reading the records

- Route records: `TRIAL id=… failure=… why=…` heads each; `failure=10` after a quick-stop velocity run is the known
  unconfirmed-stop reserve limit, not a fault (2026-11-13 report).
- `sched-*` sessions: `summary.json` lists every step's send time and reply, the refusals and the saved operation
  IDs; `records.jsonl` holds every console JSON record with its host time.
- The first console trials (`run-console.out` up to 10:15Z) used one helper invocation per step; their records
  between invocations were lost, and two of them hit a request-ID reuse bug in the driver (a planned stop refused as
  `duplicate_id`; each run then ended by its own fast stop at 12 s). They are kept and marked in the report.
