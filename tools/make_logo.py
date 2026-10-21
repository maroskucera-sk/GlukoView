"""Prevedie assets/logo.png na LVGL 8 RGB565 obrazok src/logo_boot.c."""
import struct
import zlib
from pathlib import Path

root = Path(__file__).resolve().parents[1]
src_png = root / "assets" / "logo.png"
out_c = root / "src" / "logo_boot.c"


def read_png(path: Path):
    data = path.read_bytes()
    assert data[:8] == b"\x89PNG\r\n\x1a\n"
    pos = 8
    width = height = None
    color_type = None
    idat = b""
    while pos < len(data):
        length = struct.unpack(">I", data[pos:pos + 4])[0]
        ctype = data[pos + 4:pos + 8]
        chunk = data[pos + 8:pos + 8 + length]
        pos += 12 + length
        if ctype == b"IHDR":
            width, height, bit_depth, color_type = struct.unpack(">IIBB", chunk[:10])
            if bit_depth != 8 or color_type not in (2, 6):
                raise SystemExit(f"PNG musi byt 8-bit RGB alebo RGBA, je {bit_depth}/{color_type}")
        elif ctype == b"IDAT":
            idat += chunk
        elif ctype == b"IEND":
            break
    raw = zlib.decompress(idat)
    channels = 4 if color_type == 6 else 3
    stride = width * channels
    rows = []
    i = 0
    prev = bytearray(stride)
    for _ in range(height):
        filt = raw[i]
        i += 1
        row = bytearray(raw[i:i + stride])
        i += stride
        if filt == 1:  # Sub
            for x in range(stride):
                left = row[x - channels] if x >= channels else 0
                row[x] = (row[x] + left) & 255
        elif filt == 2:  # Up
            for x in range(stride):
                row[x] = (row[x] + prev[x]) & 255
        elif filt == 3:  # Average
            for x in range(stride):
                left = row[x - channels] if x >= channels else 0
                row[x] = (row[x] + ((left + prev[x]) // 2)) & 255
        elif filt == 4:  # Paeth
            for x in range(stride):
                a = row[x - channels] if x >= channels else 0
                b = prev[x]
                c = prev[x - channels] if x >= channels else 0
                p = a + b - c
                pa, pb, pc = abs(p - a), abs(p - b), abs(p - c)
                pr = a if pa <= pb and pa <= pc else b if pb <= pc else c
                row[x] = (row[x] + pr) & 255
        elif filt != 0:
            raise SystemExit(f"neznamy PNG filter {filt}")
        prev = row
        rows.append(row)
    return width, height, channels, rows


def rgb565(r, g, b):
    value = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)
    return value & 0xFF, (value >> 8) & 0xFF


def main():
    width, height, channels, rows = read_png(src_png)
    pixels = []
    for row in rows:
        for x in range(width):
            i = x * channels
            r, g, b = row[i], row[i + 1], row[i + 2]
            if channels == 4:
                a = row[i + 3] / 255.0
                r = int(r * a)
                g = int(g * a)
                b = int(b * a)
            pixels.append(rgb565(r, g, b))

    lines = [
        '#include "lvgl.h"',
        "",
        "/* Vygenerovane z assets/logo.png. Znova: python tools/make_logo.py */",
        "",
        "const uint8_t logo_boot_map[] = {",
    ]
    row_bytes = []
    for lo, hi in pixels:
        row_bytes.append(f"0x{lo:02x}")
        row_bytes.append(f"0x{hi:02x}")
    for i in range(0, len(row_bytes), 16):
        lines.append("    " + ", ".join(row_bytes[i:i + 16]) + ",")
    lines += [
        "};",
        "",
        "const lv_img_dsc_t logo_boot = {",
        "    .header.always_zero = 0,",
        f"    .header.w = {width},",
        f"    .header.h = {height},",
        f"    .data_size = {width * height} * LV_COLOR_SIZE / 8,",
        "    .header.cf = LV_IMG_CF_TRUE_COLOR,",
        "    .data = logo_boot_map,",
        "};",
        "",
    ]
    out_c.write_text("\n".join(lines), encoding="utf-8")
    print(f"{width}x{height} -> {out_c} ({len(pixels) * 2} B)")


if __name__ == "__main__":
    main()
