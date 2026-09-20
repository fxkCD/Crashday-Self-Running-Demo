#pragma once

#include "options.hpp"
#include "types.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace flydemo {

enum class ExplosionType : std::uint8_t { Small = 0, Normal = 1, Big = 2 };

struct ExplosionState {
    CD3DVECTOR position{};
    ExplosionType type = ExplosionType::Small;
    float strength = 0.0f;
    float radius = 0.0f;
};

struct ExplosionEmitterSpec {
    std::string texture;
    std::uint16_t count = 0;
    std::uint32_t packedColor = 0;
    float sizeA = 0.0f;
    float sizeB = 0.0f;
    float paramA = 0.0f;
    float paramB = 0.0f;
    float paramC = 0.0f;
};

class ExplosionFX {
public:
    virtual ~ExplosionFX() = default;

    virtual int NextRandom() = 0;

    virtual bool SpawnEmitter(std::string_view objectName,
                              const CD3DVECTOR& position,
                              const ExplosionEmitterSpec& spec) = 0;
};

class ExplosionDynamicBody {
public:
    virtual ~ExplosionDynamicBody() = default;
    virtual bool IsDynamicObjectClass() const = 0;
    virtual const CD3DVECTOR& BoundsCenter() const = 0;
    virtual const std::array<CD3DVECTOR, 8>& BoundsPoints() const = 0;
    virtual void ApplyImpulseAtPoint(const CD3DVECTOR& point, const CD3DVECTOR& impulse) = 0;
};

class ExplosionWorld {
public:
    virtual ~ExplosionWorld() = default;
    virtual ExplosionDynamicBody* DynamicAt(std::size_t slot) = 0;
};

struct EXPLOSION {
    std::string name;
    ExplosionState state{};

    bool invalidType = false;

    bool pendingWorldDelete = false;
};

struct ExplosionUpdateResult {
    std::size_t emitterAttempts = 0;
    std::size_t emitterSpawns = 0;
    std::size_t impulsesApplied = 0;
};

inline constexpr std::size_t ExplosionDynamicSlots = 0x1FF;
inline constexpr std::size_t ExplosionPointCount = 8;

bool ValidateExplosionType(ExplosionType type);

EXPLOSION ConstructExplosionObject(std::string name,
                                                const CD3DVECTOR& position,
                                                ExplosionType type,
                                                float strength,
                                                float radius);

std::string BuildExplosionName(std::string_view baseName,
                                     int random1,
                                     int random2);

std::vector<ExplosionEmitterSpec> BuildExplosionFx(
    ExplosionType type, ParticleUse particles);

std::size_t SpawnExplosionVisuals(const EXPLOSION& object,
                                  ParticleUse particles,
                                  ExplosionFX& backend,
                                  std::size_t* successfulSpawns = nullptr);

CD3DVECTOR ComputeExplosionImpulse(const CD3DVECTOR& center, const CD3DVECTOR& target,
                             float strength, float radius);

std::size_t ApplyExplosionForce(const ExplosionState& state,
                                       ExplosionWorld& world);

ExplosionUpdateResult UpdateExplosionObject(EXPLOSION& object,
                                            ParticleUse particles,
                                            ExplosionFX& visual,
                                            ExplosionWorld& world);

inline void RenderExplosionObject(const EXPLOSION&) noexcept {}

}
