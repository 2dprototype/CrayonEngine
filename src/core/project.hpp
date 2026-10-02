#pragma once

#include <string>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cctype>
#include "log.hpp"

namespace crayon {

struct CrayonProject {
    std::string name;
    std::string version = "1.0.0";
    std::string author;
    std::string description;
    std::string main = "main.lua";
    std::string targetSrc;
    std::string license = "MIT";

    std::string resolveTargetScript(const std::filesystem::path& projectDir) const {
        std::string entry = !targetSrc.empty() ? targetSrc : (!main.empty() ? main : "main.lua");
        std::filesystem::path p = std::filesystem::absolute(projectDir) / entry;
        return std::filesystem::absolute(p).lexically_normal().string();
    }
};

namespace detail {

inline std::string trim(const std::string& s) {
    auto start = s.begin();
    while (start != s.end() && std::isspace(static_cast<unsigned char>(*start))) {
        ++start;
    }
    auto end = s.end();
    do {
        --end;
    } while (std::distance(start, end) > 0 && std::isspace(static_cast<unsigned char>(*end)));
    return std::string(start, end + 1);
}

inline std::string stripComments(const std::string& json) {
    std::string result;
    result.reserve(json.size());
    bool inString = false;
    bool inLineComment = false;
    bool inBlockComment = false;

    for (size_t i = 0; i < json.size(); ++i) {
        char c = json[i];
        char next = (i + 1 < json.size()) ? json[i + 1] : '\0';

        if (inLineComment) {
            if (c == '\n' || c == '\r') {
                inLineComment = false;
                result += c;
            }
            continue;
        }

        if (inBlockComment) {
            if (c == '*' && next == '/') {
                inBlockComment = false;
                ++i;
            }
            continue;
        }

        if (inString) {
            result += c;
            if (c == '\\' && next != '\0') {
                result += next;
                ++i;
            } else if (c == '"') {
                inString = false;
            }
            continue;
        }

        if (c == '"') {
            inString = true;
            result += c;
            continue;
        }

        if (c == '/' && next == '/') {
            inLineComment = true;
            ++i;
            continue;
        }

        if (c == '/' && next == '*') {
            inBlockComment = true;
            ++i;
            continue;
        }

        result += c;
    }
    return result;
}

inline bool extractStringField(const std::string& json, const std::string& key, std::string& outValue) {
    std::string pattern = "\"" + key + "\"";
    size_t keyPos = json.find(pattern);
    if (keyPos == std::string::npos) {
        return false;
    }

    size_t colonPos = json.find(':', keyPos + pattern.size());
    if (colonPos == std::string::npos) {
        return false;
    }

    size_t quoteStart = json.find('\"', colonPos + 1);
    if (quoteStart == std::string::npos) {
        return false;
    }

    size_t quoteEnd = quoteStart + 1;
    while (quoteEnd < json.size()) {
        if (json[quoteEnd] == '\\') {
            quoteEnd += 2;
            continue;
        }
        if (json[quoteEnd] == '\"') {
            break;
        }
        ++quoteEnd;
    }

    if (quoteEnd >= json.size()) {
        return false;
    }

    outValue = json.substr(quoteStart + 1, quoteEnd - quoteStart - 1);
    return true;
}

} // namespace detail

inline bool loadCrayonProject(const std::filesystem::path& manifestPath, CrayonProject& outProject) {
    if (!std::filesystem::exists(manifestPath)) {
        CRAYON_LOG_ERROR("Project manifest not found at: {}", manifestPath.string());
        return false;
    }

    std::ifstream file(manifestPath);
    if (!file.is_open()) {
        CRAYON_LOG_ERROR("Failed to open project manifest: {}", manifestPath.string());
        return false;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string rawJson = buffer.str();
    std::string cleanJson = detail::stripComments(rawJson);

    detail::extractStringField(cleanJson, "name", outProject.name);
    detail::extractStringField(cleanJson, "version", outProject.version);
    detail::extractStringField(cleanJson, "author", outProject.author);
    detail::extractStringField(cleanJson, "description", outProject.description);
    detail::extractStringField(cleanJson, "main", outProject.main);
    detail::extractStringField(cleanJson, "targetSrc", outProject.targetSrc);
    detail::extractStringField(cleanJson, "license", outProject.license);

    CRAYON_LOG_INFO("Loaded .crayonproj [Name='{}', Version='{}', Main='{}', TargetSrc='{}']",
        outProject.name, outProject.version, outProject.main, outProject.targetSrc);

    return true;
}

} // namespace crayon
