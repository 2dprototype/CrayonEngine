#include "rich_text.hpp"
#include "font.hpp"
#include <cmath>
#include <sstream>
#include <algorithm>
#include <stack>

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

static std::vector<GlyphState> parseMarkup(const std::string& markup, const RichTextOptions& opts) {
    std::vector<GlyphState> glyphs;
    glyphs.reserve(markup.size());

    std::stack<glm::vec4> colorStack;
    colorStack.push(opts.defaultColor);

    std::stack<std::pair<float, float>> waveStack; // amp, freq
    std::stack<float> shakeStack;                 // intensity
    std::stack<float> rainbowStack;               // speed
    std::stack<float> scaleStack;
    scaleStack.push(opts.scale);
    std::stack<bool> boldStack;

    int printIdx = 0;

    for (size_t i = 0; i < markup.size(); ++i) {
        if (markup[i] == '[') {
            size_t close = markup.find(']', i);
            if (close != std::string::npos) {
                std::string tag = markup.substr(i + 1, close - i - 1);
                i = close;

                // Closing tags
                if (!tag.empty() && tag[0] == '/') {
                    std::string endTag = tag.substr(1);
                    if (endTag == "color" && colorStack.size() > 1) colorStack.pop();
                    else if (endTag == "wave" && !waveStack.empty()) waveStack.pop();
                    else if (endTag == "shake" && !shakeStack.empty()) shakeStack.pop();
                    else if (endTag == "rainbow" && !rainbowStack.empty()) rainbowStack.pop();
                    else if (endTag == "scale" && scaleStack.size() > 1) scaleStack.pop();
                    else if (endTag == "b" && !boldStack.empty()) boldStack.pop();
                    continue;
                }

                // Opening tags
                if (tag.rfind("color=", 0) == 0) {
                    std::string val = tag.substr(6);
                    colorStack.push(parseColorString(val, colorStack.top()));
                } else if (tag == "wave") {
                    waveStack.push({3.0f, 4.0f});
                } else if (tag.rfind("wave", 0) == 0) {
                    float amp = 3.0f, freq = 4.0f;
                    size_t aPos = tag.find("amp=");
                    if (aPos != std::string::npos) std::sscanf(tag.c_str() + aPos + 4, "%f", &amp);
                    size_t fPos = tag.find("freq=");
                    if (fPos != std::string::npos) std::sscanf(tag.c_str() + fPos + 5, "%f", &freq);
                    waveStack.push({amp, freq});
                } else if (tag == "shake") {
                    shakeStack.push(1.5f);
                } else if (tag.rfind("shake", 0) == 0) {
                    float intens = 1.5f;
                    size_t sPos = tag.find("int=");
                    if (sPos != std::string::npos) std::sscanf(tag.c_str() + sPos + 4, "%f", &intens);
                    shakeStack.push(intens);
                } else if (tag == "rainbow") {
                    rainbowStack.push(1.5f);
                } else if (tag.rfind("rainbow", 0) == 0) {
                    float spd = 1.5f;
                    size_t spPos = tag.find("speed=");
                    if (spPos != std::string::npos) std::sscanf(tag.c_str() + spPos + 6, "%f", &spd);
                    rainbowStack.push(spd);
                } else if (tag.rfind("scale=", 0) == 0) {
                    float sc = opts.scale;
                    std::sscanf(tag.c_str() + 6, "%f", &sc);
                    scaleStack.push(sc);
                } else if (tag == "b") {
                    boldStack.push(true);
                }
                continue;
            }
        }

        // Printable char or newline
        GlyphState g;
        g.c = markup[i];
        g.color = colorStack.top();
        if (!waveStack.empty()) {
            g.wave = true;
            g.waveAmp = waveStack.top().first;
            g.waveFreq = waveStack.top().second;
        }
        if (!shakeStack.empty()) {
            g.shake = true;
            g.shakeInt = shakeStack.top();
        }
        if (!rainbowStack.empty()) {
            g.rainbow = true;
            g.rainbowSpeed = rainbowStack.top();
        }
        g.scale = scaleStack.top();
        g.bold = !boldStack.empty();
        g.charIndex = printIdx++;
        glyphs.push_back(g);
    }

    return glyphs;
}

int RichText::countPrintableChars(const std::string& markup) {
    RichTextOptions dummy;
    auto glyphs = parseMarkup(markup, dummy);
    return static_cast<int>(glyphs.size());
}

void RichText::drawMarkup(Batch2D& batch, const std::string& markup, float startX, float startY, const RichTextOptions& opts) {
    auto glyphs = parseMarkup(markup, opts);
    if (glyphs.empty()) return;

    GLuint fontId = batch.get_white_texture_id(); // Fallback if no font texture
    // Default 8x8 font metrics
    float baseCharW = 8.0f;
    float baseCharH = 8.0f;

    // Split into lines respecting wrapWidth
    struct LineInfo {
        std::vector<GlyphState> lineGlyphs;
        float width = 0.0f;
    };

    std::vector<LineInfo> lines;
    lines.emplace_back();

    float currentLineWidth = 0.0f;
    size_t lastSpaceIdx = 0;
    float widthAtLastSpace = 0.0f;

    for (size_t i = 0; i < glyphs.size(); ++i) {
        const auto& g = glyphs[i];
        if (g.c == '\n') {
            lines.back().width = currentLineWidth;
            lines.emplace_back();
            currentLineWidth = 0.0f;
            lastSpaceIdx = 0;
            widthAtLastSpace = 0.0f;
            continue;
        }

        float charW = baseCharW * g.scale;

        if (opts.wrapWidth > 0.0f && (currentLineWidth + charW > opts.wrapWidth) && !lines.back().lineGlyphs.empty()) {
            // Word wrap if space occurred
            if (lastSpaceIdx > 0 && lastSpaceIdx < lines.back().lineGlyphs.size()) {
                std::vector<GlyphState> carried(lines.back().lineGlyphs.begin() + lastSpaceIdx + 1, lines.back().lineGlyphs.end());
                lines.back().lineGlyphs.resize(lastSpaceIdx);
                lines.back().width = widthAtLastSpace;

                lines.emplace_back();
                lines.back().lineGlyphs = carried;
                currentLineWidth = 0.0f;
                for (const auto& cg : carried) currentLineWidth += baseCharW * cg.scale;
                lastSpaceIdx = 0;
            } else {
                lines.back().width = currentLineWidth;
                lines.emplace_back();
                currentLineWidth = 0.0f;
            }
        }

        if (g.c == ' ') {
            lastSpaceIdx = lines.back().lineGlyphs.size();
            widthAtLastSpace = currentLineWidth;
        }

        lines.back().lineGlyphs.push_back(g);
        currentLineWidth += charW;
    }
    lines.back().width = currentLineWidth;

    // Render lines with alignment and animations
    float cursorY = startY;
    int renderedCount = 0;

    for (const auto& line : lines) {
        float cursorX = startX;
        if (opts.align == 1 && opts.wrapWidth > 0.0f) { // Center
            cursorX += (opts.wrapWidth - line.width) * 0.5f;
        } else if (opts.align == 2 && opts.wrapWidth > 0.0f) { // Right
            cursorX += (opts.wrapWidth - line.width);
        }

        float maxHeight = baseCharH * opts.scale;

        for (const auto& g : line.lineGlyphs) {
            if (opts.visibleChars >= 0 && renderedCount >= opts.visibleChars) {
                return; // Typewriter cutoff
            }
            renderedCount++;

            float charW = baseCharW * g.scale;
            float charH = baseCharH * g.scale;
            maxHeight = std::max(maxHeight, charH);

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

                // Bold drop shadow accent
                if (g.bold) {
                    glm::vec4 shadowCol = glm::vec4(0.0f, 0.0f, 0.0f, finalColor.a * 0.7f);
                    std::string s(1, g.c);
                    batch.draw_text(s, drawX + 1.0f, drawY + 1.0f, g.scale, shadowCol);
                }

                std::string s(1, g.c);
                batch.draw_text(s, drawX, drawY, g.scale, finalColor);
            }

            cursorX += charW;
        }

        cursorY += maxHeight + (2.0f * opts.scale); // Line gap
    }
}

glm::vec2 RichText::measureMarkup(Batch2D& batch, const std::string& markup, float scale, float wrapWidth) {
    (void)batch;
    RichTextOptions opts;
    opts.scale = scale;
    opts.wrapWidth = wrapWidth;

    auto glyphs = parseMarkup(markup, opts);
    if (glyphs.empty()) return glm::vec2(0.0f);

    float baseCharW = 8.0f;
    float baseCharH = 8.0f;

    float maxLineWidth = 0.0f;
    float currentLineWidth = 0.0f;
    int lineCount = 1;

    for (const auto& g : glyphs) {
        if (g.c == '\n') {
            maxLineWidth = std::max(maxLineWidth, currentLineWidth);
            currentLineWidth = 0.0f;
            lineCount++;
            continue;
        }

        float charW = baseCharW * g.scale;
        if (wrapWidth > 0.0f && currentLineWidth + charW > wrapWidth) {
            maxLineWidth = std::max(maxLineWidth, currentLineWidth);
            currentLineWidth = 0.0f;
            lineCount++;
        }
        currentLineWidth += charW;
    }
    maxLineWidth = std::max(maxLineWidth, currentLineWidth);

    float totalH = lineCount * (baseCharH * scale + 2.0f * scale);
    return glm::vec2(maxLineWidth, totalH);
}

} // namespace crayon
