"""Generates src/setup/sznt.ico and docs/img/icon.png: the SZNT mark (dark tile, white S, orange signal).
Usage: python tools/make_icon.py"""
import os
from PIL import Image, ImageDraw, ImageFont

HERE = os.path.dirname(os.path.abspath(__file__))
FONT = os.path.join(HERE, "..", "src", "setup", "fonts", "SZNTWide-Regular.ttf")
INK, SIGNAL = (20, 21, 23, 255), (255, 77, 0, 255)


def mark(size=1024):
    k = size / 64.0
    img = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    d.rounded_rectangle([0, 0, size - 1, size - 1], radius=15 * k, fill=INK)
    box_x, box_y, box_s = 13 * k, 13 * k, 38 * k
    font = ImageFont.truetype(FONT, int(40 * k))
    b = d.textbbox((0, 0), "S", font=font)
    font = ImageFont.truetype(FONT, int(40 * k * min(box_s / (b[2] - b[0]), box_s / (b[3] - b[1]))))
    b = d.textbbox((0, 0), "S", font=font)
    w, h = b[2] - b[0], b[3] - b[1]
    d.text((box_x + (box_s - w) / 2 - b[0], box_y + (box_s - h) / 2 - b[1]), "S", font=font, fill=(255, 255, 255, 255))
    d.polygon([(44 * k, 49 * k), (55 * k, 49 * k), (53 * k, 54 * k), (42 * k, 54 * k)], fill=SIGNAL)
    return img


if __name__ == "__main__":
    big = mark()
    big.save(os.path.join(HERE, "..", "src", "setup", "sznt.ico"),
             sizes=[(16, 16), (20, 20), (24, 24), (32, 32), (40, 40), (48, 48), (64, 64), (128, 128), (256, 256)])
    big.resize((256, 256), Image.LANCZOS).save(os.path.join(HERE, "..", "docs", "img", "icon.png"))
    print("ok")
