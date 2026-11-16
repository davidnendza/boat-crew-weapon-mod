"""Builds the models (OBJ + MTL) of the historical side-grades for the other slots, plus preview sheets next to the stock guns.

Same method as make_turret_models.py (whose primitives, paints and Model class this reuses): each model replaces only the
gun on a stock base, model_keep= leaves the base's own mount, stand and hand cranks visible, and every part is painted
with a material the base gun already brings to the boat ('game.<name>' in the .mtl; the Kd colour is the fallback).
The base guns' palette textures (Custom/StandardColor5: Bofors, M3 37 mm, M1A1 howitzer, Mk50, BAR) share one layout:
white patch (0.5, 0.85) = _Color1, green (0.15, 0.72) = _Color3, red (0.15, 0.2) = _Color2.

Parts are authored in the base gun's pivot frame (metres, +Y up, +Z forward, the gun aimed level). The 75 mm howitzer's
pivot is its slide, the others' is 'weapon'. Bases whose barrel list mixes guns (the Elco also lists its two .50s) and the
rocket launchers (no barrels) get no barrel* part, so nothing on them recoils.

Shoot points are not moved by the mod: muzzle flashes stay where the stock gun has them.

Usage: python make_sidegrade_models.py <models folder> [--preview <folder>] [--only name,name]
       (needs the game installed and UnityPy; run python WITHOUT -I)
"""
import math
import pathlib
import sys

import numpy as np

import gamemesh as gm
import make_turret_models as tm
from make_turret_models import Paint, Part, box, box_uv, chamfer_box, cyl, extrude, game_mesh, lathe, mat4, region_uv, ring, tube

WHITE, GREEN, RED = (0.50, 0.85), (0.15, 0.72), (0.15, 0.20)


def pal(game, kd, patch):
    return Paint(game, kd, region_uv(patch, 0.08))


# ---- paints: each base's own material first, then the dark gun metal most boats carry -----------------------------------
DARK = Paint('Dark MG Metal', (0.30, 0.31, 0.33), box_uv(1.0))
BOF = pal(('Bofors Gun Blue', 'Dark MG Metal'), (0.56, 0.62, 0.63), WHITE)          # blue-grey
BOF_GREY = pal(('Bofors Gun Blue', 'Dark MG Metal'), (0.55, 0.55, 0.55), GREEN)
US3 = pal(('US 3-inch Gun Stand Material', 'Dark MG Metal'), (0.55, 0.58, 0.62), WHITE)   # no texture: UVs unused
US5 = pal(('Metal', 'Dark MG Metal'), (0.62, 0.65, 0.70), WHITE)
BRASS = pal(('Ammo', 'Metal Brass', 'Crew Bandolier'), (0.78, 0.62, 0.32), WHITE)
OLIVE = pal(('M3 37 MM AT', 'Dark MG Metal'), (0.25, 0.36, 0.28), WHITE)
AT_GREY = pal(('M3 37 MM AT', 'Dark MG Metal'), (0.40, 0.40, 0.40), RED)
HOW = pal(('M1A1 75 MM Howitzer', 'Dark MG Metal'), (0.34, 0.50, 0.39), WHITE)
HOW_DARK = pal(('M1A1 75 MM Howitzer', 'Dark MG Metal'), (0.21, 0.19, 0.19), GREEN)
RL_DARK = pal(('Rocket Launcher', 'Dark MG Metal'), (0.13, 0.16, 0.19), WHITE)
RL_LIGHT = pal(('Rocket Launcher', 'Gray'), (0.70, 0.70, 0.70), GREEN)
ROCKET = Paint(('Gray', 'Dark MG Metal'), (0.41, 0.41, 0.41), box_uv(1.0))
BAR_METAL = pal('M1918 BAR', (0.40, 0.39, 0.38), WHITE)
BAR_DARK = pal('M1918 BAR', (0.30, 0.28, 0.28), RED)
BAR_WOOD = pal('M1918 BAR', (0.60, 0.30, 0.15), GREEN)
LEATHER = Paint(('Oerlikon Leather', 'Thunderbolt Leather'), (0.55, 0.33, 0.22), box_uv(2.0))

tm.BASES.update({
    # prefab, pivot (GunMount.pivotPoint), barrel transforms (GunMount.barrels), stock parts kept (name prefixes)
    'bofors': dict(prefab='40 mm Bofors', pivot='weapon', barrels=['weapon/Bofors (Barrel)'], keep=['Mount', 'Stand', 'Bofors Hand Crank']),
    'us3': dict(prefab='3"/50 Dual Purpose Gun', pivot='weapon', barrels=['weapon/Weapon Model/US 3-inch Gun Barrel'],
                keep=['Stand', 'US_3in_base', 'Bofors Hand Crank']),
    'us5': dict(prefab='5"/25 Gun', pivot='weapon', barrels=['weapon/Barrel'], keep=['Stand', '5in_wet_mount', '5 inch 25 Hand Crank']),
    'dual20': dict(prefab='20 mm Dual Oerlikon', pivot='weapon', barrels=['weapon/Oerlikon Recoil Piece', 'weapon/Oerlikon Recoil Piece (1)'],
                   keep=['Mount', 'Stand', 'ArmRest']),
    'elco': dict(prefab='Elco Thunderbolt', pivot='weapon', barrels=[], keep=['Stand', 'Dummy Stand', 'Thunderbolt LOD Group']),
    'mortar60': dict(prefab='60 mm M2 Mortar', pivot='weapon', barrels=['weapon/Barrel'], keep=[]),
    'at37': dict(prefab='37 mm Anti-Tank Gun', pivot='weapon', barrels=['weapon/Barrel'], keep=['Stand', 'Crank']),
    'how75': dict(prefab='75 mm Howitzer', pivot='Fine Tuner/Carriage/Slide',
                  barrels=['Fine Tuner/Carriage/Slide/pack_howitzer_75mm_barrel_LOD0'], keep=['Stand', 'pack_howitzer_75mm_carriage', 'Crank']),
    'mk50': dict(prefab='Mk 50 Launcher', pivot='weapon', barrels=[], keep=['Mount']),
    'mk50mod': dict(prefab='Mk 50 Modded', pivot='weapon', barrels=[], keep=['Mount']),
    'bar': dict(prefab='M1918 BAR', pivot='weapon', barrels=['weapon/Weapon Model'], keep=[]),
})


class Model(tm.Model):
    """make_turret_models.Model with the pivot taken from the base (not always 'weapon') and barrel frames without scale
    (the mod gives a part the barrel's orientation but the mount's size)."""

    def __init__(self, name, base, keep=None, item=''):
        self.name, self.base = name, tm.BASES[base]
        self.keep = keep if keep is not None else self.base['keep']
        self.item = item
        self.prefab = gm.Prefab(self.base['prefab'])
        self.parts = []
        P = np.linalg.inv(self.prefab.frame(self.base['pivot']))
        self.B = []
        for b in self.base['barrels']:
            F = P @ self.prefab.frame(b)
            F[:3, :3] /= np.linalg.norm(F[:3, :3], axis=0)
            self.B.append(F)

    def scene(self, new=True):
        pv = self.base['pivot']
        P = self.prefab.frame(pv)
        Pl = np.eye(4); Pl[:3, 3] = P[:3, 3]
        relevel = Pl @ np.linalg.inv(P)
        out = []
        for path in self.prefab.visible_nodes():
            if new and not self.kept(path):
                continue
            tris, mats = self.prefab.mesh(path, '')
            M = relevel if path.startswith(pv + '/') else np.eye(4)
            for si, ps, ns, uv in tris:
                if mats[min(si, len(mats) - 1)] == 'Default-Material':      # the cranks' hidden helper cylinders
                    continue
                out.append((mats[min(si, len(mats) - 1)], ps @ M[:3, :3].T + M[:3, 3], ns @ M[:3, :3].T, uv))
        if new:
            for part in self.parts:
                frames = [Pl] if part.name != 'barrel' else [Pl @ B @ np.linalg.inv(self.B[0]) for B in self.B]
                for F in frames:
                    R, t = F[:3, :3], F[:3, 3]
                    for mat, ps, ns, uv in part.tris:
                        out.append((mat.game[0] if isinstance(mat, Paint) else mat, np.array(ps) @ R.T + t, np.array(ns) @ R.T, np.array(uv)))
        return out


# ---- small shared shapes -----------------------------------------------------------------------------------------------
def along(p0, d, t):
    return np.asarray(p0, float) + np.asarray(d, float) * t


def round_row(p, paint, c, n, pitch, axis_dir, length, r, step_dir):
    """n cartridges side by side: each along axis_dir, spaced along step_dir."""
    c, a, s = (np.asarray(x, float) for x in (c, axis_dir, step_dir))
    for i in range(n):
        q = c + s * pitch * (i - (n - 1) / 2)
        lathe(p, paint, q - a * length / 2, a, [(0, r), (length * 0.7, r), (length * 0.85, r * 0.75), (length, r * 0.25), (length, 0.0)], seg=6)


def handwheel(p, paint, c, axis, R, spokes=4):
    ring(p, paint, c, axis, R, R * 0.12, n=18, seg=5)
    a, u, v = tm.basis(axis)
    for k in range(spokes):
        t = 2 * math.pi * k / spokes
        cyl(p, paint, c, np.asarray(c) + R * (u * math.cos(t) + v * math.sin(t)), R * 0.07, seg=5, caps=False)
    cyl(p, paint, c, np.asarray(c) + a * R * 0.6, R * 0.12, seg=6)


def scale_parts(m, k, c):
    """Scale every part of m by k = (kx, ky, kz) about point c (normals by the inverse transpose)."""
    k, c = np.asarray(k, float), np.asarray(c, float)
    for part in m.parts:
        part.tris = [(mat, [(np.asarray(q) - c) * k + c for q in ps], [tm.unit(np.asarray(n) / k) for n in ns], uv)
                     for mat, ps, ns, uv in part.tris]


def flat_shield(p, paint, outline, z, thick=0.012):
    extrude(p, paint, outline, (1, 0, 0), (0, 1, 0), (0, 0, 1), z - thick / 2, z + thick / 2)


# ---- HEAVY: on the Bofors ----------------------------------------------------------------------------------------------
def pompom():
    """QF 2-pdr Mk VIII on the quad Mk VII mount: four water-cooled guns in two tiers of two, one pair above the other
    (Discord, 2026-11-01), cone flash hiders, belts fed from trays on the outboard sides."""
    m = Model('pompom_quad', 'bofors', item='QF 2-pdr quad pom-pom')
    y0 = 0.075
    xs = (-0.2, 0.2)
    ys = (y0 - 0.12, y0 + 0.28)                                                        # lower tier near the Bofors bore
    t, b = m.turret(), m.barrel()
    for y in ys:
        for x in xs:
            out = 1 if x > 0 else -1
            chamfer_box(t, BOF, (x, y - 0.04, -0.05), (0.15, 0.28, 0.95), 0.025)       # receiver
            box(t, BOF_GREY, (x, y + 0.105, -0.08), (0.12, 0.02, 0.8))                 # top cover
            lathe(t, BOF, (x, y - 0.03, -0.52), (0, 0, -1), [(0, 0.06), (0.05, 0.05), (0.07, 0.0)], seg=10)
            # belt tray on the outboard side and the rounds in it, feeding in sideways
            box(t, BOF_GREY, (x + out * 0.11, y - 0.02, 0.05), (0.07, 0.04, 0.34))
            round_row(t, BRASS, (x + out * 0.11, y + 0.015, 0.05), 6, 0.05, (1, 0, 0), 0.1, 0.018, (0, 0, 1))
            # water jacket, ribbed, then the barrel and the cone flash hider
            prof = [(0.0, 0.07), (0.08, 0.07), (0.08, 0.064)]
            z = 0.12
            while z < 1.05:
                prof += [(z, 0.064), (z, 0.07), (z + 0.03, 0.07), (z + 0.03, 0.064)]
                z += 0.18
            prof += [(1.15, 0.064), (1.2, 0.05), (1.2, 0.03), (1.42, 0.028), (1.42, 0.034), (1.66, 0.072), (1.66, 0.06), (1.64, 0.0)]
            lathe(b, BOF, (x, y, 0.45), (0, 0, 1), prof, seg=12)
            cyl(b, BOF_GREY, (x, y + 0.07, 0.6), (x, y + 0.1, 0.6), 0.016, seg=8)        # filler cap
    # the cradle: side plates holding both tiers, a cross member under each tier, trunnion stubs out to the mount
    for sx in (-1, 1):
        box(t, BOF_GREY, (sx * 0.44, y0 + 0.06, -0.15), (0.04, 0.72, 0.4))
        cyl(t, BOF, (sx * 0.46, -0.02, 0.0), (sx * 0.6, -0.02, 0.0), 0.07, seg=12)
    for y in ys:
        box(t, BOF_GREY, (0, y - 0.2, -0.05), (0.86, 0.04, 0.7))
        box(t, BOF_GREY, (0, y - 0.04, 0.4), (0.86, 0.07, 0.06))
    # ring-and-bead sight on the left, over the cranks
    c = np.array([-0.62, 0.38, 0.25])
    for R_ in (0.08, 0.04):
        ring(t, DARK, c, (0, 0, 1), R_, 0.004)
    cyl(t, DARK, (-0.62, 0.30, 0.25), (-0.52, 0.12, 0.25), 0.01, seg=6)
    box(t, DARK, (-0.62, 0.36, -0.3), (0.02, 0.05, 0.02))
    return m


def flak_m42():
    """3.7 cm Flak M42 (S-boat): a tall receiver with the 8-round clip fed from the left, a long barrel in a recoil sleeve
    and a slotted muzzle brake."""
    m = Model('flak_m42', 'bofors', item='3.7 cm Flak M42')
    y0 = 0.075
    t, b = m.turret(), m.barrel()
    chamfer_box(t, BOF, (0, y0 - 0.06, -0.08), (0.24, 0.42, 1.05), 0.03)
    box(t, BOF_GREY, (0, y0 + 0.16, -0.12), (0.2, 0.03, 0.9))
    lathe(t, BOF, (0, y0 - 0.05, -0.6), (0, 0, -1), [(0, 0.09), (0.06, 0.07), (0.09, 0.0)], seg=12)
    # clip guide on the left with a clip of eight rounds lying across it
    box(t, BOF_GREY, (-0.2, y0 + 0.06, -0.05), (0.18, 0.025, 0.36))
    box(t, BOF_GREY, (-0.2, y0 + 0.12, -0.21), (0.18, 0.1, 0.02))
    round_row(t, BRASS, (-0.2, y0 + 0.1, -0.05), 8, 0.042, (1, 0, 0), 0.17, 0.019, (0, 0, 1))
    box(t, BOF_GREY, (0.15, y0 - 0.15, -0.2), (0.08, 0.14, 0.3))                         # ejection chute
    # cradle arms out to the trunnions
    box(t, BOF_GREY, (0, y0 - 0.3, -0.05), (0.32, 0.06, 0.8))
    for sx in (-1, 1):
        box(t, BOF, (sx * 0.3, -0.03, 0.0), (0.28, 0.12, 0.16))
        cyl(t, BOF, (sx * 0.44, -0.02, 0.0), (sx * 0.6, -0.02, 0.0), 0.07, seg=12)
    # recoil sleeve and barrel (recoils), muzzle brake
    lathe(b, BOF, (0, y0, 0.45), (0, 0, 1), [(0, 0.085), (0.65, 0.08), (0.65, 0.05), (0.72, 0.045), (2.8, 0.036),
                                            (2.8, 0.062), (3.05, 0.062), (3.05, 0.0)], seg=16)
    for k in range(4):
        a = k * math.pi / 2 + math.pi / 4
        box(b, DARK, (0.063 * math.cos(a), y0 + 0.063 * math.sin(a), 3.4), (0.012, 0.012, 0.16), (0, 0, math.degrees(a)))
    for z in (0.75, 0.95):
        ring(b, BOF_GREY, (0, y0, z), (0, 0, 1), 0.05, 0.008, n=16)
    # cartwheel sight on the left
    c = np.array([-0.45, 0.42, 0.3])
    for R_ in (0.1, 0.06, 0.03):
        ring(t, DARK, c, (0, 0, 1), R_, 0.004)
    for k in range(6):
        a = k * math.pi / 3
        cyl(t, DARK, c, c + 0.1 * np.array([math.cos(a), math.sin(a), 0]), 0.0025, seg=4)
    cyl(t, DARK, (-0.45, 0.32, 0.3), (-0.22, 0.12, 0.1), 0.01, seg=6)
    return m


# ---- HEAVY / HEAVY+: on the 3-inch ---------------------------------------------------------------------------------------
Y3, X3 = 0.22, -0.04                       # the 3-inch bore in its pivot frame


def molins():
    """QF 6-pdr with the Molins autoloader (Fairmile D): a long plain barrel and the tall six-round hopper over the breech.
    Keeps the stock 3-inch shield."""
    m = Model('molins_6pdr', 'us3', keep=['Stand', 'US_3in_base', 'US_3in_shield', 'Bofors Hand Crank'], item='QF 6-pdr Molins')
    t, b = m.turret(), m.barrel()
    lathe(b, US3, (X3, Y3, -0.55), (0, 0, 1), [(0, 0.12), (0.62, 0.12), (0.62, 0.075), (0.85, 0.065), (0.85, 0.055), (3.2, 0.045),
                                              (3.2, 0.052), (3.26, 0.052), (3.26, 0.0)], seg=16)
    chamfer_box(b, US3, (X3, Y3, -0.35), (0.3, 0.3, 0.5), 0.04)                              # breech ring
    # cradle and recoil cylinders under the barrel
    chamfer_box(t, US3, (X3, Y3 - 0.17, 0.1), (0.26, 0.14, 1.1), 0.03)
    for sx in (-1, 1):
        cyl(t, US3, (X3 + sx * 0.08, Y3 - 0.12, -0.3), (X3 + sx * 0.08, Y3 - 0.12, 0.75), 0.04, seg=10)
        box(t, US3, (X3 + sx * 0.22, Y3 - 0.2, 0.0), (0.12, 0.18, 0.2))                      # trunnion blocks
    # the Molins hopper: a tall box over the breech, a window strip of rounds, the feed motor on the side
    chamfer_box(t, US3, (X3, Y3 + 0.42, -0.4), (0.34, 0.5, 0.62), 0.04, axis=0)
    box(t, DARK, (X3 + 0.172, Y3 + 0.42, -0.4), (0.006, 0.36, 0.48))
    for k in range(6):
        cyl(t, BRASS, (X3 + 0.176, Y3 + 0.27 + 0.06 * k, -0.6), (X3 + 0.176, Y3 + 0.27 + 0.06 * k, -0.2), 0.022, seg=8)
    cyl(t, US3, (X3 - 0.17, Y3 + 0.3, -0.55), (X3 - 0.28, Y3 + 0.3, -0.55), 0.07, seg=12)
    box(t, US3, (X3, Y3 + 0.2, -0.05), (0.2, 0.1, 0.16))
    box(t, DARK, (X3 - 0.32, Y3 + 0.18, -0.1), (0.05, 0.07, 0.3))                            # sight
    return m


def flak88():
    """8.8 cm Flak 36 (Discord 2026-11-15: 'Too bad there's no Flak 88'), as on Siebel ferries and F-lighters: the long
    L/56 barrel with no muzzle brake, the recuperator above it and the buffer below, the two balancing cylinders at the
    front of the cradle, handwheels both sides, the fuze setter on the right. On the 3-inch pedestal; keeps its shield."""
    m = Model('flak88', 'us3', keep=['Stand', 'US_3in_base', 'US_3in_shield', 'Bofors Hand Crank'], item='8.8 cm Flak 36')
    t, b = m.turret(), m.barrel()
    lathe(b, US3, (X3, Y3, -0.75), (0, 0, 1), [(0, 0.13), (0.5, 0.13), (0.5, 0.1), (0.75, 0.085), (1.3, 0.075), (4.3, 0.058),
                                             (4.3, 0.064), (4.36, 0.064), (4.36, 0.0)], seg=16)
    chamfer_box(b, US3, (X3, Y3, -0.55), (0.32, 0.32, 0.42), 0.04)                           # breech ring
    cyl(b, DARK, (X3 + 0.16, Y3 + 0.05, -0.6), (X3 + 0.26, Y3 + 0.05, -0.6), 0.03, seg=8)    # breech lever
    # the recuperator above the barrel and the recoil buffer below, on a long cradle
    cyl(t, US3, (X3, Y3 + 0.17, -0.45), (X3, Y3 + 0.17, 1.35), 0.07, seg=14)
    lathe(t, US3, (X3, Y3 + 0.17, 1.35), (0, 0, 1), [(0, 0.07), (0.05, 0.05), (0.07, 0.0)], seg=14)
    cyl(t, US3, (X3, Y3 - 0.15, -0.4), (X3, Y3 - 0.15, 1.0), 0.06, seg=12)
    chamfer_box(t, US3, (X3, Y3 - 0.06, 0.25), (0.3, 0.22, 1.4), 0.03)
    for sx in (-1, 1):
        box(t, US3, (X3 + sx * 0.2, Y3 - 0.16, 0.0), (0.1, 0.22, 0.26))                       # trunnion blocks
        # balancing cylinders: tall springs angled down from the front of the cradle
        cyl(t, US3, (X3 + sx * 0.24, Y3 - 0.02, 0.75), (X3 + sx * 0.24, Y3 - 0.55, 0.45), 0.065, seg=12)
        cyl(t, DARK, (X3 + sx * 0.24, Y3 - 0.55, 0.45), (X3 + sx * 0.24, Y3 - 0.68, 0.38), 0.03, seg=8)
    handwheel(t, DARK, (X3 - 0.42, Y3 - 0.2, -0.25), (1, 0, 0), 0.13)                         # elevation, left
    handwheel(t, DARK, (X3 + 0.42, Y3 - 0.2, -0.25), (1, 0, 0), 0.13)                         # traverse, right
    box(t, US3, (X3 + 0.34, Y3 - 0.05, -0.75), (0.16, 0.28, 0.24))                            # fuze setter
    box(t, US3, (X3 + 0.2, Y3 - 0.02, -0.95), (0.14, 0.03, 0.4))                              # loading tray
    box(t, DARK, (X3 - 0.33, Y3 + 0.12, -0.15), (0.07, 0.09, 0.36))                           # sight
    cyl(t, DARK, (X3 - 0.33, Y3 + 0.12, -0.33), (X3 - 0.33, Y3 + 0.12, -0.42), 0.028, seg=8)
    # Discord 2026-11-15 'the 8.8 is too small': to the real gun's size, barrel L/56 = 4.9 m, everything else a third thicker
    scale_parts(m, (1.3, 1.3, 1.12), (X3, Y3, 0))
    return m


def type3_80():
    """8 cm/40 Type 3 (Japanese): a slender barrel, recoil cylinders above and below, the breech with a loading tray and
    the two spring cylinders along the cradle. No shield."""
    m = Model('type3_80mm', 'us3', item='8 cm Type 3')
    t, b = m.turret(), m.barrel()
    lathe(b, US3, (X3, Y3, -0.6), (0, 0, 1), [(0, 0.11), (0.5, 0.11), (0.5, 0.07), (0.7, 0.06), (0.7, 0.05), (3.35, 0.04),
                                             (3.35, 0.05), (3.42, 0.05), (3.42, 0.0)], seg=16)
    box(b, US3, (X3, Y3, -0.45), (0.25, 0.25, 0.32))                                          # breech block
    cyl(b, DARK, (X3 + 0.13, Y3, -0.42), (X3 + 0.2, Y3, -0.42), 0.03, seg=8)                  # breech lever
    for dy in (0.13, -0.13):                                                                 # recoil cylinders
        cyl(t, US3, (X3, Y3 + dy, -0.35), (X3, Y3 + dy, 0.85), 0.045, seg=12)
        lathe(t, US3, (X3, Y3 + dy, 0.85), (0, 0, 1), [(0, 0.045), (0.04, 0.03), (0.05, 0.0)], seg=12)
    box(t, US3, (X3, Y3 - 0.06, 0.15), (0.2, 0.34, 0.5))                                      # cradle
    for sx in (-1, 1):
        cyl(t, US3, (X3 + sx * 0.17, Y3 - 0.05, -0.55), (X3 + sx * 0.17, Y3 - 0.05, 0.35), 0.035, seg=10)
        box(t, US3, (X3 + sx * 0.2, Y3 - 0.2, 0.05), (0.08, 0.2, 0.2))
    box(t, US3, (X3 + 0.22, Y3 - 0.02, -0.75), (0.16, 0.03, 0.35))                            # loading tray
    box(t, DARK, (X3 - 0.3, Y3 + 0.12, -0.1), (0.06, 0.08, 0.36))                              # sight
    cyl(t, DARK, (X3 - 0.3, Y3 + 0.12, -0.28), (X3 - 0.3, Y3 + 0.12, -0.36), 0.025, seg=8)
    return m


# ---- HEAVY+: on the 5-inch ---------------------------------------------------------------------------------------------
def qf47():
    """QF 4.7-inch Mk XII (British destroyers): a long barrel with a big breech ring, the cradle with recoil cylinders above,
    a loading tray behind and the layer's and trainer's sight boxes on either side."""
    m = Model('qf_47inch', 'us5', item='QF 4.7-inch')
    t, b = m.turret(), m.barrel()
    lathe(b, US5, (0, 0, -1.05), (0, 0, 1), [(0, 0.16), (0.75, 0.16), (0.75, 0.11), (1.1, 0.1), (1.1, 0.085), (4.6, 0.068),
                                            (4.6, 0.078), (4.75, 0.078), (4.75, 0.0)], seg=18)
    chamfer_box(b, US5, (0, 0, -0.75), (0.42, 0.42, 0.55), 0.05)
    cyl(b, DARK, (0.21, 0.05, -0.85), (0.32, 0.05, -0.85), 0.03, seg=8)                       # breech lever
    chamfer_box(t, US5, (0, -0.02, 0.2), (0.36, 0.3, 1.25), 0.04)                              # cradle
    for sx in (-1, 1):
        cyl(t, US5, (sx * 0.1, 0.19, -0.4), (sx * 0.1, 0.19, 0.9), 0.06, seg=12)              # recoil cylinders
        box(t, US5, (sx * 0.26, -0.05, 0.0), (0.16, 0.28, 0.32))                                # trunnion brackets
        box(t, DARK, (sx * 0.5, 0.12, -0.15), (0.1, 0.14, 0.42))                                # sight boxes
        cyl(t, DARK, (sx * 0.5, 0.12, -0.36), (sx * 0.5, 0.12, -0.45), 0.035, seg=10)
        box(t, US5, (sx * 0.38, 0.05, -0.15), (0.16, 0.04, 0.06))
    box(t, US5, (0.0, -0.12, -1.35), (0.24, 0.04, 0.6))                                        # loading tray
    box(t, US5, (0.0, -0.08, -1.05), (0.3, 0.1, 0.06))
    return m


def type98_100():
    """100 mm/65 Type 98 twin (Akizuki): the game's own Type 98 barrels at 0.75 size, on an open cradle with the twin breech
    housing and the loading trays."""
    m = Model('type98_100mm', 'us5', item='100 mm Type 98 twin')
    src = gm.Prefab('Type98 Dual Barrel Turret')
    k = 0.75
    S = np.diag([k, k, k, 1.0])
    b, t = m.barrel(), m.turret()
    nb = 0
    for side in ('L', 'R'):
        nb += game_mesh(b, src, 'weapon/Weapon Model %s/Model_LOD0' % side, 'weapon', mat4((0, 0, 0.25)) @ S, paint=US5)
    chamfer_box(t, US5, (0, 0.0, -0.45), (0.86, 0.5, 1.2), 0.06)                                 # twin breech housing
    box(t, US5, (0, 0.27, -0.45), (0.7, 0.04, 1.0))
    for sx in (-1, 1):
        cyl(t, US5, (sx * 0.26, 0.0, 0.15), (sx * 0.26, 0.0, 0.55), 0.12, seg=14)                # barrel collars
        box(t, US5, (sx * 0.52, -0.1, 0.0), (0.18, 0.3, 0.4))                                    # trunnion brackets
        box(t, US5, (sx * 0.26, -0.15, -1.35), (0.22, 0.04, 0.7))                                # loading trays
        box(t, DARK, (sx * 0.62, 0.2, -0.5), (0.1, 0.14, 0.4))                                   # sight boxes
    for dy in (0.17, -0.17):
        cyl(t, US5, (0, dy, -0.2), (0, dy, 0.7), 0.05, seg=10)                                  # recoil cylinders
    m.note = 'game mesh: Type 98 barrels (%d triangles) at %.2f size' % (nb, k)
    return m


# ---- on the Twin Oerlikon: the game's Type 96 triple and dual ------------------------------------------------------------
def _type96_on_dual(name, prefab, item):
    m = Model(name, 'dual20', item=item)
    src = gm.Prefab(prefab)
    node = 'weapon/Weapon Model/Model_LOD0'
    tris, _ = src.mesh(node, 'weapon')
    P = np.concatenate([q[1] for q in tris])
    muzzle = P[P[:, 2] > P[:, 2].max() - 0.12]
    xs = np.sort(muzzle[:, 0])
    groups = np.split(xs, np.where(np.diff(xs) > 0.08)[0] + 1)
    centres = [g.mean() for g in groups]
    mid = (min(centres) + max(centres)) / 2
    bore_y = muzzle[:, 1].mean()
    zmin = P[:, 2].min()
    M = mat4((-mid, -bore_y, -0.62 - zmin))                 # the receivers' backs at the Oerlikons' (z -0.6)
    n = game_mesh(m.turret(), src, node, 'weapon', M)
    m.note = 'game mesh %s: %d triangles, %d barrels at x %s' % (prefab, n, len(centres), ', '.join('%.2f' % (c - mid) for c in centres))
    return m


def type96_triple():
    return _type96_on_dual('type96_triple', 'Type96 Triple', 'Type 96 25 mm Triple')


def type96_dual():
    return _type96_on_dual('type96_dual', 'Type96 Dual', 'Type 96 25 mm Dual')


# ---- MEDIUM: Flakvierling on the Elco ------------------------------------------------------------------------------------
def _flak38_gun(t, x, y, paint):
    """One 2 cm Flak 38 of the Flakvierling, bore at (x, y), magazine on its left."""
    chamfer_box(t, paint, (x, y + 0.015, -0.03), (0.12, 0.17, 0.86), 0.02)
    box(t, paint, (x, y + 0.105, -0.08), (0.085, 0.012, 0.6))
    lathe(t, paint, (x, y + 0.01, -0.46), (0, 0, -1), [(0, 0.05), (0.04, 0.045), (0.08, 0.0)], seg=10)
    box(t, paint, (x, y - 0.095, -0.2), (0.05, 0.05, 0.2))
    box(t, paint, (x - 0.075, y + 0.035, 0.02), (0.03, 0.12, 0.17))
    chamfer_box(t, paint, (x - 0.19, y + 0.035, 0.02), (0.2, 0.11, 0.15), 0.01, axis=0)
    lathe(t, paint, (x, y, 0.43), (0, 0, 1), [(0, 0.05), (0.1, 0.05), (0.1, 0.04), (0.18, 0.04), (0.18, 0.026), (0.28, 0.024),
                                             (1.3, 0.02), (1.3, 0.024), (1.33, 0.024), (1.55, 0.045), (1.55, 0.037), (1.53, 0.0)], seg=12)


def flakvierling():
    """2 cm Flakvierling 38: four Flak 38s in a 2 x 2 block (magazines on the left of each), a cradle and trunnion axle
    between the Elco's arms, and the gunner's seat, handlebars and reflector sight on the left where the Elco gunner sits."""
    m = Model('flakvierling', 'elco', item='2 cm Flakvierling 38')
    t = m.turret()
    cx = 0.3
    for x in (cx - 0.24, cx + 0.24):
        for y in (0.19, -0.19):
            _flak38_gun(t, x, y, DARK)
    # cradle: side plates of the gun block, the axle out to both arms, a box below (open at the front)
    for x in (cx - 0.36, cx + 0.36):
        box(t, DARK, (x, 0.0, -0.05), (0.02, 0.62, 0.7))
    cyl(t, DARK, (-0.84, 0.0, 0.0), (0.84, 0.0, 0.0), 0.04, seg=10)
    box(t, DARK, (cx, -0.38, -0.05), (0.6, 0.08, 0.6))
    # the shield as on the real mount (Discord photos 2026-11-15): two tall flat panels on the carriage either side of the
    # gun block, which elevates in the gap between them; each panel's outer edge bends back and its lower inner corner is
    # stepped out, and a low plate closes the gap under the guns. Yaw part: turns with the mount, does not elevate.
    # The block (magazines included) spans x -0.23..0.67 and swings through z < 0.5 under the barrels.
    y = m.yaw()
    zs, th = 0.5, 0.012
    for xi, xo in ((-0.3, -1.0), (cx + 0.42, cx + 1.06)):               # inner edge, outer edge
        sx = 1 if xo > xi else -1
        top = [(xi, 0.62), (xo, 0.62)]
        if sx < 0:                                                     # sight notch over the Elco gunner's sight (x -0.48)
            top = [(xi, 0.62), (-0.38, 0.62), (-0.38, 0.3), (-0.58, 0.3), (-0.58, 0.62), (xo, 0.62)]
        flat_shield(y, DARK, [(xi, -0.2)] + top + [(xo, -0.62), (xi + sx * 0.18, -0.62), (xi + sx * 0.18, -0.2)], zs, th)
        a = math.radians(35)
        box(y, DARK, (xo + sx * 0.11 * math.sin(a), 0.0, zs - 0.11 * math.cos(a)), (th, 1.24, 0.22), (0, -sx * 35, 0))
    box(y, DARK, (cx, -0.62, zs + 0.02), (0.92, 0.22, th))              # low centre plate, below the block's swing
    # the gunner's place: seat pan and back on the Elco's seat, handlebars at the hands, the sight above them
    box(t, LEATHER, (-0.48, -0.12, -0.49), (0.3, 0.05, 0.28))
    box(t, LEATHER, (-0.48, 0.1, -0.66), (0.3, 0.36, 0.05))
    cyl(t, DARK, (-0.48, -0.15, -0.49), (-0.48, -0.15, -0.1), 0.02, seg=6)
    cyl(t, DARK, (-0.48, -0.15, -0.1), (cx - 0.36, -0.15, -0.1), 0.02, seg=6)
    tube(t, DARK, [(-0.68, 0.15, -0.07), (-0.64, 0.15, 0.05), (-0.32, 0.14, 0.05), (-0.28, 0.14, -0.08)], 0.016, seg=6)
    cyl(t, DARK, (-0.48, 0.15, 0.05), (cx - 0.36, 0.15, 0.05), 0.016, seg=6)
    for hx in (-0.64, -0.32):
        lathe(t, LEATHER, (hx, 0.15, -0.07), (0, 0, -1), [(0, 0.02), (0.1, 0.02)], seg=8)
    box(t, DARK, (-0.48, 0.32, 0.02), (0.08, 0.12, 0.1))
    box(t, DARK, (-0.48, 0.4, 0.06), (0.1, 0.006, 0.08), (40, 0, 0))
    cyl(t, DARK, (-0.48, 0.2, 0.05), (-0.48, 0.26, 0.02), 0.012, seg=6)
    return m


# ---- UTILITY ---------------------------------------------------------------------------------------------------------------
def mortar81():
    """81 mm mortar on the 60 mm's place: rectangular ribbed baseplate on the deck, a longer tube, the bipod with its
    traverse and elevating screws, a sight on the left."""
    m = Model('mortar_81mm', 'mortar60', item='Mk 2 81 mm mortar')
    t = m.turret()
    deck = -0.72
    d = np.array([0.0, 0.83, 0.56]); d /= np.linalg.norm(d)
    base = np.array([0.0, deck + 0.07, -0.52])
    box(t, OLIVE, (0, deck + 0.02, -0.5), (0.56, 0.04, 0.46))
    for x in (-0.18, 0.0, 0.18):
        box(t, OLIVE, (x, deck + 0.055, -0.5), (0.02, 0.04, 0.42))
    lathe(t, OLIVE, base - d * 0.02, d, [(0, 0.0), (0, 0.07), (0.05, 0.07), (0.08, 0.052)], seg=14)   # socket
    L = 1.25
    lathe(t, OLIVE, base, d, [(0, 0.06), (0.1, 0.052), (L - 0.04, 0.05), (L - 0.04, 0.056), (L, 0.056), (L, 0.044), (L - 0.06, 0.044)],
          seg=16, caps=False)
    collar = along(base, d, 0.78)
    lathe(t, AT_GREY, collar - d * 0.05, d, [(0, 0.07), (0.1, 0.07)], seg=14)
    feet = [np.array([sx * 0.32, deck + 0.02, 0.35]) for sx in (-1, 1)]
    yoke = collar + np.array([0.0, -0.28, 0.12])
    for f in feet:
        cyl(t, OLIVE, yoke, f, 0.02, seg=8)
        box(t, OLIVE, f, (0.08, 0.02, 0.1))
    cyl(t, OLIVE, feet[0] + (0, 0.25, -0.08), feet[1] + (0, 0.25, -0.08), 0.012, seg=6)                    # leg chain bar
    cyl(t, AT_GREY, yoke, collar, 0.025, seg=8)                                                             # elevating screw
    cyl(t, AT_GREY, yoke + (-0.16, 0, 0), yoke + (0.16, 0, 0), 0.022, seg=8)                                # traverse screw
    handwheel(t, AT_GREY, yoke + (0.2, 0, 0), (1, 0, 0), 0.05)
    handwheel(t, AT_GREY, yoke + (0, -0.1, 0), (0, -1, 0), 0.045)
    box(t, AT_GREY, collar + (-0.13, 0.04, -0.02), (0.06, 0.1, 0.12))                                        # sight
    cyl(t, AT_GREY, collar + (-0.13, 0.1, -0.06), collar + (-0.13, 0.1, 0.02), 0.02, seg=8)
    return m


def m18():
    """M18 57 mm recoilless rifle in a cradle on the 37 mm carriage (wheels, trails and handwheels kept, no shield): the
    thin tube, the vented breech, pistol grip and telescope."""
    m = Model('m18_recoilless', 'at37', item='M18 57 mm recoilless')
    yb = 0.465
    b, t = m.barrel(), m.turret()
    lathe(b, OLIVE, (0, yb, -0.95), (0, 0, 1), [(0, 0.0), (0, 0.06), (0.04, 0.085), (0.2, 0.085), (0.26, 0.06), (0.3, 0.05),
                                              (1.95, 0.042), (1.95, 0.05), (2.0, 0.05), (2.0, 0.0)], seg=16)
    for k in range(10):                                                                   # vent holes round the breech
        a = 2 * math.pi * k / 10
        box(b, DARK, (0.086 * math.cos(a), yb + 0.086 * math.sin(a), -0.83), (0.012, 0.012, 0.03), (0, 0, math.degrees(a)))
    for z in (-0.45, 0.35):                                                                # clamp rings
        ring(b, AT_GREY, (0, yb, z), (0, 0, 1), 0.055, 0.012, n=16)
    extrude(b, OLIVE, [(-0.38, yb - 0.04), (-0.3, yb - 0.04), (-0.32, yb - 0.17), (-0.38, yb - 0.17)], (0, 0, 1), (0, 1, 0), (1, 0, 0), -0.015, 0.015)
    tube(b, AT_GREY, [(0, yb - 0.04, -0.27), (0, yb - 0.09, -0.27), (0, yb - 0.1, -0.33), (0, yb - 0.05, -0.36)], 0.004, seg=5)
    box(b, AT_GREY, (-0.08, yb + 0.07, -0.2), (0.03, 0.04, 0.06))
    cyl(b, DARK, (-0.08, yb + 0.1, -0.35), (-0.08, yb + 0.1, -0.02), 0.018, seg=10)        # telescope
    tube(b, AT_GREY, [(0, yb + 0.05, -0.15), (0, yb + 0.12, -0.12), (0, yb + 0.12, 0.08), (0, yb + 0.05, 0.11)], 0.009, seg=6)
    # cradle from the carriage up to the clamps, and links to the kept handwheels
    box(t, AT_GREY, (0, 0.18, -0.05), (0.2, 0.3, 0.9))
    for z in (-0.45, 0.35):
        box(t, AT_GREY, (0, 0.36, z), (0.06, 0.1, 0.05))
    cyl(t, AT_GREY, (-0.1, 0.25, -0.4), (-0.33, 0.45, -0.95), 0.015, seg=6)
    cyl(t, AT_GREY, (-0.1, 0.2, -0.45), (-0.35, 0.33, -1.02), 0.015, seg=6)
    return m


def type92_70():
    """Type 92 70 mm battalion gun (Japanese) on the pack howitzer's carriage: a short barrel with a heavy breech, the recoil
    cylinder underneath and the shield with its sighting slot."""
    m = Model('type92_70mm', 'how75', item='Type 92 70 mm')
    yb, xb = 0.15, -0.01
    b, t = m.barrel(), m.turret()
    box(b, HOW, (xb, yb, 0.12), (0.22, 0.22, 0.3))                                         # breech
    lathe(b, HOW, (xb, yb, 0.27), (0, 0, 1), [(0, 0.075), (0.15, 0.07), (0.15, 0.06), (0.82, 0.05), (0.82, 0.062), (0.88, 0.062),
                                             (0.88, 0.0)], seg=16)
    cyl(b, HOW_DARK, (xb + 0.11, yb + 0.05, 0.05), (xb + 0.19, yb + 0.05, 0.05), 0.02, seg=6)
    cyl(t, HOW, (xb, yb - 0.12, 0.0), (xb, yb - 0.12, 0.85), 0.065, seg=12)                 # recoil cylinder
    box(t, HOW, (xb, yb - 0.2, 0.3), (0.2, 0.1, 0.7))                                       # cradle
    lathe(t, HOW, (xb, yb - 0.12, 0.85), (0, 0, 1), [(0, 0.065), (0.04, 0.05), (0.05, 0.0)], seg=12)
    sh = [(-0.42, -0.3), (0.42, -0.3), (0.42, 0.32), (0.3, 0.42), (0.09, 0.42), (0.09, 0.06), (-0.09, 0.06), (-0.09, 0.42),
          (-0.3, 0.42), (-0.42, 0.32)]
    flat_shield(t, HOW, [(x + xb, y + yb - 0.02) for x, y in sh], 0.45)
    box(t, HOW, (xb, yb - 0.24, 0.42), (0.2, 0.06, 0.08))
    box(t, HOW_DARK, (xb - 0.17, yb + 0.12, 0.2), (0.04, 0.06, 0.18))                        # sight
    return m


# ---- ROCKETS: on the Mk 50 frames ----------------------------------------------------------------------------------------
def _rocket(p, c, r, body, nose, paint_body, paint_head, fins=True):
    c = np.asarray(c, float)
    lathe(p, paint_body, c, (0, 0, 1), [(0, 0.0), (0, r * 0.75), (0.03, r), (body, r)], seg=12, caps=False)
    lathe(p, paint_head, c + (0, 0, body), (0, 0, 1), [(0, r * 1.05), (nose * 0.45, r * 1.05), (nose * 0.8, r * 0.7), (nose, 0.0)], seg=12)
    if fins:
        for k in range(4):
            a = k * math.pi / 2 + math.pi / 4
            box(p, paint_body, c + (r * 1.1 * math.cos(a), r * 1.1 * math.sin(a), 0.07), (0.006, r * 0.8, 0.14), (0, 0, math.degrees(a) - 90))


def woofus():
    """7.2-inch 'Woofus' launcher: four huge rockets on rails in a 2 x 2 box frame, trunnioned on the Mk 50's arm."""
    m = Model('woofus_72inch', 'mk50', item='7.2-inch Woofus')
    t = m.turret()
    xs, ys = (-0.45, -0.17), (0.2, -0.08)
    for x in xs:
        for y in ys:
            _rocket(t, (x, y, -0.45), 0.09, 0.78, 0.42, ROCKET, RL_DARK)
            box(t, RL_DARK, (x, y - 0.1, 0.05), (0.06, 0.02, 1.15))                          # rail
    for z in (-0.4, 0.15, 0.6):                                                               # frame hoops
        for y in (0.33, -0.21):
            box(t, RL_DARK, (-0.31, y, z), (0.6, 0.03, 0.05))
        for x in (-0.6, -0.02):
            box(t, RL_DARK, (x, 0.06, z), (0.03, 0.57, 0.05))
    for x in (-0.6, -0.02):
        for y in (0.33, -0.21):
            box(t, RL_DARK, (x, y, 0.1), (0.03, 0.03, 1.05))
    cyl(t, RL_DARK, (-0.75, 0.06, 0.0), (-0.6, 0.06, 0.0), 0.05, seg=10)                         # trunnion to the arm
    box(t, RL_LIGHT, (-0.31, 0.37, -0.38), (0.24, 0.04, 0.12))                                   # firing box
    return m


def land_mattress():
    """'Land Mattress' frame launcher: 32 short tubes, 8 x 4, in banded frames, each with a rocket nose showing."""
    m = Model('land_mattress', 'mk50mod', item='Land Mattress')
    t = m.turret()
    r = 0.036
    xs = [-0.62 + 0.08 * i + 0.04 for i in range(8)]
    ys = [-0.18 + 0.08 * j + 0.04 for j in range(4)]
    for x in xs:
        for y in ys:
            lathe(t, RL_DARK, (x, y, -0.5), (0, 0, 1), [(0, 0.0), (0, r), (1.2, r), (1.2, r * 0.8), (1.14, r * 0.8)], seg=8, caps=False)
            lathe(t, ROCKET, (x, y, 0.6), (0, 0, 1), [(0, r * 0.78), (0.06, r * 0.6), (0.11, 0.0)], seg=8)   # noses just inside
    x0, x1 = xs[0] - r - 0.01, xs[-1] + r + 0.01
    y0, y1 = ys[0] - r - 0.01, ys[-1] + r + 0.01
    for z in (-0.42, 0.12, 0.6):                                                              # bands
        for y in (y0, y1):
            box(t, RL_LIGHT, ((x0 + x1) / 2, y, z), (x1 - x0 + 0.02, 0.012, 0.05))
        for x in (x0, x1):
            box(t, RL_LIGHT, (x, (y0 + y1) / 2, z), (0.012, y1 - y0 + 0.02, 0.05))
    box(t, RL_DARK, (x0 - 0.02, (y0 + y1) / 2, 0.05), (0.02, 0.3, 0.9))                      # side plate to the trunnion
    cyl(t, RL_DARK, (-0.76, 0.0, 0.0), (x0 - 0.02, 0.0, 0.0), 0.05, seg=10)
    return m


# ---- CREW: in the BAR's hands --------------------------------------------------------------------------------------------
YB = 0.10                                   # the BAR's bore height; stock at z 0, the hands at z 0.22 (right) and 0.51 (left)


def _stock(b, z0, z1, drop, paint):
    extrude(b, paint, [(z0, YB - 0.03 - drop), (z0, YB + 0.03 - drop * 0.4), (z1, YB + 0.02), (z1, YB - 0.05)],
            (0, 0, 1), (0, 1, 0), (1, 0, 0), -0.02, 0.02)


def _rounded_slab(b, paint, outline, half, inset=0.005):
    """A flat wooden part with eased edges: the outline at full thickness, plus a slightly smaller copy standing proud on
    each face."""
    extrude(b, paint, outline, (0, 0, 1), (0, 1, 0), (1, 0, 0), -half, half)
    c = np.mean(np.array(outline), axis=0)
    small = [(c[0] + (u - c[0]) * (1 - inset / 0.03), c[1] + (v - c[1]) * (1 - inset / 0.06)) for u, v in outline]
    extrude(b, paint, small, (0, 0, 1), (0, 1, 0), (1, 0, 0), -half - inset * 0.8, half + inset * 0.8)


def _pistol_grip(b, z, paint):
    """Raked pistol grip: curved front and back, rounded heel; the trigger guard in front of it."""
    _rounded_slab(b, paint, [(z + 0.055, YB - 0.035), (z + 0.05, YB - 0.07), (z + 0.04, YB - 0.11), (z + 0.033, YB - 0.145),
                             (z + 0.027, YB - 0.162), (z + 0.012, YB - 0.169), (z - 0.006, YB - 0.165), (z - 0.016, YB - 0.15),
                             (z - 0.011, YB - 0.11), (z + 0.0, YB - 0.07), (z + 0.014, YB - 0.045), (z + 0.026, YB - 0.035)], 0.013)
    tube(b, BAR_DARK, [(0, YB - 0.04, z + 0.08), (0, YB - 0.09, z + 0.08), (0, YB - 0.1, z + 0.12), (0, YB - 0.05, z + 0.13)], 0.003, seg=5)


def thompson():
    """Thompson M1928 with the 50-round drum: wooden stock and vertical foregrip, finned barrel, Cutts compensator."""
    m = Model('thompson_m1928', 'bar', item='Thompson M1928')
    b = m.barrel()
    _stock(b, 0.05, 0.26, 0.05, BAR_WOOD)
    _pistol_grip(b, 0.24, BAR_WOOD)
    box(b, BAR_METAL, (0, YB, 0.42), (0.034, 0.056, 0.34))                               # receiver
    box(b, BAR_METAL, (0, YB + 0.035, 0.36), (0.012, 0.014, 0.012))                      # cocking knob
    box(b, BAR_METAL, (0, YB + 0.04, 0.27), (0.02, 0.025, 0.02))                         # rear sight
    lathe(b, BAR_METAL, (0, YB + 0.005, 0.59), (0, 0, 1), [(0, 0.016)] + sum([[(0.02 + 0.02 * k, 0.016), (0.02 + 0.02 * k, 0.021),
                                                                             (0.03 + 0.02 * k, 0.021), (0.03 + 0.02 * k, 0.016)] for k in range(8)], [])
          + [(0.3, 0.012), (0.3, 0.017), (0.37, 0.017), (0.37, 0.0)], seg=10)
    box(b, BAR_METAL, (0, YB + 0.03, 0.95), (0.004, 0.015, 0.008))
    # the drum hangs ahead of the trigger guard with its flat faces to the front and back (axis along the barrel), so
    # from the side you see its thin edge (Discord 2026-11-01, with a photo); its top just inside the receiver's slot
    dc = np.array([0.0, YB - 0.1, 0.455])
    lathe(b, BAR_METAL, dc - (0, 0, 0.04), (0, 0, 1), [(0, 0.0), (0, 0.082), (0.004, 0.085), (0.076, 0.085), (0.08, 0.082), (0.08, 0.0)], seg=24)
    for z in (-0.04, 0.04):                                                               # pressed rims on both faces
        ring(b, BAR_METAL, (0, dc[1], dc[2] + z), (0, 0, 1), 0.062, 0.004, n=24, seg=4)
    lathe(b, BAR_DARK, (0, dc[1], dc[2] + 0.04), (0, 0, 1), [(0, 0.0), (0, 0.022), (0.006, 0.02), (0.006, 0.0)], seg=10)   # winding key
    box(b, BAR_DARK, (0, dc[1], dc[2] + 0.048), (0.04, 0.006, 0.006))
    # vertical foregrip under the barrel, finger grooves at the front, a knob at the bottom
    _rounded_slab(b, BAR_WOOD, [(0.565, YB - 0.03), (0.615, YB - 0.03), (0.62, YB - 0.05), (0.61, YB - 0.065), (0.62, YB - 0.08),
                                (0.61, YB - 0.095), (0.62, YB - 0.11), (0.612, YB - 0.13), (0.606, YB - 0.145), (0.6, YB - 0.152),
                                (0.575, YB - 0.152), (0.567, YB - 0.142), (0.569, YB - 0.1), (0.566, YB - 0.05)], 0.016)
    return m


def johnson():
    """Johnson M1941 LMG: wooden stock and pistol grip, round receiver, the 20-round magazine sticking out on the left,
    a long barrel with a folded bipod."""
    m = Model('johnson_m1941', 'bar', item='Johnson M1941 LMG')
    b = m.barrel()
    _stock(b, 0.0, 0.24, 0.03, BAR_WOOD)
    _pistol_grip(b, 0.22, BAR_WOOD)
    lathe(b, BAR_METAL, (0, YB, 0.22), (0, 0, 1), [(0, 0.03), (0.4, 0.03), (0.42, 0.024), (0.48, 0.02)], seg=12)
    box(b, BAR_METAL, (-0.11, YB, 0.44), (0.18, 0.035, 0.06))                            # side magazine (left)
    box(b, BAR_METAL, (0, YB + 0.05, 0.27), (0.014, 0.04, 0.02))                          # tall rear sight
    extrude(b, BAR_WOOD, [(0.45, YB - 0.02), (0.68, YB - 0.02), (0.68, YB - 0.06), (0.47, YB - 0.07)], (0, 0, 1), (0, 1, 0), (1, 0, 0), -0.022, 0.022)
    lathe(b, BAR_METAL, (0, YB, 0.68), (0, 0, 1), [(0, 0.014), (0.42, 0.013), (0.42, 0.018), (0.47, 0.018), (0.47, 0.0)], seg=10)
    box(b, BAR_METAL, (0, YB + 0.03, 1.1), (0.004, 0.02, 0.008))
    for sx in (-1, 1):                                                                    # folded bipod legs
        cyl(b, BAR_DARK, (sx * 0.012, YB - 0.02, 0.98), (sx * 0.018, YB - 0.025, 0.66), 0.006, seg=5)
    box(b, BAR_DARK, (0, YB - 0.018, 0.98), (0.04, 0.012, 0.02))
    return m


def boys():
    """Boys anti-tank rifle: long barrel with the box muzzle brake, the five-round magazine standing on top, sights offset
    to the left, bipod and padded butt with its grip."""
    m = Model('boys_at_rifle', 'bar', item='Boys AT rifle')
    b = m.barrel()
    extrude(b, BAR_METAL, [(-0.02, YB - 0.1), (-0.02, YB + 0.03), (0.2, YB + 0.03), (0.22, YB - 0.04), (0.08, YB - 0.04), (0.06, YB - 0.1)],
            (0, 0, 1), (0, 1, 0), (1, 0, 0), -0.022, 0.022)
    box(b, BAR_DARK, (0, YB - 0.035, -0.03), (0.05, 0.14, 0.03))                          # rubber pad
    _pistol_grip(b, 0.22, BAR_WOOD)
    lathe(b, BAR_METAL, (0, YB, 0.2), (0, 0, 1), [(0, 0.04), (0.48, 0.04), (0.5, 0.03), (0.55, 0.026)], seg=14)
    box(b, BAR_METAL, (0, YB + 0.11, 0.46), (0.04, 0.16, 0.1), (-10, 0, 0))                # magazine on top
    box(b, BAR_DARK, (0, YB + 0.195, 0.475), (0.044, 0.02, 0.104), (-10, 0, 0))
    lathe(b, BAR_METAL, (0, YB, 0.75), (0, 0, 1), [(0, 0.024), (0.6, 0.02), (0.6, 0.0)], seg=12)
    box(b, BAR_METAL, (0, YB, 1.41), (0.075, 0.06, 0.13))                                 # muzzle brake
    for sx in (-1, 1):
        box(b, BAR_DARK, (sx * 0.0385, YB, 1.41), (0.003, 0.04, 0.09))
    box(b, BAR_METAL, (-0.055, YB + 0.05, 0.4), (0.02, 0.04, 0.03))                        # offset rear sight
    box(b, BAR_METAL, (-0.05, YB + 0.03, 0.75), (0.012, 0.035, 0.012))
    box(b, BAR_METAL, (0, YB - 0.04, 0.82), (0.05, 0.03, 0.05))                            # bipod
    for sx in (-1, 1):
        cyl(b, BAR_DARK, (sx * 0.02, YB - 0.05, 0.82), (sx * 0.16, YB - 0.3, 0.9), 0.008, seg=6)
    cyl(b, BAR_DARK, (0, YB - 0.1, 0.05), (0, YB - 0.2, 0.05), 0.008, seg=6)               # butt monopod
    return m


MODELS = [pompom, flak_m42, molins, flak88, type3_80, qf47, type98_100, type96_triple, type96_dual, flakvierling,
          mortar81, m18, type92_70, woofus, land_mattress, thompson, johnson, boys]


def main():
    out = pathlib.Path(sys.argv[1])
    prev = pathlib.Path(sys.argv[sys.argv.index('--preview') + 1]) if '--preview' in sys.argv else None
    only = sys.argv[sys.argv.index('--only') + 1].split(',') if '--only' in sys.argv else None
    import importlib                                     # here, not at the top: the sg2_* modules import this one
    makers = list(MODELS)
    for mod in ('sg2_heavy', 'sg2_medium', 'sg2_utility', 'sg2_rockets', 'sg2_crew'):   # side-grades batch 2 (2026-11-15)
        makers += importlib.import_module(mod).MODELS
    built = []
    for make in makers:
        if only and make.__name__ not in only:
            continue
        m = make()
        n = m.write(out)
        print('%-20s %5d triangles  keep=%s  %s' % (m.name, n, ','.join(m.keep), getattr(m, 'note', '')))
        built.append(m)
    if prev:
        prev.mkdir(parents=True, exist_ok=True)
        for i in range(0, len(built), 3):
            tm.detail_sheet(built[i:i + 3], prev / ('sidegrade_models_%d.png' % (i // 3 + 1)))
            if '--closeups' in sys.argv:
                closeup_sheet(built[i:i + 3], prev / ('sidegrade_closeups_%d.png' % (i // 3 + 1)))


def closeup_sheet(models, path):
    """Like the detail sheet, but only what stands above the pivot minus 0.4 m (the gun, not the mount), larger."""
    import preview3d as pv
    rows, labels = [], []
    for m in models:
        y0 = m.prefab.frame(m.base['pivot'])[1, 3] - 0.4
        stock = [t for t in m.scene(new=False) if t[1][:, 1].min() > y0]
        new = [t for t in m.scene(new=True) if t[1][:, 1].min() > y0]
        allp = np.concatenate([t[1] for t in stock + new])
        center = (allp.min(0) + allp.max(0)) / 2
        sc = 420 * 0.47 / np.abs(allp - center).max()
        views = [(1.0, 0.55, 1.25), (1, 0.05, 0.0), (-1.0, 0.6, -0.9)]
        rows.append([pv.render(stock, views[0], scale=sc, center=center)] + [pv.render(new, v, scale=sc, center=center) for v in views])
        labels.append('%s  close-up' % m.name)
    pv.sheet(rows, labels).save(path)


if __name__ == '__main__':
    main()
