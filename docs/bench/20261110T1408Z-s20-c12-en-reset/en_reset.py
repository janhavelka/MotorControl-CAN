"""One EN reset of the S2 through the ESP-Prog-2 (RTS asserted, DTR deasserted so IO0 stays high and the
application boots), as esptool's hard reset and s2_smoke.py --reset-en do. Records what it did in a new
directory. Usage: python en_reset.py OUTPUT_DIR REASON"""
import json
import pathlib
import sys
import time
from datetime import datetime, timezone

import serial
from serial.tools import list_ports

out = pathlib.Path(sys.argv[1])
reason = sys.argv[2]
out.mkdir(parents=True, exist_ok=False)
[item] = [p for p in list_ports.comports() if p.device.upper() == "COM15" and p.serial_number == "94A990398B40"]
port = serial.Serial(port=None, baudrate=115200, timeout=0.05)
port.dtr = port.rts = False
port.port = "COM15"
port.open()
started = datetime.now(timezone.utc).isoformat()
port.rts = True
port.dtr = False
time.sleep(0.1)
port.rts = False
port.dtr = False
captured = bytearray()
end = time.monotonic() + 3.0
while time.monotonic() < end:
    captured += port.read(4096)
port.close()
(out / "rx.bin").write_bytes(bytes(captured))
(out / "reset.json").write_text(json.dumps({
    "tool": "en_reset (lead scratch script, copied here)", "started_utc": started, "port": "COM15",
    "serial": item.serial_number, "via": "RTS asserted 0.1 s, DTR deasserted", "reason": reason,
    "captured_bytes": len(captured)}, indent=2) + "\n", encoding="utf-8")
(out / "en_reset.py").write_bytes(pathlib.Path(__file__).read_bytes())
print("reset done;", len(captured), "bytes captured")
print(captured.decode("utf-8", errors="replace")[:600])
