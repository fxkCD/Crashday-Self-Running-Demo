#include "missile.hpp"
#include "memory.hpp"
#include "offsets.hpp"
#include "transform.hpp"
#include "world.hpp"
#include <cmath>
#include <cstring>
#include <string>

namespace flydemo {
namespace {
float mulStore(float a, float b) {
    return static_cast<float>(static_cast<long double>(a) * static_cast<long double>(b));
}
float subStore(float a, float b) {
    return static_cast<float>(static_cast<long double>(a) - static_cast<long double>(b));
}
CD3DVECTOR scaleStore(CD3DVECTOR v, float scalar) {
    v.x = mulStore(v.x, scalar);
    v.y = mulStore(v.y, scalar);
    v.z = mulStore(v.z, scalar);
    return v;
}
}

void CD3DMISSILEOBJECT::Initialize(void* ownerCar,
                                    const CD3DVECTOR& position,
                                    const CD3DVECTOR& direction,
                                    std::int32_t randomLifetime,
                                    std::int32_t randomThrust,
                                    std::int16_t mass,
                                    const CD3DVECTOR& size,
                                    const CD3DMATRIX& orientation,
                                    const CD3DVECTOR& inheritedVelocity,
                                    void* localBounds) {
    std::memset(this, 0, sizeof(*this));
    if (mass <= 0)
        mass = 1;

    const long double x = static_cast<long double>(direction.x);
    const long double y = static_cast<long double>(direction.y);
    const long double z = static_cast<long double>(direction.z);
    const long double invLength = 1.0L / std::sqrt(x * x + y * y + z * z);
    missile_.direction.x = static_cast<float>(x * invLength);
    missile_.direction.y = static_cast<float>(y * invLength);
    missile_.direction.z = static_cast<float>(z * invLength);
    if (static_cast<long double>(missile_.direction.y) > -0.2L &&
        static_cast<long double>(missile_.direction.y) < 0.2L)
        missile_.direction.y = 0.0f;

    missile_.owner = static_cast<std::uint32_t>(
        reinterpret_cast<std::uintptr_t>(ownerCar));
    missile_.trail = 0;
    missile_.lifetime = static_cast<float>(
        static_cast<long double>(randomLifetime % 1000) *
            static_cast<long double>(0.0010000000474974513f) +
        static_cast<long double>(3.0f));
    missile_.thrust = static_cast<float>(randomThrust % 10 + 29);

    mem::field<float>(this, off::OBJECT_SIZE_X) = size.x;
    mem::field<float>(this, off::OBJECT_SIZE_Y) = size.y;
    mem::field<float>(this, off::OBJECT_SIZE_Z) = size.z;
    mem::field<std::int16_t>(this, off::PHYS_MASS) = mass;
    mem::field<CD3DVECTOR>(this, off::OBJECT_BOUNDS_CENTER) = position;
    mem::field<CD3DVECTOR>(this, off::PHYS_POSITION) = position;
    mem::field<CD3DMATRIX>(this, off::PHYS_MATRIX) = orientation;
    SetIdentity(mem::field<CD3DMATRIX>(this, off::OBJECT_PENDING_TRANSFORM));
    mem::field<CD3DVECTOR>(this, off::LINEAR_VELOCITY) = inheritedVelocity;

    const float bodyMass = static_cast<float>(mass);
    const float x2 = size.x * size.x;
    const float y2 = size.y * size.y;
    const float z2 = size.z * size.z;
    constexpr float oneTwelfth = 1.0f / 12.0f;
    mem::field<CD3DVECTOR>(this, off::PRINCIPAL_INERTIA) = {
        bodyMass * oneTwelfth * (y2 + z2),
        bodyMass * oneTwelfth * (x2 + z2),
        bodyMass * oneTwelfth * (x2 + y2)};

    std::memset(localBounds, 0, 8u * 0x20u);
    const float hx = size.x * 0.5f;
    const float hy = size.y * 0.5f;
    const float hz = size.z * 0.5f;
    const CD3DVECTOR corners[8] = {
        {-hx, -hy, -hz}, { hx, -hy, -hz},
        { hx,  hy, -hz}, {-hx,  hy, -hz},
        {-hx, -hy,  hz}, { hx, -hy,  hz},
        { hx,  hy,  hz}, {-hx,  hy,  hz}
    };
    for (unsigned i = 0; i < 8; ++i)
        mem::field<CD3DVECTOR>(localBounds, i * 0x20u) = corners[i];
    mem::store_pointer32(this, off::LOCAL_BOUNDS_ARRAY, localBounds);

    const std::int16_t faces[6][3] = {
        {0, 3, 2}, {4, 5, 6}, {0, 4, 7},
        {1, 2, 6}, {0, 1, 5}, {3, 7, 6}
    };
    for (unsigned face = 0; face < 6; ++face)
        for (unsigned index = 0; index < 3; ++index)
            mem::field<std::int16_t>(this, off::OBJECT_BOUNDS_FACES +
                face * 0x60u + index * 2u) = faces[face][index];

    mem::field<std::uint8_t>(this, off::GRAVITY_DAMPING_ACTIVE) = 0;
    mem::field<std::uint8_t>(this, off::COLLISION_ACTIVE) = 0;
    mem::field<std::uint8_t>(this, off::FORCE_BLOCKED_06A7) = 0;
    ResetForces();
    ResetImpulses();
    mem::field<CD3DVECTOR>(this, off::ANGULAR_VELOCITY) = {0.0f, 0.0f, 0.0f};
    SetActivePhysics(true);
    mem::field<std::uint8_t>(this, off::OBJECT_STATE_0090) = 1;
    mem::field<std::uint8_t>(this, off::OBJECT_HIDDEN_0091) = 0;
    Object_UpdateBase(this, 0.0f);
}

const CD3DVECTOR& CD3DMISSILEOBJECT::Direction() const {
    return missile_.direction;
}

float CD3DMISSILEOBJECT::Lifetime() const {
    return missile_.lifetime;
}

float CD3DMISSILEOBJECT::Thrust() const {
    return missile_.thrust;
}

void* CD3DMISSILEOBJECT::Owner() const {
    return reinterpret_cast<void*>(static_cast<std::uintptr_t>(missile_.owner));
}

void CD3DMISSILEOBJECT::SetTrail(void* trail) {
    missile_.trail = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(trail));
}

CD3DVECTOR CD3DMISSILEOBJECT::Recoil() const {
    return scaleStore(missile_.direction, -2000.0f);
}

MissileStepResult CD3DMISSILEOBJECT::UpdateMissile(float dt, const WorldState* world) {
    MissileStepResult out;
    const CD3DVECTOR previous = mem::field<CD3DVECTOR>(this, off::OBJECT_BOUNDS_CENTER);
    out.collisionStart = previous;

    CD3DVECTOR force = scaleStore(missile_.direction, missile_.thrust);
    force = scaleStore(force, static_cast<float>(mem::field<std::int16_t>(this, off::PHYS_MASS)));
    AddForce(force);

    CD3DMATRIX spin{};
    MakeZRotation(spin, dt * 256.0f);
    ComposeTransform(spin, mem::field<CD3DMATRIX>(this, off::PHYS_MATRIX));
    CopyTransform(mem::field<CD3DMATRIX>(this, off::PHYS_MATRIX), spin);

    CD3DDYNAMICOBJECT::Update(dt);
    const CD3DVECTOR current = mem::field<CD3DVECTOR>(this, off::OBJECT_BOUNDS_CENTER);
    out.collisionEnd = current;

    missile_.lifetime = subStore(missile_.lifetime, dt);
    out.lifetimeExpired = missile_.lifetime <= 0.0f;
    if (out.lifetimeExpired) {
        mem::field<std::uint8_t>(this, off::OBJECT_HIDDEN_0091) = 1;
        out.explosionPosition = current;
    }

    if (world != nullptr) {
        const WorldSegmentHit hit = world->TraceStaticSegment(previous, current);
        if (hit.hit) {
            mem::field<std::uint8_t>(this, off::OBJECT_HIDDEN_0091) = 1;
            out.explosionPosition = hit.point;
        }
    }

    out.hidden = mem::field<std::uint8_t>(this, off::OBJECT_HIDDEN_0091) == 1;
    out.spawnExplosion = out.hidden;
    out.disableTrailRespawn = out.hidden;
    return out;
}

std::string Missile_TrailName(std::string_view missileName) {
    std::string out(missileName);
    out += "_trail";
    return out;
}

std::string Missile_ExplosionName(std::string_view missileName) {
    std::string out(missileName);
    out += "_explosion";
    return out;
}

MissileTrailCreate Missile_BuildTrail(bool trailAlreadyExists,
                                                    ParticleUse particles) {
    MissileTrailCreate out;

    out.badTrailPointer = trailAlreadyExists;

    out.createTrail = particles != ParticleUse::Never;
    return out;
}

ParticleConfig Missile_TrailConfig(const CD3DVECTOR& worldForward) {
    ParticleConfig cfg;
    cfg.type = ParticleType::Texture;
    cfg.startCount = 0;
    cfg.maxCount = 100;
    cfg.respawn = true;
    cfg.respawnRate = 30;
    cfg.lifetime = 1.8f;
    cfg.minSpeed = 1.0f;
    cfg.maxSpeed = 2.0f;

    cfg.acceleration = {3.0f, 2.0f, 0.0f};
    cfg.direction = worldForward;
    cfg.variance = 1.0f;
    cfg.textureName = "gfx/smoke.cbm";
    cfg.renderMode = ParticleRenderMode::Add;
    cfg.startColor = 0x000F0F0Fu;
    cfg.endColor = 0x000F0F0Fu;
    cfg.startSize = 120.0f;
    cfg.endSize = 1000.0f;
    return cfg;
}

MissileTrailUpdate Missile_UpdateTrailPlan(bool trailExists,
                                                    bool missileHidden,
                                                    const CD3DVECTOR& position) {
    MissileTrailUpdate out;
    if (!trailExists)
        return out;

    out.updatePosition = true;
    out.position = position;

    out.disableRespawn = missileHidden;
    return out;
}

void* Missile_CreateTrail(std::string_view missileName,
                                ParticleUse particles,
                                bool trailAlreadyExists,
                                const CD3DVECTOR& position,
                                const CD3DVECTOR& forward,
                                MissileFX& backend) {
    const auto plan = Missile_BuildTrail(trailAlreadyExists, particles);
    (void)plan.badTrailPointer;
    if (!plan.createTrail)
        return nullptr;

    constexpr std::size_t kNativeTrailBytes = 0x154u;
    return backend.CreateTrailEmitter(kNativeTrailBytes,
                                      Missile_TrailName(missileName),
                                      position, forward);
}

void Missile_UpdateTrail(void* trail,
                               bool missileHidden,
                               const CD3DVECTOR& position,
                               MissileFX& backend) {
    const auto plan = Missile_UpdateTrailPlan(trail != nullptr,
                                                   missileHidden, position);
    if (!plan.updatePosition)
        return;
    backend.SetTrailPosition(trail, plan.position);
    if (plan.disableRespawn)
        backend.SetTrailRespawn(trail, false);
}

void* Missile_CreateExplosion(std::string_view missileName,
                                    const MissileStepResult& result,
                                    MissileFX& backend) {
    if (!result.spawnExplosion)
        return nullptr;

    constexpr std::size_t kNativeExplosionBytes = 0xF4u;
    return backend.CreateExplosion(kNativeExplosionBytes,
                                   Missile_ExplosionName(missileName),
                                   result.explosionType, result.explosionPosition,
                                   result.explosionStrength, result.explosionRadius);
}

}
