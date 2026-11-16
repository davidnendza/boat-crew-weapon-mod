"""Builds the female crew hair meshes (models/female_hair_<style>.bchair) for the weapon mod, plus a preview PNG.

Four 1940s styles, each a shell fitted to the crew head with a soft rounded hairline (no widow's peak):
  bob        chin-length rounded bob, side part, volume at the sides, ears partly covered
  bun        pulled back to a low bun at the nape
  ponytail   pulled back, a ponytail kept close to the neck down to the top of the shoulder blades
  rolls      victory rolls: two rolls on the top/front sides, the back gathered into a roll at the nape

How a style is made (all in the BaseCharacter frame: metres, x lateral, y up, z forward):
  - the scalp is ray-cast from a point inside the head against the crew FULL_BODY mesh (v2 female deform), so the shell
    follows the real low-poly head; the shell sits 4-10 mm out (thin at the crown so hats still fit) and its edge is
    closed with a lip and a short inner sheet, so the hair has visible thickness like the stock hair
  - extra volume (bun, tail, rolls) are tubes and ellipsoids; every vertex is pushed out until it clears the body,
    the female Shirt_Sailor, the eyes and the eyebrows
  - UVs sit in the white hair blob of the 'Crew Full Baked' atlas (u 0.911-0.920, v 0.009-0.017): the patch named in
    the notes (u 0.91-0.93, v 0.01-0.09) is two blobs with a non-hair band at v 0.03-0.07 between them, so only the
    lower blob is used. The stock hair is tinted by _Color1 there.

File format (little-endian), read by the mod: char[4] 'BCH1'; uint32 nv; uint32 ntri; float32 pos[nv*3];
float32 nrm[nv*3]; float32 uv[nv*2]; int32 tri[ntri*3]. pos/nrm are in the crew MESH space (the same space as the
stock Hair_1 mesh: P_bc = node_matrix * pos); every vertex is weighted 100% to bone 3 (Head), so no weights are stored.
Triangle winding matches the stock hair (cross(b-a, c-a) along the vertex normal in mesh space).

Inputs: the crew meshes exported from the game as .npz (export_crew_mesh.py in the female-crew scratch folder) and the
v2-deformed body (deform_female.py writes <export>/../female/*.npz).

Usage: python make_female_hair_assets.py <export folder> <models folder> [--preview <png>]
"""
import math
import pathlib
import struct
import sys

import numpy as np

STYLES = ('bob', 'bun', 'ponytail', 'rolls')
NA = 32                                    # shell columns around the head (11.25 degrees)
U0, U1, V0, V1 = 0.911, 0.920, 0.009, 0.017
HAIR_COL = '#4a3624'
CAP_CROWN_Y, HELMET_CROWN_Y = 1.725, 1.715   # above these the hats' shells are crown, below band/brim
CLEAR_MIN = 0.004                          # shell / extras distance to the scalp and body (m)


# ---------------------------------------------------------------- mesh loading and geometry helpers

def load(exp, name):
    d = np.load(exp / (name + '.npz'))
    M = d['node_matrix']
    P = (M[:3, :3] @ d['pos'].T).T + M[:3, 3]
    N = (M[:3, :3] / 100.0 @ d['nrm'].T).T
    return {'P': P, 'N': N, 'T': d['tris'].astype(np.int64), 'uv': d['uv'], 'M': M, 'd': d}


def unit(v):
    v = np.asarray(v, float)
    return v / np.maximum(np.linalg.norm(v, axis=-1, keepdims=True), 1e-12)


def smooth(x):
    x = np.clip(x, 0.0, 1.0)
    return x * x * (3 - 2 * x)


def raycast(o, dirs, tri):
    """Nearest positive hit distance of rays o + t*dirs against triangles tri (m,3,3); inf when none."""
    a, b, c = tri[:, 0], tri[:, 1], tri[:, 2]
    e1, e2 = b - a, c - a
    out = np.full(len(dirs), np.inf)
    for k in range(0, len(dirs), 256):
        D = dirs[k:k + 256][:, None, :]
        p = np.cross(D, e2[None])
        det = (e1[None] * p).sum(-1)
        ok = np.abs(det) > 1e-12
        inv = np.where(ok, 1.0 / np.where(ok, det, 1), 0)
        s = o - a
        u = (s[None] * p).sum(-1) * inv
        q = np.cross(s, e1)
        v = (D * q[None]).sum(-1) * inv
        t = (e2 * q).sum(-1)[None] * inv
        hit = ok & (u >= -1e-5) & (v >= -1e-5) & (u + v <= 1 + 1e-5) & (t > 1e-6)
        out[k:k + 256] = np.where(hit, t, np.inf).min(1)
    return out


def closest(points, tri):
    """Closest point on the triangle set for each point: (distance, signed distance by the face normal)."""
    a, b, c = tri[:, 0], tri[:, 1], tri[:, 2]
    fn = unit(np.cross(b - a, c - a))
    dist = np.empty(len(points)); sgn = np.empty(len(points))
    for k in range(0, len(points), 64):
        p = points[k:k + 64][:, None, :]
        ab, ac = b - a, c - a
        ap = p - a
        d1, d2 = (ab * ap).sum(-1), (ac * ap).sum(-1)
        bp = p - b
        d3, d4 = (ab * bp).sum(-1), (ac * bp).sum(-1)
        cp = p - c
        d5, d6 = (ab * cp).sum(-1), (ac * cp).sum(-1)
        va = d3 * d6 - d5 * d4; vb = d5 * d2 - d1 * d6; vc = d1 * d4 - d3 * d2
        den = np.where(np.abs(va + vb + vc) < 1e-18, 1e-18, va + vb + vc)
        v = vb / den; w = vc / den
        q = a + ab * v[..., None] + ac * w[..., None]
        # region tests (Ericson, Real-Time Collision Detection 5.1.5)
        A = np.broadcast_to(a, q.shape); B = np.broadcast_to(b, q.shape); C = np.broadcast_to(c, q.shape)
        r = (d1 <= 0) & (d2 <= 0); q = np.where(r[..., None], A, q)
        r2 = (d3 >= 0) & (d4 <= d3) & ~r; q = np.where(r2[..., None], B, q); r |= r2
        r2 = (d6 >= 0) & (d5 <= d6) & ~r; q = np.where(r2[..., None], C, q); r |= r2
        vv = d1 / np.where(d1 - d3 == 0, 1e-18, d1 - d3)
        r2 = (vc <= 0) & (d1 >= 0) & (d3 <= 0) & ~r; q = np.where(r2[..., None], a + ab * vv[..., None], q); r |= r2
        ww = d2 / np.where(d2 - d6 == 0, 1e-18, d2 - d6)
        r2 = (vb <= 0) & (d2 >= 0) & (d6 <= 0) & ~r; q = np.where(r2[..., None], a + ac * ww[..., None], q); r |= r2
        w3 = (d4 - d3) / np.where((d4 - d3) + (d5 - d6) == 0, 1e-18, (d4 - d3) + (d5 - d6))
        r2 = (va <= 0) & (d4 - d3 >= 0) & (d5 - d6 >= 0) & ~r
        q = np.where(r2[..., None], b + (c - b) * w3[..., None], q)
        dd = np.linalg.norm(p - q, axis=-1)
        j = dd.argmin(1)
        i = np.arange(len(j))
        dist[k:k + 64] = dd[i, j]
        sgn[k:k + 64] = np.sign(((p[:, 0] - q[i, j]) * fn[j]).sum(-1)) * dd[i, j]
    return dist, sgn


class Mesh:
    """A growing triangle list of patches; normals are smoothed inside a patch only (patches keep crisp edges)."""

    def __init__(self):
        self.V, self.N, self.T = [], [], []

    def add(self, V, T, flip_ref=None, normals=None):
        V = np.asarray(V, float); T = np.asarray(T, np.int64).reshape(-1, 3)
        T = T[(T[:, 0] != T[:, 1]) & (T[:, 1] != T[:, 2]) & (T[:, 0] != T[:, 2])]
        if flip_ref is not None:                       # per-triangle outward reference vectors (or a callable)
            fc = V[T].mean(1)
            ref = flip_ref(fc) if callable(flip_ref) else flip_ref
            fn = np.cross(V[T[:, 1]] - V[T[:, 0]], V[T[:, 2]] - V[T[:, 0]])
            bad = (fn * ref).sum(1) < 0
            T[bad] = T[bad][:, [0, 2, 1]]
        if normals is None:
            fn = np.cross(V[T[:, 1]] - V[T[:, 0]], V[T[:, 2]] - V[T[:, 0]])
            acc = np.zeros_like(V)
            for k in range(3):
                np.add.at(acc, T[:, k], fn)
            normals = unit(acc)
        base = sum(len(v) for v in self.V)
        self.V.append(V); self.N.append(normals); self.T.append(T + base)

    def arrays(self):
        V, N, T = np.concatenate(self.V), np.concatenate(self.N), np.concatenate(self.T)
        used = np.zeros(len(V), bool); used[T.ravel()] = True             # drop vertices no triangle uses
        remap = np.cumsum(used) - 1
        return V[used], N[used], remap[T]


def grid_tris(rows, cols, wrap=True, idx=None):
    """Quads of a rows x cols vertex grid (row-major), columns wrapping around when wrap."""
    idx = np.arange(rows * cols).reshape(rows, cols) if idx is None else idx
    T = []
    cmax = cols if wrap else cols - 1
    for i in range(rows - 1):
        for j in range(cmax):
            j2 = (j + 1) % cols
            a, b, c, d = idx[i, j], idx[i, j2], idx[i + 1, j2], idx[i + 1, j]
            T += [(a, b, c), (a, c, d)]
    return T


def tube(C, radii, sides=8, squash=(1.0, 1.0), up=(0, 1, 0), tip_end=True, tip_start=False):
    """Vertices/triangles of a tube along centreline C with per-point radii; the ends close to a point (or a flat cap)."""
    C = np.asarray(C, float); n = len(C)
    Tg = unit(np.gradient(C, axis=0))
    b0 = unit(np.cross(Tg[0], up)) if np.linalg.norm(np.cross(Tg[0], up)) > 1e-6 else unit(np.cross(Tg[0], (1, 0, 0)))
    V = []
    nrm = b0
    for k in range(n):
        nrm = unit(nrm - Tg[k] * (nrm @ Tg[k]))
        bin_ = np.cross(Tg[k], nrm)
        for s in range(sides):
            a = 2 * math.pi * s / sides
            V.append(C[k] + radii[k] * (math.cos(a) * squash[0] * nrm + math.sin(a) * squash[1] * bin_))
    T = grid_tris(n, sides, wrap=True)
    V = list(V)
    for end, tip in ((0, tip_start), (n - 1, tip_end)):
        ci = len(V)
        V.append(C[end] + (Tg[end] * radii[end] * 0.6 * (1 if end else -1) if tip else 0))
        for s in range(sides):
            T.append((ci, end * sides + s, end * sides + (s + 1) % sides))
    V = np.array(V)
    def ref(fc):                                                 # outward: away from the nearest centreline point
        k = np.linalg.norm(fc[:, None] - C[None], axis=-1).argmin(1)
        return fc - C[k]
    return V, T, ref


def ellipsoid(center, radii, axes=np.eye(3), rings=6, segs=10):
    V = []
    for i in range(rings + 1):
        th = math.pi * i / rings
        for j in range(segs):
            ph = 2 * math.pi * j / segs
            q = np.array([math.sin(th) * math.cos(ph), math.cos(th), math.sin(th) * math.sin(ph)]) * radii
            V.append(center + axes @ q)
    V = np.array(V)
    T = grid_tris(rings + 1, segs)
    return V, T, (lambda fc: fc - center)


# ---------------------------------------------------------------- the head

class Head:
    def __init__(self, exp):
        fem = exp.parent / 'female'
        self.body = load(fem if (fem / 'FULL_BODY.npz').exists() else exp, 'FULL_BODY')
        self.shirt = load(fem if (fem / 'Shirt_Sailor.npz').exists() else exp, 'Shirt_Sailor')
        self.eyes = load(exp, 'Eyes'); self.brows = load(exp, 'Eyebrows_1'); self.mouth = load(exp, 'Mouth_1')
        self.hairref = load(exp, 'Hair_1')
        self.cap = load(exp, 'H_Sailor_Cap'); self.helmet = load(exp, 'H_Helmet')
        self.talker = load(exp, 'H_HelmetTalker') if (exp / 'H_HelmetTalker.npz').exists() else None   # keeps the hair too
        P = self.body['P']
        self.btri = P[self.body['T']]
        hp = P[(P[:, 1] > 1.60)]
        self.c = np.array([0.0, 1.645, (hp[:, 2].max() + hp[:, 2].min()) / 2 - 0.005])
        near = lambda m: m['P'][m['T']][(m['P'][m['T']][:, :, 1].max(1) > 1.30)]
        self.solid = near(self.body)                              # closed: signed distance works
        self.open = np.concatenate([near(self.shirt), near(self.eyes), near(self.brows)])   # open shells: unsigned
        self.obst = np.concatenate([self.solid, self.open])
        self.capinner = {k: v['P'][v['T']] for k, v in (('cap', self.cap), ('helmet', self.helmet), ('talker', self.talker))
                         if v is not None}

    def dir(self, phi, th):
        p, t = np.radians(phi), np.radians(th)
        return np.stack([np.cos(t) * np.sin(p), np.sin(t) + 0 * p, np.cos(t) * np.cos(p)], -1)

    def scalp(self, dirs):
        r = raycast(self.c, dirs, self.btri)
        for k in range(4):                                   # a ray through a crack between triangles: nudge it
            bad = ~np.isfinite(r)
            if not bad.any():
                break
            jit = unit(dirs[bad] + 0.004 * np.array([[1, 0, 0], [0, 0, 1], [-1, 0, 0], [0, 0, -1]])[k])
            r[bad] = raycast(self.c, jit, self.btri)
        return r

    def clearance(self, V):
        """Signed clearance: distance outside the (closed) body, or the plain distance to the shirt/eyes/brows."""
        ds, s = closest(V, self.solid)
        s = np.where(s < -0.015, ds, s)        # thin flaps (ears): a deep 'inside' is the far side of the flap
        d, _ = closest(V, self.open)
        return np.minimum(s, d)

    def push_out(self, V, clear=CLEAR_MIN, from_pt=None, iters=8):
        """Moves points away from from_pt (head centre by default) until they clear every obstacle by `clear`."""
        V = V.copy()
        o = self.c if from_pt is None else from_pt
        for _ in range(iters):
            s = self.clearance(V)
            bad = s < clear
            if not bad.any():
                break
            step = np.minimum(clear - s[bad], 0.012) + 0.0005
            V[bad] += unit(V[bad] - o) * step[:, None]
        return V

    def scalp_clear(self, V):
        ds, s = closest(V, self.btri[self.btri[:, :, 1].min(1) > 1.50])
        return np.where(s < -0.015, ds, s)


# ---------------------------------------------------------------- shells

def front_hairline(phi, part=0.0, low=0.0):
    """Elevation (deg) of the front hairline: a soft arc, highest in the middle, coming down over the temples.

    part shifts the arc's crest sideways (side part), low lowers one side (deg at phi<0 side)."""
    a = np.abs(phi - part)
    th = 27.0 - 21.0 * smooth(a / 62.0)                      # 27 deg mid-forehead, 6 deg at the temples
    th -= low * smooth(-phi / 40.0) * (1 - smooth((np.abs(phi) - 50) / 30))
    return th


def back_hairline(phi):
    """Hairline of pulled-back styles behind the temples: up around the ear, down to the nape."""
    a = np.abs(phi)
    th = np.interp(a, [0, 60, 72, 86, 100, 118, 140, 180], [0, 5, 8, 12, 9, -12, -30, -36])
    return th


class Shell:
    """A head shell: NA columns, K rows from the crown down to each column's edge."""

    def __init__(self, head, K):
        self.h, self.K = head, K
        self.phi = -180.0 + 360.0 * np.arange(NA) / NA

    def build(self, mesh, edge_th, offset_fn, drape=None, rim_rows=2, rim_thick=0.007, extra_fn=None):
        h, K = self.h, self.K
        cols = []
        for j, ph in enumerate(self.phi):
            th = np.linspace(90.0, edge_th[j], 60)
            dirs = h.dir(np.full_like(th, ph), th)
            r = h.scalp(dirs)
            ok = np.isfinite(r)                                  # a ray through a crack: fill from its neighbours
            r = np.interp(np.arange(len(r)), np.flatnonzero(ok), r[ok])
            off = offset_fn(ph, th)
            if extra_fn is not None:
                off = off + extra_fn(ph, th)
            pts = h.c + dirs * (r + off)[:, None]
            if drape is not None and drape[j] is not None:
                pts = np.concatenate([pts, drape[j](pts[-1])])
            seg = np.linalg.norm(np.diff(pts, axis=0), axis=1)
            s = np.concatenate([[0], np.cumsum(seg)]); s /= s[-1]
            tt = np.linspace(0, 1, K) ** 0.9
            cols.append(np.stack([np.interp(tt, s, pts[:, k]) for k in range(3)], -1))
        G = np.stack(cols, 1)                                    # K x NA x 3, row 0 = crown apex
        G[0] = G[0].mean(0)
        flat = G.reshape(-1, 3)
        flat = h.push_out(flat)
        G = flat.reshape(K, NA, 3)
        self.G = G
        # outer sheet (apex shared)
        V = np.concatenate([G[0:1, 0], G[1:].reshape(-1, 3)])
        idx = np.concatenate([np.zeros((1, NA), int), 1 + np.arange((K - 1) * NA).reshape(K - 1, NA)])
        T = grid_tris(K, NA, idx=idx)
        mesh.add(V, T, flip_ref=lambda fc: fc - h.c + np.array([0, 0.02, 0]))
        # rim: the edge folds in by rim_thick, then an inner sheet runs back up rim_rows rows
        E = G[-1]
        Nout = unit(G[-1] - G[-2])                               # along the meridian, past the edge
        rad = unit(np.cross(np.cross(Nout, G[-1] - h.c), Nout))  # outward, across the meridian
        rad = np.where((rad * (G[-1] - h.c)).sum(1)[:, None] < 0, -rad, rad)
        inner = [G[-1 - k] - rad * rim_thick for k in range(rim_rows + 1)]
        inner = [h.push_out(x, clear=0.0035) for x in inner]
        lip = np.stack([E, inner[0]])
        mesh.add(lip.reshape(-1, 3), grid_tris(2, NA),
                 flip_ref=lambda fc, E=E, Nout=Nout: Nout[np.linalg.norm(fc[:, None] - E[None], axis=-1).argmin(1)])
        IS = np.stack(inner)
        mesh.add(IS.reshape(-1, 3), grid_tris(len(inner), NA),
                 flip_ref=lambda fc, E=E, rad=rad: -rad[np.linalg.norm(fc[:, None] - E[None], axis=-1).argmin(1)])
        return G


def offset_default(crown=0.0045, edge=0.009):
    def f(ph, th):
        return crown + (edge - crown) * (1 - smooth((th - 10) / 60.0))
    return f


# ---------------------------------------------------------------- styles

def style_bob(head):
    m = Mesh(); sh = Shell(head, 13)
    PART = 22.5                                                  # a column sits exactly on the part
    edge, drape = [], []
    for ph in sh.phi:
        a = abs(ph)
        w = smooth((a - 50.0) / 30.0)                            # 0 at the fringe, 1 from the ear back
        fr = front_hairline(np.array(ph), part=PART, low=12.0)
        th_end = (1 - w) * fr + w * (-8.0)
        edge.append(float(th_end))
        if w < 0.02:
            drape.append(None); continue
        y_end = 1.535 + 0.012 * smooth((a - 120) / 60.0)         # chin length, a touch shorter at the back
        def dr(p0, w=w, y_end=y_end, ph=ph):
            y_end = p0[1] - w * (p0[1] - y_end)
            n = 8
            out = []
            hor = unit(np.array([p0[0] - head.c[0], 0, p0[2] - head.c[2]]))
            for k in range(1, n + 1):
                f = k / n
                y = p0[1] + (y_end - p0[1]) * f
                bulge = 0.010 * math.sin(math.pi * min(1, f * 1.3)) * w      # the bell of the bob
                tuck = -0.010 * smooth((f - 0.75) / 0.25) * w                # ends curl under
                out.append(p0 + hor * (bulge + tuck + 0.004 * f) + np.array([0, y - p0[1], 0]))
            return np.array(out)
        drape.append(dr)

    def extra(ph, th):
        side = smooth((abs(ph) - 30) / 40.0) * (1 - smooth((abs(ph) - 150) / 30.0))
        vol = 0.007 * side * np.exp(-0.5 * ((th - 5) / 22.0) ** 2)
        big = 0.004 * smooth(-np.sin(np.radians(ph)) / 0.6) * smooth((th + 10) / 30)  # the fuller, swept side
        groove = -0.003 * np.exp(-0.5 * ((ph - PART) / 4.0) ** 2) * smooth((th - 35) / 15)
        return vol + big + groove
    sh.build(m, np.array(edge), offset_default(0.0045, 0.010), drape=drape, rim_rows=3, rim_thick=0.008, extra_fn=extra)
    return m


def pulled_back(head, m, K=10, back_low=None):
    sh = Shell(head, K)
    edge = []
    for ph in sh.phi:
        a = abs(ph)
        fr = front_hairline(np.array(ph))
        bk = back_hairline(np.array(ph))
        w = smooth((a - 55.0) / 15.0)
        e = (1 - w) * fr + w * bk
        if back_low is not None:
            e = min(e, back_low(ph))
        edge.append(float(e))

    def extra(ph, th):                                           # a little lift at the front, smooth sweep back
        return 0.002 * np.exp(-0.5 * (ph / 40.0) ** 2) * smooth((th - 20) / 20)
    return sh.build(m, np.array(edge), offset_default(0.0045, 0.008), rim_rows=2, rim_thick=0.006, extra_fn=extra)


def style_bun(head):
    m = Mesh()
    pulled_back(head, m)
    # bun: a slightly flattened ball at the nape, low and close to the head
    d = head.dir(np.array(180.0), np.array(-24.0))
    r = head.scalp(d[None])[0]
    rad = np.array([0.040, 0.032, 0.026])                        # x half-width, y, depth
    cen = head.c + d * (r + 0.006 + rad[2] * 0.75)
    cen = cen + np.array([0, 0.004, 0])
    ax = np.array([[1, 0, 0], [0, 1, 0], [0, 0, 1]], float)
    V, T, ref = ellipsoid(cen, np.array([rad[0], rad[1], rad[2]]), ax, rings=6, segs=12)
    # rotate the ellipsoid so its depth axis points away from the head
    V = head.push_out(V, from_pt=cen - unit(d) * 0.05)
    m.add(V, T, flip_ref=lambda fc: fc - cen)
    return m


def style_ponytail(head):
    m = Mesh()
    pulled_back(head, m)
    # gathered at the back of the head, above the nape; the tail hangs close to the neck
    d0 = head.dir(np.array(180.0), np.array(-14.0))
    r0 = head.scalp(d0[None])[0]
    start = head.c + d0 * (r0 + 0.014)
    # the back surface (body + female sailor collar) at the centreline, to keep the tail close but outside it
    obst = head.obst.reshape(-1, 3)
    cen = obst[np.abs(obst[:, 0]) < 0.035]

    def back_z(y):
        s = cen[np.abs(cen[:, 1] - y) < 0.02]
        return s[:, 2].min() if len(s) else -0.09
    ys = np.linspace(start[1], 1.425, 9)
    radii = np.interp(np.linspace(0, 1, 9), [0, 0.15, 0.5, 0.85, 1.0], [0.024, 0.026, 0.023, 0.017, 0.008])
    C = []
    for k, y in enumerate(ys):
        if k == 0:
            C.append(start); continue
        z = min(back_z(y), start[2] + 0.02) - radii[k] - 0.009
        z = min(z, C[-1][2] + 0.004)                            # never tucks forward faster than it falls
        C.append(np.array([0.0, y, z]))
    C = np.array(C)
    # smooth the centreline a little
    for _ in range(2):
        C[1:-1] = 0.25 * C[:-2] + 0.5 * C[1:-1] + 0.25 * C[2:]
    V, T, ref = tube(C, radii, sides=8, squash=(1.15, 0.8), up=(0, 0, 1))
    V = head.push_out(V, clear=0.006, from_pt=None)
    m.add(V, T, flip_ref=ref)
    # the tie: a short fat band where the tail starts
    tc = C[0] + unit(C[1] - C[0]) * 0.006
    Vt, Tt, reft = tube(np.array([C[0] - unit(C[1] - C[0]) * 0.004, tc]), np.array([0.016, 0.016]), sides=8,
                        up=(0, 0, 1), tip_end=True, tip_start=True)
    m.add(head.push_out(Vt), Tt, flip_ref=reft)
    return m


def style_rolls(head):
    m = Mesh()
    G = pulled_back(head, m, back_low=lambda ph: -14.0 if abs(ph) > 120 else 99)
    # two victory rolls: fat tubes on the top/front either side of a centre part, from the forehead corner back
    # toward the crown, sunk into the shell at their ends
    for sgn in (-1, 1):
        path = [(sgn * 18, 31), (sgn * 26, 43), (sgn * 36, 54), (sgn * 48, 62)]
        tt = np.linspace(0, 1, 8)
        s_ = np.linspace(0, 1, len(path))
        ph_i = np.interp(tt, s_, [p[0] for p in path]); th_i = np.interp(tt, s_, [p[1] for p in path])
        dirs = head.dir(ph_i, th_i)
        r = head.scalp(dirs)
        radii = 0.021 * np.interp(tt, [0, 0.15, 0.6, 1], [0.85, 1.0, 0.85, 0.55])   # fat roll end at the front
        C = head.c + dirs * (r + 0.004 + radii * np.interp(tt, [0, 0.2, 0.8, 1], [0.3, 0.32, 0.25, 0.05]))[:, None]
        V, T, ref = tube(C, radii, sides=8, up=(0, 1, 0), tip_end=True, tip_start=False)
        V = head.push_out(V)
        m.add(V, T, flip_ref=ref)
    # the back gathered into a roll along the nape
    ph = np.linspace(128, 232, 9)
    th = np.full_like(ph, -20.0)
    dirs = head.dir(ph, th)
    r = head.scalp(dirs)
    tt = np.linspace(0, 1, len(ph))
    radii = 0.016 * (0.6 + 0.4 * np.sin(np.pi * tt) ** 0.5)
    C = head.c + dirs * (r + 0.004 + radii * 0.6)[:, None]
    V, T, ref = tube(C, radii, sides=8, up=(0, 1, 0), tip_end=True, tip_start=True)
    V = head.push_out(V)
    m.add(V, T, flip_ref=ref)
    return m


BUILDERS = {'bob': style_bob, 'bun': style_bun, 'ponytail': style_ponytail, 'rolls': style_rolls}


# ---------------------------------------------------------------- uv, file, checks

def make_uv(V, head):
    lo, hi = V.min(0), V.max(0)
    u = U0 + (U1 - U0) * np.clip((V[:, 1] - 1.40) / 0.36, 0, 1)
    v = V0 + (V1 - V0) * np.clip((V[:, 2] + 0.14) / 0.28, 0, 1)
    return np.stack([u, v], -1)


def to_mesh_space(V, N, M):
    R = M[:3, :3]; t = M[:3, 3]
    Ri = np.linalg.inv(R)
    pos = (Ri @ (V - t).T).T
    nrm = unit((np.linalg.inv(R / 100.0) @ N.T).T)
    return pos, nrm


def write_bchair(path, pos, nrm, uv, tri):
    with open(path, 'wb') as f:
        f.write(b'BCH1' + struct.pack('<II', len(pos), len(tri)))
        f.write(pos.astype('<f4').tobytes()); f.write(nrm.astype('<f4').tobytes())
        f.write(uv.astype('<f4').tobytes()); f.write(tri.astype('<i4').tobytes())


def read_bchair(path):
    b = pathlib.Path(path).read_bytes()
    assert b[:4] == b'BCH1'
    nv, nt = struct.unpack_from('<II', b, 4)
    o = 12
    pos = np.frombuffer(b, '<f4', nv * 3, o).reshape(nv, 3); o += nv * 12
    nrm = np.frombuffer(b, '<f4', nv * 3, o).reshape(nv, 3); o += nv * 12
    uv = np.frombuffer(b, '<f4', nv * 2, o).reshape(nv, 2); o += nv * 8
    tri = np.frombuffer(b, '<i4', nt * 3, o).reshape(nt, 3); o += nt * 12
    assert o == len(b), 'trailing bytes'
    return pos, nrm, uv, tri


def winding(pos, nrm, tri):
    fn = unit(np.cross(pos[tri[:, 1]] - pos[tri[:, 0]], pos[tri[:, 2]] - pos[tri[:, 0]]))
    return float((fn * nrm[tri].mean(1)).sum(1).mean()), float(((fn * nrm[tri].mean(1)).sum(1) > 0).mean())


def hat_poke(head, V, which, crown_y):
    """Hair poking through a hat's crown: the ray from the head centre to a hair vertex crosses the hat's surface
    first, at a point above crown_y (the band and brim are left out: hair showing under a brim is normal).
    Returns (mask of poking vertices, depth beyond the hat surface in m)."""
    tri = head.capinner[which]
    dirs = unit(V - head.c)
    rv = np.linalg.norm(V - head.c, axis=1)
    rh = raycast(head.c, dirs, tri)
    hy = head.c[1] + dirs[:, 1] * np.where(np.isfinite(rh), rh, 0)
    crown = np.isfinite(rh) & (hy > crown_y)
    depth = np.where(crown, rv - rh, -np.inf)
    return depth > 0, depth


def eye_cover(head, tri):
    """Fraction of eye vertices hidden behind hair, seen from the front and from 35 degrees each side."""
    E = head.eyes['P']
    res = []
    for yaw in (0, 35, -35):
        d = np.array([math.sin(math.radians(yaw)), 0, math.cos(math.radians(yaw))])
        hit = np.array([np.isfinite(raycast(e, d[None], tri)[0]) for e in E[::4]])
        res.append(hit.mean())
    return res


# ---------------------------------------------------------------- preview

def preview(head, built, png):
    sys.path.insert(0, str(pathlib.Path(__file__).parent))
    import preview3d as pv
    from PIL import Image, ImageDraw
    exp = head.exp
    skin, eye, mouth, shirt, cap, helm = '#d2a37f', '#f2f2f2', '#a8584e', '#c9c3ae', '#ececec', '#5d6248'

    def tris(m, col, ymin=1.30):
        P, N, T = m['P'], m['N'], m['T']
        uv = m['uv'] if 'uv' in m else np.zeros((len(P), 2))
        keep = P[T][:, :, 1].max(1) > ymin
        return [(col, P[t], N[t], uv[t]) for t in T[keep]]
    fem = exp.parent / 'female'
    base = (tris(load(fem, 'Body_CutCovered'), skin) + tris(head.shirt, shirt) + tris(head.eyes, eye) +
            tris(head.brows, HAIR_COL) + tris(head.mouth, mouth))
    capT = tris(head.cap, cap); helT = tris(head.helmet, helm)
    S = 250
    views = [('front', (0, 0.05, 1), None), ('side', (1, 0.05, 0), None), ('back', (0, 0.1, -1), None),
             ('3/4', (0.7, 0.15, 0.7), None), ('3/4 back', (-0.7, 0.15, -0.7), None),
             ('cap 3/4', (0.7, 0.15, 0.7), 'cap'), ('cap side', (-1, 0.05, 0), 'cap'), ('cap back', (0, 0.1, -1), 'cap'),
             ('helmet 3/4', (0.7, 0.15, 0.7), 'helmet')]
    sheet = Image.new('RGB', (S * len(views), S * (len(built) + 1)), (255, 255, 255))
    center = np.array([0, 1.585, 0.0]); scale = S * 0.46 / 0.20
    rows = [('stock Hair_1', tris(head.hairref, HAIR_COL))] + [(k, [(HAIR_COL, V[t], N[t], np.zeros((3, 2))) for t in T])
                                                              for k, (V, N, T) in built.items()]
    for r, (name, hair) in enumerate(rows):
        for c, (vn, eyev, hat) in enumerate(views):
            tr = base + hair + (capT if hat == 'cap' else helT if hat == 'helmet' else [])
            im = pv.render(tr, eyev, size=S, center=center, scale=scale)
            ImageDraw.Draw(im).text((5, 4), '%s  %s' % (name, vn), fill=(10, 10, 10))
            sheet.paste(im, (c * S, r * S))
    sheet.save(png)


# ---------------------------------------------------------------- main

def main():
    exp = pathlib.Path(sys.argv[1]); mdir = pathlib.Path(sys.argv[2])
    head = Head(exp); head.exp = exp
    M = head.hairref['M']
    w0 = winding(head.hairref['d']['pos'], head.hairref['d']['nrm'], head.hairref['T'])
    print('stock Hair_1 winding (mean face.n dot vertex n, fraction >0): %.3f %.3f' % w0)
    built = {}
    for name in STYLES:
        mesh = BUILDERS[name](head)
        V, N, T = mesh.arrays()
        built[name] = (V, N, T)
        uv = make_uv(V, head)
        pos, nrm = to_mesh_space(V, N, M)
        path = mdir / ('female_hair_%s.bchair' % name)
        write_bchair(path, pos, nrm, uv, T)
        # read back and check
        p2, n2, uv2, t2 = read_bchair(path)
        assert len(p2) < 65535 and t2.min() >= 0 and t2.max() < len(p2)
        wd = winding(p2.astype(float), n2.astype(float), t2)
        cl = head.clearance(V)
        sc = head.scalp_clear(V[V[:, 1] > 1.56])
        eyes = eye_cover(head, V[T])
        capo, capd = hat_poke(head, V, 'cap', CAP_CROWN_Y); helo, held = hat_poke(head, V, 'helmet', HELMET_CROWN_Y)
        print('%-9s nv %4d ntri %4d  min clearance %.1f mm (scalp-region %.1f..%.1f mm)  winding %.3f/%.2f  '
              'uv u %.4f-%.4f v %.4f-%.4f  |n| %.3f-%.3f'
              % (name, len(p2), len(t2), cl.min() * 1000, sc.min() * 1000, np.percentile(sc, 95) * 1000, wd[0], wd[1],
                 uv2[:, 0].min(), uv2[:, 0].max(), uv2[:, 1].min(), uv2[:, 1].max(),
                 np.linalg.norm(n2, axis=1).min(), np.linalg.norm(n2, axis=1).max()))
        tko, tkd = hat_poke(head, V, 'talker', HELMET_CROWN_Y) if 'talker' in head.capinner else (np.zeros(1, bool), np.zeros(1))
        print('          eyes hidden (front, +35, -35 yaw): %s   through the crown: sailor cap %d verts (max %.1f mm), '
              'helmet %d verts (max %.1f mm), talker helmet %d verts (max %.1f mm)'
              % (['%.0f%%' % (e * 100) for e in eyes], capo.sum(), max(capd.max(), 0) * 1000,
                 helo.sum(), max(held.max(), 0) * 1000, tko.sum(), max(tkd.max(), 0) * 1000))
    # stock reference against the hats
    Vs = head.hairref['P']
    capo, capd = hat_poke(head, Vs, 'cap', CAP_CROWN_Y); helo, held = hat_poke(head, Vs, 'helmet', HELMET_CROWN_Y)
    tko, tkd = hat_poke(head, Vs, 'talker', HELMET_CROWN_Y) if 'talker' in head.capinner else (np.zeros(1, bool), np.zeros(1))
    print('stock Hair_1 for comparison: cap %d verts out (max %.1f mm)  helmet %d out (max %.1f mm)  talker %d out (max %.1f mm)'
          % (capo.sum(), max(capd.max(), 0) * 1000, helo.sum(), max(held.max(), 0) * 1000, tko.sum(), max(tkd.max(), 0) * 1000))
    print('(the mod swaps victory rolls for the bun under H_Helmet and H_HelmetTalker)')
    if '--preview' in sys.argv:
        preview(head, built, sys.argv[sys.argv.index('--preview') + 1])


if __name__ == '__main__':
    main()
