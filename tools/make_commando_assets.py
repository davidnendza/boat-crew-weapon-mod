"""Builds the Sneaking Eel torpedo's commando (OBJ + MTL) for the weapon mod, plus a preview PNG.

The Sneaking Eel Unique (fx crawl=1) swaps a torpedo's look for a commando crawling flat on the water: green fatigues,
a red headband with its tails trailing, a pack and a slung rifle. He is a generic soldier, not any game's character.
Each moving piece is its own .obj with its pivot at the origin, so the mod only has to turn it there every frame
(TorpCrawlPose in weaponmod.cpp, whose kCrawlPivot table must match PIVOTS below; this script prints it):
  commando_body.obj     torso, head, pack and rifle (pivot: the belly on the water line, under the chest)
  commando_arm_l/_r     an arm from the shoulder, elbow out and forearm forward on the water
  commando_leg_l/_r     a leg from the hip, straight back
  commando_tails.obj    the headband's two tails from the knot at the back of the head
Units are metres (a man at SCALE times life size), +Y up, +Z forward; the game's OBJ loader reads them as written.

Usage: python make_commando_assets.py <models folder> [--preview <png>]
"""
import math
import pathlib
import sys

import numpy as np

from make_unusual_models import Part, box, cyl, ellipsoid, ribbon, rot, sphere, torus, unit

SCALE = 2.2                                                # a 1.75 m man lying down is 1.75 m long; a torpedo is about 7 m
MATS = {'Fatigue': (0.32, 0.38, 0.2), 'FatigueDark': (0.24, 0.29, 0.15), 'Skin': (0.86, 0.64, 0.48), 'Hair': (0.12, 0.09, 0.07),
        'Band': (0.82, 0.08, 0.1), 'Pack': (0.52, 0.47, 0.32), 'Belt': (0.26, 0.18, 0.1), 'Boot': (0.07, 0.07, 0.08),
        'Gunmetal': (0.2, 0.22, 0.25), 'Wood': (0.5, 0.32, 0.16), 'Eye': (0.04, 0.04, 0.05)}
# pivots in the body frame at life size (scaled on writing); kCrawlPivot in weaponmod.cpp holds them times SCALE
PIVOTS = {'commando_body': (0, 0, 0), 'commando_arm_l': (-0.2, 0.15, 0.4), 'commando_arm_r': (0.2, 0.15, 0.4),
          'commando_leg_l': (-0.1, 0.1, -0.3), 'commando_leg_r': (0.1, 0.1, -0.3), 'commando_tails': (0, 0.33, 0.49)}
I4 = np.eye(4)


def body():
    p = Part('body', I4, [I4])
    ellipsoid(p, 'Fatigue', (0, 0.13, 0.15), (0.19, 0.12, 0.32), rings=10, seg=16)            # chest and back
    ellipsoid(p, 'Fatigue', (0, 0.13, -0.2), (0.17, 0.115, 0.17), rings=8, seg=14)             # seat
    cyl(p, 'Belt', (0, 0.13, -0.1), (0, 0.13, -0.04), 0.175, seg=16)                            # belt round the waist
    box(p, 'Gunmetal', (0, 0.13, -0.035), (0.06, 0.05, 0.03))                                 # buckle (under him)
    for s in (-1, 1):
        sphere(p, 'Fatigue', (0.19 * s, 0.15, 0.4), 0.075, rings=6, seg=10)                   # shoulders
        sphere(p, 'Fatigue', (0.1 * s, 0.1, -0.3), 0.08, rings=6, seg=10)                     # hips
    cyl(p, 'Skin', (0, 0.16, 0.42), (0, 0.24, 0.53), 0.055, seg=12)                            # neck, head raised
    ellipsoid(p, 'Skin', (0, 0.28, 0.6), (0.1, 0.115, 0.115), rings=10, seg=16)                # head
    ellipsoid(p, 'Hair', (0, 0.315, 0.585), (0.104, 0.088, 0.112), rings=8, seg=14)            # hair, cropped
    ellipsoid(p, 'Skin', (0, 0.255, 0.712), (0.022, 0.03, 0.025), rings=4, seg=6)              # nose
    for s in (-1, 1):
        sphere(p, 'Eye', (0.037 * s, 0.29, 0.705), 0.013, rings=4, seg=6)
        box(p, 'Hair', (0.037 * s, 0.312, 0.703), (0.04, 0.008, 0.01), r=(0, 0, -8 * s))       # brows, a scowl
    torus(p, 'Band', (0, 0.33, 0.6), 0.108, 0.018, r=(-12, 0, 0), seg=24, tube=6)              # headband, tipped up in front
    sphere(p, 'Band', PIVOTS['commando_tails'], 0.026, rings=5, seg=8)                         # its knot
    box(p, 'Pack', (0, 0.28, 0.12), (0.28, 0.12, 0.3))                                         # pack on his back
    box(p, 'Pack', (0, 0.35, 0.12), (0.24, 0.03, 0.26))
    for s in (-1, 1):
        box(p, 'Belt', (0.11 * s, 0.25, 0.3), (0.035, 0.03, 0.14), r=(20, 0, 0))               # pack straps
    cyl(p, 'Gunmetal', (-0.17, 0.39, -0.22), (0.13, 0.39, 0.5), 0.018, seg=8)                 # rifle slung across the pack
    box(p, 'Wood', (-0.19, 0.39, -0.26), (0.06, 0.08, 0.2), r=(0, 23, 0))
    box(p, 'Wood', (0.02, 0.39, 0.24), (0.05, 0.05, 0.22), r=(0, 23, 0))
    box(p, 'Gunmetal', (-0.06, 0.40, 0.03), (0.04, 0.09, 0.05), r=(0, 23, 0))                 # magazine
    return p


def arm(s):
    p = Part('arm', I4, [I4])
    sh = np.array(PIVOTS['commando_arm_r']) * [s, 1, 1]
    el, hd = np.array((0.27 * s, 0.03, 0.47)) - sh, np.array((0.12 * s, 0.03, 0.82)) - sh   # elbow out, hand ahead
    o = np.zeros(3)
    cyl(p, 'Fatigue', o, el, 0.06, 0.052, seg=10)
    sphere(p, 'FatigueDark', el, 0.054, rings=5, seg=8)                                        # elbow
    cyl(p, 'Fatigue', el, el + (hd - el) * 0.8, 0.05, 0.044, seg=10)                          # sleeve
    cyl(p, 'Skin', el + (hd - el) * 0.8, hd, 0.036, 0.034, seg=8)                            # wrist
    ellipsoid(p, 'Skin', hd + unit(hd - el) * 0.04, (0.045, 0.03, 0.06), r=(0, math.degrees(math.atan2(*(hd - el)[[0, 2]])), 0), rings=5, seg=8)
    return p


def leg(s):
    p = Part('leg', I4, [I4])
    hip = np.array(PIVOTS['commando_leg_r']) * [s, 1, 1]
    kn, an = np.array((0.14 * s, 0.07, -0.72)) - hip, np.array((0.16 * s, 0.06, -1.1)) - hip   # nearly straight back
    o = np.zeros(3)
    cyl(p, 'Fatigue', o, kn, 0.075, 0.06, seg=10)
    sphere(p, 'FatigueDark', kn, 0.062, rings=5, seg=8)                                        # knee
    cyl(p, 'Fatigue', kn, an, 0.058, 0.048, seg=10)
    box(p, 'Boot', an + (0, 0.02, -0.04), (0.1, 0.13, 0.12))                                  # boot, sole up (he lies face down)
    box(p, 'Boot', an + (0, 0.09, -0.07), (0.09, 0.04, 0.17))
    return p


def tails():
    p = Part('tails', I4, [I4])
    for s in (-1, 1):
        ribbon(p, 'Band', [(0, 0, 0), (0.05 * s, 0.03, -0.16), (0.09 * s, 0.08, -0.34), (0.11 * s, 0.1, -0.46)],   # up over the pack
               [0.05, 0.045, 0.04, 0.03], 0.012)
    return p


def models():
    out = {'commando_body': [body()], 'commando_tails': [tails()]}
    for s, n in ((-1, 'l'), (1, 'r')):
        out['commando_arm_' + n] = [arm(s)]
        out['commando_leg_' + n] = [leg(s)]
    return out


def write(name, parts, folder):
    folder = pathlib.Path(folder); folder.mkdir(parents=True, exist_ok=True)
    used = sorted({m for p in parts for m, _, _ in p.tris})
    lines = ['# %s: built by tools/make_commando_assets.py (pivot at the origin, +Z forward)' % name, 'mtllib %s.mtl' % name]
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


def pose(name, ph):
    """The same crawl as TorpCrawlPose (rotation matrix in the body frame, life-size pivot), for the preview."""
    a = math.radians
    if name.startswith('commando_arm'):
        s = -1 if name.endswith('_l') else 1
        f = math.sin(ph + (0 if s < 0 else math.pi))
        return rot(0, -s * 28 * f, s * 22 * max(0.0, -math.cos(ph + (0 if s < 0 else math.pi))))
    if name.startswith('commando_leg'):
        s = -1 if name.endswith('_l') else 1
        w = 0.5 + 0.5 * math.sin(ph + (math.pi if s < 0 else 0))
        return rot(0, -s * 30 * w, 0)
    if name == 'commando_tails':
        return rot(6 * math.sin(ph * 5.3), 14 * math.sin(ph * 3.7), 0)
    return np.eye(3)


def preview(ms, path):
    from PIL import Image, ImageDraw
    views = [('crawl, right arm out', 0.5 * math.pi, unit([0.7, 0.8, 0.9])), ('crawl, left arm out', 1.5 * math.pi, unit([-0.7, 0.8, 0.9])),
             ('side', 0.0, unit([1, 0.15, 0.0])), ('from above', 0.5 * math.pi, unit([0.0, 1.0, 0.02]))]
    W, H, sc = 340, 300, 58
    img = Image.new('RGB', (W * len(views), H), (120, 170, 210)); dr = ImageDraw.Draw(img)
    light = unit([0.4, 0.85, 0.6])
    for vi, (label, ph, eye) in enumerate(views):
        fwd = -eye; right = unit(np.cross([0, 1, 0], fwd) if abs(fwd[1]) < 0.99 else [1, 0, 0]); upv = np.cross(fwd, right); Vt = np.array([right, upv, fwd])
        items = []
        for name, parts in ms.items():
            M = pose(name, ph); o = np.array(PIVOTS[name]) * SCALE
            for p in parts:
                for mat, ps, ns in p.tris:
                    q = [Vt @ (o + M @ (x * SCALE)) for x in ps]; n = M @ unit(sum(ns))
                    if np.dot(Vt @ n, [0, 0, 1]) > 0.05: continue
                    shade = 0.45 + 0.55 * max(0.0, float(np.dot(n, light)))
                    items.append((sum(x[2] for x in q) / 3, [(W * vi + W / 2 + x[0] * sc, H * 0.6 - x[1] * sc) for x in q],
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
    print('kCrawlPivot (x SCALE):')
    for name in ('commando_body', 'commando_arm_l', 'commando_arm_r', 'commando_leg_l', 'commando_leg_r', 'commando_tails'):
        print('  { %.3ff, %.3ff, %.3ff },   // %s' % (tuple(v * SCALE for v in PIVOTS[name]) + (name,)))
    if '--preview' in sys.argv:
        preview(ms, sys.argv[sys.argv.index('--preview') + 1])


if __name__ == '__main__':
    main()
