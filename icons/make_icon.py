#!/usr/bin/env python3
"""Generate the Aurora Monitor icon: a sun above a solar panel.

The artwork is defined by the shapes in this file, which is the source of the
icon. It writes auroramon.svg, auroramon_NN.png and ../src/auroramon_icon.h.
Only the Python standard library is needed.

Copyright and licence: see README.txt in this directory (GPL-3.0-or-later).
"""
import math
import os
import struct
import zlib

HERE = os.path.dirname(os.path.abspath(__file__))
SIZES = (16, 32, 48, 64, 128, 256)
BOX = 256.0

SKY_TOP = (30, 74, 138)
SKY_BOTTOM = (72, 146, 206)
SUN = (255, 200, 40)
RAY = (255, 170, 30)
PANEL = (18, 40, 84)
FRAME = (226, 232, 240)
GRID = (108, 152, 214)
STAND = (70, 84, 104)


def circle(cx, cy, r, steps=96):
    return [(cx + r * math.cos(2 * math.pi * i / steps), cy + r * math.sin(2 * math.pi * i / steps)) for i in range(steps)]


def rounded_rect(x0, y0, x1, y1, r, steps=16):
    pts = []
    for cx, cy, a0 in ((x1 - r, y0 + r, -90), (x1 - r, y1 - r, 0), (x0 + r, y1 - r, 90), (x0 + r, y0 + r, 180)):
        for i in range(steps + 1):
            a = math.radians(a0 + 90.0 * i / steps)
            pts.append((cx + r * math.cos(a), cy + r * math.sin(a)))
    return pts


def lerp(a, b, t):
    return (a[0] + (b[0] - a[0]) * t, a[1] + (b[1] - a[1]) * t)


def thick_line(a, b, width):
    dx, dy = b[0] - a[0], b[1] - a[1]
    n = math.hypot(dx, dy)
    ox, oy = -dy / n * width / 2, dx / n * width / 2
    return [(a[0] + ox, a[1] + oy), (b[0] + ox, b[1] + oy), (b[0] - ox, b[1] - oy), (a[0] - ox, a[1] - oy)]


def shapes(size):
    """Painted in order, as (points, colour). Small sizes get less detail, to stay clear."""
    out = []
    sun_x, sun_y, sun_r = 172.0, 76.0, 36.0

    # sun rays
    for i in range(12):
        a = 2 * math.pi * i / 12
        inner, outer = sun_r + 9, sun_r + (27 if i % 2 == 0 else 19)
        if size < 32 and i % 2:
            continue
        out.append((thick_line((sun_x + inner * math.cos(a), sun_y + inner * math.sin(a)),
                               (sun_x + outer * math.cos(a), sun_y + outer * math.sin(a)), 9 if size >= 32 else 14), RAY))
    out.append((circle(sun_x, sun_y, sun_r), SUN))

    # the stand, then the panel in perspective
    out.append(([(116, 200), (140, 200), (146, 236), (110, 236)], STAND))
    tl, tr, br, bl = (64.0, 116.0), (196.0, 116.0), (232.0, 212.0), (24.0, 212.0)
    out.append(([tl, tr, br, bl], FRAME))
    m = 7.0
    itl, itr, ibr, ibl = (tl[0] + m, tl[1] + m), (tr[0] - m, tr[1] + m), (br[0] - m - 2, br[1] - m), (bl[0] + m + 2, bl[1] - m)
    out.append(([itl, itr, ibr, ibl], PANEL))

    # cells
    if size >= 32:
        cols, rows = (4, 3) if size >= 48 else (3, 2)
        width = 3.5 if size >= 64 else 6.0
        for c in range(1, cols):
            t = c / cols
            out.append((thick_line(lerp(itl, itr, t), lerp(ibl, ibr, t), width), GRID))
        for r in range(1, rows):
            t = r / rows
            out.append((thick_line(lerp(itl, ibl, t), lerp(itr, ibr, t), width), GRID))
    return out


BACKGROUND = rounded_rect(4, 4, 252, 252, 44)


def fill(canvas, width, height, points, colour, scale):
    """Scanline polygon fill on a list of bytearrays (RGBA)."""
    pts = [(x * scale, y * scale) for x, y in points]
    rgba = bytes(colour) + b"\xff"
    ymin = max(0, int(min(p[1] for p in pts)))
    ymax = min(height - 1, int(max(p[1] for p in pts)) + 1)
    for y in range(ymin, ymax + 1):
        yc = y + 0.5
        xs = []
        for i, a in enumerate(pts):
            b = pts[(i + 1) % len(pts)]
            if (a[1] <= yc) != (b[1] <= yc):
                xs.append(a[0] + (yc - a[1]) * (b[0] - a[0]) / (b[1] - a[1]))
        xs.sort()
        row = canvas[y]
        for j in range(0, len(xs) - 1, 2):
            x0, x1 = max(0, int(round(xs[j]))), min(width, int(round(xs[j + 1])))
            if x1 > x0:
                row[x0 * 4:x1 * 4] = rgba * (x1 - x0)


def render(size):
    ss = 4
    w = size * ss
    scale = w / BOX
    canvas = [bytearray(w * 4) for _ in range(w)]
    # the sky, as bands of a vertical gradient
    bands = 64
    bg = [(x, y) for x, y in BACKGROUND]
    for b in range(bands):
        t = b / (bands - 1)
        colour = tuple(int(SKY_TOP[i] + (SKY_BOTTOM[i] - SKY_TOP[i]) * t) for i in range(3))
        y0, y1 = BOX * b / bands, BOX * (b + 1) / bands
        band = []
        # the part of the rounded rectangle in this band
        left = min(x for x, y in bg if y0 - 8 <= y <= y1 + 8) if any(y0 - 8 <= y <= y1 + 8 for x, y in bg) else 4
        band = clip_band(bg, y0, y1)
        if band:
            fill(canvas, w, w, band, colour, scale)
    for points, colour in shapes(size):
        fill(canvas, w, w, points, colour, scale)
    # reduce with alpha weighted averaging
    out = []
    for y in range(size):
        row = bytearray(size * 4)
        rows = canvas[y * ss:(y + 1) * ss]
        for x in range(size):
            r = g = b = a = 0
            for src in rows:
                for k in range(x * ss, (x + 1) * ss):
                    al = src[k * 4 + 3]
                    r += src[k * 4] * al
                    g += src[k * 4 + 1] * al
                    b += src[k * 4 + 2] * al
                    a += al
            if a:
                row[x * 4:x * 4 + 4] = bytes((r // a, g // a, b // a, a // (ss * ss)))
        out.append(bytes(row))
    return out


def clip_band(poly, y0, y1):
    """Clip a convex polygon to a horizontal band."""
    def clip(points, keep, cross):
        result = []
        for i, a in enumerate(points):
            b = points[(i + 1) % len(points)]
            if keep(a):
                result.append(a)
            if keep(a) != keep(b):
                t = (cross - a[1]) / (b[1] - a[1])
                result.append((a[0] + (b[0] - a[0]) * t, cross))
        return result
    points = clip(poly, lambda p: p[1] >= y0, y0)
    return clip(points, lambda p: p[1] <= y1, y1) if points else []


def png(rows):
    def chunk(tag, data):
        return struct.pack(">I", len(data)) + tag + data + struct.pack(">I", zlib.crc32(tag + data))
    size = len(rows)
    raw = b"".join(b"\0" + r for r in rows)
    return (b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", size, size, 8, 6, 0, 0, 0)) +
            chunk(b"IDAT", zlib.compress(raw, 9)) + chunk(b"IEND", b""))


def svg():
    def path(points):
        return "M" + " L".join(f"{x:.2f},{y:.2f}" for x, y in points) + " Z"

    def hexcol(c):
        return "#%02x%02x%02x" % tuple(c)
    lines = ['<?xml version="1.0" encoding="UTF-8"?>',
             '<!-- Aurora Monitor icon. GPL-3.0-or-later, see README.txt. Generated by make_icon.py -->',
             '<svg xmlns="http://www.w3.org/2000/svg" width="256" height="256" viewBox="0 0 256 256">',
             '<title>Aurora Monitor</title>',
             '<defs><linearGradient id="sky" x1="0" y1="0" x2="0" y2="1">'
             f'<stop offset="0" stop-color="{hexcol(SKY_TOP)}"/><stop offset="1" stop-color="{hexcol(SKY_BOTTOM)}"/>'
             '</linearGradient></defs>',
             f'<path d="{path(BACKGROUND)}" fill="url(#sky)"/>']
    for points, colour in shapes(256):
        lines.append(f'<path d="{path(points)}" fill="{hexcol(colour)}"/>')
    lines.append("</svg>")
    return "\n".join(lines) + "\n"


def main():
    with open(os.path.join(HERE, "auroramon.svg"), "w") as f:
        f.write(svg())
    header = ["// Generated by icons/make_icon.py from the Aurora Monitor icon (GPL-3.0-or-later). Don't edit.",
              "", "#ifndef AURORAMON_ICON_H", "#define AURORAMON_ICON_H", ""]
    table = []
    for size in SIZES:
        data = png(render(size))
        with open(os.path.join(HERE, f"auroramon_{size}.png"), "wb") as f:
            f.write(data)
        header.append(f"static const unsigned char icon_png_{size}[] = {{")
        for i in range(0, len(data), 20):
            header.append("    " + ",".join(str(b) for b in data[i:i + 20]) + ",")
        header.append("};")
        header.append("")
        table.append(f"    {{icon_png_{size}, sizeof(icon_png_{size})}},")
    header.append("static const struct {const unsigned char *data; size_t size;} icon_pngs[] = {")
    header.extend(table)
    header.extend(["};", "", "#endif", ""])
    with open(os.path.join(HERE, "..", "src", "auroramon_icon.h"), "w") as f:
        f.write("\n".join(header))
    print("wrote", len(SIZES), "icons")


if __name__ == "__main__":
    main()
