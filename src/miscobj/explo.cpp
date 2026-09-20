#include "explosion.hpp"

#include <cmath>
#include <cstring>
#include <utility>

namespace flydemo {
namespace {

std::uint32_t FloatBits(float value) noexcept {
    std::uint32_t bits = 0;
    static_assert(sizeof(bits) == sizeof(value), "float must be 32-bit");
    std::memcpy(&bits, &value, sizeof(bits));
    return bits;
}

bool IsAllSignedZero(const CD3DVECTOR& v) noexcept {
    return (FloatBits(v.x) & 0x7FFFFFFFu) == 0u &&
           (FloatBits(v.y) & 0x7FFFFFFFu) == 0u &&
           (FloatBits(v.z) & 0x7FFFFFFFu) == 0u;
}

float VecLength(const CD3DVECTOR& v) noexcept {
    const long double y = static_cast<long double>(v.y);
    const long double x = static_cast<long double>(v.x);
    const long double z = static_cast<long double>(v.z);
    return static_cast<float>(std::sqrt(y * y + x * x + z * z));
}

CD3DVECTOR Subtract(const CD3DVECTOR& a, const CD3DVECTOR& b) noexcept {

    return {
        static_cast<float>(static_cast<long double>(a.x) - static_cast<long double>(b.x)),
        static_cast<float>(static_cast<long double>(a.y) - static_cast<long double>(b.y)),
        static_cast<float>(static_cast<long double>(a.z) - static_cast<long double>(b.z)),
    };
}

void NormalizeVecUnchecked(CD3DVECTOR& v) noexcept {

    const long double y = static_cast<long double>(v.y);
    const long double x = static_cast<long double>(v.x);
    const long double z = static_cast<long double>(v.z);
    const long double length = std::sqrt(y * y + x * x + z * z);
    const long double inverse = 1.0L / length;
    v.x = static_cast<float>(static_cast<long double>(v.x) * inverse);
    v.y = static_cast<float>(static_cast<long double>(v.y) * inverse);
    v.z = static_cast<float>(static_cast<long double>(v.z) * inverse);
}

void Scale(CD3DVECTOR& v, float scalar) noexcept {

    v.x = static_cast<float>(static_cast<long double>(v.x) * static_cast<long double>(scalar));
    v.y = static_cast<float>(static_cast<long double>(v.y) * static_cast<long double>(scalar));
    v.z = static_cast<float>(static_cast<long double>(v.z) * static_cast<long double>(scalar));
}

ExplosionEmitterSpec MainSmall() {
    return {"gfx/explo.cbm", 5, 0x00CDCDCDu, 300.0f, 600.0f,
            1.0f, 0.5f, 1.5f};
}

ExplosionEmitterSpec MainNormal() {
    return {"gfx/explo.cbm", 10, 0x00CDCDCDu, 600.0f, 1200.0f,
            1.0f, 1.0f, 2.5f};
}

ExplosionEmitterSpec MainBig() {
    return {"gfx/explo.cbm", 15, 0x00CDCDCDu, 800.0f, 1500.0f,
            1.0f, 2.0f, 4.0f};
}

ExplosionEmitterSpec CoronaLarge(bool big) {
    return {"gfx/corona.cbm", 1, 0x003C2800u,
            big ? 6000.0f : 4000.0f,
            big ? 6000.0f : 4000.0f,
            2.0f, 0.0f, 0.0f};
}

ExplosionEmitterSpec CoronaSparks() {
    return {"gfx/corona.cbm", 100, 0x00FAE6AAu, 40.0f, 40.0f,
            2.0f, 5.0f, 8.0f};
}

}

bool ValidateExplosionType(ExplosionType type) {
    const auto raw = static_cast<std::uint8_t>(type);
    return raw == 0u || raw == 1u || raw == 2u;
}

EXPLOSION ConstructExplosionObject(std::string name,
                                                const CD3DVECTOR& position,
                                                ExplosionType type,
                                                float strength,
                                                float radius) {
    EXPLOSION object;
    object.name = std::move(name);
    object.state.position = position;
    object.state.type = type;
    object.state.strength = strength;
    object.state.radius = radius;
    object.invalidType = !ValidateExplosionType(type);
    object.pendingWorldDelete = false;
    return object;
}

std::string BuildExplosionName(std::string_view baseName,
                                     int random1,
                                     int random2) {

    const int suffix = (random1 % 200) * random2;
    std::string out(baseName);
    out += "_sys";
    out += std::to_string(suffix);
    return out;
}

std::vector<ExplosionEmitterSpec> BuildExplosionFx(
        ExplosionType type, ParticleUse particles) {

    if (static_cast<std::uint8_t>(particles) ==
        static_cast<std::uint8_t>(ParticleUse::Never)) {
        return {};
    }

    std::vector<ExplosionEmitterSpec> out;
    const auto rawType = static_cast<std::uint8_t>(type);

    if (rawType == static_cast<std::uint8_t>(ExplosionType::Small)) {

        out.push_back(MainSmall());
        return out;
    }

    if (rawType == static_cast<std::uint8_t>(ExplosionType::Normal)) {

        out.push_back(MainNormal());
        if (static_cast<std::uint8_t>(particles) ==
            static_cast<std::uint8_t>(ParticleUse::Always)) {
            out.push_back(CoronaLarge(false));
            out.push_back(CoronaSparks());
        }
        return out;
    }

    out.push_back(MainBig());
    if (static_cast<std::uint8_t>(particles) ==
        static_cast<std::uint8_t>(ParticleUse::Always)) {
        out.push_back(CoronaLarge(true));
        out.push_back(CoronaSparks());
    }
    return out;
}

std::size_t SpawnExplosionVisuals(const EXPLOSION& object,
                                  ParticleUse particles,
                                  ExplosionFX& backend,
                                  std::size_t* successfulSpawns) {
    const auto recipe = BuildExplosionFx(object.state.type, particles);
    std::size_t spawned = 0;
    for (const auto& emitter : recipe) {

        const int random1 = backend.NextRandom();
        const int random2 = backend.NextRandom();
        const std::string name = BuildExplosionName(
            object.name, random1, random2);
        if (backend.SpawnEmitter(name, object.state.position, emitter)) {
            ++spawned;
        }
    }
    if (successfulSpawns != nullptr) *successfulSpawns = spawned;
    return recipe.size();
}

CD3DVECTOR ComputeExplosionImpulse(const CD3DVECTOR& center, const CD3DVECTOR& target,
                             float strength, float radius) {
    CD3DVECTOR direction = Subtract(target, center);
    const float distance = VecLength(direction);

    if (distance > radius) return {};

    NormalizeVecUnchecked(direction);

    const long double ratio = static_cast<long double>(distance) /
                              static_cast<long double>(radius);
    const long double oneMinus = 1.0L - ratio;
    const float falloffSquared = static_cast<float>(oneMinus * oneMinus);

    Scale(direction, falloffSquared);
    Scale(direction, strength);
    Scale(direction, 0.125f);
    return direction;
}

std::size_t ApplyExplosionForce(const ExplosionState& state,
                                       ExplosionWorld& world) {
    std::size_t calls = 0;
    for (std::size_t slot = 0; slot < ExplosionDynamicSlots; ++slot) {
        ExplosionDynamicBody* body = world.DynamicAt(slot);
        if (body == nullptr) continue;

        if (!body->IsDynamicObjectClass()) continue;

        const CD3DVECTOR coarse = Subtract(body->BoundsCenter(), state.position);
        const float coarseDistance = VecLength(coarse);
        if (coarseDistance > state.radius) continue;

        if (IsAllSignedZero(coarse)) continue;

        const auto& points = body->BoundsPoints();
        for (std::size_t i = 0; i < ExplosionPointCount; ++i) {
            const CD3DVECTOR delta = Subtract(points[i], state.position);
            const float distance = VecLength(delta);
            if (distance > state.radius) continue;

            const CD3DVECTOR impulse = ComputeExplosionImpulse(
                state.position, points[i], state.strength, state.radius);
            body->ApplyImpulseAtPoint(points[i], impulse);
            ++calls;
        }
    }
    return calls;
}

ExplosionUpdateResult UpdateExplosionObject(EXPLOSION& object,
                                            ParticleUse particles,
                                            ExplosionFX& visual,
                                            ExplosionWorld& world) {
    ExplosionUpdateResult result;
    result.emitterAttempts = SpawnExplosionVisuals(
        object, particles, visual, &result.emitterSpawns);
    result.impulsesApplied = ApplyExplosionForce(object.state, world);
    object.pendingWorldDelete = true;
    return result;
}

}
