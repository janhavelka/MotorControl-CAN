"""Step 2: join consecutive records into per-node timelines.

Usage (from the repository root): python -I ana.py RECS_JSON ROWS_JSON

Records of one archive (or, for loose captures, of one UTC day) are taken in name order and
joined while each starts after the previous one ends and less than 120 s later (MCU uptime).
An SDO event already seen in the same group (same node, object, value and MCU request and reply
times) is a second dump of the same traffic and is dropped, so a cached re-dump (for example
20261116-p52/52-ramp-up-cached-dump of trial 52020) is not counted twice.
For each node the output holds the 6064 position and 606C speed samples, timed at the middle of
the SDO request/reply pair, and every 6040 or 60FF write with the motion objects' state at that
moment (last value written or first value read).

Signedness: 6064, 606C, 607A and 60FF are INTEGER32, so each value is sign-extended from the
byte count actually transferred; 6081, 6083 and 6084 stay unsigned. (The stage (a) copy
converted values a second time in ramp.py, which turned negative speeds and positions into
values near -2^32; see the README.)
"""
import collections
import json
import sys

SIGNED = ('6064', '606c', '607a', '60ff')
STATE = ('6081', '60ff', '6083', '6084', '607a')


def value(e):
    v, n = e[4], e[7]
    if e[2] in SIGNED and v & (1 << (8 * n - 1)):
        return v - (1 << (8 * n))
    return v


def main(src, out):
    with open(src, encoding='utf-8') as fh:
        recs = json.load(fh)['records']
    groups = collections.defaultdict(list)
    for rec in recs:
        key = rec['src'].split('::')[0] if '::' in rec['src'] else rec['src'][:8]
        groups[key].append(rec)
    chains = []
    for key in sorted(groups):
        cur = None
        seen = set()
        for rec in sorted(groups[key], key=lambda r: r['src']):
            ev = []
            for e in rec['ev']:
                if e[0] in ('w', 'r') and tuple(e) not in seen:
                    seen.add(tuple(e))
                    ev.append(e)
            if not ev:
                continue
            t1 = min(e[5] for e in ev)
            t2 = max(e[5] for e in ev)
            if cur and t1 > cur['tend'] and t1 - cur['tend'] < 120e6:
                cur['ev'] += ev
                cur['tend'] = t2
                cur['srcs'].append(rec['src'])
                cur['plans'].append(rec['plan'] or '')
            else:
                cur = {'ev': list(ev), 'tend': t2, 'srcs': [rec['src']], 'plans': [rec['plan'] or '']}
                chains.append(cur)
    rows = []
    for ch in chains:
        src = ch['srcs'][0] + (' +%d' % (len(ch['srcs']) - 1))
        plan = ' | '.join(ch['plans'])
        ev = sorted(ch['ev'], key=lambda e: e[5])
        for node in sorted(set(e[1] for e in ev)):
            st_ = dict.fromkeys(STATE + ('cw', 'mode'))
            samples = []  # (t_s, 'x' or 'v', value, half width of the request/reply window in s)
            evs = []
            for e in ev:
                if e[1] != node:
                    continue
                t = e[5] / 1e6
                if e[0] == 'r':
                    tm = (e[5] + e[6]) / 2e6
                    if e[2] == '6064':
                        samples.append((tm, 'x', value(e), (e[6] - e[5]) / 2e6))
                    elif e[2] == '606c':
                        samples.append((tm, 'v', value(e), 0))
                    elif e[2] == '6061':
                        st_['mode'] = e[4]
                    elif e[2] in STATE and st_[e[2]] is None:
                        st_[e[2]] = value(e)
                elif e[0] == 'w':
                    if e[2] in STATE:
                        st_[e[2]] = value(e)
                    if e[2] == '60ff':
                        evs.append(('vchg', t, dict(st_)))
                    if e[2] == '6040':
                        prev = st_['cw']
                        st_['cw'] = e[4]
                        evs.append(('cw', t, dict(st_), prev))
            rows.append({'src': src, 'plan': plan, 'node': node, 'samples': samples, 'evs': evs})
    with open(out, 'w', encoding='utf-8', newline='\n') as fh:
        json.dump(rows, fh)
    print('node timelines:', len(rows))


if __name__ == '__main__':
    if len(sys.argv) != 3:
        sys.exit(__doc__)
    main(sys.argv[1], sys.argv[2])
