"""Decode the test GIF with Pillow and check frame count, size and that the gradient survived the styling."""
import sys
from PIL import Image, ImageSequence

im = Image.open(sys.argv[1])
frames = [f.convert("L") for f in ImageSequence.Iterator(im)]
assert len(frames) == 30, len(frames)
assert frames[0].size == (320, 180), frames[0].size
row = [frames[0].getpixel((x, 90)) for x in range(0, 256, 32)]
assert row[-1] > row[0] + 100, row   # left dark, right bright: the gradient decoded
assert im.info.get("duration") == 100, im.info.get("duration")
print("ok", len(frames), "frames, row sample", row)
