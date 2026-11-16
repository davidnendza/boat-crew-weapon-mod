"""Builds the Flying Fish torpedo's fish (OBJ + MTL) for the weapon mod, plus a preview PNG.

The Flying Fish Unique (fx skim=1) skips across the surface; with this look it is a flying fish: a slim two-tone body
(dark blue back, silver belly), big pectoral fins that spread like wings while it skims and fold along its sides in
the tube, small pelvic fins, and a forked tail whose lower lobe is the longer, beating against the water.
Each moving piece is its own .obj with its pivot at the origin, so the mod only has to turn it there every frame
(TorpFishPart in weaponmod.cpp, whose kFishPivot table must match PIVOTS below; this script prints it):
  flyfish_body.obj      body, head, eyes, dorsal, pelvic and anal fins (pivot: the middle of the body, on its axis)
  flyfish_wing_l/_r     a pectoral fin from its root behind the gills, drawn spread
  flyfish_tail.obj      the forked tail from the base of the tail
Units are metres (a 0.43 m fish at SCALE times life size), +Y up, +Z forward; the game's OBJ loader reads them as written.

Usage: python make_flyfish_assets.py <models folder> [--preview <png>]
"""
import math
import pathlib
import sys

import numpy as np

from make_unusual_models import Part, cyl, ellipsoid, prism, ribbon, rot, sphere, unit

SCALE = 10.0                                              # a 0.43 m fish (nose to tail tip) becomes 4.3 m, a torpedo's length or so
MATS = {'Back': (0.1, 0.22, 0.48), 'Belly': (0.82, 0.84, 0.88), 'Fin': (0.5, 0.66, 0.82), 'Ray': (0.2, 0.32, 0.55),
        'Eye': (0.03, 0.03, 0.04), 'EyeRing': (0.9, 0.86, 0.6)}
# pivots in the fish frame at life size (scaled on writing); kFishPivot in weaponmod.cpp holds them times SCALE
PIVOTS = {'flyfish_body': (0, 0, 0), 'flyfish_wing_l': (-0.022, 0.008, 0.075), 'flyfish_wing_r': (0.022, 0.008, 0.075),
          'flyfish_tail': (0, 0, -0.155)}
FOLD, DIHEDRAL = 45.0, 8.0                                 # degrees: a wing folded back along the side; spread, tipped up
I4 = np.eye(4)
# a right pectoral fin, from its root (x along the span, z forward), in the plane of the fin
WING = [(0.0, 0.018), (0.05, 0.028), (0.12, 0.02), (0.19, -0.02), (0.22, -0.06), (0.2, -0.095), (0.13, -0.085),
        (0.06, -0.05), (0.0, -0.012)]


def body():
    p = Part('body', I4, [I4])
    ellipsoid(p, 'Back', (0, 0.004, 0.0), (0.026, 0.029, 0.165), rings=12, seg=18)            # back, dark
    ellipsoid(p, 'Belly', (0, -0.005, 0.002), (0.0265, 0.027, 0.163), rings=12, seg=18)        # belly, silver
    ellipsoid(p, 'Belly', (0, -0.013, 0.14), (0.013, 0.008, 0.03), rings=6, seg=10)            # lower jaw
    for s in (-1, 1):
        sphere(p, 'EyeRing', (0.02 * s, 0.008, 0.118), 0.0105, rings=6, seg=10)                # big eyes
        sphere(p, 'Eye', (0.023 * s, 0.008, 0.12), 0.0075, rings=5, seg=8)
        prism(p, 'Fin', [(x * s, z) for x, z in ((0.0, 0.012), (0.05, -0.01), (0.055, -0.035), (0.0, -0.02))], 0.003,
              c=(0.012 * s, -0.022, -0.04), r=(0, 0, -25 * s))                                  # pelvic fins, angled down and out
    prism(p, 'Fin', [(0.0, 0.0), (0.032, -0.012), (0.03, -0.045), (0.0, -0.04)], 0.003,         # dorsal fin, low and far back
          c=(0, 0.027, -0.075), r=(0, 0, 90))
    prism(p, 'Fin', [(0.0, 0.0), (-0.026, -0.012), (-0.024, -0.04), (0.0, -0.036)], 0.003,      # anal fin under it
          c=(0, -0.024, -0.08), r=(0, 0, 90))
    cyl(p, 'Back', (0, 0.002, -0.12), (0, 0.001, -0.162), 0.016, 0.009, seg=12)                # the tail's stem
    return p


def wing(s):
    p = Part('wing', I4, [I4])
    out = [(x * s, z) for x, z in WING]
    prism(p, 'Fin', out, 0.0025, c=(0, 0, 0))
    for tip in ((0.19, -0.02), (0.215, -0.06), (0.2, -0.09), (0.12, -0.08)):                   # rays from the root
        ribbon(p, 'Ray', [(0.004 * s, 0.0016, -0.0), (tip[0] * 0.55 * s, 0.0016, tip[1] * 0.55), (tip[0] * 0.95 * s, 0.0016, tip[1] * 0.95)],
               [0.004, 0.003, 0.002], 0.0008)
    return p


def tail():
    p = Part('tail', I4, [I4])
    # forked, standing upright (outline x is up after the turn): the lower lobe the longer
    prism(p, 'Fin', [(0.008, 0.004), (0.06, -0.07), (0.045, -0.085), (0.004, -0.03), (-0.004, -0.03), (-0.075, -0.105),
                     (-0.088, -0.09), (-0.008, 0.004)], 0.003, c=(0, 0, 0), r=(0, 0, 90))
    return p


def models():
    return {'flyfish_body': [body()], 'flyfish_wing_l': [wing(-1)], 'flyfish_wing_r': [wing(1)], 'flyfish_tail': [tail()]}


def write(name, parts, folder):
    folder = pathlib.Path(folder); folder.mkdir(parents=True, exist_ok=True)
    used = sorted({m for p in parts for m, _, _ in p.tris})
    lines = ['# %s: built by tools/make_flyfish_assets.py (pivot at the origin, +Z forward)' % name, 'mtllib %s.mtl' % name]
    vidx, nidx, vl, nl, body_ = {}, {}, [], [], []
    for p in parts:
        body_.append('o %s' % p.name)
        for m in used:
            tris = [t for t in p.tris if t[0] == m]
            if not tris:
                continue
            body_.append('usemtl %s' % m)
            for _, ps, ns in tris:
                ids = []
                for q, n in zip(ps, ns):
                    q = tuple(round(float(x) * SCALE, 4) + 0.0 for x in q); n = tuple(round(float(x), 3) + 0.0 for x in n)
                    if q not in vidx: vidx[q] = len(vidx) + 1; vl.append('v %.4f %.4f %.4f' % q)
                    if n not in nidx: nidx[n] = len(nidx) + 1; nl.append('vn %.3f %.3f %.3f' % n)
                    ids.append('%d//%d' % (vidx[q], nidx[n]))
                body_.append('f ' + ' '.join(ids))
    (folder / ('%s.obj' % name)).write_text('\n'.join(lines + vl + nl + body_) + '\n')
    (folder / ('%s.mtl' % name)).write_text(''.join('newmtl %s\nKd %.3f %.3f %.3f\n\n' % ((m,) + MATS[m]) for m in used))
    return sum(len(p.tris) for p in parts)


def pose(name, spread, tail_a):
    """The same turns as TorpFishPart (rotation matrix in the fish frame), for the preview."""
    if name.startswith('flyfish_wing'):
        s = -1 if name.endswith('_l') else 1
        return rot(0, s * FOLD * (1 - spread), s * DIHEDRAL * spread)
    if name == 'flyfish_tail':
        return rot(0, tail_a, 0)
    return np.eye(3)


def preview(ms, path):
    from PIL import Image, ImageDraw
    views = [('skimming, wings out', 1.0, 15, unit([0.8, 0.7, 0.6])), ('from above', 1.0, -20, unit([0.0, 1.0, 0.02])),
             ('side', 1.0, 0, unit([1, 0.12, 0.0])), ('in the tube, folded', 0.0, 0, unit([0.7, 0.9, -0.5]))]
    W, H, sc = 340, 300, 62
    img = Image.new('RGB', (W * len(views), H), (120, 170, 210)); dr = ImageDraw.Draw(img)
    light = unit([0.4, 0.85, 0.6])
    for vi, (label, spread, ta, eye) in enumerate(views):
        fwd = -eye; right = unit(np.cross([0, 1, 0], fwd) if abs(fwd[1]) < 0.99 else [1, 0, 0]); upv = np.cross(fwd, right); Vt = np.array([right, upv, fwd])
        items = []
        for name, parts in ms.items():
            M = pose(name, spread, ta); o = np.array(PIVOTS[name]) * SCALE
            for p in parts:
                for mat, ps, ns in p.tris:
                    q = [Vt @ (o + M @ (x * SCALE)) for x in ps]; n = M @ unit(sum(ns))
                    if np.dot(Vt @ n, [0, 0, 1]) > 0.05: continue
                    shade = 0.45 + 0.55 * max(0.0, float(np.dot(n, light)))
                    items.append((sum(x[2] for x in q) / 3, [(W * vi + W / 2 + x[0] * sc, H * 0.55 - x[1] * sc) for x in q],
                                  tuple(int(255 * min(1, c * shade)) for c in MATS[mat])))
        for _, poly, col in sorted(items, key=lambda it: -it[0]):
            dr.polygon(poly, fill=col)
        dr.text((W * vi + 6, 6), label, fill=(10, 10, 10))
    img.save(path)


def main():
    mdir = pathlib.Path(sys.argv[1])
    ms = models()
    for name, parts in ms.items():
        print('%-16s %5d triangles' % (name, write(name, parts, mdir)))
    print('kFishPivot (x SCALE):')
    for name in ('flyfish_body', 'flyfish_wing_l', 'flyfish_wing_r', 'flyfish_tail'):
        print('  { %.3ff, %.3ff, %.3ff },   // %s' % (tuple(v * SCALE for v in PIVOTS[name]) + (name,)))
    if '--preview' in sys.argv:
        preview(ms, sys.argv[sys.argv.index('--preview') + 1])


if __name__ == '__main__':
    main()
