# Draws models/modded_banner.png: "MODDED" in blood red with drips running off the letters, for the title screen
# (David 2026-10-31: "a dripping red 'modded' thing on the main menu", then "thinner font plz, like almost a pencil").
# Needs Pillow. The look is seeded, so a rerun gives the same picture; change SEED for other drips.
#   python make_modded_banner.py              the banner, in FONT
#   python make_modded_banner.py --compare    modded_banner_compare.png: the thin fonts side by side, to choose from
import math, os, random, sys
from PIL import Image, ImageDraw, ImageFilter, ImageFont, ImageChops

SEED = 7
TEXT = "MODDED"
FONT, VARIATION = r"C:\Windows\Fonts\segoeuil.ttf", None   # Segoe UI Light
SIZE = 210                                                 # font size in pixels
TOP, BOTTOM = (255, 48, 32), (120, 0, 0)                   # letter color, top to bottom
HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "..", "models", "modded_banner.png")
CANDIDATES = [("Segoe UI Light", r"C:\Windows\Fonts\segoeuil.ttf", None), ("Bahnschrift Light", r"C:\Windows\Fonts\bahnschrift.ttf", b"Light"),
              ("Segoe Print", r"C:\Windows\Fonts\segoepr.ttf", None), ("Ink Free", r"C:\Windows\Fonts\Inkfree.ttf", None),
              ("Calibri Light", r"C:\Windows\Fonts\calibril.ttf", None), ("Impact (first one)", r"C:\Windows\Fonts\impact.ttf", None)]

def make(font_path, variation=None, seed=SEED):
    rnd = random.Random(seed)
    font = ImageFont.truetype(font_path, SIZE)
    if variation: font.set_variation_by_name(variation)
    l, t, r, b = font.getbbox(TEXT)
    pad = 30
    W, H = r - l + 2 * pad, (b - t) + 2 * pad + 190        # room below for the drips
    ox, oy = pad - l, pad - t
    mask = Image.new("L", (W, H), 0)                        # the letters
    ImageDraw.Draw(mask).text((ox, oy), TEXT, font=font, fill=255)
    px = mask.load()
    base = oy + font.getmetrics()[0]                        # the baseline row (text is drawn from the ascender line)
    def stem(x):                                            # width of the stroke at x, just above the baseline
        if px[x, base - 6] <= 128: return 0
        a = x; z = x
        while a > 0 and px[a - 1, base - 6] > 128: a -= 1
        while z < W - 1 and px[z + 1, base - 6] > 128: z += 1
        return z - a + 1
    # drips start where a letter's lower edge sits on the baseline (not on the O, whose curve would make it a Q)
    o = TEXT.index("O"); o0, o1 = ox + font.getlength(TEXT[:o]), ox + font.getlength(TEXT[:o + 1])
    cols = [x for x in range(W) if px[x, base - 3] > 128 and px[x, base - 14] > 128 and not (o0 - 4 <= x <= o1 + 4)]
    stroke = sorted(stem(x) for x in cols)[len(cols) // 2] if cols else 8
    drips, taken = [], []
    for _ in range(400):
        if len(drips) >= 14 or not cols: break
        x = rnd.choice(cols)
        if any(abs(x - u) < 26 for u in taken): continue
        taken.append(x)
        w0 = max(3.0, min(15.0, stroke * rnd.uniform(0.7, 1.05)))
        length = rnd.choice([rnd.uniform(14, 34), rnd.uniform(40, 90), rnd.uniform(90, 170)])
        drips.append((x, w0, length))
    d = ImageDraw.Draw(mask)
    phase = rnd.uniform(0, 6.28); amp = min(1.0, stroke / 14.0) ** 2
    for x in range(W):                                      # the bottom edges ooze a little: a wavy sag under each stroke
        if px[x, base - 3] > 128:
            sag = amp * (2 + 3.5 * (1 + math.sin(x / 7.0 + phase)) + 2 * math.sin(x / 2.9))
            d.line([(x, base - 2), (x, base + max(0, sag))], fill=255)
    for x, w0, length in drips:
        w1 = w0 * 0.6
        rd = w1 * 0.75 + max(1.5, w0 * 0.25)                # the drop at the end, a little wider than the stem
        end_y = base + length
        d.polygon([(x - w0 / 2, base - 8), (x + w0 / 2, base - 8), (x + w1 / 2, end_y - rd), (x - w1 / 2, end_y - rd)], fill=255)
        sh = w0 * (0.35 if stroke >= 12 else 0.15) + 1
        d.ellipse([x - w0 / 2 - sh, base - 4, x + w0 / 2 + sh, base + 3 + sh], fill=255)   # the shoulder where it leaves the letter
        d.ellipse([x - rd, end_y - rd * 1.9, x + rd, end_y + rd * 0.7], fill=255)            # a teardrop: taller than wide
    mask = mask.filter(ImageFilter.GaussianBlur(1.0 if stroke < 12 else 1.6)).point(lambda v: 255 if v > 110 else int(v * 255 / 110))

    # color: a top-to-bottom red gradient, darker as the drips run down, a glossy band near the top of the letters
    grad = Image.new("RGB", (W, H)); gp = grad.load()
    for y in range(H):
        f = min(1.0, max(0.0, (y - oy) / max(1, base - oy)))
        if y > base: f = 1.0 + min(0.6, (y - base) / 300.0)
        c = tuple(max(0, int(TOP[i] + (BOTTOM[i] - TOP[i]) * min(f, 1.0) - (40 * (f - 1.0) if f > 1 else 0))) for i in range(3))
        for x in range(W): gp[x, y] = c
    gloss = Image.new("L", (W, H), 0); gd = ImageDraw.Draw(gloss)
    gd.rectangle([0, oy + 8, W, oy + int((base - oy) * 0.28)], fill=95)
    for x, w0, length in drips: gd.rectangle([x - w0 * 0.3, base + 6, x - w0 * 0.12, base + length * 0.8], fill=110)
    gloss = ImageChops.multiply(gloss.filter(ImageFilter.GaussianBlur(3)), mask)
    color = Image.composite(Image.new("RGB", (W, H), (255, 150, 140)), grad, gloss)

    # a dark edge and a soft shadow, so it reads on any background (thinner for thin letters)
    edge = mask.filter(ImageFilter.MaxFilter(3 if stroke < 12 else 9))
    shadow = edge.filter(ImageFilter.GaussianBlur(5 if stroke < 12 else 7)).point(lambda v: int(v * 0.65))
    out = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    sha = Image.new("RGBA", (W, H), (0, 0, 0, 255)); sha.putalpha(shadow)
    out = Image.alpha_composite(out, ImageChops.offset(sha, 4, 6))
    ed = Image.new("RGBA", (W, H), (35, 0, 0, 255)); ed.putalpha(edge)
    out = Image.alpha_composite(out, ed)
    fg = color.convert("RGBA"); fg.putalpha(mask)
    out = Image.alpha_composite(out, fg)
    shine = Image.new("RGBA", (W, H), (0, 0, 0, 0)); hd = ImageDraw.Draw(shine)
    for x, w0, length in drips:                             # a small soft shine on each drop
        rd = w0 * 0.6 * 0.75 + max(1.5, w0 * 0.25)
        cx, cy = x - rd * 0.38, base + length - rd * 0.55
        hd.ellipse([cx - rd * 0.28, cy - rd * 0.4, cx + rd * 0.22, cy + rd * 0.3], fill=(255, 200, 190, 150))
    out = Image.alpha_composite(out, shine.filter(ImageFilter.GaussianBlur(0.8)))
    return out.crop(out.getbbox())

def on(img, bg, margin=20):
    p = Image.new("RGBA", (img.width + 2 * margin, img.height + 2 * margin), bg + (255,))
    p.alpha_composite(img, (margin, margin))
    return p

if "--compare" in sys.argv:
    tiles = []
    label = ImageFont.truetype(r"C:\Windows\Fonts\segoeui.ttf", 26)
    for name, path, var in CANDIDATES:
        im = on(make(path, var), (24, 30, 38))
        tile = Image.new("RGBA", (im.width, im.height + 40), (24, 30, 38, 255))
        tile.alpha_composite(im, (0, 40)); ImageDraw.Draw(tile).text((20, 6), str(len(tiles) + 1) + ". " + name, font=label, fill=(220, 225, 230))
        tiles.append(tile)
    cw = max(t.width for t in tiles); cols = 2; rows = (len(tiles) + 1) // 2
    rh = [max(t.height for t in tiles[i * cols:(i + 1) * cols]) for i in range(rows)]
    sheet = Image.new("RGB", (cw * cols, sum(rh)), (24, 30, 38))
    for i, t in enumerate(tiles): sheet.paste(t, ((i % cols) * cw, sum(rh[:i // cols])))
    sheet.save(os.path.join(HERE, "modded_banner_compare.png"))
    print("wrote modded_banner_compare.png", sheet.size)
else:
    out = make(FONT, VARIATION)
    out.save(OUT)
    print("wrote", os.path.normpath(OUT), out.size)
    for name, bg in (("dark", (24, 30, 38)), ("light", (190, 200, 210))):    # previews next to this script
        on(out, bg).convert("RGB").save(os.path.join(HERE, "modded_banner_preview_%s.png" % name))
