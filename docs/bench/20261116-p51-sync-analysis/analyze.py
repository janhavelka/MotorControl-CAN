"""Summarize retained P51 traces; never opens a device or assigns wire cycle IDs."""
import hashlib
import json
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
BENCH = ROOT / "docs" / "bench"


def span(values):
    """Return the measured extrema; an empty input has no measurement."""
    return {"min": min(values), "max": max(values)} if values else None


def trial(period_ms):
    """Count independent copied RX and owner TX records in one finite capture."""
    path = BENCH / ("20261116-p51-type1-%dms" % period_ms) / "rx.bin"
    raw = path.read_bytes()
    lines = raw.decode("ascii").splitlines()
    records = [json.loads(line) for line in lines if line.startswith("{")]
    enqueues = [int(r["processed_us"]) for r in records
                if r.get("type") == "trace" and r["kind"] == 1
                and r["can_id"] == 0x80 and r["dlc"] == 0]
    tx_reports = [r for r in records if r.get("type") == "trace" and r["kind"] == 2]
    frames = [dict(re.findall(r"(\w+)=(\S+)", line))
              for line in lines if line.startswith("PDOW_F ")]
    tpdo = [r for r in frames if r["id"] == "181"]
    terminal = [r for r in records if r.get("type") == "terminal" and r.get("id") == 5203]
    watch = [dict(re.findall(r"(\w+)=(\S+)", line))
             for line in lines if line.startswith("PDOW_END ")]
    debug = [r for r in records if r.get("type") == "reply" and r.get("command") == "debug"]
    assert len(terminal) == 1 and terminal[0]["outcome"] == "LOCAL_TX"
    assert terminal[0]["local_tx_count"] == 20 and len(enqueues) == len(tx_reports) == len(tpdo) == 20
    assert len(watch) == 1 and watch[0]["reason"] == "duration"
    assert all(r["dlc"] == "7" and r["timed"] == "1" and r["exact"] == "0" for r in tpdo)
    assert watch[0]["tapDropped"] == "0" and watch[0]["unprinted"] == "0"
    assert debug[-1]["trace_dropped"] == 0
    period = period_ms * 1000
    return {
        "input": str(path.relative_to(ROOT)).replace("\\", "/"),
        "sha256": hashlib.sha256(raw).hexdigest(),
        "requested_period_us": period, "requested_count": 20,
        "terminal": terminal[0], "tx_enqueue_records": len(enqueues),
        "local_tx_report_records": len(tx_reports), "tpdo1_rx_count": len(tpdo),
        "emcy_rx_count": sum(r["id"] in ("81", "82") for r in frames),
        "tpdo1_node2_rx_count": sum(r["id"] == "182" for r in frames),
        "enqueue_interval_us": span([b-a for a, b in zip(enqueues, enqueues[1:])]),
        "enqueue_phase_relative_first_enqueue_us": span([t-enqueues[0]-i*period for i,t in enumerate(enqueues)]),
        "rx_upper_bound_interval_us": span([int(b["hi"])-int(a["hi"]) for a,b in zip(tpdo,tpdo[1:])]),
        "rx_bound_width_us": span([int(r["hi"])-int(r["lo"]) for r in tpdo]),
        "ordered_rx_bounds_after_enqueue_us": {
            "earliest": span([int(r["lo"])-t for r,t in zip(tpdo,enqueues)]),
            "latest": span([int(r["hi"])-t for r,t in zip(tpdo,enqueues)])},
        "payloads": sorted({r["data"] for r in tpdo}), "watch": watch[0],
        "debug_after": debug[-1],
        "interpretation": "Ordering is an experimental comparison only. Enqueue and processing times are not physical TX times; no on-wire cycle identity or same-cycle sample qualification is established."
    }

if __name__ == "__main__":
    result = {"trials": [trial(100), trial(10)], "scope": "One node, twenty SYNCs per run; not sustained two-axis qualification."}
    Path(__file__).with_name("summary.json").write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    for row in result["trials"]:
        print(row["requested_period_us"], row["tx_enqueue_records"], row["tpdo1_rx_count"], row["enqueue_interval_us"], row["rx_bound_width_us"], row["ordered_rx_bounds_after_enqueue_us"])
