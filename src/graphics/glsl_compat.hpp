#pragma once

#include <cctype>
#include <string>

namespace crayon {

// Rewrites a desktop-GLSL source (`#version 330 core`, 150, 140, 130) so that it
// compiles as GLSL ES 3.00 (`#version 300 es`). Sources that already declare
// `es`, or that have no/ancient (<130) #version line, are returned unchanged.
//
// Why this is enough for Crayon: every built-in shader and every Lua example
// shader uses only the GLSL 1.30+ syntax (`in`/`out`, `layout(location)`,
// `texture()`), which is identical in ES 3.00. The only real differences are the
// version line and the fact that ES fragment shaders have no default float
// precision, so we add precision statements right after the version line.
//
// The inserted lines shift compiler error line numbers by +4 on Android.
inline std::string adapt_glsl_for_gles(const std::string& src) {
    size_t pos = 0;
    while (pos < src.size()) {
        size_t eol = src.find('\n', pos);
        size_t line_end = (eol == std::string::npos) ? src.size() : eol;
        size_t i = pos;
        while (i < line_end && std::isspace(static_cast<unsigned char>(src[i]))) ++i;

        if (src.compare(i, 8, "#version") == 0) {
            // Parse "<number> [profile]"
            size_t j = i + 8;
            while (j < line_end && std::isspace(static_cast<unsigned char>(src[j]))) ++j;
            size_t num_start = j;
            while (j < line_end && std::isdigit(static_cast<unsigned char>(src[j]))) ++j;
            if (j == num_start) return src; // malformed
            int version = std::stoi(src.substr(num_start, j - num_start));

            std::string rest = src.substr(j, line_end - j);
            if (rest.find("es") != std::string::npos) return src; // already ES
            if (version < 130) return src; // legacy attribute/varying GLSL: cannot auto-port

            std::string out;
            out.reserve(src.size() + 96);
            out.append(src, 0, pos);                       // anything before (comments)
            out += "#version 300 es\n"
                   "precision highp float;\n"
                   "precision highp int;\n"
                   "precision highp sampler2D;\n";
            if (eol != std::string::npos) out.append(src, eol + 1, std::string::npos);
            return out;
        }

        // Only comments / blank lines may precede #version.
        bool blank = (i >= line_end);
        bool comment = (i + 1 < line_end && src[i] == '/' && (src[i + 1] == '/' || src[i + 1] == '*'));
        if (!blank && !comment) return src;

        if (eol == std::string::npos) break;
        pos = eol + 1;
    }
    return src;
}

} // namespace crayon
