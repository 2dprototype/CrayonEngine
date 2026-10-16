#pragma once

// Core of the activation plugin: SHA-256 / HMAC, the license-key format and the machine id.
// No Lua, no engine - shared by the plugin and the crayon_keygen tool.
//
// License key = 30 bytes, shown as 8 groups of 6 base32 characters (XXXXXX-XXXXXX-...):
//   payload (20 bytes): version | product tag(4) | edition | feature bits(4) | expiry day(2) | machine tag(4) | serial(4)
//   tag     (10 bytes): HMAC-SHA256(secret, "key-v1" + payload), truncated

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace crayon::activation {

using Bytes = std::vector<uint8_t>;

constexpr int64_t kEpoch2020 = 1577836800;   // expiry days are counted from 2020-01-01 UTC

Bytes sha256(const uint8_t* data, size_t n);
Bytes hmac_sha256(const std::string& key, const uint8_t* data, size_t n);
std::string to_hex(const uint8_t* d, size_t n);
bool constant_time_equal(const uint8_t* a, const uint8_t* b, size_t n);

struct License {
    uint8_t  edition    = 0;
    uint32_t features   = 0;   // bit i = feature i of crayon.activation.configure{features={...}}
    uint16_t expiry_day = 0;   // 0 = never; else last valid day, counted from 2020-01-01
    uint32_t machine    = 0;   // 0 = any machine; else machine_tag() of the allowed machine
    uint32_t serial     = 0;
};

enum class KeyStatus { Ok, Malformed, BadSignature, WrongProduct };

std::string normalize_key(const std::string& key);   // upper-case, strips spaces / dashes
std::string make_key(const std::string& secret, const std::string& product, const License& lic);
KeyStatus   parse_key(const std::string& secret, const std::string& product, const std::string& key, License& out);

// ---- machine id ---------------------------------------------------------------------------
std::string raw_machine_id();                               // OS-level id (MachineGuid, /etc/machine-id, ...)
Bytes       machine_hash(const std::string& raw_id);        // sha256("crayon-machine:" + raw)
std::string machine_display_id(const Bytes& hash);          // "A1B2-C3D4-E5F6-0718"
uint32_t    machine_tag(const Bytes& hash);                 // what a machine-bound key stores
bool        parse_machine_display_id(const std::string& text, uint32_t& tag_out);

// ---- dates ---------------------------------------------------------------------------------
int64_t day_of(int64_t unix_time);                          // days since 2020-01-01 (UTC)
bool    parse_date(const std::string& yyyy_mm_dd, int64_t& day_out);

} // namespace crayon::activation
