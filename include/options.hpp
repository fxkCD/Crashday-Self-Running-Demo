#pragma once
#include <cstddef>
#include <cstdint>
#include <string>

namespace flydemo {

class RenderDevice;

enum class TextureQuality : std::uint8_t { High = 0, Medium = 1, Low = 2 };
enum class ParticleUse : std::uint8_t { Always = 0, Partial = 1, Never = 2 };

struct RenderOptionsState {
    TextureQuality textureQuality = TextureQuality::High;
    float range = 400.0f;
    ParticleUse particles = ParticleUse::Always;

    bool flag4D = true;
    bool flag4E = true;
    bool flag4F = true;
    bool flag50 = false;
    bool flag51 = true;
    bool flag52 = true;

    void ResetDefaults();
    bool SetTextureQuality(TextureQuality value, std::string* error = nullptr);
    bool SetParticleUse(ParticleUse value, std::string* error = nullptr);
    bool SetRange(float value, std::string* error = nullptr);
    bool SetFlag4D(int value, std::string* error = nullptr);
    bool SetFlag4E(int value, std::string* error = nullptr);
    bool SetFlag4F(int value, std::string* error = nullptr);
    bool SetFlag50(int value, std::string* error = nullptr);
    bool SetFlag51(int value, std::string* error = nullptr);
    bool SetFlag52(int value, std::string* error = nullptr);

    bool Activate(RenderDevice& backend,
                  bool deviceAvailable,
                  bool sceneInProgress,
                  std::uint32_t fogColor) const;
};

static_assert(sizeof(RenderOptionsState) == 0x10, "render option image must stay 16 bytes");
static_assert(offsetof(RenderOptionsState, textureQuality) == 0x00);
static_assert(offsetof(RenderOptionsState, range) == 0x04);
static_assert(offsetof(RenderOptionsState, particles) == 0x08);
static_assert(offsetof(RenderOptionsState, flag4D) == 0x09);
static_assert(offsetof(RenderOptionsState, flag4E) == 0x0A);
static_assert(offsetof(RenderOptionsState, flag4F) == 0x0B);
static_assert(offsetof(RenderOptionsState, flag50) == 0x0C);
static_assert(offsetof(RenderOptionsState, flag51) == 0x0D);
static_assert(offsetof(RenderOptionsState, flag52) == 0x0E);

}
