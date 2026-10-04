#include "rich_text.hpp"
#include "font.hpp"
#include <cmath>
#include <cstdio>
#include <cstring>
#include <cstdint>
#include <algorithm>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace crayon {

static glm::vec4 parseColorString(const std::string& str, const glm::vec4& fallback) {
    if (str.empty()) return fallback;

    if (str[0] == '#') {
        std::string hex = str.substr(1);
        if (hex.length() == 6) {
            unsigned int r, g, b;
            if (std::sscanf(hex.c_str(), "%02x%02x%02x", &r, &g, &b) == 3) {
                return glm::vec4(r / 255.0f, g / 255.0f, b / 255.0f, 1.0f);
            }
        } else if (hex.length() == 8) {
            unsigned int r, g, b, a;
            if (std::sscanf(hex.c_str(), "%02x%02x%02x%02x", &r, &g, &b, &a) == 4) {
                return glm::vec4(r / 255.0f, g / 255.0f, b / 255.0f, a / 255.0f);
            }
        }
    }

    if (str == "red") return glm::vec4(1.0f, 0.2f, 0.2f, 1.0f);
    if (str == "green") return glm::vec4(0.2f, 1.0f, 0.3f, 1.0f);
    if (str == "blue") return glm::vec4(0.3f, 0.5f, 1.0f, 1.0f);
    if (str == "yellow") return glm::vec4(1.0f, 0.95f, 0.2f, 1.0f);
    if (str == "cyan") return glm::vec4(0.2f, 0.95f, 1.0f, 1.0f);
    if (str == "magenta" || str == "purple") return glm::vec4(0.95f, 0.3f, 0.95f, 1.0f);
    if (str == "orange") return glm::vec4(1.0f, 0.6f, 0.1f, 1.0f);
    if (str == "white") return glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
    if (str == "black") return glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
    if (str == "gray") return glm::vec4(0.6f, 0.6f, 0.6f, 1.0f);

    float r = 1.0f, g = 1.0f, b = 1.0f, a = 1.0f;
    if (std::sscanf(str.c_str(), "%f,%f,%f,%f", &r, &g, &b, &a) >= 3) {
        return glm::vec4(r, g, b, a);
    }

    return fallback;
}

static glm::vec4 hsvToRgb(float h, float s, float v, float a = 1.0f) {
    h = std::fmod(h, 1.0f);
    if (h < 0.0f) h += 1.0f;
    float c = v * s;
    float x = c * (1.0f - std::abs(std::fmod(h * 6.0f, 2.0f) - 1.0f));
    float m = v - c;
    float r = 0, g = 0, b = 0;
    int sector = static_cast<int>(h * 6.0f);
    switch (sector) {
        case 0: r = c; g = x; b = 0; break;
        case 1: r = x; g = c; b = 0; break;
        case 2: r = 0; g = c; b = x; break;
        case 3: r = 0; g = x; b = c; break;
        case 4: r = x; g = 0; b = c; break;
        default: r = c; g = 0; b = x; break;
    }
    return glm::vec4(r + m, g + m, b + m, a);
}

struct GlyphState {
    char c = ' ';
    glm::vec4 color{1.0f};
    bool wave = false;
    float waveAmp = 3.0f;
    float waveFreq = 4.0f;
    bool shake = false;
    float shakeInt = 1.5f;
    bool rainbow = false;
    float rainbowSpeed = 1.5f;
    float scale = 1.0f;
    bool bold = false;
    int charIndex = 0;
};

// One laid-out line: glyphs [begin, end) of the parsed array. A space/newline
// dropped at a break is simply not inside any range.
struct RtLine {
    uint32_t begin = 0;
    uint32_t end = 0;
    float width = 0.0f;
    float maxScale = 0.0f; // largest glyph scale on the line (0 if empty)
};

// Reads the float that follows `key` inside a tag (e.g. "amp=" in "wave amp=3").
// Copies into a NUL-terminated buffer so sscanf can never run past the tag.
static bool readFloatAfter(std::string_view tag, std::string_view key, float& out) {
    size_t pos = tag.find(key);
    if (pos == std::string_view::npos) return false;
    std::string_view rest = tag.substr(pos + key.size());
    char buf[64];
    size_t n = std::min(rest.size(), sizeof(buf) - 1);
    std::memcpy(buf, rest.data(), n);
    buf[n] = '\0';
    float v = 0.0f;
    if (std::sscanf(buf, "%f", &v) == 1) {
        out = v;
        return true;
    }
    return false;
}

static std::vector<GlyphState> parseMarkup(const std::string& markup, float baseScale, const glm::vec4& defaultColor) {
    std::vector<GlyphState> glyphs;
    glyphs.reserve(markup.size());

    // Plain vectors instead of std::stack: std::stack defaults to std::deque,
    // which heap-allocates on construction (6 deques per call before).
    std::vector<glm::vec4> colorStack;
    colorStack.push_back(defaultColor);

    std::vector<std::pair<float, float>> waveStack; // amp, freq
    std::vector<float> shakeStack;                  // intensity
    std::vector<float> rainbowStack;                // speed
    std::vector<float> scaleStack;
    scaleStack.push_back(baseScale);
    int boldDepth = 0;

    int printIdx = 0;
    const std::string_view mv(markup);

    for (size_t i = 0; i < mv.size(); ++i) {
        if (mv[i] == '[') {
            size_t close = mv.find(']', i);
            if (close != std::string_view::npos) {
                std::string_view tag = mv.substr(i + 1, close - i - 1);
                i = close;

                // Closing tags
                if (!tag.empty() && tag[0] == '/') {
                    std::string_view endTag = tag.substr(1);
                    if (endTag == "color" && colorStack.size() > 1) colorStack.pop_back();
                    else if (endTag == "wave" && !waveStack.empty()) waveStack.pop_back();
                    else if (endTag == "shake" && !shakeStack.empty()) shakeStack.pop_back();
                    else if (endTag == "rainbow" && !rainbowStack.empty()) rainbowStack.pop_back();
                    else if (endTag == "scale" && scaleStack.size() > 1) scaleStack.pop_back();
                    else if (endTag == "b" && boldDepth > 0) --boldDepth;
                    continue;
                }

                // Opening tags
                if (tag.starts_with("color=")) {
                    std::string val(tag.substr(6));
                    colorStack.push_back(parseColorString(val, colorStack.back()));
                } else if (tag == "wave") {
                    waveStack.push_back({3.0f, 4.0f});
                } else if (tag.starts_with("wave")) {
                    float amp = 3.0f, freq = 4.0f;
                    readFloatAfter(tag, "amp=", amp);
                    readFloatAfter(tag, "freq=", freq);
                    waveStack.push_back({amp, freq});
                } else if (tag == "shake") {
                    shakeStack.push_back(1.5f);
                } else if (tag.starts_with("shake")) {
                    float intens = 1.5f;
                    readFloatAfter(tag, "int=", intens);
                    shakeStack.push_back(intens);
                } else if (tag == "rainbow") {
                    rainbowStack.push_back(1.5f);
                } else if (tag.starts_with("rainbow")) {
                    float spd = 1.5f;
                    readFloatAfter(tag, "speed=", spd);
                    rainbowStack.push_back(spd);
                } else if (tag.starts_with("scale=")) {
                    float sc = baseScale;
                    readFloatAfter(tag, "scale=", sc);
                    scaleStack.push_back(sc);
                } else if (tag == "b") {
                    ++boldDepth;
                }
                continue;
            }
        }

        // Printable char or newline
        GlyphState g;
        g.c = mv[i];
        g.color = colorStack.back();
        if (!waveStack.empty()) {
            g.wave = true;
            g.waveAmp = waveStack.back().first;
            g.waveFreq = waveStack.back().second;
        }
        if (!shakeStack.empty()) {
            g.shake = true;
            g.shakeInt = shakeStack.back();
        }
        if (!rainbowStack.empty()) {
            g.rainbow = true;
            g.rainbowSpeed = rainbowStack.back();
        }
        g.scale = scaleStack.back();
        g.bold = boldDepth > 0;
        g.charIndex = printIdx++;
        glyphs.push_back(g);
    }

    return glyphs;
}

// Greedy word-wrap layout over the parsed glyphs. Shared by drawing and
// measuring so the two can never disagree.
static void computeLayout(const std::vector<GlyphState>& glyphs, float wrapWidth, std::vector<RtLine>& lines) {
    constexpr float baseCharW = 8.0f;
    lines.clear();
    lines.push_back(RtLine{});

    const size_t n = glyphs.size();
    size_t lineStart = 0;
    float curW = 0.0f;
    bool haveSpace = false;
    size_t lastSpace = 0;
    float widthAtSpace = 0.0f;

    auto closeLine = [&](size_t end, float width) {
        RtLine& L = lines.back();
        L.end = static_cast<uint32_t>(end);
        L.width = width;
        float ms = 0.0f;
        for (size_t k = L.begin; k < end; ++k) ms = std::max(ms, glyphs[k].scale);
        L.maxScale = ms;
    };
    auto openLine = [&](size_t start) {
        RtLine L;
        L.begin = L.end = static_cast<uint32_t>(start);
        lines.push_back(L);
        lineStart = start;
    };

    for (size_t i = 0; i < n; ++i) {
        const GlyphState& g = glyphs[i];
        if (g.c == '\n') {
            closeLine(i, curW);
            openLine(i + 1);
            curW = 0.0f;
            haveSpace = false;
            widthAtSpace = 0.0f;
            continue;
        }

        const float charW = baseCharW * g.scale;
        const size_t count = i - lineStart; // glyphs already on this line

        if (wrapWidth > 0.0f && (curW + charW > wrapWidth) && count > 0) {
            if (haveSpace && lastSpace > lineStart && lastSpace < i) {
                // Break at the last space: drop the space, carry the partial word down.
                closeLine(lastSpace, widthAtSpace);
                openLine(lastSpace + 1);
                curW = 0.0f;
                for (size_t k = lineStart; k < i; ++k) curW += baseCharW * glyphs[k].scale;
            } else {
                // No usable space: hard break before this glyph.
                closeLine(i, curW);
                openLine(i);
                curW = 0.0f;
            }
            // The break point belongs to the previous line. (The old code left a
            // stale index behind after a hard break, which could split a word later.)
            haveSpace = false;
            widthAtSpace = 0.0f;
        }

        if (g.c == ' ') {
            haveSpace = true;
            lastSpace = i;
            widthAtSpace = curW;
        }
        curW += charW;
    }
    closeLine(n, curW);
}

// Parsed + laid-out markup, cached by markup string. Dynamic strings churn the
// cache, so it is bounded and simply dropped when full.
struct RtEntry {
    float scale = 1.0f;
    glm::vec4 color{1.0f};
    std::vector<GlyphState> glyphs;
    bool layoutValid = false;
    float layoutWrap = -1.0f;
    std::vector<RtLine> lines;
};

static RtEntry& acquireEntry(const std::string& markup, float scale, const glm::vec4& color, bool ignoreColor) {
    static std::unordered_map<std::string, RtEntry> cache;
    constexpr size_t kMaxEntries = 256;

    auto it = cache.find(markup);
    if (it != cache.end()) {
        RtEntry& e = it->second;
        if (e.scale == scale && (ignoreColor || e.color == color)) return e;
        e.scale = scale;
        e.color = color;
        e.glyphs = parseMarkup(markup, scale, color);
        e.layoutValid = false;
        return e;
    }

    if (cache.size() >= kMaxEntries) cache.clear();
    RtEntry& e = cache[markup];
    e.scale = scale;
    e.color = color;
    e.glyphs = parseMarkup(markup, scale, color);
    e.layoutValid = false;
    return e;
}

static const std::vector<RtLine>& layoutFor(RtEntry& e, float wrapWidth) {
    const float w = wrapWidth > 0.0f ? wrapWidth : -1.0f;
    if (!e.layoutValid || e.layoutWrap != w) {
        computeLayout(e.glyphs, w, e.lines);
        e.layoutWrap = w;
        e.layoutValid = true;
    }
    return e.lines;
}

int RichText::countPrintableChars(const std::string& markup) {
    // Same tag rule as parseMarkup ("[...]" with a closing bracket is a tag),
    // but counts without building any glyph objects.
    int count = 0;
    const std::string_view mv(markup);
    for (size_t i = 0; i < mv.size(); ++i) {
        if (mv[i] == '[') {
            size_t close = mv.find(']', i);
            if (close != std::string_view::npos) {
                i = close;
                continue;
            }
        }
        ++count;
    }
    return count;
}

void RichText::drawMarkup(Batch2D& batch, const std::string& markup, float startX, float startY, const RichTextOptions& opts) {
    RtEntry& entry = acquireEntry(markup, opts.scale, opts.defaultColor, false);
    if (entry.glyphs.empty()) return;

    const std::vector<RtLine>& lines = layoutFor(entry, opts.wrapWidth);

    // Default 8x8 font metrics
    constexpr float baseCharW = 8.0f;
    constexpr float baseCharH = 8.0f;

    float cursorY = startY;
    int renderedCount = 0;

    for (const RtLine& line : lines) {
        float cursorX = startX;
        if (opts.align == 1 && opts.wrapWidth > 0.0f) { // Center
            cursorX += (opts.wrapWidth - line.width) * 0.5f;
        } else if (opts.align == 2 && opts.wrapWidth > 0.0f) { // Right
            cursorX += (opts.wrapWidth - line.width);
        }

        const float maxHeight = std::max(baseCharH * opts.scale, baseCharH * line.maxScale);

        for (uint32_t k = line.begin; k < line.end; ++k) {
            const GlyphState& g = entry.glyphs[k];

            if (opts.visibleChars >= 0 && renderedCount >= opts.visibleChars) {
                return; // Typewriter cutoff
            }
            renderedCount++;

            const float charW = baseCharW * g.scale;

            if (g.c != ' ') {
                float drawX = cursorX;
                float drawY = cursorY;

                // Wave effect
                if (g.wave) {
                    float offset = std::sin(opts.time * g.waveFreq + g.charIndex * 0.45f) * g.waveAmp;
                    drawY += offset;
                }

                // Shake effect
                if (g.shake) {
                    float rx = std::sin(opts.time * 65.0f + g.charIndex * 19.0f) * g.shakeInt;
                    float ry = std::cos(opts.time * 55.0f + g.charIndex * 23.0f) * g.shakeInt;
                    drawX += rx;
                    drawY += ry;
                }

                // Color calculation
                glm::vec4 finalColor = g.color;
                if (g.rainbow) {
                    float hue = opts.time * g.rainbowSpeed + g.charIndex * 0.08f;
                    finalColor = hsvToRgb(hue, 0.85f, 1.0f, g.color.a);
                }

                // One-character views: no per-glyph std::string construction.
                const std::string_view ch(&g.c, 1);

                // Bold drop shadow accent
                if (g.bold) {
                    glm::vec4 shadowCol = glm::vec4(0.0f, 0.0f, 0.0f, finalColor.a * 0.7f);
                    batch.draw_text(ch, drawX + 1.0f, drawY + 1.0f, g.scale, shadowCol);
                }

                batch.draw_text(ch, drawX, drawY, g.scale, finalColor);
            }

            cursorX += charW;
        }

        cursorY += maxHeight + (2.0f * opts.scale); // Line gap
    }
}

glm::vec2 RichText::measureMarkup(Batch2D& batch, const std::string& markup, float scale, float wrapWidth) {
    (void)batch;

    // Colours don't affect layout, so reuse whichever cached parse matches this scale.
    RtEntry& entry = acquireEntry(markup, scale, glm::vec4(1.0f), true);
    if (entry.glyphs.empty()) return glm::vec2(0.0f);

    // Uses the exact layout drawMarkup uses (previously measure ignored word-wrap
    // and scaled glyphs, so it disagreed with what was actually drawn).
    const std::vector<RtLine>& lines = layoutFor(entry, wrapWidth);

    constexpr float baseCharH = 8.0f;
    float maxLineWidth = 0.0f;
    float totalH = 0.0f;
    for (const RtLine& line : lines) {
        maxLineWidth = std::max(maxLineWidth, line.width);
        totalH += std::max(baseCharH * scale, baseCharH * line.maxScale) + 2.0f * scale;
    }
    return glm::vec2(maxLineWidth, totalH);
}

} // namespace crayon
