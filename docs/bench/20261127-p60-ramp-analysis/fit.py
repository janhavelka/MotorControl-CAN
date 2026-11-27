"""Step 4: the ramp-law table (summary.json) and the per-run results (runs.csv).

Usage (from the repository root):
  python -I fit.py RECS_JSON ROWS_JSON RAMP_JSON BENCH_DIR OUT_DIR

Reads the outputs of parse.py, ana.py and ramp.py, and, for the three HALT runs, the later
record that holds the standstill position (under BENCH_DIR). Opens no device. Writes
OUT_DIR/summary.json and OUT_DIR/runs.csv with LF line endings.
"""
import csv
import io
import json
import math
import os
import re
import statistics as st
import sys

MIN_RPM = 20  # law fits use |V| >= 20 rpm: one count is then at most 3 ms of a cruise line
NOMINAL_RATE = 0.992  # night-soak rate factor (units re-audit section 3), used only to infer 2201
CRUISE = (0.975, 3, 1.01, 3)  # 606C window around the commanded rpm, as in ramp.py
HALT_CASES = (
    # trial, node timeline (ana.py src), node, later record holding the standstill 6064
    ('709', '20261105T1537Z-p3-node1-704/rx.txt +6', 1, '20261105T1541Z-rearm-halt-node1-710/rx.txt'),
    ('1507', '20261110T1037Z-s18-velocity-node1-1507/rx.txt +1', 1, '20261110T1037Z-s18-p1-node1-1508/rx.txt'),
    ('1613', '20261110T1043Z-s19-p3-node1-1604/rx.txt +5', 2, '20261110T1044Z-s19-p1-node2-1614/rx.txt'),
)
TRAFFIC = re.compile(r'^TRAFFIC k=0 p=\d+ id=([0-9a-f]+) ext=\d rtr=\d dlc=\d data=([0-9a-f]{16})')


def lin(pts):
    """Least-squares line y = a + b x; residual rms and largest error in y units."""
    n = len(pts)
    mx = sum(p[0] for p in pts) / n
    my = sum(p[1] for p in pts) / n
    b = sum((p[0] - mx) * (p[1] - my) for p in pts) / sum((p[0] - mx) ** 2 for p in pts)
    a = my - b * mx
    res = [p[1] - (a + b * p[0]) for p in pts]
    return {'n': n, 'intercept_ms': round(a, 2), 'slope': round(b, 5),
            'rms_ms': round(math.sqrt(sum(r * r for r in res) / n), 2), 'max_ms': round(max(map(abs, res)), 2)}


def det3(a):
    return (a[0][0] * (a[1][1] * a[2][2] - a[1][2] * a[2][1]) - a[0][1] * (a[1][0] * a[2][2] - a[1][2] * a[2][0])
            + a[0][2] * (a[1][0] * a[2][1] - a[1][1] * a[2][0]))


def start_velocity(runs):
    """Fit D = L + b T + c T/|V| by least squares (Cramer's rule on the normal equations).

    A linear ramp that starts at a start velocity Vs instead of rest gives D = L + T/2 - (Vs/2) T/V,
    so Vs = -2c. The standard error uses the residual variance and the (2, 2) element of the inverse
    normal matrix.
    """
    X = [(1.0, o['up'], o['up'] / abs(o['V'])) for o in runs]
    y = [o['D'] * 1000 for o in runs]
    m = [[sum(x[i] * x[j] for x in X) for j in range(3)] for i in range(3)]
    v = [sum(x[i] * yy for x, yy in zip(X, y)) for i in range(3)]
    d = det3(m)
    coef = [det3([[v[i] if j == k else m[i][j] for j in range(3)] for i in range(3)]) / d for k in range(3)]
    res = [yy - sum(c * xi for c, xi in zip(coef, x)) for x, yy in zip(X, y)]
    s2 = sum(r * r for r in res) / (len(y) - 3)
    inv22 = (m[0][0] * m[1][1] - m[0][1] * m[1][0]) / d
    return {'n': len(y), 'start_velocity_rpm': round(-2 * coef[2], 3),
            'standard_error_rpm': round(2 * math.sqrt(s2 * inv22), 3)}


def nominal(v_rpm, subdivision=1000):
    return math.copysign(math.floor(abs(v_rpm) * subdivision / 60), v_rpm)


def ratio1000(o):
    return o['r'] / nominal(o['V'])


def subdivision(o):
    """2201 is not in these timelines (separate setting records write it); infer it from the rate."""
    return 100 * round(ratio1000(o) * 1000 / (NOMINAL_RATE * 100))


def cruise(v_sample, v_cmd):
    lo = CRUISE[0] * abs(v_cmd) - CRUISE[1]
    hi = CRUISE[2] * abs(v_cmd) + CRUISE[3]
    return v_sample is not None and lo <= abs(v_sample) <= hi and (v_sample > 0) == (v_cmd > 0)


def timeline(row):
    """Position samples paired with the nearest 606C sample within 25 ms, as in ramp.py."""
    X = sorted((s[0], s[2], s[3]) for s in row['samples'] if s[1] == 'x')
    V = sorted((s[0], s[2]) for s in row['samples'] if s[1] == 'v')
    out = []
    for t, x, h in X:
        near = [v for v in V if abs(v[0] - t) < 0.025]
        out.append((t, x, h, min(near, key=lambda v: abs(v[0] - t))[1] if near else None))
    return out


def first_upload(path, node, index):
    """First expedited upload of index:00 from node in a capture: the raw 4-byte value, signed."""
    with open(path, encoding='utf-8', errors='replace') as fh:
        for line in fh:
            m = TRAFFIC.match(line)
            if not m or int(m[1], 16) != 0x580 + node:
                continue
            d = bytes.fromhex(m[2])
            if d[0] == 0x43 and int.from_bytes(d[1:3], 'little') == index and d[3] == 0:
                return int.from_bytes(d[4:8], 'little', signed=True)
    raise SystemExit('no %04X upload from node %d in %s' % (index, node, path))


def main(recs_path, rows_path, ramp_path, bench, out_dir):
    with open(recs_path, encoding='utf-8') as fh:
        recs = json.load(fh)
    with open(rows_path, encoding='utf-8') as fh:
        rows = {(r['src'], r['node']): r for r in json.load(fh)}
    with open(ramp_path, encoding='utf-8') as fh:
        segs = json.load(fh)
    table = []  # runs.csv rows
    summary = {'inputs': dict(recs['inputs'], records_with_motion_writes=len(recs['records']),
                              node_timelines=len(rows))}

    # Starts from rest.
    pos = [o for o in segs if o['kind'] == 'pstart' and 'D' in o and o['up'] is not None]
    vel = [o for o in segs if o['kind'] == 'vstart' and 'D' in o and o['up'] is not None]
    hi = [o for o in pos if abs(o['V']) >= MIN_RPM]
    unequal = [o for o in hi if o['up'] != o['dn']]
    law = lin([(o['up'], o['D'] * 1000) for o in hi])
    low = [o for o in pos if abs(o['V']) < MIN_RPM]
    summary['position_start'] = {
        'runs': len(pos), 'nodes': sorted({o['node'] for o in pos}),
        'rpm': [min(abs(o['V']) for o in pos), max(abs(o['V']) for o in pos)],
        'ramp_6083_ms': [min(o['up'] for o in pos), max(o['up'] for o in pos)],
        'law_6083_at_or_above_20_rpm': law,
        'below_20_rpm': {'runs': len(low), 'max_ms_from_law': round(max(
            abs(o['D'] * 1000 - law['intercept_ms'] - law['slope'] * o['up']) for o in low), 1)},
        'unequal_ramps_at_or_above_20_rpm': {
            'runs': len(unequal), 'fit_to_6083': lin([(o['up'], o['D'] * 1000) for o in unequal]),
            'fit_to_6084': lin([(o['dn'], o['D'] * 1000) for o in unequal])},
        'fixed_rate_law_D_vs_T_times_V_over_2000': lin([(o['up'] * abs(o['V']) / 2000, o['D'] * 1000) for o in hi]),
        'start_velocity_fit': start_velocity(hi),
        'ramp_duration_over_T': round(2 * law['slope'], 4)}
    for o in pos:
        o['fit'] = 'position' if abs(o['V']) >= MIN_RPM else 'position-low'
    vfast = [o for o in vel if abs(o['V']) >= 3001]
    vhi = [o for o in vel if abs(o['V']) >= MIN_RPM]
    vlaw = lin([(o['up'], o['D'] * 1000) for o in vhi])
    vfast_law = lin([(o['up'], o['D'] * 1000) for o in vfast])
    summary['velocity_start'] = {
        'runs': len(vel), 'nodes': sorted({o['node'] for o in vel}),
        'rpm': [min(abs(o['V']) for o in vel), max(abs(o['V']) for o in vel)],
        'negative_runs': sum(1 for o in vel if o['V'] < 0),
        'ramp_ms': sorted({o['up'] for o in vel}),
        'runs_with_6083_not_6084': sum(1 for o in vel if o['up'] != o['dn']),
        'subdivision_inferred': sorted({subdivision(o) for o in vel}),
        'law_all_runs': lin([(o['up'], o['D'] * 1000) for o in vel]),
        'law_at_or_above_20_rpm': vlaw,
        'law_at_or_above_3001_rpm': vfast_law,
        'ramp_duration_over_T_at_or_above_20_rpm': round(2 * vlaw['slope'], 4),
        'ramp_duration_over_T_at_or_above_3001_rpm': round(2 * vfast_law['slope'], 4)}
    for o in vel:
        o['fit'] = 'velocity' if abs(o['V']) >= MIN_RPM else 'velocity-low'

    # Reported count rate against floor(rpm x 2201 / 60), 2201 inferred from the rate itself.
    # Position fits need a cruise of at least 2000 counts: shorter ones resolve the rate to worse than 0.1 %.
    def rates(runs):
        q = [o['r'] / nominal(o['V'], subdivision(o)) for o in runs]
        return {'runs': len(q), 'rpm': [min(abs(o['V']) for o in runs), max(abs(o['V']) for o in runs)],
                'median': round(st.median(q), 5), 'range': [round(min(q), 5), round(max(q), 5)]}
    long_pos = [o for o in hi if o['span'] * abs(o['r']) >= 2000]
    summary['count_rate'] = {
        'velocity_at_1000': rates([o for o in vhi if subdivision(o) == 1000]),
        'velocity_other_subdivisions': rates([o for o in vhi if subdivision(o) != 1000]),
        'position_at_1000_cruise_2000_counts': rates([o for o in long_pos if subdivision(o) == 1000])}
    vq = [o['r'] / nominal(o['V'], subdivision(o)) for o in vhi]
    rate = st.median(vq)
    spread = (max(vq) - min(vq)) / rate  # relative rate uncertainty used for extrapolated cruise lines

    # Changes between two nonzero speeds: two cruise lines at the model rate, crossing time D12.
    changes = []
    for o in (s for s in segs if s['kind'] == 'vchg'):
        row = rows[(o['src'], o['node'])]
        evs = row['evs']
        i = next(j for j, e in enumerate(evs) if e[0] == 'vchg' and e[1] == o['t0'])
        v1, v2, T = evs[i - 1][2]['60ff'], o['V'], o['up']
        t_next = next((e[1] for e in evs[i + 1:]), math.inf)
        pts = timeline(row)
        a1 = [p for p in pts if p[0] < o['t0'] and cruise(p[3], v1)]
        a2 = [p for p in pts if o['t0'] + T / 1000 < p[0] < t_next and cruise(p[3], v2)]
        if not a1 or not a2:
            continue
        (t1, x1, h1, _), (t2, x2, h2, _) = a1[-1], a2[-1]
        r1, r2 = rate * nominal(v1), rate * nominal(v2)
        d12 = (x2 - x1 - r2 * (t2 - o['t0']) + r1 * (t1 - o['t0'])) / (r1 - r2)
        # One count at each anchor, the SDO timing window and the rate spread over the extrapolation.
        res = (2 + abs(r1) * h1 + abs(r2) * h2 + spread * (abs(r1) * (o['t0'] - t1) + abs(r2) * (t2 - o['t0']))) / abs(r1 - r2)
        guide = abs(v2 - v1) * T / abs(v2)  # the guide's old formula: a = V_commanded x 1000 / T
        changes.append({'run': o['src'], 'node': o['node'], 'rpm': [v1, v2], 'ramp_ms': [T, o['dn']],
                        'D12_ms': round(d12 * 1000, 1), 'resolution_ms': round(res * 1000, 1),
                        'D12_minus_T_over_2_ms': round(d12 * 1000 - T / 2, 1),
                        'D12_minus_old_formula_time_over_2_ms': round(d12 * 1000 - guide / 2, 1)})
        table.append(dict(o, fit='change', V1=v1, D12=d12, r=r2))
    summary['speed_change'] = changes

    # HALT stops: stop distance from the last cruise sample at the model rate to the later standstill.
    halts = []
    for trial, src, node, still in HALT_CASES:
        h = [o for o in segs if o['kind'] == 'haltdbg' and o['src'] == src and o['node'] == node]
        if len(h) != 1:
            raise SystemExit('expected one HALT in %s node %d, found %d' % (src, node, len(h)))
        h = h[0]
        row = rows[(src, node)]
        state = next(e[2] for e in row['evs'] if e[0] == 'cw' and e[1] == h['t0'])
        v = state['60ff'] if state['mode'] == 3 else state['6081']
        t, x, _, vs = [p for p in timeline(row) if p[0] < h['t0'] and cruise(p[3], math.copysign(v, p[3] or 1))][-1]
        r = rate * nominal(math.copysign(v, vs))
        x_still = first_upload(os.path.join(bench, still), node, 0x6064)
        dstop = (x_still - (x + r * (h['t0'] - t))) / r
        halts.append({'trial': trial, 'node': node, 'mode': state['mode'], 'rpm': v, 'ramp_6084_ms': h['dn'],
                      'ramp_6083_ms': h['up'], 'standstill_record': still, 'standstill_6064': x_still,
                      'Dstop_ms': round(dstop * 1000, 1), 'Dstop_minus_T_over_2_ms': round(dstop * 1000 - h['dn'] / 2, 1)})
        table.append(dict(h, fit='halt', V=math.copysign(v, vs), r=r, Dstop=dstop))
    summary['halt'] = halts

    # Position arrivals: the polls bracket the end of the deceleration only coarsely.
    arrivals = [o for o in pos if 'arrive_first' in o and o.get('arrive_prev') is not None]
    brackets = [o['arrive_first'] - o['arrive_prev'] for o in arrivals]
    summary['position_arrival'] = {
        'runs': len(arrivals), 'median_bracket_s': round(st.median(brackets), 3),
        'bracket_s': [round(min(brackets), 3), round(max(brackets), 3)],
        # A linear ramp-down lasting 6084 ends 6084/2 after the cruise line reaches the target.
        'brackets_containing_cruise_end_plus_6084_over_2': sum(
            1 for o in arrivals if o['arrive_prev'] <= o['cruise_end_pred'] + o['dn'] / 2000 <= o['arrive_first']),
        'brackets_containing_cruise_end_plus_6083_over_2': sum(
            1 for o in arrivals if o['arrive_prev'] <= o['cruise_end_pred'] + o['up'] / 2000 <= o['arrive_first']),
        'brackets_containing_cruise_end_instant_stop': sum(
            1 for o in arrivals if o['arrive_prev'] <= o['cruise_end_pred'] <= o['arrive_first']),
        'runs_with_6083_not_6084': sum(1 for o in arrivals if o['up'] != o['dn'])}

    # Quick stops: only those whose last sample before the stop word was still at cruise speed.
    quick = []
    for o in (s for s in segs if s['kind'] == 'quick' and 'Dstop' in s):
        before = [p for p in timeline(rows[(o['src'], o['node'])]) if p[0] < o['t0']]
        o['fit'] = 'quick' if before and cruise(before[-1][3], o['V']) else 'quick-not-cruising'
        quick.append(o)
    dq = sorted(o['Dstop'] * 1000 for o in quick if o['fit'] == 'quick')
    by_ramp = {}
    modes = {}
    for o in quick:
        if o['fit'] == 'quick':
            by_ramp.setdefault(o['dn'], []).append(o['Dstop'] * 1000)
            mode = next(e[2]['mode'] for e in rows[(o['src'], o['node'])]['evs'] if e[0] == 'cw' and e[1] == o['t0'])
            modes[str(mode)] = modes.get(str(mode), 0) + 1
    summary['quick_stop'] = {
        'runs': len(dq), 'excluded_not_cruising': len(quick) - len(dq), 'runs_by_mode': dict(sorted(modes.items())),
        'nodes': sorted({o['node'] for o in quick if o['fit'] == 'quick'}),
        'rpm': [min(abs(o['V']) for o in quick if o['fit'] == 'quick'),
                max(abs(o['V']) for o in quick if o['fit'] == 'quick')],
        'Dstop_ms_p5_p25_median_p75_p95': [round(dq[int(q * (len(dq) - 1))], 1) for q in (0.05, 0.25, 0.5, 0.75, 0.95)],
        'median_ms_by_6084': {str(k): [len(v), round(st.median(v), 1)] for k, v in sorted(by_ramp.items())}}
    summary['homing_ramp_writes_609A_6099'] = sum(1 for r in recs['records'] for e in r['ev']
                                                  if e[0] == 'w' and e[2] in ('609a', '6099'))

    os.makedirs(out_dir, exist_ok=True)
    with open(os.path.join(out_dir, 'summary.json'), 'w', encoding='utf-8', newline='\n') as fh:
        fh.write(json.dumps(summary, indent=1) + '\n')
    cols = ('fit', 'kind', 'source', 'node', 'trial', 't0_s', 'rpm', 'rpm_before', 'ramp_6083_ms', 'ramp_6084_ms',
            'subdivision_inferred', 'cruise_points', 'cruise_s', 'poll_s', 'rate_counts_per_s', 'rate_ratio', 'D_ms',
            'D12_ms', 'Dstop_ms')
    buf = io.StringIO()
    w = csv.writer(buf, lineterminator='\n')
    w.writerow(cols)
    for o in sorted(pos + vel + quick + table, key=lambda o: (o['fit'], o['src'], o['node'], o['t0'])):
        def f(key, scale=1, nd=2):
            return '' if o.get(key) is None else '%.*f' % (nd, o[key] * scale)
        k = subdivision(o) if o.get('r') and o.get('V') else None
        fitted_line = o['fit'].startswith(('position', 'velocity'))
        w.writerow((o['fit'], o['kind'], o['src'], o['node'], o['trial'], '%.4f' % o['t0'], o.get('V', ''),
                    o.get('V1', ''), o['up'], o['dn'], k or '', o.get('ncr', ''), f('span', 1, 3), f('dt', 1, 3),
                    f('r'), '%.5f' % (o['r'] / nominal(o['V'], k)) if fitted_line else '',
                    f('D', 1000), f('D12', 1000), f('Dstop', 1000)))
    with open(os.path.join(out_dir, 'runs.csv'), 'w', encoding='utf-8', newline='') as fh:
        fh.write(buf.getvalue())
    print(json.dumps(summary, indent=1))


if __name__ == '__main__':
    if len(sys.argv) != 6:
        sys.exit(__doc__)
    main(*sys.argv[1:])
