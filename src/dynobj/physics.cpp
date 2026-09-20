#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstring>
#include "types.hpp"
#include "offsets.hpp"
#include "memory.hpp"
#include "dynamic_object.hpp"
#include "transform.hpp"

namespace flydemo {
using namespace flydemo;
using flydemo::mem::field;
using Wide = long double;

static inline CD3DVECTOR add(CD3DVECTOR a, CD3DVECTOR b) { return {a.x+b.x,a.y+b.y,a.z+b.z}; }
static inline CD3DVECTOR sub(CD3DVECTOR a, CD3DVECTOR b) { return {a.x-b.x,a.y-b.y,a.z-b.z}; }
static inline CD3DVECTOR cross(CD3DVECTOR a, CD3DVECTOR b) {
    return {a.y*b.z-a.z*b.y, a.z*b.x-a.x*b.z, a.x*b.y-a.y*b.x};
}

void CD3DDYNAMICOBJECT::SetActivePhysics(bool state) {

    assert(state == 0 || state == 1);
    field<std::uint8_t>(this, off::PHYS_ACTIVE) = static_cast<std::uint8_t>(state);

    if (!state) {
        ResetOrientation();
        ResetForces();
        ResetImpulses();
        ResetVelocities();
    }

}

void CD3DDYNAMICOBJECT::ResetOrientation() {
    SetIdentity(field<CD3DMATRIX>(this, off::PHYS_MATRIX));
}
void CD3DDYNAMICOBJECT::ResetForces() {
    field<CD3DVECTOR>(this, off::FORCE_ACCUM) = {0, 0, 0};
    field<CD3DVECTOR>(this, off::TORQUE_ACCUM) = {0, 0, 0};
}
void CD3DDYNAMICOBJECT::ResetImpulses() {
    field<CD3DVECTOR>(this, off::LINEAR_IMPULSE) = {0, 0, 0};
    field<CD3DVECTOR>(this, off::ANGULAR_IMPULSE) = {0, 0, 0};
}
void CD3DDYNAMICOBJECT::ResetVelocities() {
    field<CD3DVECTOR>(this, off::LINEAR_VELOCITY) = {0, 0, 0};
    field<CD3DVECTOR>(this, off::ANGULAR_VELOCITY) = {0, 0, 0};
}

void CD3DDYNAMICOBJECT::AddGravity() {

    const auto mass = field<std::int16_t>(this, off::PHYS_MASS);
    float& y = field<CD3DVECTOR>(this, off::FORCE_ACCUM).y;
    y = static_cast<float>(static_cast<Wide>(y)
                            - static_cast<Wide>(mass) * Wide(9.81));
}
void CD3DDYNAMICOBJECT::AddAngularDamping() {

    const auto mass = field<std::int16_t>(this, off::PHYS_MASS);
    const CD3DVECTOR v = field<CD3DVECTOR>(this, off::ANGULAR_VELOCITY);
    CD3DVECTOR& t = field<CD3DVECTOR>(this, off::TORQUE_ACCUM);
    const auto component = [mass](float torque, float velocity) {
        return static_cast<float>((static_cast<Wide>(velocity) * Wide(-0.3))
                                  * mass + torque);
    };
    t = {component(t.x, v.x), component(t.y, v.y), component(t.z, v.z)};
}

void Dynamic_CopyOrientation(void* object, const void* transform) {

    CD3DMATRIX source;
    std::memcpy(&source, transform, sizeof(source));
    CopyTransform(field<CD3DMATRIX>(object, off::PHYS_MATRIX), source);
}

void CD3DDYNAMICOBJECT::AddForce(const CD3DVECTOR& force) {

    if (field<std::uint8_t>(this, off::FORCE_BLOCKED_06A7) == 1 ||
        field<std::uint8_t>(this, off::PHYS_ACTIVE) == 0)
        return;
    CD3DVECTOR& accum = field<CD3DVECTOR>(this, off::FORCE_ACCUM);
    accum = add(accum, force);
}

void CD3DDYNAMICOBJECT::AddForceAtPoint(const CD3DVECTOR& point, const CD3DVECTOR& force) {

    if (field<std::uint8_t>(this, off::FORCE_BLOCKED_06A7) == 1 ||
        field<std::uint8_t>(this, off::PHYS_ACTIVE) == 0)
        return;

    CD3DVECTOR& forceAccum = field<CD3DVECTOR>(this, off::FORCE_ACCUM);
    forceAccum = add(forceAccum, force);

    const CD3DVECTOR com = field<CD3DVECTOR>(this, off::PHYS_POSITION);
    const CD3DVECTOR lever = sub(com, point);
    const CD3DVECTOR computedTorque = cross(lever, force);
    (void)computedTorque;
}

static void CalculateAccelerations(CD3DDYNAMICOBJECT* self) {
    const auto mass = field<std::int16_t>(self, off::PHYS_MASS);
    const CD3DVECTOR f = field<CD3DVECTOR>(self, off::FORCE_ACCUM);
    const auto divide = [mass](float value) {
        return static_cast<float>(static_cast<Wide>(value) / static_cast<Wide>(mass));
    };
    field<CD3DVECTOR>(self, off::LINEAR_ACCEL) = {divide(f.x), divide(f.y), divide(f.z)};

    field<CD3DVECTOR>(self, off::ANGULAR_ACCEL) = {0.0f, 0.0f, 0.0f};
}

static void ApplyImpulses(CD3DDYNAMICOBJECT* self) {
    const auto mass = field<std::int16_t>(self, off::PHYS_MASS);
    const CD3DVECTOR linearImpulse = field<CD3DVECTOR>(self, off::LINEAR_IMPULSE);
    CD3DVECTOR& linearVelocity = field<CD3DVECTOR>(self, off::LINEAR_VELOCITY);
    const auto addLinear = [mass](float velocity, float impulse) {
        return static_cast<float>(static_cast<Wide>(impulse) / static_cast<Wide>(mass)
                                  + static_cast<Wide>(velocity));
    };
    linearVelocity = {
        addLinear(linearVelocity.x, linearImpulse.x),
        addLinear(linearVelocity.y, linearImpulse.y),
        addLinear(linearVelocity.z, linearImpulse.z)
    };

    CD3DMATRIX& orientation = field<CD3DMATRIX>(self, off::PHYS_MATRIX);
    const CD3DVECTOR angularImpulse = field<CD3DVECTOR>(self, off::ANGULAR_IMPULSE);
    const CD3DVECTOR principal = field<CD3DVECTOR>(self, off::PRINCIPAL_INERTIA);

    const auto component = [&](unsigned c, float divisor) {
        const Wide x = static_cast<Wide>(angularImpulse.x) * orientation.m[c];
        const Wide y = static_cast<Wide>(angularImpulse.y) * orientation.m[4 + c];
        const Wide z = static_cast<Wide>(angularImpulse.z) * orientation.m[8 + c];
        return static_cast<float>(((x + y) + z) / static_cast<Wide>(divisor));
    };
    CD3DVECTOR angularDelta = {
        component(0, principal.x),
        component(1, principal.y),
        component(2, principal.z)
    };
    TransformPoint(orientation, angularDelta);

    CD3DVECTOR& angularVelocity = field<CD3DVECTOR>(self, off::ANGULAR_VELOCITY);
    angularVelocity = add(angularVelocity, angularDelta);
}

static void BuildAngularMatrix(CD3DMATRIX& t, const CD3DVECTOR& w) {

    SetIdentity(t);
    t.m[0] = 0.0f;  t.m[1] = -w.z;  t.m[2] =  w.y;
    t.m[4] = w.z;   t.m[5] = 0.0f;  t.m[6] = -w.x;
    t.m[8] = -w.y;  t.m[9] = w.x;   t.m[10] = 0.0f;
}

static void ScaleTransform(CD3DMATRIX& t, float scalar) {

    for (float& value : t.m)
        value = static_cast<float>(static_cast<Wide>(value) * scalar);
}

static void AddTransform(CD3DMATRIX& dst, const CD3DMATRIX& rhs) {

    for (unsigned i = 0; i < 16; ++i)
        dst.m[i] = static_cast<float>(static_cast<Wide>(dst.m[i]) + rhs.m[i]);
}

static CD3DVECTOR NormalizeUnchecked(CD3DVECTOR v) {

    const Wide length = std::sqrt(static_cast<Wide>(v.x) * v.x
                                + static_cast<Wide>(v.y) * v.y
                                + static_cast<Wide>(v.z) * v.z);
    const Wide inv = Wide(1) / length;
    return {
        static_cast<float>(static_cast<Wide>(v.x) * inv),
        static_cast<float>(static_cast<Wide>(v.y) * inv),
        static_cast<float>(static_cast<Wide>(v.z) * inv)
    };
}

static void Orthonormalize(CD3DMATRIX& t) {

    CD3DVECTOR c0 = {t.m[0], t.m[4], t.m[8]};
    const CD3DVECTOR originalC1 = {t.m[1], t.m[5], t.m[9]};
    c0 = NormalizeUnchecked(c0);
    CD3DVECTOR c2 = NormalizeUnchecked(cross(c0, originalC1));
    CD3DVECTOR c1 = NormalizeUnchecked(cross(c2, c0));

    t.m[0] = c0.x; t.m[1] = c1.x; t.m[2] = c2.x; t.m[3] = 0.0f;
    t.m[4] = c0.y; t.m[5] = c1.y; t.m[6] = c2.y; t.m[7] = 0.0f;
    t.m[8] = c0.z; t.m[9] = c1.z; t.m[10]= c2.z; t.m[11]= 0.0f;
    t.m[12]=0.0f; t.m[13]=0.0f; t.m[14]=0.0f; t.m[15]=1.0f;
}

static float& matrix3x4(void* object, unsigned base, unsigned row, unsigned col) {
    return field<float>(object, base + row * 0x10u + col * 4u);
}

static void RefreshWorldInertia(CD3DDYNAMICOBJECT* self) {

    const CD3DMATRIX& r = field<CD3DMATRIX>(self, off::PHYS_MATRIX);
    const CD3DVECTOR p = field<CD3DVECTOR>(self, off::PRINCIPAL_INERTIA);

    Wide a[3][3]{};
    for (unsigned row = 0; row < 3; ++row) {
        for (unsigned col = 0; col < 3; ++col) {

            const Wide x = static_cast<Wide>(r.m[row])     * r.m[col]     * p.x;
            const Wide y = static_cast<Wide>(r.m[4+row])   * r.m[4+col]   * p.y;
            const Wide z = static_cast<Wide>(r.m[8+row])   * r.m[8+col]   * p.z;
            a[row][col] = (x + y) + z;
            matrix3x4(self, off::WORLD_INERTIA, row, col) = static_cast<float>(a[row][col]);
        }
    }

    const Wide c00 = a[1][1]*a[2][2] - a[1][2]*a[2][1];
    const Wide c01 = a[1][2]*a[2][0] - a[1][0]*a[2][2];
    const Wide c02 = a[1][0]*a[2][1] - a[1][1]*a[2][0];
    const Wide c10 = a[0][2]*a[2][1] - a[0][1]*a[2][2];
    const Wide c11 = a[0][0]*a[2][2] - a[0][2]*a[2][0];
    const Wide c12 = a[0][1]*a[2][0] - a[0][0]*a[2][1];
    const Wide c20 = a[0][1]*a[1][2] - a[0][2]*a[1][1];
    const Wide c21 = a[0][2]*a[1][0] - a[0][0]*a[1][2];
    const Wide c22 = a[0][0]*a[1][1] - a[0][1]*a[1][0];
    const Wide det = a[0][0]*c00 + a[0][1]*c01 + a[0][2]*c02;
    const Wide invDet = Wide(1) / det;

    const Wide inv[3][3] = {
        {c00*invDet, c10*invDet, c20*invDet},
        {c01*invDet, c11*invDet, c21*invDet},
        {c02*invDet, c12*invDet, c22*invDet}
    };
    for (unsigned row = 0; row < 3; ++row)
        for (unsigned col = 0; col < 3; ++col)
            matrix3x4(self, off::WORLD_INV_INERTIA, row, col) = static_cast<float>(inv[row][col]);
}

static void IntegrateOrientation(CD3DDYNAMICOBJECT* self, float dt) {

    CD3DMATRIX delta{};
    BuildAngularMatrix(delta, field<CD3DVECTOR>(self, off::ANGULAR_VELOCITY));
    ClearTranslation(delta);
    ScaleTransform(delta, dt);
    ClearTranslation(delta);

    CD3DMATRIX& orientation = field<CD3DMATRIX>(self, off::PHYS_MATRIX);
    ComposeTransform(delta, orientation);
    ClearTranslation(delta);
    AddTransform(delta, orientation);
    ClearTranslation(delta);
    CopyTransform(orientation, delta);
    Orthonormalize(orientation);
    ClearTranslation(orientation);
}

void Dynamic_Integrate(void* object, float dt) {

    auto* self = reinterpret_cast<CD3DDYNAMICOBJECT*>(object);

    if (field<std::uint8_t>(self, off::PHYS_ACTIVE) == 0) {
        self->ResetForces();
        self->ResetImpulses();
        return;
    }

    self->AddAngularDamping();

    if (field<std::uint8_t>(self, off::COLLISION_ACTIVE) == 1) {

        self->BuildGroundContacts();
        self->SolveContacts();
        self->ResetContacts();
    }

    CalculateAccelerations(self);
    ApplyImpulses(self);

    CD3DVECTOR& linearVelocity = field<CD3DVECTOR>(self, off::LINEAR_VELOCITY);
    const CD3DVECTOR linearAccel = field<CD3DVECTOR>(self, off::LINEAR_ACCEL);
    CD3DVECTOR& angularVelocity = field<CD3DVECTOR>(self, off::ANGULAR_VELOCITY);
    const CD3DVECTOR angularAccel = field<CD3DVECTOR>(self, off::ANGULAR_ACCEL);
    const auto advance = [dt](float value, float derivative) {
        return static_cast<float>(static_cast<Wide>(derivative) * dt
                                  + static_cast<Wide>(value));
    };
    linearVelocity = {
        advance(linearVelocity.x, linearAccel.x),
        advance(linearVelocity.y, linearAccel.y),
        advance(linearVelocity.z, linearAccel.z)
    };
    angularVelocity = {
        advance(angularVelocity.x, angularAccel.x),
        advance(angularVelocity.y, angularAccel.y),
        advance(angularVelocity.z, angularAccel.z)
    };

    CD3DVECTOR& position = field<CD3DVECTOR>(self, off::PHYS_POSITION);
    position = {
        advance(position.x, linearVelocity.x),
        advance(position.y, linearVelocity.y),
        advance(position.z, linearVelocity.z)
    };

    IntegrateOrientation(self, dt);
    RefreshWorldInertia(self);
    self->ResetForces();
    self->ResetImpulses();
    field<std::uint8_t>(self, off::OBJECT_STATE_0090) = 1;
}

void Dynamic_AddForceAtPoint(void* body, const CD3DVECTOR& point, const CD3DVECTOR& force) {
    reinterpret_cast<CD3DDYNAMICOBJECT*>(body)->AddForceAtPoint(point, force);
}

}
