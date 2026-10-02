#pragma once

#include <string>
#include <vector>
#include <glm/glm.hpp>
#include "batch2d.hpp"

namespace crayon {

struct RichTextOptions {
    float scale = 1.0f;
    float wrapWidth = -1.0f;
    int align = 0; // 0 = Left, 1 = Center, 2 = Right
    float time = 0.0f;
    int visibleChars = -1; // -1 for all, >= 0 for typewriter effect
    glm::vec4 defaultColor{1.0f, 1.0f, 1.0f, 1.0f};
};

class RichText {
public:
    static void drawMarkup(Batch2D& batch, const std::string& markup, float x, float y, const RichTextOptions& opts);
    static glm::vec2 measureMarkup(Batch2D& batch, const std::string& markup, float scale = 1.0f, float wrapWidth = -1.0f);
    static int countPrintableChars(const std::string& markup);
};

} // namespace crayon
