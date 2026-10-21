#include "png_write.h"

#include <stdio.h>
#include <string.h>

#include <vector>

static uint32_t g_crc[256];
static int g_crc_ready = 0;

static void crcInit() {
    if (g_crc_ready) return;
    for (uint32_t i = 0; i < 256; i++) {
        uint32_t c = i;
        for (int k = 0; k < 8; k++) c = (c & 1u) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1);
        g_crc[i] = c;
    }
    g_crc_ready = 1;
}

static uint32_t crc32(const uint8_t *data, size_t n) {
    crcInit();
    uint32_t c = 0xFFFFFFFFu;
    for (size_t i = 0; i < n; i++) c = g_crc[(c ^ data[i]) & 0xFFu] ^ (c >> 8);
    return ~c;
}

static void putBe32(std::vector<uint8_t> &o, uint32_t v) {
    o.push_back((uint8_t)(v >> 24));
    o.push_back((uint8_t)(v >> 16));
    o.push_back((uint8_t)(v >> 8));
    o.push_back((uint8_t)v);
}

static void chunk(std::vector<uint8_t> &o, const char type[4], const uint8_t *data, size_t n) {
    putBe32(o, (uint32_t)n);
    size_t at = o.size();
    o.push_back((uint8_t)type[0]);
    o.push_back((uint8_t)type[1]);
    o.push_back((uint8_t)type[2]);
    o.push_back((uint8_t)type[3]);
    if (n && data) o.insert(o.end(), data, data + n);
    uint32_t c = crc32(&o[at], 4 + n);
    putBe32(o, c);
}

static uint8_t expand5(uint16_t v) {
    uint8_t x = (uint8_t)(v & 0x1Fu);
    return (uint8_t)((x << 3) | (x >> 2));
}

static uint8_t expand6(uint16_t v) {
    uint8_t x = (uint8_t)(v & 0x3Fu);
    return (uint8_t)((x << 2) | (x >> 4));
}

bool pngWriteRgb565(const char *path, const uint16_t *px, int w, int h, bool roundMask) {
    if (!path || !px || w <= 0 || h <= 0) return false;

    const float cx = w * 0.5f;
    const float cy = h * 0.5f;
    const float r = (w < h ? w : h) * 0.5f;
    const float r2 = r * r;

    std::vector<uint8_t> raw;
    raw.reserve((size_t)(w * 4 + 1) * (size_t)h);
    uint32_t adlA = 1, adlB = 0;
    auto adler = [&](uint8_t b) {
        adlA = (adlA + b) % 65521u;
        adlB = (adlB + adlA) % 65521u;
    };

    for (int y = 0; y < h; y++) {
        raw.push_back(0);
        adler(0);
        for (int x = 0; x < w; x++) {
            uint16_t c = px[y * w + x];
            uint8_t r8 = expand5(c >> 11);
            uint8_t g8 = expand6((c >> 5) & 0x3F);
            uint8_t b8 = expand5(c & 0x1F);
            uint8_t a = 255;
            if (roundMask) {
                float dx = (x + 0.5f) - cx;
                float dy = (y + 0.5f) - cy;
                if (dx * dx + dy * dy > r2) a = 0;
            }
            raw.push_back(r8);
            raw.push_back(g8);
            raw.push_back(b8);
            raw.push_back(a);
            adler(r8);
            adler(g8);
            adler(b8);
            adler(a);
        }
    }

    std::vector<uint8_t> zlib;
    zlib.push_back(0x78);
    zlib.push_back(0x01);
    size_t off = 0;
    while (off < raw.size()) {
        size_t n = raw.size() - off;
        if (n > 65535) n = 65535;
        int last = (off + n) == raw.size();
        zlib.push_back(last ? 0x01 : 0x00);
        zlib.push_back((uint8_t)(n & 0xFF));
        zlib.push_back((uint8_t)((n >> 8) & 0xFF));
        uint16_t nlen = (uint16_t)(~(uint16_t)n);
        zlib.push_back((uint8_t)(nlen & 0xFF));
        zlib.push_back((uint8_t)((nlen >> 8) & 0xFF));
        zlib.insert(zlib.end(), raw.begin() + off, raw.begin() + off + n);
        off += n;
    }
    uint32_t adlerSum = (adlB << 16) | adlA;
    zlib.push_back((uint8_t)(adlerSum >> 24));
    zlib.push_back((uint8_t)(adlerSum >> 16));
    zlib.push_back((uint8_t)(adlerSum >> 8));
    zlib.push_back((uint8_t)adlerSum);

    uint8_t ihdr[13];
    ihdr[0] = (uint8_t)(w >> 24);
    ihdr[1] = (uint8_t)(w >> 16);
    ihdr[2] = (uint8_t)(w >> 8);
    ihdr[3] = (uint8_t)w;
    ihdr[4] = (uint8_t)(h >> 24);
    ihdr[5] = (uint8_t)(h >> 16);
    ihdr[6] = (uint8_t)(h >> 8);
    ihdr[7] = (uint8_t)h;
    ihdr[8] = 8;
    ihdr[9] = 6;
    ihdr[10] = 0;
    ihdr[11] = 0;
    ihdr[12] = 0;

    std::vector<uint8_t> png;
    const uint8_t sig[8] = {137, 80, 78, 71, 13, 10, 26, 10};
    png.insert(png.end(), sig, sig + 8);
    chunk(png, "IHDR", ihdr, 13);
    chunk(png, "IDAT", zlib.data(), zlib.size());
    chunk(png, "IEND", nullptr, 0);

    FILE *f = fopen(path, "wb");
    if (!f) return false;
    size_t n = fwrite(png.data(), 1, png.size(), f);
    fclose(f);
    return n == png.size();
}
