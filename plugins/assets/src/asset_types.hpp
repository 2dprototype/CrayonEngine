#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <cstdint>

namespace crayon::assets {

enum class AssetType {
    Texture,
    Model,
    Sound,
    Music,
    Data,
    Unknown
};

inline const char* asset_type_to_string(AssetType t) {
    switch (t) {
        case AssetType::Texture: return "texture";
        case AssetType::Model:   return "model";
        case AssetType::Sound:   return "sound";
        case AssetType::Music:   return "music";
        case AssetType::Data:    return "data";
        default:                 return "unknown";
    }
}

inline AssetType string_to_asset_type(const std::string& str) {
    if (str == "texture" || str == "textures") return AssetType::Texture;
    if (str == "model"   || str == "models")   return AssetType::Model;
    if (str == "sound"   || str == "sounds")   return AssetType::Sound;
    if (str == "music")                        return AssetType::Music;
    if (str == "data")                         return AssetType::Data;
    return AssetType::Unknown;
}

struct AssetEntry {
    std::string id;
    std::string path;
    AssetType   type = AssetType::Unknown;
    std::string filter;
    std::string wrap;
};

struct AssetBundle {
    std::string             name;
    std::vector<AssetEntry> items;
    bool                    loaded = false;
};

struct QueueTask {
    std::string bundle_name;
    AssetEntry  entry;
};

} // namespace crayon::assets
