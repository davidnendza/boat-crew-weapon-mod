"""Second batch of Utility side-grades (2026-11-15): Holman Projector, 7.5 cm Pak 40, 45 mm 21-K, Type 1 47 mm, Blacker
Bombard and the Sturmtiger's 38 cm RW 61. Same method as make_sidegrade_models.py (Model, paints, helpers come from it);
the bases (mortar60, at37, how75) are already in tm.BASES there. Items and templates: sg2_utility_items.txt.

Usage: python sg2_utility.py <models folder> [--preview <folder>] [--only name,name] [--closeups]
       (needs the game installed and UnityPy; run python WITHOUT -I, from tools/)
"""
import math
import pathlib
import sys

import numpy as np

from make_sidegrade_models import *                     # noqa: F401,F403  Model, paints, primitives, helpers
from make_sidegrade_models import closeup_sheet
import make_turret_models as tm


def _clip(poly, axis, val, keep_greater):
    """Sutherland-Hodgman: the part of a 2D polygon on one side of the line u[axis] = val."""
    out = []
    for i in range(len(poly)):
        a, b = poly[i - 1], poly[i]
        ina, inb = (a[axis] >= val) == keep_greater, (b[axis] >= val) == keep_greater
        if ina != inb:
            f = (val - a[axis]) / (b[axis] - a[axis])
            out.append(tuple(a[k] + f * (b[k] - a[k]) for k in range(2)))
        if inb:
            out.append(tuple(b))
    return out


def slotted_shield(p, paint, outline, slot, z, lean=0.0, thick=0.012, yaw=0.0, pivot=(0.0, 0.0)):
    """A flat shield plate (outline in x, y) with a rectangular slot (x0, x1, y0, y1) for the barrel, since extrude cannot
    cut holes: the plate is cut into the parts left, right, above and below the slot. lean tips it back (degrees about x),
    yaw turns it about the vertical through pivot (x, y) (positive: the +x edge goes back)."""
    x0, x1, y0, y1 = slot
    mid = _clip(_clip(outline, 0, x0, True), 0, x1, False)
    pieces = [_clip(outline, 0, x0, False), _clip(outline, 0, x1, True), _clip(mid, 1, y0, False), _clip(mid, 1, y1, True)]
    R = tm.rot(lean, yaw, 0)
    px, py = pivot
    for q in pieces:
        if len(q) >= 3:
            q = [(u - px, v - py) for u, v in q]
            extrude(p, paint, q, R @ (1, 0, 0), R @ (0, 1, 0), R @ (0, 0, 1), -thick / 2, thick / 2, origin=(px, py, z))


def angled_shield(p, paint, outline, slot, z, lean, yaw, pivot, thick=0.012):
    """Two flat halves meeting at x = pivot[0], each turned back by yaw degrees: the shallow V seen from above."""
    for keep, sy in ((False, -1), (True, 1)):
        slotted_shield(p, paint, _clip(outline, 0, pivot[0], keep), slot, z, lean, thick, sy * yaw, pivot)


def _tin(p, c, r=0.034, h=0.1):
    """The Holman round: an open-topped tin with a Mills bomb in it, the bomb's top and fuze showing."""
    c = np.asarray(c, float)
    lathe(p, AT_GREY, c, (0, 1, 0), [(0, 0.0), (0, r), (h, r), (h, r * 0.9), (h * 0.2, r * 0.9)], seg=10, caps=False)
    lathe(p, OLIVE, c + (0, h * 0.55, 0), (0, 1, 0), [(0, r * 0.88), (h * 0.35, r * 0.8), (h * 0.55, r * 0.45), (h * 0.6, 0.0)], seg=8)
    cyl(p, DARK, c + (0, h * 1.12, 0), c + (0, h * 1.25, 0), r * 0.25, seg=6)


# ---- on the 60 mm mortar (the whole stock mortar is hidden; deck at y -0.72 in its pivot frame) -------------------------
def _mills(p, c, r=0.028):
    """A Mills bomb standing on its base: the segmented egg, the fuze cap and the lever down one side."""
    c = np.asarray(c, float)
    lathe(p, OLIVE, c, (0, 1, 0), [(0, 0.0), (0, r * 0.6), (r * 0.4, r * 0.95), (r * 1.1, r), (r * 1.8, r * 0.85), (r * 2.3, r * 0.45),
                                   (r * 2.3, 0.0)], seg=8)
    ring(p, OLIVE, c + (0, r * 1.15, 0), (0, 1, 0), r * 1.0, r * 0.12, n=8, seg=3)
    cyl(p, DARK, c + (0, r * 2.3, 0), c + (0, r * 2.7, 0), r * 0.3, seg=6)
    box(p, DARK, c + (r * 0.95, r * 1.5, 0), (r * 0.15, r * 1.9, r * 0.5))


def holman():
    """Holman Projector: a plain launching tube on a pedestal, cocked up for a high throw, the firing valve and trigger at
    its breech, the air line down to two bottles on the deck, and an open box of Mills bombs at the loader's hand."""
    m = Model('holman_projector', 'mortar60', item='Holman Projector')
    t = m.turret()
    deck = -0.72
    d = np.array([0.0, 0.8, 0.6]); d /= np.linalg.norm(d)
    T = np.array([0.0, -0.04, -0.2])                                                    # trunnion
    # pedestal: base flange bolted to the deck, the column, the fork round the tube
    lathe(t, AT_GREY, (0, deck, -0.2), (0, 1, 0), [(0, 0.0), (0, 0.2), (0.03, 0.2), (0.05, 0.1), (0.05, 0.07)], seg=16)
    for k in range(6):
        a = 2 * math.pi * k / 6
        cyl(t, DARK, (0.16 * math.cos(a), deck + 0.03, -0.2 + 0.16 * math.sin(a)), (0.16 * math.cos(a), deck + 0.045, -0.2 + 0.16 * math.sin(a)), 0.014, seg=6)
    cyl(t, OLIVE, (0, deck + 0.05, -0.2), (0, T[1] - 0.12, -0.2), 0.06, seg=12)
    box(t, OLIVE, (0, T[1] - 0.11, T[2]), (0.22, 0.04, 0.1))
    for sx in (-1, 1):
        box(t, OLIVE, (sx * 0.095, T[1] - 0.04, T[2]), (0.025, 0.16, 0.08))
        cyl(t, AT_GREY, (sx * 0.075, T[1], T[2]), (sx * 0.12, T[1], T[2]), 0.025, seg=8)
    # the tube: about 1.4 m, a muzzle lip, a clamp band at the trunnion
    L0, L1 = -0.5, 0.95
    lathe(t, OLIVE, T + d * L0, d, [(0, 0.05), (L1 - L0 - 0.05, 0.05), (L1 - L0 - 0.05, 0.058), (L1 - L0, 0.058), (L1 - L0, 0.04),
                                    (L1 - L0 - 0.08, 0.04)], seg=14, caps=False)
    lathe(t, AT_GREY, T - d * 0.07, d, [(0, 0.062), (0.14, 0.062)], seg=14)
    # breech: the valve chamber, the trigger lever and a hand grip for laying it
    B = T + d * L0
    lathe(t, AT_GREY, B, -d, [(0, 0.06), (0.1, 0.075), (0.16, 0.075), (0.22, 0.04), (0.24, 0.0)], seg=12)
    side = np.array([1.0, 0.0, 0.0])
    cyl(t, DARK, B - d * 0.1 + side * 0.07, B - d * 0.1 + side * 0.07 + np.array([0, -0.1, -0.12]), 0.012, seg=6)   # trigger lever
    lathe(t, DARK, B - d * 0.1 + side * 0.07 + np.array([0, -0.1, -0.12]), (0, -0.4, -1), [(0, 0.02), (0.07, 0.02)], seg=8)
    tube(t, AT_GREY, [B - d * 0.05 - side * 0.06, B - d * 0.05 - side * 0.3, B - d * 0.25 - side * 0.3], 0.014, seg=6)   # laying handle
    # the air line from the breech down to the two bottles lying on the deck on the right
    tube(t, DARK, [B - d * 0.2, B - d * 0.2 + np.array([0.12, -0.06, -0.05]), np.array([0.3, deck + 0.24, -0.45]), np.array([0.3, deck + 0.17, -0.3])],
         0.013, seg=6)
    for x in (0.26, 0.42):
        lathe(t, OLIVE, (x, deck + 0.085, -0.62), (0, 0, 1), [(0, 0.0), (0, 0.06), (0.03, 0.08), (0.62, 0.08), (0.68, 0.04), (0.72, 0.02),
                                                             (0.72, 0.0)], seg=12)
        cyl(t, DARK, (x, deck + 0.085, 0.1), (x, deck + 0.085, 0.14), 0.018, seg=6)            # valve
    for z in (-0.5, -0.05):                                                                  # straps
        box(t, AT_GREY, (0.34, deck + 0.09, z), (0.36, 0.17, 0.03))
    cyl(t, DARK, (0.26, deck + 0.085, 0.14), (0.3, deck + 0.17, -0.3), 0.01, seg=5)
    # the bomb box, lid open against the back, eight Mills bombs inside
    bx, bz, w, dp, h = -0.42, 0.05, 0.26, 0.36, 0.12
    box(t, AT_GREY, (bx, deck + 0.01, bz), (w, 0.02, dp))
    for sx in (-1, 1):
        box(t, AT_GREY, (bx + sx * w / 2, deck + h / 2, bz), (0.015, h, dp))
        box(t, AT_GREY, (bx, deck + h / 2, bz + sx * dp / 2), (w, h, 0.015))
    box(t, AT_GREY, (bx, deck + h + 0.16, bz - dp / 2 - 0.03), (w, 0.32, 0.015), (-10, 0, 0))
    for i in range(2):
        for j in range(4):
            _tin(t, (bx - 0.06 + 0.12 * i, deck + 0.02, bz - 0.13 + 0.087 * j))
    _tin(t, (-0.2, deck, -0.45))                                                            # one set down by the breech
    # the crude anti-aircraft sight of the trials photos: a ring on a post behind the trunnion, a bead near the muzzle
    up = np.array([0.0, d[2], -d[1]])
    rc = T + d * -0.28 + up * 0.15
    cyl(t, DARK, T + d * -0.28 + up * 0.045, rc - up * 0.06, 0.006, seg=5)
    ring(t, DARK, rc, d, 0.06, 0.005, n=18, seg=4)
    cyl(t, DARK, rc - up * 0.06, rc + up * 0.06, 0.003, seg=4, caps=False)
    cyl(t, DARK, T + d * 0.8 + up * 0.05, T + d * 0.8 + up * 0.1, 0.005, seg=5)
    lathe(t, DARK, T + d * 0.8 + up * 0.1, up, [(0, 0.012), (0.012, 0.0)], seg=6)
    # pressure gauge on the line where it leaves the bottles
    g = np.array([0.3, deck + 0.24, -0.45])
    lathe(t, DARK, g, (1, 0, 0), [(0, 0.0), (0, 0.035), (0.025, 0.035), (0.025, 0.0)], seg=12)
    lathe(t, AT_GREY, g + (0.025, 0, 0), (1, 0, 0), [(0, 0.03), (0.003, 0.0)], seg=12)
    # drawn a size too big for the 60 mm's place: shrink it all toward the pedestal's foot
    c, k = np.array([0.0, deck, -0.2]), 0.72
    t.tris = [(mat, [c + (np.asarray(q) - c) * k for q in ps], ns, uv) for mat, ps, ns, uv in t.tris]
    return m


# ---- on the 75 mm pack howitzer (pivot = the slide; bore about (-0.01, 0.15)) ----------------------------------------------
YH, XH = 0.15, -0.01


def pak40():
    """7.5 cm Pak 40: the long L/46 barrel with the double-baffle muzzle brake, the sliding-block breech, the recoil cradle
    under it, and the spaced, angled two-plate shield with the sight window on the left."""
    m = Model('pak40', 'how75', item='7.5 cm Pak 40')
    b, t = m.barrel(), m.turret()
    lathe(b, HOW, (XH, YH, -0.15), (0, 0, 1), [(0, 0.085), (0.3, 0.085), (0.3, 0.065), (0.55, 0.058), (2.75, 0.045), (2.75, 0.0)], seg=16)
    # double-baffle brake: a block with two side ports each side
    z0 = 2.6
    box(b, HOW, (XH, YH, z0 + 0.06), (0.11, 0.11, 0.06))
    for zz in (z0 + 0.16, z0 + 0.31):
        box(b, HOW, (XH, YH, zz), (0.24, 0.12, 0.05))
    box(b, HOW, (XH, YH + 0.05, z0 + 0.21), (0.11, 0.02, 0.24))
    box(b, HOW, (XH, YH - 0.05, z0 + 0.21), (0.11, 0.02, 0.24))
    box(b, HOW_DARK, (XH, YH, z0 + 0.21), (0.05, 0.08, 0.24))
    box(b, HOW, (XH, YH, -0.12), (0.2, 0.18, 0.26))                                           # breech ring
    cyl(b, HOW_DARK, (XH + 0.1, YH + 0.04, -0.2), (XH + 0.18, YH + 0.04, -0.2), 0.02, seg=6)
    # cradle: recoil buffer and recuperator under the barrel, out to the trunnions on the carriage
    chamfer_box(t, HOW, (XH, YH - 0.12, 0.35), (0.18, 0.12, 1.05), 0.03)
    for dx in (-0.05, 0.05):
        cyl(t, HOW_DARK, (XH + dx, YH - 0.12, 0.88), (XH + dx, YH - 0.12, 0.93), 0.035, seg=8)
    # the shield: two 4 mm plates a hand apart, each in two halves angled back from the barrel (the Pak 40's shallow V seen
    # from above), leaning back, the wavy top, the sight window left of the barrel, the hinged apron below
    lean, yaw = -12, 9
    def plate(z, half_w, top, paint):
        # the lower part is narrower: the wide upper part stands over the wheels
        o = [(-0.4, -0.32), (0.4, -0.32), (0.4, -0.02), (half_w, -0.02), (half_w, top - 0.05), (half_w - 0.12, top), (0.12, top - 0.06),
             (-0.12, top - 0.06), (-half_w + 0.12, top), (-half_w, top - 0.05), (-half_w, -0.02), (-0.4, -0.02)]
        angled_shield(t, paint, [(x + XH, y + YH) for x, y in o], (XH - 0.08, XH + 0.08, YH - 0.08, YH + 0.09), z, lean, yaw, (XH, YH))
    plate(0.62, 0.62, 0.42, HOW)
    plate(0.7, 0.58, 0.36, HOW)
    for sx in (-1, 1):
        for x in (0.2, 0.45):                                                                   # spacer bolts between the plates
            cyl(t, HOW_DARK, (XH + sx * x, YH - 0.2, 0.62 - x * 0.16 - 0.006), (XH + sx * x, YH - 0.2, 0.7 - x * 0.16 + 0.006), 0.012, seg=6)
        a = math.radians(yaw)                                                                  # apron halves follow the V
        box(t, HOW, (XH + sx * 0.2, YH - 0.42, 0.7 - 0.2 * math.sin(a)), (0.4, 0.16, 0.012), (25, sx * yaw, 0))
    box(t, HOW_DARK, (XH - 0.22, YH + 0.18, 0.6), (0.1, 0.08, 0.03), (lean, -yaw, 0))          # sight window (dark)
    box(t, HOW, (XH - 0.22, YH + 0.24, 0.585), (0.12, 0.012, 0.06), (lean - 40, -yaw, 0))      # its flap, propped open
    # ZF 3x8 sight on the left of the cradle: the telescope, eyepiece with its rubber brow pad, the bracket
    box(t, HOW_DARK, (XH - 0.13, YH + 0.06, 0.15), (0.06, 0.04, 0.08))
    cyl(t, HOW_DARK, (XH - 0.2, YH + 0.14, 0.32), (XH - 0.2, YH + 0.14, -0.05), 0.022, seg=8)
    lathe(t, HOW_DARK, (XH - 0.2, YH + 0.14, 0.32), (0, 0, 1), [(0, 0.03), (0.04, 0.03), (0.04, 0.0)], seg=8)
    lathe(t, LEATHER, (XH - 0.2, YH + 0.14, -0.05), (0, 0, -1), [(0, 0.028), (0.05, 0.032), (0.05, 0.0)], seg=8)
    box(t, HOW_DARK, (XH - 0.15, YH + 0.1, 0.2), (0.1, 0.03, 0.04))
    return m


def sturmtiger():
    """38 cm RW 61, the Sturmtiger's rocket mortar (Discord 2026-11-15: a 'really stupid' utility gun): the huge stubby
    barrel with its ring of gas vents round the muzzle, the ball mount in a boxy armoured mantlet, the breech, a 345 kg
    rocket shell waiting on the loading tray beside it, and the little hand crane that swings it in."""
    m = Model('sturmtiger_38cm', 'how75', item='380mm RW 61')
    b, t = m.barrel(), m.turret()
    Ro, Rb = 0.25, 0.19
    yb = YH + 0.12
    # barrel: front face is an annulus; the vents are a ring of dark plugs standing on it; the bore is dark inside
    zm = 1.58                                                                                     # muzzle face
    lathe(b, HOW, (XH, yb, 0.2), (0, 0, 1), [(0, Ro), (zm - 0.35, Ro), (zm - 0.35, Ro + 0.015), (zm - 0.2, Ro + 0.015), (zm - 0.2, Ro),
                                             (zm, Ro), (zm, Rb)], seg=24, caps=False)
    lathe(b, HOW_DARK, (XH, yb, zm), (0, 0, -1), [(0, Rb), (0.35, Rb), (0.35, 0.0)], seg=24, caps=False)
    nv = 31
    for k in range(nv):
        a = 2 * math.pi * k / nv
        c = np.array([XH + 0.222 * math.cos(a), yb + 0.222 * math.sin(a), zm])
        cyl(b, HOW_DARK, c - (0, 0, 0.01), c + (0, 0, 0.012), 0.011, seg=5)
    # ball mount (Kugelblende): a ball round the barrel's root, standing well proud of the plate, then the breech block
    Rk = 0.4
    lathe(b, HOW, (XH, yb, 0.22), (0, 0, 1), [(-Rk * math.cos(math.radians(a)), Rk * math.sin(math.radians(a)))
                                               for a in range(40, 141, 10)][::-1], seg=20, caps=False)
    chamfer_box(b, HOW, (XH, yb, -0.2), (0.62, 0.62, 0.42), 0.08)
    cyl(b, HOW_DARK, (XH + 0.31, yb + 0.1, -0.25), (XH + 0.42, yb + 0.1, -0.25), 0.03, seg=8)        # breech lever
    lathe(b, HOW_DARK, (XH, yb, -0.41), (0, 0, -1), [(0, 0.12), (0.03, 0.1), (0.03, 0.0)], seg=12)   # breech plug face
    # front plate: thick armour sloped back like the Sturmtiger's 150 mm plate, the ball's socket collar, side cheeks
    box(t, HOW, (XH, yb + 0.02, 0.22), (1.0, 0.88, 0.14), (-20, 0, 0))
    lathe(t, HOW, (XH, yb, 0.28), (0, 0, 1), [(0, Rk * 0.86), (0.06, Rk * 0.86), (0.06, Rk * 0.66)], seg=24, caps=False)
    for sx in (-1, 1):
        box(t, HOW, (XH + sx * 0.45, yb - 0.1, -0.05), (0.1, 0.6, 0.45))
    # loading tray on the right with the RW 61 shell on it, base to the breech; on brackets off the right cheek, so it
    # elevates and traverses with the gun and never meets the deck
    tx = XH + 0.75
    box(t, HOW_DARK, (tx, yb - 0.24, -0.5), (0.34, 0.03, 1.6))
    for sx in (-1, 1):
        box(t, HOW_DARK, (tx + sx * 0.16, yb - 0.18, -0.5), (0.02, 0.1, 1.6))
    for z in (-0.2, 0.1):
        box(t, HOW_DARK, (XH + 0.54, yb - 0.25, z), (0.08, 0.04, 0.06))
        cyl(t, HOW_DARK, (XH + 0.5, yb - 0.36, z), (tx - 0.1, yb - 0.255, z), 0.015, seg=6)
    sy = yb - 0.24 + 0.015 + Rb
    lathe(t, HOW, (tx, sy, -1.25), (0, 0, 1), [(0, 0.0), (0, 0.16), (0.04, Rb), (0.95, Rb), (1.2, 0.15), (1.38, 0.07), (1.45, 0.0)], seg=16)
    for k in range(12):                                                                           # rocket nozzles round the base
        a = 2 * math.pi * k / 12
        c = np.array([tx + 0.12 * math.cos(a), sy + 0.12 * math.sin(a), -1.25])
        cyl(t, HOW_DARK, c, c + (0, 0, -0.04), 0.018, seg=5)
    ring(t, HOW_DARK, (tx, sy, -0.55), (0, 0, 1), Rb + 0.004, 0.01, n=20, seg=3)                # driving band
    # hand crane: a post at the back right, the jib out over the tray, the hoist chain and hook down to the shell
    px, pz = tx + 0.2, -1.2
    box(t, HOW_DARK, ((tx + px) / 2, yb - 0.24, pz), (px - tx + 0.08, 0.03, 0.1))                 # crane foot on the tray
    cyl(t, HOW_DARK, (px, yb - 0.24, pz), (px, yb + 0.85, pz), 0.04, seg=10)
    cyl(t, HOW_DARK, (px, yb + 0.85, pz), (tx, yb + 0.8, -0.65), 0.03, seg=8)
    cyl(t, HOW_DARK, (px, yb + 0.35, pz), (tx + 0.08, yb + 0.79, -0.75), 0.02, seg=6)          # brace
    handwheel(t, HOW_DARK, (px + 0.06, yb + 0.25, pz), (1, 0, 0), 0.09)
    hx, hz = tx, -0.65
    for k in range(7):
        y = yb + 0.75 - 0.06 * k
        ring(t, HOW_DARK, (hx, y, hz), (1, 0, 0) if k % 2 else (0, 0, 1), 0.018, 0.005, n=6, seg=3)
    tube(t, HOW_DARK, [(hx, yb + 0.33, hz), (hx, yb + 0.28, hz), (hx + 0.03, yb + 0.25, hz), (hx + 0.05, yb + 0.29, hz)], 0.008, seg=5)
    return m


# ---- on the 37 mm AT gun (bore (0, 0.465); Stand = carriage and wheels, Crank kept; Guard (shield) hidden) ----------------
YA = 0.465


def _cradle37(t, paint, length, z0=-0.1):
    """A cradle box from the carriage up to the barrel, the trunnion blocks, and links to the kept handwheels."""
    box(t, paint, (0, 0.2, z0), (0.22, 0.3, length))
    for sx in (-1, 1):
        box(t, paint, (sx * 0.14, 0.25, 0.0), (0.06, 0.18, 0.2))
    cyl(t, paint, (-0.11, 0.25, -0.4), (-0.33, 0.45, -0.95), 0.015, seg=6)
    cyl(t, paint, (-0.11, 0.2, -0.45), (-0.35, 0.33, -1.02), 0.015, seg=6)


def k21():
    """45 mm 21-K: the long naval barrel, a tall vertical-sliding semi-automatic breech, the recuperator above the barrel
    and the buffer below, a shoulder rest for the layer and a small flat shield."""
    m = Model('k21_45mm', 'at37', item='45 mm 21-K')
    b, t = m.barrel(), m.turret()
    lathe(b, OLIVE, (0, YA, -0.75), (0, 0, 1), [(0, 0.06), (0.25, 0.06), (0.25, 0.045), (0.6, 0.04), (2.65, 0.03), (2.65, 0.036),
                                               (2.72, 0.036), (2.72, 0.0)], seg=14)
    box(b, OLIVE, (0, YA - 0.02, -0.72), (0.18, 0.26, 0.24))                                   # breech, tall
    box(b, AT_GREY, (0, YA - 0.17, -0.72), (0.1, 0.06, 0.16))                                  # block dropped below
    cyl(b, DARK, (0.09, YA + 0.04, -0.75), (0.17, YA + 0.04, -0.75), 0.018, seg=6)
    cyl(t, OLIVE, (0, YA + 0.085, -0.5), (0, YA + 0.085, 0.45), 0.035, seg=10)                 # recuperator
    cyl(t, OLIVE, (0, YA - 0.085, -0.5), (0, YA - 0.085, 0.4), 0.04, seg=10)                   # buffer
    box(t, OLIVE, (0, YA, 0.1), (0.12, 0.22, 0.5))                                             # cradle sleeve
    _cradle37(t, AT_GREY, 0.8)
    tube(t, AT_GREY, [(-0.08, YA, -0.45), (-0.25, YA - 0.02, -0.5), (-0.27, YA - 0.02, -0.75)], 0.014, seg=6)   # shoulder rest arm
    box(t, LEATHER, (-0.27, YA - 0.02, -0.8), (0.06, 0.14, 0.1))
    box(t, DARK, (-0.14, YA + 0.12, -0.3), (0.04, 0.06, 0.24))                                 # sight
    cyl(t, DARK, (-0.14, YA + 0.12, -0.45), (-0.14, YA + 0.12, -0.5), 0.02, seg=8)
    sh = [(-0.5, -0.25), (0.5, -0.25), (0.5, 0.25), (0.35, 0.34), (-0.1, 0.34), (-0.1, 0.22), (-0.2, 0.22), (-0.2, 0.34), (-0.35, 0.34),
          (-0.5, 0.25)]
    slotted_shield(t, AT_GREY, [(x, y + YA) for x, y in sh], (-0.06, 0.06, YA - 0.06, YA + 0.06), 0.55)
    for sx in (-1, 1):                                                                         # the naval shield's wings, bent back
        box(t, AT_GREY, (sx * (0.5 + 0.075 * 0.866), YA, 0.55 - 0.075 * 0.5), (0.15, 0.5, 0.012), (0, sx * 30, 0))
    # second layer on the right (trainer): his shoulder rest; the anti-aircraft ring sight and bead over the barrel
    tube(t, AT_GREY, [(0.08, YA, -0.45), (0.25, YA - 0.02, -0.5), (0.27, YA - 0.02, -0.75)], 0.014, seg=6)
    box(t, LEATHER, (0.27, YA - 0.02, -0.8), (0.06, 0.14, 0.1))
    rc = np.array([-0.14, YA + 0.24, -0.35])
    cyl(t, DARK, (-0.14, YA + 0.15, -0.35), rc - (0, 0.08, 0), 0.007, seg=5)
    for R_ in (0.08, 0.04):
        ring(t, DARK, rc, (0, 0, 1), R_, 0.004, n=18, seg=4)
    cyl(t, DARK, rc - (0.08, 0, 0), rc + (0.08, 0, 0), 0.003, seg=4, caps=False)
    cyl(t, DARK, rc - (0, 0.08, 0), rc + (0, 0.08, 0), 0.003, seg=4, caps=False)
    cyl(t, DARK, (-0.14, YA + 0.04, 0.54), (-0.14, YA + 0.24, 0.54), 0.005, seg=5)              # foresight post off the shield
    lathe(t, DARK, (-0.14, YA + 0.24, 0.54), (0, 1, 0), [(0, 0.012), (0.012, 0.0)], seg=6)
    return m


def type1_47():
    """Type 1 47 mm AT gun: a long barrel with a muzzle collar, the horizontal sliding breech, a recoil cylinder under the
    barrel, and the low flat shield with its curved top and the sight notch on the left, the telescope looking through it."""
    m = Model('type1_47mm', 'at37', item='Type 1 47 mm AT')
    b, t = m.barrel(), m.turret()
    lathe(b, OLIVE, (0, YA, -0.7), (0, 0, 1), [(0, 0.075), (0.3, 0.075), (0.3, 0.055), (0.7, 0.048), (2.7, 0.036), (2.7, 0.046),
                                              (2.85, 0.046), (2.85, 0.0)], seg=14)
    box(b, OLIVE, (0, YA, -0.65), (0.22, 0.17, 0.24))                                          # breech ring
    cyl(b, DARK, (0.11, YA + 0.03, -0.7), (0.2, YA + 0.03, -0.72), 0.018, seg=6)
    cyl(t, OLIVE, (0, YA - 0.1, -0.45), (0, YA - 0.1, 0.55), 0.045, seg=10)                    # recoil cylinder
    lathe(t, OLIVE, (0, YA - 0.1, 0.55), (0, 0, 1), [(0, 0.045), (0.04, 0.03), (0.05, 0.0)], seg=10)
    box(t, OLIVE, (0, YA - 0.02, 0.0), (0.14, 0.08, 0.7))                                      # cradle
    _cradle37(t, AT_GREY, 0.8)
    # shield: low, curved top, a notch on the left for the sight, the bottom cut away round the axle
    sh = [(-0.62, -0.3), (-0.2, -0.3), (-0.2, -0.2), (0.2, -0.2), (0.2, -0.3), (0.62, -0.3), (0.62, 0.12), (0.5, 0.2), (0.25, 0.25),
          (-0.05, 0.26), (-0.05, 0.16), (-0.18, 0.16), (-0.18, 0.25), (-0.5, 0.2), (-0.62, 0.12)]
    slotted_shield(t, OLIVE, [(x, y + YA) for x, y in sh], (-0.07, 0.07, YA - 0.07, YA + 0.07), 0.45)
    box(t, DARK, (-0.11, YA + 0.11, -0.1), (0.05, 0.16, 0.08))                                  # sight bracket
    cyl(t, DARK, (-0.11, YA + 0.2, 0.4), (-0.11, YA + 0.2, -0.36), 0.02, seg=8)               # telescope, its objective in the notch
    lathe(t, DARK, (-0.11, YA + 0.2, 0.4), (0, 0, 1), [(0, 0.026), (0.03, 0.026), (0.03, 0.0)], seg=8)
    lathe(t, LEATHER, (-0.11, YA + 0.2, -0.36), (0, 0, -1), [(0, 0.024), (0.05, 0.028), (0.05, 0.0)], seg=8)
    box(t, OLIVE, (-0.115, YA + 0.27, 0.43), (0.15, 0.012, 0.06), (-30, 0, 0))               # flap over the notch, propped open
    return m


def blacker():
    """Blacker Bombard: the fat spigot housing on a cradle, the 20 lb bomb sitting on the spigot (round warhead, tail tube
    and ring fin), the curved shield with the sight window, and the firing handle behind."""
    m = Model('blacker_bombard', 'at37', item='Blacker Bombard')
    b, t = m.barrel(), m.turret()
    yb = YA + 0.05
    lathe(b, OLIVE, (0, yb, -0.6), (0, 0, 1), [(0, 0.0), (0, 0.07), (0.05, 0.095), (0.55, 0.095), (0.58, 0.06), (0.62, 0.025)], seg=16)
    cyl(b, AT_GREY, (0, yb, -0.02), (0, yb, 0.25), 0.022, seg=8)                               # the spigot (under the tail)
    # the bomb: tail tube over the spigot, ring fin, then the round warhead
    lathe(b, OLIVE, (0, yb, 0.0), (0, 0, 1), [(0, 0.035), (0.45, 0.035), (0.5, 0.06), (0.6, 0.11), (0.75, 0.13), (0.92, 0.11), (1.0, 0.06),
                                              (1.03, 0.0)], seg=16, caps=False)
    for k in range(4):
        a = k * math.pi / 2 + math.pi / 4
        box(b, AT_GREY, (0.06 * math.cos(a), yb + 0.06 * math.sin(a), 0.1), (0.005, 0.06, 0.12), (0, 0, math.degrees(a) - 90))
    ring(b, AT_GREY, (0, yb, 0.1), (0, 0, 1), 0.09, 0.01, n=16, seg=4)
    box(b, DARK, (0, yb, -0.68), (0.04, 0.18, 0.05))                                           # firing handle
    tube(b, DARK, [(0.0, yb - 0.08, -0.65), (0.0, yb - 0.2, -0.7), (0.0, yb - 0.2, -0.82)], 0.014, seg=6)
    # cradle and the curved shield: a band of flat strips round an arc, a window over the sight on the left
    _cradle37(t, AT_GREY, 0.6, z0=-0.2)
    box(t, AT_GREY, (0, yb - 0.11, -0.3), (0.12, 0.05, 0.45))
    Rs = 0.9
    for i in range(-4, 5):
        a = math.radians(i * 9)
        x, z = Rs * math.sin(a), -0.15 - Rs + Rs * math.cos(a)                                 # behind the bomb, round the housing
        ry = (0, math.degrees(a), 0)
        if i == -1:                                                                             # sight window strip
            box(t, AT_GREY, (x, yb - 0.25, z), (0.14, 0.35, 0.012), ry)
            box(t, AT_GREY, (x, yb + 0.26, z), (0.14, 0.1, 0.012), ry)
        elif i == 0:                                                                            # slot for the spigot housing
            box(t, AT_GREY, (x, yb - 0.255, z), (0.145, 0.31, 0.012), ry)
            box(t, AT_GREY, (x, yb + 0.21, z), (0.145, 0.2, 0.012), ry)
        else:
            box(t, AT_GREY, (x, yb - 0.05, z), (0.145, 0.72, 0.012), ry)
    box(t, DARK, (-0.14, yb + 0.12, -0.3), (0.04, 0.05, 0.12))                                  # sight, behind the window
    # the sight: a curved range bar across the shield top with upright pins for the ranges, a rear aperture post behind
    pts = []
    for i in range(-3, 4):
        a = math.radians(i * 5)
        pts.append((Rs * math.sin(a), yb + 0.316, -0.15 - Rs + Rs * math.cos(a)))   # resting on the top edge
    tube(t, DARK, pts, 0.006, seg=5)
    for q in pts[1::2]:
        cyl(t, DARK, q, (q[0], q[1] + 0.05, q[2]), 0.004, seg=4)
    cyl(t, DARK, (0, yb + 0.09, -0.55), (0, yb + 0.36, -0.55), 0.006, seg=5)
    ring(t, DARK, (0, yb + 0.37, -0.55), (0, 0, 1), 0.012, 0.003, n=10, seg=3)
    return m


MODELS = [holman, pak40, k21, type1_47, blacker, sturmtiger]


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
        print('%-20s %5d triangles  keep=%s  %s' % (m.name, n, ','.join(m.keep), getattr(m, 'note', '')))
        built.append(m)
    if prev:
        prev.mkdir(parents=True, exist_ok=True)
        for i in range(0, len(built), 3):
            tm.detail_sheet(built[i:i + 3], prev / ('sg2_utility_models_%d.png' % (i // 3 + 1)))
            if '--closeups' in sys.argv:
                closeup_sheet(built[i:i + 3], prev / ('sg2_utility_closeups_%d.png' % (i // 3 + 1)))


if __name__ == '__main__':
    main()
