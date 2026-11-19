"""Kill markings for the player boats (wm_kill_marks.h): where each tally mark sits on each hull, and the mark pictures.

WWII-style tallies on the hull side: a row of plane silhouettes (planes shot down), one of ship silhouettes (ships sunk)
and one of gun silhouettes (shore guns destroyed). Each mark is a small quad the mod parents to the boat's root, so this
tool works out, per boat, a grid of mark spots on the starboard hull surface (the mod mirrors them to port):

    python tools\\kill_marks.py     writes src\\wm_kill_marks_data.h and models\\killmark_*.png

Spots are found by casting rays at the hull mesh (LOD0) from outboard, at fixed heights below the deck edge, from just
forward of amidships going aft. Needs UnityPy, numpy and Pillow (run without python -I).
"""
import pathlib
import sys

import numpy as np
from PIL import Image, ImageDraw, ImageFilter

sys.path.insert(0, str(pathlib.Path(__file__).parent))
import hull_camo as hc  # noqa: E402

ROOT = pathlib.Path(__file__).resolve().parent.parent
ROWS, COLS = 3, 12                    # planes, ships, shore guns; marks per row (a big mark stands for ten)
# waterline / deck edge in the hull node's frame, as tools\hull_camo.py measures them (its log line per boat)
LINES = {'PT-6': (-0.76, 1.07), 'PT-20': (-0.60, 0.67), 'PT-95': (-0.05, 1.93), 'PT-109': (-0.94, 1.07),
         'PT-658': (-0.04, 1.89), 'PC-461': (0.24, 2.29)}
START = {'PC-461': 0.50}              # first column, as a fraction of the length from the stern (others 0.60)


def hull(boat):
    """Hull triangles in the boat ROOT frame (metres, +Y up, +Z bow, +X starboard), the hull node's offset, and its
    root-from-node 4x4 (the mod hangs the marks under the hull node, so corners go back into node coordinates)."""
    root = boat + ' Player'
    name = hc.BOATS[boat]['meshes'][0]
    M = hc._boat_frame(name, root)
    V, N, UV, T = hc._mesh(name, root, np.zeros(3))      # rotation and the node's position: full root frame
    return V[T], M[:3, 3], M


def quads(p, n, size, Minv):
    """Both sides' quads for one spot: 2 x 4 corners (node coordinates) in front-facing order, with their UVs. The
    picture's right edge points to the bow on both sides, so ships sail forward; corners sit 12 mm off the hull."""
    out = []
    for side in (1, -1):
        q = p * [side, 1, 1]; nn = n * [side, 1, 1]
        t = np.array([0, 0, 1.0]); t -= t.dot(nn) * nn; t /= np.linalg.norm(t)
        u = np.array([0, 1.0, 0]); u -= u.dot(nn) * nn + u.dot(t) * t; u /= np.linalg.norm(u)
        c = q + nn * 0.012
        uv = [(0, 0), (1, 0), (1, 1), (0, 1)]
        pts = [c + (a - 0.5) * size * t + (b - 0.5) * size * u for a, b in uv]
        if np.cross(pts[1] - pts[0], pts[2] - pts[0]).dot(nn) < 0:    # Unity draws the face whose corners wind
            pts, uv = pts[::-1], uv[::-1]                             # clockwise to the viewer: cross(e1, e2) faces it
        out.append([tuple((Minv @ np.append(v, 1))[:3]) + w for v, w in zip(pts, uv)])
    return out


def cast(tris, y, z):
    """Outermost starboard hit of the ray x = +inf -> -inf at (y, z): (x, unit normal) or None."""
    a, b, c = tris[:, 0], tris[:, 1], tris[:, 2]
    e1, e2 = b - a, c - a
    d = np.array([-1.0, 0, 0])
    o = np.array([100.0, y, z])
    p = np.cross(d, e2)
    det = (e1 * p).sum(1)
    ok = np.abs(det) > 1e-12
    inv = np.where(ok, 1 / np.where(ok, det, 1), 0)
    s = o - a
    u = (s * p).sum(1) * inv
    q = np.cross(s, e1)
    v = (q * d).sum(1) * inv
    t = (e2 * q).sum(1) * inv
    hit = ok & (u >= 0) & (v >= 0) & (u + v <= 1) & (t > 0)
    if not hit.any():
        return None
    i = np.where(hit)[0][np.argmin(t[hit])]
    n = np.cross(e1[i], e2[i]); n /= np.linalg.norm(n)
    if n[0] < 0:
        n = -n
    return 100.0 - t[i], n


def slots(boat):
    tris, off, M = hull(boat)
    Minv = np.linalg.inv(M)
    wl, sheer = (v + off[1] for v in LINES[boat])
    fb = sheer - wl
    zs = tris[:, :, 2]; lo, hi = zs.min(), zs.max(); length = hi - lo
    size = float(np.clip(0.15 * fb, 0.18, 0.36))           # mark edge, metres
    pitch = size * 1.15
    z0 = lo + START.get(boat, 0.60) * length
    out = []
    for r in range(ROWS):
        y = sheer - (0.30 + 1.2 * r) * size - size / 2       # rows hang down from just under the deck edge
        for c in range(COLS):
            z = z0 - c * pitch
            h = cast(tris, y, z)
            if h is None:
                out.append(None); continue
            x, n = h
            out.append(quads(np.array([x, y, z]), n, size, Minv))
    print('%s: freeboard %.2f m, marks %.2f m, %d of %d spots on the hull' % (boat, fb, size, sum(o is not None for o in out), len(out)))
    return out


# ---------------------------------------------------------------- mark pictures (white-edged dark silhouettes)
def _stamp(draw_fn, gold=False, res=256):
    big = Image.new('L', (res * 2, res * 2), 0)
    draw_fn(ImageDraw.Draw(big), res * 2)
    shape = big.resize((res, res), Image.LANCZOS)
    edge = shape.filter(ImageFilter.MaxFilter(15))            # a white rim so the mark reads on any camo
    fill = (214, 170, 40) if gold else (28, 30, 32)
    img = Image.new('RGBA', (res, res), (0, 0, 0, 0))
    img.paste(Image.new('RGBA', (res, res), (240, 240, 236, 255)), (0, 0), edge)
    img.paste(Image.new('RGBA', (res, res), fill + (255,)), (0, 0), shape)
    return img


def plane(d, s):                                              # top view, nose up
    k = s / 512
    P = lambda pts: [(x * k, y * k) for x, y in pts]
    d.ellipse(P([(226, 40), (286, 470)]), fill=255)                                       # fuselage
    d.polygon(P([(30, 210), (482, 210), (482, 250), (300, 285), (212, 285), (30, 250)]), fill=255)   # wings
    d.polygon(P([(160, 420), (352, 420), (352, 450), (280, 465), (232, 465), (160, 450)]), fill=255)  # tailplane


def ship(d, s):                                               # side view, bow right
    k = s / 512
    P = lambda pts: [(x * k, y * k) for x, y in pts]
    d.polygon(P([(20, 300), (492, 300), (440, 380), (60, 380)]), fill=255)              # hull
    d.rectangle(P([(170, 240), (330, 300)]), fill=255)                                   # superstructure
    d.rectangle(P([(220, 180), (285, 240)]), fill=255)                                   # bridge
    d.rectangle(P([(300, 200), (330, 240)]), fill=255)                                   # funnel
    d.rectangle(P([(248, 90), (256, 180)]), fill=255)                                    # mast
    d.rectangle(P([(380, 270), (440, 300)]), fill=255); d.rectangle(P([(400, 262), (490, 272)]), fill=255)  # fore gun
    d.rectangle(P([(80, 270), (140, 300)]), fill=255); d.rectangle(P([(30, 262), (110, 272)]), fill=255)    # aft gun


def gun(d, s):                                                # shore gun: barrel over a sandbagged emplacement
    k = s / 512
    P = lambda pts: [(x * k, y * k) for x, y in pts]
    d.polygon(P([(40, 400), (472, 400), (420, 300), (92, 300)]), fill=255)              # emplacement
    d.ellipse(P([(170, 220), (330, 330)]), fill=255)                                     # gun body
    d.polygon(P([(280, 250), (470, 140), (486, 166), (300, 285)]), fill=255)             # barrel


def write_header(boats):
    lines = ['// Generated by tools\\kill_marks.py: kill mark quads per player boat, in the hull node\'s own coordinates (the',
             '// marks hang under that node). Spot = row * kKillCols + col: rows 0 planes, 1 ships, 2 shore guns; col 0 is',
             '// forward-most. Each spot: starboard then port, 4 corners (x, y, z, u, v) in front-facing order; ok 0 = no hull.',
             'struct KillMarkCorner { float x, y, z, u, v; };',
             'struct KillMarkSpot { int ok; KillMarkCorner c[2][4]; };',
             'static const int kKillRows = %d, kKillCols = %d;' % (ROWS, COLS),
             'struct KillMarkBoat { const char* boat; const char* node; KillMarkSpot spot[%d]; };' % (ROWS * COLS),
             'static const KillMarkBoat kKillBoats[] = {']
    f = lambda v: '%.4ff' % v
    for b, sp in boats:
        lines.append('  { "%s", "%s", {' % (b, hc.BOATS[b]['meshes'][0]))
        for s in sp:
            if s is None:
                lines.append('    { 0, {} },'); continue
            sides = ', '.join('{ %s }' % ', '.join('{ %s }' % ', '.join(f(x) for x in c) for c in side) for side in s)
            lines.append('    { 1, { %s } },' % sides)
        lines.append('  } },')
    lines += ['};', 'static const int kKillBoatCount = %d;' % len(boats), '']
    (ROOT / 'src' / 'wm_kill_marks_data.h').write_bytes('\r\n'.join(lines).encode())


def main():
    models = ROOT / 'models'
    atlas = Image.new('RGBA', (768, 512), (0, 0, 0, 0))                 # columns plane, ship, gun; top row gold (tens)
    for i, fn in enumerate((plane, ship, gun)):
        atlas.paste(_stamp(fn, gold=True), (i * 256, 0))
        atlas.paste(_stamp(fn), (i * 256, 256))
    atlas.save(models / 'killmarks.png')
    write_header([(b, slots(b)) for b in hc.BOATS])
    print('wrote src\\wm_kill_marks_data.h and models\\killmarks.png')


if __name__ == '__main__':
    main()
