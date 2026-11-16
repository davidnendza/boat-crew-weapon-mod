"""Draws the 0.9.6 release banner for Discord: release/banner-0.9.6.png (1600x900).

The 0.9.5 desk again, now with a strip of newsreel film (real frames from the mod's GIFs), a crew file card with an
earned nickname and a pinned photo of the Flak 88 model. Pillow only; Windows fonts.
    python tools/make_banner_096.py <frame.png> <frame.png> <frame.png> <flak88 photo.png>
"""
import os, random, sys
from PIL import Image, ImageDraw, ImageEnhance, ImageFilter, ImageFont, ImageOps

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = os.path.join(ROOT, 'release', 'banner-0.9.6.png')
FRAMES, FLAK = sys.argv[1:4], sys.argv[4]
W, H = 1600, 900
F = r'C:\Windows\Fonts'
def font(name, size): return ImageFont.truetype(os.path.join(F, name), size)
TYPE, TYPEB = 'cour.ttf', 'courbd.ttf'
random.seed(96)
INK, RED, PAPER = (43, 42, 38), (179, 38, 30), (232, 220, 192)


def paper(w, h, tint=PAPER):
    tex = Image.open(os.path.join(ROOT, 'models', 'ui_paper.png')).convert('RGB')
    if tex.width < w or tex.height < h: tex = tex.resize((max(w, tex.width), max(h, tex.height)))
    x = random.randint(0, tex.width - w); y = random.randint(0, tex.height - h)
    return Image.blend(tex.crop((x, y, x + w, y + h)), Image.new('RGB', (w, h), tint), 0.35).convert('RGBA')


def place(base, card, cx, cy, angle, shadow=18):
    r = card.rotate(angle, resample=Image.BICUBIC, expand=True)
    sh = Image.new('RGBA', r.size, (0, 0, 0, 0))
    sh.putalpha(r.getchannel('A').point(lambda a: int(a * 0.55)))
    sh = sh.filter(ImageFilter.GaussianBlur(shadow / 2))
    x, y = int(cx - r.width / 2), int(cy - r.height / 2)
    base.alpha_composite(sh, (x + shadow // 2, y + shadow))
    base.alpha_composite(r, (x, y))


def stamp(text, size, color=RED, pad=12, border=5):
    f = font('bahnschrift.ttf', size)
    im = Image.new('RGBA', (int(f.getlength(text)) + 2 * pad, size + 2 * pad), (0, 0, 0, 0))
    d = ImageDraw.Draw(im)
    d.rounded_rectangle((border // 2, border // 2, im.width - border // 2 - 1, im.height - border // 2 - 1), 8, outline=color + (230,), width=border)
    d.text((pad, pad - size // 8), text, font=f, fill=color + (235,))
    px = im.load()
    for _ in range(im.width * im.height // 9):                          # worn ink
        x, y = random.randrange(im.width), random.randrange(im.height)
        r, g, b, a = px[x, y]
        if a: px[x, y] = (r, g, b, int(a * random.uniform(0.2, 0.8)))
    return im


def pin(img, x, y):
    p = Image.new('RGBA', (34, 34), (0, 0, 0, 0))
    ImageDraw.Draw(p).ellipse((2, 2, 31, 31), fill=(170, 30, 30, 255), outline=(90, 10, 10, 255), width=2)
    img.alpha_composite(p, (x, y))


# ---- the desk (as 0.9.5) ----
img = Image.new('RGBA', (W, H), (16, 24, 34, 255))
g = Image.new('L', (W, H)); gd = ImageDraw.Draw(g)
for r in range(0, 900, 6):
    gd.ellipse((1100 - r * 1.5, 430 - r, 1100 + r * 1.5, 430 + r), fill=max(0, 120 - r // 6))
img = Image.composite(Image.new('RGBA', (W, H), (88, 70, 46, 255)), img, g.filter(ImageFilter.GaussianBlur(60)))
noise = Image.effect_noise((W, H), 18).convert('L')
img = Image.blend(img, Image.merge('RGBA', (noise, noise, noise, Image.new('L', (W, H), 255))), 0.06)

# ---- the film strip: three newsreel frames, sprocket holes down both sides ----
fw, fh, side, gap = 440, 248, 46, 26
strip = Image.new('RGBA', (fw + 2 * side, 3 * (fh + gap) + gap), (14, 13, 12, 255))
sd = ImageDraw.Draw(strip)
for y in range(10, strip.height - 20, 30):
    for x in (14, strip.width - 14 - 18):
        sd.rounded_rectangle((x, y, x + 18, y + 14), 3, fill=(205, 200, 188, 255))
for i, f in enumerate(FRAMES):
    fr = ImageOps.fit(Image.open(f).convert('L'), (fw, fh), centering=(0.5, 0.4))
    fr = ImageEnhance.Contrast(fr).enhance(1.15).convert('RGBA')
    strip.paste(fr, (side, gap + i * (fh + gap)))
sd.text((side, 4), 'NEWSREEL  .  BOAT CREW  .  1943', font=font(TYPEB, 13), fill=(190, 160, 90, 255))
place(img, strip, 1000, 452, 4, 26)

# ---- the Flak 88, a pinned photo ----
fl = Image.open(FLAK).convert('L')
fl = ImageOps.autocontrast(fl, cutoff=1)
fl = ImageOps.fit(fl, (330, 230), centering=(0.5, 0.5)).convert('RGB')
fl = ImageOps.colorize(fl.convert('L'), (40, 34, 26), (236, 226, 204))
photo = Image.new('RGBA', (fl.width + 30, fl.height + 76), (238, 233, 220, 255))
photo.paste(fl, (15, 15))
pd = ImageDraw.Draw(photo)
pd.text((20, fl.height + 24), '8.8 cm Flak 36', font=font(TYPEB, 22), fill=INK)
pd.text((20, fl.height + 48), 'asked for on Discord', font=font(TYPE, 17), fill=INK)
place(img, photo, 1405, 250, -5, 22)
pin(img, 1388, 96)

# ---- the crew file card with an earned nickname ----
card = paper(420, 280, (240, 234, 216))
cd = ImageDraw.Draw(card)
cd.text((24, 22), 'THOMASON, GEORGE', font=font(TYPEB, 24), fill=INK)
cd.text((24, 58), 'Ammunition party', font=font(TYPE, 18), fill=INK)
cd.line((24, 90, 396, 90), fill=INK, width=2)
cd.text((24, 104), 'Known aboard as', font=font(TYPE, 18), fill=INK)
cd.text((24, 130), '"JONAH"', font=font(TYPEB, 44), fill=INK)
y = 188
for ln in ('for being lost at sea', 'and picked up again.'):
    cd.text((24, y), ln, font=font(TYPE, 18), fill=INK); y += 24
card.alpha_composite(stamp('KEPT', 34).rotate(-10, expand=True), (268, 120))
place(img, card, 1400, 680, 3)

# ---- the title (as 0.9.5) ----
shade = Image.new('RGBA', (W, H), (0, 0, 0, 0)); sh = ImageDraw.Draw(shade)
for x in range(720):
    sh.line((x, 0, x, H), fill=(10, 14, 20, int(235 * max(0, 1 - x / 720) ** 0.8)))
img.alpha_composite(shade)
d = ImageDraw.Draw(img)
d.text((70, 80), 'BOAT CREW', font=font('bahnschrift.ttf', 92), fill=(228, 230, 232))
mod = Image.open(os.path.join(ROOT, 'models', 'modded_banner.png')).convert('RGBA')
mod = mod.resize((560, int(mod.height * 560 / mod.width)), Image.LANCZOS)
img.alpha_composite(mod, (62, 182))
d.text((74, 382), 'WEAPON MOD 0.9.6', font=font('bahnschrift.ttf', 46), fill=(200, 190, 160))
d.line((74, 452, 600, 452), fill=(200, 190, 160), width=2)
d.text((72, 478), 'THE', font=font(TYPEB, 60), fill=PAPER)
d.text((72, 544), 'NEWSREEL', font=font(TYPEB, 84), fill=PAPER)
d.text((76, 646), 'your battles, on film', font=font(TYPE, 30), fill=(200, 190, 160))
y = 708
for line in ('Nicknames: KEEP or VETO', "Crew bonds, the dead man's station", 'The Flak 88, by request'):
    d.text((80, y), '>  ' + line, font=font(TYPE, 24), fill=(214, 206, 186)); y += 38

img.convert('RGB').save(OUT)
print('wrote', OUT)
