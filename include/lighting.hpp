#pragma once
#include "types.hpp"
#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace flydemo {

struct RGBColor {
    std::uint8_t c0 = 0;
    std::uint8_t c1 = 0;
    std::uint8_t c2 = 0;
};

inline constexpr std::uint32_t PackRGBColor(std::uint8_t c0, std::uint8_t c1,
                                              std::uint8_t c2) {
    return std::uint32_t(c0) | (std::uint32_t(c1) << 8) | (std::uint32_t(c2) << 16);
}

struct CD3DLIGHT {
    CD3DVECTOR position{};
    float range = 10.0f;
    std::uint32_t packedColor = 0x00FFFFFFu;
    float cachedVectorLength = 0.0f;
    bool coronas = true;
    bool lensFlares = true;
    float scale = 1.0f;
    bool lightUp = true;
    std::array<std::uint8_t, 0x14> reserved24To37{};

    bool Configure(const CD3DVECTOR& p, float newRange, std::uint32_t color,
                   int useCoronas, int useLensFlares, int useLightUp,
                   std::string* error = nullptr);
    float RecomputeVectorLength();
};
static_assert(sizeof(CD3DLIGHT) == 0x38);

enum class LightingMode : std::uint8_t { Full = 0, VertexOnly = 1 };

struct LightingVertex {
    CD3DVECTOR position{};
    CD3DVECTOR normal{};
    RGBColor lighting{};
};

class LightingState {
public:
    bool SetMode(LightingMode mode, std::string* error = nullptr);
    LightingMode Mode() const { return mode_; }

    RGBColor ApplyLights(const LightingVertex& vertex,
                          const std::vector<CD3DLIGHT>& lights,
                          RGBColor initial = {}) const;

    RGBColor ApplyDirectional(const CD3DVECTOR& normal, const CD3DVECTOR& direction,
                               std::uint32_t packedColor) const;
private:
    LightingMode mode_ = LightingMode::Full;
};

}
