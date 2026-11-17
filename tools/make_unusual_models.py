"""Builds the Unusual guns' models (OBJ + MTL) for the mod's gun model loader, plus preview PNGs.

The mod places each OBJ object by its name (ApplyModel in weaponmod.cpp):
  stand*  -> GunMount.stand (turns with the gun), or the mount root when the gun has no stand (the .50 Cal)
  barrel* -> every Barrel.transform (a copy per barrel; recoils with it)
  others  -> GunMount.pivotPoint (turns and elevates with the gun)
The game's OBJ loader (Dummiesman) reads coordinates as written: no mirroring, faces in file order, normals from the
file (checked in its disassembly, 2026-10-28). Units are metres, +Y up, +Z forward.

Each part is authored in the stock gun's root frame (stand) or pivot frame (turret, single barrels), or in the barrel's
own frame for guns with several barrels, and converted to the frame it is attached to. The offsets come from the
game's prefabs (read with UnityPy from Boat Crew_Data/data.unity3d, 2026-10-28).

Usage: python make_unusual_models.py <models folder> [--preview <folder>]
"""
import math
import pathlib
import sys

import numpy as np

# ---- materials (diffuse colour; the mod uses model_material=own, so these show as they are) ---------------------------
MATS = {
    'Pink': (1.0, 0.37, 0.78), 'Gunmetal': (0.2, 0.22, 0.25), 'Steel': (0.58, 0.6, 0.64), 'Navy': (0.24, 0.31, 0.47),
    'Hull': (0.58, 0.61, 0.65), 'Deck': (0.36, 0.38, 0.41), 'Yellow': (1.0, 0.82, 0.1), 'Duck': (1.0, 0.85, 0.15),
    'Wing': (1.0, 0.7, 0.08), 'Orange': (1.0, 0.5, 0.05), 'Black': (0.04, 0.04, 0.05), 'White': (0.95, 0.95, 0.95),
    'Grey': (0.6, 0.63, 0.67), 'GullGrey': (0.62, 0.66, 0.72), 'Wood': (0.5, 0.32, 0.16), 'Rope': (0.78, 0.65, 0.42),
    'Water': (0.15, 0.45, 0.8), 'Red': (0.85, 0.1, 0.12), 'Gold': (1.0, 0.78, 0.2), 'Cyan': (0.2, 0.85, 0.95),
    'Purple': (0.48, 0.22, 0.78), 'DarkPurple': (0.3, 0.12, 0.5), 'Green': (0.2, 0.75, 0.3), 'Ochre': (0.82, 0.45, 0.18),
    'PlaneBlue': (0.25, 0.35, 0.6), 'Leather': (0.4, 0.25, 0.12), 'Space': (0.1, 0.09, 0.22), 'Night': (0.1, 0.12, 0.3),
    'Boomerang': (0.62, 0.38, 0.18), 'Olive': (0.33, 0.4, 0.24),
}

# ---- the stock guns' frames (root frame of the GunMount; pivot and barrels in the pivot frame) -------------------------
# stand: (position in root, yaw) or None; barrels: (position in pivot, euler) ; seats in root and shoot points in pivot
# are only drawn in the previews.
RIGS = {
    'bofors': dict(stand=((0, -1.15, 0), 180), pivot=(0, 0.23, -0.17), barrels=[((0, -0.09, -0.14), (0, 0, 0))],
                   seats=[(-0.8, -0.54, -0.13), (0.8, -0.54, -0.13)], shoot=[(0, 0.08, 1.14)]),
    'at37': dict(stand=((0, -1.12, 0), 0), pivot=(0, -0.70, 0), barrels=[((0, 0, 0), (0, 180, 0))],
                 seats=[(-0.51, -1.10, -1.27)], shoot=[(0, 0.47, 1.91)]),
    'oerlikon': dict(stand=((0, -1.13, 0), 0), pivot=(0, 0.27, -0.34), barrels=[((0, 0, -0.27), (0, 0, 0))],
                     seats=[(0, -1.12, -1.08)], shoot=[(0, 0, 0)]),
    'browning': dict(stand=None, pivot=(0.14, 0.05, 0.48), barrels=[((-0.06, 0, -0.22), (0, 90, 0))],
                     seats=[(0, -1.50, -0.38)], shoot=[(-0.30, 0, 0), (0.01, 0.01, 0)]),
    'dualbofors': dict(stand=((0, -1.15, 0), 180), pivot=(0, 0.23, -0.17),
                       barrels=[((0.17, -0.09, -0.14), (0, 0, 0)), ((-0.17, -0.09, -0.14), (0, 0, 0))],
                       seats=[(-0.8, -0.54, -0.13), (0.8, -0.54, -0.13)], shoot=[(0.22, 0.08, 1.14), (-0.22, 0.08, 1.14)]),
    'elco': dict(stand=((0, -1.09, 0), 0), pivot=(0, 0.02, -0.03),
                 barrels=[((-0.21, -0.03, -0.25), (0, 0, 21)), ((0.21, -0.03, -0.25), (0, 0, 21)),
                          ((-0.01, -0.01, -0.64), (0, 0, 21)), ((0.41, -0.01, -0.64), (0, 0, 21)),
                          ((0.68, -0.03, 0.02), (0, 0, 90)), ((-0.68, -0.03, 0.07), (0, 0, 90))],
                 seats=[(-0.48, -0.13, -0.52)], shoot=[]),
    'autocannon': dict(stand=((0, -1.12, 0), 0), pivot=(0, -0.15, 0), barrels=[((0, 0, 0), (0, 180, 0))],
                       seats=[(0, -1.06, -1.10)], shoot=[(0, 0.07, 1.67)]),
    # 3"/50 (3-inch Gun Player), read 2026-11-16: the barrel faces -z; its bore runs at x -0.04, y 0.22 in the pivot frame
    'us3': dict(stand=((0, -1.147, 0), 0), pivot=(0.037, 0.128, -0.053), barrels=[((-0.039, -1.409, 0.036), (0, 180, 0))],
                seats=[], shoot=[(-0.04, 0.22, 2.87)]),
    # Browning .30 Cal (M1919 Browning), read 2026-11-16: no stand; the twin guns share one barrel transform turned 90 deg,
    # so the guns are turret parts. Bores at x -0.30 and 0.02 in the pivot frame, along +z
    'm1919': dict(stand=None, pivot=(0.139, 0.052, 0.101), barrels=[((-0.051, 0, -0.125), (0, 90, 0))],
                  seats=[(0, -1.5, -0.376)], shoot=[(-0.302, 0, 0.95), (0.024, 0, 0.95)]),
    # 5"/25 (5-inch 25 Gun Player), read 2026-11-16: Stand at the root (deck y -1.32), the barrel transform sits on the
    # pivot unturned; bore on the axis, muzzle z 2.85 in the pivot frame
    'us5': dict(stand=((0, 0, 0), 0), pivot=(0, 0.072, -0.068), barrels=[((0, 0, 0), (0, 0, 0))],
                seats=[(-0.956, -0.761, -0.14), (0.956, -0.761, -0.14)], shoot=[(0, 0, 2.85)]),
}


def rot(rx=0.0, ry=0.0, rz=0.0):
    """Unity order: Z, then X, then Y."""
    ax, ay, az = (math.radians(a) for a in (rx, ry, rz))
    X = np.array([[1, 0, 0], [0, math.cos(ax), -math.sin(ax)], [0, math.sin(ax), math.cos(ax)]])
    Y = np.array([[math.cos(ay), 0, math.sin(ay)], [0, 1, 0], [-math.sin(ay), 0, math.cos(ay)]])
    Z = np.array([[math.cos(az), -math.sin(az), 0], [math.sin(az), math.cos(az), 0], [0, 0, 1]])
    return Y @ X @ Z


def mat4(t=(0, 0, 0), r=(0, 0, 0)):
    M = np.eye(4)
    M[:3, :3] = rot(*r)
    M[:3, 3] = t
    return M


def unit(v):
    v = np.asarray(v, float)
    n = np.linalg.norm(v)
    return v / n if n > 1e-12 else v


class Part:
    def __init__(self, name, to_file, to_root):
        self.name, self.to_file, self.to_root = name, to_file, to_root   # 4x4 rigid matrices
        self.tris = []                                                      # (mat, 3 points, 3 normals)

    def tri(self, mat, ps, ns, outward=None):
        ps = [np.asarray(p, float) for p in ps]
        ns = [unit(n) for n in ns]
        face = np.cross(ps[1] - ps[0], ps[2] - ps[0])
        if np.linalg.norm(face) < 1e-10:
            return
        ref = outward if outward is not None else ns[0] + ns[1] + ns[2]
        if np.dot(face, ref) < 0:                                           # the placeholder's winding (shown right in game)
            ps[1], ps[2], ns[1], ns[2] = ps[2], ps[1], ns[2], ns[1]
        self.tris.append((mat, ps, ns))

    def quad(self, mat, a, b, c, d, na, nb=None, nc=None, nd=None):
        nb = na if nb is None else nb
        nc = na if nc is None else nc
        nd = na if nd is None else nd
        self.tri(mat, (a, b, c), (na, nb, nc))
        self.tri(mat, (a, c, d), (na, nc, nd))


# ---- primitives --------------------------------------------------------------------------------------------------------
def box(p, mat, c, s, r=(0, 0, 0)):
    R, c, h = rot(*r), np.asarray(c, float), np.asarray(s, float) / 2
    for axis in range(3):
        for sign in (-1, 1):
            n = np.zeros(3); n[axis] = sign
            u, v = [i for i in range(3) if i != axis]
            corners = []
            for su, sv in ((-1, -1), (1, -1), (1, 1), (-1, 1)):
                q = np.zeros(3); q[axis] = sign * h[axis]; q[u] = su * h[u]; q[v] = sv * h[v]
                corners.append(c + R @ q)
            p.quad(mat, *corners, R @ n)


def cyl(p, mat, p0, p1, r0, r1=None, seg=16, caps=True):
    r1 = r0 if r1 is None else r1
    p0, p1 = np.asarray(p0, float), np.asarray(p1, float)
    a = p1 - p0; L = np.linalg.norm(a); ah = a / L
    u = unit(np.cross(ah, [0, 1, 0] if abs(ah[1]) < 0.9 else [1, 0, 0])); v = np.cross(ah, u)
    for i in range(seg):
        t0, t1 = 2 * math.pi * i / seg, 2 * math.pi * (i + 1) / seg
        d0, d1 = u * math.cos(t0) + v * math.sin(t0), u * math.cos(t1) + v * math.sin(t1)
        n0, n1 = unit(d0 * L + ah * (r0 - r1)), unit(d1 * L + ah * (r0 - r1))
        if r0 > 1e-6 or r1 > 1e-6:
            p.quad(mat, p0 + d0 * r0, p0 + d1 * r0, p1 + d1 * r1, p1 + d0 * r1, n0, n1, n1, n0)
        if caps and r0 > 1e-6:
            p.tri(mat, (p0, p0 + d1 * r0, p0 + d0 * r0), (-ah, -ah, -ah))
        if caps and r1 > 1e-6:
            p.tri(mat, (p1, p1 + d0 * r1, p1 + d1 * r1), (ah, ah, ah))


def ellipsoid(p, mat, c, rad, r=(0, 0, 0), rings=10, seg=16):
    R, c, rad = rot(*r), np.asarray(c, float), np.asarray(rad, float)

    def pt(i, j):
        th, ph = math.pi * i / rings, 2 * math.pi * j / seg
        q = np.array([math.sin(th) * math.cos(ph), math.cos(th), math.sin(th) * math.sin(ph)]) * rad
        return c + R @ q, R @ unit(q / (rad * rad))
    for i in range(rings):
        for j in range(seg):
            (a, na), (b, nb), (cc, nc), (d, nd) = pt(i, j), pt(i, j + 1), pt(i + 1, j + 1), pt(i + 1, j)
            p.tri(mat, (a, b, cc), (na, nb, nc))
            p.tri(mat, (a, cc, d), (na, nc, nd))


def sphere(p, mat, c, r, rings=8, seg=12):
    ellipsoid(p, mat, c, (r, r, r), rings=rings, seg=seg)


def torus(p, mat, c, R_, r_, r=(0, 0, 0), seg=28, tube=8, a0=0.0, a1=360.0):
    """Ring in the local XZ plane (axis +Y), turned by r."""
    M, c = rot(*r), np.asarray(c, float)

    def pt(i, k):
        a = math.radians(a0 + (a1 - a0) * i / seg); b = 2 * math.pi * k / tube
        rad = np.array([math.cos(a), 0, math.sin(a)]); n = rad * math.cos(b) + np.array([0, 1, 0]) * math.sin(b)
        return c + M @ (rad * R_ + n * r_), M @ n
    for i in range(seg):
        for k in range(tube):
            (a, na), (b, nb), (cc, nc), (d, nd) = pt(i, k), pt(i + 1, k), pt(i + 1, k + 1), pt(i, k + 1)
            p.tri(mat, (a, b, cc), (na, nb, nc))
            p.tri(mat, (a, cc, d), (na, nc, nd))


def prism(p, mat, outline, h, c=(0, 0, 0), r=(0, 0, 0)):
    """Flat shape from a star-shaped outline in the local XZ plane, h thick along +Y, centred on c, turned by r."""
    M, c = rot(*r), np.asarray(c, float)
    pts = [np.array([x, 0.0, z]) for x, z in outline]
    ctr = sum(pts) / len(pts)
    up = np.array([0, 1.0, 0])
    for i in range(len(pts)):
        a, b = pts[i], pts[(i + 1) % len(pts)]
        top = [c + M @ (q + up * h / 2) for q in (ctr, a, b)]
        bot = [c + M @ (q - up * h / 2) for q in (ctr, b, a)]
        p.tri(mat, top, [M @ up] * 3)
        p.tri(mat, bot, [M @ -up] * 3)
        out = unit(np.cross(b - a, up)); mid = (a + b) / 2
        if np.dot(out, mid - ctr) < 0:
            out = -out
        p.quad(mat, c + M @ (a - up * h / 2), c + M @ (b - up * h / 2), c + M @ (b + up * h / 2), c + M @ (a + up * h / 2), M @ out)


def star_outline(R_, r_, n=5, turn=0.0):
    return [((R_ if i % 2 == 0 else r_) * math.sin(math.radians(turn) + math.pi * i / n),
             (R_ if i % 2 == 0 else r_) * math.cos(math.radians(turn) + math.pi * i / n)) for i in range(2 * n)]


def ribbon(p, mat, pts, widths, thick, up=(0, 1, 0)):
    """A flat strip along a centre line (wings, a boomerang): width across, thick along up."""
    pts = [np.asarray(q, float) for q in pts]; up = unit(up)
    rows = []
    for i, q in enumerate(pts):
        t = unit(pts[min(i + 1, len(pts) - 1)] - pts[max(i - 1, 0)])
        side = unit(np.cross(up, t)); u2 = unit(np.cross(t, side))
        w, h = widths[i] / 2, thick / 2
        rows.append((q + side * w + u2 * h, q - side * w + u2 * h, q - side * w - u2 * h, q + side * w - u2 * h, side, u2))
    for (a0, b0, c0, d0, s0, n0), (a1, b1, c1, d1, s1, n1) in zip(rows, rows[1:]):
        p.quad(mat, a0, a1, b1, b0, n0, n1, n1, n0)          # top
        p.quad(mat, d0, c0, c1, d1, -n0, -n0, -n1, -n1)      # bottom
        p.quad(mat, a0, d0, d1, a1, s0, s0, s1, s1)          # one side
        p.quad(mat, b0, b1, c1, c0, -s0, -s1, -s1, -s0)      # other side
    for (a, b, c, d, s, n), sign in ((rows[0], -1), (rows[-1], 1)):
        t = unit(np.cross(s, n)) * sign
        p.quad(mat, a, b, c, d, t)


def smooth_curve(ctrl, n=24):
    """Catmull-Rom through the control points."""
    P = [np.asarray(q, float) for q in ctrl]
    P = [P[0]] + P + [P[-1]]
    out = []
    for i in range(1, len(P) - 2):
        for k in range(n // (len(P) - 3) + 1):
            t = k / (n // (len(P) - 3) + 1)
            p0, p1, p2, p3 = P[i - 1], P[i], P[i + 1], P[i + 2]
            out.append(0.5 * (2 * p1 + (-p0 + p2) * t + (2 * p0 - 5 * p1 + 4 * p2 - p3) * t * t + (-p0 + 3 * p1 - 3 * p2 + p3) * t ** 3))
    out.append(P[-2])
    return out


# ---- models ------------------------------------------------------------------------------------------------------------
class Model:
    def __init__(self, name, rig):
        self.name, self.rig, self.parts = name, RIGS[rig], []
        self.pivotM = mat4(self.rig['pivot'])

    def stand(self):
        """Authored in the gun's root frame."""
        s = self.rig['stand']
        S = mat4(s[0], (0, s[1], 0)) if s else np.eye(4)
        return self._part('stand', np.linalg.inv(S), [np.eye(4)])

    def turret(self):
        """Authored in the pivot frame."""
        return self._part('turret', np.eye(4), [self.pivotM])

    def barrel_from_pivot(self):
        """Single barrel, authored in the pivot frame."""
        bp, br = self.rig['barrels'][0]
        return self._part('barrel', np.linalg.inv(mat4(bp, br)), [self.pivotM])

    def barrel_local(self):
        """Authored in the barrel's own frame; copied onto every barrel."""
        return self._part('barrel', np.eye(4), [self.pivotM @ mat4(bp, br) for bp, br in self.rig['barrels']])

    def _part(self, name, F, roots):
        p = Part(name, F, roots)
        self.parts.append(p)
        return p

    def write(self, folder):
        folder = pathlib.Path(folder); folder.mkdir(parents=True, exist_ok=True)
        used = sorted({m for p in self.parts for m, _, _ in p.tris})
        lines = ['# %s: built by tools/make_unusual_models.py' % self.name, 'mtllib %s.mtl' % self.name]
        vidx, nidx, vlines, nlines, body = {}, {}, [], [], []
        ntri = 0
        for p in self.parts:
            if not p.tris:
                continue
            body.append('o %s' % p.name)
            R, t = p.to_file[:3, :3], p.to_file[:3, 3]
            for m in used:
                tris = [tr for tr in p.tris if tr[0] == m]
                if not tris:
                    continue
                body.append('usemtl %s' % m)
                for _, ps, ns in tris:
                    ids = []
                    for q, n in zip(ps, ns):
                        q = tuple(round(float(x), 4) + 0.0 for x in R @ q + t)
                        n = tuple(round(float(x), 3) + 0.0 for x in R @ n)
                        if q not in vidx:
                            vidx[q] = len(vidx) + 1; vlines.append('v %.4f %.4f %.4f' % q)
                        if n not in nidx:
                            nidx[n] = len(nidx) + 1; nlines.append('vn %.3f %.3f %.3f' % n)
                        ids.append('%d//%d' % (vidx[q], nidx[n]))
                    body.append('f ' + ' '.join(ids)); ntri += 1
        (folder / ('%s.obj' % self.name)).write_text('\n'.join(lines + vlines + nlines + body) + '\n')
        (folder / ('%s.mtl' % self.name)).write_text(
            ''.join('newmtl %s\nKd %.3f %.3f %.3f\n\n' % ((m,) + MATS[m]) for m in used))
        return ntri, len(vidx)


def hangar():
    """The Hangar (40 mm Bofors, launches mini fighters): a little aircraft carrier on a gun carriage."""
    m = Model('hangar', 'bofors')
    s = m.stand()
    cyl(s, 'Grey', (0, -1.15, 0), (0, -1.07, 0), 0.75, seg=24)
    cyl(s, 'Navy', (0, -1.07, 0), (0, -0.97, 0), 0.62, seg=24)
    box(s, 'Navy', (0, -0.9, -0.1), (0.9, 0.14, 0.9))
    for x in (-1, 1):
        box(s, 'Navy', (0.5 * x, -0.33, -0.1), (0.1, 1.28, 1.0))
        box(s, 'Pink', (0.556 * x, -0.15, -0.1), (0.012, 0.07, 1.0))
        cyl(s, 'Pink', (0.45 * x, 0.23, -0.17), (0.62 * x, 0.23, -0.17), 0.11)
        box(s, 'Grey', (0.8 * x, -0.62, -0.13), (0.32, 0.06, 0.3))                  # seats
        box(s, 'Navy', (0.66 * x, -0.64, -0.13), (0.25, 0.05, 0.08))
    t = m.turret()
    box(t, 'Hull', (0, 0.02, 0.45), (0.62, 0.62, 2.3))                           # the ship's hull around the breech
    box(t, 'Deck', (0, 0.37, 0.4), (1.05, 0.06, 2.7))                              # flight deck z -0.95..1.75
    for i in range(7):
        box(t, 'Yellow', (0, 0.405, -0.75 + i * 0.38), (0.04, 0.012, 0.2))
    for x in (-1, 1):
        box(t, 'Pink', (0.5 * x, 0.405, 0.4), (0.03, 0.012, 2.7))
    box(t, 'Grey', (0.42, 0.62, 0.15), (0.14, 0.44, 0.44))                         # island
    box(t, 'Black', (0.42, 0.74, 0.15), (0.152, 0.05, 0.452))
    cyl(t, 'Steel', (0.42, 0.84, 0.1), (0.42, 1.15, 0.1), 0.015, seg=6)
    box(t, 'Pink', (0.42, 1.1, 0.02), (0.01, 0.07, 0.14))
    # a parked mini fighter at the back of the deck
    px, py = -0.2, 0.47
    cyl(t, 'PlaneBlue', (px, py, -0.85), (px, py, -0.37), 0.055, seg=10)
    cyl(t, 'Yellow', (px, py, -0.37), (px, py, -0.31), 0.055, 0.02, seg=10)
    box(t, 'PlaneBlue', (px, py - 0.015, -0.55), (0.62, 0.02, 0.13))
    box(t, 'PlaneBlue', (px, py, -0.82), (0.22, 0.015, 0.06))
    box(t, 'PlaneBlue', (px, py + 0.06, -0.82), (0.015, 0.12, 0.07))
    ellipsoid(t, 'Cyan', (px, py + 0.05, -0.55), (0.035, 0.03, 0.07), rings=6, seg=10)
    for x in (px - 0.2, px + 0.2):
        box(t, 'Pink', (x, py - 0.004, -0.55), (0.06, 0.022, 0.06))                # roundels
    b = m.barrel_local()
    cyl(b, 'Gunmetal', (0, 0.165, 1.2), (0, 0.165, 4.0), 0.06)
    cyl(b, 'Navy', (0, 0.165, 1.2), (0, 0.165, 2.0), 0.1)
    cyl(b, 'Pink', (0, 0.165, 3.85), (0, 0.165, 4.1), 0.085, 0.105)
    return m


def duckling(s, x, z, y=-1.08, k=1.0):
    ellipsoid(s, 'Duck', (x, y + 0.1 * k, z), (0.09 * k, 0.08 * k, 0.12 * k), rings=6, seg=10)
    sphere(s, 'Duck', (x, y + 0.22 * k, z + 0.08 * k), 0.065 * k, rings=6, seg=10)
    cyl(s, 'Orange', (x, y + 0.21 * k, z + 0.13 * k), (x, y + 0.205 * k, z + 0.19 * k), 0.03 * k, 0.008 * k, seg=8)
    for e in (-1, 1):
        sphere(s, 'Black', (x + 0.035 * e * k, y + 0.24 * k, z + 0.125 * k), 0.012 * k, rings=4, seg=6)
    cyl(s, 'Duck', (x, y + 0.11 * k, z - 0.1 * k), (x, y + 0.16 * k, z - 0.16 * k), 0.04 * k, 0.005 * k, seg=8)


def mother_duck():
    """Mother Duck (37 mm AT, launches mini PT boats): a rubber duck with a gun for a beak, ducklings behind."""
    m = Model('mother_duck', 'at37')
    s = m.stand()
    cyl(s, 'Water', (0, -1.12, 0), (0, -1.08, 0), 0.95, seg=32)
    torus(s, 'Pink', (0, -1.08, 0), 0.95, 0.03, seg=40, tube=6)
    cyl(s, 'Grey', (0, -1.08, 0), (0, -0.72, 0), 0.15)
    for x, z in ((0.42, -0.28), (0.5, -0.56), (0.38, -0.82)):
        duckling(s, x, z)
    t = m.turret()
    ellipsoid(t, 'Duck', (0, 0.25, -0.2), (0.42, 0.3, 0.58), rings=10, seg=16)
    cyl(t, 'Duck', (0, 0.33, -0.68), (0, 0.6, -0.92), 0.17, 0.02, seg=12)
    for x in (-1, 1):
        ellipsoid(t, 'Wing', (0.36 * x, 0.3, -0.22), (0.09, 0.17, 0.32), rings=8, seg=12)
    sphere(t, 'Duck', (0, 0.62, 0.33), 0.28, rings=10, seg=14)
    ellipsoid(t, 'Orange', (0, 0.5, 0.66), (0.17, 0.07, 0.2), rings=8, seg=14)
    for x in (-1, 1):
        sphere(t, 'Black', (0.16 * x, 0.74, 0.53), 0.05, rings=6, seg=10)
        sphere(t, 'White', (0.17 * x, 0.77, 0.575), 0.016, rings=4, seg=6)
        ellipsoid(t, 'Pink', (0.1 * x, 0.91, 0.3), (0.09, 0.06, 0.04), rings=6, seg=10)
    sphere(t, 'Pink', (0, 0.9, 0.3), 0.04, rings=6, seg=8)
    b = m.barrel_from_pivot()
    cyl(b, 'Gunmetal', (0, 0.465, 0.8), (0, 0.465, 1.95), 0.042)
    cyl(b, 'Pink', (0, 0.465, 1.86), (0, 0.465, 1.97), 0.06)
    return m


def kamikaze_gull():
    """Kamikaze Gull (20 mm Oerlikon, launches dive bombers): a seagull in flying goggles on a pier post."""
    m = Model('kamikaze_gull', 'oerlikon')
    s = m.stand()
    cyl(s, 'Grey', (0, -1.13, 0), (0, -1.08, 0), 0.38, seg=20)
    cyl(s, 'Wood', (0, -1.08, -0.15), (0, 0.12, -0.15), 0.15, seg=14)
    cyl(s, 'Leather', (0, 0.12, -0.15), (0, 0.15, -0.15), 0.16, seg=14)
    for y in (-0.35, -0.75):
        torus(s, 'Rope', (0, y, -0.15), 0.155, 0.025, seg=20, tube=6)
    box(s, 'Gunmetal', (0, 0.2, -0.3), (0.12, 0.16, 0.16))
    t = m.turret()
    ellipsoid(t, 'White', (0, 0.04, -0.05), (0.2, 0.19, 0.42), rings=10, seg=16)
    sphere(t, 'White', (0, 0.25, 0.3), 0.15, rings=10, seg=14)
    cyl(t, 'Yellow', (0, 0.22, 0.42), (0, 0.19, 0.7), 0.05, 0.012, seg=10)
    sphere(t, 'Red', (0, 0.185, 0.6), 0.018, rings=4, seg=6)
    torus(t, 'Leather', (0, 0.3, 0.3), 0.152, 0.018, seg=20, tube=6)              # goggle strap
    box(t, 'Leather', (0, 0.3, 0.45), (0.05, 0.02, 0.02))
    for x in (-1, 1):
        cyl(t, 'Leather', (0.075 * x, 0.3, 0.39), (0.075 * x, 0.3, 0.43), 0.058, seg=12)
        cyl(t, 'Pink', (0.075 * x, 0.3, 0.43), (0.075 * x, 0.3, 0.455), 0.048, seg=12)
        sphere(t, 'Black', (0.075 * x, 0.3, 0.458), 0.018, rings=4, seg=6)
    # red headband (the game's developer, 2026-10-30: "give him a little red headband too"): above the goggles, tipped up
    # at the front to clear the lenses, knotted at the back with two short tails blowing back
    hb_c, hb_R, hb_M = np.array([0, 0.355, 0.285]), 0.104, rot(-18, 0, 0)
    torus(t, 'Red', hb_c, hb_R, 0.02, r=(-18, 0, 0), seg=24, tube=6)
    knot = hb_c + hb_M @ np.array([0, 0, -hb_R - 0.012])
    sphere(t, 'Red', knot, 0.028, rings=5, seg=8)
    for x in (-1, 1):
        ribbon(t, 'Red', [knot, knot + (0.04 * x, -0.03, -0.1), knot + (0.07 * x, -0.09, -0.19)], [0.045, 0.04, 0.035], 0.01)
        ribbon(t, 'GullGrey', [(0.17 * x, 0.08, -0.05), (0.55 * x, 0.14, -0.1), (0.9 * x, 0.2, -0.2)], [0.36, 0.3, 0.16], 0.035)
        ribbon(t, 'Black', [(0.9 * x, 0.2, -0.2), (1.05 * x, 0.22, -0.27)], [0.16, 0.04], 0.03)
        cyl(t, 'Orange', (0.07 * x, -0.12, -0.02), (0.07 * x, -0.25, -0.02), 0.015, seg=6)
        box(t, 'Orange', (0.07 * x, -0.26, 0.03), (0.08, 0.015, 0.1))
    ribbon(t, 'White', [(0, 0.06, -0.42), (0, 0.08, -0.7)], [0.22, 0.3], 0.03)
    ribbon(t, 'Black', [(0, 0.08, -0.7), (0, 0.085, -0.76)], [0.3, 0.3], 0.03)
    b = m.barrel_from_pivot()
    cyl(b, 'Gunmetal', (0, 0, 0.3), (0, 0, 2.15), 0.032)
    cyl(b, 'Steel', (0, 0, 0.3), (0, 0, 0.85), 0.055)
    cyl(b, 'Pink', (0, 0, 2.0), (0, 0, 2.2), 0.04, 0.055)
    return m


def gull_bomber():
    """The Kamikaze Gull's dive bombers (David 2026-10-30: 'can they look like' the gun's gull): a flying seagull in the
    same pink goggles and red headband, wings in a gull's M, a little bomb in its feet. Not a gun: the mod hangs it on
    each spawned Dauntless (whose meshes it hides), so it is authored at the Dauntless's size, about 13 m across, nose
    along +Z, centred on the origin; the plane's own scale (0.6) shrinks it with the plane."""
    m = Model('gull_bomber', 'oerlikon')
    m.rig = dict(stand=None, pivot=(0, 0, 0), barrels=[], seats=[], shoot=[])
    m.pivotM = np.eye(4); m.preview_scale = 22
    g = m._part('gull', np.eye(4), [np.eye(4)])
    ellipsoid(g, 'White', (0, 0, 0), (1.2, 1.1, 2.6), rings=12, seg=18)                 # body
    hc = np.array([0, 0.75, 2.4]); hr = 0.95                                              # head
    sphere(g, 'White', hc, hr, rings=12, seg=16)
    cyl(g, 'Yellow', hc + (0, -0.15, 0.75), hc + (0, -0.35, 2.3), 0.32, 0.07, seg=12)     # beak
    sphere(g, 'Red', hc + (0, -0.3, 2.0), 0.1, rings=4, seg=8)
    torus(g, 'Leather', hc + (0, 0.3, 0), hr + 0.02, 0.11, seg=24, tube=6)                # goggles
    box(g, 'Leather', hc + (0, 0.3, 0.9), (0.3, 0.12, 0.12))
    for x in (-1, 1):
        cyl(g, 'Leather', hc + (0.45 * x, 0.3, 0.55), hc + (0.45 * x, 0.3, 0.8), 0.35, seg=14)
        cyl(g, 'Pink', hc + (0.45 * x, 0.3, 0.8), hc + (0.45 * x, 0.3, 0.95), 0.29, seg=14)
        sphere(g, 'Black', hc + (0.45 * x, 0.3, 0.97), 0.11, rings=4, seg=8)
    # red headband: above the goggles, tipped up at the front, the knot's tails streaming back in the wind
    hb_c, hb_R, hb_M = hc + (0, 0.62, -0.09), 0.64, rot(-18, 0, 0)
    torus(g, 'Red', hb_c, hb_R, 0.12, r=(-18, 0, 0), seg=24, tube=6)
    knot = hb_c + hb_M @ np.array([0, 0, -hb_R - 0.07])
    sphere(g, 'Red', knot, 0.17, rings=5, seg=8)
    for x in (-1, 1):
        ribbon(g, 'Red', [knot, knot + (0.25 * x, 0.05, -0.8), knot + (0.4 * x, -0.05, -1.6)], [0.28, 0.25, 0.22], 0.06)
    # wings: up from the body to the elbow, then down to black tips (a gull's M from the front)
    for x in (-1, 1):
        ribbon(g, 'GullGrey', [(0.9 * x, 0.35, 0.3), (3.0 * x, 1.15, 0.1), (5.6 * x, 0.55, -0.6)], [2.4, 2.0, 1.3], 0.22)
        ribbon(g, 'Black', [(5.6 * x, 0.55, -0.6), (6.5 * x, 0.35, -1.1)], [1.3, 0.35], 0.2)
    ribbon(g, 'White', [(0, 0.15, -2.3), (0, 0.25, -4.0)], [1.3, 2.0], 0.16)            # tail
    ribbon(g, 'Black', [(0, 0.25, -4.0), (0, 0.27, -4.35)], [2.0, 2.0], 0.15)
    # feet tucked under, gripping a little bomb with a red band
    ellipsoid(g, 'Gunmetal', (0, -1.55, 0.3), (0.42, 0.42, 1.05), rings=8, seg=12)
    torus(g, 'Red', (0, -1.55, 0.55), 0.43, 0.06, r=(90, 0, 0), seg=16, tube=5)
    for a in (0, 90):
        box(g, 'Gunmetal', (0, -1.55, -0.85), (0.9, 0.06, 0.45), r=(0, 0, a))
    for x in (-1, 1):
        cyl(g, 'Orange', (0.32 * x, -0.85, 0.2), (0.3 * x, -1.15, 0.35), 0.07, seg=6)
        box(g, 'Orange', (0.3 * x, -1.2, 0.45), (0.22, 0.08, 0.5))
    return m


def boomerang():
    """Boomerang (twin .50 Cal, misses come back): a painted gun tub and a big wooden boomerang over the guns.
    The twin guns share one barrel transform (listed twice), so they are part of the turret, not barrel* parts."""
    m = Model('boomerang', 'browning')
    s = m.stand()                                                                  # no stand: the root turns with the gun
    n = 20
    for i in range(n):
        a = 2 * math.pi * (i + 0.5) / n
        box(s, 'Ochre', (0.66 * math.cos(a), -0.375, 0.66 * math.sin(a)), (0.06, 0.95, 2 * math.pi * 0.66 / n * 1.06), (0, -math.degrees(a), 0))
    torus(s, 'Pink', (0, 0.1, 0), 0.66, 0.04, seg=40, tube=6)
    torus(s, 'Wood', (0, -0.85, 0), 0.66, 0.035, seg=40, tube=6)
    for row, y in enumerate((-0.2, -0.55)):
        for i in range(24):
            a = 2 * math.pi * (i + 0.5 * row) / 24
            box(s, 'Pink' if (i + row) % 2 == 0 else 'White', (0.692 * math.cos(a), y, 0.692 * math.sin(a)), (0.012, 0.045, 0.045), (0, -math.degrees(a), 0))
    cyl(s, 'Gunmetal', (0.14, -0.85, 0.48), (0.14, -0.8, 0.48), 0.15)
    cyl(s, 'Gunmetal', (0.14, -0.8, 0.48), (0.14, -0.02, 0.48), 0.06)
    t = m.turret()
    for x in (-0.29, 0.0):
        box(t, 'Gunmetal', (x, 0.05, -0.2), (0.13, 0.22, 0.8))
        cyl(t, 'Steel', (x, 0.05, 0.2), (x, 0.05, 0.78), 0.04)
        cyl(t, 'Gunmetal', (x, 0.05, 0.2), (x, 0.05, 1.2), 0.022, seg=10)
        cyl(t, 'Pink', (x, 0.05, 1.12), (x, 0.05, 1.22), 0.035, seg=10)
        cyl(t, 'Black', (x, 0.0, -0.6), (x, 0.0, -0.7), 0.025, seg=8)
        cyl(t, 'Steel', (x, 0.16, 0.1), (x, 0.33, 0.15), 0.02, seg=6)
    box(t, 'Gunmetal', (-0.145, -0.08, -0.1), (0.5, 0.08, 0.5))
    xc, y = -0.145, 0.35
    curve = smooth_curve([(xc - 0.8, y, -0.28), (xc - 0.45, y, 0.12), (xc, y, 0.42), (xc + 0.45, y, 0.12), (xc + 0.8, y, -0.28)], n=40)
    widths = [0.1 + 0.08 * math.sin(math.pi * i / (len(curve) - 1)) ** 0.5 for i in range(len(curve))]
    ribbon(t, 'Boomerang', curve, widths, 0.045)
    for i in range(2, len(curve) - 2, 3):
        q = curve[i]
        cyl(t, ['Pink', 'White', 'Ochre'][i % 3], (q[0], y + 0.02, q[2]), (q[0], y + 0.03, q[2]), 0.025, seg=8)
    return m


def fireworks():
    """Fireworks (40 mm Dual Bofors, colourful bursts): a red carriage with stars, a rocket rack, candy-striped barrels."""
    m = Model('fireworks', 'dualbofors')
    s = m.stand()
    cyl(s, 'Night', (0, -1.15, 0), (0, -1.07, 0), 0.75, seg=24)
    cyl(s, 'Gold', (0, -1.07, 0), (0, -0.97, 0), 0.62, seg=24)
    box(s, 'Red', (0, -0.9, -0.1), (0.9, 0.14, 0.9))
    for x in (-1, 1):
        box(s, 'Red', (0.5 * x, -0.33, -0.1), (0.1, 1.28, 1.0))
        cyl(s, 'Gold', (0.45 * x, 0.23, -0.17), (0.62 * x, 0.23, -0.17), 0.11)
        box(s, 'Grey', (0.8 * x, -0.62, -0.13), (0.32, 0.06, 0.3))
        box(s, 'Red', (0.66 * x, -0.64, -0.13), (0.25, 0.05, 0.08))
        for (yy, zz, rr, col) in ((-0.45, -0.3, 0.13, 'Gold'), (-0.05, 0.15, 0.1, 'Pink'), (-0.75, 0.22, 0.08, 'Cyan')):
            prism(s, col, star_outline(rr, rr * 0.42), 0.014, (0.556 * x, yy, zz), (0, 0, -90 * x))
    t = m.turret()
    box(t, 'Night', (0, 0.03, 0.45), (0.9, 0.62, 2.3))
    for x in (-1, 1):
        box(t, 'Pink', (0.452 * x, 0.03, 0.45), (0.01, 0.08, 2.3))
    for (xx, zz, rr, col) in ((-0.3, 1.2, 0.08, 'Gold'), (0.28, 0.9, 0.07, 'Pink'), (-0.1, 0.6, 0.06, 'Cyan'), (0.3, 1.45, 0.05, 'Green'), (0.05, 1.3, 0.06, 'Gold')):
        prism(t, col, star_outline(rr, rr * 0.42), 0.012, (xx, 0.345, zz))
    box(t, 'Wood', (0, 0.37, -0.25), (0.66, 0.05, 0.5))                           # rocket rack
    d = np.array([0, math.sin(math.radians(40)), math.cos(math.radians(40))])
    for i, col in enumerate(('Red', 'Green', 'Purple', 'Cyan', 'Gold')):
        base = np.array([-0.24 + i * 0.12, 0.42, -0.35])
        cyl(t, 'Wood', base - 0.2 * d, base, 0.008, seg=5)
        cyl(t, col, base, base + 0.32 * d, 0.035, seg=10)
        cyl(t, 'Pink' if i % 2 else 'White', base + 0.32 * d, base + 0.43 * d, 0.035, 0.0, seg=10)
    b = m.barrel_local()
    z, i = 1.2, 0
    while z < 3.95:
        cyl(b, 'Pink' if i % 2 == 0 else 'White', (0, 0.165, z), (0, 0.165, min(z + 0.25, 3.95)), 0.065, seg=14, caps=False)
        z += 0.25; i += 1
    cyl(b, 'Black', (0, 0.165, 3.949), (0, 0.165, 3.95), 0.065, seg=14)
    prism(b, 'Gold', star_outline(0.16, 0.075), 0.06, (0, 0.165, 3.97), (90, 0, 0))
    return m


def orbital():
    """Orbital (Elco Thunderbolt, shells circle the boat): a small ringed planet with moons, on a starry drum."""
    m = Model('orbital', 'elco')
    s = m.stand()
    cyl(s, 'Night', (0, -1.09, 0), (0, -0.98, 0), 0.82, seg=28)
    cyl(s, 'Space', (0, -0.98, 0), (0, -0.3, 0), 0.58, seg=28)
    cyl(s, 'Cyan', (0, -0.66, 0), (0, -0.6, 0), 0.595, seg=28)
    cyl(s, 'Pink', (0, -0.3, 0), (0, -0.24, 0), 0.62, seg=28)
    rng = np.random.default_rng(7)
    for _ in range(16):
        a, y = rng.uniform(0, 2 * math.pi), rng.uniform(-0.95, -0.35)
        if -0.68 < y < -0.58:
            continue
        sphere(s, 'White', (0.585 * math.cos(a), y, 0.585 * math.sin(a)), 0.016, rings=3, seg=5)
    t = m.turret()
    c = np.array([0.1, 0.05, -0.35])
    sphere(t, 'Purple', c, 0.5, rings=12, seg=18)
    for d, rr in (((0.5, 0.6, 0.62), 0.08), ((-0.7, 0.3, 0.64), 0.06), ((0.2, -0.3, 0.93), 0.07)):
        sphere(t, 'DarkPurple', c + unit(d) * 0.48, rr, rings=5, seg=8)
    r1, r2 = (20, 0, -12), (-15, 0, 18)
    torus(t, 'Pink', c, 0.92, 0.035, r1, seg=40, tube=6)
    torus(t, 'Cyan', c, 1.08, 0.018, r2, seg=44, tube=5)
    for a, col in ((40, 'Gold'), (160, 'White'), (280, 'Cyan')):
        q = c + rot(*r1) @ np.array([0.92 * math.cos(math.radians(a)), 0, 0.92 * math.sin(math.radians(a))])
        sphere(t, col, q, 0.075, rings=6, seg=10)
    b = m.barrel_local()
    box(b, 'Space', (0, 0, 0.45), (0.09, 0.09, 0.5))
    cyl(b, 'Space', (0, 0, 0.7), (0, 0, 2.3), 0.03, seg=10)
    for z in (1.05, 1.55, 2.0):
        cyl(b, 'Cyan', (0, 0, z), (0, 0, z + 0.04), 0.045, seg=10)
    cyl(b, 'Pink', (0, 0, 2.25), (0, 0, 2.42), 0.035, 0.05, seg=10)
    return m


def magnet():
    """Magnet (37 mm Autocannon, shells home in and hits pull ships closer): a big horseshoe magnet round the barrel."""
    m = Model('magnet', 'autocannon')
    s = m.stand()
    cyl(s, 'Grey', (0, -1.12, 0), (0, -1.04, 0), 0.5, seg=24)
    cyl(s, 'Gunmetal', (0, -1.04, 0), (0, -0.32, 0), 0.2, seg=16)
    cyl(s, 'Gunmetal', (0, -0.32, 0), (0, -0.22, 0), 0.3, seg=20)
    t = m.turret()
    box(t, 'Gunmetal', (0, 0.07, -0.25), (0.36, 0.34, 1.3))
    y = 0.05
    for x in (-1, 1):
        cyl(t, 'Red', (0.33 * x, y, 0.0), (0.33 * x, y, 0.7), 0.11, seg=16, caps=False)
        cyl(t, 'Pink', (0.33 * x, y, 0.5), (0.33 * x, y, 0.56), 0.114, seg=16, caps=False)
        cyl(t, 'Steel', (0.33 * x, y, 0.7), (0.33 * x, y, 0.92), 0.112, seg=16)
    torus(t, 'Red', (0, y, 0.0), 0.33, 0.11, seg=20, tube=12, a0=180, a1=360)
    b = m.barrel_from_pivot()
    cyl(b, 'Gunmetal', (0, y, 0.3), (0, y, 1.8), 0.045)
    cyl(b, 'Pink', (0, y, 1.0), (0, y, 1.05), 0.055)
    cyl(b, 'Steel', (0, y, 1.7), (0, y, 1.86), 0.065)
    return m


def home_run():
    """Home Run (37 mm Autocannon, kills fly 60-80 m up): home plate on a dirt mound, a catcher's mitt, a bat for a barrel."""
    m = Model('home_run', 'autocannon')
    s = m.stand()
    cyl(s, 'Ochre', (0, -1.12, 0), (0, -1.05, 0), 0.55, 0.5, seg=24)              # the dirt
    prism(s, 'White', [(-0.22, -0.12), (0.22, -0.12), (0.22, 0.08), (0, 0.26), (-0.22, 0.08)], 0.03, c=(0, -1.035, 0))
    cyl(s, 'Gunmetal', (0, -1.04, 0), (0, -0.32, 0), 0.2, seg=16)
    cyl(s, 'Gunmetal', (0, -0.32, 0), (0, -0.22, 0), 0.3, seg=20)
    t = m.turret()
    y = 0.05
    ellipsoid(t, 'Leather', (0, y, -0.25), (0.32, 0.27, 0.62), rings=10, seg=18)  # the mitt round the breech
    for k in range(4):                                                             # its fingers, up the back
        x = -0.2 + 0.133 * k
        ellipsoid(t, 'Leather', (x, y + 0.24, -0.45), (0.065, 0.2, 0.08), r=(-20, 0, 0), rings=6, seg=10)
    torus(t, 'Gold', (0, y, 0.3), 0.18, 0.025, r=(90, 0, 0), seg=20, tube=6)      # laces round the barrel hole
    sphere(t, 'White', (0.16, y + 0.3, -0.05), 0.12)                               # a ball in the glove
    torus(t, 'Red', (0.16, y + 0.3, -0.05), 0.12, 0.012, r=(0, 0, 70), seg=20, tube=4)
    torus(t, 'Pink', (0, y, -0.25), 0.33, 0.03, r=(90, 0, 0), seg=24, tube=6)     # the pink stripe of the tier
    b = m.barrel_from_pivot()
    cyl(b, 'Wood', (0, y, 0.3), (0, y, 0.33), 0.06, seg=14)                        # the knob
    cyl(b, 'Black', (0, y, 0.33), (0, y, 0.7), 0.04, seg=14, caps=False)           # grip tape
    cyl(b, 'Wood', (0, y, 0.7), (0, y, 1.35), 0.04, 0.085, seg=14, caps=False)
    cyl(b, 'Wood', (0, y, 1.35), (0, y, 1.8), 0.085, 0.09, seg=14)
    cyl(b, 'Pink', (0, y, 1.5), (0, y, 1.56), 0.093, seg=14, caps=False)
    return m


def fire_mission():
    """Fire Mission (3-inch, every 6th hit calls a barrage): a giant flare pistol on a spotter's pedestal with a radio."""
    m = Model('fire_mission', 'us3')
    s = m.stand()
    cyl(s, 'Gunmetal', (0, -1.32, 0), (0, -1.24, 0), 0.75, 0.7, seg=24)
    cyl(s, 'Gunmetal', (0, -1.24, 0), (0, -0.3, 0), 0.18, seg=16)
    cyl(s, 'Pink', (0, -0.75, 0), (0, -0.7, 0), 0.2, seg=16, caps=False)
    cyl(s, 'Gunmetal', (0, -0.3, 0), (0, -0.2, 0), 0.32, seg=20)
    box(s, 'Olive', (0.45, -1.02, -0.2), (0.3, 0.42, 0.22))                       # the field radio
    box(s, 'Black', (0.45, -0.94, -0.088), (0.22, 0.12, 0.01))
    cyl(s, 'Steel', (0.55, -0.81, -0.25), (0.62, -0.1, -0.3), 0.008, seg=5)       # its whip aerial
    box(s, 'Wood', (-0.45, -1.13, -0.15), (0.36, 0.2, 0.26))                       # a crate of flare shells
    for i, col in enumerate(('Red', 'Green', 'Red', 'Yellow')):
        x = -0.57 + 0.08 * i
        cyl(s, col, (x, -1.03, -0.15), (x, -0.95, -0.15), 0.03, seg=8)
    t = m.turret()
    y, x0 = 0.22, -0.04
    box(t, 'Orange', (x0, y, -0.35), (0.42, 0.42, 0.9))                            # the pistol frame round the breech
    box(t, 'Pink', (x0, y + 0.215, -0.35), (0.3, 0.012, 0.8))
    box(t, 'Gunmetal', (x0, -0.15, -0.2), (0.3, 0.3, 0.3))                         # the trunnion block on the pedestal
    box(t, 'Black', (x0, y - 0.36, -0.62), (0.3, 0.6, 0.28), r=(-18, 0, 0))       # the grip, raked back
    for k in range(4):
        box(t, 'Gunmetal', (x0, y - 0.18 - 0.11 * k, -0.62 + 0.035 * k - 0.145), (0.31, 0.03, 0.02), r=(-18, 0, 0))
    torus(t, 'Orange', (x0, y - 0.28, -0.3), 0.13, 0.025, r=(0, 0, 90), seg=16, tube=6, a0=180, a1=360)   # trigger guard
    box(t, 'Gunmetal', (x0, y - 0.25, -0.32), (0.04, 0.14, 0.04), r=(15, 0, 0))   # trigger
    box(t, 'Gunmetal', (x0, y + 0.25, -0.82), (0.08, 0.16, 0.08), r=(-30, 0, 0))  # hammer
    b = m.barrel_from_pivot()
    cyl(b, 'Orange', (x0, y, 0.1), (x0, y, 2.5), 0.16, seg=18)
    cyl(b, 'Orange', (x0, y, 2.5), (x0, y, 2.85), 0.16, 0.2, seg=18, caps=False)  # the flared muzzle
    cyl(b, 'Black', (x0, y, 2.84), (x0, y, 2.86), 0.17, seg=18)                   # the bore, dark
    cyl(b, 'Pink', (x0, y, 2.0), (x0, y, 2.1), 0.17, seg=18, caps=False)
    cyl(b, 'Red', (x0, y, 1.2), (x0, y, 1.3), 0.17, seg=18, caps=False)
    return m


def up_up_away():
    """Up, Up and Away (twin .30 Cal, hits swell ships until they float off): a wicker balloon basket, helium tanks,
    nozzle muzzles and three balloons tied to the guns."""
    m = Model('up_up_away', 'm1919')
    s = m.stand()                                                                  # no stand: the root turns with the gun
    n = 18
    for i in range(n):                                                             # the wicker basket
        a = 2 * math.pi * (i + 0.5) / n
        box(s, 'Ochre' if i % 2 else 'Wood', (0.6 * math.cos(a), -0.48, 0.6 * math.sin(a)), (0.06, 0.74, 2 * math.pi * 0.6 / n * 1.06), (0, -math.degrees(a), 0))
    torus(s, 'Wood', (0, -0.1, 0), 0.6, 0.045, seg=36, tube=6)
    torus(s, 'Pink', (0, -0.45, 0), 0.62, 0.025, seg=36, tube=6)
    torus(s, 'Wood', (0, -0.85, 0), 0.6, 0.04, seg=36, tube=6)
    cyl(s, 'Gunmetal', (0.14, -0.85, 0.1), (0.14, -0.02, 0.1), 0.06)
    t = m.turret()
    for x in (-0.30, 0.02):                                                        # the guns, with nozzle muzzles
        box(t, 'Gunmetal', (x, 0.0, -0.15), (0.12, 0.18, 0.6))
        cyl(t, 'Steel', (x, 0.0, 0.15), (x, 0.0, 0.85), 0.025, seg=10)
        cyl(t, 'Pink', (x, 0.0, 0.85), (x, 0.0, 0.97), 0.03, 0.055, seg=12)
        cyl(t, 'Black', (x, 0.0, -0.45), (x, 0.0, -0.55), 0.03, seg=8)
    cyl(t, 'White', (-0.14, -0.1, -0.45), (-0.14, -0.1, 0.25), 0.07, seg=14)      # the helium tank between them
    sphere(t, 'White', (-0.14, -0.1, 0.25), 0.07, rings=6, seg=14)
    cyl(t, 'Pink', (-0.14, -0.1, -0.1), (-0.14, -0.1, -0.05), 0.073, seg=14, caps=False)
    cyl(t, 'Steel', (-0.14, -0.1, 0.32), (-0.14, -0.1, 0.38), 0.02, seg=8)
    knot = (-0.14, 0.12, -0.35)
    box(t, 'Gunmetal', (-0.14, 0.1, -0.35), (0.06, 0.06, 0.06))
    for col, c in (('Pink', (-0.55, 1.05, -0.65)), ('Cyan', (-0.1, 1.25, -0.8)), ('Yellow', (0.3, 1.0, -0.6))):
        cyl(t, 'White', knot, (c[0], c[1] - 0.24, c[2]), 0.006, seg=4, caps=False)   # the string
        ellipsoid(t, col, c, (0.17, 0.21, 0.17), rings=8, seg=12)
        cyl(t, col, (c[0], c[1] - 0.2, c[2]), (c[0], c[1] - 0.25, c[2]), 0.02, 0.03, seg=6)   # the tied neck
    return m


def big_brother():
    """Big Brother (3-inch, every 40th shot calls in a Fletcher): a destroyer's enclosed gun house with a watching eye on
    the roof and a mini Fletcher steaming round the pedestal. On the 3-inch since 2026-11-16: the 5-inch only fits a
    Heavy+ slot, which no PT boat has; the barrel follows the 3-inch bore (x -0.04, y 0.22 in the pivot frame)."""
    m = Model('big_brother', 'us3')
    s = m.stand()
    cyl(s, 'Grey', (0, -1.317, 0), (0, -1.2, 0), 0.85, 0.8, seg=24)
    cyl(s, 'Navy', (0, -1.2, 0), (0, -0.62, 0), 0.4, 0.34, seg=18)
    cyl(s, 'Pink', (0, -0.95, 0), (0, -0.9, 0), 0.405, seg=18, caps=False)
    box(s, 'Water', (0, -1.26, 1.15), (1.9, 0.11, 0.44))                          # a strip of sea for the little brother
    hull = [(-0.82, 0), (-0.62, -0.11), (0.55, -0.11), (0.84, 0), (0.55, 0.11), (-0.62, 0.11)]
    prism(s, 'Hull', hull, 0.14, c=(0, -1.13, 1.15))
    box(s, 'Pink', (0, -1.13, 1.15), (1.2, 0.025, 0.225))                          # the tier's stripe at the waterline
    box(s, 'Deck', (0, -1.055, 1.15), (1.3, 0.012, 0.18))
    box(s, 'Hull', (0.36, -0.99, 1.15), (0.34, 0.12, 0.15))                        # forward superstructure and bridge
    box(s, 'Grey', (0.44, -0.9, 1.15), (0.14, 0.08, 0.13))
    box(s, 'Black', (0.51, -0.9, 1.15), (0.01, 0.025, 0.12))
    cyl(s, 'Steel', (0.36, -0.93, 1.15), (0.36, -0.68, 1.15), 0.01, seg=5)
    for x in (0.08, -0.1):                                                         # the two stacks
        cyl(s, 'Grey', (x, -1.05, 1.15), (x, -0.86, 1.15), 0.04, seg=10)
        cyl(s, 'Black', (x, -0.861, 1.15), (x, -0.85, 1.15), 0.034, seg=10)
    for x, d in ((0.66, 1), (0.53, 1), (-0.42, -1), (-0.56, -1), (-0.7, -1)):     # five 5-inch mounts
        cyl(s, 'Grey', (x, -1.05, 1.15), (x, -1.0, 1.15), 0.045, seg=10)
        cyl(s, 'Steel', (x, -1.025, 1.15), (x + 0.12 * d, -1.02, 1.15), 0.007, seg=5)
    t = m.turret()
    side = [(-0.45, -1.3), (0.45, -1.3), (0.55, -0.3), (0.3, 1.0), (-0.45, 1.0)]   # the gun house, side on (y, z)
    prism(t, 'Hull', side, 1.7, r=(0, 0, 90))
    for x in (-0.86, 0.86):
        box(t, 'Pink', (x, 0.1, -0.15), (0.012, 0.08, 2.1))
    box(t, 'Black', (0, 0.0, 1.005), (0.36, 0.5, 0.02))                            # the gun port
    box(t, 'Grey', (0, 0.6, -0.8), (0.5, 0.12, 0.4))                               # the pointer's hood
    sphere(t, 'White', (0, 0.62, -0.1), 0.24, rings=10, seg=16)                    # Big Brother is watching
    cyl(t, 'Pink', (0, 0.62, 0.1), (0, 0.62, 0.14), 0.13, seg=16)
    cyl(t, 'Black', (0, 0.62, 0.135), (0, 0.62, 0.15), 0.06, seg=12)
    cyl(t, 'Steel', (0.5, 0.55, -1.1), (0.5, 1.25, -1.1), 0.015, seg=6)           # a pennant at the back
    box(t, 'Pink', (0.5, 1.15, -1.28), (0.01, 0.12, 0.34))
    b = m.barrel_from_pivot()
    bx, by = -0.04, 0.22                                                           # the 3-inch bore
    cyl(b, 'Grey', (bx, by, 0.95), (bx, by, 1.25), 0.17, 0.12, seg=16)             # the blast bag
    cyl(b, 'Steel', (bx, by, 1.25), (bx, by, 2.87), 0.085, 0.075, seg=16)
    cyl(b, 'Pink', (bx, by, 2.57), (bx, by, 2.67), 0.088, seg=16, caps=False)
    cyl(b, 'Black', (bx, by, 2.865), (bx, by, 2.88), 0.07, seg=14)
    return m


MODELS = [hangar, mother_duck, kamikaze_gull, gull_bomber, boomerang, fireworks, orbital, magnet, home_run, fire_mission, up_up_away, big_brother]


# ---- previews: flat-shaded painter's renders from three sides, seats red and stock shoot points cyan -------------------
def preview(model, path):
    from PIL import Image, ImageDraw
    tris = []
    for p in model.parts:
        for M in p.to_root:
            R, t = M[:3, :3], M[:3, 3]
            for mat, ps, ns in p.tris:
                tris.append((mat, [R @ q + t for q in ps], R @ unit(sum(ns))))
    marks = [(np.array(s, float), (255, 40, 40)) for s in model.rig['seats']]
    marks += [(model.pivotM[:3, :3] @ np.array(s, float) + model.pivotM[:3, 3], (0, 220, 255)) for s in model.rig['shoot']]
    views = [('3/4', (1.0, 0.75, 1.3)), ('right side', (1, 0.08, 0)), ('top', (0, 1, 0.001)), ('from behind', (0.15, 0.35, -1))]
    W, H, scale = 360, 360, getattr(model, 'preview_scale', 70)
    img = Image.new('RGB', (W * len(views), H), (235, 238, 242))
    dr = ImageDraw.Draw(img)
    light = unit([0.35, 0.9, 0.45])
    for vi, (label, eye) in enumerate(views):
        fwd = -unit(eye); right = unit(np.cross([0, 1, 0], fwd)); upv = np.cross(fwd, right)
        Vt = np.array([right, upv, fwd])                                           # screen x, screen y, depth (Unity axes)
        items = []
        for mat, ps, n in tris:
            q = [Vt @ x for x in ps]
            depth = sum(x[2] for x in q) / 3
            shade = 0.45 + 0.55 * max(0.0, float(np.dot(n, light)))
            col = tuple(int(255 * min(1, c * shade)) for c in MATS[mat])
            items.append((depth, [(W * vi + W / 2 + x[0] * scale, H * 0.6 - x[1] * scale) for x in q], col))
        for depth, poly, col in sorted(items, key=lambda it: -it[0]):
            dr.polygon(poly, fill=col)
        for q, col in marks:
            v = Vt @ q
            x, y = W * vi + W / 2 + v[0] * scale, H * 0.6 - v[1] * scale
            dr.ellipse((x - 3, y - 3, x + 3, y + 3), fill=col)
        dr.text((W * vi + 6, 6), '%s  %s' % (model.name, label), fill=(20, 20, 20))
    img.save(path)


def main():
    out = pathlib.Path(sys.argv[1])
    prev = pathlib.Path(sys.argv[sys.argv.index('--preview') + 1]) if '--preview' in sys.argv else None
    for make in MODELS:
        m = make()
        ntri, nv = m.write(out)
        print('%-14s %5d triangles %5d vertices  parts: %s' % (m.name, ntri, nv, ', '.join('%s(%d)' % (p.name, len(p.tris)) for p in m.parts)))
        if prev:
            prev.mkdir(parents=True, exist_ok=True)
            preview(m, prev / ('%s.png' % m.name))


if __name__ == '__main__':
    main()
