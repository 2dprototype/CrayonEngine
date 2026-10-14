#pragma once
// Minimal dependency-free PNG encoder (RGBA8, stored/uncompressed deflate blocks). Files are
// larger than a compressing encoder would produce but are valid PNGs readable everywhere, and
// there is nothing to link. Used by screenshots and ImageData:save().
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

namespace crayon::png {

inline uint32_t crc32(const uint8_t* data, size_t len, uint32_t crc = 0) {
    static uint32_t table[256];
    static bool init = false;
    if (!init) {
        for (uint32_t i = 0; i < 256; ++i) {
            uint32_t c = i;
            for (int k = 0; k < 8; ++k) c = (c & 1) ? 0xEDB88320u ^ (c >> 1) : (c >> 1);
            table[i] = c;
        }
        init = true;
    }
    crc = ~crc;
    for (size_t i = 0; i < len; ++i) crc = table[(crc ^ data[i]) & 0xFF] ^ (crc >> 8);
    return ~crc;
}

inline void put_be32(std::vector<uint8_t>& v, uint32_t x) {
    v.push_back(uint8_t(x >> 24)); v.push_back(uint8_t(x >> 16)); v.push_back(uint8_t(x >> 8)); v.push_back(uint8_t(x));
}

inline void put_chunk(std::vector<uint8_t>& out, const char type[4], const std::vector<uint8_t>& data) {
    put_be32(out, uint32_t(data.size()));
    size_t start = out.size();
    out.insert(out.end(), type, type + 4);
    out.insert(out.end(), data.begin(), data.end());
    put_be32(out, crc32(out.data() + start, out.size() - start));
}

// rgba: width*height*4 bytes, row 0 = top.
inline std::vector<uint8_t> encode_rgba(const uint8_t* rgba, int width, int height) {
    std::vector<uint8_t> raw;
    raw.reserve(size_t(height) * (size_t(width) * 4 + 1));
    for (int y = 0; y < height; ++y) {
        raw.push_back(0);   // filter: none
        raw.insert(raw.end(), rgba + size_t(y) * width * 4, rgba + size_t(y + 1) * width * 4);
    }
    // zlib stream with stored blocks
    std::vector<uint8_t> z;
    z.push_back(0x78); z.push_back(0x01);
    size_t pos = 0;
    while (pos < raw.size() || raw.empty()) {
        size_t n = std::min<size_t>(65535, raw.size() - pos);
        bool last = (pos + n >= raw.size());
        z.push_back(last ? 1 : 0);
        z.push_back(uint8_t(n & 0xFF)); z.push_back(uint8_t(n >> 8));
        z.push_back(uint8_t(~n & 0xFF)); z.push_back(uint8_t((~n >> 8) & 0xFF));
        z.insert(z.end(), raw.begin() + pos, raw.begin() + pos + n);
        pos += n;
        if (last) break;
    }
    uint32_t a = 1, b = 0;
    for (uint8_t c : raw) { a = (a + c) % 65521; b = (b + a) % 65521; }
    put_be32(z, (b << 16) | a);

    std::vector<uint8_t> out = {0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A};
    std::vector<uint8_t> ihdr;
    put_be32(ihdr, uint32_t(width)); put_be32(ihdr, uint32_t(height));
    ihdr.push_back(8); ihdr.push_back(6); ihdr.push_back(0); ihdr.push_back(0); ihdr.push_back(0);
    put_chunk(out, "IHDR", ihdr);
    put_chunk(out, "IDAT", z);
    put_chunk(out, "IEND", {});
    return out;
}

inline bool write_file(const std::string& path, const uint8_t* rgba, int width, int height) {
    if (!rgba || width <= 0 || height <= 0) return false;
    std::vector<uint8_t> bytes = encode_rgba(rgba, width, height);
    FILE* f = std::fopen(path.c_str(), "wb");
    if (!f) return false;
    bool ok = std::fwrite(bytes.data(), 1, bytes.size(), f) == bytes.size();
    std::fclose(f);
    return ok;
}

} // namespace crayon::png
