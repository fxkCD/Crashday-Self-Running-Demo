#include "particle.hpp"

#include <cmath>
#include <cstdint>
#include <cstring>

namespace flydemo {
namespace {

bool Fail(std::string* error, const char* message) {
    if (error)
        *error = message;
    return false;
}

std::int32_t RawFloatBitsAsSigned(float value) {
    std::uint32_t bits = 0;
    static_assert(sizeof(bits) == sizeof(value));
    std::memcpy(&bits, &value, sizeof(bits));
    return static_cast<std::int32_t>(bits);
}

bool VarianceAccepted(float value) {

    if (!std::isnan(value) && 0.0f > value)
        return false;
    return RawFloatBitsAsSigned(value) <= static_cast<std::int32_t>(0x3F800000u);
}

CD3DVECTOR NormalizeUnchecked(CD3DVECTOR value) {

    const long double x = value.x;
    const long double y = value.y;
    const long double z = value.z;
    const long double inv = 1.0L / std::sqrt(x * x + y * y + z * z);
    value.x = static_cast<float>(x * inv);
    value.y = static_cast<float>(y * inv);
    value.z = static_cast<float>(z * inv);
    return value;
}

}

bool ValidateParticleConfig(const ParticleConfig& config, std::string* error) {
    const auto type = static_cast<std::uint8_t>(config.type);
    const auto mode = static_cast<std::uint8_t>(config.renderMode);

    if (type != 0 && type != 1)
        return Fail(error, "type must be LINE_PARTICLE or TEXTURE_PARTICLE");

    if (config.maxCount < config.startCount)
        return Fail(error, "startcount must be <= maxcount");

    if (!std::isnan(config.minSpeed) && !std::isnan(config.maxSpeed) &&
        config.minSpeed > config.maxSpeed)
        return Fail(error, "minspeed must be <= maxspeed");

    if (!VarianceAccepted(config.variance))
        return Fail(error, "variance must satisfy the native 0..1 check");

    if (mode != 0 && mode != 1)
        return Fail(error, "rendermode must be SOLID or ADD");

    if (error)
        error->clear();
    return true;
}

bool CD3DPARTICLEOBJECT::NeedsBaseUpdate() {
    dirty = true;

    if (activeCount == 0 && !config.respawn) {
        hidden = true;
        return false;
    }
    return true;
}

bool CD3DPARTICLEOBJECT::SetRespawnRaw(std::uint8_t respawn, std::string* error) {

    const bool valid = respawn == 0 || respawn == 1;
    config.respawn = respawn != 0;
    if (!valid)
        return Fail(error, "respawn must be 0 or 1");
    if (error)
        error->clear();
    return true;
}

void CD3DPARTICLEOBJECT::SetEmitterPosition(const CD3DVECTOR& value) {

    emitterPosition = value;
}

void CD3DPARTICLEOBJECT::SetAttachmentObject(std::string value) {

    config.attachmentObject = std::move(value);
}

void CD3DPARTICLEOBJECT::SetDirection(const CD3DVECTOR& value) {

    config.direction = NormalizeUnchecked(value);
}

void CD3DPARTICLEOBJECT::SetStartColor(std::uint32_t color) {

    config.startColor = color;
}

}
