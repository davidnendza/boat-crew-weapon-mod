"""Builds the models (OBJ + MTL) of the six extra turret guns, plus preview sheets next to the stock guns.

The guns sit in the PT boat's turret slots on three stock bases: the twin Lewis, the twin M2 Browning and the 20 mm
Oerlikon in its tub. Each model replaces only the gun: the item's model_keep= leaves the stock ring mount, belts,
pedestal, cradle and shoulder rests visible, so the new gun stands on the game's own parts.

Looks follow the game's own guns: the Type 96 and the 37 mm M4 are the game's meshes, moved onto the Oerlikon's aim
point; the Vickers K, Vickers .50, Type 93 and Flak 38 are built here at the stock guns' level of detail (1,000 to
3,500 triangles a gun). Every part is painted with a material the boat already has: .mtl names 'game.<name>' (spaces as
'_', alternatives split by '|') are swapped for the game material of that name by the mod (ApplyModel); the Kd colour
is the fallback. UVs are chosen per paint so the game's palette textures give the right colour (Lewis: dark metal from
the white area, wood from the green one).

The mod places each OBJ object by its name (ApplyModel in weaponmod.cpp): barrel* on every Barrel.transform (recoils),
stand* or base* on GunMount.stand, yaw* on the traverse at the pivot (turns, does not elevate), the rest on
GunMount.pivotPoint. Parts are authored in the stock gun's pivot frame
(metres, +Y up, +Z forward, Unity axes) and converted to the frame they hang on.

Usage: python make_turret_models.py <models folder> [--preview <folder>]   (needs the game installed and UnityPy)
"""
import math
import pathlib
import sys

import numpy as np

import gamemesh as gm


# ---- paints: a game material plus how UVs are laid on it ---------------------------------------------------------------
class Paint:
    def __init__(self, game, kd, uv):
        self.game = game if isinstance(game, tuple) else (game,)   # first name used for previews
        self.kd, self.uv = kd, uv

    @property
    def mtl(self):
        return 'game.' + '|'.join(g.replace(' ', '_') for g in self.game)


def box_uv(scale=1.0, origin=(0.0, 0.0)):
    """Planar UVs by the normal's main axis, `scale` UV units per metre (the stock guns run about 1 per metre)."""
    def f(p, n):
        a = int(np.argmax(np.abs(n)))
        u, v = [p[i] for i in range(3) if i != a]
        return (origin[0] + u * scale, origin[1] + v * scale)
    return f


def region_uv(center, half):
    """UVs squeezed into a small patch of a palette texture (one colour), keeping a little variation for detail maps."""
    cx, cy = center

    def f(p, n):
        a = int(np.argmax(np.abs(n)))
        u, v = [p[i] for i in range(3) if i != a]
        return (cx + half * math.tanh(u * 1.5), cy + half * math.tanh(v * 1.5))
    return f


METAL = Paint('Dark MG Metal', (0.30, 0.31, 0.33), box_uv(1.0))
LEWIS_METAL = Paint('Lewis', (0.22, 0.22, 0.23), region_uv((0.50, 0.85), 0.08))      # white -> _Color1, dark metal
LEWIS_WOOD = Paint('Lewis', (0.55, 0.34, 0.17), region_uv((0.15, 0.72), 0.08))       # green -> _Color3, wood
LEATHER = Paint('Oerlikon Leather', (0.55, 0.33, 0.22), box_uv(2.0))
M4 = Paint(('M4 37 MM Autocannon', 'Dark MG Metal'), (0.42, 0.43, 0.45), box_uv(1.0))
ROUNDS = Paint('Crew Bandolier', (0.78, 0.62, 0.32), box_uv(1.0))


# ---- maths -------------------------------------------------------------------------------------------------------------
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


def basis(axis):
    a = unit(axis)
    u = unit(np.cross(a, [0, 1, 0] if abs(a[1]) < 0.9 else [1, 0, 0]))
    return a, u, np.cross(a, u)


# ---- parts and primitives ----------------------------------------------------------------------------------------------
class Part:
    """Triangles (paint or game material, 3 points, 3 normals, 3 uvs) authored in the stock gun's pivot frame."""

    def __init__(self, name):
        self.name, self.tris = name, []

    def tri(self, paint, ps, ns, outward=None, uvs=None):
        ps = [np.asarray(p, float) for p in ps]
        ns = [unit(n) for n in ns]
        face = np.cross(ps[1] - ps[0], ps[2] - ps[0])
        if np.linalg.norm(face) < 1e-12:
            return
        ref = outward if outward is not None else ns[0] + ns[1] + ns[2]
        if np.dot(face, ref) < 0:                      # front faces: cross product along the outward normal (as in game)
            ps[1], ps[2], ns[1], ns[2] = ps[2], ps[1], ns[2], ns[1]
            if uvs is not None:
                uvs = [uvs[0], uvs[2], uvs[1]]
        if uvs is None:
            fn = unit(face) if np.dot(face, ref) >= 0 else -unit(face)
            uvs = [paint.uv(p, fn) for p in ps]
        self.tris.append((paint, ps, ns, [np.asarray(u, float) for u in uvs]))

    def quad(self, paint, a, b, c, d, na, nb=None, nc=None, nd=None):
        nb = na if nb is None else nb
        nc = na if nc is None else nc
        nd = na if nd is None else nd
        self.tri(paint, (a, b, c), (na, nb, nc))
        self.tri(paint, (a, c, d), (na, nc, nd))


def box(p, paint, c, s, r=(0, 0, 0)):
    R, c, h = rot(*r), np.asarray(c, float), np.asarray(s, float) / 2
    for axis in range(3):
        for sign in (-1, 1):
            n = np.zeros(3); n[axis] = sign
            u, v = [i for i in range(3) if i != axis]
            corners = []
            for su, sv in ((-1, -1), (1, -1), (1, 1), (-1, 1)):
                q = np.zeros(3); q[axis] = sign * h[axis]; q[u] = su * h[u]; q[v] = sv * h[v]
                corners.append(c + R @ q)
            p.quad(paint, *corners, R @ n)


def lathe(p, paint, p0, axis, profile, seg=16, caps=True, smooth=True):
    """Turned part: profile = [(distance along axis, radius), ...] from p0. Hard edges where the radius jumps."""
    a, u, v = basis(axis)
    p0 = np.asarray(p0, float)
    angles = [2 * math.pi * i / seg for i in range(seg + 1)]
    dirs = [u * math.cos(t) + v * math.sin(t) for t in angles]
    for (t0, r0), (t1, r1) in zip(profile, profile[1:]):
        if abs(t1 - t0) < 1e-9 and abs(r1 - r0) < 1e-9:
            continue
        L = math.hypot(t1 - t0, r1 - r0)
        for i in range(seg):
            d0, d1 = dirs[i], dirs[i + 1]
            n0 = unit(d0 * (t1 - t0) - a * (r1 - r0)) if L > 1e-9 else d0
            n1 = unit(d1 * (t1 - t0) - a * (r1 - r0)) if L > 1e-9 else d1
            if not smooth:
                n0 = n1 = unit((n0 + n1) / 2)
            q00, q01 = p0 + a * t0 + d0 * r0, p0 + a * t0 + d1 * r0
            q10, q11 = p0 + a * t1 + d0 * r1, p0 + a * t1 + d1 * r1
            if r0 > 1e-6 and r1 > 1e-6:
                p.tri(paint, (q00, q01, q11), (n0, n1, n1))
                p.tri(paint, (q00, q11, q10), (n0, n1, n0))
            elif r0 > 1e-6 or r1 > 1e-6:
                tip = p0 + a * (t1 if r1 <= 1e-6 else t0)
                if r1 <= 1e-6:
                    p.tri(paint, (q00, q01, tip), (n0, n1, unit(n0 + n1)))
                else:
                    p.tri(paint, (q10, q11, tip), (n0, n1, unit(n0 + n1)))
    if caps:
        for (t, r), sign in ((profile[0], -1), (profile[-1], 1)):
            if r > 1e-6:
                c = p0 + a * t
                for i in range(seg):
                    p.tri(paint, (c, c + dirs[i] * r, c + dirs[i + 1] * r), (a * sign,) * 3)


def cyl(p, paint, p0, p1, r0, r1=None, seg=14, caps=True):
    r1 = r0 if r1 is None else r1
    p0, p1 = np.asarray(p0, float), np.asarray(p1, float)
    L = np.linalg.norm(p1 - p0)
    lathe(p, paint, p0, p1 - p0, [(0, r0), (L, r1)], seg, caps)


def _earclip(pts):
    """Triangulates a simple polygon (2D points, either winding). Returns index triples."""
    n = len(pts)
    idx = list(range(n))
    area = sum(pts[i][0] * pts[(i + 1) % n][1] - pts[(i + 1) % n][0] * pts[i][1] for i in range(n))
    ccw = area > 0
    out = []
    guard = 0
    while len(idx) > 3 and guard < 10000:
        guard += 1
        for k in range(len(idx)):
            i0, i1, i2 = idx[k - 1], idx[k], idx[(k + 1) % len(idx)]
            a, b, c = pts[i0], pts[i1], pts[i2]
            cross = (b[0] - a[0]) * (c[1] - a[1]) - (b[1] - a[1]) * (c[0] - a[0])
            if (cross > 0) != ccw or abs(cross) < 1e-14:
                continue
            ok = True
            for j in idx:
                if j in (i0, i1, i2):
                    continue
                q = pts[j]
                d1 = (b[0] - a[0]) * (q[1] - a[1]) - (b[1] - a[1]) * (q[0] - a[0])
                d2 = (c[0] - b[0]) * (q[1] - b[1]) - (c[1] - b[1]) * (q[0] - b[0])
                d3 = (a[0] - c[0]) * (q[1] - c[1]) - (a[1] - c[1]) * (q[0] - c[0])
                if ccw and d1 >= 0 and d2 >= 0 and d3 >= 0 or not ccw and d1 <= 0 and d2 <= 0 and d3 <= 0:
                    ok = False
                    break
            if ok:
                out.append((i0, i1, i2))
                idx.pop(k)
                break
        else:
            break
    if len(idx) == 3:
        out.append(tuple(idx))
    return out


def extrude(p, paint, outline, ax_u, ax_v, ax_w, w0, w1, origin=(0, 0, 0)):
    """A flat shape: outline [(u, v)] in the plane of ax_u/ax_v, pushed from w0 to w1 along ax_w."""
    U, V, W, O = (np.asarray(x, float) for x in (ax_u, ax_v, ax_w, origin))
    P3 = lambda q, w: O + U * q[0] + V * q[1] + W * w
    n = len(outline)
    for i in range(n):
        a, b = outline[i], outline[(i + 1) % n]
        e = np.array(b) - np.array(a)
        out = unit(U * e[1] - V * e[0])
        mid = (np.array(a) + np.array(b)) / 2
        ctr = np.mean(np.array(outline), axis=0)
        if np.dot(out, U * (mid[0] - ctr[0]) + V * (mid[1] - ctr[1])) < 0 and _inside(outline, mid + 0.001 * np.array([np.dot(out, U), np.dot(out, V)])):
            out = -out
        p.quad(paint, P3(a, w0), P3(b, w0), P3(b, w1), P3(a, w1), out)
    for i0, i1, i2 in _earclip(outline):
        p.tri(paint, (P3(outline[i0], w1), P3(outline[i1], w1), P3(outline[i2], w1)), (unit(W),) * 3)
        p.tri(paint, (P3(outline[i0], w0), P3(outline[i1], w0), P3(outline[i2], w0)), (-unit(W),) * 3)


def _inside(poly, q):
    c = False
    for i in range(len(poly)):
        (x0, y0), (x1, y1) = poly[i], poly[i - 1]
        if (y0 > q[1]) != (y1 > q[1]) and q[0] < (x1 - x0) * (q[1] - y0) / (y1 - y0) + x0:
            c = not c
    return c


def chamfer_box(p, paint, c, s, ch, axis=2):
    """Box with its long edges along `axis` cut at 45 degrees by ch: an octagonal prism."""
    s = np.asarray(s, float) / 2
    u, v = [i for i in range(3) if i != axis]
    hu, hv = s[u], s[v]
    ch = min(ch, hu * 0.9, hv * 0.9)
    outline = [(-hu + ch, -hv), (hu - ch, -hv), (hu, -hv + ch), (hu, hv - ch), (hu - ch, hv), (-hu + ch, hv), (-hu, hv - ch), (-hu, -hv + ch)]
    E = np.eye(3)
    extrude(p, paint, outline, E[u], E[v], E[axis], -s[axis], s[axis], origin=c)


def tube(p, paint, pts, r, seg=8, closed=False):
    """A round bar along a polyline (handles, sight rings, loops)."""
    pts = [np.asarray(q, float) for q in pts]
    if closed:
        pts = pts + [pts[0]]
    rings = []
    for i, q in enumerate(pts):
        if closed and (i == 0 or i == len(pts) - 1):
            t = unit(pts[1] - pts[-2])
        else:
            t = unit(pts[min(i + 1, len(pts) - 1)] - pts[max(i - 1, 0)])
        a, u, v = basis(t)
        if rings:                                           # keep the ring from twisting
            u = unit(rings[-1][1] - np.dot(rings[-1][1], t) * t)
            v = np.cross(t, u)
        dirs = [u * math.cos(2 * math.pi * k / seg) + v * math.sin(2 * math.pi * k / seg) for k in range(seg + 1)]
        rings.append((q, u, dirs))
    for (q0, _, d0), (q1, _, d1) in zip(rings, rings[1:]):
        for k in range(seg):
            p.quad(paint, q0 + d0[k] * r, q0 + d0[k + 1] * r, q1 + d1[k + 1] * r, q1 + d1[k] * r, d0[k], d0[k + 1], d1[k + 1], d1[k])
    if not closed:
        for (q, _, d), sign in ((rings[0], -1), (rings[-1], 1)):
            t = unit(pts[-1] - pts[-2]) if sign > 0 else unit(pts[1] - pts[0])
            for k in range(seg):
                p.tri(paint, (q, q + d[k] * r, q + d[k + 1] * r), (t * sign,) * 3)


def ring(p, paint, c, axis, R, r, n=28, seg=6):
    a, u, v = basis(axis)
    c = np.asarray(c, float)
    tube(p, paint, [c + R * (u * math.cos(2 * math.pi * i / n) + v * math.sin(2 * math.pi * i / n)) for i in range(n)], r, seg, closed=True)


def game_mesh(p, prefab, node, src_frame, M=np.eye(4), keep=None, paint=None, game_mat=None):
    """Copies a game mesh into the part: its points in src_frame, then moved by M into the pivot frame. keep(points)
    picks triangles; the material is the mesh's own unless paint/game_mat says otherwise."""
    tris, mats = prefab.mesh(prefab.find(node) if node not in prefab.nodes else node, src_frame)
    R, t = M[:3, :3], M[:3, 3]
    count = 0
    for si, ps, ns, uv in tris:
        q = ps @ R.T + t
        if keep is not None and not keep(q):
            continue
        mat = paint or game_mat or mats[min(si, len(mats) - 1)]
        p.tris.append((mat, list(q), [unit(R @ n) for n in ns], list(uv)))
        count += 1
    return count


# ---- the stock bases ---------------------------------------------------------------------------------------------------
BASES = {
    # prefab, the barrel transforms (the barrel* part is authored for the first), stock parts the item keeps (prefixes)
    'lewis': dict(prefab='Lewis Gun', barrels=['weapon/Weapon model', 'weapon/Weapon model (1)'], keep=['Mount']),
    'm2': dict(prefab='Browning .50 Cal', barrels=['weapon/50cal twin'], keep=['Mount', 'Bullets']),
    'tub20': dict(prefab='20 mm Tub Oerlikon', barrels=['weapon/Oerlikon Recoil Piece'], keep=['Mount', 'Mount3_Holder', 'ArmRest']),
}


class Model:
    def __init__(self, name, base, keep=None):
        self.name, self.base = name, BASES[base]
        self.keep = keep if keep is not None else self.base['keep']
        self.prefab = gm.Prefab(self.base['prefab'])
        self.parts = []
        self.B = [np.linalg.inv(self.prefab.frame('weapon')) @ self.prefab.frame(b) for b in self.base['barrels']]

    def turret(self):
        p = Part('turret'); self.parts.append(p); return p

    def barrel(self):
        p = Part('barrel'); self.parts.append(p); return p

    def yaw(self):
        """Turns with the aim but does not elevate (OBJ objects yaw_*; the mod hangs them on the traverse at the pivot)."""
        p = Part('yaw'); self.parts.append(p); return p

    def to_file(self, part):
        return np.linalg.inv(self.B[0]) if part.name == 'barrel' else np.eye(4)

    def write(self, folder):
        folder = pathlib.Path(folder); folder.mkdir(parents=True, exist_ok=True)
        lines = ['# %s: built by tools/make_turret_models.py from the game\'s %s' % (self.name, self.base['prefab']),
                 'mtllib %s.mtl' % self.name]
        vl, tl, nl, body, mtl = [], [], [], [], {}
        vidx, tidx, nidx = {}, {}, {}
        ntri = 0
        for part in self.parts:
            F = self.to_file(part); R, t = F[:3, :3], F[:3, 3]
            groups = {}
            for tr in part.tris:
                key = tr[0].mtl if isinstance(tr[0], Paint) else 'game.' + tr[0].replace(' ', '_')
                if isinstance(tr[0], Paint):
                    mtl[key] = tr[0].kd
                else:
                    mtl.setdefault(key, (0.4, 0.4, 0.42))
                groups.setdefault(key, []).append(tr)
            for gi, (key, trs) in enumerate(groups.items()):
                body.append('o %s_%d' % (part.name, gi))
                body.append('usemtl %s' % key)
                for _, ps, ns, uvs in trs:
                    ids = []
                    for q, n, uv in zip(ps, ns, uvs):
                        q = tuple(round(float(x), 4) + 0.0 for x in R @ q + t)
                        n = tuple(round(float(x), 3) + 0.0 for x in R @ n)
                        uv = tuple(round(float(x), 4) + 0.0 for x in uv)
                        for val, idx, out, fmt in ((q, vidx, vl, 'v %.4f %.4f %.4f'), (uv, tidx, tl, 'vt %.4f %.4f'), (n, nidx, nl, 'vn %.3f %.3f %.3f')):
                            if val not in idx:
                                idx[val] = len(idx) + 1; out.append(fmt % val)
                        ids.append('%d/%d/%d' % (vidx[q], tidx[uv], nidx[n]))
                    body.append('f ' + ' '.join(ids)); ntri += 1
        (folder / ('%s.obj' % self.name)).write_text('\n'.join(lines + vl + tl + nl + body) + '\n')
        (folder / ('%s.mtl' % self.name)).write_text(''.join('newmtl %s\nKd %.3f %.3f %.3f\n\n' % ((k,) + tuple(v)) for k, v in mtl.items()))
        return ntri

    # -- previews --
    def kept(self, path):
        """Whether the item's model_keep= leaves this stock node visible (any path segment starts with a keep name)."""
        return any(seg.startswith(k) for seg in path.split('/') for k in self.keep)

    def scene(self, new=True):
        """Triangles in the gun's root frame, the gun aimed level: the stock gun (new=False), or the kept stock parts
        plus the new model."""
        P = self.prefab.frame('weapon')
        Pl = np.eye(4); Pl[:3, 3] = P[:3, 3]
        relevel = Pl @ np.linalg.inv(P)
        out = []
        for path in self.prefab.visible_nodes():
            if new and not self.kept(path):
                continue
            tris, mats = self.prefab.mesh(path, '')
            M = relevel if path.startswith('weapon/') else np.eye(4)
            for si, ps, ns, uv in tris:
                out.append((mats[min(si, len(mats) - 1)], ps @ M[:3, :3].T + M[:3, 3], ns @ M[:3, :3].T, uv))
        if new:
            for part in self.parts:
                frames = [Pl] if part.name != 'barrel' else [Pl @ B @ np.linalg.inv(self.B[0]) for B in self.B]
                for F in frames:
                    R, t = F[:3, :3], F[:3, 3]
                    for mat, ps, ns, uv in part.tris:
                        out.append((mat.game[0] if isinstance(mat, Paint) else mat, np.array(ps) @ R.T + t, np.array(ns) @ R.T, np.array(uv)))
        return out


def detail_sheet(models, path, closeup=False):
    """One row per model: the stock gun, then the new one from three sides. closeup=True leaves the mount out and
    zooms on the guns."""
    import preview3d as pv
    rows, labels = [], []
    for m in models:
        stock, new = m.scene(new=False), m.scene(new=True)
        if closeup:
            y0 = m.prefab.frame('weapon')[1, 3] - 0.3
            stock = [t for t in stock if t[1][:, 1].min() > y0]
            new = [t for t in new if t[1][:, 1].min() > y0]
        allp = np.concatenate([t[1] for t in stock + new])
        center = (allp.min(0) + allp.max(0)) / 2
        ext = np.abs(allp - center).max()
        sc = 420 * (0.47 if closeup else 0.42) / ext
        views = [(1.0, 0.55, 1.25), (1, 0.05, 0.0), (-1.0, 0.6, -0.9)]
        row = [pv.render(stock, views[0], scale=sc, center=center)]
        row += [pv.render(new, v, scale=sc, center=center) for v in views]
        rows.append(row)
        labels.append('%s   (left: the stock %s; then the new model, %d triangles)' % (m.name, m.base['prefab'], sum(len(p.tris) for p in m.parts)))
    pv.sheet(rows, labels).save(path)


# ---- the guns ----------------------------------------------------------------------------------------------------------
def vickers_k():
    """Twin Vickers K .303 on the Lewis ring mount: flat pan magazines, bare barrels with flash hiders, pistol grips."""
    m = Model('twin_vickers_k', 'lewis')
    b = m.barrel()                                         # gun 0 (x = 0) in the pivot frame; copied onto gun 1
    M, W = LEWIS_METAL, LEWIS_WOOD
    chamfer_box(b, M, (0, 0.0, -0.20), (0.058, 0.10, 0.44), 0.012)          # receiver
    box(b, M, (0, 0.054, -0.20), (0.046, 0.012, 0.40))                       # top cover
    lathe(b, M, (0, 0.0, -0.42), (0, 0, -1), [(0, 0.03), (0.03, 0.028), (0.045, 0.02), (0.05, 0.0)], seg=12)   # rear cap
    box(b, M, (0, -0.062, -0.12), (0.034, 0.03, 0.16))                       # trigger housing / mount lug
    # barrel, gas tube and flash hider
    lathe(b, M, (0, 0.0, 0.02), (0, 0, 1), [(0, 0.024), (0.05, 0.024), (0.05, 0.016), (0.06, 0.013), (0.47, 0.012),
                                            (0.47, 0.017), (0.49, 0.017), (0.49, 0.020), (0.56, 0.031), (0.565, 0.0)], seg=14)
    cyl(b, M, (0, -0.032, 0.02), (0, -0.032, 0.36), 0.009, seg=10)
    box(b, M, (0, -0.018, 0.365), (0.026, 0.05, 0.03))                       # gas block
    for z in (0.16, 0.28):
        box(b, M, (0, -0.016, z), (0.03, 0.012, 0.012))                      # barrel bands
    box(b, M, (0, 0.028, 0.45), (0.005, 0.024, 0.01))                        # front sight
    box(b, M, (0, 0.068, -0.39), (0.03, 0.02, 0.018))                        # rear sight block
    box(b, M, (0, 0.082, -0.39), (0.022, 0.012, 0.004))
    # the pan: a flat drum on a feed neck, ribbed top, centre hub, wind-up lever
    box(b, M, (0, 0.066, -0.22), (0.034, 0.02, 0.05))
    lathe(b, M, (0, 0.074, -0.22), (0, 1, 0), [(0, 0.0), (0, 0.088), (0.004, 0.093), (0.034, 0.093), (0.038, 0.088), (0.040, 0.03),
                                               (0.050, 0.026), (0.052, 0.0)], seg=28)
    for k in range(8):
        a = 2 * math.pi * k / 8
        box(b, M, (0.058 * math.sin(a), 0.115, -0.22 + 0.058 * math.cos(a)), (0.006, 0.006, 0.062), (0, math.degrees(a), 0))
    box(b, M, (0.06, 0.12, -0.22), (0.05, 0.006, 0.012), (0, 30, 0))
    # pistol grip and trigger guard
    extrude(b, W, [(-0.30, -0.075), (-0.27, -0.075), (-0.30, -0.19), (-0.345, -0.19), (-0.325, -0.075)], (0, 0, 1), (0, 1, 0), (1, 0, 0), -0.014, 0.014)
    tube(b, M, [(0, -0.075, -0.22), (0, -0.11, -0.225), (0, -0.115, -0.26), (0, -0.09, -0.285), (0, -0.075, -0.285)], 0.004, seg=6)
    box(b, M, (0.034, 0.02, -0.12), (0.016, 0.014, 0.03))                    # cocking handle
    t = m.turret()                                         # the twin cradle on the mount's arm
    box(t, M, (0.10, -0.085, -0.17), (0.30, 0.025, 0.06))
    for x in (0.0, 0.2):
        box(t, M, (x, -0.075, -0.17), (0.04, 0.03, 0.09))
    cyl(t, M, (0.10, -0.10, -0.17), (0.10, -0.16, -0.17), 0.022, seg=12)
    return m


def vickers_50():
    """Twin Vickers .50 Mk III on the M2 ring mount (belts kept): water-jacketed barrels with muzzle boosters,
    spade grips at the crew's hands."""
    m = Model('twin_vickers_50', 'm2')
    b = m.barrel()
    for x in (-0.30, 0.0):
        chamfer_box(b, METAL, (x, -0.005, -0.28), (0.10, 0.15, 0.56), 0.015)                 # receiver
        box(b, METAL, (x, 0.074, -0.30), (0.086, 0.012, 0.46))                                # top cover
        for z in (-0.47, -0.13):
            cyl(b, METAL, (x - 0.052, 0.074, z), (x + 0.052, 0.074, z), 0.008, seg=8)        # cover hinge pins
        box(b, METAL, (x, -0.005, -0.565), (0.11, 0.13, 0.02))                                # back plate
        # water jacket: fluted sleeve, front cap, filler plug and steam tube
        prof = [(0.0, 0.058), (0.035, 0.058), (0.035, 0.052)]
        z = 0.035
        while z < 0.55:
            prof += [(z + 0.005, 0.056), (z + 0.035, 0.056), (z + 0.04, 0.052)]
            z += 0.04
        prof += [(0.57, 0.054), (0.6, 0.054), (0.62, 0.046), (0.64, 0.032)]
        lathe(b, METAL, (x, 0.0, 0.0), (0, 0, 1), prof, seg=12)
        cyl(b, METAL, (x, 0.058, 0.1), (x, 0.075, 0.1), 0.014, seg=10)
        cyl(b, METAL, (x, -0.062, 0.05), (x, -0.062, 0.5), 0.008, seg=8)
        # muzzle booster and barrel tip
        lathe(b, METAL, (x, 0.0, 0.64), (0, 0, 1), [(0, 0.024), (0.02, 0.03), (0.13, 0.026), (0.13, 0.018), (0.17, 0.016), (0.17, 0.0)], seg=14)
        box(b, METAL, (x, 0.04, 0.6), (0.006, 0.03, 0.012))                                   # front sight
        # feed block on the outer side, where the belt comes up
        side = -1 if x < -0.1 else 1
        box(b, METAL, (x + side * 0.068, 0.02, -0.12), (0.04, 0.05, 0.1))
        box(b, METAL, (x + side * 0.068, 0.05, -0.12), (0.046, 0.012, 0.11))
    # spade grips where the crew's hands are, joined to the receivers
    for hx, gx in ((-0.436, -0.35), (0.148, 0.05)):
        cyl(b, METAL, (gx, -0.03, -0.40), (hx, -0.03, -0.40), 0.012, seg=8)
        lathe(b, LEATHER, (hx, -0.005, -0.382), (0, -1, 0), [(0, 0.016), (0.11, 0.016)], seg=10)
        cyl(b, METAL, (hx, -0.115, -0.382), (hx, -0.125, -0.382), 0.02, seg=10)
    box(b, METAL, (-0.15, -0.05, -0.47), (0.24, 0.02, 0.03))                                  # thumb trigger bar
    t = m.turret()
    box(t, METAL, (-0.15, -0.095, -0.22), (0.38, 0.03, 0.3))                                  # cradle
    box(t, METAL, (-0.15, -0.14, -0.22), (0.12, 0.06, 0.12))
    ring(t, METAL, (-0.15, 0.20, 0.12), (0, 0, 1), 0.07, 0.004)                                # ring sight
    ring(t, METAL, (-0.15, 0.20, 0.12), (0, 0, 1), 0.035, 0.003)
    for k in range(4):
        a = math.pi / 4 + k * math.pi / 2
        cyl(t, METAL, (-0.15, 0.20, 0.12), (-0.15 + 0.07 * math.cos(a), 0.20 + 0.07 * math.sin(a), 0.12), 0.0025, seg=4)
    cyl(t, METAL, (-0.15, 0.13, 0.12), (-0.15, 0.06, 0.12), 0.006, seg=6)
    return m


def type93():
    """Twin Type 93 13.2 mm (Japanese, Hotchkiss) on the M2 ring mount without belts: finned barrels, slotted flash
    hiders, curved 30-round magazines on top, a spider-web AA sight."""
    m = Model('twin_type93', 'm2', keep=['Mount'])
    b = m.barrel()
    for x in (-0.30, 0.0):
        chamfer_box(b, METAL, (x, -0.005, -0.27), (0.09, 0.12, 0.62), 0.012)                 # receiver
        box(b, METAL, (x, 0.06, -0.42), (0.07, 0.012, 0.26))
        lathe(b, METAL, (x, -0.005, -0.58), (0, 0, -1), [(0, 0.04), (0.04, 0.036), (0.06, 0.02), (0.065, 0.0)], seg=12)
        box(b, METAL, (x, -0.08, -0.22), (0.04, 0.04, 0.2))                                   # trigger group
        # barrel: radiator fins over the rear half, gas tube, flash hider
        prof = [(0.0, 0.03), (0.02, 0.03), (0.02, 0.016)]
        z = 0.03
        while z < 0.42:
            prof += [(z, 0.016), (z, 0.036), (z + 0.008, 0.036), (z + 0.008, 0.016)]
            z += 0.03
        prof += [(0.95, 0.014), (0.95, 0.02), (0.97, 0.02), (0.97, 0.024), (1.12, 0.034), (1.12, 0.0)]
        lathe(b, METAL, (x, 0.0, 0.03), (0, 0, 1), prof, seg=10)
        for k in range(6):                                                                     # flash hider slots
            a = 2 * math.pi * k / 6
            box(b, METAL, (x + 0.03 * math.cos(a), 0.03 * math.sin(a), 1.09), (0.006, 0.006, 0.07), (0, 0, math.degrees(a)))
        cyl(b, METAL, (x, -0.04, 0.03), (x, -0.04, 0.62), 0.011, seg=10)
        box(b, METAL, (x, -0.024, 0.62), (0.03, 0.05, 0.035))
        # 30-round box: a curved magazine standing on top
        R, w = 0.5, 0.045
        cl = [(-0.17 + R * (1 - math.cos(t)), 0.055 + R * math.sin(t)) for t in np.linspace(0, 0.46, 9)]
        outline = [(z0 - w * math.cos(t), y0 + w * math.sin(t)) for (z0, y0), t in zip(cl, np.linspace(0, 0.46, 9))]
        outline += [(z0 + w * math.cos(t), y0 - w * math.sin(t)) for (z0, y0), t in reversed(list(zip(cl, np.linspace(0, 0.46, 9))))]
        extrude(b, METAL, outline, (0, 0, 1), (0, 1, 0), (1, 0, 0), x - 0.016, x + 0.016)
        extrude(b, METAL, [(-0.215, 0.055), (-0.125, 0.055), (-0.125, 0.075), (-0.215, 0.075)], (0, 0, 1), (0, 1, 0), (1, 0, 0), x - 0.022, x + 0.022)
        zt, yt = cl[-1]
        box(b, METAL, (x, yt + 0.005, zt), (0.036, 0.012, 0.1), (math.degrees(0.46), 0, 0))
        for k in (2, 4, 6):                                                                     # pressed ribs
            zc, yc = cl[k]
            box(b, METAL, (x - 0.017, yc, zc), (0.004, 0.012, 0.08), (math.degrees(np.linspace(0, 0.46, 9)[k]), 0, 0))
            box(b, METAL, (x + 0.017, yc, zc), (0.004, 0.012, 0.08), (math.degrees(np.linspace(0, 0.46, 9)[k]), 0, 0))
    for hx, gx in ((-0.436, -0.345), (0.148, 0.045)):                                         # grips at the hands
        cyl(b, METAL, (gx, -0.04, -0.40), (hx, -0.04, -0.40), 0.012, seg=8)
        lathe(b, LEATHER, (hx, -0.01, -0.382), (0, -1, 0), [(0, 0.017), (0.11, 0.017)], seg=10)
    t = m.turret()
    box(t, METAL, (-0.15, -0.10, -0.2), (0.40, 0.03, 0.34))                                   # cradle
    box(t, METAL, (-0.15, -0.15, -0.2), (0.12, 0.07, 0.12))
    c = np.array([-0.15, 0.25, 0.25])                                                          # spider-web sight
    for R_ in (0.09, 0.06, 0.03):
        ring(t, METAL, c, (0, 0, 1), R_, 0.0035)
    for k in range(8):
        a = k * math.pi / 4
        cyl(t, METAL, c, c + 0.09 * np.array([math.cos(a), math.sin(a), 0]), 0.002, seg=4)
    cyl(t, METAL, (c[0], c[1] - 0.09, c[2]), (c[0], -0.07, c[2]), 0.007, seg=6)
    box(t, METAL, (-0.15, 0.10, -0.45), (0.02, 0.05, 0.02))                                   # rear bead
    return m


def flak38():
    """2 cm Flak 38 on the Oerlikon pedestal (cradle and shoulder rests kept): a boxy receiver, a side-fed 20-round
    magazine, a long barrel with the cone flash hider and a reflector sight."""
    m = Model('flak38', 'tub20')
    t = m.turret()
    chamfer_box(t, METAL, (0, 0.015, -0.03), (0.13, 0.18, 0.88), 0.022)                    # receiver
    extrude(t, METAL, [(0.41, -0.075), (0.53, -0.06), (0.53, 0.04), (0.46, 0.105), (0.41, 0.105)], (0, 0, 1), (0, 1, 0), (1, 0, 0), -0.06, 0.06)
    for sx in (-1, 1):                                                                       # side plates and rivets
        box(t, METAL, (sx * 0.067, 0.02, -0.05), (0.006, 0.11, 0.62))
        for z in (-0.33, -0.17, 0.0, 0.17):
            for y in (-0.02, 0.06):
                cyl(t, METAL, (sx * 0.07, y, z), (sx * 0.076, y, z), 0.007, seg=6)
    box(t, METAL, (0, 0.11, -0.08), (0.09, 0.014, 0.62))                                    # bolt cover with ribs
    for z in (-0.34, -0.2, -0.06, 0.08, 0.2):
        box(t, METAL, (0, 0.12, z), (0.094, 0.008, 0.025))
    lathe(t, METAL, (0, 0.01, -0.47), (0, 0, -1), [(0, 0.055), (0.03, 0.055), (0.03, 0.05), (0.06, 0.05), (0.09, 0.036), (0.1, 0.0)], seg=16)
    box(t, METAL, (0, -0.105, -0.21), (0.055, 0.07, 0.22))                                  # trigger housing
    box(t, METAL, (0, -0.15, -0.28), (0.03, 0.04, 0.05))
    extrude(t, METAL, [(-0.02, -0.07), (0.1, -0.07), (0.14, -0.2), (0.07, -0.2)], (0, 0, 1), (0, 1, 0), (1, 0, 0), 0.0, 0.05)   # ejection chute
    cyl(t, METAL, (0.065, 0.045, -0.06), (0.15, 0.045, -0.06), 0.011, seg=8)               # cocking lever
    lathe(t, METAL, (0.15, 0.045, -0.06), (1, 0, 0), [(0, 0.02), (0.035, 0.02), (0.045, 0.0)], seg=10)
    # the 20-round box, pushed in from the left through a feed housing
    box(t, METAL, (-0.08, 0.035, 0.02), (0.04, 0.14, 0.19))
    chamfer_box(t, METAL, (-0.21, 0.035, 0.02), (0.24, 0.12, 0.16), 0.012, axis=0)
    for xx in (-0.14, -0.19, -0.24, -0.29):
        box(t, METAL, (xx, 0.035, 0.02), (0.01, 0.126, 0.166))
    box(t, METAL, (-0.335, 0.035, 0.02), (0.014, 0.13, 0.17))
    box(t, METAL, (-0.08, 0.12, 0.02), (0.03, 0.03, 0.06))                                  # magazine catch
    # ring-and-bead AA sight on an arm to the left: front rings ahead, eyepiece at the back
    box(t, METAL, (-0.14, 0.15, -0.05), (0.16, 0.02, 0.03))
    c = np.array([-0.21, 0.24, 0.18])
    for R_ in (0.075, 0.045):
        ring(t, METAL, c, (0, 0, 1), R_, 0.004)
    for k in range(4):
        a = k * math.pi / 2
        cyl(t, METAL, c, c + 0.075 * np.array([math.cos(a), math.sin(a), 0]), 0.0025, seg=4)
    cyl(t, METAL, (-0.21, 0.165, -0.05), (-0.21, 0.165, 0.18), 0.008, seg=6)
    box(t, METAL, (-0.21, 0.2, -0.06), (0.03, 0.07, 0.03))
    cyl(t, METAL, (-0.21, 0.24, -0.04), (-0.21, 0.24, -0.1), 0.016, seg=10)
    b = m.barrel()
    lathe(b, METAL, (0, 0.0, 0.44), (0, 0, 1), [(0, 0.055), (0.12, 0.055), (0.12, 0.042), (0.2, 0.042), (0.2, 0.028), (0.3, 0.025),
                                              (1.36, 0.02), (1.36, 0.024), (1.39, 0.024), (1.39, 0.026), (1.62, 0.048), (1.62, 0.04),
                                              (1.6, 0.0)], seg=18)
    for k in range(8):                                                                       # nut flats on the collar
        a = 2 * math.pi * k / 8
        box(b, METAL, (0.055 * math.cos(a), 0.055 * math.sin(a), 0.5), (0.012, 0.012, 0.11), (0, 0, math.degrees(a)))
    tube(b, METAL, [(0, 0.03, 0.68), (0, 0.095, 0.71), (0, 0.095, 0.86), (0, 0.03, 0.89)], 0.01, seg=8)  # barrel handle
    box(b, METAL, (0, 0.035, 1.74), (0.006, 0.03, 0.012))                                     # front sight
    return m


def type96():
    """Type 96 25 mm single (Japanese): the game's own gun, moved onto the Oerlikon pedestal's aim point; the barrel
    recoils."""
    m = Model('type96_25mm', 'tub20')
    src = gm.Prefab('Type96 Single')
    node = 'weapon/Weapon Model/Model_LOD0'
    tris, _ = src.mesh(node, 'weapon')
    P = np.concatenate([q[1] for q in tris])
    muzzle = P[P[:, 2] > P[:, 2].max() - 0.15]
    bore = np.array([muzzle[:, 0].mean(), muzzle[:, 1].mean()])
    M = mat4((-bore[0], -bore[1], 0.05))
    zcut = 0.62                                            # in front of the receiver: barrel
    is_barrel = lambda q: bool(q[:, 2].min() > zcut and (np.hypot(q[:, 0], q[:, 1]) < 0.07).all())
    nb = game_mesh(m.barrel(), src, node, 'weapon', M, keep=is_barrel)
    nt = game_mesh(m.turret(), src, node, 'weapon', M, keep=lambda q: not is_barrel(q))
    m.note = 'game mesh %s: %d barrel + %d gun triangles, bore moved by (%.3f, %.3f)' % (node, nb, nt, -bore[0], -bore[1])
    return m


def barge_buster():
    """37 mm M4 'Barge Buster': the game's M4 aircraft cannon with its ring magazine of rounds, moved onto the Oerlikon
    pedestal; the barrel recoils."""
    m = Model('m4_barge_buster', 'tub20', keep=['Mount', 'Mount3_Holder'])
    src = gm.Prefab('37 mm Autocannon')
    bt, _ = src.mesh('weapon/Barrel/Barrel_LOD0', 'weapon')
    P = np.concatenate([q[1] for q in bt])
    muzzle = P[P[:, 2] > P[:, 2].max() - 0.1]
    bore = np.array([(muzzle[:, 0].min() + muzzle[:, 0].max()) / 2, (muzzle[:, 1].min() + muzzle[:, 1].max()) / 2])
    M = mat4((-bore[0], -bore[1] + 0.02, 0.10))
    game_mesh(m.barrel(), src, 'weapon/Barrel/Barrel_LOD0', 'weapon', M)
    t = m.turret()
    game_mesh(t, src, 'weapon/Receiver_LOD0', 'weapon', M)
    rounds = [q for q in src.nodes if q.startswith('weapon/Visible Rounds/') and q.count('/') == 2]
    for q in rounds:
        game_mesh(t, src, q, 'weapon', M)
    m.note = 'game meshes: M4 receiver, barrel and %d rounds' % len(rounds)
    for part in m.parts:                                   # its own material, or the dark gun metal on boats without one
        part.tris = [(M4 if mat == 'M4 37 MM Autocannon' else mat, ps, ns, uv) for mat, ps, ns, uv in part.tris]
    return m


MODELS = [vickers_k, vickers_50, type93, flak38, type96, barge_buster]


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
        print('%-18s %5d triangles  keep=%s  %s' % (m.name, n, ','.join(m.keep), getattr(m, 'note', '')))
        built.append(m)
    if prev:
        prev.mkdir(parents=True, exist_ok=True)
        for i in range(0, len(built), 3):
            detail_sheet(built[i:i + 3], prev / ('turret_models_%d.png' % (i // 3 + 1)))
            detail_sheet(built[i:i + 3], prev / ('turret_closeups_%d.png' % (i // 3 + 1)), closeup=True)


if __name__ == '__main__':
    main()
