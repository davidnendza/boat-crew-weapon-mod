"""Builds the female crew bra (models/female_bra.bcbra) for the weapon mod, plus a preview PNG.

Worn by women whose shirt is 'none' (the game then shows the full 'Body' mesh). A light grey band cut out of the crew
FULL_BODY surface: cups with a centre V, a band round the back and two shoulder straps, 3.5 mm off the skin, with a
short skirt down to 0.6 mm above the skin so the edge has thickness. Colour: the untinted light grey of the stock
Underwear briefs (albedo 206,207,206, segmentation black), so no tint channel touches it.

How it is made (all in the BaseCharacter frame, metres: x lateral, y up, z forward; mesh space p -> P = 100*(x, z, -y),
exactly as FemAfter in weaponmod.cpp does it):
  1. body P, deformed body Pd = P + field(P): field() is the approved 'v2' shape, a 1:1 copy of FemField in weaponmod.cpp
     (and of deform_female.py field()); change all three together
  2. torso triangles, each subdivided n x n on the DEFORMED facets, clipped smoothly along region() at f = 0
  3. weld, area-weighted normals, offset outward by OFFSET, skirt at the edges
  4. bone weights = barycentric mix of the body corners (top 4, renormalised, sorted); UV = one point in the grey patch
  5. the field is PRE-INVERTED per vertex (q + field(q) = target), so the mod's normal FemAfter pass moves the bra onto
     the bust together with the body (float32 round trip checked). The file is therefore tied to field v2.
  6. the runtime is simulated (field on every vertex), clearance to the deformed body and to V_VestLife is printed

File format (little-endian), read by the mod: char[4] 'BCB1'; uint32 nv; uint32 ntri; float32 pos[nv*3] (mesh space,
before the field); float32 nrm[nv*3] (final shape, mesh space); float32 uv[nv*2]; BoneWeight[nv] (float32 w[4] then
int32 idx[4], Unity's layout; indices into the 27 crew bones); int32 tri[ntri*3]. Size = 12 + nv*64 + ntri*12.

Inputs: the crew meshes exported from the game as .npz (FULL_BODY, V_VestLife, Pants_Sailor, Shoes, Eyes, Mouth_1;
export_crew_mesh.py in the female-crew scratch folder) and models/female_hair_bun.bchair for the preview.

Usage: python make_female_bra_assets.py <export folder> <models folder> [--preview <png>]
"""
import os, sys, struct
import numpy as np
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
EXP = None
MODELS = None



def field(P, dom):
    """The approved v2 female shape (FemField in weaponmod.cpp). P: (n,3) BaseCharacter frame; dom: dominant bone names."""
    x, y, z = P[:, 0], P[:, 1], P[:, 2]
    D = np.zeros_like(P)
    torso = np.abs(x) < 0.26
    front = np.clip((z + 0.02) / 0.06, 0, 1)
    for sx in (-0.078, 0.078):                                       # bust
        b = g(x - sx, 0.060) * g(y - 1.295, 0.050) * front * torso
        D[:, 2] += 0.055 * b
        D[:, 1] -= 0.014 * b
    w = 0.12 * g(y - 1.06, 0.07) * torso                             # waist
    D[:, 0] += -x * w; D[:, 2] += -(z + 0.07) * w * 0.6
    h = 0.10 * g(y - 0.82, 0.07) * (np.abs(x) < 0.30)                # hips
    D[:, 0] += x * h
    s = 0.07 * g(y - 1.42, 0.06) * torso                             # shoulders
    D[:, 0] += -x * s
    arm = np.isin(dom, ['ArmUpper.L', 'ArmUpper.R', 'ArmLower.L', 'ArmLower.R'])
    D[arm, 0] += -np.sign(x[arm]) * 0.012
    return D


R0 = np.array([[1, 0, 0], [0, 0, 1], [0, -1, 0]], float)          # mesh -> BaseCharacter (times 100)
OFFSET = 0.0035                                                     # m outward from the body facets
SKIRT = 0.0006                                                      # m: skirt bottom, just above the skin
NSUB = int(os.environ.get('NSUB', '6'))
UV = (0.9510, 0.4530)                                               # Underwear patch, albedo (206,207,206), seg 0


def to_bc(p): return 100.0 * (R0 @ np.asarray(p, float).T).T
def to_mesh(P): return (R0.T @ (np.asarray(P, float) / 100.0).T).T


def load(name):
    d = np.load(os.path.join(EXP, name + '.npz'))
    bn = [str(b) for b in d['bone_names']]
    W = np.zeros((len(d['pos']), 27))
    for k in range(d['bone_idx'].shape[1]):
        np.add.at(W, (np.arange(len(W)), d['bone_idx'][:, k]), d['bone_w'][:, k])
    dom = np.array([bn[i] for i in W.argmax(1)])
    return d, bn, W, dom


def g(x, s): return np.exp(-0.5 * (x / s) ** 2)


ZC = -0.02                                                          # torso axis (z) for the wrap angle


def region(P):
    """f >= 0 inside. Defined on the deformed body (bust centres x +-0.078, y ~1.281 after the 14 mm drop)."""
    x, y, z = P[:, 0], P[:, 1], P[:, 2]
    th = np.arctan2(x, z - ZC)
    wf = np.clip((np.cos(th) - 0.15) / 0.45, 0, 1)                   # 1 on the front, 0 from the sides round the back
    top_front = 1.352 - 0.050 * g(x, 0.028) - 0.012 * g(np.abs(x) - 0.14, 0.03)   # V at the gore, a touch lower at the sides
    yhi = wf * top_front + (1 - wf) * 1.268
    ylo = wf * 1.200 + (1 - wf) * 1.222
    band = np.minimum(y - ylo, yhi - y)
    # straps: a slab |x| ~ 0.092 +- 0.010 over the shoulder, from the cups up and down the back to the band
    xs = 0.092 - 0.010 * np.clip((y - 1.30) / 0.14, 0, 1)            # leans in a little toward the neck
    strap = np.minimum(0.010 - np.abs(np.abs(x) - xs), y - 1.24)
    return np.maximum(band, strap)


def build():
    d, bn, W, dom = load('FULL_BODY')
    P = to_bc(d['pos']); T = d['tris']
    Pd = P + field(P, dom)
    bad = np.isin(dom, ['ArmLower.L', 'ArmLower.R', 'Hand.L', 'Hand.R', 'Head', 'LegUpper.L', 'LegUpper.R',
                        'LegLower.L', 'LegLower.R', 'Foot.L', 'Foot.R'])
    cy = Pd[T].mean(1)[:, 1]
    cand = [t for t in range(len(T)) if not bad[T[t]].any() and 1.12 < cy[t] < 1.56]
    # an arm vertex is allowed only if the triangle's other corners are torso: keeps the armpit closed but the arm out
    armu = np.isin(dom, ['ArmUpper.L', 'ArmUpper.R'])
    cand = [t for t in cand if armu[T[t]].sum() <= 1]
    pts, wts, tris = [], [], []
    keyidx = {}

    def vid(p, w):
        k = tuple(np.round(p / 1e-7).astype(np.int64))
        if k not in keyidx:
            keyidx[k] = len(pts); pts.append(p); wts.append(w)
        return keyidx[k]
    n = NSUB
    for t in cand:
        a, b, c = T[t]
        grid = {}
        for i in range(n + 1):
            for j in range(n + 1 - i):
                k = n - i - j
                l = np.array([i, j, k], float) / n
                p = l[0] * Pd[a] + l[1] * Pd[b] + l[2] * Pd[c]
                w = l[0] * W[a] + l[1] * W[b] + l[2] * W[c]
                grid[(i, j)] = (p, w)
        F = {k: region(v[0][None])[0] for k, v in grid.items()}
        subs = []
        for i in range(n):
            for j in range(n - i):
                subs.append([(i, j), (i + 1, j), (i, j + 1)])
                if i + j + 1 < n:
                    subs.append([(i + 1, j), (i + 1, j + 1), (i, j + 1)])
        for s in subs:
            fv = [F[q] for q in s]
            if max(fv) < 0:
                continue
            poly = []
            for e in range(3):
                q0, q1 = s[e], s[(e + 1) % 3]
                f0, f1 = F[q0], F[q1]
                if f0 >= 0:
                    poly.append(grid[q0])
                if (f0 >= 0) != (f1 >= 0):
                    # canonical order so neighbours compute the same point
                    A, B, fa, fb = (q0, q1, f0, f1) if q0 < q1 else (q1, q0, f1, f0)
                    u = fa / (fa - fb)
                    pa, wa = grid[A]; pb, wb = grid[B]
                    poly.append((pa + u * (pb - pa), wa + u * (wb - wa)))
            ids = [vid(p, w) for p, w in poly]
            for m in range(1, len(ids) - 1):
                tri = (ids[0], ids[m], ids[m + 1])
                if len(set(tri)) == 3:
                    tris.append(tri)
    X = np.array(pts); Wd = np.array(wts); Tb = np.array(tris)
    # drop slivers (area < 1e-8 m^2) produced by clipping right at a grid point
    ar = 0.5 * np.linalg.norm(np.cross(X[Tb[:, 1]] - X[Tb[:, 0]], X[Tb[:, 2]] - X[Tb[:, 0]]), axis=1)
    Tb = Tb[ar > 1e-9]
    # keep the largest connected piece only (stray islands where the slab grazes the neck/arms)
    parent = list(range(len(X)))

    def fnd(i):
        while parent[i] != i:
            parent[i] = parent[parent[i]]; i = parent[i]
        return i
    for a, b, c in Tb:
        for u, v in ((a, b), (b, c)):
            ru, rv = fnd(u), fnd(v)
            if ru != rv: parent[ru] = rv
    roots = np.array([fnd(i) for i in range(len(X))])
    rt = roots[Tb[:, 0]]
    vals, cnt = np.unique(rt, return_counts=True)
    print('pieces (tris):', sorted(cnt.tolist(), reverse=True)[:6])
    Tb = Tb[rt == vals[cnt.argmax()]]
    used = np.unique(Tb); remap = -np.ones(len(X), int); remap[used] = np.arange(len(used))
    X = X[used]; Wd = Wd[used]; Tb = remap[Tb]
    # orientation: the body winding gives outward normals? check against the body normals
    fn = np.cross(X[Tb[:, 1]] - X[Tb[:, 0]], X[Tb[:, 2]] - X[Tb[:, 0]])
    Nv = np.zeros_like(X)
    for k in range(3): np.add.at(Nv, Tb[:, k], fn)
    Nv /= np.linalg.norm(Nv, axis=1, keepdims=True)
    outward = np.sign(np.mean(np.sum(Nv[:, [0, 2]] * (X[:, [0, 2]] - [0, ZC]), axis=1)))
    print('normal sign vs radial:', outward)
    if outward < 0:
        Tb = Tb[:, ::-1]; Nv = -Nv
    return X, Nv, Wd, Tb, (P, Pd, T, W, dom, d)


def add_offset_and_skirt(X, Nv, Wd, Tb):
    top = X + OFFSET * Nv
    # boundary edges (used once, directed as in the triangles)
    from collections import Counter
    ec = Counter()
    for a, b, c in Tb:
        for u, v in ((a, b), (b, c), (c, a)):
            ec[(min(u, v), max(u, v))] += 1
    bnd = []
    for a, b, c in Tb:
        for u, v in ((a, b), (b, c), (c, a)):
            if ec[(min(u, v), max(u, v))] == 1:
                bnd.append((u, v))
    bv = sorted({u for e in bnd for u in e})
    sk = {u: len(X) + i for i, u in enumerate(bv)}
    Xs = np.vstack([top, X[bv] + SKIRT * Nv[bv]])
    Ns = np.vstack([Nv, Nv[bv]])
    Ws = np.vstack([Wd, Wd[bv]])
    quads = []
    for u, v in bnd:                                                 # triangle u->v outward: skirt continues it downward
        quads += [(v, u, sk[u]), (v, sk[u], sk[v])]
    T2 = np.vstack([Tb, np.array(quads)])
    return Xs, Ns, Ws, T2, len(bnd)


def top4(W):
    idx = np.argsort(-W, axis=1)[:, :4]
    w = np.take_along_axis(W, idx, 1)
    w = np.where(w < 1e-4, 0, w)
    w /= w.sum(1, keepdims=True)
    idx = np.where(w > 0, idx, 0)
    order = np.argsort(-w, axis=1)
    return np.take_along_axis(idx, order, 1).astype(np.int32), np.take_along_axis(w, order, 1).astype(np.float32)


def runtime_forward(pos_mesh_f32, bidx, bw):
    """FemAfter in numpy: field on every vertex, from float32 mesh positions, written back as float32."""
    p = pos_mesh_f32.astype(np.float64)
    Pb = np.stack([100 * p[:, 0], 100 * p[:, 2], -100 * p[:, 1]], 1)
    best = bidx[np.arange(len(p)), bw.argmax(1)]
    arm_names = np.array(['', 'ArmUpper.L'])
    dom = np.where(np.isin(best, [5, 6, 9, 10]), 'ArmUpper.L', 'Chest')   # field() only tests arm membership
    D = field(Pb, dom)
    out = pos_mesh_f32.copy()
    out[:, 0] = (out[:, 0] + (D[:, 0] / 100.0)).astype(np.float32)
    out[:, 1] = (out[:, 1] + (-D[:, 2] / 100.0)).astype(np.float32)
    out[:, 2] = (out[:, 2] + (D[:, 1] / 100.0)).astype(np.float32)
    return out


def invert(target_bc, bidx, bw):
    best = bidx[np.arange(len(target_bc)), bw.argmax(1)]
    dom = np.where(np.isin(best, [5, 6, 9, 10]), 'ArmUpper.L', 'Chest')
    q = target_bc.copy()
    for it in range(60):
        q = target_bc - field(q, dom)
    res = np.linalg.norm(q + field(q, dom) - target_bc, axis=1).max()
    return q, res


def closest_on_tris(p, A, B, C):
    """Exact closest points from points p (N,3) to each triangle (M,3); returns dist (N,M), closest (N,M,3)."""
    ab = B - A; ac = C - A
    ap = p[:, None, :] - A[None]
    d1 = (ab[None] * ap).sum(-1); d2 = (ac[None] * ap).sum(-1)
    bp = p[:, None, :] - B[None]
    d3 = (ab[None] * bp).sum(-1); d4 = (ac[None] * bp).sum(-1)
    cp = p[:, None, :] - C[None]
    d5 = (ab[None] * cp).sum(-1); d6 = (ac[None] * cp).sum(-1)
    va = d3 * d6 - d5 * d4; vb = d5 * d2 - d1 * d6; vc = d1 * d4 - d3 * d2
    den = va + vb + vc
    den = np.where(np.abs(den) < 1e-30, 1e-30, den)
    v = vb / den; w = vc / den
    res = A[None] + ab[None] * v[..., None] + ac[None] * w[..., None]
    def put(mask, val):
        nonlocal res
        res = np.where(mask[..., None], val, res)
    # edge / vertex regions (later masks override earlier, ordered so vertex regions win)
    m = (vc <= 0) & (d1 >= 0) & (d3 <= 0); t = d1 / np.where(d1 - d3 == 0, 1, d1 - d3); put(m, A[None] + t[..., None] * ab[None])
    m = (vb <= 0) & (d2 >= 0) & (d6 <= 0); t = d2 / np.where(d2 - d6 == 0, 1, d2 - d6); put(m, A[None] + t[..., None] * ac[None])
    m = (va <= 0) & ((d4 - d3) >= 0) & ((d5 - d6) >= 0)
    t = (d4 - d3) / np.where((d4 - d3) + (d5 - d6) == 0, 1, (d4 - d3) + (d5 - d6)); put(m, B[None] + t[..., None] * (C - B)[None])
    put((d1 <= 0) & (d2 <= 0), np.broadcast_to(A[None], res.shape))
    put((d3 >= 0) & (d4 <= d3), np.broadcast_to(B[None], res.shape))
    put((d6 >= 0) & (d5 <= d6), np.broadcast_to(C[None], res.shape))
    dist = np.linalg.norm(p[:, None, :] - res, axis=-1)
    return dist, res


def signed_clearance(pts, V, T, sel=None):
    if sel is not None: T = T[sel]
    A, B, C = V[T[:, 0]], V[T[:, 1]], V[T[:, 2]]
    fn = np.cross(B - A, C - A); fn /= np.maximum(np.linalg.norm(fn, axis=1, keepdims=True), 1e-30)
    out = np.empty(len(pts))
    for s in range(0, len(pts), 200):
        p = pts[s:s + 200]
        dist, cl = closest_on_tris(p, A, B, C)
        j = dist.argmin(1)
        sgn = np.sign(((p - cl[np.arange(len(p)), j]) * fn[j]).sum(1))
        out[s:s + 200] = sgn * dist[np.arange(len(p)), j]
    return out


def tri_samples(V, T, k=3):
    """Vertices plus interior/edge samples of each triangle."""
    out = [V]
    for i in range(k + 1):
        for j in range(k + 1 - i):
            l = np.array([i, j, k - i - j], float) / k
            if l.max() == 1: continue
            out.append(l[0] * V[T[:, 0]] + l[1] * V[T[:, 1]] + l[2] * V[T[:, 2]])
    return np.vstack(out)


def welded_normals(V, T, moved, N0):
    key = np.round(V / 1e-7).astype(np.int64)                         # mesh units, as FemAfter (1e-7)
    _, inv = np.unique(key, axis=0, return_inverse=True); inv = inv.ravel()
    fn = np.cross(V[T[:, 1]] - V[T[:, 0]], V[T[:, 2]] - V[T[:, 0]])
    acc = np.zeros((inv.max() + 1, 3))
    for k in range(3): np.add.at(acc, inv[T[:, k]], fn)
    N = acc[inv]; N /= np.maximum(np.linalg.norm(N, axis=1, keepdims=True), 1e-30)
    out = N0.copy(); out[moved] = N[moved]
    return out


def main():
    X, Nv, Wd, Tb, (P, Pd, T, W, dom, d) = build()
    nb_v, nb_t = len(X), len(Tb)
    Xs, Ns, Ws, T2, nbnd = add_offset_and_skirt(X, Nv, Wd, Tb)
    bidx, bw = top4(Ws)
    print('bra surface: %d verts, %d tris; with skirt: %d verts, %d tris, %d boundary edges' % (nb_v, nb_t, len(Xs), len(T2), nbnd))
    # pre-invert the field
    q, res = invert(Xs, bidx, bw)
    print('field inversion residual %.3g m' % res)
    pos_mesh = to_mesh(q).astype(np.float32)
    nrm_mesh = (R0.T @ Ns.T).T.astype(np.float32)                    # final-shape normals (unmoved vertices keep these)
    # runtime simulation: body + bra both go through FemAfter
    body_mesh = d['pos'].astype(np.float32)
    bodyw_idx = d['bone_idx'].astype(np.int32); bodyw = d['bone_w'].astype(np.float32)
    body_after = runtime_forward(body_mesh, bodyw_idx, bodyw)
    bra_after = runtime_forward(pos_mesh, bidx, bw)
    err = np.linalg.norm(to_bc(bra_after) - Xs, axis=1)
    print('float32 round trip: max %.3f mm, mean %.4f mm' % (err.max() * 1000, err.mean() * 1000))
    Bd = to_bc(body_after)
    print('body: runtime vs offline deformed max diff %.4f mm' % (np.abs(Bd - Pd).max() * 1000))
    # clearance to the deformed body (bra samples, top surface only and the skirt)
    Vbra = to_bc(bra_after)
    top_s = tri_samples(Vbra[:nb_v], T2[:nb_t], 3)
    skirt_s = Vbra[nb_v:]
    near = (Bd[T].mean(1)[:, 1] > 1.05) & (Bd[T].mean(1)[:, 1] < 1.62)
    ct = signed_clearance(top_s, Bd, T, near)
    cs = signed_clearance(skirt_s, Bd, T, near)
    print('clearance top surface (%d samples): min %.2f mm, p1 %.2f, median %.2f, max %.2f' % (len(top_s), ct.min() * 1000, np.percentile(ct, 1) * 1000, np.median(ct) * 1000, ct.max() * 1000))
    print('clearance skirt bottom (%d): min %.2f mm, median %.2f' % (len(cs), cs.min() * 1000, np.median(cs) * 1000))
    # weights summary
    best = bidx[np.arange(len(bidx)), 0]
    bn = [str(b) for b in d['bone_names']]
    u, c = np.unique(best, return_counts=True)
    print('dominant bones:', {bn[a]: int(b) for a, b in zip(u, c)})
    print('verts with any non-Chest weight > 0.05:', int(((bw > 0.05) & (bidx != 1)).any(1).sum()), 'of', len(bw))
    # vest check: the bra must stay inside V_VestLife (same field)
    vd, vbn, vW, vdom = load('V_VestLife')
    vest_after = runtime_forward(vd['pos'].astype(np.float32), vd['bone_idx'].astype(np.int32), vd['bone_w'].astype(np.float32))
    Vv = to_bc(vest_after)
    cv = signed_clearance(top_s, Vv, vd['tris'])
    av = np.abs(cv)
    print('vs V_VestLife: |distance| min %.2f mm; samples within 1 mm %d, within 2 mm %d of %d (crossings show as sign flips next to each other)' % (av.min() * 1000, int((av < 0.001).sum()), int((av < 0.002).sum()), len(cv)))
        # write the file
    uv = np.tile(np.array(UV, np.float32), (len(Xs), 1))
    out = os.path.join(MODELS, 'female_bra.bcbra')
    with open(out, 'wb') as f:
        f.write(b'BCB1' + struct.pack('<II', len(Xs), len(T2)))
        f.write(pos_mesh.astype('<f4').tobytes()); f.write(nrm_mesh.astype('<f4').tobytes()); f.write(uv.astype('<f4').tobytes())
        bwrec = np.zeros(len(Xs), dtype=[('w', '<f4', 4), ('i', '<i4', 4)]); bwrec['w'] = bw; bwrec['i'] = bidx
        f.write(bwrec.tobytes()); f.write(T2.astype('<i4').tobytes())
    print('wrote', out, os.path.getsize(out), 'bytes')
    check_file(out, len(Xs), len(T2))
    if PREVIEW: render(d, body_after, bra_after, T2, PREVIEW)


def render(d, body_after, bra_after, T2, png):
    import preview3d as pv
    from PIL import Image, ImageDraw
    # moved-vertex normals as FemAfter recomputes them, over the whole combined set (body + pants + bra)
    parts = [('FULL_BODY', '#d2a37f'), ('Pants_Sailor', '#345b72'), ('Shoes', '#3a3a3a'), ('Eyes', '#f2f2f2'), ('Mouth_1', '#a8584e')]
    V, N, Tt, cols = [], [], [], []
    off = 0
    for nm, col in parts:
        pd_ = np.load(os.path.join(EXP, nm + '.npz'))
        pa = runtime_forward(pd_['pos'].astype(np.float32), pd_['bone_idx'].astype(np.int32), pd_['bone_w'].astype(np.float32))
        V.append(pa); N.append(pd_['nrm']); Tt.append(pd_['tris'] + off); cols += [col] * len(pd_['tris']); off += len(pa)
    hair = open(os.path.join(MODELS, 'female_hair_bun.bchair'), 'rb').read()
    nv, nt = struct.unpack_from('<II', hair, 4)
    hp = np.frombuffer(hair, '<f4', nv * 3, 12).reshape(-1, 3); hn = np.frombuffer(hair, '<f4', nv * 3, 12 + nv * 12).reshape(-1, 3)
    ht = np.frombuffer(hair, '<i4', nt * 3, 12 + nv * 32).reshape(-1, 3)
    V.append(hp); N.append(hn); Tt.append(ht + off); cols += ['#4a3624'] * nt; off += nv
    n_nobra = off
    V.append(bra_after); N.append(np.zeros_like(bra_after)); Tt.append(T2 + off); cols += ['#cecece'] * len(T2); off += len(bra_after)
    Vall = np.vstack(V).astype(np.float64); N0 = np.vstack(N).astype(np.float64); Tall = np.vstack(Tt)
    moved = np.ones(len(Vall), bool); moved[n_nobra - nv:n_nobra] = False   # hair is Head, not moved
    Nall = welded_normals(Vall, Tall, moved, N0)
    Pw = to_bc(Vall); Nw = (R0 @ Nall.T).T
    tris = [(cols[i], Pw[t], Nw[t], np.zeros((3, 2))) for i, t in enumerate(Tall)]
    tris_nobra = [tr for tr in tris[:sum(len(x) for x in Tt[:-1])]]
    S = 440
    views = [((0, 0, 1), 'front'), ((1, 0, 0.0), 'side'), ((0.6, 0.15, 0.8), '3/4'), ((0, 0, -1), 'back')]
    upper = [tr for tr in tris if tr[1][:, 1].min() > 0.95]
    upper_nb = [tr for tr in tris_nobra if tr[1][:, 1].min() > 0.95]
    allp = np.vstack([tr[1] for tr in upper]); ctr = (allp.min(0) + allp.max(0)) / 2; ctr[1] = 1.27
    row1 = [pv.render(tris, e, size=S) for e, _ in views]
    row2 = [pv.render(upper, e, size=S, center=ctr, scale=S * 0.46 / 0.30) for e, _ in views]
    row3 = [pv.render(upper_nb, e, size=S, center=ctr, scale=S * 0.46 / 0.30) for e, _ in views]
    img = Image.new('RGB', (S * 4, S * 3), (255, 255, 255))
    dr = ImageDraw.Draw(img)
    for r, row in enumerate([row1, row2, row3]):
        for c, im in enumerate(row):
            img.paste(im, (c * S, r * S))
            dr.text((c * S + 8, r * S + 6), ['female v2 + bra', 'chest close-up, bra', 'chest close-up, no bra'][r] + ' - ' + views[c][1], fill=(10, 10, 10))
    img.save(png); print('wrote', png)


def check_file(path, nv, nt):
    """Reads the file back with the mod's checks (size, index range, weights)."""
    b = open(path, 'rb').read()
    assert b[:4] == b'BCB1'
    n, t = struct.unpack_from('<II', b, 4)
    assert (n, t) == (nv, nt) and len(b) == 12 + n * 64 + t * 12 and n < 65535
    bw = np.frombuffer(b, dtype=[('w', '<f4', 4), ('i', '<i4', 4)], count=n, offset=12 + n * 32)
    tri = np.frombuffer(b, '<i4', t * 3, 12 + n * 64)
    assert tri.min() >= 0 and tri.max() < n
    assert np.abs(bw['w'].sum(1) - 1).max() < 1e-3 and bw['i'].min() >= 0 and bw['i'].max() < 27
    print('read back OK: %d vertices, %d triangles, %d bytes' % (n, t, len(b)))


if __name__ == '__main__':
    a = sys.argv[1:]
    PREVIEW = None
    if '--preview' in a:
        k = a.index('--preview'); PREVIEW = a[k + 1]; del a[k:k + 2]
    if len(a) != 2:
        sys.exit(__doc__)
    EXP, MODELS = a
    main()
