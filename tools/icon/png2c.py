"""Convert a PNG to an LVGL v9 RGB565 lv_image_dsc_t C file, matching main/assets/images/icon_*.c.

Usage: python3 png2c.py <input.png> <output.c> <symbol_name>
Example: python3 png2c.py icon_qr_manager.png ../../main/assets/images/icon_qr_manager.c icon_qr_manager
Only the standard library is needed. Alpha is composited over black like the existing icons.
"""
import struct, sys, zlib

def read_png(path):
    d = open(path, 'rb').read()
    assert d[:8] == b'\x89PNG\r\n\x1a\n'
    i, idat = 8, b''
    while i < len(d):
        l = struct.unpack('>I', d[i:i+4])[0]; t = d[i+4:i+8]; c = d[i+8:i+8+l]
        if t == b'IHDR':
            w, h, depth, ctype, _, _, interlace = struct.unpack('>IIBBBBB', c)
            assert depth == 8 and ctype in (2, 6) and interlace == 0, (depth, ctype, interlace)
        elif t == b'IDAT':
            idat += c
        i += 12 + l
    bpp = 4 if ctype == 6 else 3
    raw = zlib.decompress(idat); stride = w * bpp
    rows, prev, p = [], bytearray(stride), 0
    for _ in range(h):
        f = raw[p]; line = bytearray(raw[p+1:p+1+stride]); p += 1 + stride
        for x in range(stride):
            a = line[x-bpp] if x >= bpp else 0; b = prev[x]; c = prev[x-bpp] if x >= bpp else 0
            if f == 1: line[x] = (line[x] + a) & 255
            elif f == 2: line[x] = (line[x] + b) & 255
            elif f == 3: line[x] = (line[x] + (a + b) // 2) & 255
            elif f == 4:
                pa, pb, pc = abs(b - c), abs(a - c), abs(a + b - 2 * c)
                line[x] = (line[x] + (a if pa <= pb and pa <= pc else b if pb <= pc else c)) & 255
        rows.append(bytes(line)); prev = line
    return w, h, bpp, rows

src, dst, name = sys.argv[1:4]
w, h, bpp, rows = read_png(src)
out = bytearray()
for row in rows:
    for x in range(w):
        r, g, b = row[x*bpp:x*bpp+3]
        a = row[x*bpp+3] if bpp == 4 else 255
        # Composite over black like the existing icons
        r, g, b = r * a // 255, g * a // 255, b * a // 255
        v = (r >> 3) << 11 | (g >> 2) << 5 | (b >> 3)
        out += struct.pack('<H', v)

attr = 'LV_ATTRIBUTE_IMAGE_' + name.upper()
lines = [', '.join(f'0x{v:02x}' for v in out[i:i+19]) + ',' for i in range(0, len(out), 19)]
open(dst, 'w').write(f'''#ifdef __has_include
#if __has_include("lvgl.h")
#ifndef LV_LVGL_H_INCLUDE_SIMPLE
#define LV_LVGL_H_INCLUDE_SIMPLE
#endif
#endif
#endif

#if defined(LV_LVGL_H_INCLUDE_SIMPLE)
#include "lvgl.h"
#else
#include "lvgl/lvgl.h"
#endif

#ifndef LV_ATTRIBUTE_MEM_ALIGN
#define LV_ATTRIBUTE_MEM_ALIGN
#endif

#ifndef {attr}
#define {attr}
#endif

const LV_ATTRIBUTE_MEM_ALIGN LV_ATTRIBUTE_LARGE_CONST {attr} uint8_t {name}_map[] = {{
''' + '\n'.join('    ' + l for l in lines) + f'''
}};

const lv_image_dsc_t {name} = {{
    .header.cf    = LV_COLOR_FORMAT_RGB565,
    .header.magic = LV_IMAGE_HEADER_MAGIC,
    .header.w     = {w},
    .header.h     = {h},
    .data_size    = {w * h} * 2,
    .data         = {name}_map,
}};
''')
print(f'{name}: {w}x{h} -> {dst}')
