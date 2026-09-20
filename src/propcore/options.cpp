#include "options.hpp"
#include "render.hpp"
#include <cmath>
#include <cstring>

namespace flydemo {
namespace {
bool Fail(std::string* e, const char* msg) { if (e) *e = msg; return false; }
bool SetBool(bool& dst, int value, std::string* error) {
    if (value != 0 && value != 1) return Fail(error, "state must be 0 or 1");
    dst = value != 0;
    if (error) error->clear();
    return true;
}
}

void RenderOptionsState::ResetDefaults() {
    textureQuality = TextureQuality::High;
    flag50 = false;
    particles = ParticleUse::Always;
    flag4D = true;
    flag4E = true;
    flag4F = true;
    range = 400.0f;
    flag51 = true;
    flag52 = true;
}

bool RenderOptionsState::SetTextureQuality(TextureQuality value, std::string* error) {
    const auto v = static_cast<std::uint8_t>(value);
    if (v > 2) return Fail(error, "texqual must be HIGH, MEDIUM or LOW");
    textureQuality = value;
    if (error) error->clear();
    return true;
}

bool RenderOptionsState::SetParticleUse(ParticleUse value, std::string* error) {
    const auto v = static_cast<std::uint8_t>(value);
    if (v > 2) return Fail(error, "use must be PARTICLES_ALWAYS, PARTIAL or NEVER");
    particles = value;
    if (error) error->clear();
    return true;
}

bool RenderOptionsState::SetRange(float value, std::string* error) {

    if (value <= 0.0f)
        return Fail(error, "range must be > 0");
    range = value;
    if (error) error->clear();
    return true;
}

bool RenderOptionsState::SetFlag4D(int v, std::string* e) { return SetBool(flag4D, v, e); }
bool RenderOptionsState::SetFlag4E(int v, std::string* e) { return SetBool(flag4E, v, e); }
bool RenderOptionsState::SetFlag4F(int v, std::string* e) { return SetBool(flag4F, v, e); }
bool RenderOptionsState::SetFlag50(int v, std::string* e) { return SetBool(flag50, v, e); }
bool RenderOptionsState::SetFlag51(int v, std::string* e) { return SetBool(flag51, v, e); }
bool RenderOptionsState::SetFlag52(int v, std::string* e) { return SetBool(flag52, v, e); }

bool RenderOptionsState::Activate(RenderDevice& backend,
                                  bool deviceAvailable,
                                  bool sceneInProgress,
                                  std::uint32_t fogColor) const {

    if (!deviceAvailable || sceneInProgress)
        return false;

    backend.SetRenderState(0x07u, 1u);
    backend.SetRenderState(0x17u, 4u);

    backend.SetTextureStageState(0u, 0x11u, 2u);
    backend.SetTextureStageState(0u, 0x10u, 2u);
    backend.SetTextureStageState(0u, 0x12u, 3u);
    backend.SetTextureStageState(0u, 0x01u, 4u);
    backend.SetTextureStageState(0u, 0x02u, 2u);
    backend.SetTextureStageState(0u, 0x03u, 0u);
    backend.SetTextureStageState(0u, 0x04u, 2u);
    backend.SetTextureStageState(0u, 0x05u, 2u);
    backend.SetTextureStageState(0u, 0x0Cu, 1u);
    backend.SetTextureStageState(0u, 0x0Du, 1u);
    backend.SetTextureStageState(0u, 0x0Eu, 1u);

    backend.SetRenderState(0x80u, 0u);
    backend.SetRenderState(0x04u, 1u);
    backend.SetRenderState(0x1Bu, 0u);
    backend.SetRenderState(0x13u, 2u);
    backend.SetRenderState(0x14u, 1u);

    backend.SetRenderState(0x1Au, flag4D ? 1u : 0u);
    backend.SetRenderState(0x1Cu, flag4F ? 1u : 0u);

    backend.SetLightState(0x04u, 3u);
    std::uint32_t oneBits = 0;
    const float one = 1.0f;
    std::memcpy(&oneBits, &one, sizeof(oneBits));
    backend.SetLightState(0x05u, oneBits);
    std::uint32_t endBits = 0;
    const float end = 500.0f;
    std::memcpy(&endBits, &end, sizeof(endBits));
    backend.SetLightState(0x06u, endBits);
    backend.SetRenderState(0x22u, fogColor);
    return true;
}

}
