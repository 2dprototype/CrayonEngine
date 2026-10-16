#include "activation_core.hpp"

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>

#if defined(_WIN32)
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #ifndef NOMINMAX
        #define NOMINMAX
    #endif
    #include <windows.h>
    #ifndef RRF_SUBKEY_WOW6464KEY
        #define RRF_SUBKEY_WOW6464KEY 0x00010000
    #endif
#endif

namespace crayon::activation {

// =============================================================================================
// SHA-256 / HMAC
// =============================================================================================
namespace {

inline uint32_t rotr(uint32_t x, int n) { return (x >> n) | (x << (32 - n)); }

const uint32_t K[64] = {
    0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
    0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
    0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
    0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
    0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
    0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
    0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
    0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2};

struct Sha256 {
    uint32_t h[8] = {0x6a09e667,0xbb67ae85,0x3c6ef372,0xa54ff53a,0x510e527f,0x9b05688c,0x1f83d9ab,0x5be0cd19};
    uint8_t  buf[64];
    size_t   buflen = 0;
    uint64_t total = 0;

    void block(const uint8_t* p) {
        uint32_t w[64];
        for (int i = 0; i < 16; ++i)
            w[i] = (uint32_t(p[i*4]) << 24) | (uint32_t(p[i*4+1]) << 16) | (uint32_t(p[i*4+2]) << 8) | uint32_t(p[i*4+3]);
        for (int i = 16; i < 64; ++i) {
            uint32_t s0 = rotr(w[i-15], 7) ^ rotr(w[i-15], 18) ^ (w[i-15] >> 3);
            uint32_t s1 = rotr(w[i-2], 17) ^ rotr(w[i-2], 19) ^ (w[i-2] >> 10);
            w[i] = w[i-16] + s0 + w[i-7] + s1;
        }
        uint32_t a=h[0],b=h[1],c=h[2],d=h[3],e=h[4],f=h[5],g=h[6],hh=h[7];
        for (int i = 0; i < 64; ++i) {
            uint32_t S1 = rotr(e,6) ^ rotr(e,11) ^ rotr(e,25);
            uint32_t ch = (e & f) ^ (~e & g);
            uint32_t t1 = hh + S1 + ch + K[i] + w[i];
            uint32_t S0 = rotr(a,2) ^ rotr(a,13) ^ rotr(a,22);
            uint32_t mj = (a & b) ^ (a & c) ^ (b & c);
            uint32_t t2 = S0 + mj;
            hh=g; g=f; f=e; e=d+t1; d=c; c=b; b=a; a=t1+t2;
        }
        h[0]+=a; h[1]+=b; h[2]+=c; h[3]+=d; h[4]+=e; h[5]+=f; h[6]+=g; h[7]+=hh;
    }
    void update(const uint8_t* d, size_t n) {
        total += n;
        while (n > 0) {
            size_t take = 64 - buflen; if (take > n) take = n;
            std::memcpy(buf + buflen, d, take);
            buflen += take; d += take; n -= take;
            if (buflen == 64) { block(buf); buflen = 0; }
        }
    }
    Bytes finish() {
        const uint64_t bits = total * 8;
        const uint8_t pad = 0x80;
        update(&pad, 1);
        const uint8_t zero = 0;
        while (buflen != 56) update(&zero, 1);
        uint8_t len[8];
        for (int i = 0; i < 8; ++i) len[i] = uint8_t(bits >> (56 - 8 * i));
        update(len, 8);
        Bytes out(32);
        for (int i = 0; i < 8; ++i) {
            out[i*4]   = uint8_t(h[i] >> 24); out[i*4+1] = uint8_t(h[i] >> 16);
            out[i*4+2] = uint8_t(h[i] >> 8);  out[i*4+3] = uint8_t(h[i]);
        }
        return out;
    }
};

const char* kAlphabet = "ABCDEFGHJKLMNPQRSTUVWXYZ23456789";   // no I, O, 0, 1

} // namespace

Bytes sha256(const uint8_t* data, size_t n) {
    Sha256 s;
    s.update(data, n);
    return s.finish();
}

Bytes hmac_sha256(const std::string& key, const uint8_t* data, size_t n) {
    uint8_t k[64] = {0};
    if (key.size() > 64) {
        Bytes kh = sha256(reinterpret_cast<const uint8_t*>(key.data()), key.size());
        std::memcpy(k, kh.data(), 32);
    } else {
        std::memcpy(k, key.data(), key.size());
    }
    uint8_t ipad[64], opad[64];
    for (int i = 0; i < 64; ++i) { ipad[i] = k[i] ^ 0x36; opad[i] = k[i] ^ 0x5c; }
    Sha256 in;  in.update(ipad, 64);  in.update(data, n);
    Bytes inner = in.finish();
    Sha256 out; out.update(opad, 64); out.update(inner.data(), inner.size());
    return out.finish();
}

std::string to_hex(const uint8_t* d, size_t n) {
    static const char* hx = "0123456789abcdef";
    std::string s;
    for (size_t i = 0; i < n; ++i) { s.push_back(hx[d[i] >> 4]); s.push_back(hx[d[i] & 15]); }
    return s;
}

bool constant_time_equal(const uint8_t* a, const uint8_t* b, size_t n) {
    uint8_t diff = 0;
    for (size_t i = 0; i < n; ++i) diff |= uint8_t(a[i] ^ b[i]);
    return diff == 0;
}

// =============================================================================================
// license keys
// =============================================================================================
namespace {

constexpr size_t kPayload = 20, kTag = 10, kKeyBytes = kPayload + kTag, kKeyChars = 48;

Bytes product_tag(const std::string& product) {
    const std::string s = "product:" + product;
    return sha256(reinterpret_cast<const uint8_t*>(s.data()), s.size());
}

void put32(uint8_t* p, uint32_t v) { p[0] = uint8_t(v >> 24); p[1] = uint8_t(v >> 16); p[2] = uint8_t(v >> 8); p[3] = uint8_t(v); }
uint32_t get32(const uint8_t* p) { return (uint32_t(p[0]) << 24) | (uint32_t(p[1]) << 16) | (uint32_t(p[2]) << 8) | uint32_t(p[3]); }

Bytes key_tag(const std::string& secret, const uint8_t* payload) {
    Bytes msg = {'k','e','y','-','v','1'};
    msg.insert(msg.end(), payload, payload + kPayload);
    Bytes mac = hmac_sha256(secret, msg.data(), msg.size());
    mac.resize(kTag);
    return mac;
}

} // namespace

std::string normalize_key(const std::string& key) {
    std::string s;
    for (char c : key) {
        if (std::isalnum(static_cast<unsigned char>(c))) s.push_back(char(std::toupper(static_cast<unsigned char>(c))));
    }
    return s;
}

std::string make_key(const std::string& secret, const std::string& product, const License& lic) {
    uint8_t raw[kKeyBytes] = {0};
    raw[0] = 1;
    const Bytes pt = product_tag(product);
    std::memcpy(raw + 1, pt.data(), 4);
    raw[5] = lic.edition;
    put32(raw + 6, lic.features);
    raw[10] = uint8_t(lic.expiry_day >> 8);
    raw[11] = uint8_t(lic.expiry_day);
    put32(raw + 12, lic.machine);
    put32(raw + 16, lic.serial);
    const Bytes tag = key_tag(secret, raw);
    std::memcpy(raw + kPayload, tag.data(), kTag);

    std::string chars;
    uint32_t acc = 0; int bits = 0;
    for (size_t i = 0; i < kKeyBytes; ++i) {
        acc = (acc << 8) | raw[i]; bits += 8;
        while (bits >= 5) { chars.push_back(kAlphabet[(acc >> (bits - 5)) & 31]); bits -= 5; }
    }
    // 240 bits = exactly 48 chars, nothing left over.
    std::string out;
    for (size_t i = 0; i < chars.size(); ++i) {
        if (i && i % 6 == 0) out.push_back('-');
        out.push_back(chars[i]);
    }
    return out;
}

KeyStatus parse_key(const std::string& secret, const std::string& product, const std::string& key, License& out) {
    const std::string s = normalize_key(key);
    if (s.size() != kKeyChars) return KeyStatus::Malformed;

    uint8_t raw[kKeyBytes];
    size_t n = 0; uint32_t acc = 0; int bits = 0;
    for (char c : s) {
        const char* p = std::strchr(kAlphabet, c);
        if (!p || !c) return KeyStatus::Malformed;
        acc = (acc << 5) | uint32_t(p - kAlphabet); bits += 5;
        if (bits >= 8) { if (n < kKeyBytes) raw[n++] = uint8_t(acc >> (bits - 8)); bits -= 8; }
    }
    if (n != kKeyBytes || raw[0] != 1) return KeyStatus::Malformed;

    const Bytes tag = key_tag(secret, raw);
    if (!constant_time_equal(tag.data(), raw + kPayload, kTag)) return KeyStatus::BadSignature;

    const Bytes pt = product_tag(product);
    if (!constant_time_equal(pt.data(), raw + 1, 4)) return KeyStatus::WrongProduct;

    out.edition    = raw[5];
    out.features   = get32(raw + 6);
    out.expiry_day = uint16_t((raw[10] << 8) | raw[11]);
    out.machine    = get32(raw + 12);
    out.serial     = get32(raw + 16);
    return KeyStatus::Ok;
}

// =============================================================================================
// machine id
// =============================================================================================
std::string raw_machine_id() {
    std::string id;
#if defined(_WIN32)
    char buf[256] = {0};
    DWORD sz = sizeof(buf);
    if (RegGetValueA(HKEY_LOCAL_MACHINE, "SOFTWARE\\Microsoft\\Cryptography", "MachineGuid",
                     RRF_RT_REG_SZ | RRF_SUBKEY_WOW6464KEY, nullptr, buf, &sz) == ERROR_SUCCESS) {
        id = buf;
    }
#elif defined(__APPLE__)
    if (FILE* p = popen("ioreg -rd1 -c IOPlatformExpertDevice 2>/dev/null", "r")) {
        char line[512];
        while (std::fgets(line, sizeof(line), p)) {
            const char* u = std::strstr(line, "IOPlatformUUID");
            if (!u) continue;
            const char* q1 = std::strchr(u + 14, '"');
            if (q1) q1 = std::strchr(q1 + 1, '"');   // skip the opening quote of the value
            const char* q2 = q1 ? std::strchr(q1 + 1, '"') : nullptr;
            if (q1 && q2) id.assign(q1 + 1, q2);
            break;
        }
        pclose(p);
    }
#else
    for (const char* path : {"/etc/machine-id", "/var/lib/dbus/machine-id"}) {
        std::ifstream f(path);
        if (f && std::getline(f, id) && !id.empty()) break;
        id.clear();
    }
#endif
    while (!id.empty() && std::isspace(static_cast<unsigned char>(id.back()))) id.pop_back();
    if (id.empty()) {
        const char* h = std::getenv("HOSTNAME");
        if (!h) h = std::getenv("COMPUTERNAME");
        id = std::string("host:") + (h ? h : "unknown");
    }
    return id;
}

Bytes machine_hash(const std::string& raw_id) {
    const std::string s = "crayon-machine:" + raw_id;
    return sha256(reinterpret_cast<const uint8_t*>(s.data()), s.size());
}

std::string machine_display_id(const Bytes& h) {
    std::string hex = to_hex(h.data(), 8);
    for (char& c : hex) c = char(std::toupper(static_cast<unsigned char>(c)));
    return hex.substr(0, 4) + "-" + hex.substr(4, 4) + "-" + hex.substr(8, 4) + "-" + hex.substr(12, 4);
}

uint32_t machine_tag(const Bytes& h) { return get32(h.data()); }

bool parse_machine_display_id(const std::string& text, uint32_t& tag_out) {
    std::string hex;
    for (char c : text) if (std::isxdigit(static_cast<unsigned char>(c))) hex.push_back(c);
    if (hex.size() < 8) return false;
    tag_out = uint32_t(std::strtoul(hex.substr(0, 8).c_str(), nullptr, 16));
    return true;
}

// =============================================================================================
// dates
// =============================================================================================
int64_t day_of(int64_t t) {
    int64_t d = (t - kEpoch2020) / 86400;
    if ((t - kEpoch2020) < 0 && ((t - kEpoch2020) % 86400) != 0) --d;
    return d;
}

bool parse_date(const std::string& s, int64_t& day_out) {
    int y, m, d;
    if (std::sscanf(s.c_str(), "%d-%d-%d", &y, &m, &d) != 3 || m < 1 || m > 12 || d < 1 || d > 31) return false;
    // days_from_civil (Howard Hinnant)
    y -= m <= 2;
    const int64_t era = (y >= 0 ? y : y - 399) / 400;
    const unsigned yoe = unsigned(y - era * 400);
    const unsigned doy = unsigned((153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1);
    const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    const int64_t days_since_1970 = era * 146097 + int64_t(doe) - 719468;
    day_out = days_since_1970 - kEpoch2020 / 86400;
    return true;
}

} // namespace crayon::activation
