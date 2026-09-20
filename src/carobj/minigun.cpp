#include "minigun.hpp"
#include "memory.hpp"
#include "offsets.hpp"
#include "transform.hpp"
#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <string>

namespace flydemo {
using flydemo::mem::field;

namespace {
float x87MulStore(float a, float b) {
    return static_cast<float>(static_cast<long double>(a) * static_cast<long double>(b));
}
float x87AddStore(float a, float b) {
    return static_cast<float>(static_cast<long double>(a) + static_cast<long double>(b));
}
int x87TruncToInt(long double value) {

    return static_cast<int>(std::trunc(value));
}
}

void Minigun_Init(void* object, MinigunSide side, int ammo) {
    const auto raw = static_cast<std::uint8_t>(side);
    assert(raw <= static_cast<std::uint8_t>(MinigunSide::Right));
    assert(ammo >= 0);
    field<std::uint8_t>(object, minigun_off::Side) = raw;
    field<int>(object, minigun_off::Ammo) = ammo;
    field<std::uint8_t>(object, minigun_off::FireLatch) = 0;
    field<float>(object, minigun_off::SpinStep) = 0.0f;
    field<float>(object, minigun_off::BarrelAngle) = 0.0f;
}

bool Minigun_Fire(void* object) {

    if (field<int>(object, minigun_off::Ammo) == 0)
        return false;
    field<std::uint8_t>(object, minigun_off::FireLatch) = 1;

    return true;
}

int Minigun_GetAmmo(const void* object) {
    return field<int>(object, minigun_off::Ammo);
}

void Minigun_SetAmmo(void* object, int ammo) {

    assert(ammo >= 0);
    field<int>(object, minigun_off::Ammo) = ammo;
}

void Minigun_Update(void* object, float dt) {
    auto& ammo = field<int>(object, minigun_off::Ammo);
    auto& spin = field<float>(object, minigun_off::SpinStep);
    auto& angle = field<float>(object, minigun_off::BarrelAngle);
    const bool firing = field<std::uint8_t>(object, minigun_off::FireLatch) != 0;

    if (firing) {
        const long double dt80 = static_cast<long double>(dt);
        ammo -= x87TruncToInt(dt80 * static_cast<long double>(50.0f));
        if (ammo < 0)
            ammo = 0;
        spin = static_cast<float>(dt80 * static_cast<long double>(2500.0f));
    } else {

        const long double denom = 1.0L + static_cast<long double>(dt) * 3.0L;
        spin = static_cast<float>(static_cast<long double>(spin) / denom);
    }

    angle = x87AddStore(spin, angle);

    field<std::uint8_t>(object, minigun_off::FireLatch) = 0;
}

std::uint32_t Minigun_MuzzleColor(std::int32_t randomValue) {
    const std::int32_t remainder = randomValue % 3;
    const long double scale = 1.0L / static_cast<long double>(remainder + 1);
    const auto r = static_cast<std::uint32_t>(x87TruncToInt(scale * static_cast<long double>(179.0f))) & 0xffu;
    const auto g = static_cast<std::uint32_t>(x87TruncToInt(scale * static_cast<long double>(98.0f))) & 0xffu;
    const auto b = static_cast<std::uint32_t>(x87TruncToInt(scale * static_cast<long double>(240.0f))) & 0xffu;
    return (b << 16) | (g << 8) | r;
}

MinigunLight Minigun_BuildLight(bool firing,
                                                     std::int32_t randomValue) {
    MinigunLight out{};
    if (!firing)
        return out;
    const std::uint32_t color = Minigun_MuzzleColor(randomValue);
    out.light0 = color;
    out.light1 = color;
    out.light2 = color;
    return out;
}

namespace {
float floatFromBits(std::uint32_t bits) {
    float out = 0.0f;
    std::memcpy(&out, &bits, sizeof(out));
    return out;
}

CD3DVECTOR shotfireOffset(std::int32_t randomValue) {

    CD3DVECTOR out{};
    switch (randomValue % 4) {
    case 0:
        out = {floatFromBits(0xBCA3D70Au), floatFromBits(0x3CA3D70Au),
               floatFromBits(0x3F266666u)};
        break;
    case 1:
        out = {floatFromBits(0x3CA3D70Au), floatFromBits(0x3CA3D70Au),
               floatFromBits(0x3F266666u)};
        break;
    case 2:
        out = {floatFromBits(0xBCA3D70Au), floatFromBits(0xBCA3D70Au),
               floatFromBits(0x3F266666u)};
        break;
    case 3:
        out = {floatFromBits(0x3CA3D70Au), floatFromBits(0xBCA3D70Au),
               floatFromBits(0x3F266666u)};
        break;
    default:
        break;
    }
    return out;
}

std::uint32_t shotfireColor(std::int32_t randomValue) {
    switch (randomValue % 4) {
    case 0: return 0x00DDDDDDu;
    case 1: return 0x00333333u;
    case 2: return 0x00AAAAAAu;
    case 3: return 0x00000000u;
    default: return 0u;
    }
}
}

MinigunFlash Minigun_BuildFlash(ParticleUse particles,
                                               bool firing,
                                               bool emitterAlreadyExists,
                                               std::int32_t randomOffset,
                                               std::int32_t randomColor) {
    MinigunFlash out{};

    if (particles == ParticleUse::Never)
        return out;

    out.process = true;
    out.localOffset = shotfireOffset(randomOffset);
    out.color = shotfireColor(randomColor);

    if (emitterAlreadyExists) {
        out.lookupEmitter = true;
    } else if (firing) {
        out.createEmitter = true;
    }

    if (emitterAlreadyExists || out.createEmitter) {
        out.setRespawn = true;
        out.respawn = firing;
        if (firing) {
            out.updatePosition = true;
            out.updateDirection = true;
            out.updateColor = true;
        }
    }
    return out;
}

MinigunBullet Minigun_BuildBulletPlan(ParticleUse particles,
                                           MinigunSide side,
                                           bool firing,
                                           bool emitterAlreadyExists) {
    MinigunBullet out{};

    if (particles != ParticleUse::Always)
        return out;

    out.process = true;
    out.localPosition = {floatFromBits(0xBDA3D70Au),
                         floatFromBits(0x3C23D70Au),
                         floatFromBits(0xBD75C28Fu)};
    out.localDirection = {-2.0f, 4.0f, 0.0f};
    out.offsetVector = {0.0f, -8.0f, 0.0f};

    if (side == MinigunSide::Right) {

        std::uint32_t xbits = 0;
        std::memcpy(&xbits, &out.localPosition.x, sizeof(xbits));
        xbits ^= 0x80000000u;
        std::memcpy(&out.localPosition.x, &xbits, sizeof(xbits));
        std::memcpy(&xbits, &out.localDirection.x, sizeof(xbits));
        xbits ^= 0x80000000u;
        std::memcpy(&out.localDirection.x, &xbits, sizeof(xbits));
    }

    if (emitterAlreadyExists) {
        out.lookupEmitter = true;
    } else if (firing) {
        out.createEmitter = true;
    }

    if (emitterAlreadyExists || out.createEmitter) {
        out.setRespawn = true;
        out.respawn = firing;
        if (firing) {
            out.updatePosition = true;
            out.updateDirection = true;
        }
    }
    return out;
}

ParticleConfig Minigun_ShotConfig(const CD3DVECTOR& worldDirection,
                                                    std::uint32_t color) {
    ParticleConfig cfg;
    cfg.type = ParticleType::Texture;
    cfg.startCount = 1;
    cfg.maxCount = 3;
    cfg.respawn = true;
    cfg.respawnRate = 20;
    cfg.lifetime = 0.01f;
    cfg.minSpeed = 0.0f;
    cfg.maxSpeed = 0.0f;
    cfg.acceleration = {0.0f, 0.0f, 0.0f};
    cfg.direction = worldDirection;
    cfg.variance = 0.0f;
    cfg.textureName = "gfx/shotfire.cbm";
    cfg.renderMode = ParticleRenderMode::Add;
    cfg.startColor = color;
    cfg.endColor = color;
    cfg.startSize = 120.0f;
    cfg.endSize = 120.0f;
    return cfg;
}

ParticleConfig Minigun_BulletConfig(const CD3DVECTOR& worldDirection,
                                                  const CD3DVECTOR& acceleration,
                                                  std::uint32_t ambientColor) {
    ParticleConfig cfg;
    cfg.type = ParticleType::Texture;
    cfg.startCount = 0;
    cfg.maxCount = 20;
    cfg.respawn = true;
    cfg.respawnRate = 20;
    cfg.lifetime = 1.0f;
    cfg.minSpeed = 1.5f;
    cfg.maxSpeed = 2.5f;
    cfg.acceleration = acceleration;
    cfg.direction = worldDirection;
    cfg.variance = 0.0f;
    cfg.textureName = "gfx/bullets.cbm";
    cfg.renderMode = ParticleRenderMode::Solid;
    cfg.startColor = ambientColor;
    cfg.endColor = ambientColor;
    cfg.startSize = 15.0f;
    cfg.endSize = 15.0f;
    return cfg;
}

namespace {
std::string minigunEmitterName(std::string_view base, const char* suffix) {
    std::string out(base);
    out += suffix;
    return out;
}

CD3DVECTOR effectPoint(void* object, CD3DVECTOR local) {
    TransformPoint(field<CD3DMATRIX>(object, off::PHYS_MATRIX), local);
    const CD3DVECTOR base = field<CD3DVECTOR>(object, off::OBJECT_BOUNDS_CENTER);
    local.x = static_cast<float>(static_cast<long double>(local.x) + base.x);
    local.y = static_cast<float>(static_cast<long double>(local.y) + base.y);
    local.z = static_cast<float>(static_cast<long double>(local.z) + base.z);
    return local;
}

CD3DVECTOR effectDirection(void* object, CD3DVECTOR local) {
    TransformPoint(field<CD3DMATRIX>(object, off::PHYS_MATRIX), local);
    return local;
}
}

MinigunEffects Minigun_ApplyParticles(void* object,
                                                     ParticleUse particles,
                                                     bool firing,
                                                     std::int32_t randomOffset,
                                                     std::int32_t randomColor,
                                                     MinigunFX& backend) {
    MinigunEffects result{};
    constexpr std::size_t kNativeTexParticleBytes = 0x154u;
    const std::string_view baseName = backend.ObjectName(object);

    if (particles != ParticleUse::Never) {
        result.shotfireProcessed = true;
        const std::string name = minigunEmitterName(baseName, "pC_shotfire");
        const bool exists = backend.DynamicNameExists(name);
        const auto plan = Minigun_BuildFlash(particles, firing, exists,
                                                     randomOffset, randomColor);
        void* emitter = nullptr;
        const CD3DVECTOR position = effectPoint(object, plan.localOffset);
        const CD3DVECTOR direction = field<CD3DVECTOR>(object, off::OBJECT_FORWARD);
        if (exists) {
            emitter = backend.FindDynamic(name);
        } else if (plan.createEmitter) {
            ++result.constructorAttempts;
            emitter = backend.CreateShotfireEmitter(kNativeTexParticleBytes, name,
                                                     position, direction, plan.color);
        }

        if (emitter != nullptr && plan.setRespawn) {
            backend.SetEmitterRespawn(emitter, plan.respawn);
            if (plan.updatePosition) backend.SetEmitterPosition(emitter, position);
            if (plan.updateDirection) backend.SetEmitterDirection(emitter, direction);
            if (plan.updateColor) backend.SetEmitterColor(emitter, plan.color);
        }
    }

    if (particles == ParticleUse::Always) {
        result.bulletsProcessed = true;
        const auto side = static_cast<MinigunSide>(field<std::uint8_t>(object, minigun_off::Side));
        const std::string name = minigunEmitterName(baseName, "_bullets");
        const bool exists = backend.DynamicNameExists(name);
        const auto plan = Minigun_BuildBulletPlan(particles, side, firing, exists);
        const CD3DVECTOR position = effectPoint(object, plan.localPosition);
        const CD3DVECTOR direction = effectDirection(object, plan.localDirection);
        void* emitter = nullptr;
        if (exists) {
            emitter = backend.FindDynamic(name);
        } else if (plan.createEmitter) {
            ++result.constructorAttempts;
            emitter = backend.CreateBulletEmitter(kNativeTexParticleBytes, name,
                                                   position, direction,
                                                   plan.offsetVector);
        }
        if (emitter != nullptr && plan.setRespawn) {
            backend.SetEmitterRespawn(emitter, plan.respawn);
            if (plan.updatePosition) backend.SetEmitterPosition(emitter, position);
            if (plan.updateDirection) backend.SetEmitterDirection(emitter, direction);
        }
    }

    return result;
}

}
