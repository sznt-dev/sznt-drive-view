"""Generates src/setup/sznt.ico (stylized steering wheel). Usage: python tools/make_icon.py"""
import math
import os
from PIL import Image, ImageDraw

S = 1024
BG = (21, 23, 28, 255)
ACCENT = (242, 169, 0, 255)
LIGHT = (236, 238, 242, 255)


def draw():
    img = Image.new("RGBA", (S, S), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    d.rounded_rectangle([24, 24, S - 24, S - 24], radius=210, fill=BG)
    c = S / 2
    r_out, ring = 360, 74
    d.ellipse([c - r_out, c - r_out, c + r_out, c + r_out], outline=LIGHT, width=ring)
    hub = 92
    d.ellipse([c - hub, c - hub, c + hub, c + hub], fill=ACCENT)
    w = 64
    for ang in (180, 0, 90):
        a = math.radians(ang)
        x0, y0 = c + math.cos(a) * hub * 0.6, c + math.sin(a) * hub * 0.6
        x1, y1 = c + math.cos(a) * (r_out - ring / 2), c + math.sin(a) * (r_out - ring / 2)
        d.line([x0, y0, x1, y1], fill=LIGHT, width=w)
    a0, a1 = 200, 340
    d.arc([c - r_out, c - r_out, c + r_out, c + r_out], start=a0, end=a1, fill=ACCENT, width=ring)
    return img


if __name__ == "__main__":
    here = os.path.dirname(os.path.abspath(__file__))
    out = os.path.join(here, "..", "src", "setup", "sznt.ico")
    draw().save(out, sizes=[(16, 16), (24, 24), (32, 32), (48, 48), (64, 64), (128, 128), (256, 256)])
    draw().resize((256, 256), Image.LANCZOS).save(os.path.join(here, "..", "docs", "img", "icon.png"))
    print("ok", os.path.normpath(out))
