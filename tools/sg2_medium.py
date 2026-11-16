"""Second batch of side-grade models, Medium and Tub Turret slots: the MG 151/20 Drilling on the twin Oerlikon, the Breda
20/65 Mod. 40, Madsen 20 mm and Polsten on the single Oerlikon in its tub, the twin MG 34 on the Lewis ring mount and the
twin DShK on the M2 ring mount. Same method and helpers as make_sidegrade_models.py.

The Oerlikon guns wear the Oerlikon's own baked material: their UVs are squeezed onto one small patch of its atlas, taken
from a flat face of the stock receiver or magazine, so the paint matches the stock gun (Dark MG Metal is the fallback).

Usage: python sg2_medium.py <models folder> [--preview <folder>] [--only name,name] [--closeups]
       (needs the game installed and UnityPy; run python WITHOUT -I, from tools/)
"""
import math
import pathlib
import sys

import numpy as np

import gamemesh as gm
import make_turret_models as tm
from make_sidegrade_models import *                     # noqa: F401,F403  Model, paints, shapes, closeup_sheet
from make_sidegrade_models import Model, closeup_sheet

# the turret guns' bases, with the pivot the side-grade Model wants (all three turn on 'weapon')
for _k in ('lewis', 'm2', 'tub20'):
    tm.BASES[_k].setdefault('pivot', 'weapon')

LEWIS_METAL, LEWIS_WOOD, METAL = tm.LEWIS_METAL, tm.LEWIS_WOOD, tm.METAL


def _atlas_patch(prefab, node):
    """A UV on the stock mesh's baked atlas whose colour is the mesh's typical (median) colour and even around it: one
    flat spot of paint. Picked from the bigger triangles, judged by the preview renderer's texture lookup."""
    import preview3d as pv
    tris, mats = gm.Prefab(prefab).mesh(node, 'weapon')
    tris = [t for t in tris if mats[min(t[0], len(mats) - 1)] == 'Oerlikon Baked Material']
    shade = pv._shade_fn('Oerlikon Baked Material')
    area = np.array([np.linalg.norm(np.cross(t[1][1] - t[1][0], t[1][2] - t[1][0])) for t in tris])
    uvs = np.array([t[3] for t in tris])                  # n x 3 x 2
    cen = uvs.mean(1)
    lum = shade(cen).mean(1)
    med = np.median(np.repeat(lum, np.maximum(1, (area / area.max() * 50).astype(int))))
    spread = np.ptp(np.stack([shade(uvs[:, k]).mean(1) for k in range(3)] + [lum]), axis=0)
    big = area >= np.quantile(area, 0.7)
    score = np.where(big, np.abs(lum - med) + spread, 9)
    return tuple(cen[int(score.argmin())])


def _oer(node):
    return Paint(('Oerlikon Baked Material', 'Dark MG Metal'), (0.30, 0.31, 0.33),
                 region_uv(_atlas_patch('20 mm Tub Oerlikon', 'weapon/' + node), 0.002))


OER = _oer('Oerlikon Barrel And Reciever/Reciever_LOD0')          # the twin Oerlikon wears the same material and atlas
OER_BAR = _oer('Oerlikon Barrel And Reciever/Barrel_LOD0')
OER_MAG = _oer('Oerlikon Magazine/Magazine_LOD0')


def ring_sight(t, paint, c, R, spokes=4, post_to=None):
    c = np.asarray(c, float)
    for r_ in (R, R * 0.5):
        ring(t, paint, c, (0, 0, 1), r_, 0.004)
    for k in range(spokes):
        a = math.pi / 4 + k * 2 * math.pi / spokes
        cyl(t, paint, c, c + R * np.array([math.cos(a), math.sin(a), 0]), 0.0025, seg=4)
    if post_to is not None:
        cyl(t, paint, (c[0], c[1] - R, c[2]), post_to, 0.006, seg=6)


def shoulder_rests(t, x_from, y, z_from, x=0.18, z=-0.72, r=0.08):
    """The pair of C-shaped padded shoulder pieces of a free-swinging single mount, open towards the gunner, each on an
    arm from the back of the receiver (x_from, y, z_from)."""
    for sx in (-1, 1):
        arc = [(sx * x, y + r * math.sin(a), z + r * math.cos(a)) for a in np.radians(np.linspace(-115, 115, 11))]
        tube(t, OER, arc, 0.007, seg=6)
        tube(t, LEATHER, arc[2:-2], 0.016, seg=8)
        tube(t, OER, [(sx * x_from, y, z_from), (sx * x * 0.75, y, z_from - 0.06), (sx * x, y, z + r)], 0.009, seg=6)


# ---- MEDIUM: on the twin Oerlikon ----------------------------------------------------------------------------------------
def drilling():
    """MG 151/20 Drilling: three slim aircraft cannon side by side on one flat cradle, a belt box hung under each gun with
    a feed chute up into it, a ring sight over the middle gun. The twin's own shoulder rests and handwheel are kept.
    The guns sit inside the mount's trunnion arms (|x| < 0.18); the boxes stay behind the pedestal head."""
    m = Model('mg151_drilling', 'dual20', item='MG 151/20 Drilling')
    t = m.turret()
    P = OER
    for x in (-0.115, 0.0, 0.115):
        chamfer_box(t, P, (x, 0.0, -0.22), (0.075, 0.11, 0.78), 0.018)                      # receiver
        box(t, P, (x, 0.062, -0.28), (0.055, 0.014, 0.5))                                    # feed cover
        lathe(t, P, (x, 0.0, -0.61), (0, 0, -1), [(0, 0.035), (0.03, 0.032), (0.05, 0.0)], seg=10)   # buffer
        box(t, P, (x, -0.065, -0.36), (0.03, 0.03, 0.12))                                    # trigger solenoid
        # barrel: the thick breech end, the long thin tube, the small flash cone
        lathe(t, OER_BAR, (x, 0.0, 0.17), (0, 0, 1), [(0, 0.032), (0.12, 0.032), (0.12, 0.026), (0.28, 0.024), (0.3, 0.017),
                                               (1.25, 0.014), (1.25, 0.018), (1.3, 0.018), (1.36, 0.026), (1.36, 0.0)], seg=12)
        # belt box under the gun and the feed chute from its lid up the gun's outer side (middle gun: its left side)
        sx = 1 if x > 0 else -1
        box(t, P, (x, -0.165, -0.37), (0.072, 0.13, 0.34))
        box(t, P, (x, -0.098, -0.37), (0.078, 0.01, 0.35))                                     # lid
        box(t, DARK, (x + sx * 0.037, -0.165, -0.37), (0.004, 0.05, 0.1))                      # lid catch
        cx = x + sx * 0.045
        tube(t, P, [(cx, -0.1, -0.3), (cx + sx * 0.008, -0.06, -0.27), (cx + sx * 0.004, -0.02, -0.24), (cx, 0.02, -0.22)], 0.013, seg=6)
        box(t, P, (cx, 0.02, -0.2), (0.016, 0.04, 0.08))                                       # feed adapter
    box(t, P, (0.0, -0.075, -0.25), (0.33, 0.02, 0.62))                                      # cradle
    # ring sight on a pillar over the middle receiver, rear bead behind it
    box(t, DARK, (0.0, 0.08, 0.04), (0.04, 0.03, 0.06))
    ring_sight(t, DARK, (0.0, 0.2, 0.04), 0.075, post_to=(0.0, 0.08, 0.04))
    box(t, DARK, (0.0, 0.095, -0.5), (0.02, 0.06, 0.02))
    return m


# ---- TUB TURRET: on the single Oerlikon -----------------------------------------------------------------------------------
def breda():
    """Breda 20/65 Mod. 40: a boxy receiver with the 12-round strip lying in its feed tray on the left, a long thin barrel
    with the gas cylinder under it and the cone flash hider, a small flat shield on brackets at the front of the
    receiver (an inverted-U cut clears the barrel, gas cylinder and the stock cradle under them), the padded shoulder
    rests of the free-swinging mount and a ring sight on the left."""
    m = Model('breda_20mm', 'tub20', item='Breda 20/65 Mod. 40')
    t, b = m.turret(), m.barrel()
    P = OER
    chamfer_box(t, P, (0.0, 0.02, -0.12), (0.13, 0.19, 0.84), 0.02)                            # receiver
    box(t, P, (0.0, 0.12, -0.16), (0.1, 0.014, 0.6))
    lathe(t, P, (0.0, 0.02, -0.54), (0, 0, -1), [(0, 0.05), (0.04, 0.045), (0.07, 0.0)], seg=12)
    box(t, P, (0.0, -0.1, -0.3), (0.05, 0.06, 0.16))                                           # trigger group
    # the strip feed: a tray out to the left with a strip of 12 rounds lying across it, the empty strip comes out right;
    # high enough to clear the stock trunnion arms (|x| 0.14, up to y 0.06)
    box(t, P, (-0.2, 0.085, 0.0), (0.27, 0.012, 0.22))
    box(t, P, (-0.2, 0.115, -0.1), (0.27, 0.06, 0.012))
    round_row(t, BRASS, (-0.17, 0.11, 0.0), 12, 0.026, (0, 0, 1), 0.19, 0.011, (1, 0, 0))
    box(t, P, (0.09, 0.095, 0.0), (0.06, 0.01, 0.2))
    cyl(t, P, (0.065, 0.0, -0.3), (0.14, 0.0, -0.3), 0.011, seg=8)                              # cocking lever
    # gas cylinder under the barrel, held by a band
    cyl(t, P, (0.0, -0.055, 0.3), (0.0, -0.055, 1.05), 0.016, seg=10)
    box(t, P, (0.0, -0.035, 1.05), (0.03, 0.06, 0.04))
    # ring sight and rear peep on a bracket off the left of the receiver top
    for z in (0.22, -0.35):
        box(t, P, (-0.11, 0.121, z), (0.13, 0.012, 0.03))
    ring_sight(t, DARK, (-0.17, 0.3, 0.22), 0.07, post_to=(-0.17, 0.121, 0.22))
    box(t, DARK, (-0.17, 0.175, -0.35), (0.02, 0.11, 0.02))
    box(t, DARK, (-0.17, 0.23, -0.35), (0.035, 0.035, 0.008))
    # the shield: one plate with chamfered top corners, cut from below for the barrel and the cradle
    sh = [(-0.31, -0.17), (-0.08, -0.17), (-0.08, -0.01), (-0.05, -0.01), (-0.05, 0.055), (0.05, 0.055), (0.05, -0.01),
          (0.08, -0.01), (0.08, -0.17), (0.22, -0.17), (0.22, 0.13), (0.17, 0.18), (-0.26, 0.18), (-0.31, 0.13)]
    flat_shield(t, P, sh, 0.335, thick=0.008)
    for x in (-0.058, 0.058):                                                                  # brackets to the receiver
        box(t, P, (x, 0.08, 0.315), (0.012, 0.05, 0.04))
    box(t, P, (0.0, 0.112, 0.315), (0.13, 0.012, 0.04))
    shoulder_rests(t, 0.065, 0.11, -0.48)
    lathe(b, OER_BAR, (0.0, 0.0, 0.3), (0, 0, 1), [(0, 0.045), (0.1, 0.045), (0.1, 0.03), (0.2, 0.026), (1.65, 0.02), (1.65, 0.024),
                                            (1.68, 0.024), (1.88, 0.04), (1.88, 0.032), (1.86, 0.0)], seg=14)
    return m


def madsen():
    """Madsen 20 mm: a long slim receiver with the flat round drum lying on top, a long barrel with the big ported
    muzzle brake, the padded shoulder stocks of the light AA mount, a pistol grip with the trigger and a ring sight on a
    bracket to the left; no shield (the Finnish MTB guns had none)."""
    m = Model('madsen_20mm', 'tub20', item='Madsen 20 mm')
    t, b = m.turret(), m.barrel()
    P = OER
    chamfer_box(t, P, (0.0, 0.01, -0.1), (0.11, 0.15, 0.95), 0.025)                             # receiver
    box(t, P, (0.0, -0.08, -0.33), (0.045, 0.05, 0.18))
    lathe(t, P, (0.0, 0.01, -0.575), (0, 0, -1), [(0, 0.045), (0.02, 0.045), (0.05, 0.0)], seg=12)
    cyl(t, P, (0.055, 0.03, -0.25), (0.13, 0.03, -0.25), 0.01, seg=8)                           # cocking handle
    extrude(t, LEATHER, [(-0.42, -0.1), (-0.38, -0.1), (-0.395, -0.17), (-0.435, -0.17)], (0, 0, 1), (0, 1, 0), (1, 0, 0), -0.015, 0.015)
    tube(t, P, [(0, -0.1, -0.31), (0, -0.13, -0.315), (0, -0.135, -0.345), (0, -0.12, -0.375), (0, -0.1, -0.38)], 0.004, seg=6)
    # the drum: a flat disc on a feed neck, ribbed, with a hub and a winding key
    box(t, P, (0.0, 0.1, -0.05), (0.06, 0.05, 0.1))
    lathe(t, OER_MAG, (0.0, 0.115, -0.05), (0, 1, 0), [(0, 0.0), (0, 0.19), (0.008, 0.2), (0.075, 0.2), (0.083, 0.19),
                                                       (0.086, 0.05), (0.1, 0.04), (0.104, 0.0)], seg=32)
    for k in range(10):
        a = 2 * math.pi * k / 10
        box(t, OER_MAG, (0.12 * math.sin(a), 0.2, -0.05 + 0.12 * math.cos(a)), (0.01, 0.006, 0.12), (0, math.degrees(a), 0))
    box(t, DARK, (0.0, 0.225, -0.05), (0.07, 0.012, 0.012))
    # barrel and the big muzzle brake (ported rings)
    lathe(b, OER_BAR, (0.0, 0.0, 0.38), (0, 0, 1), [(0, 0.05), (0.12, 0.05), (0.12, 0.03), (0.22, 0.026), (1.4, 0.021)], seg=14, caps=False)
    prof = [(1.4, 0.021), (1.4, 0.045)]
    z = 1.4
    for k in range(5):
        prof += [(z + 0.035, 0.045), (z + 0.035, 0.034), (z + 0.05, 0.034), (z + 0.05, 0.045)]
        z += 0.05
    prof += [(z + 0.03, 0.045), (z + 0.03, 0.0)]
    lathe(b, P, (0.0, 0.0, 0.38), (0, 0, 1), prof, seg=14)
    box(b, P, (0.0, 0.035, 0.6), (0.005, 0.03, 0.012))                                         # barrel sight
    # ring sight and rear peep on brackets off the left of the receiver top (clear of the drum)
    for z in (0.3, -0.42):
        box(t, P, (-0.12, 0.091, z), (0.15, 0.012, 0.03))
    ring_sight(t, DARK, (-0.19, 0.27, 0.3), 0.075, spokes=6, post_to=(-0.19, 0.091, 0.3))
    box(t, DARK, (-0.19, 0.14, -0.42), (0.02, 0.1, 0.02))
    box(t, DARK, (-0.19, 0.19, -0.42), (0.035, 0.035, 0.008))
    shoulder_rests(t, 0.055, 0.1, -0.5)
    return m


def polsten():
    """Polsten: the Oerlikon simplified, so the stock barrel, receiver, spring and sights stay (model_keep) and only the
    drum is swapped for the straight 30-round box standing on top."""
    keep = ['Mount', 'Mount3_Holder', 'ArmRest', 'Oerlikon Barrel', 'Oerlikon Recoil', 'Oerlikon Spring', 'Oerlikon Sights']
    m = Model('polsten_20mm', 'tub20', keep=keep, item='Polsten 20 mm')
    t = m.turret()
    c = np.array([0.01, 0.33, 0.2])
    a = math.radians(-6)                                     # leaning back a little, as in the photos
    box(t, OER, (0.01, 0.09, 0.2), (0.06, 0.08, 0.26))                                          # magazine well
    box(t, OER_MAG, c, (0.07, 0.42, 0.22), (math.degrees(a), 0, 0))
    for dz in (-0.07, 0.0, 0.07):                                                              # pressed ribs both sides
        for sx in (-1, 1):
            box(t, OER_MAG, c + (sx * 0.037, 0.0, dz), (0.006, 0.36, 0.014), (math.degrees(a), 0, 0))
    box(t, OER_MAG, c + (0.0, 0.215, -0.02), (0.08, 0.02, 0.24), (math.degrees(a), 0, 0))       # base plate
    box(t, OER, (0.045, 0.1, 0.06), (0.03, 0.04, 0.04))                                          # catch
    return m


# ---- TUB TURRET: on the Lewis ring mount ----------------------------------------------------------------------------------
def mg34():
    """Twin MG 34 on the Zwillingssockel: perforated barrel jackets, a saddle drum straddling each receiver, butt stocks
    and pistol grips; a cradle and ring sight between the guns."""
    m = Model('twin_mg34', 'lewis', item='Twin MG 34')
    b = m.barrel()                                         # gun 0 (x = 0); the mod copies it onto gun 1 (x = 0.2)
    M = LEWIS_METAL
    chamfer_box(b, M, (0, 0.0, -0.265), (0.05, 0.085, 0.29), 0.012)                              # receiver
    box(b, M, (0, 0.01, -0.075), (0.046, 0.065, 0.09))              # its front, slimmer below to clear the mount block
    box(b, M, (0, 0.046, -0.24), (0.044, 0.012, 0.26))                                          # feed cover
    # barrel jacket with rows of holes, then the muzzle booster and front sight
    lathe(b, M, (0, 0.0, -0.03), (0, 0, 1), [(0, 0.024), (0.4, 0.023), (0.4, 0.0)], seg=12)
    for k in range(6):
        a = 2 * math.pi * k / 6 + math.pi / 6
        for z in (0.06, 0.13, 0.2, 0.27, 0.34):
            box(b, DARK, (0.0234 * math.cos(a), 0.0234 * math.sin(a), z), (0.003, 0.012, 0.026), (0, 0, math.degrees(a)))
    lathe(b, M, (0, 0.0, 0.37), (0, 0, 1), [(0, 0.016), (0.08, 0.016), (0.08, 0.022), (0.14, 0.022), (0.14, 0.0)], seg=12)
    box(b, M, (0, 0.03, 0.34), (0.005, 0.02, 0.01))
    # the saddle drum: two drums either side of the receiver joined over the top
    for sx in (-1, 1):
        lathe(b, M, (sx * 0.052, 0.015, -0.31), (0, 0, 1), [(0, 0.0), (0, 0.04), (0.006, 0.042), (0.12, 0.042), (0.126, 0.04),
                                                             (0.126, 0.0)], seg=16)
    box(b, M, (0, 0.06, -0.25), (0.13, 0.014, 0.1))
    # butt stock and grip in black
    extrude(b, DARK, [(-0.41, -0.03), (-0.41, 0.03), (-0.62, 0.025), (-0.66, -0.0), (-0.66, -0.09), (-0.62, -0.09)],
            (0, 0, 1), (0, 1, 0), (1, 0, 0), -0.018, 0.018)
    extrude(b, DARK, [(-0.33, -0.04), (-0.3, -0.04), (-0.33, -0.15), (-0.37, -0.15)], (0, 0, 1), (0, 1, 0), (1, 0, 0), -0.014, 0.014)
    tube(b, M, [(0, -0.04, -0.26), (0, -0.08, -0.265), (0, -0.085, -0.29), (0, -0.06, -0.31), (0, -0.04, -0.31)], 0.004, seg=6)
    box(b, M, (0, -0.052, -0.3), (0.03, 0.02, 0.06))                                            # mount lug
    t = m.turret()                         # the twin cradle behind the stock mount block, the sight on a jacket clamp
    box(t, M, (0.10, -0.055, -0.24), (0.26, 0.025, 0.18))
    box(t, M, (0.10, -0.035, -0.09), (0.04, 0.03, 0.12))
    cyl(t, M, (-0.04, -0.03, 0.0), (0.24, -0.03, 0.0), 0.045, seg=14)      # trunnion on the mount's axis, down onto its block
    box(t, M, (0.10, 0.0, 0.1), (0.2, 0.012, 0.02))                                             # clamp across the jackets
    ring_sight(t, M, (0.10, 0.14, 0.1), 0.06, post_to=(0.10, 0.0, 0.1))
    box(t, M, (0.10, 0.11, -0.42), (0.016, 0.04, 0.014))                                        # rear sight
    cyl(t, M, (0.10, 0.09, -0.42), (0.10, -0.05, -0.3), 0.005, seg=5)
    # Doppelgriffstueck: one bar linking both triggers so either grip fires the pair
    cyl(t, M, (0.0, -0.075, -0.335), (0.2, -0.075, -0.335), 0.006, seg=6)
    for part in m.parts:                   # authored with the bore at y 0; the stock Lewis bores sit 3 cm higher
        part.tris = [(pa, [q + (0, 0.03, 0) for q in ps], ns, uv) for pa, ps, ns, uv in part.tris]
    return m


# ---- TUB TURRET: on the M2 ring mount (belts kept) ------------------------------------------------------------------------
def dshk():
    """Twin DShK on the MSTU: receivers with the round rotary feed cover on top, finned barrels with the flat-ring muzzle
    brakes, each gun's own spade grips and butterfly trigger on its back plate, the mount's side handles where the crew's
    hands are, a ring sight on a bridge between the receivers and the small flat shield, cut down for both barrels, on
    brackets from the receivers. The guns sit on the stock barrels' lines (x -0.261 and -0.025), so the muzzle flashes
    line up; the stock M2 cradle and belts stay."""
    m = Model('twin_dshk', 'm2', item='Twin DShK')
    b = m.barrel()
    X = (-0.261, -0.025)
    for x in X:
        chamfer_box(b, METAL, (x, -0.01, -0.29), (0.1, 0.13, 0.56), 0.014)                    # receiver
        box(b, METAL, (x, 0.062, -0.36), (0.084, 0.012, 0.36))
        box(b, METAL, (x, -0.01, -0.575), (0.105, 0.11, 0.02))                                 # back plate
        # rotary feed cover: a squat drum on the top cover
        lathe(b, METAL, (x, 0.068, -0.13), (0, 1, 0), [(0, 0.0), (0, 0.06), (0.04, 0.06), (0.045, 0.05), (0.05, 0.0)], seg=18)
        box(b, METAL, (x + 0.045, 0.08, -0.13), (0.03, 0.02, 0.05))
        # barrel: fins over the rear half, the gas tube, the flat-ring muzzle brake
        prof = [(0.0, 0.03), (0.02, 0.03), (0.02, 0.018)]
        z = 0.04
        while z < 0.5:
            prof += [(z, 0.018), (z, 0.034), (z + 0.01, 0.034), (z + 0.01, 0.018)]
            z += 0.035
        prof += [(1.05, 0.015), (1.05, 0.022), (1.08, 0.022), (1.08, 0.05), (1.1, 0.05), (1.1, 0.022), (1.14, 0.022),
                 (1.14, 0.05), (1.16, 0.05), (1.16, 0.0)]
        lathe(b, METAL, (x, 0.0, 0.0), (0, 0, 1), prof, seg=10)
        cyl(b, METAL, (x, -0.045, 0.0), (x, -0.045, 0.62), 0.012, seg=8)
        box(b, METAL, (x, -0.028, 0.62), (0.03, 0.05, 0.035))
        box(b, METAL, (x, 0.035, 0.98), (0.005, 0.03, 0.012))                                  # front sight
        # spade grips: two short handles on a yoke behind the back plate, the butterfly trigger between them
        box(b, METAL, (x, -0.01, -0.6), (0.1, 0.02, 0.03))
        for gx in (x - 0.04, x + 0.04):
            lathe(b, LEATHER, (gx, 0.035, -0.62), (0, -1, 0), [(0, 0.012), (0.09, 0.012)], seg=8)
        box(b, METAL, (x, 0.0, -0.62), (0.05, 0.03, 0.006))
    for hx, gx in ((-0.43, -0.311), (0.14, 0.025)):                                             # the mount's side handles
        cyl(b, METAL, (gx, 0.0, -0.53), (hx, 0.0, -0.53), 0.012, seg=8)
        lathe(b, LEATHER, (hx, 0.07, -0.53), (0, -1, 0), [(0, 0.016), (0.14, 0.016)], seg=10)
    # ring sight on a bridge across the receiver tops, rear peep on a second bridge
    cx = sum(X) / 2
    for z in (-0.05, -0.48):
        box(b, METAL, (cx, 0.062, z), (X[1] - X[0] + 0.06, 0.014, 0.03))
    ring_sight(b, METAL, (cx, 0.2, -0.05), 0.07, post_to=(cx, 0.062, -0.05))
    box(b, METAL, (cx, 0.11, -0.48), (0.015, 0.08, 0.015))
    box(b, METAL, (cx, 0.15, -0.48), (0.03, 0.03, 0.006))
    # the small shield ahead of the receivers, cut down past the fins and the gas tube of both barrels
    n = 0.045
    sh = [(-0.47, -0.16), (0.19, -0.16), (0.19, 0.08), (0.15, 0.12), (X[1] + n, 0.12), (X[1] + n, -0.07), (X[1] - n, -0.07),
          (X[1] - n, 0.12), (X[0] + n, 0.12), (X[0] + n, -0.07), (X[0] - n, -0.07), (X[0] - n, 0.12), (-0.43, 0.12), (-0.47, 0.08)]
    flat_shield(b, METAL, sh, 0.2, thick=0.008)
    for x in X:                                                                                 # brackets from the receivers
        for sx in (-1, 1):
            box(b, METAL, (x + sx * 0.056, -0.03, 0.095), (0.012, 0.03, 0.21))
    return m


MODELS = [drilling, breda, madsen, polsten, mg34, dshk]


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
            tm.detail_sheet(built[i:i + 3], prev / ('sg2_medium_models_%d.png' % (i // 3 + 1)))
            if '--closeups' in sys.argv:
                closeup_sheet(built[i:i + 3], prev / ('sg2_medium_closeups_%d.png' % (i // 3 + 1)))


if __name__ == '__main__':
    main()
