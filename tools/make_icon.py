"""Generates src/setup/sznt.ico and docs/img/icon.png: the SZNT monogram (black tile, white S, orange signal).
Usage: python tools/make_icon.py"""
import os
from PIL import Image, ImageDraw, ImageFont

HERE = os.path.dirname(os.path.abspath(__file__))
FONT = os.path.join(HERE, "..", "src", "setup", "fonts", "SZNTWide-Black.ttf")
INK, SIGNAL = (17, 18, 20, 255), (255, 77, 0, 255)


def mark(size=1024):
    k = size / 64.0
    img = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    d.rounded_rectangle([0, 0, size - 1, size - 1], radius=15 * k, fill=INK)
    target_w, target_h = 34 * k, 30 * k
    font = ImageFont.truetype(FONT, int(40 * k))
    box = d.textbbox((0, 0), "S", font=font)
    scale = min(target_w / (box[2] - box[0]), target_h / (box[3] - box[1]))
    font = ImageFont.truetype(FONT, int(40 * k * scale))
    box = d.textbbox((0, 0), "S", font=font)
    w, h = box[2] - box[0], box[3] - box[1]
    d.text((14 * k + (target_w - w) / 2 - box[0], 15 * k + (target_h - h) / 2 - box[1]), "S", font=font, fill=(255, 255, 255, 255))
    d.polygon([(41 * k, 46 * k), (53 * k, 46 * k), (50.6 * k, 52 * k), (38.6 * k, 52 * k)], fill=SIGNAL)
    return img


if __name__ == "__main__":
    big = mark()
    big.save(os.path.join(HERE, "..", "src", "setup", "sznt.ico"),
             sizes=[(16, 16), (20, 20), (24, 24), (32, 32), (40, 40), (48, 48), (64, 64), (128, 128), (256, 256)])
    big.resize((256, 256), Image.LANCZOS).save(os.path.join(HERE, "..", "docs", "img", "icon.png"))
    print("ok")
