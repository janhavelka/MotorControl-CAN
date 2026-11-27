"""Step 3: cut each node timeline into motion segments and fit the cruise line of each.

Usage (from the repository root): python -I ramp.py ROWS_JSON RAMP_JSON

Segments start at a controlword or 60FF write:
  pstart  position start: bit 4 rises, not in mode 3, 607A known; V is 6081 signed by the
          direction of the displacement, so reverse moves count;
  vstart  velocity start: mode 3, word 000F (from 0007), 60FF nonzero;
  vchg    a 60FF write while running in mode 3;
  halt, quick, disable, disvolt: the stop words (bit 8; 000B-type quick stop; 0006/0007; bit 1 clear).
A segment ends at the next segment's write.

For a start or change, the cruise samples are the first unbroken run of 6064 samples whose
nearest 606C sample (within 25 ms) lies within [0.975 |V| - 3, 1.01 |V| + 3] rpm with the sign of
V. A least-squares line x = a + r t goes through them; leading samples are dropped while the
first residual exceeds 4 + 0.004 |r| counts. For a start from rest (606C = 0 at the last sample
before the start), D = (x0 - a) / r - t0 is the time from the start write t0 to the line's
crossing of the start position x0. A linear ramp from rest of duration T after a reaction L
gives D = L + T/2. For velocity starts x0 is the last sample before the enable words
(0006/0007 within 0.6 s before word 000F), not the one after them (see the README).

For a stop after a fitted segment, when the last two samples of the stop segment are equal and
606C reads 0, Dstop = (x_final - (a + r t0)) / r: the stop distance expressed as time at cruise
speed (an instant stop after a reaction L gives L; a linear ramp of Td gives L + Td/2).
"""
import bisect
import collections
import json
import re
import statistics as st
import sys


def fit(pts):
    n = len(pts)
    mt = sum(p[0] for p in pts) / n
    mx = sum(p[1] for p in pts) / n
    stt = sum((p[0] - mt) ** 2 for p in pts)
    if stt == 0:
        return None
    r = sum((p[0] - mt) * (p[1] - mx) for p in pts) / stt
    a = mx - r * mt
    return r, a, [p[1] - (a + r * p[0]) for p in pts]


def kind(cw, prev):
    if cw & 0x80:
        return 'faultreset'
    if (cw & 0x2) == 0:
        return 'disvolt'
    if (cw & 0x6) == 0x2:
        return 'quick'
    if (cw & 0xF) in (0x6, 0x7):
        return 'disable'
    if cw & 0x100:
        return 'halt-again' if (prev is not None and prev & 0x100) else 'halt'
    return 'run'


def segments(evs):
    segs = []  # (kind, t, V, 6083, 6084, 607A, controlword)
    for e in evs:
        t = e[1]
        s = e[2]
        mode = s['mode']
        if e[0] == 'cw':
            cw, prev = s['cw'], e[3]
            k = kind(cw, prev)
            if k == 'run' and s['607a'] is not None and mode != 3 and (cw & 0x10) and not ((prev or 0) & 0x10):
                segs.append(('pstart', t, s['6081'], s['6083'], s['6084'], s['607a'], cw))
            elif k == 'run' and mode == 3 and (cw & 0x10F) == 0x0F and prev is not None and prev != cw and s['60ff']:
                segs.append(('vstart', t, s['60ff'], s['6083'], s['6084'], None, cw))
            elif k in ('halt', 'quick', 'disable', 'disvolt'):
                segs.append((k, t, None, s['6083'], s['6084'], None, cw))
        elif e[0] == 'vchg' and s['cw'] is not None and (s['cw'] & 0x10F) == 0x0F and mode == 3:
            segs.append(('vchg', t, s['60ff'], s['6083'], s['6084'], None, s['cw']))
    return segs


def analyse(row, out):
    X = sorted([(t, v, h) for t, o, v, h in row['samples'] if o == 'x'])
    V = sorted([(t, v) for t, o, v, h in row['samples'] if o == 'v'])
    vt = [p[0] for p in V]

    def vnear(t):
        i = bisect.bisect_left(vt, t)
        c = [j for j in (i - 1, i) if 0 <= j < len(V) and abs(V[j][0] - t) < 0.025]
        return min((V[j] for j in c), key=lambda p: abs(p[0] - t)) if c else None

    evs = row['evs']
    segs = segments(evs)
    trial = ','.join(re.findall(r'trial=(\d+)', row['plan'] or '')[:3])
    line = None
    cur = None
    for j, sg in enumerate(segs):
        t0 = sg[1]
        te = segs[j + 1][1] if j + 1 < len(segs) else 1e18
        nxt = segs[j + 1][0] if j + 1 < len(segs) else None
        pre = [p for p in X if p[0] < t0]
        seg = [p for p in X if t0 <= p[0] < te]
        rec = {'src': row['src'], 'trial': trial, 'node': row['node'], 'kind': sg[0], 't0': t0,
               'up': sg[3], 'dn': sg[4], 'next': nxt}
        if sg[0] in ('pstart', 'vstart', 'vchg'):
            Vc = sg[2]
            if not Vc:
                line = None
                continue
            x0 = pre[-1][1] if pre else None
            if sg[0] == 'pstart' and sg[5] is not None:
                rec['target'] = sg[5]
                # 6081 is an unsigned speed; the direction is the displacement's: 607A itself for a
                # relative edge (bit 6), the target minus the start position for an absolute one.
                delta = sg[5] if sg[6] & 0x40 else (sg[5] - x0 if x0 is not None else 0)
                if delta < 0:
                    Vc = -Vc
            rec['V'] = Vc
            v0 = vnear(pre[-1][0]) if pre else None
            if sg[0] == 'vstart':
                cws = [e for e in evs if e[0] == 'cw' and t0 - 0.6 < e[1] < t0 and (e[2]['cw'] & 0xF) in (0x6, 0x7)]
                if cws:
                    pre2 = [p for p in X if p[0] < cws[0][1]]
                    if pre2:
                        x0 = pre2[-1][1]
                        v0 = vnear(pre2[-1][0])
                        rec['x0fix'] = 1
            rec['x0'] = x0
            rec['v0'] = v0[1] if v0 else None
            cr = []
            allp = []
            for p in seg:
                vv = vnear(p[0])
                allp.append((p[0], p[1], vv[1] if vv else None, vv[0] if vv else None))
                if vv and 0.975 * abs(Vc) - 3 <= abs(vv[1]) <= 1.01 * abs(Vc) + 3 and (vv[1] > 0) == (Vc > 0):
                    cr.append((p[0], p[1]))
            if cr:
                first = cr[0][0]
                run = []
                cs = set(cr)
                for q in allp:
                    if q[0] < first:
                        continue
                    if (q[0], q[1]) in cs:
                        run.append((q[0], q[1]))
                    else:
                        break
                cr = run
            rec['dt'] = st.median([b[0] - a[0] for a, b in zip(seg, seg[1:])]) if len(seg) > 1 else None
            rec['nseg'] = len(seg)
            f = fit(cr) if len(cr) >= 2 else None
            while f and len(cr) > 2 and abs(f[2][0]) > 4 + 0.004 * abs(f[0]):
                cr = cr[1:]
                f = fit(cr)
            if f:
                r, a, res = f
                rec['r'] = r
                rec['ratio'] = r / (Vc * 1000 / 60)
                rec['rms'] = (sum(x * x for x in res) / len(res)) ** 0.5
                rec['ncr'] = len(cr)
                rec['span'] = cr[-1][0] - cr[0][0]
                rec['firstcr'] = cr[0][0] - t0
                if sg[0] in ('pstart', 'vstart') and x0 is not None and v0 is not None and v0[1] == 0:
                    rec['D'] = (x0 - a) / r - t0
                if sg[0] == 'vchg' and line is not None:
                    r1, a1 = line
                    if abs(r1 - r) > 1:
                        rec['D12'] = ((a - a1) + (r - r1) * t0) / (r1 - r)
                    rec['V1'] = cur
                if sg[0] == 'pstart' and rec.get('target') is not None:
                    # Arrival bracket: the first sample at the target with 606C = 0 and the sample before it.
                    arr = [q for q in allp if q[1] == rec['target'] and q[2] == 0]
                    if arr:
                        rec['arrive_first'] = arr[0][0] - t0
                        before = [q for q in allp if q[0] < arr[0][0]]
                        rec['arrive_prev'] = before[-1][0] - t0 if before else None
                        rec['cruise_end_pred'] = (rec['target'] - a) / r - t0
                line = (r, a)
                cur = Vc
            else:
                line = None
            out.append(rec)
        else:
            if sg[0] == 'halt':
                rec['hasline'] = line is not None
                rec['V'] = cur
                if line is not None:
                    rec['xline'] = line[1] + line[0] * t0
                    rec['r'] = line[0]
                out.append(dict(rec, kind='haltdbg'))
            if line is not None and seg:
                vs = [vnear(p[0]) for p in seg]
                if len(seg) >= 2 and vs[-1] and vs[-1][1] == 0 and vs[-2] and vs[-2][1] == 0 and seg[-1][1] == seg[-2][1]:
                    r, a = line
                    rec['V'] = cur
                    rec['r'] = r
                    rec['Dstop'] = (seg[-1][1] - (a + r * t0)) / r
                    rec['xf'] = seg[-1][1]
                    rec['dt'] = st.median([b[0] - a_[0] for a_, b in zip(seg, seg[1:])])
                    rec['firststill'] = next((p[0] - t0 for p, v in zip(seg, vs) if v and v[1] == 0), None)
                    out.append(rec)
            line = None


def main(src, dst):
    with open(src, encoding='utf-8') as fh:
        rows = json.load(fh)
    out = []
    for row in rows:
        analyse(row, out)
    with open(dst, 'w', encoding='utf-8', newline='\n') as fh:
        json.dump(out, fh, indent=0)
    print(collections.Counter(o['kind'] for o in out))
    print('with D', sum(1 for o in out if 'D' in o), 'with D12', sum(1 for o in out if 'D12' in o),
          'with Dstop', sum(1 for o in out if 'Dstop' in o))


if __name__ == '__main__':
    if len(sys.argv) != 3:
        sys.exit(__doc__)
    main(sys.argv[1], sys.argv[2])
