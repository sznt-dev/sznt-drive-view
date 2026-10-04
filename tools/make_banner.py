"""Generates the README images in the SZNT look, light and dark:
docs/img/banner-{light,dark}.png and docs/img/weight-{light,dark}.png.
Needs ffmpeg on PATH for the gameplay still.

Usage: python tools/make_banner.py <folder with curva.mp4>
"""
import os
import subprocess
import sys
import tempfile
from PIL import Image, ImageChops, ImageDraw, ImageFilter, ImageFont

HERE = os.path.dirname(os.path.abspath(__file__))
FONTS = os.path.join(HERE, "..", "src", "setup", "fonts")
OUT = os.path.join(HERE, "..", "docs", "img")

THEMES = {
    "light": dict(paper=(237, 235, 230), card=(247, 246, 242), ink=(20, 21, 23), ink2=(61, 62, 66), muted=(116, 114, 107),
                  line=(217, 213, 204), track=(228, 225, 218), signal=(255, 77, 0), cobalt=(43, 89, 255), grid=(225, 222, 215)),
    "dark": dict(paper=(16, 17, 19), card=(26, 27, 31), ink=(236, 235, 230), ink2=(195, 193, 187), muted=(141, 139, 133),
                 line=(42, 43, 48), track=(36, 37, 42), signal=(255, 90, 22), cobalt=(91, 124, 255), grid=(26, 27, 30)),
}


def font(name, size):
    return ImageFont.truetype(os.path.join(FONTS, name), size)


def tracked(d, xy, text, f, fill, track):
    x, y = xy
    for ch in text:
        d.text((x, y), ch, font=f, fill=fill)
        x += d.textlength(ch, font=f) + track
    return x


def signal(d, x, y, w, h, color):
    d.polygon([(x + 0.1 * w, y), (x + w, y), (x + 0.9 * w, y + h), (x, y + h)], fill=color)


def still(folder, at=3.0, width=1240):
    with tempfile.TemporaryDirectory() as tmp:
        out = os.path.join(tmp, "f.png")
        subprocess.run(["ffmpeg", "-v", "error", "-ss", str(at), "-i", os.path.join(folder, "curva.mp4"), "-frames:v", "1",
                        "-vf", f"scale={width}:-2:flags=lanczos", out], check=True)
        im = Image.open(out).convert("RGB")
    med = im.filter(ImageFilter.MedianFilter(3))
    mask = ImageChops.subtract(im.convert("L"), med.convert("L")).point(lambda v: 255 if v > 40 else 0)
    im.paste(med, mask=mask.filter(ImageFilter.MaxFilter(3)))
    return im


def rounded(im, radius):
    mask = Image.new("L", (im.width * 2, im.height * 2), 0)
    ImageDraw.Draw(mask).rounded_rectangle([0, 0, mask.width - 1, mask.height - 1], radius=radius * 2, fill=255)
    return mask.resize(im.size, Image.LANCZOS)


def banner(t, shot):
    W, H = 2560, 900
    img = Image.new("RGB", (W, H), t["paper"])
    d = ImageDraw.Draw(img)
    for x in range(112, W, 112):
        d.line([(x, 0), (x, H)], fill=t["grid"], width=2)
    for y in range(112, H, 112):
        d.line([(0, y), (W, y)], fill=t["grid"], width=2)
    x = 150
    wide = font("SZNTWide-Regular.ttf", 64)
    end = tracked(d, (x, 140), "SZNT", wide, t["ink"], 2) - 2
    top, bottom = wide.getbbox("T")[1] + 140, wide.getbbox("T")[3] + 140
    cap = bottom - top
    signal(d, end + 0.1 * cap, bottom - 0.156 * cap, 0.47 * cap, 0.156 * cap, t["signal"])
    mono = font("SZNTMono-Medium.ttf", 30)
    signal(d, x, 318, 30, 18, t["signal"])
    tracked(d, (x + 48, 306), "PLUGINS FOR ETS2 & ATS", mono, t["ink2"], 4.5)
    disp = font("SZNTDisplay-Regular.ttf", 150)
    tracked(d, (x - 6, 372), "Drive + View", disp, t["ink"], -4.5)
    sans = font("SZNTSans-Regular.ttf", 42)
    d.text((x, 590), "Steering, pedals and a cab camera that", font=sans, fill=t["muted"])
    d.text((x, 646), "move like the real thing. Keyboard and mouse.", font=sans, fill=t["muted"])
    for i, (label, c) in enumerate((("SZNT DRIVE", t["signal"]), ("SZNT VIEW", t["cobalt"]))):
        lx = x + i * 330
        d.ellipse([lx, 762, lx + 18, 780], fill=c)
        tracked(d, (lx + 34, 752), label, font("SZNTWide-Regular.ttf", 30), t["ink"], 1.5)
    sx, sy = W - shot.width - 120, (H - shot.height) // 2
    img.paste(shot, (sx, sy), rounded(shot, 44))
    d.rounded_rectangle([sx, sy, sx + shot.width - 1, sy + shot.height - 1], radius=44, outline=t["line"], width=2)
    return img


def weight_chart(t):
    W, H = 1720, 700
    img = Image.new("RGB", (W, H), t["card"])
    d = ImageDraw.Draw(img)
    d.rounded_rectangle([0, 0, W - 1, H - 1], radius=56, outline=t["line"], width=3)
    signal(d, 64, 74, 24, 14, t["signal"])
    tracked(d, (102, 62), "WEIGHT", font("SZNTMono-Medium.ttf", 26), t["ink2"], 5)
    tracked(d, (62, 100), "Time to floor the throttle with W", font("SZNTDisplay-Regular.ttf", 60), t["ink"], -1.8)
    rows = [("Tractor only", "8.5 t", 6.1), ("Empty trailer", "15.5 t", 7.1), ("10 t cargo", "25.5 t", 8.0),
            ("Heavy load (reference)", "40 t", 9.0), ("40 t cargo", "55.5 t", 9.7), ("Heavy haul", "95.5 t", 11.1)]
    sans, sb, mono = font("SZNTSans-Regular.ttf", 32), font("SZNTSans-SemiBold.ttf", 32), font("SZNTMono-SemiBold.ttf", 28)
    x0, x1, y = 560, 1560, 222
    for name, mass, secs in rows:
        ref = "reference" in name
        d.text((64, y), name, font=sb if ref else sans, fill=t["ink"] if ref else t["ink2"])
        d.text((64 + d.textlength(name, font=sb if ref else sans) + 16, y + 3), mass, font=mono, fill=t["muted"])
        d.rounded_rectangle([x0, y + 4, x1, y + 40], radius=18, fill=t["track"])
        w = (x1 - x0) * secs / 11.1
        d.rounded_rectangle([x0, y + 4, x0 + w, y + 40], radius=18, fill=t["signal"] if ref else t["ink"] if secs > 9 else t["muted"])
        d.text((x0 + w + 18, y + 4), f"{secs:.1f} s", font=mono, fill=t["ink"])
        y += 74
    return img


if __name__ == "__main__":
    if len(sys.argv) != 2:
        sys.exit(__doc__)
    shot = still(sys.argv[1])
    for name, t in THEMES.items():
        banner(t, shot).save(os.path.join(OUT, f"banner-{name}.png"), optimize=True)
        weight_chart(t).save(os.path.join(OUT, f"weight-{name}.png"), optimize=True)
    print("ok")
