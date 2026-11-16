# YouTube thumbnail: the Boat Crew key art + logo, the dripping MODDED banner, and a Rare loot card from the inventory.
#   python thumb.py <banner.webp> <inventory.png> <modded_banner.png> <out.png>
import sys
from PIL import Image, ImageDraw, ImageFilter, ImageChops

art_p, inv_p, mod_p, out_p = sys.argv[1:5]
W, H = 1280, 720
art = Image.open(art_p).convert("RGB").crop((0, 4, 1253, 317))     # drop the dark strip along the top
inv = Image.open(inv_p).convert("RGB")
modded = Image.open(mod_p).convert("RGBA")

# background: the art at full height, slid right so the boat fills the right side
s = H / art.height
big = art.resize((round(art.width * s), H), Image.LANCZOS).filter(ImageFilter.UnsharpMask(2, 60, 2))
x0 = big.width - W
canvas = big.crop((x0, 0, x0 + W, H)).convert("RGBA")
shade = Image.new("L", (W, H), 0); sd = ImageDraw.Draw(shade)       # darken the left so the text pops
for y in range(H):                                                 # strongest top left, where the island sits behind the logo
    for x in range(0, 700, 2):
        v = 150 * max(0.0, 1 - x / 620) ** 1.4 + 130 * max(0.0, 1 - x / 640) * max(0.0, 1 - y / 320)
        sd.line([(x, y), (x + 1, y)], fill=min(215, int(v)))
canvas = Image.composite(Image.new("RGBA", (W, H), (4, 18, 40, 255)), canvas, shade)

def shadowed(img, blur=10, off=(6, 8), alpha=0.75, glow=None):
    """img with a soft drop shadow (and an optional colored glow) on a padded transparent layer."""
    pad = blur * 3
    layer = Image.new("RGBA", (img.width + 2 * pad, img.height + 2 * pad), (0, 0, 0, 0))
    a = img.split()[3]
    if glow:
        g = Image.new("RGBA", layer.size, glow + (0,))
        ga = Image.new("L", layer.size, 0); ga.paste(a, (pad, pad))
        g.putalpha(ga.filter(ImageFilter.MaxFilter(9)).filter(ImageFilter.GaussianBlur(blur)))
        layer = Image.alpha_composite(layer, g)
    sh = Image.new("L", layer.size, 0); sh.paste(a, (pad + off[0], pad + off[1]))
    sh = sh.filter(ImageFilter.GaussianBlur(blur)).point(lambda v: int(v * alpha))
    black = Image.new("RGBA", layer.size, (0, 0, 0, 255)); black.putalpha(sh)
    layer = Image.alpha_composite(layer, black)
    layer.alpha_composite(img, (pad, pad))
    return layer, pad

# the game's logo, lifted off the sea: keep pixels that are not blue (white, grey, the yellow muzzle flashes)
logo = art.crop((60, 126, 340, 281))
r, g, b = [ch.point(lambda v: v) for ch in logo.split()]
blueness = ImageChops.subtract(b, r)                                    # sea: blue far above red; logo: about equal
mask = blueness.point(lambda v: 255 if v < 35 else max(0, 255 - (v - 35) * 9))
mask = mask.filter(ImageFilter.MinFilter(3)).filter(ImageFilter.GaussianBlur(0.8))
logo = logo.convert("RGBA"); logo.putalpha(mask)
ls = 1.75
logo = logo.resize((round(logo.width * ls), round(logo.height * ls)), Image.LANCZOS)
layer, pad = shadowed(logo, blur=8, off=(4, 6), alpha=0.6)
lx, ly = 34, 18
canvas.alpha_composite(layer, (lx - pad, ly - pad))

# MODDED under it, as on the title screen
mw = 470
mod = modded.resize((mw, round(modded.height * mw / modded.width)), Image.LANCZOS)
my = ly + logo.height - 18
canvas.alpha_composite(mod, (lx + (logo.width - mw) // 2 + 6, my))

# the Rare loot card: the tooltip, with a gold gun tile from the grid on its empty bottom right corner
# (the Rare twin gun from YOUR GUNS: the loot tile has the mouse pointer on it)
tip = inv.crop((662, 175, 1068, 346))
tile = inv.crop((24, 359, 183, 465))
cs, ts = 1.3, 1.0
tip = tip.resize((round(tip.width * cs), round(tip.height * cs)), Image.LANCZOS)
tile = tile.resize((round(tile.width * ts), round(tile.height * ts)), Image.LANCZOS)
tl = Image.new("RGBA", (tile.width + 8, tile.height + 8), (255, 179, 0, 255)); tl.paste(tile, (4, 4))
tx, ty = tip.width - tl.width + 60, tip.height - tl.height + 34       # sticks out past the right and bottom edges
card = Image.new("RGBA", (tx + tl.width, ty + tl.height), (0, 0, 0, 0))
card.paste(tip, (0, 0))
ImageDraw.Draw(card).rectangle([0, 0, tip.width - 1, tip.height - 1], outline=(255, 179, 0, 255), width=4)
card.alpha_composite(tl, (tx, ty))
card = card.rotate(4, resample=Image.BICUBIC, expand=True)
layer, pad = shadowed(card, blur=14, off=(8, 12), alpha=0.8, glow=(255, 170, 0))
canvas.alpha_composite(layer, (22 - pad, H - card.height - 6 - pad))

canvas.convert("RGB").save(out_p, quality=95)
print("wrote", out_p, canvas.size)
