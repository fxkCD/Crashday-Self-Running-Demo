#include <cassert>
#include <cstdint>
#include "types.hpp"
#include "offsets.hpp"
#include "memory.hpp"
#include "transform.hpp"
#include "dynamic_object.hpp"
#include "wheel.hpp"

namespace flydemo {
using flydemo::mem::field;

class WHEELOBJECT {
public:
    void Update(float dt);
    void SetRollRate(float rate);
    void SetSteeringAngle(float angle);
    bool HasGroundContact() const;
    float GetRollRate() const;
    float GetSteeringAngle() const;
};

static void Wheel_InitState(WHEELOBJECT* self, void* car,
                                               std::uint8_t rotate180) {
    assert(rotate180 == 0 || rotate180 == 1);

    field<float>(self, flydemo::off::WHEEL_ROLL_ANGLE) = 0.0f;
    field<float>(self, flydemo::off::WHEEL_ROLL_RATE) = 0.0f;
    field<float>(self, flydemo::off::WHEEL_STEER_ANGLE) = 0.0f;
    flydemo::mem::store_pointer32(self, flydemo::off::WHEEL_PARENT_CAR_OBJ, car);

}

void WHEELOBJECT::SetRollRate(float rate) {

    field<float>(this, flydemo::off::WHEEL_ROLL_RATE) = rate;
}

void WHEELOBJECT::SetSteeringAngle(float angle) {

    assert(angle > -64.0f && angle < 64.0f);
    field<float>(this, flydemo::off::WHEEL_STEER_ANGLE) = angle;
}

bool WHEELOBJECT::HasGroundContact() const {

    return field<std::uint8_t>(this,
                               flydemo::off::WHEEL_GROUND_CONTACT) != 0;
}

float WHEELOBJECT::GetRollRate() const {
    return field<float>(this, flydemo::off::WHEEL_ROLL_RATE);
}

float WHEELOBJECT::GetSteeringAngle() const {
    return field<float>(this, flydemo::off::WHEEL_STEER_ANGLE);
}

static void BeginWheelUpdate(WHEELOBJECT* self, float dt) {
    float& rollAngle = field<float>(self, flydemo::off::WHEEL_ROLL_ANGLE);
    rollAngle = static_cast<float>(
        static_cast<long double>(field<float>(self, flydemo::off::WHEEL_ROLL_RATE))
        * dt + rollAngle);

    std::int32_t bits;
    std::memcpy(&bits, &rollAngle, sizeof(bits));
    if (bits > 0x46c80000)
        rollAngle = static_cast<float>(static_cast<long double>(rollAngle) - 25600.0f);
}

static void FinishWheelUpdate(WHEELOBJECT* self) {
    const float centerY =
        field<CD3DVECTOR>(self, flydemo::off::OBJECT_BOUNDS_CENTER).y;
    const float height =
        field<float>(self, flydemo::off::OBJECT_SIZE_Y);

    const long double lowerBoundY = static_cast<long double>(centerY)
                                  - static_cast<long double>(height) * 0.5;
    field<std::uint8_t>(self, flydemo::off::WHEEL_GROUND_CONTACT) =
        static_cast<std::uint8_t>(!(lowerBoundY > 0.2));

}

void WHEELOBJECT::Update(float dt) {
    using namespace flydemo;
    BeginWheelUpdate(this, dt);

    CD3DMATRIX rolled{}, roll{}, steer{}, fixed{};
    SetIdentity(rolled);
    MakeXRotation(roll, field<float>(this, off::WHEEL_ROLL_ANGLE));
    MakeYRotation(steer, field<float>(this, off::WHEEL_STEER_ANGLE));
    MakeXRotation(fixed, 32.0f);
    CD3DMATRIX& body = field<CD3DMATRIX>(this, off::PHYS_MATRIX);
    CD3DMATRIX& a = field<CD3DMATRIX>(this, off::WHEEL_TRANSFORM_A);
    CD3DMATRIX& b = field<CD3DMATRIX>(this, off::WHEEL_TRANSFORM_B);

    SetIdentity(a);
    ComposeTransform(a, steer);
    ComposeTransform(a, body);
    SetIdentity(b);
    ComposeTransform(b, fixed);
    ComposeTransform(b, steer);
    ComposeTransform(b, body);
    ComposeTransform(rolled, roll);
    ComposeTransform(rolled, steer);
    ComposeTransform(rolled, body);
    CopyTransform(body, rolled);

    DynamicObject_Update(this, dt);
    FinishWheelUpdate(this);
}

void Wheel_Update(void* wheel, float dt) {
    reinterpret_cast<WHEELOBJECT*>(wheel)->Update(dt);
}

bool Wheel_HasGroundContact(void* wheel) {
    return reinterpret_cast<const WHEELOBJECT*>(wheel)->HasGroundContact();
}
void Wheel_SetRollRate(void* wheel, float rate) {
    reinterpret_cast<WHEELOBJECT*>(wheel)->SetRollRate(rate);
}
void Wheel_SetSteeringAngle(void* wheel, float angle) {
    reinterpret_cast<WHEELOBJECT*>(wheel)->SetSteeringAngle(angle);
}

}
