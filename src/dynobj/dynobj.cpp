#include <cstdint>
#include "dynamic_object.hpp"
#include "memory.hpp"
#include "offsets.hpp"
#include "transform.hpp"

namespace flydemo {
using flydemo::mem::field;
using flydemo::mem::pointer32;

namespace {
constexpr unsigned kVertexStride = 0x20;
constexpr unsigned kMaterialStride = 0x38;
constexpr unsigned kBoundsStride = 0x20;

CD3DVECTOR transformBodyPoint(CD3DMATRIX& orientation, CD3DVECTOR point, const CD3DVECTOR& position) {
    TransformPoint(orientation, point);
    point.x = static_cast<float>(static_cast<long double>(point.x) + position.x);
    point.y = static_cast<float>(static_cast<long double>(point.y) + position.y);
    point.z = static_cast<float>(static_cast<long double>(point.z) + position.z);
    return point;
}
}

void CD3DDYNAMICOBJECT::RebuildPhysicsTransform() {

    if (field<std::uint8_t>(this, off::OBJECT_STATE_0090) == 0)
        return;

    CD3DVECTOR& position = field<CD3DVECTOR>(this, off::PHYS_POSITION);
    CD3DMATRIX& pending = field<CD3DMATRIX>(this, off::OBJECT_PENDING_TRANSFORM);
    CD3DMATRIX& orientation = field<CD3DMATRIX>(this, off::PHYS_MATRIX);

    TransformPoint(pending, position);
    ClearTranslation(pending);
    ComposeTransform(orientation, pending);

    const CD3DVECTOR bodyPosition = position;
    const std::int16_t vertexCount = field<std::int16_t>(this, off::OBJECT_VERTEX_COUNT);
    auto* localVertices = pointer32<std::uint8_t>(this, off::LOCAL_VERTEX_ARRAY);
    auto* worldVertices = pointer32<std::uint8_t>(this, off::OBJECT_VERTEX_ARRAY);
    for (int i = 0; i < vertexCount; ++i) {
        const unsigned offset = static_cast<unsigned>(i) * kVertexStride;
        CD3DVECTOR point = field<CD3DVECTOR>(localVertices, offset);
        point = transformBodyPoint(orientation, point, bodyPosition);
        field<CD3DVECTOR>(worldVertices, offset) = point;
    }

    const std::int16_t materialCount = field<std::int16_t>(this, off::OBJECT_MATERIAL_COUNT);
    auto* localMaterials = pointer32<std::uint8_t>(this, off::LOCAL_MATERIAL_ARRAY);
    auto* worldMaterials = pointer32<std::uint8_t>(this, off::OBJECT_MATERIAL_ARRAY);
    for (int i = 0; i < materialCount; ++i) {
        const unsigned offset = static_cast<unsigned>(i) * kMaterialStride;
        CD3DVECTOR point = field<CD3DVECTOR>(localMaterials, offset);
        point = transformBodyPoint(orientation, point, bodyPosition);
        field<CD3DVECTOR>(worldMaterials, offset) = point;
    }

    auto* localBounds = pointer32<std::uint8_t>(this, off::LOCAL_BOUNDS_ARRAY);
    for (unsigned i = 0; i < 8; ++i) {
        CD3DVECTOR point = field<CD3DVECTOR>(localBounds, i * kBoundsStride);
        point = transformBodyPoint(orientation, point, bodyPosition);
        field<CD3DVECTOR>(this, off::OBJECT_BOUNDS_POINTS + i * kBoundsStride) = point;
    }

    const CD3DVECTOR p0 = field<CD3DVECTOR>(this, off::OBJECT_BOUNDS_POINTS);
    const CD3DVECTOR p6 = field<CD3DVECTOR>(this, off::OBJECT_BOUNDS_POINTS + 6 * kBoundsStride);
    field<CD3DVECTOR>(this, off::OBJECT_BOUNDS_CENTER) = {
        static_cast<float>((static_cast<long double>(p0.x) + p6.x) * 0.5L),
        static_cast<float>((static_cast<long double>(p0.y) + p6.y) * 0.5L),
        static_cast<float>((static_cast<long double>(p0.z) + p6.z) * 0.5L),
    };

    field<CD3DVECTOR>(this, off::OBJECT_FORWARD) = {0, 0, 1};
    TransformPoint(orientation, field<CD3DVECTOR>(this, off::OBJECT_FORWARD));
    field<CD3DVECTOR>(this, off::OBJECT_UP) = {0, 1, 0};
    TransformPoint(orientation, field<CD3DVECTOR>(this, off::OBJECT_UP));
    field<CD3DVECTOR>(this, off::OBJECT_RIGHT) = {1, 0, 0};
    TransformPoint(orientation, field<CD3DVECTOR>(this, off::OBJECT_RIGHT));

    Object_RebuildGeometry(this);

    SetIdentity(pending);
}

void CD3DDYNAMICOBJECT::Update(float dt) {

    if (field<std::uint8_t>(this, off::GRAVITY_DAMPING_ACTIVE) == 1) {
        AddGravity();
        AddAngularDamping();
    }
    Dynamic_Integrate(this, dt);
    Object_UpdateBase(this, dt);

    if (!(field<CD3DVECTOR>(this, off::OBJECT_BOUNDS_CENTER).y >= -10.0f))
        SetActivePhysics(false);
}

void DynamicObject_Update(void* object, float dt) {
    reinterpret_cast<CD3DDYNAMICOBJECT*>(object)->Update(dt);
}

}
