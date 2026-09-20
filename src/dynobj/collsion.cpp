#include <cassert>
#include <cstdint>
#include "types.hpp"
#include "offsets.hpp"
#include "memory.hpp"
#include "dynamic_object.hpp"

namespace flydemo {
using namespace flydemo;
using flydemo::mem::field;
using Wide = long double;

namespace {
constexpr int kMaxContacts = 12;
constexpr Wide kRestitutionFactor = Wide(-1.9L);
constexpr Wide kClosingThreshold = Wide(-0.01L);

CD3DVECTOR subtract(CD3DVECTOR a, const CD3DVECTOR& b) {
    return {
        static_cast<float>(static_cast<Wide>(a.x) - b.x),
        static_cast<float>(static_cast<Wide>(a.y) - b.y),
        static_cast<float>(static_cast<Wide>(a.z) - b.z),
    };
}

CD3DVECTOR crossRounded(const CD3DVECTOR& a, const CD3DVECTOR& b) {

    return {
        static_cast<float>(static_cast<Wide>(a.y) * b.z - static_cast<Wide>(a.z) * b.y),
        static_cast<float>(static_cast<Wide>(a.z) * b.x - static_cast<Wide>(a.x) * b.z),
        static_cast<float>(static_cast<Wide>(a.x) * b.y - static_cast<Wide>(a.y) * b.x),
    };
}

float dotRounded(const CD3DVECTOR& a, const CD3DVECTOR& b) {

    return static_cast<float>((static_cast<Wide>(a.x) * b.x
                             + static_cast<Wide>(a.y) * b.y)
                             + static_cast<Wide>(a.z) * b.z);
}

float matrixElement(const void* object, unsigned row, unsigned col) {
    return field<float>(object, off::WORLD_INV_INERTIA + row * 0x10u + col * 4u);
}

CD3DVECTOR ApplyInvInertia(const void* object, const CD3DVECTOR& v) {
    CD3DVECTOR out{};
    float* dst[3] = {&out.x, &out.y, &out.z};
    for (unsigned row = 0; row < 3; ++row) {
        *dst[row] = static_cast<float>(
            (static_cast<Wide>(v.x) * matrixElement(object, row, 0)
           + static_cast<Wide>(v.y) * matrixElement(object, row, 1))
           + static_cast<Wide>(v.z) * matrixElement(object, row, 2));
    }
    return out;
}

void applyContactImpulse(void* object,
                         float impulse,
                         const CD3DVECTOR& normal,
                         const CD3DVECTOR& angularResponse) {

    const auto mass = field<std::int16_t>(object, off::PHYS_MASS);
    if (mass == 0)
        return;

    CD3DVECTOR& linear = field<CD3DVECTOR>(object, off::LINEAR_VELOCITY);
    const auto advanceLinear = [impulse, mass](float value, float n) {
        return static_cast<float>((static_cast<Wide>(impulse) * n) /
                                  static_cast<Wide>(mass) + value);
    };
    linear = {
        advanceLinear(linear.x, normal.x),
        advanceLinear(linear.y, normal.y),
        advanceLinear(linear.z, normal.z),
    };

    CD3DVECTOR& angular = field<CD3DVECTOR>(object, off::ANGULAR_VELOCITY);
    const auto advanceAngular = [impulse](float value, float response) {
        return static_cast<float>(static_cast<Wide>(impulse) * response + value);
    };
    angular = {
        advanceAngular(angular.x, angularResponse.x),
        advanceAngular(angular.y, angularResponse.y),
        advanceAngular(angular.z, angularResponse.z),
    };
}
}

void CD3DDYNAMICOBJECT::SetActiveCollision(bool state) {
    assert(state == 0 || state == 1);
    field<std::uint8_t>(this, off::COLLISION_ACTIVE) = static_cast<std::uint8_t>(state);

    if (field<std::uint8_t>(this, 0x6A3) == 0)
        field<std::uint16_t>(this, off::COLLISION_COUNT) = 0;
}

bool CD3DDYNAMICOBJECT::GetActiveCollision() const {
    return field<std::uint8_t>(this, off::COLLISION_ACTIVE) != 0;
}

void CD3DDYNAMICOBJECT::BuildGroundContacts() {

    std::uint16_t& count = field<std::uint16_t>(this, off::COLLISION_COUNT);

    for (int i = 0; i < 8; ++i) {
        const CD3DVECTOR p = field<CD3DVECTOR>(this,
            off::OBJECT_BOUNDS_POINTS + static_cast<unsigned>(i) * 0x20u);

        if (!(p.y < 0.0f))
            continue;
        if (count == kMaxContacts)
            return;

        const unsigned n = count++;
        field<CD3DVECTOR>(this, off::COLLISION_POINTS + n * 12u) = p;
        field<CD3DVECTOR>(this, off::COLLISION_NORMALS + n * 12u) = {0.0f, 1.0f, 0.0f};
        field<float>(this, off::COLLISION_DEPTHS + n * 4u) = -p.y;
    }
}

void CD3DDYNAMICOBJECT::SolveContacts() {

    const int count = static_cast<int>(
        field<std::int16_t>(this, off::COLLISION_COUNT));
    if (count <= 0)
        return;

    const CD3DVECTOR position = field<CD3DVECTOR>(this, off::PHYS_POSITION);
    const CD3DVECTOR linearVelocity = field<CD3DVECTOR>(this, off::LINEAR_VELOCITY);
    const CD3DVECTOR angularVelocity = field<CD3DVECTOR>(this, off::ANGULAR_VELOCITY);

    float bestNumerator = 0.0f;
    int selected = -1;
    CD3DVECTOR lastNormalCrossLever{0.0f, 0.0f, 0.0f};

    for (int i = 0; i < count; ++i) {
        const CD3DVECTOR point = field<CD3DVECTOR>(this,
            off::COLLISION_POINTS + static_cast<unsigned>(i) * 12u);
        const CD3DVECTOR normal = field<CD3DVECTOR>(this,
            off::COLLISION_NORMALS + static_cast<unsigned>(i) * 12u);
        const CD3DVECTOR lever = subtract(point, position);

        const float linearNormal = dotRounded(normal, linearVelocity);
        lastNormalCrossLever = crossRounded(normal, lever);
        const float angularNormal = dotRounded(lastNormalCrossLever, angularVelocity);

        const Wide closingSpeed = static_cast<Wide>(linearNormal) + angularNormal;
        if (!(closingSpeed < kClosingThreshold))
            continue;

        const float candidate = static_cast<float>(closingSpeed * kRestitutionFactor);
        if (candidate > bestNumerator) {
            bestNumerator = candidate;
            selected = i;
        }
    }

    if (selected < 0)
        return;

    const CD3DVECTOR angularResponse =
        ApplyInvInertia(this, lastNormalCrossLever);
    const float rotationalTerm = dotRounded(angularResponse, lastNormalCrossLever);

    const auto mass = field<std::int16_t>(this, off::PHYS_MASS);
    if (mass == 0)
        return;

    const int integerInverseMass = 1 / static_cast<int>(mass);
    const float denominator = static_cast<float>(
        static_cast<Wide>(integerInverseMass) + rotationalTerm);
    const float impulse = static_cast<float>(
        static_cast<Wide>(bestNumerator) / denominator);

    const CD3DVECTOR selectedNormal = field<CD3DVECTOR>(this,
        off::COLLISION_NORMALS + static_cast<unsigned>(selected) * 12u);
    applyContactImpulse(this, impulse, selectedNormal, angularResponse);
}

void CD3DDYNAMICOBJECT::ResetContacts() {

    field<std::uint16_t>(this, off::COLLISION_COUNT) = 0;
}

}
