"""Small z-buffered software renderer for model previews: per-pixel UVs, the game's palette shaders approximated.

Triangles are (material name, 3 points, 3 normals, 3 uvs) in one frame (+Y up, +Z forward). Materials are looked up
with gamemesh.material(); names starting with '#' are plain colours ('#5a6070').
"""
import numpy as np
from PIL import Image, ImageDraw

import gamemesh as gm

_cache = {}


def _shade_fn(mat):
    """Returns f(uv array Nx2) -> rgb Nx3 for a game material, or a constant for '#rrggbb'."""
    if mat in _cache:
        return _cache[mat]
    if mat.startswith('#'):
        c = np.array([int(mat[i:i + 2], 16) / 255 for i in (1, 3, 5)])
        f = lambda uv: np.tile(c, (len(uv), 1))
        _cache[mat] = f
        return f
    m = gm.material(mat)
    cols = m['colors']
    base = np.array(cols.get('_Color', (1, 1, 1)))
    metal = m['floats'].get('_Metallic', 0.0)
    dim = 1.0 - 0.75 * max(0.0, metal - 0.3)                          # metals read darker than their albedo in game
    tex = np.asarray(m['tex'], float) / 255 if m['tex'] is not None else None
    palette = 'Color' in m['shader'] and '_Color1' in cols
    chans = np.array([cols.get('_Channel%d' % i, (0, 0, 0)) for i in range(1, 6)])
    pcols = np.array([cols.get('_Color%d' % i, (1, 1, 1)) for i in range(1, 6)])
    sx, sy = m['scale']

    def f(uv):
        if tex is None:
            texel = np.ones((len(uv), 3))
        else:
            H, W = tex.shape[:2]
            u = np.mod(uv[:, 0] * sx, 1.0); v = np.mod(uv[:, 1] * sy, 1.0)
            texel = tex[np.clip(((1 - v) * H).astype(int), 0, H - 1), np.clip((u * W).astype(int), 0, W - 1)]
        if palette:
            d = ((texel[:, None, :] - chans[None, :, :]) ** 2).sum(-1)
            rgb = pcols[d.argmin(1)]
            if not m['name'].startswith('Dark MG'):
                rgb = rgb * base
            else:
                rgb = np.tile([0.36, 0.37, 0.39], (len(uv), 1))           # the dark gun metal as it shows in game
        else:
            rgb = texel * base
        return np.clip(rgb * dim, 0, 1)
    _cache[mat] = f
    return f


def render(tris, eye, size=420, scale=None, center=None, light=(0.35, 0.9, 0.45), bg=(232, 235, 240), marks=()):
    """Orthographic render looking from `eye` (a direction) at the model. Returns a PIL image."""
    eye = np.asarray(eye, float); eye /= np.linalg.norm(eye)
    fwd = -eye
    right = np.cross([0, 1, 0], fwd)
    if np.linalg.norm(right) < 1e-6:
        right = np.array([1.0, 0, 0])
    right /= np.linalg.norm(right)
    up = np.cross(fwd, right)
    P = np.array([t[1] for t in tris])                                 # T x 3 x 3
    allp = P.reshape(-1, 3)
    if center is None:
        center = (allp.min(0) + allp.max(0)) / 2
    q = (P - center) @ np.array([right, up, fwd]).T                      # screen x, y, depth
    if scale is None:
        ext = np.abs(((allp - center) @ np.array([right, up]).T)).max()
        scale = size * 0.46 / max(ext, 1e-6)
    sx = size / 2 + q[..., 0] * scale
    sy = size / 2 - q[..., 1] * scale
    zb = np.full((size, size), np.inf)
    img = np.ones((size, size, 3)) * np.array(bg) / 255
    L = np.asarray(light, float); L /= np.linalg.norm(L)
    for i, (mat, ps, ns, uvs) in enumerate(tris):
        x0, x1 = int(max(0, np.floor(sx[i].min()))), int(min(size - 1, np.ceil(sx[i].max())))
        y0, y1 = int(max(0, np.floor(sy[i].min()))), int(min(size - 1, np.ceil(sy[i].max())))
        if x1 < x0 or y1 < y0:
            continue
        ax, ay, bx, by, cx, cy = sx[i][0], sy[i][0], sx[i][1], sy[i][1], sx[i][2], sy[i][2]
        den = (by - cy) * (ax - cx) + (cx - bx) * (ay - cy)
        if abs(den) < 1e-9:
            continue
        X, Y = np.meshgrid(np.arange(x0, x1 + 1) + 0.5, np.arange(y0, y1 + 1) + 0.5)
        w0 = ((by - cy) * (X - cx) + (cx - bx) * (Y - cy)) / den
        w1 = ((cy - ay) * (X - cx) + (ax - cx) * (Y - cy)) / den
        w2 = 1 - w0 - w1
        inside = (w0 >= -1e-6) & (w1 >= -1e-6) & (w2 >= -1e-6)
        if not inside.any():
            continue
        z = w0 * q[i][0, 2] + w1 * q[i][1, 2] + w2 * q[i][2, 2]
        sub = zb[y0:y1 + 1, x0:x1 + 1]
        hit = inside & (z < sub)
        if not hit.any():
            continue
        n = ns[0] * w0[hit][:, None] + ns[1] * w1[hit][:, None] + ns[2] * w2[hit][:, None]
        n /= np.maximum(np.linalg.norm(n, axis=1, keepdims=True), 1e-9)
        n = np.where((n @ fwd)[:, None] > 0, -n, n)                     # light the side we see
        uv = uvs[0] * w0[hit][:, None] + uvs[1] * w1[hit][:, None] + uvs[2] * w2[hit][:, None]
        col = _shade_fn(mat)(uv)
        lam = 0.42 + 0.58 * np.clip(n @ L, 0, 1)
        spec = 0.12 * np.clip(n @ unit_half(L, eye), 0, 1) ** 24
        sub[hit] = z[hit]
        img[y0:y1 + 1, x0:x1 + 1][hit] = np.clip(col * lam[:, None] + spec[:, None], 0, 1)
    out = Image.fromarray((img * 255).astype(np.uint8))
    dr = ImageDraw.Draw(out)
    for p, col in marks:
        v = (np.asarray(p, float) - center) @ np.array([right, up]).T
        x, y = size / 2 + v[0] * scale, size / 2 - v[1] * scale
        dr.ellipse((x - 3, y - 3, x + 3, y + 3), fill=col)
    return out


def unit_half(L, eye):
    h = L + eye
    return h / np.linalg.norm(h)


def sheet(rows, labels, size=420):
    """rows: list of lists of PIL images (same size); labels: one text per row."""
    W = max(len(r) for r in rows) * size
    img = Image.new('RGB', (W, size * len(rows)), (255, 255, 255))
    dr = ImageDraw.Draw(img)
    for ri, r in enumerate(rows):
        for ci, im in enumerate(r):
            img.paste(im, (ci * size, ri * size))
        dr.text((8, ri * size + 6), labels[ri], fill=(10, 10, 10))
    return img
