#include <cassert>
#include <cmath>
#include <cstdint>
#include <string>
#include <string_view>
#include "types.hpp"
#include "offsets.hpp"
#include "memory.hpp"
#include "car_object.hpp"
#include "afterburner.hpp"
#include "minigun.hpp"
#include "missile.hpp"
#include "transform.hpp"

namespace flydemo {
using namespace flydemo;
using flydemo::mem::field;
using flydemo::mem::pointer32;

extern float Renderer_GetFrameDelta();
extern bool Wheel_HasGroundContact(void* wheel);
extern const CD3DVECTOR* Object_GetForward(void* object);
extern const CD3DVECTOR* Object_GetPosition(void* object);
extern float Car_GetStageSpeed(void* car, int stage);
extern float Car_GetDriveCoefficient(void* car, int stage);
extern float Car_ReverseBrake(void* car);
extern void Dynamic_AddForceAtPoint(void* body,
                                    const CD3DVECTOR& point,
                                    const CD3DVECTOR& force);

static inline bool CarCanAct(const CD3DCAROBJECT* self) {

    return (field<float>(self, off::CAR_ALIVE_VALUE) != 0.0f);
}

void CD3DCAROBJECT::Brake() {

    if (!CarCanAct(this))
        return;
    field<std::uint8_t>(this, off::BRAKE_ACTIVE) = 1;
}

void CD3DCAROBJECT::Left(float rate) {

    assert(rate >= 0.0f && rate <= 100.0f);
    if (!CarCanAct(this))
        return;

    rate *= 0.01f;

    const float delta = field<float>(this, off::STEERING_RATE) * rate * Renderer_GetFrameDelta();

    float steering = field<float>(this, off::STEER_LEFT) - delta;
    if (steering < -20.0f)
        steering = -20.0f;

    field<std::uint8_t>(this, off::STEERING_ACTIVE) = 1;
    field<float>(this, off::STEER_LEFT)  = steering;
    field<float>(this, off::STEER_RIGHT) = steering;
}

void CD3DCAROBJECT::Right(float rate) {

    assert(rate >= 0.0f && rate <= 100.0f);
    if (!CarCanAct(this))
        return;

    rate *= 0.01f;
    const float delta = field<float>(this, off::STEERING_RATE) * rate * Renderer_GetFrameDelta();

    float steering = field<float>(this, off::STEER_LEFT) + delta;
    if (steering > 20.0f)
        steering = 20.0f;

    field<std::uint8_t>(this, off::STEERING_ACTIVE) = 1;
    field<float>(this, off::STEER_LEFT)  = steering;
    field<float>(this, off::STEER_RIGHT) = steering;
}

void CD3DCAROBJECT::Forward(float rate) {

    assert(rate >= 0.0f && rate <= 100.0f);
    rate *= 0.01f;
    if (!CarCanAct(this))
        return;

    const auto* raw = reinterpret_cast<const std::uint8_t*>(this);
    const int speedStage = static_cast<std::int8_t>(raw[off::NUM_GEARS]);
    const float speedLimit = Car_GetStageSpeed(this, speedStage);
    const float speed = field<float>(this, off::SPEED);
    if (speedLimit <= speed)
        return;

    const int forceStage = static_cast<std::int8_t>(raw[off::CURRENT_GEAR]);
    const float mass = static_cast<float>(field<std::int16_t>(this, off::PHYS_MASS));
    const float magnitude = Car_GetDriveCoefficient(this, forceStage)
                          * mass * rate * 0.5f;

    for (unsigned slot = 0; slot < 2; ++slot) {
        void* wheel = pointer32(this, off::WHEEL_PTR_BASE + slot * 0x10u);
        if (!Wheel_HasGroundContact(wheel))
            continue;

        CD3DVECTOR force = *Object_GetForward(wheel);
        force.x *= magnitude;
        force.y *= magnitude;
        force.z *= magnitude;
        Dynamic_AddForceAtPoint(this, *Object_GetPosition(wheel), force);
    }

    field<std::uint8_t>(this, off::FORWARD_ACTIVE) = 1;
}

void CD3DCAROBJECT::Reverse(float rate) {

    assert(rate >= 0.0f && rate <= 100.0f);
    rate *= 0.01f;
    if (!CarCanAct(this))
        return;

    const float speed = field<float>(this, off::SPEED);
    const float reverseLimit = Car_GetStageSpeed(this, -1);
    if (reverseLimit >= speed)
        return;

    const float mass = static_cast<float>(field<std::int16_t>(this, off::PHYS_MASS));
    float coeff;
    if (speed < 0.0f) {

        coeff = -Car_GetDriveCoefficient(this, -1);
    } else {

        coeff = Car_ReverseBrake(this);
    }
    const float magnitude = coeff * mass * rate * 0.5f;

    for (unsigned slot = 0; slot < 2; ++slot) {
        void* wheel = pointer32(this, off::WHEEL_PTR_BASE + slot * 0x10u);
        if (!Wheel_HasGroundContact(wheel))
            continue;

        CD3DVECTOR force = *Object_GetForward(wheel);
        force.x *= magnitude;
        force.y *= magnitude;
        force.z *= magnitude;
        Dynamic_AddForceAtPoint(this, *Object_GetPosition(wheel), force);
    }

    field<std::uint8_t>(this, off::REVERSE_ACTIVE) = 1;
}

bool CD3DCAROBJECT::FireMinigun() {

    if (!CarCanAct(this))
        return false;

    void* minigun = pointer32(this, off::MINIGUN_PTR);
    if (!minigun)
        return false;

    return Minigun_Fire(minigun);
}

bool CD3DCAROBJECT::UseAfterburner() {

    if (!CarCanAct(this))
        return false;

    void* aft = pointer32(this, off::AFTERBURNER_PTR);
    if (!aft)
        return false;

    if (!Afterburner_RequestUse(aft))
        return false;
    const AfterburnerInfo info = Afterburner_GetInfo(aft);

    CD3DVECTOR force{};
    if (info.usefulInAir == 1) {
        force = *Object_GetForward(this);
        force.x *= info.force;
        force.y *= info.force;
        force.z *= info.force;
        Dynamic_AddForceAtPoint(this, *Object_GetPosition(this), force);
        return true;
    }

    void* wheel0 = pointer32(this, off::WHEEL_PTR_BASE + 0 * off::WHEEL_SLOT_STRIDE);
    void* wheel1 = pointer32(this, off::WHEEL_PTR_BASE + 1 * off::WHEEL_SLOT_STRIDE);
    if (!Wheel_HasGroundContact(wheel0) && !Wheel_HasGroundContact(wheel1))
        return false;

    force = *Object_GetForward(wheel0);
    force.x *= info.force;
    force.y *= info.force;
    force.z *= info.force;
    if (field<float>(this, off::SPEED) < 0.0f) {
        force.x *= -1.0f;
        force.y *= -1.0f;
        force.z *= -1.0f;
    }
    Dynamic_AddForceAtPoint(this, *Object_GetPosition(this), force);
    return true;
}

namespace {
CarMissileIO* gMissileActionBackend = nullptr;

CD3DVECTOR missileSpawnPoint(void* car, bool center, bool mirrorX) {
    CD3DVECTOR point = field<CD3DVECTOR>(car, off::MISSILE_MOUNT);
    if (center)
        point.x = 0.0f;
    else if (mirrorX)
        point.x = static_cast<float>(static_cast<long double>(point.x) * -1.0L);

    TransformPoint(field<CD3DMATRIX>(car, off::PHYS_MATRIX), point);
    const CD3DVECTOR base = field<CD3DVECTOR>(car, off::OBJECT_BOUNDS_CENTER);
    point.x = static_cast<float>(static_cast<long double>(point.x) + base.x);
    point.y = static_cast<float>(static_cast<long double>(point.y) + base.y);
    point.z = static_cast<float>(static_cast<long double>(point.z) + base.z);
    return point;
}

bool canFireMissile(void* car) {

    const float condition = field<float>(car, off::CONDITION);
    return (std::isnan(condition) || condition > 0.0f) &&
           field<std::uint8_t>(car, off::MISSILE_COUNT) != 0;
}

std::string missileName(std::string_view carName, const char* suffix) {
    std::string out(carName);
    out += suffix;
    return out;
}
}

void Car_SetMissileIO(CarMissileIO* backend) {
    gMissileActionBackend = backend;
}

CarMissileIO* Car_GetMissileIO() {
    return gMissileActionBackend;
}

MissileFire Car_BuildMissileFire(void* car,
                                         std::string_view carName,
                                         bool missile1AlreadyExists,
                                         bool missile2AlreadyExists) {
    MissileFire plan{};

    if (!canFireMissile(car) ||
        missile1AlreadyExists || missile2AlreadyExists)
        return plan;

    plan.accepted = true;
    const std::uint8_t mode = field<std::uint8_t>(car, off::MISSILE_MODE);
    const CD3DVECTOR direction = field<CD3DVECTOR>(car, off::OBJECT_FORWARD);

    if (mode == 1) {
        plan.spawn[0] = {missileName(carName, "_missile1"),
                         missileSpawnPoint(car, true, false), direction};
        plan.spawnCount = 1;
        --field<std::uint8_t>(car, off::MISSILE_COUNT);
        return plan;
    }
    if (mode == 2) {
        plan.spawn[0] = {missileName(carName, "_missile1"),
                         missileSpawnPoint(car, false, false), direction};
        plan.spawnCount = 1;
        --field<std::uint8_t>(car, off::MISSILE_COUNT);
        if (field<std::uint8_t>(car, off::MISSILE_COUNT) == 0)
            return plan;
        plan.spawn[1] = {missileName(carName, "_missile2"),
                         missileSpawnPoint(car, false, true), direction};
        plan.spawnCount = 2;
        --field<std::uint8_t>(car, off::MISSILE_COUNT);
    }

    return plan;
}

bool CD3DCAROBJECT::FireMissile() {

    CarMissileIO* backend = Car_GetMissileIO();
    if (backend == nullptr)
        return false;
    if (!canFireMissile(this))
        return false;

    const std::string carName(backend->CarName(this));
    const std::string name1 = missileName(carName, "_missile1");
    if (backend->DynamicNameExists(name1))
        return false;
    const std::string name2 = missileName(carName, "_missile2");
    if (backend->DynamicNameExists(name2))
        return false;

    const std::uint8_t mode = field<std::uint8_t>(this, off::MISSILE_MODE);
    const CD3DVECTOR direction = field<CD3DVECTOR>(this, off::OBJECT_FORWARD);
    constexpr std::size_t kNativeMissileBytes = 0xFE0u;

    if (mode == 1u) {
        const CD3DVECTOR position = missileSpawnPoint(this, true, false);

        (void)backend->CreateMissile(kNativeMissileBytes, name1,
                                                   this, position, direction);
        --field<std::uint8_t>(this, off::MISSILE_COUNT);
        return true;
    }

    if (mode == 2u) {
        const CD3DVECTOR first = missileSpawnPoint(this, false, false);
        (void)backend->CreateMissile(kNativeMissileBytes, name1,
                                                   this, first, direction);
        --field<std::uint8_t>(this, off::MISSILE_COUNT);
        if (field<std::uint8_t>(this, off::MISSILE_COUNT) == 0u)
            return true;

        const CD3DVECTOR second = missileSpawnPoint(this, false, true);
        (void)backend->CreateMissile(kNativeMissileBytes, name2,
                                                   this, second, direction);
        --field<std::uint8_t>(this, off::MISSILE_COUNT);
        return true;
    }

    return true;
}

void CD3DCAROBJECT::ToggleLights() {

    if (!CarCanAct(this))
        return;

    auto& state = field<std::uint8_t>(this, off::LIGHTS_ACTIVE);
    state = static_cast<std::uint8_t>(1u - state);
}

void CD3DCAROBJECT::SetLights(std::uint8_t state) {

    assert(state == 0 || state == 1);
    if (!CarCanAct(this))
        return;
    field<std::uint8_t>(this, off::LIGHTS_ACTIVE) = state;
}

void CD3DCAROBJECT::ApplyConditionDamage(float amount) {

    float& condition = field<float>(this, off::CONDITION);
    condition -= amount;
    if (condition < 0.0f)
        condition = 0.0f;
}

void Car_ApplyTyrePhysics(void* car) {
    const CD3DVECTOR velocity = field<CD3DVECTOR>(car, off::LINEAR_VELOCITY);

    if (velocity.x < 0.2 && velocity.y < 0.2 && velocity.z < 0.2)
        return;

    CD3DVECTOR opposite = {-velocity.x, -velocity.y, -velocity.z};
    const long double invLength = 1.0L / std::sqrt(
        static_cast<long double>(opposite.y) * opposite.y +
        static_cast<long double>(opposite.x) * opposite.x +
        static_cast<long double>(opposite.z) * opposite.z);
    opposite.x = static_cast<float>(opposite.x * invLength);
    opposite.y = static_cast<float>(opposite.y * invLength);
    opposite.z = static_cast<float>(opposite.z * invLength);

    const float weight = static_cast<float>(
        static_cast<long double>(field<std::int16_t>(car, off::PHYS_MASS)) * 9.81);
    const CD3DVECTOR resistance = {
        opposite.x * weight, opposite.y * weight, opposite.z * weight
    };
    const float gripMinusFloor = field<float>(car, off::TYRE_GRIP) - 0.03f;

    for (unsigned i = 0; i < 4; ++i) {
        void* wheel = pointer32(car, off::WHEEL_PTR_BASE + i * off::WHEEL_SLOT_STRIDE);
        if (!Wheel_HasGroundContact(wheel))
            continue;

        CD3DVECTOR force = {resistance.x * 0.25f, resistance.y * 0.25f,
                      resistance.z * 0.25f};
        const CD3DVECTOR& forward = *Object_GetForward(wheel);
        float alignment = static_cast<float>(
            static_cast<long double>(opposite.y) * forward.y +
            static_cast<long double>(opposite.x) * forward.x +
            static_cast<long double>(opposite.z) * forward.z);
        if (alignment < 0.0f)
            alignment = -alignment;
        if (alignment > 1.0f)
            alignment = 1.0f;
        if (alignment > 0.9986)
            continue;

        const float scale = static_cast<float>(
            (1.0L - alignment) * 4.0f * gripMinusFloor + 0.03f);
        force.x *= scale;
        force.y *= scale;
        force.z *= scale;
        Dynamic_AddForceAtPoint(car, *Object_GetPosition(wheel), force);
    }
}

}
