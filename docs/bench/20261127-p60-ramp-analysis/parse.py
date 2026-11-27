"""Step 1: parse retained rx.txt captures into per-record SDO timelines.

Usage (from the repository root): python -I parse.py BENCH_DIR OUT_JSON

Reads every loose BENCH_DIR/**/rx.txt and every rx.txt member of BENCH_DIR/*/packed/*.tar.xz
(members are read in memory, nothing is extracted). Keeps the records that write a motion
object (6040, 60FF, 6081, 609A or 6099). Opens no device and uses no network.

Each event is (kind, node, object, subindex, value, request_us, reply_us, bytes):
kind 'w' (confirmed download), 'r' (expedited upload), 'abort' or 'pdo'. Values are the raw
unsigned little-endian bytes; ana.py applies each object's signedness.
"""
import collections
import glob
import hashlib
import json
import os
import re
import sys
import tarfile

TR = re.compile(r'^TRAFFIC k=(\d) p=(\d+) id=([0-9a-f]+) ext=\d rtr=\d dlc=(\d) data=([0-9a-f]{16})'
                r'(?:.*?bounds=(\d) lo=(\d+) hi=(\d+))?')
MOTION = ('6040', '60ff', '6081', '609a', '6099')
DOWNLOAD_BYTES = {0x23: 4, 0x27: 3, 0x2b: 2, 0x2f: 1}


def le(b):
    return int.from_bytes(b, 'little')


def parse(lines, src):
    pend = {}  # node -> (request_us, command, (index, subindex), data bytes)
    ev = []
    plan = None
    for line in lines:
        if line.startswith('TRIAL_PLAN'):
            plan = line.strip()
        m = TR.match(line)
        if not m:
            continue
        k, p, cid, dlc, data = int(m[1]), int(m[2]), int(m[3], 16), int(m[4]), bytes.fromhex(m[5])
        if k == 1 and 0x601 <= cid <= 0x67f:
            node = cid - 0x600
            pend[node] = (p, data[0], (le(data[1:3]), data[3]), data[4:8])
        elif k == 0 and 0x581 <= cid <= 0x5ff:
            node = cid - 0x580
            if node not in pend:
                continue
            tp, cmd, obj, wdat = pend.pop(node)
            rc = data[0]
            robj = (le(data[1:3]), data[3])
            hi = int(m[8]) if m[8] else p
            if robj != obj:
                continue
            if rc == 0x80:
                ev.append(('abort', node, '%04x' % obj[0], obj[1], le(data[4:8]), tp, hi, 4))
                continue
            if cmd in DOWNLOAD_BYTES and rc == 0x60:
                n = DOWNLOAD_BYTES[cmd]
                ev.append(('w', node, '%04x' % obj[0], obj[1], le(wdat[:n]), tp, hi, n))
            elif cmd == 0x40 and (rc & 0xe0) == 0x40:
                n = 4 - ((rc >> 2) & 3) if rc & 1 else 4
                ev.append(('r', node, '%04x' % obj[0], obj[1], le(data[4:4 + n]), tp, hi, n))
        elif k == 0 and 0x181 <= cid <= 0x4ff:
            ev.append(('pdo', cid, None, None, data.hex()[:2 * dlc], p, p, dlc))
    return {'src': src, 'plan': plan, 'ev': ev}


def keep(record):
    return any(e[0] == 'w' and e[2] in MOTION for e in record['ev'])


def main(base, out):
    recs = []
    inputs = []  # (relative path, sha256 of the original bytes)
    for f in sorted(glob.glob(os.path.join(base, '**', 'rx.txt'), recursive=True)):
        rel = os.path.relpath(f, base).replace(os.sep, '/')
        with open(f, 'rb') as fh:
            raw = fh.read()
        inputs.append((rel, hashlib.sha256(raw).hexdigest()))
        r = parse(raw.decode('utf-8', 'replace').splitlines(), rel)
        if keep(r):
            recs.append(r)
    for a in sorted(glob.glob(os.path.join(base, '*', 'packed', '*.tar.xz'))):
        rel = os.path.relpath(a, base).replace(os.sep, '/')
        with open(a, 'rb') as fh:
            inputs.append((rel, hashlib.sha256(fh.read()).hexdigest()))
        with tarfile.open(a) as t:
            for mm in t.getmembers():
                if not mm.name.endswith('rx.txt'):
                    continue
                txt = t.extractfile(mm).read().decode('utf-8', 'replace').splitlines()
                r = parse(txt, rel + '::' + mm.name)
                if keep(r):
                    recs.append(r)
    digest = hashlib.sha256(''.join('%s %s\n' % i for i in inputs).encode()).hexdigest()
    loose = sum(1 for i in inputs if i[0].endswith('rx.txt'))
    with open(out, 'w', encoding='utf-8', newline='\n') as fh:
        json.dump({'inputs': {'loose_rx': loose, 'archives': len(inputs) - loose, 'sha256': digest},
                   'records': recs}, fh)
    print('inputs: %d loose rx.txt, %d archives, list sha256 %s' % (loose, len(inputs) - loose, digest))
    print('records with motion writes:', len(recs))
    c = collections.Counter(e[2] for r in recs for e in r['ev'] if e[0] == 'w')
    print(c.most_common(30))


if __name__ == '__main__':
    if len(sys.argv) != 3:
        sys.exit(__doc__)
    main(sys.argv[1], sys.argv[2])
