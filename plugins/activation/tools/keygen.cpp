// crayon_keygen - generates license keys for the `activation` plugin.
//
//   crayon_keygen --product mygame [--edition 1] [--features 0,2] [--expires 2027-12-31 | --days 365]
//                 [--machine A1B2-C3D4-E5F6-0718] [--serial 1] [--count 10] [--secret S]
//
// The secret defaults to the one this tool was built with (same CRAYON_ACTIVATION_SECRET as the plugin);
// override with --secret or the CRAYON_ACTIVATION_SECRET environment variable.
// --features takes 0-based indices into the `features = {...}` list the game passes to configure().

#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <string>

#include "activation_core.hpp"

#ifndef CRAYON_ACTIVATION_SECRET
#define CRAYON_ACTIVATION_SECRET "CHANGE-ME-set-CRAYON_ACTIVATION_SECRET-in-cmake"
#endif

using namespace crayon::activation;

static void usage() {
    std::puts("usage: crayon_keygen --product NAME [--edition N] [--features i,j,...] [--expires YYYY-MM-DD | --days N]\n"
              "                     [--machine XXXX-XXXX-XXXX-XXXX] [--serial N] [--count N] [--secret S]");
}

int main(int argc, char** argv) {
    std::string product, secret = CRAYON_ACTIVATION_SECRET, features, expires, machine;
    int edition = 0, days = 0, count = 1;
    unsigned long serial = 1;
    if (const char* env = std::getenv("CRAYON_ACTIVATION_SECRET")) if (*env) secret = env;

    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        auto next = [&]() -> std::string { return i + 1 < argc ? argv[++i] : std::string(); };
        if (a == "--product") product = next();
        else if (a == "--edition") edition = std::atoi(next().c_str());
        else if (a == "--features") features = next();
        else if (a == "--expires") expires = next();
        else if (a == "--days") days = std::atoi(next().c_str());
        else if (a == "--machine") machine = next();
        else if (a == "--serial") serial = std::strtoul(next().c_str(), nullptr, 10);
        else if (a == "--count") count = std::atoi(next().c_str());
        else if (a == "--secret") secret = next();
        else { usage(); return a == "--help" || a == "-h" ? 0 : 1; }
    }
    if (product.empty() || count < 1 || edition < 0 || edition > 255) { usage(); return 1; }

    License lic;
    lic.edition = uint8_t(edition);
    for (size_t p = 0; p < features.size();) {
        const size_t c = features.find(',', p);
        const int bit = std::atoi(features.substr(p, c == std::string::npos ? c : c - p).c_str());
        if (bit < 0 || bit > 31) { std::fprintf(stderr, "feature index must be 0..31\n"); return 1; }
        lic.features |= 1u << bit;
        if (c == std::string::npos) break;
        p = c + 1;
    }
    if (!expires.empty()) {
        int64_t d;
        if (!parse_date(expires, d) || d < 1 || d > 65535) { std::fprintf(stderr, "bad --expires (use YYYY-MM-DD, after 2020)\n"); return 1; }
        lic.expiry_day = uint16_t(d);
    } else if (days > 0) {
        const int64_t d = day_of(std::time(nullptr)) + days;
        if (d < 1 || d > 65535) { std::fprintf(stderr, "bad --days\n"); return 1; }
        lic.expiry_day = uint16_t(d);
    }
    if (!machine.empty() && !parse_machine_display_id(machine, lic.machine)) {
        std::fprintf(stderr, "bad --machine id\n");
        return 1;
    }
    if (secret.rfind("CHANGE-ME", 0) == 0) std::fprintf(stderr, "warning: using the default secret\n");

    for (int i = 0; i < count; ++i) {
        lic.serial = uint32_t(serial + i);
        std::puts(make_key(secret, product, lic).c_str());
    }
    return 0;
}
