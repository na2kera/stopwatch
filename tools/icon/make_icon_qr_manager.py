"""Generate the QR Manager launcher icon with Pillow, at 4x resolution.

Requires Pillow. Run: python3 make_icon_qr_manager.py
Writes icon_qr_manager.png beside this script; convert it with png2c.py afterwards.
"""
from pathlib import Path
import math
from PIL import Image, ImageDraw, ImageFilter, ImageChops

SCALE = 4
SIZE = 200
N = SIZE * SCALE


def box(values):
    return tuple(round(v * SCALE) for v in values)


def rounded_mask(bounds, radius):
    mask = Image.new("L", (N, N))
    ImageDraw.Draw(mask).rounded_rectangle(box(bounds), radius=radius * SCALE, fill=255)
    return mask


def generate():
    canvas = Image.new("RGB", (N, N), "black")
    plate = rounded_mask((34, 34, 166, 166), 28)

    # Layered bloom is composited onto opaque black, with a black outer margin.
    for blur, color, opacity in [(12, (100, 65, 235), .25),
                                  (7, (123, 77, 246), .29),
                                  (3, (113, 86, 230), .22)]:
        bloom = plate.filter(ImageFilter.GaussianBlur(blur * SCALE))
        bloom = bloom.point(lambda v: round(v * opacity))
        canvas.paste(color, (0, 0), bloom)

    glass = Image.new("RGB", (N, N))
    pixels = glass.load()
    for y in range(N):
        py = y / SCALE
        for x in range(N):
            px = x / SCALE
            t = max(0, min(1, (.38 * (px - 34) + .62 * (py - 34)) / 132))
            base = tuple(a * (1 - t) + b * t for a, b in
                         zip((139, 92, 246), (79, 70, 229)))
            # Diffuse white reflection gives the glass its frosted appearance.
            top = .31 * math.exp(-((px - 66) / 98) ** 2 - ((py - 43) / 57) ** 2)
            middle = .24 * math.exp(-((px - 95) / 48) ** 2 - ((py - 95) / 48) ** 2)
            edge = .12 * math.exp(-((px - 151) / 25) ** 2 - ((py - 124) / 77) ** 2)
            white = min(.5, top + middle + edge)
            distance = min(px - 34, 166 - px, py - 34, 166 - py)
            shade = .18 * math.exp(-max(0, distance) / 10)
            pixels[x, y] = tuple(round((c * (1 - white) + 255 * white) * (1 - shade))
                                 for c in base)
    canvas.paste(glass, (0, 0), plate)

    # Thin pale rim, strongest at the top left, fading into the lower edge.
    rim = Image.new("L", (N, N))
    ImageDraw.Draw(rim).rounded_rectangle(box((34.2, 34.2, 165.8, 165.8)),
                                        radius=27.8 * SCALE, outline=255, width=3)
    fade = Image.new("L", (N, N))
    fade.putdata([round(160 * (1 - .70 * y / (N - 1)) *
                        (1 - .25 * x / (N - 1)))
                  for y in range(N) for x in range(N)])
    canvas.paste((242, 236, 255), (0, 0), ImageChops.multiply(rim, fade))

    glyph = Image.new("L", (N, N))
    draw = ImageDraw.Draw(glyph)
    for x, y in [(58, 58), (112, 58), (58, 112)]:
        draw.rounded_rectangle(box((x, y, x + 30, y + 30)), radius=6 * SCALE, fill=255)
        draw.rounded_rectangle(box((x + 5.5, y + 5.5, x + 24.5, y + 24.5)),
                               radius=2.8 * SCALE, fill=0)
        draw.rounded_rectangle(box((x + 10.5, y + 10.5, x + 19.5, y + 19.5)),
                               radius=2 * SCALE, fill=255)
    # Sparse modules keep the symbol legible at launcher resolution.
    for x, y in [(96, 62), (96, 96), (62, 96), (116, 96),
                 (132, 112), (112, 128), (132, 132)]:
        draw.rounded_rectangle(box((x, y, x + 8, y + 8)), radius=2.5 * SCALE, fill=255)
    canvas.paste((255, 253, 255), (0, 0), glyph)
    result = canvas.resize((SIZE, SIZE), Image.Resampling.LANCZOS)
    result.save(Path(__file__).with_name("icon_qr_manager.png"))
    assert result.size == (200, 200) and result.mode == "RGB"


if __name__ == "__main__":
    generate()
