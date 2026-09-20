#include "lighting.hpp"
#include <algorithm>
#include <cmath>

namespace flydemo {
namespace {
bool Fail(std::string* e, const char* msg) { if (e) *e = msg; return false; }
float Dot(const CD3DVECTOR& a, const CD3DVECTOR& b) { return a.x*b.x + a.y*b.y + a.z*b.z; }
std::uint8_t SatAdd(std::uint8_t base, float amount) {

    const long add = static_cast<long>(std::trunc(static_cast<double>(amount)));
    const long out = static_cast<long>(base) + add;
    return static_cast<std::uint8_t>(std::clamp(out, 0L, 255L));
}
RGBColor Unpack(std::uint32_t c) {
    return {static_cast<std::uint8_t>(c), static_cast<std::uint8_t>(c >> 8),
            static_cast<std::uint8_t>(c >> 16)};
}
}

bool LightingState::SetMode(LightingMode mode, std::string* error) {
    const auto v = static_cast<std::uint8_t>(mode);
    if (v > 1) return Fail(error, "mode must be LIGHTING_FULL or LIGHTING_VERTEXONLY");
    mode_ = mode;
    if (error) error->clear();
    return true;
}

RGBColor LightingState::ApplyDirectional(const CD3DVECTOR& normal, const CD3DVECTOR& direction,
                                          std::uint32_t packedColor) const {
    const float f = Dot(normal, direction);
    if (!(f >= 0.0f)) return {};
    const RGBColor c = Unpack(packedColor);
    return {SatAdd(0, c.c0*f), SatAdd(0, c.c1*f), SatAdd(0, c.c2*f)};
}

RGBColor LightingState::ApplyLights(const LightingVertex& vertex,
                                     const std::vector<CD3DLIGHT>& lights,
                                     RGBColor out) const {
    for (const auto& light : lights) {
        if (!light.lightUp || light.range < 0.0f) continue;
        CD3DVECTOR delta{vertex.position.x-light.position.x,
                   vertex.position.y-light.position.y,
                   vertex.position.z-light.position.z};
        const float distance = std::sqrt(Dot(delta, delta));
        if (!(distance <= light.range)) continue;
        const float normalDot = Dot(vertex.normal, delta);
        float factor;
        if (distance == 0.0f) {

            factor = normalDot > 0.0f ? 1.0f : 0.0f;
        } else if (light.range == 0.0f) {
            factor = 0.0f;
        } else {
            factor = ((1.0f - distance/light.range) / distance) * normalDot;
        }
        if (!(factor >= 0.0f)) continue;
        if (factor > 1.0f) factor = 1.0f;
        const RGBColor c = Unpack(light.packedColor);
        out.c0 = SatAdd(out.c0, c.c0 * factor);
        out.c1 = SatAdd(out.c1, c.c1 * factor);
        out.c2 = SatAdd(out.c2, c.c2 * factor);
    }
    return out;
}

}
