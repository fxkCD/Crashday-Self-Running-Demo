#pragma once
#include "options.hpp"
#include "dynamic_object.hpp"
#include "transform.hpp"
#include "particle.hpp"
#include "types.hpp"
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

namespace flydemo {

class WorldState;

struct MissileStepResult;

struct CD3DMISSILEDATA {
    std::uint32_t owner = 0;
    std::uint32_t trail = 0;
    float lifetime = 0.0f;
    float thrust = 0.0f;
    CD3DVECTOR direction{};
};
static_assert(sizeof(CD3DMISSILEDATA) == 0x1c);

class CD3DMISSILEOBJECT : public CD3DDYNAMICOBJECT {
    CD3DMISSILEDATA missile_{};
public:
    void Initialize(void* ownerCar,
                    const CD3DVECTOR& position,
                    const CD3DVECTOR& direction,
                    std::int32_t randomLifetime,
                    std::int32_t randomThrust,
                    std::int16_t mass,
                    const CD3DVECTOR& size,
                    const CD3DMATRIX& orientation,
                    const CD3DVECTOR& inheritedVelocity,
                    void* localBounds);
    MissileStepResult UpdateMissile(float dt, const WorldState* world);
    const CD3DVECTOR& Direction() const;
    float Lifetime() const;
    float Thrust() const;
    void* Owner() const;
    void SetTrail(void* trail);
    CD3DVECTOR Recoil() const;
};
static_assert(sizeof(CD3DMISSILEOBJECT) == 0x0fe0);

struct MissileStepResult {
    CD3DVECTOR collisionStart{};
    CD3DVECTOR collisionEnd{};
    bool lifetimeExpired = false;
    bool hidden = false;
    bool spawnExplosion = false;
    CD3DVECTOR explosionPosition{};
    std::uint8_t explosionType = 1;
    float explosionStrength = 10000.0f;
    float explosionRadius = 15.0f;
    bool updateTrail = true;
    bool disableTrailRespawn = false;
};

std::string Missile_TrailName(std::string_view missileName);

std::string Missile_ExplosionName(std::string_view missileName);

struct MissileTrailCreate {
    bool badTrailPointer = false;
    bool createTrail = false;
};
MissileTrailCreate Missile_BuildTrail(bool trailAlreadyExists,
                                                    ParticleUse particles);

ParticleConfig Missile_TrailConfig(const CD3DVECTOR& worldForward);

struct MissileTrailUpdate {
    bool updatePosition = false;
    CD3DVECTOR position{};
    bool disableRespawn = false;
};
MissileTrailUpdate Missile_UpdateTrailPlan(bool trailExists,
                                                    bool missileHidden,
                                                    const CD3DVECTOR& position);

class MissileFX {
public:
    virtual ~MissileFX() = default;
    virtual void* CreateTrailEmitter(std::size_t nativeBytes,
                                      std::string_view fullName,
                                      const CD3DVECTOR& position,
                                      const CD3DVECTOR& forward) = 0;
    virtual void SetTrailPosition(void* trail, const CD3DVECTOR& position) = 0;
    virtual void SetTrailRespawn(void* trail, bool enabled) = 0;
    virtual void* CreateExplosion(std::size_t nativeBytes,
                                   std::string_view fullName,
                                   std::uint8_t type,
                                   const CD3DVECTOR& position,
                                   float strength,
                                   float radius) = 0;
};

void* Missile_CreateTrail(std::string_view missileName,
                                ParticleUse particles,
                                bool trailAlreadyExists,
                                const CD3DVECTOR& position,
                                const CD3DVECTOR& forward,
                                MissileFX& backend);
void Missile_UpdateTrail(void* trail,
                               bool missileHidden,
                               const CD3DVECTOR& position,
                               MissileFX& backend);
void* Missile_CreateExplosion(std::string_view missileName,
                                    const MissileStepResult& result,
                                    MissileFX& backend);

class CarMissileIO {
public:
    virtual ~CarMissileIO() = default;
    virtual std::string_view CarName(void* car) const = 0;
    virtual bool DynamicNameExists(std::string_view fullName) const = 0;
    virtual void* CreateMissile(std::size_t nativeBytes,
                                               std::string_view fullName,
                                               void* ownerCar,
                                               const CD3DVECTOR& position,
                                               const CD3DVECTOR& direction) = 0;
};

void Car_SetMissileIO(CarMissileIO* backend);
CarMissileIO* Car_GetMissileIO();

struct MissileSpawn {
    std::string name;
    CD3DVECTOR position{};
    CD3DVECTOR direction{};
};
struct MissileFire {
    bool accepted = false;
    std::uint8_t spawnCount = 0;
    MissileSpawn spawn[2]{};
};
MissileFire Car_BuildMissileFire(void* car,
                                         std::string_view carName,
                                         bool missile1AlreadyExists,
                                         bool missile2AlreadyExists);

}
