"""Finite local-console network inspection; sends no CAN request or reset."""
import json
import time
from datetime import datetime, timezone
from pathlib import Path
import serial
from serial.tools import list_ports

record = Path('docs/bench/20261102-nmt-pdo-boot-watch')
record.mkdir(exist_ok=False)
ports = [p for p in list_ports.comports() if p.device == 'COM15']
assert len(ports) == 1 and ports[0].serial_number == '94A990398B40'
data = bytearray()
meta = {'started_utc': datetime.now(timezone.utc).isoformat(), 'seconds': 90,
        'port': 'COM15', 'serial': '94A990398B40', 'baud': 115200,
        'commands': 'local n inspection once per second; no device command',
        'reset': False, 'retry': False, 'transmissions': 0}
try:
    with serial.Serial(port=None, baudrate=115200, timeout=.05, write_timeout=1) as port:
        port.dtr = port.rts = False
        port.port = 'COM15'
        port.open()
        start = time.monotonic()
        next_request = start
        print('ARMED: 90 seconds of cached network inspection, no CAN TX', flush=True)
        while time.monotonic() - start < 90:
            if time.monotonic() >= next_request:
                assert port.write(b'n') == 1
                meta['transmissions'] += 1
                next_request += 1
            data.extend(port.read(min(max(port.in_waiting, 1), 4096)))
            assert len(data) <= 262144
    meta['status'] = 'captured'
finally:
    (record/'rx.bin').write_bytes(data)
    (record/'rx.txt').write_bytes(data.decode('utf8', errors='replace').encode('utf8'))
    (record/'request.json').write_bytes((json.dumps(meta, indent=2)+'\n').encode('utf8'))
print('Retained', len(data), 'bytes', record, flush=True)
