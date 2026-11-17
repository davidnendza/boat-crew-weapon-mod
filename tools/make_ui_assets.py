# Draws the "Field Manual" look for Mod Options / What's New into models\ (David 2026-11-05: 'I love B, the field
# manual one'): manila paper, its drop shadow, the red MODDED rubber stamp, the clipped note card, the blue-ink X, the
# ink arrows and the red "restart pending" dot. Everything is drawn at 2x the size the mod shows it at, supersampled,
# and seeded, so a rerun gives the same pictures. Needs Pillow + numpy.
#   python make_ui_assets.py      writes models\ui_*.png and tools\ui_assets_preview.png (the options sheet mocked up)
# ui_notecard.png is a FIXED CONTRACT with weaponmod.cpp: shown at 262x372, card box x 6..256, y 22..362 (rotated 1.5
# deg about its centre), text-safe box x 26..236, y 52..342. Change those numbers only together with the C++.
import math, os
import numpy as np
from PIL import Image, ImageDraw, ImageFilter, ImageFont

HERE = os.path.dirname(os.path.abspath(__file__))
MODELS = os.path.normpath(os.path.join(HERE, "..", "models"))
BAHN = r"C:\Windows\Fonts\bahnschrift.ttf"          # stand-in for Kanit (stamp lettering, preview text)
PAPER, INK, MUTED, BLUE, RED, CARD = "#e8dcc0", "#2b2a26", "#6b5f45", "#1f3f8a", "#b3261e", "#f6f1e3"
STEEL = "#7d8790"

def rgb(h): return tuple(int(h[i:i + 2], 16) for i in (1, 3, 5))
def rng(seed): return np.random.default_rng(seed)

def font(size, weight="Regular"):
    f = ImageFont.truetype(BAHN, max(1, int(round(size))))
    f.set_variation_by_name(weight)
    return f

def smooth_noise(w, h, scale, seed):
    """Value noise in -1..1 with features about `scale` px across (bicubic-upscaled random grid)."""
    gw, gh = max(2, int(w / scale) + 2), max(2, int(h / scale) + 2)
    g = Image.fromarray((rng(seed).random((gh, gw)) * 255).astype(np.uint8))
    a = np.asarray(g.resize((w, h), Image.BICUBIC), np.float32) / 127.5 - 1
    return a

def down(img, size):
    """Supersampled RGBA -> output size, filtering in premultiplied alpha so edges keep their colour."""
    a = np.asarray(img, np.float32) / 255
    pre = a.copy(); pre[..., :3] *= a[..., 3:4]
    out = []
    for c in range(4):
        out.append(np.asarray(Image.fromarray(pre[..., c]).resize(size, Image.LANCZOS), np.float32))
    out = np.clip(np.stack(out, -1), 0, 1)
    al = out[..., 3:4]
    out[..., :3] = np.where(al > 1e-4, out[..., :3] / np.maximum(al, 1e-4), 0)
    return Image.fromarray((np.clip(out, 0, 1) * 255 + 0.5).astype(np.uint8), "RGBA")

def colour_alpha(hexcol, alpha):
    """RGBA image of one colour with the given float alpha array (0..1)."""
    h, w = alpha.shape
    a = np.zeros((h, w, 4), np.uint8)
    a[..., :3] = rgb(hexcol)
    a[..., 3] = np.clip(alpha * 255 + 0.5, 0, 255).astype(np.uint8)
    return Image.fromarray(a, "RGBA")

def save(img, name):
    p = os.path.join(MODELS, name)
    img.save(p, optimize=True)
    print("wrote %-24s %4dx%-4d %8d bytes" % ("models\\" + name, img.size[0], img.size[1], os.path.getsize(p)))
    return img

# ---------------------------------------------------------------------------------------------------- paper + shadow
def make_paper(W=1280, H=776, seed=11):
    r = rng(seed)
    base = np.array(rgb(PAPER), np.float32)
    lum = np.zeros((H, W), np.float32)
    lum += smooth_noise(W, H, 180, seed + 1) * 2.2          # big soft blotches (uneven pulp)
    lum += smooth_noise(W, H, 40, seed + 2) * 1.4
    lum += smooth_noise(W, H, 6, seed + 3) * 0.9            # fine tooth
    # fibres: short faint strokes, mostly lying along the sheet
    fib = Image.new("L", (W, H), 128); d = ImageDraw.Draw(fib)
    for _ in range(5200):
        x, y = r.random() * W, r.random() * H
        ang = r.normal(0, 0.5) + (math.pi / 2 if r.random() < 0.25 else 0)
        L = r.uniform(4, 18)
        v = 128 + int(r.choice([-1, 1]) * r.uniform(10, 26))
        d.line([(x, y), (x + math.cos(ang) * L, y + math.sin(ang) * L)], fill=v, width=1)
    fib = np.asarray(fib.filter(ImageFilter.GaussianBlur(0.6)), np.float32) - 128
    lum += fib * 0.24
    # faint horizontal ruling every 3 shown px (4 px here), 3% darker
    yy = np.arange(H)[:, None]
    rule = ((yy % 4) == 0).astype(np.float32) * np.ones((1, W), np.float32)
    # darker edges
    xs, ys = np.linspace(-1, 1, W)[None, :], np.linspace(-1, 1, H)[:, None]
    edge = np.clip(np.maximum(np.abs(xs) ** 6, np.abs(ys) ** 6) * 1.0 + (xs ** 2 + ys ** 2) * 0.05, 0, 1)
    edge = edge + np.clip(smooth_noise(W, H, 60, seed + 4) * 0.4, -0.3, 0.3) * edge
    # one fold: a slightly tilted horizontal crease, dark below, a lighter lip above
    fy = H * 0.47 + (np.arange(W)[None, :] - W / 2) * 0.012 + smooth_noise(W, 1, 300, seed + 5)[0][None, :] * 2.0
    dist = yy - fy
    fold = -np.exp(-(dist - 1.2) ** 2 / 2.0) * 4.2 + np.exp(-(dist + 1.5) ** 2 / 3.0) * 2.4 - np.exp(-(dist - 6) ** 2 / 60.0) * 0.8
    col = base[None, None, :] + lum[..., None]
    col = col * (1 - 0.035 * rule[..., None]) + np.array([90, 70, 30], np.float32) * 0.035 * rule[..., None]
    col = col * (1 - 0.13 * edge[..., None]) + np.array([150, 120, 70], np.float32) * 0.13 * edge[..., None]
    col += fold[..., None]
    return Image.fromarray(np.clip(col + 0.5, 0, 255).astype(np.uint8), "RGB")

def make_shadow(W=504, H=316):
    # maps to the sheet + 24 px on each side (1008x630 shown): sheet sits 12 px in from each edge here, shadow drops
    # ~5 px (shown 10) and blurs ~15 px (shown 30) like the mockup's box-shadow 0 10px 30px
    m = Image.new("L", (W, H), 0)
    sx, sy = W / 1008, H / 630
    ImageDraw.Draw(m).rectangle([24 * sx, (24 + 8) * sy, W - 24 * sx, H - (24 - 8) * sy], fill=255)
    m = m.filter(ImageFilter.GaussianBlur(7))
    a = np.asarray(m, np.float32) / 255 * 0.55
    return colour_alpha("#000000", a)

# ---------------------------------------------------------------------------------------------------------- stamp
def ink_texture(mask, seed, density=0.86, holes=900, hole_r=(0.6, 2.4)):
    """Rubber-stamp ink on a float mask (0..1): ragged edges, fuller ink at the rims, speckle holes, uneven pressure."""
    h, w = mask.shape
    r = rng(seed)
    # ragged edge: blur, add grain, re-threshold
    soft = np.asarray(Image.fromarray((mask * 255).astype(np.uint8)).filter(ImageFilter.GaussianBlur(2.2)), np.float32) / 255
    grain = smooth_noise(w, h, 5, seed + 1) * 0.22 + smooth_noise(w, h, 22, seed + 2) * 0.12
    m = np.clip((soft + grain - 0.5) * 6 + 0.5, 0, 1)
    # rims hold more ink
    er = np.asarray(Image.fromarray((m * 255).astype(np.uint8)).filter(ImageFilter.MinFilter(9)).filter(ImageFilter.GaussianBlur(3)), np.float32) / 255
    rim = np.clip(m - er, 0, 1)
    pressure = 0.82 + smooth_noise(w, h, 140, seed + 3) * 0.16 + smooth_noise(w, h, 18, seed + 4) * 0.08
    dens = np.clip(pressure + rim * 0.2, 0, 1)
    # where the rubber missed: irregular pale patches from thresholded noise, a sprinkle of pin holes, dry streaks
    patchy = smooth_noise(w, h, 7 * w / 300, seed + 5) + smooth_noise(w, h, 2.5 * w / 300, seed + 6) * 0.6
    miss = np.clip((patchy - 0.62) * 3.0, 0, 1)
    hole = Image.new("L", (w, h), 0); d = ImageDraw.Draw(hole)
    for _ in range(holes):
        x, y, rr = r.random() * w, r.random() * h, r.uniform(*hole_r) * (w / 300)
        d.ellipse([x - rr, y - rr * r.uniform(0.5, 1.1), x + rr, y + rr], fill=int(r.uniform(120, 230)))
    for _ in range(4):
        y = r.random() * h; x0 = r.random() * w
        d.line([(x0, y), (x0 + r.uniform(40, 140) * w / 300, y + r.normal(0, 3))], fill=90, width=max(1, int(1.5 * w / 300)))
    hole = np.asarray(hole.filter(ImageFilter.GaussianBlur(0.7 * w / 300)), np.float32) / 255
    dens *= (1 - miss * 0.85) * (1 - hole)
    return m * dens * density, rim

def make_stamp(seed=21):
    F = 8                                                    # supersample: 8 per shown px, output 2 per shown px
    W, H = 150 * F, 56 * F
    m = Image.new("L", (W, H), 0); d = ImageDraw.Draw(m)
    bw, bh, t = 136 * F, 34 * F, 3 * F
    x0, y0 = (W - bw) // 2, (H - bh) // 2
    d.rounded_rectangle([x0, y0, x0 + bw, y0 + bh], radius=2 * F, outline=255, width=t)
    f = font(19 * F, "Bold SemiCondensed")
    text, sp = "MODDED", 4.5 * F
    widths = [f.getlength(c) for c in text]
    total = sum(widths) + sp * (len(text) - 1)
    l, tp, rr, bt = f.getbbox("MODDED")
    x = (W - total) / 2; ty = y0 + (bh - (bt - tp)) / 2 - tp + 0.5 * F
    for c, cw in zip(text, widths):
        d.text((x, ty), c, font=f, fill=255); x += cw + sp
    mask = np.asarray(m, np.float32) / 255
    a, rim = ink_texture(mask, seed, density=0.9, holes=140, hole_r=(0.25, 0.9))
    col = np.zeros((H, W, 4), np.float32)
    red, dark = np.array(rgb(RED), np.float32), np.array([142, 26, 20], np.float32)
    col[..., :3] = red + (dark - red) * np.clip(rim * 1.4, 0, 1)[..., None]
    col[..., 3] = a * 255
    img = Image.fromarray(col.astype(np.uint8), "RGBA")
    img = img.rotate(6, resample=Image.BICUBIC, center=(W / 2, H / 2))   # CSS rotate(-6deg): right end up
    return down(img, (300, 112))

# -------------------------------------------------------------------------------------------------------- notecard
def clip_path(ox, oy, F):
    """The mockup's paperclip (SVG path, 26x64 box at card-left+28, 22 px above the card), shortened so it ends above
    the text-safe box (y 52). Returns polylines in supersampled px."""
    def P(x, y): return ((ox + x) * F, (oy + 2 + (y - 3) * 0.84) * F)
    def quad(p0, p1, p2, n=24):
        return [((1 - t) ** 2 * p0[0] + 2 * (1 - t) * t * p1[0] + t * t * p2[0], (1 - t) ** 2 * p0[1] + 2 * (1 - t) * t * p1[1] + t * t * p2[1]) for t in np.linspace(0, 1, n)]
    pts = [(8, 50), (8, 10)] + quad((8, 10), (8, 3), (14, 3)) + quad((14, 3), (20, 3), (20, 10)) + [(20, 46)] \
        + quad((20, 46), (20, 56), (13, 56)) + quad((13, 56), (4, 56), (4, 46)) + [(4, 18)]
    return [P(x, y) for x, y in pts]

def stroke(draw, pts, width, fill):
    draw.line(pts, fill=fill, width=int(round(width)), joint="curve")
    r = width / 2
    for x, y in (pts[0], pts[-1]):
        draw.ellipse([x - r, y - r, x + r, y + r], fill=fill)

def make_notecard(seed=31):
    F = 4                                                    # 4 per shown px, output 2
    W, H = 262 * F, 372 * F
    cx0, cy0, cx1, cy1 = 6 * F, 22 * F, 256 * F, 362 * F
    ccx, ccy = (cx0 + cx1) / 2, (cy0 + cy1) / 2
    # the card: off-white, faint fibres, a touch darker toward the rim, softly worn corners
    cm = Image.new("L", (W, H), 0)
    ImageDraw.Draw(cm).rounded_rectangle([cx0, cy0, cx1, cy1], radius=int(1.6 * F), fill=255)
    lum = smooth_noise(W, H, 120, seed) * 1.8 + smooth_noise(W, H, 8, seed + 1) * 1.1
    r = rng(seed + 2)
    fib = Image.new("L", (W, H), 128); d = ImageDraw.Draw(fib)
    for _ in range(1600):
        x, y = r.uniform(cx0, cx1), r.uniform(cy0, cy1); ang = r.normal(0, 0.6); L = r.uniform(5, 22)
        d.line([(x, y), (x + math.cos(ang) * L, y + math.sin(ang) * L)], fill=128 + int(r.choice([-1, 1]) * r.uniform(8, 20)), width=1)
    lum += (np.asarray(fib.filter(ImageFilter.GaussianBlur(0.8)), np.float32) - 128) * 0.12
    X, Y = np.meshgrid(np.arange(W, dtype=np.float32), np.arange(H, dtype=np.float32))
    ex = np.minimum(X - cx0, cx1 - X) / F; ey = np.minimum(Y - cy0, cy1 - Y) / F
    rim = np.exp(-np.clip(np.minimum(ex, ey), 0, None) / 5.0)
    col = np.array(rgb(CARD), np.float32)[None, None, :] + lum[..., None]
    col = col * (1 - 0.07 * rim[..., None]) + np.array([170, 150, 110], np.float32) * 0.07 * rim[..., None]
    card = np.zeros((H, W, 4), np.float32); card[..., :3] = col; card[..., 3] = np.asarray(cm, np.float32)
    card = Image.fromarray(np.clip(card, 0, 255).astype(np.uint8), "RGBA")
    card = card.rotate(-1.5, resample=Image.BICUBIC, center=(ccx, ccy))   # CSS rotate(1.5deg): clockwise
    # shadow of the card on the sheet: soft 2,3 px drop + tight contact shadow
    ca = card.getchannel("A")
    sh = Image.new("L", (W, H), 0); sh.paste(ca, (2 * F, 3 * F))
    sh1 = np.asarray(sh.filter(ImageFilter.GaussianBlur(3.2 * F)), np.float32) / 255 * 0.30
    sh2 = Image.new("L", (W, H), 0); sh2.paste(ca, (int(0.6 * F), int(1 * F)))
    sh2 = np.asarray(sh2.filter(ImageFilter.GaussianBlur(0.9 * F)), np.float32) / 255 * 0.22
    # the PNG edge would cut the shadow off hard (the rotated card reaches x 260.4 of 262): fade it out over the last 4 px
    fade = np.clip(np.minimum(np.minimum(X, W - 1 - X), np.minimum(Y, H - 1 - Y)) / (4 * F), 0, 1)
    out = colour_alpha("#1a140a", np.clip(sh1 + sh2, 0, 1) * fade)
    out.alpha_composite(card)
    # paperclip: shadow on the card, dark rim, steel body, a cold highlight on its upper-left edge
    # The clip grips the card: the inner leg (the path's first segment) runs behind it, so it only shows above the
    # card's top edge (y ~19.7 at the clip after the 1.5 deg turn); the outer loop lies on the front.
    pts = clip_path(28 + 6, 0, F)
    top = 19.7 * F
    back, front = [(x, min(y, top + 0.6 * F)) for x, y in pts[:2]], pts[1:]
    cs = Image.new("L", (W, H), 0); stroke(ImageDraw.Draw(cs), [(x + 1.3 * F, y + 1.8 * F) for x, y in front], 2.6 * F, 255)
    cs = np.asarray(cs.filter(ImageFilter.GaussianBlur(1.1 * F)), np.float32) / 255 * 0.38
    out.alpha_composite(colour_alpha("#1a140a", cs))
    def wire(path, cut=None):
        im = Image.new("RGBA", (W, H), (0, 0, 0, 0)); d = ImageDraw.Draw(im)
        stroke(d, path, 2.9 * F, (78, 86, 94, 255))
        stroke(d, path, 2.1 * F, rgb(STEEL) + (255,))
        stroke(d, [(x - 0.45 * F, y - 0.45 * F) for x, y in path], 0.8 * F, (205, 213, 220, 255))
        if cut is not None:                                  # hide what is behind the card
            a = np.asarray(im).copy(); a[int(cut):, :, 3] = 0; im = Image.fromarray(a, "RGBA")
        return im
    out.alpha_composite(wire(back, cut=top))
    gap = Image.new("L", (W, H), 0)                          # a hair of shade where the leg slips under the edge
    ImageDraw.Draw(gap).ellipse([back[0][0] - 2.2 * F, top - 0.6 * F, back[0][0] + 2.2 * F, top + 1.4 * F], fill=255)
    out.alpha_composite(colour_alpha("#1a140a", np.asarray(gap.filter(ImageFilter.GaussianBlur(0.8 * F)), np.float32) / 255 * 0.25))
    out.alpha_composite(wire(front))
    return down(out, (524, 744))

# --------------------------------------------------------------------------------------------------- X, arrows, dot
def pen_stroke(mask_draw, p0, p1, p2, w0, w1, F, n=90, seed=0):
    """A quadratic pen stroke with pressure: thin start, full middle, flicked thin end."""
    r = rng(seed)
    wob = smooth_noise(n, 1, 12, seed)[0]
    for i, t in enumerate(np.linspace(0, 1, n)):
        x = (1 - t) ** 2 * p0[0] + 2 * (1 - t) * t * p1[0] + t * t * p2[0]
        y = (1 - t) ** 2 * p0[1] + 2 * (1 - t) * t * p1[1] + t * t * p2[1]
        press = math.sin(min(1, t * 1.25 + 0.15) * math.pi) ** 0.55
        rad = (w0 + (w1 - w0) * press) * (1 + wob[i] * 0.08) * F / 2
        mask_draw.ellipse([x - rad, y - rad, x + rad, y + rad], fill=255)

def make_x(seed=41):
    F = 8; W = H = 18 * F
    m1 = Image.new("L", (W, H), 0); m2 = Image.new("L", (W, H), 0)
    pen_stroke(ImageDraw.Draw(m1), (3.2 * F, 3.0 * F), (8.0 * F, 9.6 * F), (15.0 * F, 15.2 * F), 1.3, 3.1, F, seed=seed)
    pen_stroke(ImageDraw.Draw(m2), (14.9 * F, 2.6 * F), (8.6 * F, 7.6 * F), (3.0 * F, 15.0 * F), 1.2, 2.9, F, seed=seed + 1)
    a1 = np.asarray(m1, np.float32) / 255 * 0.9; a2 = np.asarray(m2, np.float32) / 255 * 0.9
    grain = 1 + smooth_noise(W, H, 6, seed + 2) * 0.07
    a = np.clip((a1 + a2 - a1 * a2) * grain, 0, 1)
    over = a1 * a2                                           # ink is darker where the strokes cross
    col = np.zeros((H, W, 4), np.float32)
    blue, deep = np.array(rgb(BLUE), np.float32), np.array([18, 36, 92], np.float32)
    col[..., :3] = blue + (deep - blue) * np.clip(over * 1.2, 0, 1)[..., None]
    col[..., 3] = a * 255
    return down(Image.fromarray(col.astype(np.uint8), "RGBA"), (36, 36))

def make_arrow(left, seed=51):
    F = 8; W, H = 14 * F, 16 * F
    m = Image.new("L", (W, H), 0)
    pts = [(2.2, 8.1), (11.8, 2.0), (12.1, 14.1)]
    if not left: pts = [(14 - x, y) for x, y in pts]
    ImageDraw.Draw(m).polygon([(x * F, y * F) for x, y in pts], fill=255)
    # slightly soft, inked corners: round them and let the edge wander a hair
    s = np.asarray(m.filter(ImageFilter.GaussianBlur(0.9 * F)), np.float32) / 255
    s = s + smooth_noise(W, H, 3 * F, seed) * 0.06
    a = np.clip((s - 0.42) * 8, 0, 1) * 0.96
    return down(colour_alpha(INK, a), (28, 32))

def make_dot():
    F = 8; W = H = 8 * F
    m = Image.new("L", (W, H), 0)
    ImageDraw.Draw(m).ellipse([0.6 * F, 0.6 * F, 7.4 * F, 7.4 * F], fill=255)
    return down(colour_alpha(RED, np.asarray(m, np.float32) / 255), (16, 16))

# ----------------------------------------------------------------------------------------------------------- preview
def preview(paper, shadow, stamp, card, x, al, ar, dot):
    """The Mod Options sheet roughly as the mockup lays it out, at 1x (1040x700), assets scaled down from 2x."""
    W, H = 1040, 700
    xs, ys = np.meshgrid(np.linspace(0, 1, W), np.linspace(0, 1, H))
    t = np.clip(np.sqrt(((xs - 0.3) / 0.7) ** 2 + ((ys - 0.2) / 0.7) ** 2) / 0.7, 0, 1)[..., None]
    bg = np.array([0x3d, 0x55, 0x66], np.float32) * (1 - t) + np.array([0x1b, 0x27, 0x32], np.float32) * t
    img = Image.fromarray(bg.astype(np.uint8), "RGB").convert("RGBA")
    d = ImageDraw.Draw(img)
    half = lambda im: im.resize((im.size[0] // 2, im.size[1] // 2), Image.LANCZOS)
    def tx(xy, s, size, colr, w="Regular", sp=0, anchor="la"):
        f = font(size, w)
        if not sp: d.text(xy, s, font=f, fill=colr, anchor=anchor); return
        x0, y0 = xy
        for c in s: d.text((x0, y0), c, font=f, fill=colr, anchor=anchor); x0 += f.getlength(c) + sp
    Px, Py = 40, 40; Sx, Sy, Sw, Sh = Px, Py + 38, 960, 582
    # inactive tabs first, then shadow + sheet, then the active tab joining the sheet
    tabs = ["LOOT", "ARSENAL", "VISUALS", "ENEMIES", "CREW", "CO-OP"]; act = 2
    for i, n in enumerate(tabs):
        if i == act: continue
        x0 = Sx + 24 + i * 120
        d.rounded_rectangle([x0, Py + 6, x0 + 118, Sy + 8], radius=8, fill="#c9b88f")
        d.rectangle([x0, Sy - 4, x0 + 118, Sy], fill="#b8a77e")
        tx((x0 + 59, Py + 6 + 16), " ".join(n), 13, "#5a4f38", anchor="mm")
    img.alpha_composite(shadow.resize((Sw + 48, Sh + 48), Image.BILINEAR), (Sx - 24, Sy - 24))
    img.alpha_composite(paper.convert("RGBA").resize((Sw, Sh), Image.LANCZOS), (Sx, Sy))
    d = ImageDraw.Draw(img)
    x0 = Sx + 24 + act * 120
    d.rounded_rectangle([x0, Py, x0 + 118, Sy + 6], radius=8, fill="#dccdaf")   # kTabOn: the paper's darkened top band
    tx((x0 + 59, Py + 20), " ".join(tabs[act]), 13, INK, "SemiBold", anchor="mm")
    img.alpha_composite(half(dot), (Sx + 24 + 4 * 120 + 92, Py + 6 + 12))
    # header
    tx((Sx + 52, Sy + 16), " ".join("BOAT CREW WEAPON MOD  ·  FORM 0.9.2"), 10, MUTED)
    tx((Sx + 52, Sy + 30), " ".join("ORDNANCE OPTIONS").replace("   ", "     "), 26, INK, "SemiBold")
    img.alpha_composite(half(stamp), (Sx + Sw - 22 - 30 - 16 - 150, Sy + 7))
    bx, by = Sx + Sw - 22 - 30 - 8, Sy + 20
    d.rectangle([bx, by, bx + 29, by + 29], outline=INK, width=2)
    xi = half(x); xa = np.asarray(xi).copy(); xa[..., :3] = rgb(INK)
    img.alpha_composite(Image.fromarray(xa, "RGBA"), (bx + 6, by + 6))
    d.rectangle([Sx + 22, Sy + 68, Sx + Sw - 22, Sy + 69], fill=INK)
    # rows
    rx, rw = Sx + 52, Sw - 52 - 26 - 250 - 52
    hy = Sy + 70 + 14
    tx((rx, hy), " ".join("SECTION 3  —  WHAT YOU SEE"), 12, MUTED)
    rows = [("Rarity tracer colors", "Game needs a restart to take effect", "b", 1), ("Heal and chain streaks", "A streak on each heal and chain leap", "b", 1),
            ("Smoking barrels", "Muzzle smoke after a long burst", "b", 1), ("Torpedo kill cam", "Single player only", "b", 1),
            ("Wait between kill cams", "Seconds before the next one", "d", "60 s"), ("Game speed during a kill cam", "25% is a quarter speed", "d", "25%"),
            ("MODDED on the title screen", "The red tag under the logo", "b", 0)]
    for i, (lab, desc, k, v) in enumerate(rows):
        y = hy + 20 + i * 52
        if i == 2:
            hl = Image.new("RGBA", (rw, 52), (0xf0, 0xc8, 0x3c, 90)); img.alpha_composite(hl, (rx, y)); d = ImageDraw.Draw(img)
        d.line([(rx, y + 51), (rx + rw, y + 51)], fill=(43, 42, 38, 64) if False else "#bcae8f", width=1)
        tx((rx + 6, y + 18), "3.%d" % (i + 1), 12, MUTED)
        tx((rx + 32, y + 9), lab, 15, INK); tx((rx + 32, y + 30), desc, 11, MUTED)
        if k == "b":
            for j, (lbl, on) in enumerate((("ON", v == 1), ("OFF", v == 0))):
                cx = rx + rw - 118 + j * 58
                d.rectangle([cx, y + 18, cx + 15, y + 33], outline=INK, width=2)
                if on: img.alpha_composite(half(x), (cx - 1, y + 16)); d = ImageDraw.Draw(img)
                tx((cx + 22, y + 18), " ".join(lbl), 12, INK)
        else:
            vx = rx + rw - 128
            img.alpha_composite(half(al), (vx, y + 18)); img.alpha_composite(half(ar), (vx + 100, y + 18)); d = ImageDraw.Draw(img)
            d.rectangle([vx + 22, y + 36, vx + 92, y + 37], fill=INK)
            tx((vx + 57, y + 26), v, 16, BLUE, "SemiBold", anchor="mm")
    # note card (contract: card box at PNG x 6, y 22; text-safe box x 26..236, y 52..342)
    cxp, cyp = rx + rw + 26 - 6, hy + 8 - 22
    img.alpha_composite(half(card), (cxp, cyp)); d = ImageDraw.Draw(img)
    tx((cxp + 26, cyp + 52), " ".join("NOTE"), 11, MUTED)
    tx((cxp + 26, cyp + 72), "Smoking barrels", 18, INK, "SemiBold")
    body = ["A gun that fires steadily for a", "few seconds keeps smoking from", "the muzzle for a while after it", "stops. Looks only, on this", "machine only."]
    for i, s in enumerate(body): tx((cxp + 26, cyp + 104 + i * 19), s, 13, INK)
    for xx in range(cxp + 26, cxp + 236, 7): d.line([(xx, cyp + 300), (xx + 3, cyp + 300)], fill=MUTED)
    tx((cxp + 26, cyp + 308), "drops.ini  ·  barrel_smoke", 11, MUTED); tx((cxp + 26, cyp + 325), "default ON  ·  applies at once", 11, MUTED)
    # footer
    fy = Sy + Sh - 58
    d.rectangle([Sx + 22, fy, Sx + Sw - 22, fy + 1], fill=INK)
    tx((Sx + 52, fy + 22), "Filed to BoatCrewMod\\drops.ini", 11, MUTED)
    tx((Sx + 240, fy + 22), "1 change waits for a game restart", 11, RED)
    d.rectangle([Sx + 480, fy + 21, Sx + 495, fy + 36], outline=INK, width=2); tx((Sx + 503, fy + 22), "Detailed log (for bug reports)", 11, INK)
    d.rectangle([Sx + Sw - 52 - 80 - 14 - 120, fy + 13, Sx + Sw - 52 - 80 - 14, fy + 45], outline=INK, width=2)
    tx((Sx + Sw - 52 - 80 - 14 - 60, fy + 29), " ".join("WHAT'S NEW"), 12, INK, anchor="mm")
    d.rectangle([Sx + Sw - 52 - 80, fy + 13, Sx + Sw - 52, fy + 45], fill=INK)
    tx((Sx + Sw - 52 - 40, fy + 29), " ".join("DONE"), 12, PAPER, "SemiBold", anchor="mm")
    return img.convert("RGB")

# ===================================================================================================== field crate
# The inventory window as a plywood field crate (David 2026-11-05: 'maybe like a field crate sort of look', 'make it
# look like it's made out of plywood?', 'Yes, build it'). The C++ draws the seams, the ply edge and every flat fill so
# they stay crisp at any grid size; these are only the textures.
PLY, WOOD_INK, STAMP_RED, OLIVE_A, OLIVE_B = "#cfae7c", "#2d1e0f", "#c9372c", "#5b5a45", "#3d3c2c"

def aniso_noise(w, h, sx, sy, seed):
    """Value noise in -1..1 with features about sx px long and sy px tall (long streaks when sx >> sy)."""
    gw, gh = max(2, int(w / sx) + 3), max(2, int(h / sy) + 3)
    g = Image.fromarray((rng(seed).random((gh, gw)) * 255).astype(np.uint8))
    return np.asarray(g.resize((w, h), Image.BICUBIC), np.float32) / 127.5 - 1

def make_ply(W=2016, H=1380, seed=61):
    """Rotary-cut birch face: pale, long soft wavy grain left-right, a few faint figure patches and pin knots."""
    r = rng(seed)
    X, Y = np.meshgrid(np.arange(W, dtype=np.float32), np.arange(H, dtype=np.float32))
    # the grain runs along x; its lines wander up and down slowly, and bend around the pin knots
    warp = aniso_noise(W, H, 900, 500, seed + 1) * 70 + aniso_noise(W, H, 260, 160, seed + 2) * 14 + aniso_noise(W, H, 70, 60, seed + 3) * 2.5
    knots = [(r.uniform(0.06, 0.94) * W, r.uniform(0.06, 0.94) * H, r.uniform(5, 9)) for _ in range(5)]
    for kx, ky, ks in knots:
        dx, dy = (X - kx) / (ks * 9), (Y - ky) / (ks * 2.2)
        warp += np.sign(Y - ky) * np.exp(-(dx * dx + dy * dy)) * ks * 2.4
    v = Y + warp
    # grain lines: a 1-D random profile across the lines (soft, uneven widths), looked up at the warped height
    n = int(H + 400)
    prof = np.zeros(n, np.float32)
    for sc, amp in ((3.0, 0.55), (7.0, 1.0), (19.0, 0.8), (55.0, 0.5)):
        k = max(2, int(n / sc) + 3)
        prof += amp * (np.interp(np.arange(n) / sc, np.arange(k), r.random(k) * 2 - 1)).astype(np.float32)
    lines = np.interp(v + 200, np.arange(n), prof).astype(np.float32)
    lines = np.sign(lines) * np.abs(lines) ** 1.4                      # mostly quiet, some distinct lines
    fade = 0.55 + 0.45 * aniso_noise(W, H, 700, 90, seed + 4)          # lines come and go along their length
    lum = lines * fade * 5.0
    lum += aniso_noise(W, H, 420, 9, seed + 5) * 2.6                   # long fine streaks
    lum += aniso_noise(W, H, 120, 4, seed + 6) * 1.6
    lum += smooth_noise(W, H, 6, seed + 7) * 0.7                       # tooth
    # figure: a few broad, faint, darker cathedral patches (ring bands inside an elongated blob)
    tone = np.zeros((H, W), np.float32)
    for (fx, fy, fw, fh) in ((0.22, 0.30, 360, 56), (0.78, 0.72, 480, 62), (0.60, 0.12, 280, 40), (0.36, 0.86, 320, 44), (0.92, 0.36, 260, 48)):
        dx, dy = (X - fx * W) / fw, (Y - fy * H - warp * 0.4) / fh
        d2 = dx * dx + dy * dy
        blob = np.exp(-d2 * 2.2)
        rings = 0.5 + 0.5 * np.cos(np.sqrt(d2) * 14)
        tone += blob * (0.55 + 0.45 * rings)
    lum -= tone * 7.0
    for kx, ky, ks in knots:                                           # pin knots: tiny dark eyes with a soft halo
        dx, dy = (X - kx) / ks, (Y - ky) / (ks * 0.55)
        d2 = dx * dx + dy * dy
        lum -= np.exp(-d2 * 1.6) * 30 + np.exp(-d2 * 0.08) * 6
    col = np.array(rgb(PLY), np.float32)[None, None, :]
    dark, light = np.array([150, 108, 58], np.float32), np.array([228, 200, 152], np.float32)
    t = np.clip(lum / 34, -1, 1)[..., None]
    col = np.where(t < 0, col + (dark - col) * -t, col + (light - col) * t)
    # very slight colour drift: warmer / greyer regions
    drift = smooth_noise(W, H, 500, seed + 8)[..., None]
    col = col + drift * np.array([4, 1, -4], np.float32)
    return Image.fromarray(np.clip(col + 0.5, 0, 255).astype(np.uint8), "RGB")

def make_bracket(corner, seed=71):
    """Steel L corner bracket, 44x44 shown, outer corner at the panel corner. Drawn for top-left, then mirrored."""
    F = 8; S = 44 * F; T = 13.2 * F                          # arm width = 30% (mockup clip-path)
    m = Image.new("L", (S, S), 0); d = ImageDraw.Draw(m)
    d.polygon([(0, 0), (S, 0), (S, T), (T, T), (T, S), (0, S)], fill=255)
    soft = np.asarray(m.filter(ImageFilter.GaussianBlur(0.5 * F)), np.float32) / 255
    mask = np.clip((soft - 0.5) * 3 + 0.5, 0, 1)
    X, Y = np.meshgrid(np.arange(S, dtype=np.float32), np.arange(S, dtype=np.float32))
    a, b = np.array(rgb("#6d7378"), np.float32), np.array(rgb("#3d4246"), np.float32)
    g = np.clip((X + Y) / (2 * S) * 1.6, 0, 1)[..., None]   # 135deg light -> dark
    col = a + (b - a) * g
    # brushing: fine streaks along each arm (horizontal on the top arm, vertical on the left one)
    bh = aniso_noise(S, S, 60 * F, 0.35 * F, seed) * 9 + aniso_noise(S, S, 14 * F, 0.25 * F, seed + 1) * 5
    bv = aniso_noise(S, S, 0.35 * F, 60 * F, seed + 2) * 9 + aniso_noise(S, S, 0.25 * F, 14 * F, seed + 3) * 5
    on_top = (Y < T) & (Y <= X)
    col += np.where(on_top, bh, bv)[..., None]
    # bevel: light along the top/left faces of the plate edges, dark along the bottom/right ones
    er = np.asarray(m.filter(ImageFilter.MinFilter(int(1.6 * F) | 1)), np.float32) / 255
    rim = np.clip(mask - er, 0, 1)
    gy, gx = np.gradient(np.asarray(m.filter(ImageFilter.GaussianBlur(1.0 * F)), np.float32))
    light = np.clip((gx + gy) / (np.abs(gx) + np.abs(gy) + 1e-3), -1, 1)   # +: edge faces up-left
    col += (rim * light * 38)[..., None]
    col -= (rim * (light < 0) * 10)[..., None]
    # rivets: domed heads, dark ring, a highlight up-left, a small shadow down-right
    out = np.zeros((S, S, 4), np.float32); out[..., :3] = col; out[..., 3] = mask
    def rivet(cx, cy):
        rr = 3.4 * F
        dd = np.sqrt((X - cx) ** 2 + (Y - cy) ** 2) / rr
        sh = np.sqrt((X - cx - 0.8 * F) ** 2 + (Y - cy - 0.9 * F) ** 2) / rr
        out[..., :3] *= (1 - np.clip(1.25 - sh, 0, 1) * 0.45 * (dd > 0.9))[..., None]
        head = np.clip((1 - dd) * 6, 0, 1)
        hl = np.clip(1 - np.sqrt((X - cx + 1.1 * F) ** 2 + (Y - cy + 1.1 * F) ** 2) / (2.0 * F), 0, 1)
        hc = np.array(rgb("#a7adb2"), np.float32) * (0.78 + 0.22 * (1 - dd))[..., None] + (hl ** 1.5 * 60)[..., None]
        ring = np.clip(1 - np.abs(dd - 0.95) * 8, 0, 1)
        hc = hc * (1 - ring * 0.5)[..., None]
        out[..., :3] = out[..., :3] * (1 - head[..., None]) + hc * head[..., None]
    rivet(30.5 * F, 6.6 * F); rivet(6.6 * F, 30.5 * F)
    out[..., :3] = np.clip(out[..., :3], 0, 255); out[..., 3] *= 255
    img = Image.fromarray(out.astype(np.uint8), "RGBA")
    if corner in ("tr", "br"): img = img.transpose(Image.FLIP_LEFT_RIGHT)
    if corner in ("bl", "br"): img = img.transpose(Image.FLIP_TOP_BOTTOM)
    return down(img, (88, 88))

def make_stamp_issued(seed=81):
    F = 8; W, H = 60 * F, 22 * F
    m = Image.new("L", (W, H), 0); d = ImageDraw.Draw(m)
    bw, bh, t = 50 * F, 13.5 * F, 2 * F
    x0, y0 = (W - bw) / 2, (H - bh) / 2
    d.rectangle([x0, y0, x0 + bw, y0 + bh], outline=255, width=t)
    f = font(9.8 * F, "Bold")
    text, sp = "ISSUED", 1.3 * F
    widths = [f.getlength(c) for c in text]
    total = sum(widths) + sp * (len(text) - 1)
    l, tp, rr, bt = f.getbbox("ISSUED")
    x = (W - total) / 2; ty = y0 + (bh - (bt - tp)) / 2 - tp
    for c, cw in zip(text, widths):
        d.text((x, ty), c, font=f, fill=255); x += cw + sp
    mask = np.asarray(m, np.float32) / 255
    a, rim = ink_texture(mask, seed, density=0.95, holes=50, hole_r=(0.5, 1.4))
    # at 60x22 every stroke is "rim" and the MODDED misses eat whole letters: keep a floor of ink so it stays legible
    floor = np.asarray(Image.fromarray((mask * 255).astype(np.uint8)).filter(ImageFilter.GaussianBlur(0.4 * F)), np.float32) / 255
    a = np.maximum(a, floor * (0.62 + smooth_noise(W, H, 6 * F, seed + 9) * 0.12))
    col = np.zeros((H, W, 4), np.float32)
    red, dark = np.array(rgb(STAMP_RED), np.float32), np.array([150, 34, 26], np.float32)
    col[..., :3] = red + (dark - red) * np.clip(rim * 0.5, 0, 1)[..., None]
    col[..., 3] = a * 255
    img = Image.fromarray(col.astype(np.uint8), "RGBA")
    img = img.rotate(8, resample=Image.BICUBIC, center=(W / 2, H / 2))   # CSS rotate(-8deg): right end up
    return down(img, (120, 44))

def make_up(seed=91):
    F = 8; W = H = 12 * F
    m = Image.new("L", (W, H), 0)
    ImageDraw.Draw(m).polygon([(6 * F, 1.2 * F), (11.2 * F, 10.6 * F), (0.8 * F, 10.6 * F)], fill=255)
    s = np.asarray(m.filter(ImageFilter.GaussianBlur(0.35 * F)), np.float32) / 255
    s = s + smooth_noise(W, H, 2.5 * F, seed) * 0.05
    return down(colour_alpha(WOOD_INK, np.clip((s - 0.45) * 8, 0, 1)), (24, 24))

def make_tile(W=192, H=96, seed=101):
    """Olive drab oilcloth: 160deg shade, faint canvas weave, 1 px darker rim, 1 px light top edge."""
    X, Y = np.meshgrid(np.arange(W, dtype=np.float32), np.arange(H, dtype=np.float32))
    ang = math.radians(160)                                  # CSS: 0deg = up, clockwise; 160 = toward bottom, a bit right
    dx, dy = math.sin(ang), -math.cos(ang)
    p = (X / W - 0.5) * dx * (W / H) + (Y / H - 0.5) * dy
    t = np.clip((p - p.min()) / (p.max() - p.min()), 0, 1)[..., None]
    a, b = np.array(rgb(OLIVE_A), np.float32), np.array(rgb(OLIVE_B), np.float32)
    col = a + (b - a) * t
    weave = (np.sin(X * math.pi / 1.5) * 0.5 + np.sin(Y * math.pi / 1.5) * 0.5) * 1.6   # 3 px period threads
    col += (weave + smooth_noise(W, H, 2, seed) * 1.6 + smooth_noise(W, H, 30, seed + 1) * 2.4)[..., None]
    col[0, :, :] += 20                                         # light top edge (inset 0 2px rgba(255,255,255,.08))
    col[1, :, :] += 7
    rim = np.zeros((H, W), bool); rim[0, :] = rim[-1, :] = True; rim[:, 0] = rim[:, -1] = True
    col[rim] = col[rim] * 0.5                                  # inset 0 0 0 1px rgba(0,0,0,.5)
    return Image.fromarray(np.clip(col + 0.5, 0, 255).astype(np.uint8), "RGB").convert("RGBA")

def make_shade(W=4, H=28):
    y = (np.arange(H, dtype=np.float32) + 0.5) / H
    a = 0.65 * (1 - y) ** 1.6
    return colour_alpha("#000000", np.repeat(a[:, None], W, 1))

def crate_preview(ply, shadow, brk, stamp, up, tile, shade):
    """A full inventory window as the mockup lays it out (1100x760, crate 1008x668 at 46,46), assets at 1x."""
    W, H = 1100, 760
    xs, ys = np.meshgrid(np.linspace(0, 1, W), np.linspace(0, 1, H))
    t = np.clip(np.sqrt(((xs - 0.3) / 0.7) ** 2 + ((ys - 0.2) / 0.7) ** 2) / 0.7, 0, 1)[..., None]
    bg = np.array([0x3d, 0x55, 0x66], np.float32) * (1 - t) + np.array([0x1b, 0x27, 0x32], np.float32) * t
    img = Image.fromarray(bg.astype(np.uint8), "RGB").convert("RGBA")
    half = lambda im: im.resize((max(1, im.size[0] // 2), max(1, im.size[1] // 2)), Image.LANCZOS)
    def over(im, xy): img.alpha_composite(im, (int(xy[0]), int(xy[1])))
    def fill(box, c):                                        # box x, y, w, h; c "#rrggbb" or rgba tuple
        x, y, w, h = box
        if isinstance(c, str): c = rgb(c) + (255,)
        over(Image.new("RGBA", (int(w), int(h)), c), (x, y))
    def frame(box, c, th=1):
        x, y, w, h = box
        fill((x, y, w, th), c); fill((x, y + h - th, w, th), c); fill((x, y, th, h), c); fill((x + w - th, y, th, h), c)
    def tx(xy, s, size, c, w="Regular", sp=0, anchor="la"):
        d = ImageDraw.Draw(img); f = font(size, w)
        if isinstance(c, str): c = rgb(c) + (255,)
        if not sp: d.text(xy, s, font=f, fill=c, anchor=anchor); return
        if anchor[0] in "mr":                                   # letter-spaced, centred / right: measure first
            tw = sum(f.getlength(ch) for ch in s) + sp * (len(s) - 1)
            xy = (xy[0] - (tw / 2 if anchor[0] == "m" else tw), xy[1]); anchor = "l" + anchor[1]
        x0 = xy[0]
        lay = Image.new("RGBA", img.size, (0, 0, 0, 0)); ld = ImageDraw.Draw(lay)
        for ch in s: ld.text((x0, xy[1]), ch, font=f, fill=c, anchor=anchor); x0 += f.getlength(ch) + sp
        img.alpha_composite(lay)
    PX, PY, PW, PH = 46, 46, 1008, 668
    over(shadow.resize((PW + 48, PH + 48), Image.BILINEAR), (PX - 24, PY - 24))
    over(ply.convert("RGBA").resize((PW, PH), Image.LANCZOS), (PX, PY))
    for i, c in enumerate(("#7a5a30", "#e0c495", "#9c7848", "#e4cb9c", "#8a6a3e")):   # ply edge, outside in
        frame((PX + i, PY + i, PW - 2 * i, PH - 2 * i), c)
    sx, sy = PX + 559, PY + 344                               # seams (under everything else)
    fill((sx, PY + 5, 1, PH - 10), (70, 45, 20, 140)); fill((sx + 1, PY + 5, 1, PH - 10), (255, 240, 210, 64))
    fill((PX + 5, sy, PW - 10, 1), (70, 45, 20, 140)); fill((PX + 5, sy + 1, PW - 10, 1), (255, 240, 210, 64))
    for k, (bx, by) in zip(("tl", "tr", "bl", "br"), ((PX, PY), (PX + PW - 44, PY), (PX, PY + PH - 44), (PX + PW - 44, PY + PH - 44))):
        over(half(brk[k]), (bx, by))
    INKC = (45, 30, 15, 128)
    # stencils start right of the brackets' 13 px arms (at x+30 the top arm would cut through "U.S.N.")
    tx((PX + 52, PY + 8), "U.S.N.  ·  ORDNANCE  ·  HANDLE WITH CARE", 13, INKC, "SemiBold", sp=4.5)
    a = np.asarray(half(up)).copy(); a[..., 3] = a[..., 3] // 2; upi = Image.fromarray(a)
    ux = PX + PW - 150; sw = sum(font(13, "SemiBold").getlength(c) for c in "THIS SIDE UP") + 4.5 * 11
    over(upi, (ux - 12, PY + 12)); tx((ux - 22, PY + 8), "THIS SIDE UP", 13, INKC, "SemiBold", sp=4.5, anchor="ra")
    over(upi, (ux - 32 - sw - 12, PY + 12))
    # manifest label
    lx, ly = PX + 30, PY + 34
    fill((lx + 1, ly + 2, 330, 46), (60, 40, 20, 150)); fill((lx, ly, 330, 46), PAPER)
    tx((lx + 14, ly + 9), "I N V E N T O R Y", 22, INK, "SemiBold")
    tx((lx + 222, ly + 10), "MANIFEST", 10, MUTED, sp=2); tx((lx + 222, ly + 23), "WEAPON MOD v0.9.2", 10, MUTED, sp=1.2)
    fill((lx + 6, ly + 4, 10, 2), "#8d969e"); fill((lx + 330 - 16, ly + 4, 10, 2), "#8d969e")
    def tag(box, label, filled=False, size=12, red=False):
        x, y, w, h = box
        if not filled: fill((x + 1, y + 2, w, h), (40, 28, 14, 110))
        fill(box, INK if filled else PAPER); frame(box, "#b3261e" if red else INK, 2)
        tx((x + w / 2, y + h / 2 + 1), label, size, PAPER if filled else INK, "SemiBold" if filled else "Regular", sp=1.5, anchor="mm")
    tag((PX + PW - 30 - 104, PY + 42, 104, 32), "CLOSE [I]")
    # section headers
    gx, gy = PX + 24, PY + 122; lx2 = PX + 576
    tx((gx, PY + 98), "YOUR GUNS", 15, INK, "SemiBold", sp=3); tx((gx + 520, PY + 100), "22 / 60 CELLS", 12, MUTED, sp=1, anchor="ra")
    tx((lx2, PY + 98), "LOOT", 15, INK, "SemiBold", sp=3); tx((lx2 + 416 - 104, PY + 100), "3 FOUND", 12, MUTED, sp=1, anchor="ra")
    tag((lx2 + 416 - 94, PY + 96, 94, 22), "SCRAP ALL", size=11)
    RC = {"COMMON": "#d8d8d8", "UNCOMMON": "#4fc3f7", "RARE": "#ffb300", "UNIQUE": "#ff7a1a"}
    def gun(box):                                            # a stand-in sprite (the game draws the real one)
        x, y, w, h = box
        pts = [(4, 26), (58, 22), (96, 21), (96, 25), (60, 27), (44, 34), (30, 34), (26, 29), (4, 30)]
        s = min(w * 0.8 / 100, h * 0.6 / 40); ox, oy = x + w / 2 - 50 * s, y + h * 0.48 - 23 * s
        d = ImageDraw.Draw(img)
        d.polygon([(ox + px * s, oy + py * s) for px, py in pts], fill="#9aa3ab", outline="#2a2f35")
        d.polygon([(ox + px * s, oy + py * s) for px, py in ((34, 22), (40, 12), (48, 12), (50, 22))], fill="#9aa3ab", outline="#2a2f35")
    def item(ox, oy, c, r, w, h, rar, uses, issued):
        x, y, iw, ih = ox + c * 52 + 1, oy + r * 52 + 1, w * 52 - 2, h * 52 - 2
        over(tile.resize((iw, ih), Image.BILINEAR), (x, y)); gun((x, y, iw, ih))
        tw = {"COMMON": 59, "UNCOMMON": 72, "RARE": 44, "UNIQUE": 54}[rar] if iw >= 80 else 16   # 21 + word
        fill((x + 5, y + 5, tw, 14), (0, 0, 0, 120)); fill((x + 4, y + 4, tw, 14), PAPER); fill((x + 4, y + 4, 4, 14), RC[rar])
        ImageDraw.Draw(img).ellipse([x + 10, y + 9, x + 14, y + 13], fill="#22170d")
        if iw >= 80: tx((x + 17, y + 11), rar, 8, INK, "Bold", sp=1, anchor="lm")
        tx((x + 6, y + ih - 4), str(uses), 14, "#f3e6c8", "SemiBold", anchor="ls")
        if issued: over(half(stamp), (x + iw - 64, y + ih - 26))
    def grid(box, cols, rows):
        x, y, w, h = box
        fill(box, "#22170d")
        for i in range(1, cols): fill((x + i * 52, y, 1, h), (243, 230, 200, 23))
        for j in range(1, rows): fill((x, y + j * 52, w, 1), (243, 230, 200, 23))
        over(half(shade).resize((w, 14), Image.BILINEAR), (x, y)); frame(box, "#6b4e2a")
    grid((gx, gy, 520, 312), 10, 6); grid((lx2, gy, 416, 416), 8, 8)
    for g in ((0, 0, 3, 2, "RARE", 6, 1), (3, 0, 3, 1, "UNCOMMON", 4, 1), (3, 1, 2, 1, "COMMON", 3, 0), (6, 0, 4, 2, "UNIQUE", 9, 1), (0, 2, 3, 1, "UNCOMMON", 5, 0), (0, 3, 4, 1, "COMMON", 2, 0)):
        item(gx, gy, *g)
    for g in ((0, 0, 3, 1, "COMMON", 4, 0), (3, 0, 2, 1, "UNCOMMON", 5, 0), (0, 1, 3, 2, "RARE", 7, 0)):
        item(lx2, gy, *g)
    ImageDraw.Draw(img).rectangle([gx + 3 * 52 + 1, gy + 1 * 52 + 1, gx + 5 * 52 - 2, gy + 2 * 52 - 2], outline=(243, 230, 200, 180))   # hover
    # scrap zone
    zx, zy, zw, zh = lx2, PY + 548, 416, 46
    hz = Image.new("RGBA", (zw, zh)); hd = ImageDraw.Draw(hz)
    for k in range(-zh, zw + zh, 18): hd.polygon([(k, zh), (k + 9, zh), (k + 9 + zh, 0), (k + zh, 0)], fill="#e0a222")
    bgz = Image.new("RGBA", (zw, zh), "#1e150c"); bgz.alpha_composite(hz)
    inner = Image.new("L", (zw, zh), 255); ImageDraw.Draw(inner).rectangle([5, 5, zw - 6, zh - 6], fill=0)
    over(Image.composite(bgz, Image.new("RGBA", (zw, zh), (0, 0, 0, 0)), inner), (zx, zy))
    fill((zx + 5, zy + 5, zw - 10, zh - 10), (34, 23, 13, 217))
    tx((zx + zw / 2, zy + zh / 2 + 1), "DROP HERE TO SCRAP", 13, "#f0c050", "SemiBold", sp=4, anchor="mm")
    # workshop slips
    for i, (nm, sw, amt, cr, cf) in enumerate((("SCRAP IRON", "#9aa3ab", 12, "2 crates", "CRAFT 5"), ("BRASS", "#c9a24a", 4, "0 crates", "CRAFT 3"), ("GUNMETAL", "#5a6570", 1, "1 crate", "CRAFT 2"))):
        x, y = gx, PY + 452 + i * 52
        fill((x + 1, y + 2, 520, 44), (40, 28, 14, 120)); fill((x, y, 520, 44), PAPER)
        fill((x + 12, y + 9, 26, 26), sw); frame((x + 12, y + 9, 26, 26), (0, 0, 0, 90), 2)
        tx((x + 48, y + 6), nm, 11, MUTED, sp=2)
        tx((x + 48, y + 18), str(amt), 18, INK, "SemiBold"); tx((x + 48 + 12 * len(str(amt)) + 4, y + 24), "·  " + cr, 12, MUTED)
        tag((x + 520 - 12 - 60 - 8 - 70, y + 9, 70, 26), cf, size=11)
        tag((x + 520 - 12 - 60, y + 9, 60, 26), "OPEN", filled=True, size=11)
    # footer + bottom stencil
    fill((PX + 24, PY + 606, 960, 28), (232, 220, 192, 224))
    tx((PX + 36, PY + 620), "NUMBER = MISSIONS LEFT  ·  CLICK A WEAPON SLOT TO EQUIP  ·  DRAG LOOT INTO YOUR GUNS TO KEEP IT", 11, "#4a4232", sp=1, anchor="lm")
    tx((PX + 52, PY + 641), "LOT 0.9.2  ·  CONTENTS: SMALL ARMS AND ORDNANCE  ·  KEEP DRY", 12, (30, 22, 14, 128), "SemiBold", sp=4)
    return img.convert("RGB")

def crate_assets(shadow):
    ply = save(make_ply(), "ui_ply.png")
    brk = {k: save(make_bracket(k), "ui_bracket_%s.png" % k) for k in ("tl", "tr", "bl", "br")}
    stamp = save(make_stamp_issued(), "ui_stamp_issued.png")
    up = save(make_up(), "ui_up.png")
    tile = save(make_tile(), "ui_tile.png")
    shade = save(make_shade(), "ui_shade.png")
    p = os.path.join(HERE, "crate_preview.png")
    crate_preview(ply, shadow, brk, stamp, up, tile, shade).save(p, optimize=True)
    print("wrote tools\\crate_preview.png          %dx%d %8d bytes" % (1100, 760, os.path.getsize(p)))

if __name__ == "__main__":
    paper = save(make_paper(), "ui_paper.png")
    shadow = save(make_shadow(), "ui_shadow.png")
    stamp = save(make_stamp(), "ui_stamp_modded.png")
    card = save(make_notecard(), "ui_notecard.png")
    x = save(make_x(), "ui_x.png")
    # the close box X in ink: same strokes and alpha, flat #2b2a26 (tinting the blue X by ink comes out near black)
    xk = np.asarray(x).copy(); xk[..., :3] = rgb(INK); save(Image.fromarray(xk, "RGBA"), "ui_x_ink.png")
    al = save(make_arrow(True), "ui_arrow_l.png")
    ar = save(make_arrow(False), "ui_arrow_r.png")
    dot = save(make_dot(), "ui_dot.png")
    p = os.path.join(HERE, "ui_assets_preview.png")
    preview(paper, shadow, stamp, card, x, al, ar, dot).save(p, optimize=True)
    print("wrote tools\\ui_assets_preview.png      %dx%d" % (1040, 700))
    crate_assets(shadow)
