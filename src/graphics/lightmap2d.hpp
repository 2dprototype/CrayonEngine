#pragma once
// 2D lighting: accumulates point/spot lights into an RGBA light map (ImageData). The map is then
// drawn over the scene with the multiply blend mode (see crayon.lights2d in the Lua prelude), so it
// works identically on the GPU and CPU backends.
#include "image_data.hpp"
#include <vector>

namespace crayon {

struct Light2D {
    float x = 0, y = 0;              // world position
    float radius = 100.0f;           // world units
    float r = 1, g = 1, b = 1;       // colour
    float intensity = 1.0f;
    float falloff = 2.0f;            // attenuation exponent: 1 = linear, 2 = smooth, higher = tighter core
    bool spot = false;
    float direction = 0.0f;          // radians, spot only (0 = +x, y down)
    float cone = 0.6f;               // half-angle in radians, spot only
    float softness = 0.25f;          // 0..1 fraction of the cone used for the soft edge
};

struct LightMapView {
    float offset_x = 0, offset_y = 0;   // world coordinate at light-map pixel (0,0)
    float scale_x = 1, scale_y = 1;     // light-map pixels per world unit
};

// Fills `out` with ambient + the sum of all lights (clamped to 1).
void render_lightmap(ImageData& out, float ambient_r, float ambient_g, float ambient_b,
                     const std::vector<Light2D>& lights, const LightMapView& view);

} // namespace crayon
