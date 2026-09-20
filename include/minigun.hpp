#pragma once
#include "options.hpp"
#include "particle.hpp"
#include "types.hpp"
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace flydemo {

enum class MinigunSide : std::uint8_t { Left = 0, Right = 1 };

namespace minigun_off {
constexpr unsigned Side = 0x0FCC;
constexpr unsigned Ammo = 0x0FD0;
constexpr unsigned FireLatch = 0x0FD4;
constexpr unsigned SpinStep = 0x0FD8;
constexpr unsigned BarrelAngle = 0x0FDC;
constexpr unsigned ObjectSize = 0x0FE0;
}

void Minigun_Init(void* object, MinigunSide side, int ammo = 500);
bool Minigun_Fire(void* object);
int Minigun_GetAmmo(const void* object);
void Minigun_SetAmmo(void* object, int ammo);

void Minigun_Update(void* object, float dt);

std::uint32_t Minigun_MuzzleColor(std::int32_t randomValue);

struct MinigunLight {
    std::uint32_t light0 = 0;
    std::uint32_t light1 = 0;
    std::uint32_t light2 = 0;
};
MinigunLight Minigun_BuildLight(bool firing,
                                                     std::int32_t randomValue);

struct MinigunFlash {
    bool process = false;
    CD3DVECTOR localOffset{};
    std::uint32_t color = 0;
    bool createEmitter = false;
    bool lookupEmitter = false;
    bool setRespawn = false;
    bool respawn = false;
    bool updatePosition = false;
    bool updateDirection = false;
    bool updateColor = false;
};
MinigunFlash Minigun_BuildFlash(ParticleUse particles,
                                               bool firing,
                                               bool emitterAlreadyExists,
                                               std::int32_t randomOffset,
                                               std::int32_t randomColor);

struct MinigunBullet {
    bool process = false;
    CD3DVECTOR localPosition{};
    CD3DVECTOR localDirection{};
    CD3DVECTOR offsetVector{};
    bool createEmitter = false;
    bool lookupEmitter = false;
    bool setRespawn = false;
    bool respawn = false;
    bool updatePosition = false;
    bool updateDirection = false;
};
MinigunBullet Minigun_BuildBulletPlan(ParticleUse particles,
                                           MinigunSide side,
                                           bool firing,
                                           bool emitterAlreadyExists);

ParticleConfig Minigun_ShotConfig(const CD3DVECTOR& worldDirection,
                                                    std::uint32_t color);
ParticleConfig Minigun_BulletConfig(const CD3DVECTOR& worldDirection,
                                                  const CD3DVECTOR& acceleration,
                                                  std::uint32_t ambientColor);

class MinigunFX {
public:
    virtual ~MinigunFX() = default;
    virtual std::string_view ObjectName(void* minigun) const = 0;
    virtual bool DynamicNameExists(std::string_view fullName) const = 0;
    virtual void* FindDynamic(std::string_view fullName) = 0;
    virtual void* CreateShotfireEmitter(std::size_t nativeBytes,
                                         std::string_view fullName,
                                         const CD3DVECTOR& worldPosition,
                                         const CD3DVECTOR& worldDirection,
                                         std::uint32_t color) = 0;
    virtual void* CreateBulletEmitter(std::size_t nativeBytes,
                                       std::string_view fullName,
                                       const CD3DVECTOR& worldPosition,
                                       const CD3DVECTOR& worldDirection,
                                       const CD3DVECTOR& offsetVector) = 0;
    virtual void SetEmitterRespawn(void* emitter, bool enabled) = 0;
    virtual void SetEmitterPosition(void* emitter, const CD3DVECTOR& position) = 0;
    virtual void SetEmitterDirection(void* emitter, const CD3DVECTOR& direction) = 0;
    virtual void SetEmitterColor(void* emitter, std::uint32_t color) = 0;
};

struct MinigunEffects {
    std::uint8_t constructorAttempts = 0;
    bool shotfireProcessed = false;
    bool bulletsProcessed = false;
};
MinigunEffects Minigun_ApplyParticles(void* object,
                                                     ParticleUse particles,
                                                     bool firing,
                                                     std::int32_t randomOffset,
                                                     std::int32_t randomColor,
                                                     MinigunFX& backend);

}
