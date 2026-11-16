"""Crew-weapon side-grades, second batch (2026-11-15): Stinger, M1 Garand, Bren, PIAT, Type 89 'knee mortar'.

Same method as make_sidegrade_models.py: parts are authored in the base gun's 'weapon' frame (metres, +Y up, +Z forward),
the whole stock model is hidden (keep=[]), and every part uses the base gun's own material. The handheld bases' textures
(M1918 BAR, M1919 Handheld, M1 Garand) all share the StandardColor5 palette layout, so a game mesh can move between them
with its UVs (the Garand is the game's own, taken from the M7 Grenade Launcher prefab).

Usage: python sg2_crew.py <models folder> [--preview <folder>] [--only name,name] [--closeups]
       (needs the game installed and UnityPy; run python WITHOUT -I)
"""
import math
import pathlib
import sys

import numpy as np

import gamemesh as gm
import make_turret_models as tm
import make_sidegrade_models as sg
from make_sidegrade_models import *                     # noqa: F401,F403  (Model, paints, shapes)

BLACK, BLUE = (0.92, 0.93), (0.78, 0.58)               # the palette's Color5 (black dot) and Color4 (blue dot)

tm.BASES.update({
    'm1919h': dict(prefab='M1919 Handheld', pivot='weapon', barrels=['weapon/Weapon Model'], keep=[]),
    'bazooka': dict(prefab='Bazooka', pivot='weapon', barrels=['weapon/Weapon Model'], keep=[]),
    'm7': dict(prefab='M7 Grenade Launcher', pivot='weapon', barrels=['weapon/Weapon Model'], keep=[]),
})

# M1919 Handheld: its body is the red patch, the grips the green one, the barrel jacket 'Holed 50 Cal Metal' (tiled)
HH_METAL = pal('M1919 Handheld', (0.30, 0.30, 0.32), RED)
HH_DARK = pal('M1919 Handheld', (0.14, 0.14, 0.14), (BLACK[0], BLACK[1]))
HH_GRIP = pal('M1919 Handheld', (0.55, 0.40, 0.30), GREEN)
YH = 0.07                                               # the M1919 Handheld's bore height


def _jacket_uv(p, n):
    a = math.atan2(p[1] - YH, p[0])
    return (a / (2 * math.pi) + 0.5, p[2] * 17.0)       # the stock jacket: one tile round, about 17 a metre along


HOLED = Paint('Holed 50 Cal Metal', (0.42, 0.42, 0.42), _jacket_uv)
# M1 Garand material (the M7's): metal white, dark red, wood green
GAR_METAL = pal('M1 Garand', (0.54, 0.52, 0.50), WHITE)
GAR_DARK = pal('M1 Garand', (0.37, 0.37, 0.37), (BLACK[0], BLACK[1]))
GAR_GREY = pal('M1 Garand', (0.44, 0.40, 0.40), RED)
BAR_BLACK = pal('M1918 BAR', (0.49, 0.49, 0.49), (BLACK[0], BLACK[1]))


def _round(b, paint, p0, d, L=0.075, r=0.0055):
    d = np.asarray(d, float); d /= np.linalg.norm(d)
    lathe(b, paint, p0, d, [(0, r), (L * 0.68, r), (L * 0.8, r * 0.7), (L, r * 0.3), (L, 0.0)], seg=6)


def _strap(b, paint, pts, half_w=0.015, t=0.0018):
    """A flat strap (sling, belt) along a polyline in the side plane [(z, y)], its width across the gun (x)."""
    pts = np.array(pts, float)
    side = []
    for i in range(len(pts)):
        d = pts[min(i + 1, len(pts) - 1)] - pts[max(i - 1, 0)]
        side.append(np.array([-d[1], d[0]]) / np.linalg.norm(d) * t)
    outline = [tuple(q + s) for q, s in zip(pts, side)] + [tuple(q - s) for q, s in zip(pts[::-1], side[::-1])]
    for i in range(len(pts) - 1):                       # per segment: a sagging strap outline is not convex
        quad = [outline[i], outline[i + 1], outline[-i - 2], outline[-i - 1]]
        extrude(b, paint, quad, (0, 0, 1), (0, 1, 0), (1, 0, 0), -half_w, half_w)


def _swivel(b, paint, c):
    ring(b, paint, c, (1, 0, 0), 0.008, 0.0018, n=10, seg=4)


# ---- 1. Stinger: the AN/M2 aircraft .30 made handheld, on the M1919 Handheld --------------------------------------------
def stinger():
    """AN/M2 Stinger (Iwo Jima): the slim aircraft receiver, the thin perforated barrel jacket and a muzzle booster, a
    Garand-type wooden butt, a BAR-type bipod folded under the jacket, a pistol grip at the right hand, a carry handle at the
    left hand, and a belt of rounds hanging from the feed on the left."""
    m = Model('stinger_anm2', 'm1919h', item='Stinger')
    b = m.barrel()
    # receiver: a narrow box with the top cover, the back plate and the cocking handle on the right
    chamfer_box(b, HH_METAL, (0, YH + 0.012, 0.29), (0.048, 0.1, 0.4), 0.006)
    box(b, HH_DARK, (0, YH + 0.066, 0.31), (0.05, 0.01, 0.3))                             # top cover
    box(b, HH_DARK, (0, YH + 0.06, 0.465), (0.03, 0.02, 0.02))                            # cover latch
    box(b, HH_METAL, (0, YH + 0.0, 0.085), (0.05, 0.11, 0.02))                            # back plate
    box(b, HH_DARK, (0.03, YH + 0.01, 0.23), (0.012, 0.015, 0.06))                        # charging handle slot
    cyl(b, HH_DARK, (0.03, YH + 0.01, 0.2), (0.07, YH + 0.01, 0.2), 0.007, seg=6)
    # feed on the left: the feed throat and a belt of rounds hanging down from it
    box(b, HH_DARK, (-0.04, YH + 0.035, 0.37), (0.035, 0.03, 0.05))
    for k in range(13):                                  # the 100-round belt hangs off the feed (Stein's gun, Iwo Jima)
        a = min(k * 0.22, 1.25)
        x = -0.06 - 0.012 * min(k, 8)
        y = YH + 0.035 - 0.011 * k * (0.5 + min(k, 8) * 0.12)
        _round(b, BRASS, (x, y, 0.37 - 0.035), (0.0, 0.0, 1.0), 0.075, 0.0052)
        box(b, HH_DARK, (x, y, 0.37), (0.012, 0.004, 0.012), (0, 0, math.degrees(a)))     # belt link
    # barrel jacket (perforated), the barrel showing at the front, the muzzle booster
    lathe(b, HH_METAL, (0, YH, 0.49), (0, 0, 1), [(0, 0.0), (0, 0.026), (0.04, 0.026), (0.04, 0.02)], seg=12)
    lathe(b, HOLED, (0, YH, 0.53), (0, 0, 1), [(0, 0.02), (0.56, 0.02)], seg=12, caps=False)
    lathe(b, HH_METAL, (0, YH, 1.09), (0, 0, 1), [(0, 0.022), (0.03, 0.022), (0.03, 0.012), (0.06, 0.012), (0.06, 0.018),
                                                   (0.15, 0.018), (0.16, 0.012), (0.16, 0.0)], seg=10)
    box(b, HH_DARK, (0, YH + 0.025, 1.12), (0.004, 0.014, 0.01))                         # front sight
    box(b, HH_DARK, (0, YH + 0.07, 0.42), (0.016, 0.02, 0.012))                          # rear sight
    # pistol grip under the back of the receiver, at the right hand, and its trigger guard
    sg._rounded_slab(b, HH_GRIP, [(0.13, YH - 0.035), (0.12, YH - 0.08), (0.112, YH - 0.13), (0.1, YH - 0.145),
                                  (0.08, YH - 0.145), (0.075, YH - 0.13), (0.085, YH - 0.08), (0.09, YH - 0.035)], 0.013)
    tube(b, HH_DARK, [(0, YH - 0.04, 0.15), (0, YH - 0.085, 0.15), (0, YH - 0.09, 0.19), (0, YH - 0.045, 0.2)], 0.003, seg=5)
    # Garand-type butt: a wooden stock from the back plate down to the butt plate
    sg._rounded_slab(b, HH_GRIP, [(0.08, YH + 0.04), (0.08, YH - 0.04), (-0.05, YH - 0.075), (-0.24, YH - 0.105),
                                  (-0.25, YH - 0.1), (-0.25, YH + 0.01), (-0.24, YH + 0.015), (-0.05, YH + 0.025)], 0.019)
    box(b, HH_DARK, (0, YH - 0.045, -0.255), (0.04, 0.12, 0.012))                        # butt plate
    # carry handle over the jacket at the left hand
    tube(b, HH_DARK, [(-0.012, YH + 0.02, 0.54), (-0.03, YH + 0.11, 0.55), (-0.04, YH + 0.13, 0.6), (-0.03, YH + 0.11, 0.65),
                      (-0.012, YH + 0.02, 0.66)], 0.006, seg=6)
    lathe(b, HH_GRIP, (-0.04, YH + 0.13, 0.565), (0, 0, 1), [(0, 0.012), (0.07, 0.012)], seg=8)
    # BAR-type bipod: clamp at the front of the jacket, two legs folded back under it, splayed feet
    ring(b, HH_DARK, (0, YH, 1.02), (0, 0, 1), 0.023, 0.005, n=12, seg=4)
    box(b, HH_DARK, (0, YH - 0.03, 1.02), (0.02, 0.02, 0.025))
    for sx in (-1, 1):
        cyl(b, HH_DARK, (sx * 0.008, YH - 0.035, 1.02), (sx * 0.016, YH - 0.04, 0.68), 0.0045, seg=5)
        box(b, HH_DARK, (sx * 0.02, YH - 0.042, 0.67), (0.012, 0.004, 0.025))
        cyl(b, HH_DARK, (sx * 0.02, YH - 0.044, 0.655), (sx * 0.02, YH - 0.044, 0.64), 0.004, 0.001, seg=5)   # BAR spike feet
    box(b, HH_DARK, (0, YH - 0.015, 1.02), (0.012, 0.012, 0.03))                           # bipod wing nut
    # the Garand butt's sling swivel and the toe of its butt plate
    _swivel(b, HH_DARK, (0, YH - 0.1, -0.18))
    return m


# ---- 2. M1 Garand: the game's own Garand (from the M7), without the launcher, on the BAR --------------------------------
def garand():
    """M1 Garand: the M7 Grenade Launcher prefab's rifle mesh alone (the launcher and grenade are other nodes), moved back
    so its grip sits nearer the BAR's right hand, painted with the BAR's material (same palette layout)."""
    m = Model('m1_garand', 'bar', item='M1 Garand')
    src = gm.Prefab('M7 Grenade Launcher')
    n = game_mesh(m.barrel(), src, 'weapon/Weapon Model/Model_LOD0', 'weapon', mat4((0, -0.004, -0.03)), game_mat='M1918 BAR')
    m.note = 'game mesh: M1 Garand from the M7 (%d triangles)' % n
    b = m.barrel()
    # the M1907 sling, slack, from the swivel on the front band to the one under the butt (the mesh has its sights)
    fz, fy, rz, ry = 0.84, 0.066, 0.1, -0.022
    _swivel(b, BAR_DARK, (0, fy + 0.006, fz)); _swivel(b, BAR_DARK, (0, ry + 0.008, rz))
    pts = [(fz, fy), (fz - 0.004, fy - 0.012)]
    for k in range(1, 12):
        t = k / 12
        pts.append((fz + (rz - fz) * t, fy - 0.012 + (ry - fy + 0.012) * t - 0.07 * math.sin(math.pi * t)))
    pts += [(rz + 0.004, ry - 0.008), (rz, ry)]
    _strap(b, BAR_DARK, pts, 0.016)
    kz, ky = pts[6]
    box(b, BAR_METAL, (0, ky, kz), (0.036, 0.007, 0.014))                                  # the sling's hook buckle
    return m


# ---- 3. Bren Mk I on the BAR --------------------------------------------------------------------------------------------
def bren():
    """Bren Mk I: the metal butt with its strap and rear grip, wooden pistol grip, the square receiver, the curved 30-round
    magazine standing on top with the drum rear sight offset to the left, gas cylinder under the barrel, wooden carry
    handle, conical flash hider, bipod folded back."""
    m = Model('bren_mk1', 'bar', item='Bren Mk I')
    b = m.barrel()
    # butt: a tapered metal butt, the butt plate, the strap on top, the small rear grip underneath
    extrude(b, BAR_METAL, [(0.0, YB - 0.1), (0.0, YB + 0.03), (0.04, YB + 0.035), (0.27, YB + 0.02), (0.27, YB - 0.04), (0.06, YB - 0.1)],
            (0, 0, 1), (0, 1, 0), (1, 0, 0), -0.02, 0.02)
    box(b, BAR_DARK, (0, YB - 0.035, -0.006), (0.045, 0.14, 0.014))
    box(b, BAR_DARK, (0, YB + 0.04, 0.06), (0.03, 0.012, 0.1), (-5, 0, 0))               # butt strap
    cyl(b, BAR_WOOD, (0, YB - 0.1, 0.08), (0, YB - 0.16, 0.07), 0.013, seg=8)            # rear grip
    sg._pistol_grip(b, 0.22, BAR_WOOD)
    # receiver: the square body, the cocking handle on the right, the ejection slot underneath
    chamfer_box(b, BAR_METAL, (0, YB - 0.005, 0.44), (0.05, 0.085, 0.36), 0.008)
    box(b, BAR_DARK, (0.03, YB - 0.01, 0.52), (0.012, 0.014, 0.05))
    box(b, BAR_DARK, (0, YB - 0.05, 0.47), (0.02, 0.006, 0.08))
    # the curved magazine on top, leaning and curving forward: a banana outline in the side plane
    back, front = [], []
    for k in range(7):
        t = k / 6
        a = math.radians(12 + 22 * t)                                                     # the curve leans further forward
        c = (0.445 + 0.07 * t * t + 0.03 * t, YB + 0.045 + 0.17 * t)
        n = (math.cos(a), -math.sin(a))                                                   # forward normal of the centre line
        w = 0.042 - 0.006 * t
        back.append((c[0] - n[0] * w, c[1] - n[1] * w)); front.append((c[0] + n[0] * w, c[1] + n[1] * w))
    extrude(b, BAR_METAL, back + front[::-1], (0, 0, 1), (0, 1, 0), (1, 0, 0), -0.015, 0.015)
    extrude(b, BAR_DARK, [(back[-1][0] - 0.004, back[-1][1] - 0.012), (front[-1][0] + 0.004, front[-1][1] - 0.012),
                          front[-1], back[-1]], (0, 0, 1), (0, 1, 0), (1, 0, 0), -0.017, 0.017)          # base plate of the mag
    box(b, BAR_DARK, (0, YB + 0.05, 0.44), (0.034, 0.012, 0.09))                         # magazine catch housing
    # sights offset to the left of the magazine: the drum rear sight and the front sight blade on a post
    cyl(b, BAR_DARK, (-0.035, YB + 0.055, 0.33), (-0.05, YB + 0.055, 0.33), 0.02, seg=10)
    box(b, BAR_DARK, (-0.035, YB + 0.08, 0.33), (0.008, 0.02, 0.01))
    box(b, BAR_DARK, (-0.02, YB + 0.03, 0.97), (0.02, 0.008, 0.02))                        # front sight bracket
    box(b, BAR_DARK, (-0.028, YB + 0.055, 0.97), (0.006, 0.05, 0.012))
    # barrel nut, the barrel with its cooling fins, the conical flash hider
    lathe(b, BAR_METAL, (0, YB, 0.62), (0, 0, 1), [(0, 0.026), (0.05, 0.026), (0.05, 0.016)] +
          sum([[(0.08 + 0.025 * k, 0.016), (0.08 + 0.025 * k, 0.021), (0.09 + 0.025 * k, 0.021), (0.09 + 0.025 * k, 0.016)] for k in range(5)], []) +
          [(0.47, 0.012), (0.47, 0.014), (0.53, 0.019), (0.53, 0.0)], seg=10)
    # gas cylinder under the barrel to the gas block, the bipod legs folded back under it
    cyl(b, BAR_METAL, (0, YB - 0.035, 0.62), (0, YB - 0.035, 0.93), 0.011, seg=8)
    box(b, BAR_METAL, (0, YB - 0.02, 0.93), (0.024, 0.05, 0.03))
    for sx in (-1, 1):
        cyl(b, BAR_DARK, (sx * 0.012, YB - 0.045, 0.92), (sx * 0.02, YB - 0.055, 0.66), 0.005, seg=5)
        box(b, BAR_DARK, (sx * 0.024, YB - 0.058, 0.655), (0.014, 0.006, 0.03))           # the legs' flat feet
    lathe(b, BAR_DARK, (0, YB - 0.035, 0.945), (0, 0, 1), [(0, 0.0), (0, 0.015), (0.025, 0.015), (0.025, 0.0)], seg=8)   # gas regulator
    box(b, BAR_DARK, (0, YB - 0.035, 0.96), (0.036, 0.006, 0.006))                        # regulator's tommy-bar slot
    box(b, BAR_DARK, (0, YB - 0.01, 0.905), (0.034, 0.008, 0.01))                          # bipod clamp band
    _swivel(b, BAR_DARK, (0, YB - 0.108, 0.04))                                             # sling swivels: butt and bipod
    _swivel(b, BAR_DARK, (0, YB - 0.054, 0.93))
    # carry handle: a wooden grip on an arm from the barrel nut
    cyl(b, BAR_DARK, (0, YB + 0.025, 0.66), (0.0, YB + 0.085, 0.66), 0.006, seg=6)
    lathe(b, BAR_WOOD, (0, YB + 0.09, 0.6), (0, 0, 1), [(0, 0.0), (0, 0.011), (0.01, 0.013), (0.11, 0.013), (0.12, 0.011), (0.12, 0.0)], seg=8)
    return m


# ---- 4. PIAT on the Bazooka -----------------------------------------------------------------------------------------------
YZ = 0.1                                                 # the Bazooka's tube axis height


def piat():
    """PIAT: the long boxy spring tube with the padded butt at the shoulder, the big trigger guard under it, the open
    trough at the front with a bomb lying in it, the sight on the left, the monopod folded under."""
    m = Model('piat_mk1', 'bazooka', item='PIAT')
    b = m.barrel()
    # the body: a tube with a flat top strip and side seam, from the butt pad to the trough
    lathe(b, BAR_METAL, (0, YZ, -0.2), (0, 0, 1), [(0, 0.0), (0, 0.04), (0.01, 0.042), (0.6, 0.042), (0.6, 0.045), (0.63, 0.045),
                                                   (0.63, 0.0)], seg=14)
    box(b, BAR_METAL, (0, YZ + 0.04, 0.1), (0.035, 0.012, 0.58))
    # shoulder pad, its bracket and the cheek rest
    chamfer_box(b, BAR_DARK, (0, YZ - 0.01, -0.265), (0.08, 0.18, 0.05), 0.014)         # rubber-padded butt
    box(b, BAR_DARK, (0, YZ - 0.01, -0.23), (0.04, 0.13, 0.03))
    box(b, BAR_WOOD, (-0.03, YZ + 0.045, -0.11), (0.02, 0.03, 0.14))
    # trigger housing and the long trigger guard under the body, at the right hand
    box(b, BAR_METAL, (0, YZ - 0.06, 0.13), (0.04, 0.05, 0.16))
    tube(b, BAR_DARK, [(0, YZ - 0.08, 0.06), (0, YZ - 0.15, 0.06), (0, YZ - 0.16, 0.12), (0, YZ - 0.15, 0.22), (0, YZ - 0.08, 0.23)], 0.005, seg=6)
    box(b, BAR_DARK, (0, YZ - 0.11, 0.13), (0.008, 0.06, 0.012), (15, 0, 0))             # trigger
    # the trough: two side plates and a curved floor, open on top, with the bomb lying in it (tail fins back)
    for sx in (-1, 1):
        box(b, BAR_METAL, (sx * 0.048, YZ - 0.02, 0.62), (0.006, 0.05, 0.36))
    box(b, BAR_METAL, (0, YZ - 0.045, 0.62), (0.1, 0.008, 0.36))
    for sx in (-1, 1):                                                                    # front horns that hold the bomb's nose
        box(b, BAR_METAL, (sx * 0.048, YZ - 0.005, 0.8), (0.006, 0.08, 0.02))
    lathe(b, BAR_DARK, (0, YZ, 0.43), (0, 0, 1), [(0, 0.0), (0, 0.022), (0.03, 0.02), (0.03, 0.0)], seg=10)   # spigot face
    bomb = (0, YZ + 0.005, 0.46)
    lathe(b, BAR_BLACK, bomb, (0, 0, 1), [(0, 0.0), (0, 0.02), (0.13, 0.02), (0.13, 0.03), (0.17, 0.042), (0.28, 0.042),
                                          (0.33, 0.03), (0.37, 0.0)], seg=12)
    ring(b, BAR_BLACK, (0, YZ + 0.005, 0.465), (0, 0, 1), 0.03, 0.004, n=12, seg=4)        # tail ring
    for k in range(4):
        a = k * math.pi / 2 + math.pi / 4
        box(b, BAR_BLACK, (0.022 * math.cos(a), YZ + 0.005 + 0.022 * math.sin(a), 0.5), (0.004, 0.022, 0.07), (0, 0, math.degrees(a) - 90))
    # sight on the left: a bracket with the aperture plate, the front post on the trough
    box(b, BAR_DARK, (-0.06, YZ + 0.04, 0.28), (0.03, 0.012, 0.05))
    box(b, BAR_DARK, (-0.075, YZ + 0.07, 0.28), (0.006, 0.05, 0.012))                   # aperture sight post
    for k, h in enumerate((0.1, 0.125)):                                                  # two range apertures (70 and 100 yd)
        ring(b, BAR_DARK, (-0.075, YZ + h, 0.28), (0, 0, 1), 0.008, 0.0022, n=10, seg=4)
    box(b, BAR_DARK, (-0.052, YZ + 0.045, 0.79), (0.005, 0.06, 0.01))
    # monopod folded under the body
    box(b, BAR_DARK, (0, YZ - 0.05, 0.36), (0.03, 0.02, 0.03))
    cyl(b, BAR_DARK, (0, YZ - 0.06, 0.36), (0, YZ - 0.065, 0.26), 0.007, seg=6)
    box(b, BAR_DARK, (0, YZ - 0.068, 0.255), (0.04, 0.006, 0.02))
    # left-hand grip: a strap ring around the body where the left hand holds it
    ring(b, BAR_DARK, (0, YZ, 0.4), (0, 0, 1), 0.046, 0.005, n=14, seg=4)
    _swivel(b, BAR_DARK, (0, YZ - 0.05, -0.12)); _swivel(b, BAR_DARK, (0, YZ - 0.055, 0.5))   # sling swivels
    return m


# ---- 5. Type 89 grenade discharger on the M7 ------------------------------------------------------------------------------
YM = 0.06                                                # below the M7's bore (0.10), so the rod meets the right hand


def type89():
    """Type 89 50 mm grenade discharger: the curved base plate (at the hip), the rod body with the range-setting knob and
    the trigger lever, the short rifled tube in front. 61 cm long, as the real one."""
    m = Model('type89_discharger', 'm7', item='Type 89 grenade discharger')
    b = m.barrel()
    z0 = 0.05
    # curved base plate: a bent plate across the rod's end, as a row of tilted slabs
    for k in range(-3, 4):
        a = k * 12
        box(b, GAR_GREY, (0.035 * math.sin(math.radians(a)) * 1.2, YM - 0.005, z0 + 0.012 - 0.03 * (1 - math.cos(math.radians(a)))),
            (0.022, 0.09, 0.008), (0, a, 0))
    box(b, GAR_GREY, (0, YM - 0.005, z0 + 0.02), (0.02, 0.07, 0.03))                     # the plate's boss
    # the rod body: a thick rod, the threaded collar, the range knob and the scale, the trigger lever on the right hand
    cyl(b, GAR_METAL, (0, YM, z0 + 0.03), (0, YM, z0 + 0.3), 0.012, seg=10)
    lathe(b, GAR_METAL, (0, YM, z0 + 0.18), (0, 0, 1), [(0, 0.018), (0.1, 0.018), (0.1, 0.024), (0.14, 0.024), (0.14, 0.0)], seg=12)
    for k in range(5):
        ring(b, GAR_DARK, (0, YM, z0 + 0.19 + 0.018 * k), (0, 0, 1), 0.018, 0.0025, n=12, seg=4)
    lathe(b, GAR_DARK, (0, YM - 0.024, z0 + 0.24), (0, -1, 0), [(0, 0.0), (0, 0.022), (0.02, 0.022), (0.02, 0.0)], seg=14)   # range knob
    box(b, GAR_GREY, (0.02, YM + 0.0, z0 + 0.22), (0.006, 0.012, 0.08))                  # range scale
    box(b, GAR_DARK, (0.022, YM - 0.03, z0 + 0.25), (0.008, 0.05, 0.012), (-20, 0, 0))   # trigger lever
    ring(b, GAR_DARK, (0.022, YM - 0.06, z0 + 0.26), (1, 0, 0), 0.009, 0.0025, n=10, seg=4)   # its finger ring
    box(b, GAR_DARK, (0.024, YM + 0.0, z0 + 0.24), (0.004, 0.016, 0.003))               # range index mark
    # the tube: a short barrel with a reinforcing band, the level and line on top
    lathe(b, GAR_METAL, (0, YM, z0 + 0.32), (0, 0, 1), [(0, 0.0), (0, 0.032), (0.03, 0.032), (0.03, 0.029), (0.29, 0.029),
                                                        (0.29, 0.033), (0.3, 0.033), (0.3, 0.025), (0.27, 0.025)], seg=16, caps=False)
    lathe(b, GAR_METAL, (0, YM, z0 + 0.32), (0, 0, -1), [(0, 0.032), (0.0, 0.0)], seg=16)
    box(b, GAR_DARK, (0, YM + 0.032, z0 + 0.45), (0.004, 0.006, 0.18))                   # sighting line
    box(b, GAR_DARK, (0, YM + 0.036, z0 + 0.36), (0.012, 0.008, 0.02))                   # level
    # a carry sling ring
    ring(b, GAR_GREY, (0, YM - 0.035, z0 + 0.34), (1, 0, 0), 0.01, 0.002, n=10, seg=4)
    return m


MODELS = [stinger, garand, bren, piat, type89]


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
            tm.detail_sheet(built[i:i + 3], prev / ('sg2_crew_models_%d.png' % (i // 3 + 1)))
            if '--closeups' in sys.argv:
                sg.closeup_sheet(built[i:i + 3], prev / ('sg2_crew_closeups_%d.png' % (i // 3 + 1)))


if __name__ == '__main__':
    main()
