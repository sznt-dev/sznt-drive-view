"""Builds the static fonts embedded in the installer (src/setup/fonts) from the variable
Archivo, Geist and Geist Mono fonts (SIL Open Font License 1.1).

Usage: python tools/make_fonts.py <Archivo.ttf> <Geist.ttf> <GeistMono.ttf>
"""
import os
import sys
from fontTools.ttLib import TTFont
from fontTools.varLib import instancer

OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "src", "setup", "fonts")

# (source index, family, style, axes, weight class)
INSTANCES = [
    (0, "SZNT Wide", "Regular", {"wdth": 125, "wght": 400}, 400),
    (0, "SZNT Display", "Regular", {"wdth": 100, "wght": 420}, 400),
    (1, "SZNT Sans", "Regular", {"wght": 400}, 400),
    (1, "SZNT Sans", "Medium", {"wght": 500}, 500),
    (1, "SZNT Sans", "SemiBold", {"wght": 600}, 600),
    (2, "SZNT Mono", "Medium", {"wght": 500}, 500),
    (2, "SZNT Mono", "SemiBold", {"wght": 600}, 600),
]


def rename(font, family, style, weight):
    name = font["name"]
    for rec in list(name.names):
        if rec.nameID in (1, 2, 3, 4, 6, 16, 17, 21, 22, 25):
            name.removeNames(nameID=rec.nameID)
    ps = (family + "-" + style).replace(" ", "")
    for nid, value in ((1, family), (2, "Regular" if style == "Regular" else style), (3, ps), (4, family + " " + style), (6, ps)):
        name.setName(value, nid, 3, 1, 0x409)
    font["OS/2"].usWeightClass = weight
    font["OS/2"].fsSelection = (font["OS/2"].fsSelection & ~0b1100001) | (0b1000000 if style == "Regular" else 0)
    font["head"].macStyle = 0


def main(paths):
    os.makedirs(OUT, exist_ok=True)
    for src, family, style, axes, weight in INSTANCES:
        font = instancer.instantiateVariableFont(TTFont(paths[src]), axes)
        rename(font, family, style, weight)
        out = os.path.join(OUT, (family + "-" + style).replace(" ", "") + ".ttf")
        font.save(out)
        print(os.path.basename(out), os.path.getsize(out))


if __name__ == "__main__":
    if len(sys.argv) != 4:
        sys.exit(__doc__)
    main(sys.argv[1:])
