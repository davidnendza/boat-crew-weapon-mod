"""Hull camo masks for the mod's extra camouflage schemes (wm_hull_camo.h loads them from BoatCrewMod\\models).

The game paints a player hull with a key-colour MASK in the hull's UV layout (white/red/green/blue/black -> _Color1..5,
shader Custom/Standard5ColorDamageMultipoint); every boat has its own layout. This tool rasterises each hull mesh in UV
space so every texel knows its 3D point and normal, keeps the waterline (Draft) and deck areas of the boat's stock Plain
mask, and paints each scheme as a function of hull position (along the length, height above the waterline, side).

    python tools\\hull_camo.py            writes models\\camo_<boat>_<scheme>.png (masks) and camo_icon_*.png (picker)
    python tools\\hull_camo.py --preview  also writes out\\camo_preview\\<scheme>.png side views of every boat

Needs UnityPy, numpy and Pillow; run WITHOUT python -I (UnityPy is in the user site). Reads the game read-only.
The scheme list order and names are the mod's save format (the game saves the camo as a list index and colour edits by
pattern name), so only ever append schemes and never rename one. Keep in step with kCamoSchemes in src\\wm_hull_camo.h.
"""
import glob
import pathlib
import struct
import sys

import numpy as np
from PIL import Image

sys.path.insert(0, str(pathlib.Path(__file__).parent))
import gamemesh as gm  # noqa: E402

ROOT = pathlib.Path(__file__).resolve().parent.parent
BUNDLES = gm.GAME.parent / 'StreamingAssets/aa/StandaloneWindows64'
RES = 2048
KEYS = np.array([(255, 255, 255), (255, 0, 0), (0, 255, 0), (0, 0, 255), (0, 0, 0)], np.uint8)  # key k -> _Color(k)

# boat -> hull mesh, stock Plain mask (its Draft / Deck areas are kept), and the start of the addressable GUID of the
# boat's SECOND stock pattern, which is how the mod tells the boats apart at run time (PT-20 and PT-109 share Plain)
BOATS = {
    'PT-6':   dict(meshes=['PT-6_Body_LOD0'], plain='PT-6_Hull0', guid='62ca078d'),
    'PT-20':  dict(meshes=['PT-20_Body_LOD0'], plain=None, wl=-0.6, guid='38d94ff2',   # its Plain is PT-109's mask on its own UVs
                   extra=['PT-109 Rudder'], sides_win=True),
    'PT-95':  dict(meshes=['Huckins_LOD0'], plain='PT-95_Hull0', guid='416c7db9'),
    'PT-109': dict(meshes=['PT-109_Body_LOD0'], plain='PT-109_Hull0', guid='482377a7', extra=['PT-109 Rudder']),
    'PT-658': dict(meshes=['PT-658_Body_LOD0'], plain='PT-658_Hull0', guid='84f7717d', extra=['PT-658_Rudder']),
    'PC-461': dict(meshes=['PC-461_Hull_LOD0'], plain='PC-461_Hull0', guid='b8476934',
                   extra=['PC-461_Helm_LOD0', 'PC-461_HelmBase_LOD0']),
}
# extra = other meshes of the player boat drawn with the hull material (found by listing level2's renderers): painted
# from their own 3D position, only on texels the hull mesh leaves empty (unpainted texels show as plain _Color1)


# ---------------------------------------------------------------- game data
def _mesh(name, root=None, origin=None):
    """A mesh in its player boat's frame; with origin (the hull node's position) its own offset from the hull is kept."""
    from UnityPy.helpers.MeshHelper import MeshHandler
    for o in gm.env().objects:
        if o.type.name == 'Mesh' and o.assets_file.name.startswith('sharedassets') and o.peek_name() == name:
            h = MeshHandler(o.read()); h.process()
            n = h.m_VertexCount
            V = np.array(h.m_Vertices, float).reshape(n, -1)[:, :3]
            N = np.array(h.m_Normals, float).reshape(n, -1)[:, :3]
            UV = np.array(h.m_UV0, float).reshape(n, -1)[:, :2]
            T = np.array(h.get_triangles()[0], int)                   # submesh 0 = the hull material on every boat
            M = _boat_frame(name, root); R = M[:3, :3]                # meshes are Blender-oriented and some in cm:
            V = V @ R.T                                               # bring them into the boat's frame (+Y up, +Z bow)
            if origin is not None:
                V = V + (M[:3, 3] - origin)
            N = N @ np.linalg.inv(R)
            N /= np.maximum(np.linalg.norm(N, axis=1, keepdims=True), 1e-9)
            return V, N, UV, T
    raise KeyError(name)


def _boat_frame(mesh_name, root=None):
    """4x4 of the mesh's node in its player boat's root frame (root: that boat's root name), from the scene the player
    boats live in (level2)."""
    for o in gm.env().objects:
        if o.type.name != 'MeshFilter' or o.assets_file.name != 'level2':
            continue
        mf = o.read()
        try:
            if not mf.m_Mesh.path_id or mf.m_Mesh.read().m_Name != mesh_name:
                continue
        except Exception:
            continue
        go = mf.m_GameObject.read()
        t = next(gm._comp(c) for c in go.m_Components if gm._comp(c).type.name == 'Transform').read()
        M = np.eye(4)
        while t.m_Father.path_id:                                     # up to, not including, the boat root
            M = gm.trs(t) @ M
            t = t.m_Father.read()
        rn = t.m_GameObject.read().m_Name
        if rn == root if root else rn.endswith('Player'):
            return M
    raise KeyError('no player boat holds %s' % mesh_name)


_stock = {}


def stock_mask(name):
    """A stock mask from the shiphulls bundle as key indices 1..5 at RES (row 0 = v 1)."""
    if not _stock:
        import UnityPy
        env = UnityPy.load(glob.glob(str(BUNDLES / 'shiphulls_assets_all_*.bundle'))[0])
        for o in env.objects:
            if o.type.name == 'Texture2D':
                d = o.read(); _stock[d.m_Name] = d
    im = np.asarray(_stock[name].image.convert('RGB').resize((RES, RES), Image.NEAREST), int)
    d = ((im[:, :, None, :] - KEYS[None, None].astype(int)) ** 2).sum(-1)
    return d.argmin(-1) + 1


def _read_str(b, p):
    n = struct.unpack_from('<i', b, p)[0]
    return b[p + 4:p + 4 + n].decode(), p + 4 + ((n + 3) & ~3)


def plain_roles():
    """{plain mask name: (draft slot, deck slot or 0)} from the boats' CustomPatternSlot data (raw bytes; this IL2CPP
    build has no type trees). Layout of a ColorPattern: name, int global, PPtr baseTexture, 3 strings (addressable),
    PPtr sprite, ColorEntry[] (name, Color, int channel, bool+pad, int slot), SyncEntry list, GameObject list."""
    out = {}
    for o in gm.env().objects:
        if o.type.name != 'MonoBehaviour':
            continue
        raw = o.get_raw_data()
        i = raw.find(b'\x05\x00\x00\x00Plain')
        if i < 0 or b'Camouflage' not in raw:
            continue
        p = i
        _, p = _read_str(raw, p)
        p += 4 + 12
        guid, p = _read_str(raw, p); _, p = _read_str(raw, p); _, p = _read_str(raw, p)
        p += 12
        n = struct.unpack_from('<i', raw, p)[0]; p += 4
        draft = deck = 0
        for _ in range(n):
            _, p = _read_str(raw, p)
            p += 16
            ch = struct.unpack_from('<i', raw, p)[0]; p += 8
            slot = struct.unpack_from('<i', raw, p)[0]; p += 4
            if ch == 4: draft = slot
            if ch == 3: deck = slot
        out[guid] = (draft, deck)
    return out


# ---------------------------------------------------------------- UV rasteriser
def rasterise(V, N, UV, T):
    pos = np.zeros((RES, RES, 3)); nrm = np.zeros((RES, RES, 3)); hit = np.zeros((RES, RES), bool)
    px = np.stack([UV[:, 0] * RES - 0.5, (1 - UV[:, 1]) * RES - 0.5], 1)
    for a, b, c in T:
        P = px[[a, b, c]]
        x0, y0 = np.floor(P.min(0)).astype(int); x1, y1 = np.ceil(P.max(0)).astype(int)
        x0, y0 = max(x0, 0), max(y0, 0); x1, y1 = min(x1, RES - 1), min(y1, RES - 1)
        if x1 < x0 or y1 < y0:
            continue
        xs, ys = np.meshgrid(np.arange(x0, x1 + 1), np.arange(y0, y1 + 1))
        d = (P[1, 1] - P[2, 1]) * (P[0, 0] - P[2, 0]) + (P[2, 0] - P[1, 0]) * (P[0, 1] - P[2, 1])
        if abs(d) < 1e-12:
            continue
        w0 = ((P[1, 1] - P[2, 1]) * (xs - P[2, 0]) + (P[2, 0] - P[1, 0]) * (ys - P[2, 1])) / d
        w1 = ((P[2, 1] - P[0, 1]) * (xs - P[2, 0]) + (P[0, 0] - P[2, 0]) * (ys - P[2, 1])) / d
        w2 = 1 - w0 - w1
        m = (w0 >= -0.02) & (w1 >= -0.02) & (w2 >= -0.02)             # a hair of overdraw closes seams between triangles
        if not m.any():
            continue
        W = np.stack([w0[m], w1[m], w2[m]], 1)
        pos[ys[m], xs[m]] = W @ V[[a, b, c]]
        nrm[ys[m], xs[m]] = W @ N[[a, b, c]]
        hit[ys[m], xs[m]] = True
    for _ in range(8):                                                # grow islands into the gutters (texture filtering)
        grow = ~hit
        for dy, dx in ((0, 1), (0, -1), (1, 0), (-1, 0)):
            src = np.roll(np.roll(hit, dy, 0), dx, 1) & grow
            pos[src] = np.roll(np.roll(pos, dy, 0), dx, 1)[src]
            nrm[src] = np.roll(np.roll(nrm, dy, 0), dx, 1)[src]
            hit |= src; grow &= ~src
    return pos, nrm, hit


class Hull:
    """One boat's texel geometry in boat terms: s along the length (0 stern .. 1 bow), y metres, h height (0 waterline,
    1 lowest deck edge), m metres from the stern, side +1/-1."""

    def __init__(self, boat, roles):
        cfg = BOATS[boat]
        root = boat + ' Player'
        body = [_mesh(n) for n in cfg['meshes']]
        origin = _boat_frame(cfg['meshes'][0], root)[:3, 3]
        parts = [_mesh(n, root, origin) for n in cfg.get('extra', [])] + body   # rasterised in order, last wins: hull on top
        V = np.concatenate([p[0] for p in parts]); N = np.concatenate([p[1] for p in parts])
        UV = np.concatenate([p[2] for p in parts]); off = np.cumsum([0] + [len(p[0]) for p in parts])
        T = np.concatenate([p[3] + off[i] for i, p in enumerate(parts)])
        if cfg.get('sides_win'):                                      # PT-20 reuses texels for several places (55% shared):
            f = np.abs(np.cross(V[T[:, 1]] - V[T[:, 0]], V[T[:, 2]] - V[T[:, 0]]))   # the most sideways triangles are
            T = T[np.argsort(f[:, 0] / np.maximum(np.linalg.norm(f, axis=1), 1e-9))]  # drawn last so the visible side wins
        self.pos, self.nrm, self.hit = rasterise(V, N, UV, T)
        L, W = 2, 0                                                   # Unity boat frame: +Z bow, +X starboard
        x = np.concatenate([p[0] for p in body])[:, L]; lo, hi = x.min(), x.max(); span = hi - lo
        self.s = (self.pos[:, :, L] - lo) / span
        self.m = self.s * span
        self.length = span
        self.side = np.where(self.pos[:, :, W] >= 0, 1, -1)
        self.y = self.pos[:, :, 1]
        if cfg['plain']:
            plain = stock_mask(cfg['plain'])
            draft_slot, deck_slot = roles[cfg['plain']]
            self.draft = (plain == draft_slot) & self.hit
            wl = np.percentile(self.y[self.draft], 92)
        else:                                                         # shortcut: waterline by hand (PT-109's depth below
            plain, deck_slot, wl = None, 0, cfg['wl']                 # its deck edge), upgrade if a mask of its own turns up
            self.draft = self.hit & (self.y < wl)
        sideways = self.hit & ~self.draft & (np.abs(self.nrm[:, :, 1]) < 0.5)
        bins = np.clip((self.s * 20).astype(int), 0, 19)
        tops = [np.percentile(self.y[sideways & (bins == b)], 98) for b in range(2, 18) if (sideways & (bins == b)).sum() > 50]
        self.sheer = min(tops) if tops else wl + 1.0                  # the lowest deck edge between the ends
        self.wl = wl
        self.h = (self.y - wl) / max(self.sheer - wl, 0.2)
        up = self.nrm[:, :, 1] > 0.75
        self.deck = self.hit & ~self.draft & ((plain == deck_slot) if deck_slot else (up & (self.h > 0.6)))
        print('%s: %.1f m long, waterline %.2f, deck edge %.2f, texels %d (draft %d, deck %d)' % (
            boat, span, wl, self.sheer, self.hit.sum(), self.draft.sum(), self.deck.sum()))


# ---------------------------------------------------------------- noise helpers
def vnoise(x, y, seed, cell):
    """Smooth value noise in 0..1 on metre coordinates."""
    rng = np.random.default_rng(seed)
    g = rng.random((64, 64))
    gx, gy = x / cell, y / cell
    ix, iy = np.floor(gx).astype(int), np.floor(gy).astype(int)
    fx, fy = gx - ix, gy - iy
    fx, fy = fx * fx * (3 - 2 * fx), fy * fy * (3 - 2 * fy)
    a = g[iy % 64, ix % 64]; b = g[iy % 64, (ix + 1) % 64]; c = g[(iy + 1) % 64, ix % 64]; d = g[(iy + 1) % 64, (ix + 1) % 64]
    return (a * (1 - fx) + b * fx) * (1 - fy) + (c * (1 - fx) + d * fx) * fy


def voronoi(x, y, seed, cell, n_colours, weights=None):
    """Hard-edged angular patches: the colour of the nearest of a jittered grid of points (stretched cells)."""
    rng = np.random.default_rng(seed)
    pts = []
    for i in range(-2, int(60 / cell) + 3):
        for j in range(-3, int(12 / (cell * 0.55)) + 3):
            pts.append(((i + rng.random()) * cell, (j + rng.random()) * cell * 0.55 - 2, rng.choice(n_colours, p=weights)))
    P = np.array(pts)
    best = np.full(x.shape, 1e9); col = np.zeros(x.shape, int)
    for px, py, c in P:
        d = np.abs(x - px) + 1.6 * np.abs(y - py)                    # L1-ish distance gives straight, angular edges
        m = d < best; best[m] = d[m]; col[m] = int(c)
    return col


# ---------------------------------------------------------------- schemes
# Each scheme: name (saved in the player's save, never rename), the stock pattern it stands in for (GlobalPattern: 0 Plain,
# 1 Stormy, 2 Wavy, 3 ThickStripes, 4 Dazzle), colour entries (name, slot = key 1 white 2 red 3 green 4 blue 5 black,
# rgb, GlobalColorChannel 0 base 1 bright 2 dark 3 deck 4 draft) and paint(hull) -> key per texel.
def paint_m22(H):
    line = H.sheer if H.length < 30 else H.wl + 0.55 * (H.sheer - H.wl)
    k = np.where(H.y < line - 0.02 * (H.sheer - H.wl), 5, 1)
    return k


def paint_m31(H):
    c = voronoi(H.m, H.y - H.wl, 31, 2.6, 3, [0.4, 0.35, 0.25])
    return np.choose(c, [1, 3, 5])


def paint_m32(H):
    out = np.zeros(H.s.shape, int)
    for side in (1, -1):
        rng = np.random.default_rng(32 + side)
        x = H.m; y = (H.y - H.wl)
        parity = np.zeros(H.s.shape, int)
        for _ in range(7):                                            # slanted cuts across the side, crossing parity -> wedges
            x0 = rng.uniform(0, H.length); ang = np.radians(rng.uniform(25, 70)) * rng.choice([-1, 1])
            parity += ((x - x0) * np.sin(ang) - y * np.cos(ang) > 0)
        band = (np.floor(x / max(H.length / 5, 3.0)) + parity) % 3
        sel = H.side == side
        out[sel] = np.choose(band[sel].astype(int), [1, 3, 5])
    return out


def paint_wa(H):
    x = H.m; y = H.y - H.wl
    n = vnoise(x * 0.45 + y * 0.6, y * 1.6, 7, 1.7)                    # long, slanting panels
    k = np.where(n < 0.36, 4, np.where(n < 0.55, 3, 1))
    return k


def paint_adm44(H):
    hr = H.h
    panel = (H.s > 0.16) & (H.s < 0.80) & (hr < 0.86) & (np.abs(H.nrm[:, :, 1]) < 0.85)
    return np.where(panel, 4, 1)


def paint_tiger(H):
    x = H.m; hr = np.clip(H.h, -0.2, 1.6)
    rng = np.random.default_rng(66)
    # tiger stripes: wavy near-vertical bands, wide at the top, tapering to a point lower down, each its own length
    pitch = max(1.25, H.length / 19)                                 # about 19 stripes a side on any hull
    u = x / pitch + 0.22 * np.sin(hr * 4.0 + x * 0.7) + 0.10 * np.sin(hr * 9.0 - x * 1.9)
    idx = np.floor(u).astype(int); f = u - idx
    reach = rng.uniform(-0.1, 0.45, 512)[idx % 512]                   # where each stripe's point ends
    width = 0.32 * np.clip((hr - reach) / (1.1 - reach), 0, 1) ** 0.7
    fork = (rng.random(512)[idx % 512] > 0.6) & (np.abs(f - 0.5) < 0.05) & (hr > reach + 0.35)
    stripe = (np.abs(f - 0.5) < width / 2) & ~fork
    k = np.where(stripe, 5, 1)
    # hot-rod flames from the bow, tongues streaming aft with pointed tips; red core, yellow rim
    from_bow = (1 - H.s) * H.length
    hh = hr + 0.06 * np.sin(from_bow * 1.3)
    tri = 1 - np.abs(2 * ((hh * 2.3 + 0.15) % 1) - 1)                 # 2.3 tongues per hull height
    tongue = H.length * (0.16 + 0.30 * tri ** 1.6 * (0.8 + 0.2 * np.sin(np.floor(hh * 2.3) * 2.1)))
    flame = (from_bow < tongue) & (hr > -0.05) & (hr < 1.25)
    core = from_bow < tongue * 0.55
    k = np.where(flame, np.where(core, 4, 3), k)
    return k


def paint_badger(H):
    x = H.m; hr = H.h; L = H.length
    k = np.where(hr < 0.38, 5, 3)                                     # grey back, black legs and belly
    # claw marks: three ragged white slashes amidships
    fb = max(H.sheer - H.wl, 0.2)                                                # freeboard, metres
    for i in range(3):
        cx = 0.42 * L + i * 0.9 * fb; ang = 0.75
        d = (x - cx) * np.cos(ang) - (hr * fb) * np.sin(ang)
        along = (x - cx) * np.sin(ang) + (hr * fb) * np.cos(ang)
        rag = 0.04 * fb * np.sin(along * 9 / fb + i)
        taper = (1 - np.abs(along - 0.55 * fb) / (0.75 * fb)).clip(0)
        k = np.where(np.abs(d + rag) < 0.13 * fb * taper, 1, k)
    # the face: white from 70 % forward, a black stripe from the snout back over the eye, widening into the body
    face = H.s > 0.70
    t = np.clip((H.s - 0.70) / 0.30, 0, 1)                            # 0 at the cheek .. 1 at the snout
    half = 0.22 - 0.15 * t
    stripe = np.abs(hr - (0.55 - 0.05 * t)) < half
    k = np.where(face, np.where(stripe, 5, 1), k)
    eye = np.hypot(x - 0.83 * L, (hr - 0.53) * (H.sheer - H.wl)) < 0.32
    pupil = np.hypot(x - 0.835 * L, (hr - 0.53) * (H.sheer - H.wl)) < 0.15
    k = np.where(eye, np.where(pupil, 5, 1), k)
    k = np.where(H.s > 0.975, 5, k)                                   # the nose
    k = np.where((hr > 1.15) & ~face, 5, k)                           # dark superstructure
    return k


def paint_rude(H):
    """The Big Johnson: a cartoon one along each side, head to the bow, black outline so it reads from the camera."""
    fb = max(H.sheer - H.wl, 0.2)
    x = H.m; y = H.h * fb                                             # metres along, metres above the waterline
    L = H.length
    cy = 0.60 * fb; r = 0.19 * fb                                    # shaft centre line and half height
    x0, x1 = 0.26 * L, 0.70 * L
    hx, hl, hr = x1 + 0.045 * L, 0.075 * L, 1.35 * r                  # the head: an oval wider than the shaft
    def shapes(grow):
        shaft = (x > x0) & (x < x1 + 0.01 * L) & (np.abs(y - cy) < r + grow)
        head = ((x - hx) / (hl + grow)) ** 2 + ((y - cy) / (hr + grow)) ** 2 < 1
        br = 1.3 * r
        balls = (np.hypot(x - (x0 - 0.005 * L), y - (cy - 0.55 * r)) < br + grow) | \
                (np.hypot(x - (x0 + 0.03 * L), y - (cy - 0.75 * r)) < br + grow)
        return shaft, head, balls
    shaft, head, balls = shapes(0.0)
    outline = np.any(shapes(0.07 * fb), 0) & ~(shaft | head | balls)
    rim = head & (x < hx - hl + 0.012 * L)                            # the ridge where the head meets the shaft
    split = balls & ~shaft & (np.abs(x - (x0 + 0.0125 * L)) < 0.004 * L)             # the line between the two
    slit = head & (np.abs(y - cy) < 0.03 * fb) & (x > hx + 0.45 * hl)
    k = np.full(H.s.shape, 1)
    k = np.where(shaft | balls, 3, k)
    k = np.where(head, 4, k)
    k = np.where(outline | rim | slit | split, 5, k)
    return k


def paint_pride(H):
    """Rainbow bands from the deck edge down, sweeping up toward the bow; violet runs on below the waterline (key 2,
    the Draft). Five keys only, so the flag's blue is left out."""
    hh = H.h - 0.35 * (H.s - 0.5)
    band = np.clip(np.floor((1.08 - hh) / 0.22), 0, 4).astype(int)    # 0 red at the top .. 4 violet at the waterline
    return np.choose(band, [1, 3, 4, 5, 2])


def paint_storm(H):
    """Stormy Night: night-blue hull, soft storm clouds, jagged lightning bolts (white core, pale blue glow) striking
    from the deck edge toward the waterline, with the odd fork."""
    fb = max(H.sheer - H.wl, 0.2)
    x = H.m; y = H.y - H.wl
    k = np.where(vnoise(x * 0.5, y * 1.4, 77, 1.3) > 0.58, 3, 1)       # clouds
    rng = np.random.default_rng(88)
    segs = []
    def bolt(px, py, length, depth):
        side = rng.choice([-1, 1]); drift = rng.uniform(-0.25, 0.25) * fb
        while py > length:                                            # hard zig-zags: each step swings the other way
            side = -side
            nx = px + side * rng.uniform(0.18, 0.42) * fb + drift; ny = py - rng.uniform(0.10, 0.20) * fb
            segs.append((px, py, nx, ny, 1.0 if depth == 0 else 0.6))
            if depth == 0 and rng.random() < 0.28:
                bolt(nx, ny, ny - fb * 0.4, 1)
            px, py = nx, ny
    for bx in np.arange(rng.uniform(1.5, 4), H.length - 1.5, max(5.5, H.length / 4.5)):
        bolt(bx, fb * 1.15, fb * 0.05, 0)
    core = np.zeros(x.shape, bool); glow = np.zeros(x.shape, bool)
    w = 0.03 * fb
    for x0, y0, x1, y1, scale in segs:
        dx, dy = x1 - x0, y1 - y0
        t = np.clip(((x - x0) * dx + (y - y0) * dy) / (dx * dx + dy * dy), 0, 1)
        d = np.hypot(x - (x0 + t * dx), y - (y0 + t * dy))
        core |= d < w * scale
        glow |= d < w * scale * 2.3
    k = np.where(glow, 4, k)
    return np.where(core, 5, k)


DRAFT_RED = (0.57, 0.11, 0.11)
SCHEMES = [
    dict(id='m22', name='Measure 22 Graded', global_=0, paint=paint_m22, deck=4, colours=[
        ('Haze Gray', 1, (0.53, 0.56, 0.58), 0), ('Navy Blue', 5, (0.20, 0.25, 0.32), 2),
        ('Draft', 2, DRAFT_RED, 4), ('Deck', 4, (0.24, 0.28, 0.32), 3)]),
    dict(id='m31', name='Measure 31 Jungle', global_=2, paint=paint_m31, deck=4, colours=[
        ('Light Green', 1, (0.45, 0.53, 0.38), 1), ('Navy Green', 3, (0.25, 0.33, 0.23), 0),
        ('Black', 5, (0.11, 0.12, 0.11), 2), ('Draft', 2, DRAFT_RED, 4), ('Deck', 4, (0.24, 0.29, 0.22), 3)]),
    dict(id='m32', name='Measure 32 Dazzle', global_=4, paint=paint_m32, deck=4, colours=[
        ('Light Gray', 1, (0.66, 0.68, 0.70), 1), ('Ocean Gray', 3, (0.40, 0.43, 0.46), 0),
        ('Dull Black', 5, (0.12, 0.12, 0.13), 2), ('Draft', 2, DRAFT_RED, 4), ('Deck', 4, (0.24, 0.28, 0.32), 3)]),
    dict(id='wa', name='Western Approaches', global_=1, paint=paint_wa, deck=5, colours=[
        ('White', 1, (0.92, 0.93, 0.93), 0), ('WA Green', 3, (0.62, 0.79, 0.70), 1),
        ('WA Blue', 4, (0.58, 0.74, 0.82), 2), ('Draft', 2, DRAFT_RED, 4), ('Deck', 5, (0.45, 0.47, 0.49), 3)]),
    dict(id='adm44', name='Admiralty 1944', global_=3, paint=paint_adm44, deck=5, colours=[
        ('Light Grey', 1, (0.80, 0.82, 0.82), 0), ('Panel', 4, (0.24, 0.31, 0.42), 2),
        ('Draft', 2, DRAFT_RED, 4), ('Deck', 5, (0.33, 0.35, 0.37), 3)]),
    dict(id='tiger', name='Flaming Tiger', global_=3, paint=paint_tiger, deck=0, colours=[
        ('Tiger Orange', 1, (0.93, 0.42, 0.06), 0), ('Stripes', 5, (0.07, 0.06, 0.05), 2),
        ('Flame', 3, (1.00, 0.82, 0.12), 1), ('Flame Core', 4, (0.86, 0.10, 0.03), 1), ('Draft', 2, (0.18, 0.05, 0.04), 4)]),
    dict(id='badger', name='Wild Badger', global_=1, paint=paint_badger, deck=5, colours=[
        ('Badger White', 1, (0.93, 0.92, 0.88), 1), ('Badger Grey', 3, (0.52, 0.51, 0.48), 0),
        ('Badger Black', 5, (0.08, 0.08, 0.08), 2), ('Draft', 2, (0.10, 0.09, 0.09), 4)]),
    dict(id='rude', name='Big Johnson', global_=0, paint=paint_rude, deck=0, colours=[
        ('Hull', 1, (0.55, 0.58, 0.60), 0), ('Skin', 3, (0.96, 0.69, 0.62), 1),
        ('Head', 4, (0.86, 0.42, 0.45), 1), ('Outline', 5, (0.10, 0.07, 0.07), 2), ('Draft', 2, DRAFT_RED, 4)]),
    dict(id='pride', name='Pride', global_=3, paint=paint_pride, deck=0, colours=[
        ('Red', 1, (0.89, 0.01, 0.01), 0), ('Orange', 3, (1.00, 0.55, 0.00), 1), ('Yellow', 4, (1.00, 0.93, 0.00), 1),
        ('Green', 5, (0.00, 0.50, 0.15), 2), ('Violet', 2, (0.46, 0.03, 0.53), 4)]),
    dict(id='storm', name='Stormy Night', global_=1, paint=paint_storm, deck=3, colours=[
        ('Night', 1, (0.06, 0.08, 0.14), 0), ('Storm Clouds', 3, (0.19, 0.22, 0.29), 2),
        ('Glow', 4, (0.55, 0.70, 0.98), 1), ('Lightning', 5, (0.97, 0.97, 1.00), 1), ('Draft', 2, (0.04, 0.04, 0.06), 4)]),
]


def mask(H, sc):
    k = sc['paint'](H)
    if sc['deck']:
        k = np.where(H.deck, sc['deck'], k)
    k = np.where(H.draft, 2, k)
    k = np.where(H.hit, k, 1)
    return KEYS[np.clip(k, 1, 5) - 1]


# ---------------------------------------------------------------- previews and icons
def preview(H, img, sc, size=(880, 260), side=1, bg=(222, 226, 231)):
    """Orthographic side view: every texel splatted at its 3D point, coloured with the scheme's colours."""
    cols = np.zeros((6, 3)); cols[:] = (0.5, 0.5, 0.5)
    for _, slot, rgb, _ in sc['colours']:
        cols[slot] = rgb
    key = (np.abs(img[:, :, None, :].astype(int) - KEYS[None, None].astype(int)).sum(-1)).argmin(-1) + 1
    sel = H.hit & (H.side == side)
    shade = 0.55 + 0.45 * np.clip(H.nrm[:, :, 1] * 0.6 + np.abs(H.nrm[:, :, 0] + H.nrm[:, :, 2]) * 0.5, 0, 1)
    rgb = cols[key[sel]] * shade[sel, None]
    X = H.m[sel] if side == 1 else H.length - H.m[sel]
    Y = H.y[sel]
    W, Hh = size
    sc_ = min((W - 20) / H.length, (Hh - 20) / (Y.max() - Y.min() + 1e-6))
    px = (10 + X * sc_).astype(int); py = (Hh - 10 - (Y - Y.min()) * sc_).astype(int)
    out = np.zeros((Hh, W, 3)); out[:] = np.array(bg) / 255
    lat = np.abs(H.pos[:, :, 0][sel])
    ok = (px >= 0) & (px < W) & (py >= 0) & (py < Hh)
    order = np.argsort(lat[ok])
    for i in np.nonzero(ok)[0][order]:
        out[py[i], px[i]] = rgb[i]
    return Image.fromarray((out * 255).astype(np.uint8))


def write_header():
    """src\\wm_hull_camo_data.h: the scheme and boat tables the mod reads, so names and colours have one source."""
    L = ['// GENERATED by tools\\hull_camo.py (write_header); edit the schemes there and run it again.',
         '// Order and names are the save format: the game saves the camo as a list index, colour edits by pattern name.',
         'struct CamoColour { const char* name; int slot; float r, g, b; int channel; };',
         'struct CamoScheme { const char* id; const char* name; int global; int ncols; CamoColour cols[5]; };',
         'static const CamoScheme kCamoSchemes[] = {']
    for sc in SCHEMES:
        cols = ', '.join('{ "%s", %d, %.2ff, %.2ff, %.2ff, %d }' % (n, s, *rgb, ch) for n, s, rgb, ch in sc['colours'])
        L.append('  { "%s", "%s", %d, %d, { %s } },' % (sc['id'], sc['name'], sc['global_'], len(sc['colours']), cols))
    L += ['};', 'struct CamoBoat { const char* boat; const char* guid8; };', 'static const CamoBoat kCamoBoats[] = {']
    L += ['  { "%s", "%s" },' % (b, c['guid']) for b, c in BOATS.items()]
    L += ['};', '']
    (ROOT / 'src' / 'wm_hull_camo_data.h').write_bytes('\r\n'.join(L).encode())


def main():
    write_header()
    if '--header' in sys.argv:
        return
    want_preview = '--preview' in sys.argv
    roles_by_guid = plain_roles()
    import UnityPy
    env = UnityPy.load(glob.glob(str(BUNDLES / 'shiphulls_assets_all_*.bundle'))[0])
    pid = {o.path_id: o for o in env.objects}
    g2n = {}
    for o in env.objects:
        if o.type.name == 'AssetBundle':
            for k, v in o.read().m_Container:
                g2n[k] = pid[v.asset.path_id].peek_name()
    roles = {g2n[g]: r for g, r in roles_by_guid.items() if g in g2n}
    models = ROOT / 'models'; prev = ROOT / 'out' / 'camo_preview'; prev.mkdir(parents=True, exist_ok=True)
    sheets = {sc['id']: [] for sc in SCHEMES}
    only = sys.argv[sys.argv.index('--boat') + 1] if '--boat' in sys.argv else None
    for boat in BOATS:
        if only and boat != only:
            continue
        H = Hull(boat, roles)
        for sc in SCHEMES:
            img = mask(H, sc)
            Image.fromarray(img).save(models / ('camo_%s_%s.png' % (boat, sc['id'])), optimize=True)
            if want_preview or boat == 'PT-109':
                sv = preview(H, img, sc)
                sheets[sc['id']].append(sv)
                if boat == 'PT-109':                                  # picker icon, the size of the game's own
                    ic = sv.copy(); ic.thumbnail((220, 110))
                    canvas = Image.new('RGB', (220, 110), (222, 226, 231)); canvas.paste(ic, ((220 - ic.width) // 2, (110 - ic.height) // 2))
                    canvas.save(models / ('camo_icon_%s.png' % sc['id']))
    if want_preview:
        for sid, ims in sheets.items():
            sheet = Image.new('RGB', (ims[0].width, sum(i.height for i in ims)))
            y = 0
            for i in ims:
                sheet.paste(i, (0, y)); y += i.height
            sheet.save(prev / ('%s.png' % sid))
    print('wrote %d masks and %d icons to %s' % (len(BOATS) * len(SCHEMES), len(SCHEMES), models))


if __name__ == '__main__':
    main()
