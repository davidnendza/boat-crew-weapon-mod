"""Builds the chomper torpedoes' models (OBJ + MTL) and sounds (WAV) for the weapon mod, plus a preview PNG.

The Chomper and Chompette Uniques (fx chomp=1 / 2) swap a torpedo's look for a yellow ball that rides on the water and
opens and closes its mouth. Each jaw is its own .obj with the hinge at the origin and the mouth toward +Z, so the mod
only has to turn each one about its X axis every frame (TorpLookTick in weaponmod.cpp):
  chomper_upper.obj / chomper_lower.obj    the Chomper (eyes)
  chompette_upper.obj                      the Chompette's upper jaw (eyes, lashes and a bow); her lower jaw is the Chomper's
Units are metres, +Y up, +Z forward; the game's OBJ loader reads them as written (see make_unusual_models.py).

Sounds, made from scratch here (16-bit mono WAV):
  chomp.wav   a loop of two soft 'nom' bites, 2/3 s long: one bite each time the mouth shuts (3 times a second)
  gulp.wav    a swallow and a low thump, played where a chomper hits

Usage: python make_chomper_assets.py <models folder> <sounds folder> [--preview <png>]
"""
import math
import pathlib
import sys
import wave

import numpy as np

from make_unusual_models import Part, ellipsoid, unit

R = 1.3                                                    # the ball's radius (m); torpedoes run 0.4-0.6 m wide
MATS = {'Chomp': (1.0, 0.84, 0.08), 'Mouth': (0.22, 0.03, 0.05), 'Eye': (0.97, 0.97, 0.97), 'Pupil': (0.04, 0.04, 0.06),
        'Bow': (0.92, 0.12, 0.32), 'Lash': (0.05, 0.05, 0.07)}
RINGS, SEG = 12, 36


def dome(p, mat, r, up=True):
    """Half a sphere above (up) or below the XZ plane, outside faces."""
    s = 1 if up else -1

    def pt(i, j):
        th, ph = 0.5 * math.pi * i / RINGS, 2 * math.pi * j / SEG
        q = np.array([math.sin(th) * math.cos(ph), s * math.cos(th), math.sin(th) * math.sin(ph)])
        return q * r, q
    for i in range(RINGS):
        for j in range(SEG):
            (a, na), (b, nb), (c, nc), (d, nd) = pt(i, j), pt(i, j + 1), pt(i + 1, j + 1), pt(i + 1, j)
            p.tri(mat, (a, b, c), (na, nb, nc))
            p.tri(mat, (a, c, d), (na, nc, nd))


def disc(p, mat, r, facing_up):
    """The mouth's inside: a disc in the XZ plane, seen from above (tongue) or below (palate)."""
    n = np.array([0.0, 1.0 if facing_up else -1.0, 0.0])
    for j in range(SEG):
        a0, a1 = 2 * math.pi * j / SEG, 2 * math.pi * (j + 1) / SEG
        p.tri(mat, ((0, 0, 0), (r * math.cos(a0), 0, r * math.sin(a0)), (r * math.cos(a1), 0, r * math.sin(a1))), (n, n, n))


def on_ball(r, yaw, pitch):
    """A point on the ball: yaw from +Z toward +X, pitch up from the equator (degrees)."""
    y, p = math.radians(yaw), math.radians(pitch)
    return np.array([math.cos(p) * math.sin(y), math.sin(p), math.cos(p) * math.cos(y)]) * r


def eyes(p, lashes=False):
    for side in (-1, 1):
        c = on_ball(R * 0.97, side * 34, 38)
        n = unit(c)
        ellipsoid(p, 'Eye', c, (0.2, 0.26, 0.2), rings=8, seg=14)
        ellipsoid(p, 'Pupil', c + n * 0.15 + np.array([0, -0.02, 0.03]), (0.1, 0.13, 0.08), rings=6, seg=10)
        if lashes:                                         # three small strokes above each eye
            for k in (-1, 0, 1):
                base = on_ball(R * 1.0, side * (34 + 9 * k), 52)
                tip = base + unit(base) * 0.16 + np.array([side * 0.05 * k, 0.1, 0])
                ellipsoid(p, 'Lash', (base + tip) / 2, (0.025, 0.11, 0.025), rings=4, seg=6)


def bow(p):
    """A bow on top of the head, slightly forward: two lobes and a knot."""
    top = on_ball(R * 0.98, 0, 62)
    for side in (-1, 1):
        ellipsoid(p, 'Bow', top + np.array([side * 0.36, 0.1, 0]), (0.36, 0.24, 0.12), r=(0, 0, side * -18), rings=8, seg=14)
    ellipsoid(p, 'Bow', top + np.array([0, 0.1, 0]), (0.13, 0.13, 0.13), rings=6, seg=10)


def write(name, parts, folder):
    folder = pathlib.Path(folder); folder.mkdir(parents=True, exist_ok=True)
    used = sorted({m for p in parts for m, _, _ in p.tris})
    lines = ['# %s: built by tools/make_chomper_assets.py (hinge at the origin, mouth toward +Z)' % name, 'mtllib %s.mtl' % name]
    vidx, nidx, vl, nl, body = {}, {}, [], [], []
    for p in parts:
        body.append('o %s' % p.name)
        for m in used:
            tris = [t for t in p.tris if t[0] == m]
            if not tris:
                continue
            body.append('usemtl %s' % m)
            for _, ps, ns in tris:
                ids = []
                for q, n in zip(ps, ns):
                    q = tuple(round(float(x), 4) + 0.0 for x in q); n = tuple(round(float(x), 3) + 0.0 for x in n)
                    if q not in vidx: vidx[q] = len(vidx) + 1; vl.append('v %.4f %.4f %.4f' % q)
                    if n not in nidx: nidx[n] = len(nidx) + 1; nl.append('vn %.3f %.3f %.3f' % n)
                    ids.append('%d//%d' % (vidx[q], nidx[n]))
                body.append('f ' + ' '.join(ids))
    (folder / ('%s.obj' % name)).write_text('\n'.join(lines + vl + nl + body) + '\n')
    (folder / ('%s.mtl' % name)).write_text(''.join('newmtl %s\nKd %.3f %.3f %.3f\n\n' % ((m,) + MATS[m]) for m in used))
    return sum(len(p.tris) for p in parts)


def jaws():
    I = np.eye(4)
    up = Part('upper', I, [I]); dome(up, 'Chomp', R); disc(up, 'Mouth', R * 0.995, facing_up=False); eyes(up)
    upF = Part('upper', I, [I]); dome(upF, 'Chomp', R); disc(upF, 'Mouth', R * 0.995, facing_up=False); eyes(upF, lashes=True); bow(upF)
    lo = Part('lower', I, [I]); dome(lo, 'Chomp', R * 0.99, up=False); disc(lo, 'Mouth', R * 0.985, facing_up=True)
    return {'chomper_upper': [up], 'chomper_lower': [lo], 'chompette_upper': [upF]}


# ---- sounds ---------------------------------------------------------------------------------------------------------
RATE = 44100


def bite(dur, f0, f1, gain):
    t = np.arange(int(dur * RATE)) / RATE
    f = f0 * (f1 / f0) ** (t / dur)                         # a glide down
    ph = 2 * np.pi * np.cumsum(f) / RATE
    tone = np.sin(ph) + 0.35 * np.sin(2 * ph) + 0.12 * np.sin(3 * ph)
    env = np.minimum(1, t / 0.006) * np.exp(-t / (dur * 0.35))
    rng = np.random.default_rng(7)
    click = rng.standard_normal(len(t)) * np.exp(-t / 0.004) * 0.5                  # the teeth meeting
    thump = np.sin(2 * np.pi * 85 * t) * np.exp(-t / 0.03) * 0.6
    return gain * (tone * env + click + thump)


def chomp_loop():
    period = 1 / 3                                           # one bite each time the mouth shuts (3 Hz)
    out = np.zeros(int(2 * period * RATE))
    for k, (f0, f1, g) in enumerate([(360, 210, 1.0), (300, 175, 0.85)]):
        b = bite(0.15, f0, f1, g)
        s = int(k * period * RATE)
        out[s:s + len(b)] += b
    return out


def gulp():
    t = np.arange(int(0.55 * RATE)) / RATE
    f = 420 * (110 / 420) ** (t / 0.55)
    ph = 2 * np.pi * np.cumsum(f) / RATE
    wob = 1 + 0.45 * np.sin(2 * np.pi * 14 * t)                                     # the bubbly swallow
    tone = (np.sin(ph) + 0.25 * np.sin(2 * ph)) * wob * np.minimum(1, t / 0.02) * np.exp(-t / 0.22)
    boom = np.sin(2 * np.pi * 55 * t) * np.exp(-t / 0.12) * 0.9
    return tone + boom


def write_wav(path, x, peak=0.8):
    x = x / max(1e-9, np.max(np.abs(x))) * peak
    ramp = np.minimum(1, np.minimum(np.arange(len(x)), np.arange(len(x))[::-1]) / 64)   # no click at the ends
    x = x * ramp
    with wave.open(str(path), 'wb') as w:
        w.setnchannels(1); w.setsampwidth(2); w.setframerate(RATE)
        w.writeframes((x * 32767).astype('<i2').tobytes())


def preview(models, path):
    from PIL import Image, ImageDraw
    views = [('Chomper, mouth open', 'chomper_upper', 30), ('Chompette, mouth open', 'chompette_upper', 30), ('Chomper shut, side', 'chomper_upper', 0)]
    W, H, sc = 340, 300, 85
    img = Image.new('RGB', (W * len(views), H), (120, 170, 210)); dr = ImageDraw.Draw(img)
    light = unit([0.4, 0.85, 0.6])
    for vi, (label, upper, ang) in enumerate(views):
        side = vi == 2
        eye = unit([1, 0.12, 0.05]) if side else unit([0.55, 0.45, 1.0])
        fwd = -eye; right = unit(np.cross([0, 1, 0], fwd)); upv = np.cross(fwd, right); Vt = np.array([right, upv, fwd])
        a = math.radians(ang)
        Rx = lambda s: np.array([[1, 0, 0], [0, math.cos(s), -math.sin(s)], [0, math.sin(s), math.cos(s)]])   # +s pitches +Z down
        items = []
        for name, M in ((upper, Rx(-a)), ('chomper_lower', Rx(a))):
            for p in models[name]:
                for mat, ps, ns in p.tris:
                    q = [Vt @ (M @ x) for x in ps]; n = M @ unit(sum(ns))
                    if np.dot(Vt @ n, [0, 0, 1]) > 0.05: continue                     # back faces
                    shade = 0.45 + 0.55 * max(0.0, float(np.dot(n, light)))
                    items.append((sum(x[2] for x in q) / 3, [(W * vi + W / 2 + x[0] * sc, H * 0.55 - x[1] * sc) for x in q],
                                  tuple(int(255 * min(1, c * shade)) for c in MATS[mat])))
        for _, poly, col in sorted(items, key=lambda it: -it[0]):
            dr.polygon(poly, fill=col)
        dr.text((W * vi + 6, 6), label, fill=(10, 10, 10))
    img.save(path)


def main():
    mdir, sdir = pathlib.Path(sys.argv[1]), pathlib.Path(sys.argv[2])
    models = jaws()
    for name, parts in models.items():
        print('%-16s %5d triangles' % (name, write(name, parts, mdir)))
    sdir.mkdir(parents=True, exist_ok=True)
    write_wav(sdir / 'chomp.wav', chomp_loop()); write_wav(sdir / 'gulp.wav', gulp(), peak=0.9)
    print('sounds: chomp.wav %.2f s, gulp.wav 0.55 s' % (2 / 3))
    if '--preview' in sys.argv:
        preview(models, sys.argv[sys.argv.index('--preview') + 1])


if __name__ == '__main__':
    main()
