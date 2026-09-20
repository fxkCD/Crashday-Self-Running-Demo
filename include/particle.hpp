#pragma once

#include "types.hpp"
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace flydemo {

class RendererState;

enum class ParticleType : std::uint8_t { Line = 0, Texture = 1 };
enum class ParticleRenderMode : std::uint8_t { Solid = 0, Add = 1 };

struct ParticleConfig {
    ParticleType type = ParticleType::Texture;
    std::int16_t startCount = 0;
    std::int16_t maxCount = 0;
    bool respawn = false;
    std::int16_t respawnRate = 0;
    float lifetime = 1.0f;
    float minSpeed = 0.0f;
    float maxSpeed = 0.0f;
    CD3DVECTOR acceleration{0.0f, 0.0f, 0.0f};
    CD3DVECTOR direction{0.0f, 0.0f, 1.0f};
    float variance = 0.0f;
    std::string textureName;
    ParticleRenderMode renderMode = ParticleRenderMode::Solid;
    std::uint32_t startColor = 0x00FFFFFFu;
    std::string attachmentObject;

    float startSize = 1.0f;
    float endSize = 1.0f;
    std::uint32_t endColor = 0x00FFFFFFu;
};

bool ValidateParticleConfig(const ParticleConfig& config, std::string* error = nullptr);

struct CD3DPARTICLEOBJECT {
    ParticleConfig config{};
    std::int16_t activeCount = 0;
    bool dirty = false;
    bool hidden = false;
    CD3DVECTOR emitterPosition{0.0f, 0.0f, 0.0f};

    bool NeedsBaseUpdate();

    bool SetRespawnRaw(std::uint8_t respawn, std::string* error = nullptr);
    void SetEmitterPosition(const CD3DVECTOR& value);
    void SetAttachmentObject(std::string value);
    void SetDirection(const CD3DVECTOR& value);
    void SetStartColor(std::uint32_t color);
};

struct CD3DTEXPARTICLE {
    std::uint8_t used = 0;
    std::uint8_t reserved01To03[3]{};
    float remainingLife = 0.0f;
    CD3DVECTOR position{0.0f, 0.0f, 0.0f};
    std::uint8_t reserved14To27[0x14]{};
    float projectedX = 0.0f;
    float projectedY = 0.0f;
    float projectedZ = 0.0f;
    float rhw = 0.0f;
    std::uint32_t projectedDiffuse = 0;
    std::uint32_t projectedSpecular = 0;
    std::uint8_t reserved40To4F[0x10]{};
    CD3DVECTOR velocity{0.0f, 0.0f, 0.0f};
    float size = 0.0f;
    std::uint8_t textureFrame = 0;
    std::uint8_t reserved61To63[3]{};
    std::uint32_t color = 0;
};
static_assert(sizeof(CD3DTEXPARTICLE) == 0x68,
              "texture-particle record must match PE32 stride");

class TexPartIO {
public:
    virtual ~TexPartIO() = default;
    virtual int RandomInt() = 0;
    virtual bool EnsureTextureLoaded(std::string_view) { return true; }
    virtual std::uint8_t TextureFrameCount(std::string_view) const = 0;
    virtual std::uint8_t TextureIndex(std::string_view) const = 0;
    virtual bool ResolveObjectPosition(std::string_view, CD3DVECTOR&) const { return false; }

    virtual std::uint16_t RegisterBounds(const CD3DVECTOR&, const CD3DVECTOR&) { return 0; }
};

class CD3DTEXPARTICLEOBJECT {
public:
    CD3DTEXPARTICLEOBJECT() = default;
    explicit CD3DTEXPARTICLEOBJECT(const ParticleConfig& config);

    bool Initialize(const ParticleConfig& config,
                    TexPartIO& backend,
                    std::string* error = nullptr);

    std::int32_t Spawn(TexPartIO& backend);

    void Update(float dt, TexPartIO& backend);

    bool Render(RendererState& renderer,
                TexPartIO& backend);

    void RebuildBounds();

    const ParticleConfig& Config() const { return config_; }
    std::int16_t ActiveCount() const { return activeCount_; }
    bool Dirty() const { return dirty_; }
    bool Hidden() const { return hidden_; }
    const CD3DVECTOR& EmitterPosition() const { return emitterPosition_; }
    void SetEmitterPosition(const CD3DVECTOR& value) { emitterPosition_ = value; }

    bool SetRespawnRaw(std::uint8_t respawn, std::string* error = nullptr);
    void SetRespawn(bool enabled) { (void)SetRespawnRaw(enabled ? 1u : 0u); }
    void SetDirection(const CD3DVECTOR& value);
    void SetStartColor(std::uint32_t color) { config_.startColor = color; }
    const CD3DVECTOR& BoundsMin() const { return boundsMin_; }
    const CD3DVECTOR& BoundsMax() const { return boundsMax_; }
    float BoundsRadius() const { return boundsRadius_; }
    std::size_t MaxCount() const { return records_.size(); }
    const std::vector<CD3DTEXPARTICLE>& Records() const { return records_; }
    std::vector<CD3DTEXPARTICLE>& MutableRecordsForTest() { return records_; }

private:
    void ConfigureUnchecked(const ParticleConfig& config);
    void ProjectActive(const RendererState& renderer);

    ParticleConfig config_{};
    std::vector<CD3DTEXPARTICLE> records_{};
    std::int16_t activeCount_ = 0;
    bool dirty_ = false;
    bool hidden_ = false;
    CD3DVECTOR emitterPosition_{0.0f, 0.0f, 0.0f};
    CD3DVECTOR boundsMin_{0.0f, 0.0f, 0.0f};
    CD3DVECTOR boundsMax_{0.0f, 0.0f, 0.0f};
    float boundsRadius_ = 0.0f;
    std::uint16_t worldSector_ = 0;
    float respawnAccumulator_ = 0.0f;
    bool forceInitialRespawn_ = false;
};

class TextureParticleSlots {
public:
    static constexpr std::size_t RecordSize = 0x68;
    explicit TextureParticleSlots(std::size_t maxCount = 0);
    void Reset(std::size_t maxCount);
    std::int32_t Acquire();
    bool Release(std::size_t index);
    bool Used(std::size_t index) const;
    std::size_t ActiveCount() const { return activeCount_; }
    std::size_t MaxCount() const { return records_.size() / RecordSize; }
private:
    std::vector<std::uint8_t> records_;
    std::size_t activeCount_ = 0;
};

}
