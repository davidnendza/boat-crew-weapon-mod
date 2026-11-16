"""Draws the 0.9.5 release banner for Discord: release/banner-0.9.5.png (1600x900).

A dossier on a dark desk: the MODDED title on the left, and on the right a pinned in-game photo (the talking crew), a
crew file card with a KIA stamp, a letter home and a returned envelope. Pillow only; Windows fonts.
    python tools/make_release_banner.py [screenshot]
"""
import os, random, sys
from PIL import Image, ImageDraw, ImageEnhance, ImageFilter, ImageFont, ImageOps

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = os.path.join(ROOT, 'release', 'banner-0.9.5.png')
SHOT = sys.argv[1] if len(sys.argv) > 1 else None
W, H = 1600, 900
F = r'C:\Windows\Fonts'
def font(name, size): return ImageFont.truetype(os.path.join(F, name), size)
TYPE, TYPEB = 'cour.ttf', 'courbd.ttf'
random.seed(95)

INK = (43, 42, 38)
RED = (179, 38, 30)
PAPER = (232, 220, 192)


def paper(w, h, tint=PAPER):
    """A sheet of the mod's paper texture, cropped and tinted."""
    tex = Image.open(os.path.join(ROOT, 'models', 'ui_paper.png')).convert('RGB')
    if tex.width < w or tex.height < h: tex = tex.resize((max(w, tex.width), max(h, tex.height)))
    x = random.randint(0, tex.width - w); y = random.randint(0, tex.height - h)
    return Image.blend(tex.crop((x, y, x + w, y + h)), Image.new('RGB', (w, h), tint), 0.35).convert('RGBA')


def place(base, card, cx, cy, angle, shadow=18):
    """Rotates card, drops a soft shadow, pastes it centred at (cx, cy)."""
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


def typed(d, xy, text, f, fill=INK, spacing=6, width=None):
    """Typewriter text, wrapped by words."""
    x, y = xy
    lines = []
    for para in text.split('\n'):
        if width is None: lines.append(para); continue
        cur = ''
        for word in para.split(' '):
            if f.getlength((cur + ' ' + word).strip()) > width: lines.append(cur); cur = word
            else: cur = (cur + ' ' + word).strip()
        lines.append(cur)
    for ln in lines:
        d.text((x, y), ln, font=f, fill=fill)
        y += f.size + spacing
    return y


# ---- the desk ----
img = Image.new('RGBA', (W, H), (16, 24, 34, 255))
g = Image.new('L', (W, H))
gd = ImageDraw.Draw(g)
for r in range(0, 900, 6):                                              # a lamp's pool of light on the right
    gd.ellipse((1100 - r * 1.5, 430 - r, 1100 + r * 1.5, 430 + r), fill=max(0, 120 - r // 6))
img = Image.composite(Image.new('RGBA', (W, H), (88, 70, 46, 255)), img, g.filter(ImageFilter.GaussianBlur(60)))
noise = Image.effect_noise((W, H), 18).convert('L')
img = Image.blend(img, Image.merge('RGBA', (noise, noise, noise, Image.new('L', (W, H), 255))), 0.06)

# ---- the dossier, back to front ----
folder = Image.new('RGBA', (900, 640), (0, 0, 0, 0))
fd = ImageDraw.Draw(folder)
fd.rounded_rectangle((0, 40, 899, 639), 14, fill=(163, 128, 74, 255))
fd.rounded_rectangle((40, 0, 330, 70), 14, fill=(163, 128, 74, 255))
fd.text((70, 14), 'CREW FILES', font=font(TYPEB, 30), fill=(70, 50, 25))
place(img, folder, 1180, 470, -3, 24)

if SHOT and os.path.exists(SHOT):                                       # the in-game photo, pinned
    shot = ImageEnhance.Brightness(ImageOps.fit(Image.open(SHOT).convert('RGB'), (540, 338), centering=(0.5, 0.35))).enhance(1.3)
    photo = Image.new('RGBA', (shot.width + 36, shot.height + 90), (238, 233, 220, 255))
    photo.paste(shot, (18, 18))
    ImageDraw.Draw(photo).text((24, shot.height + 32), 'battle 21, in the mod', font=font(TYPE, 22), fill=INK)
    place(img, photo, 955, 290, -4, 26)
    pin = Image.new('RGBA', (34, 34), (0, 0, 0, 0))
    ImageDraw.Draw(pin).ellipse((2, 2, 31, 31), fill=(170, 30, 30, 255), outline=(90, 10, 10, 255), width=2)
    img.alpha_composite(pin, (940, 70))

# a letter home, in the mod's own words for a man lost with the boat
letter = paper(400, 430)
ld = ImageDraw.Draw(letter)
y = typed(ld, (30, 30), 'Dear Mrs. Thomason,', font(TYPEB, 20))
y = typed(ld, (30, y + 10), 'It is my sad duty to tell you that your husband, George Thomason, was killed in action '
          'when our boat was sunk. He was at his station, and it was over in moments.\n\n'
          'He had it all worked out: after the war, he would open a diner on the highway.', font(TYPE, 17), width=345)
typed(ld, (30, y + 14), 'Sincerely,\nCommanding Officer', font(TYPE, 17))
place(img, letter, 1392, 370, 5)

# the returned envelope
env = paper(400, 220, (214, 196, 160))
ed = ImageDraw.Draw(env)
ed.rectangle((316, 18, 384, 94), outline=(120, 40, 35), width=3)
ed.text((328, 38), '3c', font=font(TYPEB, 30), fill=(120, 40, 35))
typed(ed, (34, 120), 'S1c G. Thomason\nc/o Fleet Post Office\nSan Francisco, Calif.', font(TYPE, 17))
env.alpha_composite(stamp('RETURNED TO SENDER', 24).rotate(-7, expand=True), (16, 22))
place(img, env, 1380, 790, -4)

# the crew file card
card = paper(470, 300, (240, 234, 216))
cd = ImageDraw.Draw(card)
cd.rectangle((24, 24, 134, 164), fill=(205, 196, 176), outline=INK, width=2)
cd.text((40, 82), 'NO PHOTO', font=font(TYPE, 14), fill=(110, 95, 69))
cd.text((152, 24), 'THOMASON, GEORGE', font=font(TYPEB, 24), fill=INK)
cd.text((152, 58), 'Killed in action, battle 8', font=font(TYPE, 18), fill=RED)
typed(cd, (152, 90), "From Wichita, Kansas.\nStrung wire for the\ntelephone company.\nAboard: the boat's joker.", font(TYPE, 17), spacing=4)
cd.line((24, 196, 446, 196), fill=INK, width=2)
typed(cd, (24, 206), 'IN MEMORY  Eight engagements.\nNothing of his was saved,\nnot even his harmonica.', font(TYPE, 17), spacing=4)
card.alpha_composite(stamp('KIA', 34).rotate(12, expand=True), (40, 108))
place(img, card, 905, 705, 3)

# a speech bubble
bub = Image.new('RGBA', (440, 120), (0, 0, 0, 0))
bd = ImageDraw.Draw(bub)
bd.rounded_rectangle((0, 0, 439, 88), 12, fill=(24, 22, 20, 225))
bd.polygon([(60, 86), (90, 86), (58, 118)], fill=(24, 22, 20, 225))
bd.text((22, 22), 'Ma says Pepper had six puppies.', font=font('georgia.ttf', 26), fill=(240, 236, 226))
place(img, bub, 1290, 92, 2, 10)

# ---- the title ----
shade = Image.new('RGBA', (W, H), (0, 0, 0, 0))
sd = ImageDraw.Draw(shade)
for x in range(720):                                                    # a dark fade under the title
    sd.line((x, 0, x, H), fill=(10, 14, 20, int(235 * max(0, 1 - x / 720) ** 0.8)))
img.alpha_composite(shade)
d = ImageDraw.Draw(img)
d.text((70, 80), 'BOAT CREW', font=font('bahnschrift.ttf', 92), fill=(228, 230, 232))
mod = Image.open(os.path.join(ROOT, 'models', 'modded_banner.png')).convert('RGBA')
mod = mod.resize((560, int(mod.height * 560 / mod.width)), Image.LANCZOS)
img.alpha_composite(mod, (62, 182))
d.text((74, 382), 'WEAPON MOD 0.9.5', font=font('bahnschrift.ttf', 46), fill=(200, 190, 160))
d.line((74, 452, 600, 452), fill=(200, 190, 160), width=2)
d.text((72, 478), 'CREW', font=font(TYPEB, 78), fill=PAPER)
d.text((72, 562), 'ATTACHMENTS', font=font(TYPEB, 74), fill=PAPER)
d.text((76, 660), 'initial version, more coming', font=font(TYPE, 30), fill=(200, 190, 160))
y = 722
for line in ('A past for every crew member', 'Letters home and mail call', 'Eight voices that talk in battle'):
    d.text((80, y), '>  ' + line, font=font(TYPE, 24), fill=(214, 206, 186)); y += 38

img.convert('RGB').save(OUT)
print('wrote', OUT)
