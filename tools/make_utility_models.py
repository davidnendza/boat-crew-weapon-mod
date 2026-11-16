"""Builds the utility gear's models (OBJ + MTL) for the depth charge slots, plus a preview PNG.

  smoke_pot.obj    a floating smoke pot (US M4A2 style): olive drum, yellow smoke band, lid rims, bail handle, pull ring
  raft_pack.obj    a packed life raft: a yellow canvas bundle with dark straps and a grab handle
  life_raft.obj    the raft on the water: a yellow Carley float (oval ring), slatted floor and a lifeline around it

The first two replace the depth charge models in the launcher (TubeLookOn in weaponmod.cpp) and on a dropped charge
until it lands; their long axis is +Z, centred on the origin, sized to the 0.6 m Mk 6 charge the mod fits them to.
life_raft.obj sits with the water line at y 0, bow +Z. Units are metres, +Y up; the game's OBJ loader reads them as
written (see make_unusual_models.py).

Usage: python make_utility_models.py <models folder> [--preview <png>]
"""
import math
import pathlib
import sys

import numpy as np

import make_chomper_assets as mc
from make_unusual_models import Part, box, cyl, ellipsoid, sphere, torus, unit

mc.MATS.update({'OD': (0.30, 0.33, 0.20), 'SmokeBand': (0.95, 0.78, 0.15), 'Lid': (0.22, 0.23, 0.21), 'Steel': (0.55, 0.57, 0.6),
                'Canvas': (0.98, 0.72, 0.12), 'Strap': (0.14, 0.13, 0.11), 'Float': (0.97, 0.66, 0.1), 'Wood': (0.5, 0.33, 0.17),
                'Rope': (0.78, 0.66, 0.44), 'White': (0.95, 0.95, 0.95),
                'Skin': (0.93, 0.74, 0.62), 'Hair': (0.34, 0.3, 0.26), 'HairGrey': (0.56, 0.54, 0.51), 'Beard': (0.6, 0.57, 0.53), 'BeardDark': (0.36, 0.31, 0.27), 'Lip': (0.72, 0.45, 0.42),
                'Eye': (0.08, 0.08, 0.1), 'Shirt': (0.75, 0.77, 0.8), 'Cat': (0.07, 0.07, 0.08), 'CatEye': (0.98, 0.8, 0.15),
                'Nose': (0.9, 0.5, 0.6), 'Jeans': (0.2, 0.31, 0.52), 'Denim': (0.3, 0.42, 0.65),                 'Sock': (0.62, 0.63, 0.65), 'SockToe': (0.95, 0.8, 0.2)})
I = np.eye(4)


def smoke_pot():
    p = Part('smoke_pot', I, [I])
    R, H = 0.17, 0.21                                       # drum radius and half length (axis +Z)
    cyl(p, 'OD', (0, 0, -H), (0, 0, H), R, seg=24)
    cyl(p, 'SmokeBand', (0, 0, -0.05), (0, 0, 0.05), R + 0.004, seg=24, caps=False)   # yellow band: smoke
    for z in (-H, H):                                       # rolled rims at both ends
        torus(p, 'Lid', (0, 0, z), R - 0.004, 0.012, r=(90, 0, 0), seg=24, tube=6)
    cyl(p, 'Lid', (0, 0, H), (0, 0, H + 0.015), 0.11, seg=20)   # the lid's raised centre
    torus(p, 'Steel', (0, 0, H + 0.03), 0.03, 0.006, r=(90, 0, 0), seg=14, tube=5)   # pull ring
    torus(p, 'Steel', (0, R, 0), 0.15, 0.008, r=(0, 0, 90), seg=16, tube=5, a0=180, a1=360)   # bail handle over the top
    return p


def raft_pack():
    p = Part('raft_pack', I, [I])
    box(p, 'Canvas', (0, 0, 0), (0.42, 0.40, 0.56))
    for z in (-0.17, 0.17):                                 # two straps round the bundle
        box(p, 'Strap', (0, 0, z), (0.432, 0.412, 0.05))
    torus(p, 'Strap', (0, 0.205, 0), 0.08, 0.012, r=(0, 0, 90), seg=12, tube=5, a0=180, a1=360)   # grab handle
    box(p, 'White', (0.212, 0.0, 0.0), (0.004, 0.12, 0.22))   # stencil patch
    return p


def oval_ring(p, mat, a, b, r, y, seg=48, tube=10):
    """A tube of radius r along the ellipse x = a cos t, z = b sin t at height y."""
    def pt(i, k):
        t = 2 * math.pi * i / seg; s = 2 * math.pi * k / tube
        c = np.array([a * math.cos(t), y, b * math.sin(t)])
        out = unit([b * math.cos(t), 0, a * math.sin(t)])     # the ellipse's outward normal
        n = out * math.cos(s) + np.array([0, 1, 0]) * math.sin(s)
        return c + n * r, n
    for i in range(seg):
        for k in range(tube):
            (q0, n0), (q1, n1), (q2, n2), (q3, n3) = pt(i, k), pt(i + 1, k), pt(i + 1, k + 1), pt(i, k + 1)
            p.tri(mat, (q0, q1, q2), (n0, n1, n2))
            p.tri(mat, (q0, q2, q3), (n0, n2, n3))


def life_raft():
    p = Part('life_raft', I, [I])
    a, b, r = 0.85, 1.3, 0.18                               # half width, half length, float radius
    oval_ring(p, 'Float', a, b, r, 0.08)
    for x in np.linspace(-0.6, 0.6, 7):                     # slatted floor inside the ring
        half = b * math.sqrt(max(0.0, 1 - (x / a) ** 2)) - 0.05
        box(p, 'Wood', (x, -0.02, 0), (0.09, 0.03, 2 * half))
    for z in (-0.7, 0, 0.7):                                # cross battens under the slats
        half = a * math.sqrt(max(0.0, 1 - (z / b) ** 2)) - 0.05
        box(p, 'Wood', (0, -0.05, z), (2 * half, 0.03, 0.07))
    n = 14                                                  # lifeline loops hanging round the outside
    for i in range(n):
        t = 2 * math.pi * (i + 0.5) / n
        c = np.array([(a + r + 0.02) * math.cos(t), 0.0, (b + r + 0.02) * math.sin(t)])
        yaw = -math.degrees(t) + 90
        torus(p, 'Rope', c, 0.13, 0.012, r=(90, yaw, 0), seg=10, tube=4, a0=0, a1=180)
    return p


def beard_shell(p, mat, c, rad, th0=0.56, ph0=-0.12, ph1=1.12, rings=8, seg=18):
    """The lower front of an ellipsoid (polar angle from th0*pi down to the chin, front half plus a little), for a beard."""
    c, rad = np.asarray(c, float), np.asarray(rad, float)

    def pt(i, j):
        th = math.pi * (th0 + (1 - th0) * i / rings); ph = math.pi * (ph0 + (ph1 - ph0) * j / seg)
        q = np.array([math.sin(th) * math.cos(ph), math.cos(th), math.sin(th) * math.sin(ph)]) * rad
        return c + q, unit(q / (rad * rad))
    for i in range(rings):
        for j in range(seg):
            (a, na), (b, nb), (cc, nc), (d, nd) = pt(i, j), pt(i, j + 1), pt(i + 1, j + 1), pt(i + 1, j)
            p.tri(mat, (a, b, cc), (na, nb, nc))
            p.tri(mat, (a, cc, d), (na, nc, nd))


def dave():
    """Dave, the developer placeholder: a stiff T-pose that glides over the water. Soles at y 0, facing +Z, 1.8 m tall."""
    p = Part('dave', I, [I])
    for s in (-1, 1):
        x = 0.12 * s
        box(p, 'Sock', (x, 0.04, 0.03), (0.11, 0.08, 0.26))              # grey socks, no shoes
        box(p, 'SockToe', (x, 0.04, 0.155), (0.112, 0.082, 0.04))        # yellow toes
        box(p, 'SockToe', (x, 0.05, -0.095), (0.112, 0.1, 0.03))         # and heels
        cyl(p, 'Sock', (x, 0.07, 0), (x, 0.16, 0), 0.06, seg=12)
        cyl(p, 'Jeans', (x, 0.12, 0), (x, 0.95, 0), 0.085, 0.11, seg=14)  # straight-leg jeans
        cyl(p, 'Denim', (x, 0.12, 0), (x, 0.15, 0), 0.088, seg=14, caps=False)   # turned-up hem
        cyl(p, 'Shirt', (0.24 * s, 1.43, 0), (0.46 * s, 1.43, 0), 0.085, 0.07, seg=12)   # short sleeves, arms straight out
        cyl(p, 'Skin', (0.45 * s, 1.43, 0), (0.83 * s, 1.43, 0), 0.058, 0.045, seg=12)
        ellipsoid(p, 'Skin', (0.88 * s, 1.43, 0), (0.07, 0.022, 0.045), seg=10, rings=6)   # flat palms down
        ellipsoid(p, 'Skin', (0.1 * s, 1.67, 0), (0.02, 0.035, 0.025), seg=8, rings=5)     # ears
        ellipsoid(p, 'HairGrey', (0.09 * s, 1.705, -0.02), (0.016, 0.03, 0.05), seg=10, rings=6)   # short greying sides
        sphere(p, 'Eye', (0.036 * s, 1.685, 0.098), 0.011, rings=4, seg=6)
        box(p, 'Hair', (0.038 * s, 1.712, 0.098), (0.034, 0.007, 0.01), r=(0, 0, -8 * s))   # eyebrows
    ellipsoid(p, 'Jeans', (0, 0.96, 0), (0.23, 0.13, 0.15))             # seat of the jeans
    ellipsoid(p, 'Shirt', (0, 1.2, 0.02), (0.245, 0.34, 0.18))            # torso
    ellipsoid(p, 'Shirt', (0, 1.06, 0.06), (0.23, 0.18, 0.17))            # the belly
    ellipsoid(p, 'Shirt', (0, 1.4, 0), (0.25, 0.1, 0.14))                # shoulders
    cyl(p, 'Skin', (0, 1.47, 0), (0, 1.57, 0), 0.06, seg=12)             # neck
    ellipsoid(p, 'Skin', (0, 1.67, 0.005), (0.1, 0.115, 0.105))          # head, round full face
    ellipsoid(p, 'Skin', (0, 1.67, 0.105), (0.018, 0.025, 0.02), seg=8, rings=5)   # nose
    ellipsoid(p, 'Hair', (0, 1.712, -0.022), (0.104, 0.09, 0.112), r=(-18, 0, 0))   # salt-and-pepper hair hugging the skull
    ellipsoid(p, 'HairGrey', (0, 1.665, -0.03), (0.098, 0.085, 0.085))      # short greying back of the head
    beard_shell(p, 'Beard', (0, 1.665, 0.008), (0.105, 0.12, 0.112))      # short beard: a thin shell over the jaw
    ellipsoid(p, 'BeardDark', (0, 1.639, 0.106), (0.045, 0.011, 0.012), seg=10, rings=5)    # moustache
    box(p, 'Lip', (0, 1.622, 0.108), (0.04, 0.008, 0.01))                # mouth
    # the shirt print: a black cat face on the chest
    z, y = 0.215, 1.04
    ellipsoid(p, 'Cat', (0, 1.25 + y - 1.0, z), (0.09, 0.072, 0.025), seg=14, rings=6)
    for s in (-1, 1):
        cyl(p, 'Cat', (0.055 * s, 1.29 + y - 1.0, z - 0.01), (0.078 * s, 1.355 + y - 1.0, z - 0.03), 0.028, 0.0, seg=4)   # ears
        ellipsoid(p, 'CatEye', (0.034 * s, 1.258 + y - 1.0, z + 0.022), (0.017, 0.012, 0.006), seg=8, rings=4)
        sphere(p, 'Eye', (0.034 * s, 1.258 + y - 1.0, z + 0.027), 0.005, rings=3, seg=5)
        for k in (-1, 1):
            box(p, 'White', (0.075 * s, 1.235 + y - 1.0 + 0.01 * k, z + 0.024), (0.07, 0.003, 0.003), r=(0, 0, 8 * k * s))   # whiskers
    ellipsoid(p, 'Nose', (0, 1.24 + y - 1.0, z + 0.024), (0.009, 0.006, 0.005), seg=6, rings=3)
    return p


def preview(models, path, views=None):
    """Each model from above and to one side, flat shaded (painter's order), on a sea-blue background."""
    from PIL import Image, ImageDraw
    views = views or [('Smoke pot', 'smoke_pot', 380), ('Packed raft', 'raft_pack', 330), ('Raft on the water', 'life_raft', 95)]
    W, H = 340, 300
    img = Image.new('RGB', (W * len(views), H), (60, 110, 150)); dr = ImageDraw.Draw(img)
    light = unit([0.4, 0.85, 0.6])
    for vi, view in enumerate(views):
        label, name, sc = view[:3]
        eye = unit(view[3] if len(view) > 3 else [0.8, 0.6, 0.9]); fwd = -eye; right = unit(np.cross([0, 1, 0], fwd)); upv = np.cross(fwd, right)
        Vt = np.array([right, upv, fwd]); oy = view[4] if len(view) > 4 else 0.0
        items = []
        for p in models[name]:
            for mat, ps, ns in p.tris:
                n = unit(sum(ns)); q = [Vt @ x for x in ps]
                if np.dot(Vt @ n, [0, 0, 1]) > 0.05: continue
                shade = 0.45 + 0.55 * max(0.0, float(np.dot(n, light)))
                items.append((sum(x[2] for x in q) / 3, [(W * vi + W / 2 + x[0] * sc, H * 0.55 - (x[1] - oy) * sc) for x in q],
                              tuple(int(255 * min(1, c * shade)) for c in mc.MATS[mat])))
        for _, poly, col in sorted(items, key=lambda it: -it[0]):
            dr.polygon(poly, fill=col)
        dr.text((W * vi + 6, 6), label, fill=(240, 240, 240))
    img.save(path)


def main():
    mdir = pathlib.Path(sys.argv[1])
    models = {'smoke_pot': [smoke_pot()], 'raft_pack': [raft_pack()], 'life_raft': [life_raft()], 'dave': [dave()]}
    for name, parts in models.items():
        print('%-10s %5d triangles' % (name, mc.write(name, parts, mdir)))
    if '--preview' in sys.argv:
        out = pathlib.Path(sys.argv[sys.argv.index('--preview') + 1])
        preview(models, out)
        preview(models, out.with_name('dave_preview.png'), [('Dave (front)', 'dave', 150, [0, 0.15, 1], 0.95),
                ('Dave', 'dave', 150, [0.8, 0.35, 0.9], 0.95), ('Dave (side)', 'dave', 150, [1, 0.1, 0], 0.95)])


if __name__ == '__main__':
    main()
