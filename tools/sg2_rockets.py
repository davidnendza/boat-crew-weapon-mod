"""Second batch of rocket side-grades (2026-11-15): two Katyushas, the 4.5-inch Beach Barrage Rocket rack, the Japanese
20 cm naval rocket and the Hedgerow. Same method and helpers as make_sidegrade_models.py.

The X50 is the Mk 50's 4-rail sister on the same mount and pivot. The Mk 20 Mousetrap has no stand and no barrels: its
whole mount mesh hangs under 'weapon' (the pivot, at the prefab root), and its four shoot points sit on a 'Rack' node
outside the pivot, tilted 45 degrees up, at x -0.32..0.32, y 0.73, z 0.59. The Hedgerow keeps nothing of it.

Usage: python sg2_rockets.py <models folder> [--preview <folder>] [--only name,name] [--closeups]
       (needs the game installed and UnityPy; run python WITHOUT -I)
"""
import math
import pathlib
import sys

import numpy as np

import make_turret_models as tm
from make_sidegrade_models import *                     # noqa: F401,F403 (Model, paints, primitives, closeup_sheet)
from make_sidegrade_models import Model, closeup_sheet, pal, RL_DARK, RL_LIGHT, ROCKET, DARK

# the Mousetrap's own materials: its LambertColor5 texture is _Color1 (dark grey) almost everywhere; the rockets' plain green
MT = pal(('Mousetrap Mk20', 'Dark MG Metal'), (0.25, 0.25, 0.25), (0.1, 0.1))
MT_ROCKET = tm.Paint(('Light Green Rocket', 'Gray'), (0.46, 0.51, 0.42), tm.box_uv(1.0))

tm.BASES.update({
    'x50': dict(prefab='X50 Launcher', pivot='weapon', barrels=[], keep=['Mount']),
    'mousetrap': dict(prefab='Mk 20 Mousetrap', pivot='weapon', barrels=[], keep=[]),
})


def _finned(p, c, r, body, nose, paint_body, paint_head, fin_len, fin_span, n_fins=4, seg=10):
    """A rocket along +Z from c (its tail): body, ogive head, n_fins plain fins at the tail."""
    c = np.asarray(c, float)
    lathe(p, paint_body, c, (0, 0, 1), [(0, 0.0), (0, r * 0.7), (0.02, r), (body, r)], seg=seg, caps=False)
    lathe(p, paint_head, c + (0, 0, body), (0, 0, 1), [(0, r * 1.02), (nose * 0.35, r * 1.02), (nose * 0.75, r * 0.6), (nose, 0.0)], seg=seg)
    for k in range(n_fins):
        a = k * 2 * math.pi / n_fins + math.pi / 4
        box(p, paint_body, c + ((r + fin_span / 2) * math.cos(a), (r + fin_span / 2) * math.sin(a), fin_len / 2),
            (0.005, fin_span, fin_len), (0, 0, math.degrees(a) - 90))


def _ibeam(p, paint, c, length, h, w, t=0.006):
    """An I-beam rail along +Z centred on c: web plus top and bottom flanges."""
    x, y, z = c
    box(p, paint, (x, y, z), (t, h, length))
    box(p, paint, (x, y + h / 2, z), (w, t, length))
    box(p, paint, (x, y - h / 2, z), (w, t, length))


def _cradle(p, x0, x1, ys, zs, paint=RL_DARK, arm_y=0.06):
    """An open frame: cross bars under each tier at each z, end posts and side rails at x0/x1, a short trunnion plate,
    and the trunnion out to the Mk 50 arm (x -0.75). Open sides, so every rail and rocket shows."""
    yc, hy, zc, lz = (ys[0] + ys[1]) / 2, ys[1] - ys[0], (zs[0] + zs[-1]) / 2, zs[-1] - zs[0]
    for x in (x0, x1):
        for z in zs:
            box(p, paint, (x, yc, z), (0.025, hy, 0.04))
        for y in ys:
            box(p, paint, (x, y, zc), (0.025, 0.025, lz + 0.04))
    box(p, paint, (x0, yc, 0.0), (0.014, hy, 0.3))
    for z in zs:
        for y in ys:
            box(p, paint, ((x0 + x1) / 2, y, z), (x1 - x0, 0.025, 0.04))
    cyl(p, paint, (-0.76, arm_y, 0.0), (x0, arm_y, 0.0), 0.05, seg=10)


def _katyusha_fittings(t, rail_xs, ys, rail_h, r, z_tail, z_rear, x_end, arm_y=0.06):
    """What the real naval Katyusha mounts had beside the rails: two guide studs under each rocket (the M-8/M-13 ride the
    rail on studs, not on the body), a stop/contact clip at the rear of every rail, the elevating handwheel on a trunnion
    stub on the far side (on the elevation axis, so it only spins) and a plain collimator sight on the arm side.
    No armour: none is documented for these mounts."""
    for y in ys:
        for x in rail_xs:
            top = y + rail_h / 2
            for dz in (0.12, 0.45):
                box(t, RL_DARK, (x, top + 0.006, z_tail + dz * (r / 0.042) ** 0.5), (0.012, 0.012, 0.025))   # guide stud
            box(t, RL_LIGHT, (x, top + 0.012, z_rear), (0.022, 0.02, 0.025))                         # contact clip
    cyl(t, RL_DARK, (x_end, arm_y, 0.0), (x_end + 0.07, arm_y, 0.0), 0.022, seg=8)
    handwheel(t, RL_DARK, (x_end + 0.07, arm_y, 0.0), (1, 0, 0), 0.07)
    x0 = rail_xs[0] - 0.05
    box(t, RL_DARK, (x0 - 0.04, ys[1] + 0.04, -0.1), (0.06, 0.012, 0.03))                            # sight bracket
    box(t, RL_DARK, (x0 - 0.07, ys[1] + 0.075, -0.1), (0.04, 0.06, 0.08))                            # collimator body
    cyl(t, RL_DARK, (x0 - 0.07, ys[1] + 0.085, -0.15), (x0 - 0.07, ys[1] + 0.085, -0.17), 0.012, seg=8)  # eyepiece


def katyusha_m8():
    """16-M-8 naval launcher: sixteen short open I-beam rails in two tiers of eight, an 82 mm M-8 on top of each, on the
    X50's arm."""
    m = Model('katyusha_m8', 'x50', item='16-M-8 Katyusha')
    t = m.turret()
    r, L = 0.042, 0.95
    xs = [-0.6 + 0.084 * i for i in range(8)]
    for y in (-0.12, 0.13):
        for x in xs:
            _ibeam(t, RL_DARK, (x, y, 0.1), L, 0.035, 0.03)
            _finned(t, (x, y + 0.0175 + r + 0.004, -0.3), r, 0.5, 0.13, ROCKET, RL_DARK, 0.1, 0.03, seg=8)
    _cradle(t, xs[0] - 0.05, xs[-1] + 0.05, (-0.17, 0.08), (-0.3, 0.1, 0.5))
    _katyusha_fittings(t, xs, (-0.12, 0.13), 0.035, r, -0.3, -0.36, xs[-1] + 0.05 + 0.0125)
    box(t, RL_LIGHT, (-0.3, -0.21, -0.25), (0.2, 0.06, 0.1))                                     # firing box
    return m


def katyusha_m13():
    """M-13 rail launcher: six long open I-beam rails in two tiers of three, a 132 mm M-13 on each, on the Mk 50's arm."""
    m = Model('katyusha_m13', 'mk50', item='M-13 Katyusha')
    t = m.turret()
    r = 0.068
    xs = (-0.5, -0.3, -0.1)
    for y in (-0.16, 0.17):
        for x in xs:
            _ibeam(t, RL_DARK, (x, y, 0.15), 1.7, 0.05, 0.05, 0.008)
            _finned(t, (x, y + 0.025 + r + 0.006, -0.55), r, 1.1, 0.26, ROCKET, RL_DARK, 0.18, 0.05, seg=10)
    _cradle(t, -0.63, 0.03, (-0.24, 0.1), (-0.6, 0.15, 0.9))
    _katyusha_fittings(t, xs, (-0.16, 0.17), 0.05, r, -0.55, -0.68, 0.03 + 0.0125)
    box(t, RL_LIGHT, (-0.3, -0.24 - 0.0125 - 0.03, -0.6), (0.22, 0.06, 0.12))                  # firing box under the rear bar
    return m


def bbr_mk7():
    """4.5-inch Beach Barrage Rocket on a Mk 7 rack: twelve finned rockets on open rails (two rods each), three rows
    of four, in a box frame on the Mk 50's arm."""
    m = Model('bbr_mk7', 'mk50', item='4.5-inch BBR Mk 7')
    t = m.turret()
    r = 0.058
    xs = [-0.55 + 0.165 * i for i in range(4)]
    ys = (-0.16, 0.02, 0.2)
    for y in ys:
        for x in xs:
            for dx in (-0.03, 0.03):
                cyl(t, RL_DARK, (x + dx, y - r - 0.008, -0.4), (x + dx, y - r - 0.008, 0.55), 0.008, seg=5, caps=False)
            _finned(t, (x, y, -0.38), r, 0.55, 0.2, ROCKET, RL_DARK, 0.12, 0.04, seg=10)
            box(t, RL_DARK, (x, y - r - 0.004, 0.0), (0.075, 0.008, 0.02))                        # detent spring holding the rocket
            box(t, RL_LIGHT, (x, y - r + 0.002, -0.385), (0.03, 0.02, 0.02))                        # contact arm at the tail
    x0, x1, y0, y1 = xs[0] - 0.1, xs[-1] + 0.1, ys[0] - 0.09, ys[-1] + 0.09
    for z in (-0.4, 0.55):                                                                        # end frames
        for y in ys:
            box(t, RL_DARK, ((x0 + x1) / 2, y - r - 0.008, z), (x1 - x0, 0.02, 0.03))
        box(t, RL_DARK, ((x0 + x1) / 2, y1, z), (x1 - x0, 0.025, 0.04))
        for x in (x0, x1):
            box(t, RL_DARK, (x, (y0 + y1) / 2, z), (0.025, y1 - y0, 0.04))
    for x in (x0, x1):
        for y in (y0, y1):
            box(t, RL_DARK, (x, y, 0.075), (0.025, 0.025, 0.95))
    cyl(t, RL_DARK, (-0.76, 0.06, 0.0), (x0, 0.06, 0.0), 0.05, seg=10)
    box(t, RL_LIGHT, (-0.3, y1 + 0.0375, -0.4), (0.2, 0.05, 0.1))                                # firing box on the rear top bar
    tube(t, RL_DARK, [(-0.3, y1 + 0.03, -0.45), (x1 - 0.01, y1 - 0.01, -0.44), (x1 - 0.01, y0 + 0.02, -0.42)], 0.008, seg=5)  # firing cable
    return m


def naval_rocket_20cm():
    """IJN 20 cm naval rocket: one fat finless, spin-stabilised rocket lying in an open trough on the Mk 50's arm."""
    m = Model('naval_rocket_20cm', 'mk50', item='20 cm naval rocket')
    t = m.turret()
    x, y, r = -0.3, 0.02, 0.11
    # the trough: a floor and two flared walls, with three hoops underneath
    box(t, RL_DARK, (x, y - r - 0.01, 0.1), (0.16, 0.014, 1.5))
    for s in (-1, 1):
        box(t, RL_DARK, (x + s * (r + 0.02), y - 0.04, 0.1), (0.014, 0.16, 1.5), (0, 0, s * 20))
    for z in (-0.5, 0.1, 0.7):
        box(t, RL_DARK, (x, y - r - 0.04, z), (0.32, 0.04, 0.05))
        box(t, RL_DARK, (x + 0.15, y - 0.06, z), (0.025, 0.12, 0.05))
        box(t, RL_DARK, (x - 0.15, y - 0.06, z), (0.025, 0.12, 0.05))
    # the rocket: nozzle ring at the base, plain body, long blunt head
    c = np.array([x, y + 0.005, -0.55])
    lathe(t, RL_DARK, c, (0, 0, 1), [(0, 0.0), (0, r * 0.75), (0.04, r * 0.85), (0.06, r)], seg=14)
    for k in range(6):                                                                            # angled spin nozzles
        a = k * math.pi / 3
        box(t, RL_DARK, c + (r * 0.55 * math.cos(a), r * 0.55 * math.sin(a), -0.005), (0.03, 0.03, 0.02), (20, 0, math.degrees(a)))
    lathe(t, ROCKET, c + (0, 0, 0.06), (0, 0, 1), [(0, r), (0.75, r)], seg=14, caps=False)
    box_band = c + (0, 0, 0.42)
    lathe(t, RL_LIGHT, box_band, (0, 0, 1), [(0, r * 1.01), (0.03, r * 1.01)], seg=14, caps=False)  # rotating band
    lathe(t, ROCKET, c + (0, 0, 0.81), (0, 0, 1), [(0, r), (0.12, r * 0.95), (0.3, r * 0.6), (0.4, r * 0.15), (0.41, 0.0)], seg=14)
    cyl(t, RL_DARK, (-0.76, 0.06, 0.0), (x - 0.15, 0.06, 0.0), 0.05, seg=10)
    box(t, RL_LIGHT, (x + 0.2, y - 0.1, -0.45), (0.12, 0.06, 0.1))
    box(t, RL_DARK, (x - r - 0.06, y + 0.05, -0.3), (0.02, 0.04, 0.03))                           # sight bracket on the trough wall
    box(t, RL_DARK, (x - r - 0.06, y + 0.085, -0.3), (0.03, 0.03, 0.1))                           # open sight
    return m


def hedgerow():
    """Hedgerow: a Hedgehog bed of 24 spigots in four rows of six, each with a bomb on it, tilted 45 degrees up like the
    Mousetrap's rack (its shoot points sit among the front bombs), on two side frames and a deck frame. Painted in the
    Mousetrap's own grey and rocket green."""
    m = Model('hedgerow', 'mousetrap', item='Hedgerow')
    t = m.turret()
    up = math.radians(45)
    d = np.array([0.0, math.sin(up), math.cos(up)])                                          # spigot axis
    n = np.array([0.0, math.cos(up), -math.sin(up)])                                         # up the bed
    o = np.array([0.0, 0.42, 0.3])                                                           # centre of the bed plate
    cols = [-0.5 + 0.2 * i for i in range(6)]
    rows = [-0.3 + 0.2 * j for j in range(4)]
    box(t, MT, o - d * 0.02, (1.2, 0.84, 0.03), (-45, 0, 0))                                   # bed plate, facing d
    # side frames: deck to the underside of the bed
    lo, hi = o + n * -0.42 - d * 0.035, o + n * 0.42 - d * 0.035
    outline = [(-0.1, 0.0), (lo[2] + 0.05, 0.0), (lo[2] + 0.05, lo[1] - 0.05), (lo[2], lo[1]), (hi[2], hi[1]), (-0.1, hi[1] - 0.04)]
    for x in (-0.61, 0.59):
        extrude(t, MT, outline, (0, 0, 1), (0, 1, 0), (1, 0, 0), x, x + 0.02)
    box(t, MT, (0, 0.015, (lo[2] - 0.05) / 2), (1.26, 0.03, lo[2] + 0.2))                        # deck frame
    for z in (-0.08, lo[2]):
        box(t, MT, (0, 0.05, z), (1.22, 0.06, 0.05))
    box(t, MT, (0.64, 0.25, -0.02), (0.06, 0.14, 0.1))                                           # firing junction box on the side frame
    for x in cols:
        for y in rows:
            base = o + np.array([x, 0, 0]) + n * y
            cyl(t, DARK, base, base + d * 0.3, 0.018, seg=6)                                     # spigot
            tail = base + d * 0.06
            cyl(t, MT_ROCKET, tail, tail + d * 0.26, 0.03, seg=8, caps=False)                    # tail tube over the spigot
            lathe(t, MT_ROCKET, tail, d, [(0, 0.062), (0.09, 0.062)], seg=10, caps=False)          # tail drum
            lathe(t, MT_ROCKET, tail + d * 0.26, d, [(0, 0.0), (0, 0.075), (0.02, 0.085), (0.24, 0.085), (0.3, 0.06), (0.32, 0.0)], seg=10)
            nose = tail + d * 0.58
            cyl(t, DARK, nose - d * 0.01, nose + d * 0.16, 0.007, seg=5)                          # fuze extension rod (bursts above the beach)
            cyl(t, DARK, nose + d * 0.16, nose + d * 0.175, 0.016, seg=6)                         # striker head
    return m



MODELS = [katyusha_m8, katyusha_m13, bbr_mk7, naval_rocket_20cm, hedgerow]


def main():
    out = pathlib.Path(sys.argv[1])
    prev = pathlib.Path(sys.argv[sys.argv.index('--preview') + 1]) if '--preview' in sys.argv else None
    only = sys.argv[sys.argv.index('--only') + 1].split(',') if '--only' in sys.argv else None
    built = []
    for make in MODELS:
        if only and make.__name__ not in only:
            continue
        m = make()
        n = m.write(out)
        print('%-20s %5d triangles  keep=%s' % (m.name, n, ','.join(m.keep)))
        built.append(m)
    if prev:
        prev.mkdir(parents=True, exist_ok=True)
        for i in range(0, len(built), 3):
            tm.detail_sheet(built[i:i + 3], prev / ('sg2_rockets_models_%d.png' % (i // 3 + 1)))
            if '--closeups' in sys.argv:
                closeup_sheet(built[i:i + 3], prev / ('sg2_rockets_closeups_%d.png' % (i // 3 + 1)))


if __name__ == '__main__':
    main()
