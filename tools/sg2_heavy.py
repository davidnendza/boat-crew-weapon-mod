"""Second batch of side-grade models, HEAVY / HEAVY+ group: T-34 turret on the 3-inch, 1.1-inch quad and Breda 37/54
twin on the Dual Bofors, QF 2-pdr single and 3.7 cm SK C/30 on the Bofors, 10.5 cm SK C/32 on the 5-inch.

Same method and helpers as make_sidegrade_models.py (parts in the base gun's pivot frame, +Z forward, the base's own
materials, model_keep leaves the stock mount). Items are in sg2_heavy_items.txt.

Usage: python sg2_heavy.py <models folder> [--preview <folder>] [--closeups] [--only name,name]
       (needs the game installed and UnityPy; run python WITHOUT -I)
"""
import math
import pathlib
import sys

import numpy as np

from make_sidegrade_models import *          # Model, paints, BASES, shapes; X3/Y3 = the 3-inch bore
import make_sidegrade_models as sg
import make_turret_models as tm

tm.BASES.update({
    # the Dual Bofors: pivot not tilted, bores at x +-0.172, y 0.07; the receivers and clips sit under weapon
    'dualbofors': dict(prefab='40 mm Dual Bofors', pivot='weapon', barrels=['weapon/Bofors (Barrel)', 'weapon/Bofors (Barrel) (1)'],
                       keep=['Mount', 'Stand', 'Bofors Hand Crank']),
})

YD, XD = 0.07, 0.172                         # Dual Bofors bore height and half spacing (pivot frame)
Y1 = 0.075                                   # single Bofors bore height


def loft(p, paint, rings):
    """A closed hull through horizontal-ish outlines of equal point count (bottom first), flat shaded, capped.
    Outlines must be convex: the caps are fans from the centroid."""
    rings = [np.asarray(r, float) for r in rings]
    ctr = np.mean(np.concatenate(rings), axis=0)
    n = len(rings[0])
    for r0, r1 in zip(rings, rings[1:]):
        for i in range(n):
            a, b, c, d = r0[i], r0[(i + 1) % n], r1[(i + 1) % n], r1[i]
            nrm = np.cross(b - a, d - a)
            if np.linalg.norm(nrm) < 1e-12:
                nrm = np.cross(c - b, a - b)
            if np.dot(nrm, (a + b + c + d) / 4 - ctr) < 0:
                nrm = -nrm
            p.quad(paint, a, b, c, d, nrm)
    for r, sign in ((rings[0], -1), (rings[-1], 1)):
        c = r.mean(0)
        for i in range(n):
            p.tri(paint, (c, r[i], r[(i + 1) % n]), ((0, sign, 0),) * 3)


def yaw_part(m):
    """The part that turns with the mount but does not elevate (OBJ objects yaw_*; the mod parents them to the traverse)."""
    if hasattr(m, 'yaw'):
        return m.yaw()
    p = tm.Part('yaw'); m.parts.append(p); return p


def ring_sight(t, c, R, foot):
    """Open AA ring sight: two concentric rings across the line of sight on a post down to foot."""
    c = np.asarray(c, float)
    for r_ in (R, R / 2):
        ring(t, DARK, c, (0, 0, 1), r_, 0.004)
    for k in (-1, 1):
        cyl(t, DARK, c + (k * R, 0, 0), c - (k * R, 0, 0), 0.003, seg=4, caps=False)   # cross wires
    cyl(t, DARK, c - (0, R, 0), foot, 0.01, seg=6)


def plan(pts, y):
    """Plan outline [(x, z)] at height y."""
    return [(x, y, z) for x, z in pts]


def scaled(pts, k, kz=None, dz=0.0):
    kz = k if kz is None else kz
    return [(x * k, z * kz + dz) for x, z in pts]


# ---- HEAVY / HEAVY+: on the 3-inch ---------------------------------------------------------------------------------------
PIVOT_FWD = 1.0                              # T-34 item: pivot_offset=0,0,1


def t34_turret():
    """T-34/76 1942 welded hexagonal turret as on the Project 1125 river boats, true scale (body 2.3 wide, 1.9 m long,
    2.1 m with the mantlet, 0.84 m high): flat welded plates with sharp edges (vertical front plate, sloped sides and rear),
    the wide drum mantlet across the front with the F-34's boxy recoil cover, the coaxial DT right of the gun and the sight
    left of it; two round roof hatches with their lids standing open on the front hinges and a periscope between them;
    vision slits, pistol ports, lifting eyes. Under it a dark turret-ring band (the body overhangs it) and a hull-like
    glacis sloping out to the deck, which hide the crew's legs.
    The two hatches sit over the 3-inch crew spots (seats x +-0.55 from the axis, z -0.3, moved to -0.63); with the item's
    crew_offset=0,0.4,-0.33 the crew stand 0.4 m higher and 0.33 m further back and their heads and shoulders come out of the hatches.
    Body, skirt, roof fittings, lids: yaw part (turns, does not elevate); drum, recoil cover, DT: turret part; barrel recoils.
    The drum (r 0.5, centred on the elevation axis) reaches inside the body and skirt at both ends from -5 to +30 degrees."""
    m = Model('t34_turret', 'us3', keep=['Stand'], item='T-34 turret, 76 mm F-34')
    t, b = m.turret(), m.barrel()
    y = yaw_part(m)
    # authored in the level pivot frame (elevation axis = x axis through the origin); the traverse axis is x X3, z 0.053 - PIVOT_FWD
    cx = X3
    yb, yt, zf = -0.26, 0.58, 0.04                                           # body bottom, roof, front plate (crew toes reach z -0.08)
    # proportions from the 1942 drawing: half again as long as wide, widest a third of the way back (Discord: 'squished')
    lo = plan([(cx + x, z) for x, z in [(0.78, zf), (1.04, -0.70), (0.55, -2.2), (-0.55, -2.2), (-1.04, -0.70), (-0.78, zf)]], yb)
    hi = plan([(cx + x, z) for x, z in [(0.68, zf), (0.92, -0.70), (0.45, -2.06), (-0.45, -2.06), (-0.92, -0.70), (-0.68, zf)]], yt)
    loft(y, US3, [lo, hi])
    # the base (Discord 2026-11-15 'the whole mounting and turret swings'): on the 3-inch the whole mount turns, nothing
    # stays still, so the base is round about the traverse axis (x X3, z 0.053) and its turning cannot be seen: a dark
    # turret ring under the body (wide enough to hide the crew's legs) and a sloped armoured barbette out to the deck that covers the kept Stand disc (r 0.75
    # about the origin, 0.07 off the axis). The turret overhangs it, as tank turrets overhang their ring.
    # the item's pivot_offset=0,0,1 moves the pivot (and this whole model) 1 m forward, so the traverse axis is 1 m back
    # in this frame, under the middle of the turret body
    ax = (X3, 0.0, 0.053 - PIVOT_FWD)
    lathe(y, DARK, (ax[0], -0.43, ax[2]), (0, 1, 0), [(0, 0.0), (0, 1.04), (yb + 0.44, 1.04), (yb + 0.44, 0.0)], seg=32)
    hull = Paint(('Gray', 'Dark MG Metal'), (0.42, 0.43, 0.45), sg.box_uv(1.0))
    lathe(y, hull, (ax[0], -1.30, ax[2]), (0, 1, 0), [(0, 0.0), (0, 1.28), (0.82, 1.1), (0.87, 1.1), (0.87, 0.0)], seg=32)
    # mantlet as on the 1942 turret (Discord: 'the mantlet is wrong'): a drum lying across the front plate, narrower
    # than the plate, with rounded ends, centred on the elevation axis so it rolls in place; a raised band over its top;
    # the F-34's narrow cast recoil cover standing out of the drum around the barrel, bolted flanges on its sides
    R, hw = 0.36, 0.38
    cyl(t, US3, (cx - hw, 0, 0), (cx + hw, 0, 0), R, seg=28)
    for sx in (-1, 1):
        lathe(t, US3, (cx + sx * hw, 0, 0), (sx, 0, 0), [(0, R), (0.05, R * 0.94), (0.09, R * 0.78), (0.115, R * 0.5), (0.125, 0.0)], seg=28)
    _t34_sector(t, US3, cx - 0.24, cx + 0.24, R - 0.02, R + 0.02, 15, 85, n=8)
    zr = math.sqrt(R * R - Y3 * Y3)                                          # the drum's front at bore height
    box(t, US3, (X3, Y3 - 0.01, zr + 0.11), (0.2, 0.3, 0.3))                  # recoil cover, out to z ~0.6
    lathe(t, US3, (X3, Y3, zr + 0.26), (0, 0, 1), [(0, 0.1), (0.05, 0.085), (0.07, 0.05), (0.07, 0.0)], seg=14)
    for sx in (-1, 1):
        box(t, US3, (X3 + sx * 0.115, Y3 - 0.03, zr + 0.06), (0.03, 0.22, 0.14))  # flanges
        for dy in (-0.09, 0.03):
            cyl(t, DARK, (X3 + sx * 0.13, Y3 + dy, zr + 0.04), (X3 + sx * 0.14, Y3 + dy, zr + 0.04), 0.012, seg=6)
    zd = math.sqrt(R * R - (Y3 - 0.06) ** 2)
    cyl(t, US3, (X3 + 0.24, Y3 - 0.06, zd - 0.02), (X3 + 0.24, Y3 - 0.06, zd + 0.05), 0.045, seg=10)   # DT ball mount
    cyl(t, DARK, (X3 + 0.24, Y3 - 0.06, zd + 0.05), (X3 + 0.24, Y3 - 0.06, zd + 0.13), 0.011, seg=6)
    cyl(t, DARK, (X3 - 0.22, Y3 + 0.02, zr - 0.02), (X3 - 0.22, Y3 + 0.02, zr + 0.03), 0.024, seg=8)   # sight port
    # roof: open hatches over the crew spots, lids standing up on their front hinges, leaning 10 degrees back
    rh, th = yt, math.radians(80)
    for sx in (-1, 1):
        hx, hz = cx + sx * 0.55, -0.62                                      # crew_offset moves the crew 0.33 m back to here
        lathe(y, US3, (hx, rh, hz), (0, 1, 0), [(0, 0.29), (0.045, 0.29), (0.045, 0.245), (0, 0.245)], seg=20, caps=False)
        cyl(y, DARK, (hx, rh, hz), (hx, rh + 0.006, hz), 0.245, seg=20)
        hinge = np.array([hx, rh + 0.05, hz + 0.26])
        n = np.array([0, math.cos(th), math.sin(th)])                        # the lid's outer face, now facing forward
        c = hinge + 0.26 * np.array([0, math.sin(th), -math.cos(th)])
        cyl(y, US3, c - n * 0.03, c, 0.26, seg=20)
        for k in (-1, 1):
            box(y, US3, hinge + (k * 0.1, -0.01, 0.01), (0.06, 0.06, 0.08))
    box(y, US3, (cx, rh + 0.06, -0.3), (0.12, 0.12, 0.16))                   # periscope ahead of the hatches
    box(y, DARK, (cx, rh + 0.08, -0.219), (0.09, 0.04, 0.004))
    for k in range(3):
        cyl(y, DARK, (cx, rh, -1.4 - 0.12 * k), (cx, rh + 0.012, -1.4 - 0.12 * k), 0.025, seg=6)
    for sx in (-1, 1):
        ring(y, US3, (cx + sx * 0.35, rh + 0.03, -1.8), (1, 0, 0), 0.04, 0.012, n=10, seg=5)
    # side plates: vision slit forward, pistol port behind; rear pistol port
    for i in (0, 4):
        p, nrm, e = _t34_face(lo, hi, i, 0.5, 0.62)
        w = np.cross(nrm, e); w /= np.linalg.norm(w)
        q = p + nrm * 0.004
        y.quad(DARK, q - e * 0.08 - w * 0.012, q + e * 0.08 - w * 0.012, q + e * 0.08 + w * 0.012, q - e * 0.08 + w * 0.012, nrm)
    for i, u in ((1, 0.35), (3, 0.65), (2, 0.5)):
        p, nrm, e = _t34_face(lo, hi, i, u, 0.45)
        cyl(y, US3, p - nrm * 0.01, p + nrm * 0.04, 0.05, seg=10)
    # F-34 barrel, 3.1 m beyond the mantlet: plain tube tapering, slight swell at the muzzle (recoils)
    lathe(b, US3, (X3, Y3, 0.5), (0, 0, 1), [(0, 0.07), (0.25, 0.066), (0.31, 0.055), (3.15, 0.046), (3.23, 0.05), (3.3, 0.05),
                                             (3.3, 0.032), (3.28, 0.0)], seg=16)
    return m


def _t34_face(lo, hi, i, u, v):
    """Point at (u along, v up) on loft face i between rings lo and hi, its outward normal and its along direction."""
    a, b_ = np.array(lo[i]), np.array(lo[(i + 1) % len(lo)])
    c, d = np.array(hi[i]), np.array(hi[(i + 1) % len(hi)])
    p = (1 - v) * (a + (b_ - a) * u) + v * (c + (d - c) * u)
    nrm = np.cross(b_ - a, c - a)
    if np.dot(nrm, p - (np.mean(lo, 0) + np.mean(hi, 0)) / 2) < 0:
        nrm = -nrm
    return p, nrm / np.linalg.norm(nrm), (b_ - a) / np.linalg.norm(b_ - a)


def _t34_sector(p, paint, x0, x1, r0, r1, a0, a1, n=12):
    """Slab between x0 and x1: the ring sector r0..r1 about the x axis, angles a0..a1 degrees up from +z."""
    A = [math.radians(a0 + (a1 - a0) * i / n) for i in range(n + 1)]
    q = lambda x, r, a: np.array([x, r * math.sin(a), r * math.cos(a)])
    for i in range(n):
        a, c = A[i], A[i + 1]
        na, nc = np.array([0, math.sin(a), math.cos(a)]), np.array([0, math.sin(c), math.cos(c)])
        p.quad(paint, q(x0, r1, a), q(x1, r1, a), q(x1, r1, c), q(x0, r1, c), na, na, nc, nc)
        p.quad(paint, q(x0, r0, a), q(x0, r0, c), q(x1, r0, c), q(x1, r0, a), -na, -nc, -nc, -na)
        for x, sx in ((x0, -1), (x1, 1)):
            p.quad(paint, q(x, r0, a), q(x, r1, a), q(x, r1, c), q(x, r0, c), (sx, 0, 0))
    for a, sg_ in ((A[0], -1), (A[-1], 1)):
        nt = sg_ * np.array([0, math.cos(a), -math.sin(a)])
        p.quad(paint, q(x0, r0, a), q(x1, r0, a), q(x1, r1, a), q(x0, r1, a), nt)


# ---- HEAVY / HEAVY+: on the Dual Bofors ------------------------------------------------------------------------------------
def chicago_piano():
    """1.1-inch/75 Mk 1 quad 'Chicago Piano': four water-cooled guns side by side in one boxy slide, the cooling hoses
    from the recirculating pump to each jacket, the Mod 1 flash suppressors, a clip cradle over each breech holding
    two 8-round clips (one feeding, one waiting), a flat front shield with the barrels through it, open ring sights for
    the pointer and trainer (the mount was laid from a Mk 44 director). Each stock barrel carries a pair of guns, so the
    four recoil two by two."""
    m = Model('chicago_piano', 'dualbofors', item='1.1-inch/75 quad')
    t, b = m.turret(), m.barrel()
    gx = (XD - 0.086, XD + 0.086)                                              # the pair on barrel 0; copied to -x
    for x in gx:
        prof = [(0, 0.05), (0.06, 0.05), (0.06, 0.042)]                        # water jacket, ribbed at the ends
        prof += [(0.08, 0.042), (0.08, 0.047), (0.11, 0.047), (0.11, 0.042), (0.9, 0.042), (0.9, 0.047), (0.93, 0.047), (0.93, 0.042)]
        prof += [(1.0, 0.04), (1.0, 0.022), (2.0, 0.019), (2.0, 0.022), (2.16, 0.031), (2.16, 0.025), (2.14, 0.0)]   # flash suppressor
        lathe(b, BOF, (x, YD, 0.7), (0, 0, 1), prof, seg=12)
        cyl(b, BOF_GREY, (x, YD + 0.04, 0.95), (x, YD + 0.065, 0.95), 0.012, seg=6)              # jacket filler
        tube(b, DARK, [(x, YD - 0.04, 0.86), (x, YD - 0.085, 0.8), (x, YD - 0.085, 0.63)], 0.009, seg=5)   # cooling hose
    # the slide: one box across all four receivers, the trunnions out to the Dual Bofors' carriage
    chamfer_box(t, BOF, (0, YD - 0.02, -0.05), (0.7, 0.3, 1.25), 0.04)
    box(t, BOF_GREY, (0, YD - 0.2, 0.05), (0.62, 0.1, 0.9))                                      # recoil housings below
    for sx in (-1, 1):
        box(t, BOF_GREY, (sx * 0.4, YD - 0.04, 0.05), (0.1, 0.36, 0.7))
        cyl(t, BOF, (sx * 0.45, YD - 0.04, 0.05), (sx * 0.95, YD - 0.04, 0.05), 0.08, seg=12)
    # clip cradles: an open frame over each breech holding two 8-round clips side by side (rounds lying, base aft)
    for sx in (-1, 1):
        for x in gx:
            x *= sx
            for dz in (-0.07, 0.07):
                box(t, BOF_GREY, (x, YD + 0.38, -0.22 + dz), (0.11, 0.48, 0.012))
            box(t, BOF_GREY, (x - 0.055, YD + 0.38, -0.22), (0.012, 0.48, 0.14))
            for dx in (-0.024, 0.024):
                round_row(t, BRASS, (x + dx, YD + 0.37, -0.22), 8, 0.052, (0, 0, 1), 0.2, 0.016, (0, 1, 0))
                box(t, DARK, (x + dx, YD + 0.37, -0.325), (0.03, 0.43, 0.01))                    # clip spine at the case heads
    # flat front shield across the guns, fixed to the carriage (yaw part): an open-topped vertical slot for each barrel
    # (strips, no holes in extrude)
    y = yaw_part(m)
    zs, th = 0.62, 0.014
    xs = sorted([x * s for x in gx for s in (-1, 1)])
    box(y, BOF, (0, YD - 0.28, zs), (1.2, 0.04, th))                                           # bar under the slots
    edges = [-0.6] + [v for x in xs for v in (x - 0.06, x + 0.06)] + [0.6]
    for x0, x1 in zip(edges[0::2], edges[1::2]):
        box(y, BOF, ((x0 + x1) / 2, YD + 0.02, zs), (x1 - x0, 0.56, th))
    for sx in (-1, 1):
        box(y, BOF, (sx * 0.64, YD, zs - 0.08), (th, 0.6, 0.18), (0, sx * 25, 0))
    # open ring sights for the pointer (left) and trainer (right), on brackets from the slide
    for sx in (-1, 1):
        ring_sight(t, (sx * 0.75, YD + 0.42, 0.25), 0.09, (sx * 0.75, YD, 0.1))
        cyl(t, DARK, (sx * 0.75, YD, 0.1), (sx * 0.5, YD, 0.1), 0.01, seg=6)
    return m


def breda_twin():
    """Breda 37/54 Model 1938 twin: two guns side by side in an open cradle, the recoil spring sleeves and conical flash
    hiders, the 6-round clips dropped in from the top through tall vertical guides (empties out through a chute below),
    a low curved shield, open ring sights for the layer (left) and trainer (right)."""
    m = Model('breda_37_twin', 'dualbofors', item='Breda 37/54 twin')
    t, b = m.turret(), m.barrel()
    lathe(b, BOF, (XD, YD, 0.5), (0, 0, 1), [(0, 0.06), (0.4, 0.055), (0.4, 0.042), (0.5, 0.04), (2.3, 0.032), (2.3, 0.036),
                                            (2.36, 0.036), (2.62, 0.064), (2.62, 0.054), (2.58, 0.0)], seg=14)
    ring(b, BOF_GREY, (XD, YD, 0.62), (0, 0, 1), 0.045, 0.008, n=16)
    for sx in (-1, 1):
        x = sx * XD
        chamfer_box(t, BOF, (x, YD - 0.01, -0.12), (0.2, 0.3, 1.0), 0.03)                    # receiver
        lathe(t, BOF, (x, YD - 0.01, -0.62), (0, 0, -1), [(0, 0.08), (0.05, 0.065), (0.08, 0.0)], seg=12)
        # vertical clip guide over the breech with a 6-round clip standing in it
        for dz in (-0.07, 0.07):
            box(t, BOF_GREY, (x, YD + 0.34, -0.05 + dz), (0.12, 0.4, 0.014))
        round_row(t, BRASS, (x, YD + 0.36, -0.05), 6, 0.06, (0, 0, 1), 0.22, 0.019, (0, 1, 0))
        box(t, DARK, (x, YD + 0.36, -0.165), (0.034, 0.36, 0.01))                              # clip spine at the case heads
        box(t, BOF_GREY, (x, YD - 0.2, -0.32), (0.1, 0.12, 0.22))                            # empty-clip chute
    box(t, BOF_GREY, (0, YD - 0.2, -0.05), (0.56, 0.08, 0.8))                                 # cradle bridge
    for sx in (-1, 1):
        box(t, BOF_GREY, (sx * 0.3, YD - 0.05, 0.0), (0.06, 0.34, 0.6))
        cyl(t, BOF, (sx * 0.33, YD - 0.05, 0.0), (sx * 0.95, YD - 0.05, 0.0), 0.08, seg=12)
    # low curved shield in front, fixed to the carriage (yaw part), an open-topped slot over each barrel
    y = yaw_part(m)
    R, zc = 1.1, -0.5
    n = 27
    for k in range(n):
        a0, a1 = math.radians(-34 + 68 * k / n), math.radians(-34 + 68 * (k + 1) / n)
        am = (a0 + a1) / 2
        x, z = R * math.sin(am), zc + R * math.cos(am)
        w = 2 * R * math.sin((a1 - a0) / 2) + 0.01
        lo, hi = sorted((abs(R * math.sin(a0)), abs(R * math.sin(a1))))
        top = YD - 0.2 if lo < XD + 0.07 and hi > XD - 0.07 else YD + 0.24          # slot over each barrel
        box(y, BOF, (x, (YD - 0.32 + top) / 2, z), (w, top - YD + 0.32, 0.014), (0, math.degrees(am), 0))
    for sx in (-1, 1):
        ring_sight(t, (sx * 0.72, YD + 0.4, 0.2), 0.08, (sx * 0.72, YD - 0.05, 0.05))
        cyl(t, DARK, (sx * 0.72, YD - 0.05, 0.05), (sx * 0.33, YD - 0.05, 0.05), 0.01, seg=6)
    return m


# ---- HEAVY / HEAVY+: on the single Bofors ----------------------------------------------------------------------------------
def qf2pdr_single():
    """QF 2-pdr Mk VIII on the single power-operated Mk XVI mount: one water-jacketed gun with the flared flash hider,
    the 56-round belt fed from a box on the left (rounds parallel to the bore, the belt running sideways into the
    feed), links out on the right, a curved shield whose top falls toward the wings, with cut-outs for the layer's
    (left) and trainer's (right) ring sights."""
    m = Model('qf2pdr_single', 'bofors', item='QF 2-pdr single')
    t, b = m.turret(), m.barrel()
    chamfer_box(t, BOF, (0, Y1 - 0.04, -0.08), (0.18, 0.3, 1.0), 0.03)                     # receiver
    box(t, BOF_GREY, (0, Y1 + 0.12, -0.1), (0.14, 0.02, 0.84))
    lathe(t, BOF, (0, Y1 - 0.03, -0.58), (0, 0, -1), [(0, 0.07), (0.05, 0.055), (0.08, 0.0)], seg=12)
    prof = [(0.0, 0.075), (0.08, 0.075), (0.08, 0.068)]
    z = 0.12
    while z < 1.05:
        prof += [(z, 0.068), (z, 0.075), (z + 0.03, 0.075), (z + 0.03, 0.068)]
        z += 0.18
    prof += [(1.15, 0.068), (1.2, 0.052), (1.2, 0.032), (1.48, 0.03), (1.48, 0.036), (1.74, 0.078), (1.74, 0.066), (1.72, 0.0)]
    lathe(b, BOF, (0, Y1, 0.42), (0, 0, 1), prof, seg=14)
    cyl(b, BOF_GREY, (0, Y1 + 0.07, 0.6), (0, Y1 + 0.1, 0.6), 0.018, seg=8)
    # belt box on the left, the belt rising in a chute to the feed on the receiver's side
    chamfer_box(t, BOF_GREY, (-0.33, Y1 - 0.1, -0.12), (0.26, 0.36, 0.5), 0.02)
    box(t, BOF_GREY, (-0.17, Y1 + 0.02, -0.12), (0.08, 0.06, 0.2))
    round_row(t, BRASS, (-0.31, Y1 + 0.1, -0.06), 6, 0.044, (0, 0, 1), 0.2, 0.018, (1, 0, 0))     # belt on top of the box
    box(t, DARK, (-0.31, Y1 + 0.083, -0.1), (0.27, 0.006, 0.05))                                  # belt links
    box(t, BOF_GREY, (-0.15, Y1 + 0.06, -0.06), (0.06, 0.016, 0.22))                             # feed tray into the gun
    box(t, BOF_GREY, (0.15, Y1 - 0.14, -0.2), (0.08, 0.14, 0.3))                            # link chute
    for sx in (-1, 1):                                                                       # cradle cheeks, trunnions
        box(t, BOF, (sx * 0.24, -0.03, 0.0), (0.22, 0.14, 0.18))
        cyl(t, BOF, (sx * 0.35, -0.02, 0.0), (sx * 0.6, -0.02, 0.0), 0.07, seg=12)
    # curved shield ahead of the gun, fixed to the carriage (yaw part), an open-topped slot for the barrel
    y = yaw_part(m)
    R, zc = 0.9, -0.42
    n = 27
    for k in range(n):
        a0, a1 = math.radians(-40 + 80 * k / n), math.radians(-40 + 80 * (k + 1) / n)
        am = (a0 + a1) / 2
        x, z = R * math.sin(am), zc + R * math.cos(am)
        w = 2 * R * math.sin((a1 - a0) / 2) + 0.01
        x0, x1 = R * math.sin(a0), R * math.sin(a1)
        top = Y1 + 0.44 - 0.16 * (abs(x) / 0.58) ** 2                                       # the top falls toward the wings
        if abs(abs(x) - 0.4) < 0.06:
            top = Y1 + 0.24                                                                  # sight cut-outs
        bot = Y1 - 0.38
        if x0 < 0.09 and x1 > -0.09:                                                         # slot over the barrel
            box(y, BOF, (x, (bot + Y1 - 0.24) / 2, z), (w, Y1 - 0.24 - bot, 0.014), (0, math.degrees(am), 0))
        else:
            box(y, BOF, (x, (bot + top) / 2, z), (w, top - bot, 0.014), (0, math.degrees(am), 0))
    for sx in (-1, 1):                                                                       # sights look through the cut-outs
        ring_sight(t, (sx * 0.4, Y1 + 0.3, 0.1), 0.07, (sx * 0.4, Y1 + 0.05, 0.0))
        cyl(t, DARK, (sx * 0.4, Y1 + 0.05, 0.0), (sx * 0.2, Y1 + 0.0, 0.0), 0.01, seg=6)
    return m


def skc30():
    """3.7 cm SK C/30 on the single Ein LC/34 mount: a long thin 83-calibre barrel, the vertical sliding breech in a
    square block, the recoil cylinder under the barrel on an open cradle, no shield (the S-boat and U-boat singles
    went without one), hand-loaded one round at a time so no clip or belt. The layer has a telescope and an AA ring
    sight with its front bead, the trainer a shoulder piece; the stock Bofors hand cranks stand in for the handwheels."""
    m = Model('skc30_37mm', 'bofors', item='3.7 cm SK C/30')
    t, b = m.turret(), m.barrel()
    chamfer_box(b, BOF, (0, Y1, -0.3), (0.2, 0.22, 0.34), 0.03)                             # breech block
    box(b, BOF_GREY, (0, Y1 - 0.13, -0.3), (0.08, 0.06, 0.2))                               # breech slide
    cyl(b, DARK, (0.1, Y1 + 0.03, -0.4), (0.18, Y1 + 0.03, -0.4), 0.02, seg=8)              # breech lever
    lathe(b, BOF, (0, Y1, -0.13), (0, 0, 1), [(0, 0.06), (0.3, 0.055), (0.3, 0.045), (0.5, 0.04), (3.4, 0.03),
                                             (3.4, 0.035), (3.46, 0.035), (3.46, 0.0)], seg=14)
    # cradle: a long open trough with the recoil cylinder below, side plates out to the trunnions
    box(t, BOF_GREY, (0, Y1 - 0.08, 0.1), (0.14, 0.03, 0.9))
    cyl(t, BOF, (0, Y1 - 0.12, -0.25), (0, Y1 - 0.12, 0.7), 0.045, seg=12)
    lathe(t, BOF, (0, Y1 - 0.12, 0.7), (0, 0, 1), [(0, 0.045), (0.04, 0.03), (0.05, 0.0)], seg=12)
    for z in (-0.05, 0.45):
        ring(t, BOF_GREY, (0, Y1, z), (0, 0, 1), 0.06, 0.012, n=16)
    for sx in (-1, 1):
        box(t, BOF, (sx * 0.15, -0.03, 0.0), (0.04, 0.3, 0.4))
        box(t, BOF, (sx * 0.3, -0.03, 0.0), (0.3, 0.12, 0.16))
        cyl(t, BOF, (sx * 0.44, -0.02, 0.0), (sx * 0.6, -0.02, 0.0), 0.07, seg=12)
    # layer's telescope and a ring sight on the left, shoulder piece on the right
    cyl(t, DARK, (-0.3, Y1 + 0.18, -0.35), (-0.3, Y1 + 0.18, 0.05), 0.03, seg=10)
    box(t, DARK, (-0.3, Y1 + 0.1, -0.15), (0.04, 0.12, 0.04))
    box(t, DARK, (-0.22, Y1 + 0.04, -0.15), (0.16, 0.03, 0.03))
    ring_sight(t, (-0.45, Y1 + 0.35, 0.0), 0.07, (-0.3, Y1 + 0.2, 0.0))                       # post stands on the telescope
    cyl(t, DARK, (-0.45, Y1 + 0.35, 0.62), (-0.2, Y1 - 0.05, 0.62), 0.008, seg=5)                 # front bead on its arm
    lathe(t, DARK, (-0.45, Y1 + 0.35, 0.62), (0, 1, 0), [(0, 0.0), (0, 0.012), (0.02, 0.0)], seg=6)
    box(t, BOF_GREY, (-0.135, Y1 - 0.05, 0.62), (0.13, 0.03, 0.04))                               # bead arm clamp on the cradle
    tube(t, DARK, [(0.15, Y1 - 0.05, -0.2), (0.38, Y1 - 0.02, -0.25), (0.42, Y1 - 0.08, -0.45)], 0.015, seg=6)
    box(t, BOF_GREY, (0.42, Y1 - 0.06, -0.5), (0.1, 0.12, 0.05))
    return m


# ---- HEAVY+: on the 5-inch ---------------------------------------------------------------------------------------------
def skc32():
    """10.5 cm SK C/32 in the MPL C/32 pedestal mount of the torpedo boats and minesweepers: a plain monobloc barrel
    with no muzzle brake, the long recuperator over it and the buffer under it on a narrow cradle, the horizontal
    wedge breech, handwheels both sides, the layer's and trainer's telescopes, a short loading tray, and the small
    open-backed splinter shield those boats carried (flat face, cut corners, short angled wings, slot for the cradle,
    a sight window each side). U-boat mounts had no shield."""
    m = Model('skc32_105mm', 'us5', item='10.5 cm SK C/32')
    t, b = m.turret(), m.barrel()
    lathe(b, US5, (0, 0, -0.85), (0, 0, 1), [(0, 0.14), (0.45, 0.14), (0.45, 0.1), (0.8, 0.088), (1.4, 0.078), (4.05, 0.062),
                                            (4.05, 0.068), (4.12, 0.068), (4.12, 0.0)], seg=16)
    chamfer_box(b, US5, (0, 0, -0.62), (0.36, 0.34, 0.46), 0.04)                              # breech ring
    box(b, US5, (0.21, 0, -0.62), (0.08, 0.22, 0.3))                                          # wedge housing (right)
    cyl(b, DARK, (0.25, 0.08, -0.78), (0.36, 0.08, -0.78), 0.03, seg=8)                       # breech lever
    cyl(t, US5, (0, 0.17, -0.4), (0, 0.17, 1.45), 0.075, seg=14)                              # recuperator
    lathe(t, US5, (0, 0.17, 1.45), (0, 0, 1), [(0, 0.075), (0.05, 0.055), (0.07, 0.0)], seg=14)
    cyl(t, US5, (0, -0.15, -0.35), (0, -0.15, 1.05), 0.065, seg=12)                           # buffer
    lathe(t, US5, (0, -0.15, 1.05), (0, 0, 1), [(0, 0.065), (0.04, 0.045), (0.05, 0.0)], seg=12)
    chamfer_box(t, US5, (0, 0.0, 0.3), (0.3, 0.26, 1.5), 0.04)                                 # cradle
    for z in (-0.25, 0.95):
        box(t, US5, (0, 0.01, z), (0.22, 0.42, 0.08))                                          # cradle bands
    y = yaw_part(m)                                                                           # the carriage: turns, does not elevate
    for sx in (-1, 1):
        box(y, US5, (sx * 0.25, -0.06, 0.0), (0.16, 0.3, 0.34))                               # trunnion brackets
        cyl(y, US5, (sx * 0.33, -0.06, 0.0), (sx * 0.5, -0.06, 0.0), 0.09, seg=12)
        handwheel(y, DARK, (sx * 0.6, -0.12, -0.1), (1, 0, 0), 0.14)                          # on the carriage, below the sights' swing
        cyl(y, DARK, (sx * 0.6, -0.12, -0.1), (sx * 0.33, -0.12, -0.1), 0.02, seg=6)
    box(t, DARK, (-0.42, 0.18, -0.2), (0.08, 0.1, 0.4))                                       # layer's sight
    cyl(t, DARK, (-0.42, 0.18, -0.4), (-0.42, 0.18, -0.48), 0.03, seg=8)
    box(t, DARK, (0.42, 0.18, -0.2), (0.08, 0.1, 0.4))                                        # trainer's sight
    box(t, US5, (0.0, -0.12, -1.04), (0.24, 0.04, 0.4))                                       # loading tray, kept short of the loader
    box(t, US5, (0.0, -0.1, -0.86), (0.2, 0.08, 0.06))
    # splinter shield ahead of the trunnions, fixed to the carriage: two halves either side of a full-height slot the
    # cradle elevates in, a sight window in each, wings angled back
    zs, top, bot, hw = 0.78, 0.48, -0.42, 0.72
    left = [(-hw, bot), (-0.19, bot), (-0.19, top), (-0.36, top), (-0.36, 0.12), (-0.48, 0.12), (-0.48, top),
            (-hw + 0.16, top), (-hw, top - 0.16)]
    for face in (left, [(-x, yy) for x, yy in reversed(left)]):
        sg.flat_shield(y, US5, face, zs, thick=0.016)
    for sx in (-1, 1):
        wing = [(0, bot), (0.34, bot), (0.34, top - 0.24), (0, top - 0.16)]
        a = math.radians(30)
        extrude(y, US5, wing, (sx * math.sin(a), 0, -math.cos(a)), (0, 1, 0),
                (math.cos(a), 0, sx * math.sin(a)), -0.008, 0.008, origin=(sx * hw, 0, zs))
        for yy in (bot + 0.12, 0.08):                                                              # stays back to the trunnion brackets
            cyl(y, US5, (sx * 0.3, yy, zs - 0.01), (sx * 0.26, yy * 0.4, 0.12), 0.018, seg=6)
    return m


def shift_parts(m, d):
    """Move every part of m by d (parts authored with the bore at the origin)."""
    d = np.asarray(d, float)
    for part in m.parts:
        part.tris = [(mat, [np.asarray(q) + d for q in ps], ns, uv) for mat, ps, ns, uv in part.tris]


def long_tom():
    """US 155 mm Gun M1 'Long Tom' (field artillery), on the 3-inch pedestal so it fits Heavy and Heavy+ slots: a very
    long L/45 monobloc barrel with no muzzle brake, the big interrupted-screw breech, the long cradle with the recoil
    cylinders under it, the two tall equilibrators either side, handwheels on the carriage and a panoramic sight. No
    shield. The barrel runs far past the stock muzzle, so the muzzle flash shows partway along it (shoot points stay)."""
    m = Model('long_tom_155mm', 'us3', keep=['Stand', 'US_3in_base', 'Bofors Hand Crank'], item='155 mm M1 Long Tom')
    t, b = m.turret(), m.barrel()
    lathe(b, US3, (0, 0, -1.15), (0, 0, 1), [(0, 0.24), (0.7, 0.24), (0.7, 0.19), (1.6, 0.17), (2.6, 0.15), (5.85, 0.11),
                                            (5.85, 0.125), (5.95, 0.125), (5.95, 0.0)], seg=20)
    chamfer_box(b, US3, (0, 0, -0.85), (0.54, 0.54, 0.62), 0.06)                              # breech ring
    cyl(b, DARK, (-0.27, 0.0, -1.16), (-0.27, -0.2, -1.16), 0.035, seg=8)                     # breech-block hinge
    cyl(b, DARK, (0.27, 0.05, -1.05), (0.42, 0.05, -1.05), 0.035, seg=8)                      # operating lever
    chamfer_box(t, US3, (0, -0.04, 0.55), (0.5, 0.44, 2.3), 0.05)                              # cradle
    for sx in (-1, 1):
        cyl(t, US3, (sx * 0.11, -0.3, -0.45), (sx * 0.11, -0.3, 1.75), 0.085, seg=12)         # recoil cylinders
        lathe(t, US3, (sx * 0.11, -0.3, 1.75), (0, 0, 1), [(0, 0.085), (0.05, 0.06), (0.07, 0.0)], seg=12)
        cyl(t, US3, (sx * 0.34, 0.05, 0.9), (sx * 0.34, -0.6, 0.25), 0.09, seg=12)            # equilibrators
        cyl(t, DARK, (sx * 0.34, -0.6, 0.25), (sx * 0.34, -0.75, 0.12), 0.04, seg=8)
    for z in (-0.4, 0.6, 1.5):
        box(t, US3, (0, -0.05, z), (0.56, 0.5, 0.09))                                          # cradle bands
    y = yaw_part(m)                                                                           # the carriage: turns, does not elevate
    for sx in (-1, 1):
        box(y, US3, (sx * 0.32, -0.1, 0.0), (0.16, 0.42, 0.42))                               # trunnion brackets
        cyl(y, US3, (sx * 0.4, -0.1, 0.0), (sx * 0.55, -0.1, 0.0), 0.11, seg=12)
        handwheel(y, DARK, (sx * 0.66, -0.18, -0.15), (1, 0, 0), 0.16)
        cyl(y, DARK, (sx * 0.66, -0.18, -0.15), (sx * 0.4, -0.18, -0.15), 0.02, seg=6)
    box(t, DARK, (-0.45, 0.22, -0.35), (0.09, 0.12, 0.3))                                     # panoramic sight
    cyl(t, DARK, (-0.45, 0.28, -0.35), (-0.45, 0.42, -0.35), 0.04, seg=10)
    box(t, US3, (0.0, -0.2, -1.5), (0.3, 0.05, 0.45))                                         # loading tray
    shift_parts(m, (X3, Y3, 0))
    return m


def longer_tom():
    """The Longer Tom Unique's look: an over-the-top siege gun (an original design from generic heavy-artillery parts):
    a thick stepped barrel with collars and a slotted muzzle brake, a big ribbed breech drum, two hydraulic recoil struts,
    slab armour cheeks on the carriage, a counterweight behind the breech. The Unique also scales the whole mount up."""
    m = Model('longer_tom', 'us3', keep=['Stand', 'US_3in_base', 'Bofors Hand Crank'], item='155 mm M1 Long Tom')
    t, b = m.turret(), m.barrel()
    # barrel: three steps, a collar at each step, the muzzle brake at the end
    lathe(b, US3, (0, 0, -0.2), (0, 0, 1), [(0, 0.27), (1.4, 0.27), (1.4, 0.21), (3.2, 0.21), (3.2, 0.17), (5.25, 0.17),
                                           (5.25, 0.0)], seg=24)                                 # runs 0.05 m into the brake (starts 0.2 m back)
    for z, r in ((1.3, 0.31), (3.1, 0.25), (4.9, 0.2)):
        lathe(b, DARK, (0, 0, z), (0, 0, 1), [(0, r), (0.16, r), (0.16, 0.0)], seg=24)
    lathe(b, US3, (0, 0, 5.0), (0, 0, 1), [(0, 0.25), (0.62, 0.25), (0.62, 0.12), (0.66, 0.12), (0.66, 0.0)], seg=24)
    for sx in (-1, 1):                                                                        # muzzle brake ports
        for k in range(3):
            box(b, DARK, (sx * 0.235, 0, 5.12 + 0.17 * k), (0.04, 0.26, 0.1))
    lathe(b, DARK, (0, 0, 5.62), (0, 0, 1), [(0, 0.15), (0.05, 0.15), (0.05, 0.0)], seg=24)   # bore mouth
    # breech drum with ribs, and a counterweight block behind it
    lathe(b, US3, (0, 0, -1.25), (0, 0, 1), [(0, 0.3), (0.1, 0.44), (1.0, 0.44), (1.1, 0.34), (1.1, 0.0)], seg=24)
    for z in (-1.0, -0.75, -0.5):
        lathe(b, DARK, (0, 0, z), (0, 0, 1), [(0, 0.46), (0.07, 0.46), (0.07, 0.0)], seg=24)
    chamfer_box(b, US3, (0, 0.05, -1.45), (0.6, 0.56, 0.4), 0.08)
    box(b, DARK, (0.36, 0.1, -1.45), (0.08, 0.2, 0.26))                                      # breech actuator
    # cradle under the barrel and two hydraulic recoil struts: thick cylinder, chrome rod, end eyes
    chamfer_box(t, US3, (0, -0.36, 0.4), (0.62, 0.26, 2.4), 0.05)
    for sx in (-1, 1):
        x = sx * 0.2
        cyl(t, US3, (x, -0.42, -0.6), (x, -0.42, 1.0), 0.1, seg=14)
        cyl(t, BRASS, (x, -0.42, 1.0), (x, -0.32, 2.3), 0.045, seg=10)
        cyl(t, DARK, (x, -0.3, 2.35), (x, -0.22, 2.35), 0.07, seg=10)
        box(t, US3, (x, -0.26, 2.35), (0.1, 0.12, 0.14))
    # armoured cheeks on the carriage (turn, do not elevate), trunnion hubs and a handwheel each side
    y = yaw_part(m)
    cheek = [(-0.75, -0.75), (0.85, -0.75), (0.55, 0.15), (0.05, 0.45), (-0.6, 0.35)]
    for sx in (-1, 1):
        extrude(y, US3, cheek, (0, 0, 1), (0, 1, 0), (1, 0, 0), sx * 0.5, sx * 0.62)
        cyl(y, DARK, (sx * 0.62, 0, 0), (sx * 0.7, 0, 0), 0.2, seg=16)
        handwheel(y, DARK, (sx * 0.78, -0.35, -0.35), (1, 0, 0), 0.15)
    box(t, DARK, (-0.4, 0.42, -0.6), (0.12, 0.14, 0.36))                                      # sight box on top of the drum
    shift_parts(m, (X3, Y3, 0))
    return m


MODELS = [t34_turret, chicago_piano, qf2pdr_single, skc30, breda_twin, skc32, long_tom, longer_tom]


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
            tm.detail_sheet(built[i:i + 3], prev / ('sg2_heavy_models_%d.png' % (i // 3 + 1)))
            if '--closeups' in sys.argv:
                sg.closeup_sheet(built[i:i + 3], prev / ('sg2_heavy_closeups_%d.png' % (i // 3 + 1)))


if __name__ == '__main__':
    main()
