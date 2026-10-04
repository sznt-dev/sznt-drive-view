"""Builds the looping in-game clips shown by the installer (src/setup/clips/*.clip) from the
website's gameplay videos. Needs ffmpeg on PATH and Pillow.

Usage: python tools/make_clips.py <folder with curva.mp4 and olhar.mp4>

.clip format: b"SZCL", u32 frame count, u32 milliseconds per frame, then for each frame
u32 size + JPEG bytes. The last frames are cross-faded into the first ones so the loop has no seam.
"""
import io
import os
import struct
import subprocess
import sys
import tempfile
from PIL import Image, ImageChops, ImageFilter

OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "src", "setup", "clips")
CLIPS = [("drive", "curva.mp4", 0.3, 7.0), ("view", "olhar.mp4", 2.0, 6.5)]
FPS, WIDTH, FADE = 12, 544, 10


def frames(video, start, length):
    with tempfile.TemporaryDirectory() as tmp:
        subprocess.run(["ffmpeg", "-v", "error", "-ss", str(start), "-t", str(length), "-i", video,
                        "-vf", f"fps={FPS},scale={WIDTH}:-2:flags=lanczos", os.path.join(tmp, "%04d.png")], check=True)
        return [despeckle(Image.open(os.path.join(tmp, f)).convert("RGB")) for f in sorted(os.listdir(tmp))]


def despeckle(im):
    # The capture has single-pixel lighting sparkles; swap them for the local median.
    med = im.filter(ImageFilter.MedianFilter(3))
    mask = ImageChops.subtract(im.convert("L"), med.convert("L")).point(lambda v: 255 if v > 40 else 0)
    im.paste(med, mask=mask.filter(ImageFilter.MaxFilter(3)))
    return im


def seamless(fr):
    n = len(fr)
    body = fr[FADE:n - FADE]
    tail = [Image.blend(fr[n - FADE + i], fr[i], (i + 1) / (FADE + 1)) for i in range(FADE)]
    return body + tail


def main(folder):
    os.makedirs(OUT, exist_ok=True)
    for name, src, start, length in CLIPS:
        fr = seamless(frames(os.path.join(folder, src), start, length))
        blob = bytearray(b"SZCL" + struct.pack("<II", len(fr), 1000 // FPS))
        for im in fr:
            buf = io.BytesIO()
            im.save(buf, "JPEG", quality=70, optimize=True, progressive=False)
            blob += struct.pack("<I", buf.tell()) + buf.getvalue()
        path = os.path.join(OUT, name + ".clip")
        with open(path, "wb") as f:
            f.write(blob)
        print(name, len(fr), "frames", fr[0].size, len(blob) // 1024, "KB")


if __name__ == "__main__":
    if len(sys.argv) != 2:
        sys.exit(__doc__)
    main(sys.argv[1])
