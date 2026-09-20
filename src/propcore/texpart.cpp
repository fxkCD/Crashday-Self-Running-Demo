#include "particle.hpp"
#include "render.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace flydemo {
namespace {

constexpr float kSpeedRandomScale = 0.009999999776482582f;

float Float32(long double value) {
    return static_cast<float>(value);
}

CD3DVECTOR NormalizeUnchecked(CD3DVECTOR value) {

    const long double x = value.x;
    const long double y = value.y;
    const long double z = value.z;
    const long double inv = 1.0L / std::sqrt(x * x + y * y + z * z);
    value.x = Float32(x * inv);
    value.y = Float32(y * inv);
    value.z = Float32(z * inv);
    return value;
}

int TruncateTowardZero(float value) {

    return static_cast<int>(std::trunc(static_cast<double>(value)));
}

int TruncateTowardZero(long double value) {
    return static_cast<int>(std::trunc(value));
}

std::uint32_t InterpolateRgb(std::uint32_t startColor,
                             std::uint32_t endColor,
                             float ratio) {
    if (startColor == endColor)
        return startColor;

    const long double startWeight = ratio;
    const long double endWeight = 1.0L - static_cast<long double>(ratio);
    const auto channel = [&](unsigned shift) {
        const std::uint32_t start = (startColor >> shift) & 0xFFu;
        const std::uint32_t end = (endColor >> shift) & 0xFFu;
        return TruncateTowardZero(
            static_cast<long double>(end) * endWeight +
            static_cast<long double>(start) * startWeight);
    };

    const std::uint32_t blue = static_cast<std::uint32_t>(channel(0));
    const std::uint32_t green = static_cast<std::uint32_t>(channel(8));
    const std::uint32_t red = static_cast<std::uint32_t>(channel(16));

    return blue + (green << 8u) + (red << 16u);
}

}

CD3DTEXPARTICLEOBJECT::CD3DTEXPARTICLEOBJECT(const ParticleConfig& config) {
    ConfigureUnchecked(config);
}

void CD3DTEXPARTICLEOBJECT::ConfigureUnchecked(const ParticleConfig& config) {
    config_ = config;
    config_.direction = NormalizeUnchecked(config_.direction);
    const std::size_t count = config_.maxCount > 0
        ? static_cast<std::size_t>(config_.maxCount) : 0u;
    records_.assign(count, CD3DTEXPARTICLE{});
    activeCount_ = 0;
    dirty_ = false;
    hidden_ = false;
    boundsMin_ = emitterPosition_;
    boundsMax_ = emitterPosition_;
    boundsRadius_ = 0.0f;
    worldSector_ = 0;
    respawnAccumulator_ = 0.0f;
    forceInitialRespawn_ = config_.respawn && config_.respawnRate != 0;
}

bool CD3DTEXPARTICLEOBJECT::Initialize(const ParticleConfig& config,
                                               TexPartIO& backend,
                                               std::string* error) {
    if (!ValidateParticleConfig(config, error))
        return false;
    ConfigureUnchecked(config);
    if (!backend.EnsureTextureLoaded(config_.textureName)) {
        if (error)
            *error = "texture load failed";
        return false;
    }

    for (std::int16_t i = 0; i < config_.startCount; ++i)
        Spawn(backend);
    if (error)
        error->clear();
    return true;
}

bool CD3DTEXPARTICLEOBJECT::SetRespawnRaw(std::uint8_t respawn,
                                                  std::string* error) {

    if (respawn > 1u) {
        if (error)
            *error = "particle respawn must be 0 or 1";
        return false;
    }
    const bool enabled = respawn != 0u;
    if (enabled != config_.respawn) {
        respawnAccumulator_ = 0.0f;
        forceInitialRespawn_ = enabled && config_.respawnRate != 0;
    }
    config_.respawn = enabled;
    if (error)
        error->clear();
    return true;
}

void CD3DTEXPARTICLEOBJECT::SetDirection(const CD3DVECTOR& value) {

    config_.direction = NormalizeUnchecked(value);
}

std::int32_t CD3DTEXPARTICLEOBJECT::Spawn(TexPartIO& backend) {

    if (activeCount_ >= config_.maxCount)
        return -1;

    std::size_t index = 0;
    for (; index < records_.size(); ++index) {
        if (records_[index].used == 0)
            break;
    }
    if (index == records_.size()) {

        return -1;
    }

    ++activeCount_;

    const long double keep = 1.0L - static_cast<long double>(config_.variance);
    const auto component = [&](float base) {
        const int random = backend.RandomInt() % 100 - 50;
        return Float32(
            static_cast<long double>(random) * config_.variance +
            static_cast<long double>(base) * keep);
    };
    CD3DVECTOR direction{
        component(config_.direction.x),
        component(config_.direction.y),
        component(config_.direction.z),
    };
    direction = NormalizeUnchecked(direction);

    const float speedRange = Float32(
        static_cast<long double>(config_.maxSpeed) - config_.minSpeed);
    const int speedRandom = backend.RandomInt() % 100;
    const float speed = Float32(
        static_cast<long double>(speedRandom) * speedRange * kSpeedRandomScale +
        config_.minSpeed);

    if (!config_.attachmentObject.empty()) {
        CD3DVECTOR attached{};
        if (backend.ResolveObjectPosition(config_.attachmentObject, attached))
            emitterPosition_ = attached;
    }

    CD3DTEXPARTICLE& particle = records_[index];
    particle.used = 1;
    particle.remainingLife = config_.lifetime;
    particle.velocity.x = Float32(
        static_cast<long double>(direction.x) * speed);
    particle.velocity.y = Float32(
        static_cast<long double>(direction.y) * speed);
    particle.velocity.z = Float32(
        static_cast<long double>(direction.z) * speed);
    particle.position = emitterPosition_;
    particle.size = config_.startSize;
    particle.textureFrame = 1;
    particle.color = config_.startColor;
    return static_cast<std::int32_t>(index);
}

void CD3DTEXPARTICLEOBJECT::Update(float dt,
                                          TexPartIO& backend) {

    const std::uint8_t frameCount = backend.TextureFrameCount(config_.textureName);

    for (CD3DTEXPARTICLE& particle : records_) {
        if (particle.used == 0)
            continue;

        particle.remainingLife = Float32(
            static_cast<long double>(particle.remainingLife) - dt);

        if (particle.remainingLife <= 0.0f) {
            particle.used = 0;
            --activeCount_;
            continue;
        }

        particle.velocity.x = Float32(
            static_cast<long double>(config_.acceleration.x) * dt + particle.velocity.x);
        particle.velocity.y = Float32(
            static_cast<long double>(config_.acceleration.y) * dt + particle.velocity.y);
        particle.velocity.z = Float32(
            static_cast<long double>(config_.acceleration.z) * dt + particle.velocity.z);

        particle.position.x = Float32(
            static_cast<long double>(particle.velocity.x) * dt + particle.position.x);
        particle.position.y = Float32(
            static_cast<long double>(particle.velocity.y) * dt + particle.position.y);
        particle.position.z = Float32(
            static_cast<long double>(particle.velocity.z) * dt + particle.position.z);

        const float ratio = Float32(
            static_cast<long double>(particle.remainingLife) / config_.lifetime);
        const float oneMinus = Float32(1.0L - static_cast<long double>(ratio));
        particle.size = Float32(
            static_cast<long double>(config_.endSize) * oneMinus +
            static_cast<long double>(config_.startSize) * ratio);

        if (frameCount <= 1u) {
            particle.textureFrame = 1;
        } else {
            const int frame0 = TruncateTowardZero(
                static_cast<long double>(frameCount) * oneMinus);

            const std::uint8_t low = static_cast<std::uint8_t>(frame0);
            particle.textureFrame = static_cast<std::uint8_t>(low + 1u);
            if (particle.textureFrame < 1u)
                particle.textureFrame = 1u;
            if (particle.textureFrame > frameCount)
                particle.textureFrame = frameCount;
        }

        particle.color = InterpolateRgb(
            config_.startColor, config_.endColor, ratio);
    }

    if (config_.respawn && config_.respawnRate != 0) {
        const long double quota =
            static_cast<long double>(respawnAccumulator_) +
            static_cast<long double>(config_.respawnRate) * dt;
        int spawnCount = TruncateTowardZero(quota);
        respawnAccumulator_ = Float32(quota - spawnCount);
        if (forceInitialRespawn_ && spawnCount == 0)
            spawnCount = 1;
        forceInitialRespawn_ = false;
        for (int i = 0; i < spawnCount; ++i)
            Spawn(backend);
    } else {
        respawnAccumulator_ = 0.0f;
        forceInitialRespawn_ = false;
    }

    dirty_ = true;
    if (activeCount_ == 0 && !config_.respawn) {
        hidden_ = true;
        return;
    }

    RebuildBounds();
    worldSector_ = backend.RegisterBounds(boundsMin_, boundsMax_);
    dirty_ = false;
}

void CD3DTEXPARTICLEOBJECT::ProjectActive(const RendererState& renderer) {
    TransformMatrix matrix{};
    const CD3DMATRIX& combined = renderer.GetFrameTransforms().combined;
    for (std::size_t i = 0; i < matrix.size(); ++i)
        matrix[i] = combined.m[i];

    for (CD3DTEXPARTICLE& particle : records_) {
        if (particle.used == 0)
            continue;
        const ProjectedPosition projected = ProjectPosition(particle.position, matrix);
        particle.projectedX = projected.x;
        particle.projectedY = projected.y;
        particle.projectedZ = projected.z;
        particle.rhw = projected.rhw;
        particle.projectedDiffuse = 0x00FFFFFFu;
        particle.projectedSpecular = 0xFF000000u;
    }
}

bool CD3DTEXPARTICLEOBJECT::Render(RendererState& renderer,
                                          TexPartIO& backend) {
    if (!renderer.SceneInProgress() || !renderer.HasGetFrameTransforms())
        return false;

    ProjectActive(renderer);

    const std::uint8_t textureIndex = backend.TextureIndex(config_.textureName);
    const std::uint8_t blendMode = static_cast<std::uint8_t>(config_.renderMode);
    for (const CD3DTEXPARTICLE& particle : records_) {
        if (particle.used == 0)
            continue;
        std::size_t queued = 0;
        if (!renderer.QueueParticleBillboard(
                particle.projectedX, particle.projectedY, particle.projectedZ,
                particle.rhw, particle.size, particle.color,
                textureIndex, particle.textureFrame, blendMode, &queued))
            return false;

        if (queued < 2u)
            return true;
    }
    return true;
}

void CD3DTEXPARTICLEOBJECT::RebuildBounds() {

    boundsMin_ = emitterPosition_;
    boundsMax_ = emitterPosition_;

    for (const CD3DTEXPARTICLE& particle : records_) {
        if (particle.used == 0)
            continue;

        if (!(particle.position.x >= boundsMin_.x)) boundsMin_.x = particle.position.x;
        if (!(particle.position.y >= boundsMin_.y)) boundsMin_.y = particle.position.y;
        if (!(particle.position.z >= boundsMin_.z)) boundsMin_.z = particle.position.z;
        if (particle.position.x > boundsMax_.x) boundsMax_.x = particle.position.x;
        if (particle.position.y > boundsMax_.y) boundsMax_.y = particle.position.y;
        if (particle.position.z > boundsMax_.z) boundsMax_.z = particle.position.z;
    }

    const long double dx = static_cast<long double>(boundsMax_.x) - boundsMin_.x;
    const long double dy = static_cast<long double>(boundsMax_.y) - boundsMin_.y;
    const long double dz = static_cast<long double>(boundsMax_.z) - boundsMin_.z;
    boundsRadius_ = Float32(std::sqrt((dx * dx + dy * dy + dz * dz) * 0.25L));
}

TextureParticleSlots::TextureParticleSlots(std::size_t maxCount) { Reset(maxCount); }

void TextureParticleSlots::Reset(std::size_t maxCount) {
    records_.assign(maxCount * RecordSize, 0);
    activeCount_ = 0;
}

std::int32_t TextureParticleSlots::Acquire() {
    if (activeCount_ >= MaxCount())
        return -1;
    for (std::size_t i = 0; i < MaxCount(); ++i) {
        auto& used = records_[i * RecordSize];
        if (used == 0) {
            used = 1;
            ++activeCount_;
            return static_cast<std::int32_t>(i);
        }
    }
    return -1;
}

bool TextureParticleSlots::Release(std::size_t index) {
    if (index >= MaxCount())
        return false;
    auto& used = records_[index * RecordSize];
    if (used == 0)
        return false;
    used = 0;
    if (activeCount_ != 0)
        --activeCount_;
    return true;
}

bool TextureParticleSlots::Used(std::size_t index) const {
    return index < MaxCount() && records_[index * RecordSize] != 0;
}

}
