"""Generates the README images in the SZNT look: docs/img/banner.png and docs/img/weight.png.
Usage: python tools/make_banner.py"""
import os
from PIL import Image, ImageDraw, ImageFont

HERE = os.path.dirname(os.path.abspath(__file__))
FONTS = os.path.join(HERE, "..", "src", "setup", "fonts")
OUT = os.path.join(HERE, "..", "docs", "img")

PAPER, CARD, INK, INK2, MUTED, LINE = (237, 235, 230), (248, 247, 243), (17, 18, 20), (57, 58, 62), (111, 109, 103), (216, 212, 202)
SIGNAL, COBALT = (255, 77, 0), (43, 89, 255)


def font(name, size):
    return ImageFont.truetype(os.path.join(FONTS, name), size)


def grid(d, w, h, step, top=0):
    for x in range(step, w, step):
        d.line([(x, top), (x, h)], fill=(226, 223, 216), width=2)
    for y in range(top + step, h, step):
        d.line([(0, y), (w, y)], fill=(226, 223, 216), width=2)


def tracked(d, xy, text, f, fill, track):
    x, y = xy
    for ch in text:
        d.text((x, y), ch, font=f, fill=fill)
        x += d.textlength(ch, font=f) + track
    return x


def signal(d, x, y, s, color=SIGNAL):
    d.polygon([(x + 0.17 * s, y), (x + 1.0 * s, y), (x + 0.83 * s, y + 0.5 * s), (x, y + 0.5 * s)], fill=color)


def banner():
    W, H = 2560, 840
    img = Image.new("RGB", (W, H), PAPER)
    d = ImageDraw.Draw(img)
    grid(d, W, H, 112)
    wide = font("SZNTWide-Black.ttf", 248)
    x = 150
    d.text((x, 150), "SZNT", font=font("SZNTWide-Black.ttf", 70), fill=INK2)
    x2 = x + d.textlength("DRIVE", font=wide)
    d.text((x - 6, 250), "DRIVE", font=wide, fill=SIGNAL)
    d.text((x2 + 40, 262), "+", font=font("SZNTWide-Black.ttf", 160), fill=(201, 196, 184))
    d.text((x2 + 190, 250), "VIEW", font=wide, fill=COBALT)
    mono = font("SZNTMono-Medium.ttf", 34)
    signal(d, x, 640, 46)
    tracked(d, (x + 70, 628), "PLUGINS FOR EURO TRUCK SIMULATOR 2 & AMERICAN TRUCK SIMULATOR", mono, INK2, 5)
    sans = font("SZNTSans-Regular.ttf", 44)
    d.text((x, 700), "Realistic steering, pedals and head movement on keyboard and mouse.", font=sans, fill=MUTED)
    return img


def weight_chart():
    W, H = 1720, 700
    img = Image.new("RGB", (W, H), CARD)
    d = ImageDraw.Draw(img)
    d.rounded_rectangle([0, 0, W - 1, H - 1], radius=48, outline=LINE, width=3)
    signal(d, 64, 72, 28)
    tracked(d, (108, 62), "WEIGHT", font("SZNTMono-Medium.ttf", 26), (184, 55, 0), 5)
    d.text((64, 104), "Time to floor the throttle with W", font=font("SZNTDisplay-ExtraBold.ttf", 56), fill=INK)
    rows = [("Tractor only", "8.5 t", 6.1), ("Empty trailer", "15.5 t", 7.1), ("10 t cargo", "25.5 t", 8.0),
            ("Heavy load (reference)", "40 t", 9.0), ("40 t cargo", "55.5 t", 9.7), ("Heavy haul", "95.5 t", 11.1)]
    sans, sb, mono = font("SZNTSans-Regular.ttf", 32), font("SZNTSans-SemiBold.ttf", 32), font("SZNTMono-SemiBold.ttf", 28)
    x0, x1, y = 560, 1560, 222
    for name, mass, secs in rows:
        ref = "reference" in name
        d.text((64, y), name, font=sb if ref else sans, fill=INK if ref else INK2)
        d.text((64 + d.textlength(name, font=sb if ref else sans) + 16, y + 3), mass, font=mono, fill=MUTED)
        d.rounded_rectangle([x0, y + 4, x1, y + 40], radius=18, fill=(229, 226, 219))
        w = (x1 - x0) * secs / 11.1
        d.rounded_rectangle([x0, y + 4, x0 + w, y + 40], radius=18, fill=SIGNAL if ref else (INK if secs > 9 else (255, 140, 90)))
        d.text((x0 + w + 18, y + 4), f"{secs:.1f} s", font=mono, fill=INK)
        y += 74
    return img


if __name__ == "__main__":
    banner().save(os.path.join(OUT, "banner.png"), optimize=True)
    weight_chart().save(os.path.join(OUT, "weight.png"), optimize=True)
    print("ok")
