#pragma once
#include "types.hpp"
#include <cstddef>

namespace flydemo {

class CD3DDYNAMICOBJECT {
    alignas(4) std::byte data_[0x0FC4]{};
public:
    void Update(float dt);
    void SetActivePhysics(bool state);
    void ResetOrientation();
    void ResetForces();
    void AddForce(const CD3DVECTOR& force);
    void AddForceAtPoint(const CD3DVECTOR& point, const CD3DVECTOR& force);
    void AddGravity();
    void AddAngularDamping();
    void ResetImpulses();
    void ResetVelocities();
    void RebuildPhysicsTransform();
    void SetActiveCollision(bool state);
    bool GetActiveCollision() const;
    void BuildGroundContacts();
    void SolveContacts();
    void ResetContacts();
};
static_assert(sizeof(CD3DDYNAMICOBJECT) == 0x0FC4);

void DynamicObject_Update(void* object, float dt);
void Dynamic_CopyOrientation(void* object, const void* transform);
void Dynamic_AddForceAtPoint(void* body, const CD3DVECTOR& point, const CD3DVECTOR& force);

void Dynamic_Integrate(void* object, float dt);

class WorldQuery {
public:
    virtual ~WorldQuery() = default;
    virtual std::int16_t FindContainingSector(const CD3DVECTOR& minimum,
                                               const CD3DVECTOR& maximum) = 0;
};

void Object_UpdateBase(void* object, float dt);
void Object_UpdateBase(void* object, float dt, WorldQuery& world);
void Object_RebuildGeometry(void* object);
void Object_RecomputeBounds(void* object);

}
