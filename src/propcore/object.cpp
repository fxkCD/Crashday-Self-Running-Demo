#include "dynamic_object.hpp"
#include "memory.hpp"
#include "offsets.hpp"
#include "runtime.hpp"
#include "transform.hpp"

#include <cmath>
#include <cstdint>
#include <cstring>
#include <initializer_list>

namespace flydemo {
using flydemo::mem::field;
using flydemo::mem::pointer32;

namespace {
constexpr unsigned kVertexStride = 0x20;
constexpr unsigned kFaceStride = 0x30;
constexpr unsigned kGroupStride = 0x20;
constexpr unsigned kVertexNormal = 0x0c;
constexpr unsigned kFaceNormal = 0x24;

std::int16_t count16(const void* object, unsigned offset) {
    return field<std::int16_t>(object, offset);
}

CD3DVECTOR& recordVec(void* base, std::size_t byteOffset) {
    return field<CD3DVECTOR>(base, byteOffset);
}
const CD3DVECTOR& recordVec(const void* base, std::size_t byteOffset) {
    return field<CD3DVECTOR>(base, byteOffset);
}

void normalizeUnchecked(CD3DVECTOR& v) {

    const long double length = std::sqrt(
        static_cast<long double>(v.x) * v.x +
        static_cast<long double>(v.y) * v.y +
        static_cast<long double>(v.z) * v.z);
    const long double inv = 1.0L / length;
    v.x = static_cast<float>(static_cast<long double>(v.x) * inv);
    v.y = static_cast<float>(static_cast<long double>(v.y) * inv);
    v.z = static_cast<float>(static_cast<long double>(v.z) * inv);
}

CD3DVECTOR cross(const CD3DVECTOR& a, const CD3DVECTOR& b) {
    return {
        static_cast<float>(static_cast<long double>(a.y) * b.z -
                           static_cast<long double>(a.z) * b.y),
        static_cast<float>(static_cast<long double>(a.z) * b.x -
                           static_cast<long double>(a.x) * b.z),
        static_cast<float>(static_cast<long double>(a.x) * b.y -
                           static_cast<long double>(a.y) * b.x),
    };
}

int wrappedCountSum3(const void* group, unsigned a, unsigned b, unsigned c) {

    std::uint16_t value = field<std::uint16_t>(group, a);
    value = static_cast<std::uint16_t>(value + field<std::uint16_t>(group, b));
    value = static_cast<std::uint16_t>(value + field<std::uint16_t>(group, c));
    return static_cast<int>(static_cast<std::int16_t>(value));
}

void RebuildGeometryNormals(void* object) {

    const std::int16_t vertexCount = count16(object, off::OBJECT_VERTEX_COUNT);
    const std::int16_t faceCount = count16(object, off::OBJECT_FACE_COUNT);
    auto* vertices = pointer32<std::uint8_t>(object, off::OBJECT_VERTEX_ARRAY);
    auto* faces = pointer32<std::uint8_t>(object, off::OBJECT_FACE_ARRAY);

    if (faceCount > 0 && vertices != nullptr && faces != nullptr) {
        for (int i = 0; i < faceCount; ++i) {
            auto* face = faces + static_cast<unsigned>(i) * kFaceStride;
            const std::int16_t i0 = field<std::int16_t>(face, 0x00);
            const std::int16_t i1 = field<std::int16_t>(face, 0x02);
            const std::int16_t i2 = field<std::int16_t>(face, 0x04);
            const CD3DVECTOR& v0 = recordVec(vertices, static_cast<unsigned>(i0) * kVertexStride);
            const CD3DVECTOR& v1 = recordVec(vertices, static_cast<unsigned>(i1) * kVertexStride);
            const CD3DVECTOR& v2 = recordVec(vertices, static_cast<unsigned>(i2) * kVertexStride);
            const CD3DVECTOR a{v0.x - v1.x, v0.y - v1.y, v0.z - v1.z};
            const CD3DVECTOR b{v2.x - v1.x, v2.y - v1.y, v2.z - v1.z};
            CD3DVECTOR& normal = field<CD3DVECTOR>(face, kFaceNormal);
            normal = cross(a, b);
            normalizeUnchecked(normal);
        }
    }

    if (vertexCount > 0 && vertices != nullptr) {
        for (int i = 0; i < vertexCount; ++i)
            recordVec(vertices, static_cast<unsigned>(i) * kVertexStride + kVertexNormal) = {0, 0, 0};
    }

    const std::uint8_t groupCount = field<std::uint8_t>(object, off::OBJECT_GROUP_COUNT);
    auto* groups = pointer32<std::uint8_t>(object, off::OBJECT_GROUP_ARRAY);
    if (groupCount != 0 && groups != nullptr && faces != nullptr && vertices != nullptr) {
        for (unsigned g = 0; g < groupCount; ++g) {
            auto* group = groups + g * kGroupStride;
            const int first = wrappedCountSum3(group, 0x12, 0x14, 0x16);
            const int count = wrappedCountSum3(group, 0x18, 0x1a, 0x1c);
            for (int n = 0; n < count; ++n) {
                auto* face = faces + static_cast<unsigned>(first + n) * kFaceStride;
                const CD3DVECTOR normal = field<CD3DVECTOR>(face, kFaceNormal);
                for (unsigned slot : {0u, 2u, 4u}) {
                    const std::int16_t index = field<std::int16_t>(face, slot);
                    CD3DVECTOR& dst = recordVec(vertices,
                        static_cast<unsigned>(index) * kVertexStride + kVertexNormal);
                    dst.x = static_cast<float>(static_cast<long double>(dst.x) + normal.x);
                    dst.y = static_cast<float>(static_cast<long double>(dst.y) + normal.y);
                    dst.z = static_cast<float>(static_cast<long double>(dst.z) + normal.z);
                }
            }
        }
    }

    if (vertexCount > 0 && vertices != nullptr) {
        for (int i = 0; i < vertexCount; ++i)
            normalizeUnchecked(recordVec(vertices,
                static_cast<unsigned>(i) * kVertexStride + kVertexNormal));
    }

    const std::int16_t lightCount = count16(object, off::OBJECT_MATERIAL_COUNT);
    auto* lights = pointer32<std::uint8_t>(object, off::OBJECT_MATERIAL_ARRAY);
    if (lightCount > 0 && lights != nullptr) {
        for (int i = 0; i < lightCount; ++i) {
            auto* light = lights + static_cast<unsigned>(i) * 0x38;
            const CD3DVECTOR& position = field<CD3DVECTOR>(light, 0x00);
            const long double length = std::sqrt(
                static_cast<long double>(position.x) * position.x +
                static_cast<long double>(position.y) * position.y +
                static_cast<long double>(position.z) * position.z);
            field<float>(light, 0x28) = static_cast<float>(length);
        }
    }
}

struct RawBoundsPlane {
    float a = 0.0f, b = 0.0f, c = 0.0f, d = 0.0f;
};
static_assert(sizeof(RawBoundsPlane) == 0x10);

RawBoundsPlane planeFromPoints(const CD3DVECTOR& p0, const CD3DVECTOR& p1,
                                              const CD3DVECTOR& p2) {

    const float ux = static_cast<float>(p1.x - p0.x);
    const float uy = static_cast<float>(p1.y - p0.y);
    const float uz = static_cast<float>(p1.z - p0.z);
    const float vx = static_cast<float>(p2.x - p0.x);
    const float vy = static_cast<float>(p2.y - p0.y);
    const float vz = static_cast<float>(p2.z - p0.z);
    long double nx = static_cast<long double>(uy) * vz - static_cast<long double>(uz) * vy;
    long double ny = static_cast<long double>(uz) * vx - static_cast<long double>(ux) * vz;
    long double nz = static_cast<long double>(ux) * vy - static_cast<long double>(uy) * vx;
    const long double length = std::sqrt(nx*nx + ny*ny + nz*nz);
    const long double inv = 1.0L / length;
    RawBoundsPlane out;
    out.a = static_cast<float>(nx * inv);
    out.b = static_cast<float>(ny * inv);
    out.c = static_cast<float>(nz * inv);
    out.d = static_cast<float>(static_cast<long double>(out.a) * p0.x +
                               static_cast<long double>(out.b) * p0.y +
                               static_cast<long double>(out.c) * p0.z);
    return out;
}

void orientPlane(RawBoundsPlane& plane, const CD3DVECTOR& center) {

    const long double distance =
        static_cast<long double>(plane.a) * center.x +
        static_cast<long double>(plane.b) * center.y +
        static_cast<long double>(plane.c) * center.z - plane.d;
    if (!std::isnan(static_cast<double>(distance)) && distance > 0.001L) {
        plane.a = -plane.a; plane.b = -plane.b;
        plane.c = -plane.c; plane.d = -plane.d;
    }
}

void RecomputeBoundsComplete(void* object) {

    const CD3DVECTOR center = field<CD3DVECTOR>(object, off::OBJECT_BOUNDS_CENTER);
    CD3DVECTOR minimum = center;
    CD3DVECTOR maximum = center;

    for (unsigned i = 0; i < 8; ++i) {
        const CD3DVECTOR p = field<CD3DVECTOR>(object, off::OBJECT_BOUNDS_POINTS + i * 0x20);
        if (!(p.x >= minimum.x)) minimum.x = p.x;
        if (!(p.y >= minimum.y)) minimum.y = p.y;
        if (!(p.z >= minimum.z)) minimum.z = p.z;
        if (p.x > maximum.x) maximum.x = p.x;
        if (p.y > maximum.y) maximum.y = p.y;
        if (p.z > maximum.z) maximum.z = p.z;
    }
    field<CD3DVECTOR>(object, off::OBJECT_BOUNDS_MIN) = minimum;
    field<CD3DVECTOR>(object, off::OBJECT_BOUNDS_MAX) = maximum;

    const long double sx = field<float>(object, off::OBJECT_SIZE_X);
    const long double sy = field<float>(object, off::OBJECT_SIZE_Y);
    const long double sz = field<float>(object, off::OBJECT_SIZE_Z);
    field<float>(object, off::OBJECT_BOUND_RADIUS) = static_cast<float>(
        std::sqrt((sx * sx + sy * sy + sz * sz) * 0.25L));

    const CD3DVECTOR& boundsCenter = field<CD3DVECTOR>(object, off::OBJECT_BOUNDS_CENTER);
    for (unsigned planeIndex = 0; planeIndex < 6; ++planeIndex) {
        const auto* face = reinterpret_cast<const std::uint8_t*>(object) +
                           off::OBJECT_BOUNDS_FACES + planeIndex * 0x60;
        const std::int16_t i0 = field<std::int16_t>(face, 0x00);
        const std::int16_t i1 = field<std::int16_t>(face, 0x02);
        const std::int16_t i2 = field<std::int16_t>(face, 0x04);
        const CD3DVECTOR& p0 = field<CD3DVECTOR>(object, off::OBJECT_BOUNDS_POINTS +
                                     static_cast<unsigned>(i0) * 0x20);
        const CD3DVECTOR& p1 = field<CD3DVECTOR>(object, off::OBJECT_BOUNDS_POINTS +
                                     static_cast<unsigned>(i1) * 0x20);
        const CD3DVECTOR& p2 = field<CD3DVECTOR>(object, off::OBJECT_BOUNDS_POINTS +
                                     static_cast<unsigned>(i2) * 0x20);
        RawBoundsPlane plane = planeFromPoints(p0, p1, p2);
        orientPlane(plane, boundsCenter);
        field<RawBoundsPlane>(object, off::OBJECT_BOUNDS_PLANES + planeIndex * 0x10) = plane;
    }
}

void finalizeBaseUpdate(void* object, WorldQuery* world) {
    Object_RecomputeBounds(object);
    if (world != nullptr) {
        const CD3DVECTOR& minimum = field<CD3DVECTOR>(object, off::OBJECT_BOUNDS_MIN);
        const CD3DVECTOR& maximum = field<CD3DVECTOR>(object, off::OBJECT_BOUNDS_MAX);
        field<std::int16_t>(object, off::OBJECT_SECTOR_INDEX) =
            world->FindContainingSector(minimum, maximum);
    }
    field<std::uint8_t>(object, off::OBJECT_STATE_0090) = 0;
}
}

void Object_RebuildGeometry(void* object) {
    RebuildGeometryNormals(object);

}

void Object_RecomputeBounds(void* object) {
    RecomputeBoundsComplete(object);
}

void Object_UpdateBase(void* object, float ) {

    if (field<std::uint8_t>(object, off::OBJECT_STATE_0090) != 1)
        return;

    reinterpret_cast<CD3DDYNAMICOBJECT*>(object)->RebuildPhysicsTransform();

    finalizeBaseUpdate(object, nullptr);
}

void Object_UpdateBase(void* object, float , WorldQuery& world) {
    if (field<std::uint8_t>(object, off::OBJECT_STATE_0090) != 1)
        return;
    reinterpret_cast<CD3DDYNAMICOBJECT*>(object)->RebuildPhysicsTransform();

    finalizeBaseUpdate(object, &world);
}

}

namespace flydemo {
using flydemo::mem::field;

namespace {
float gFrameDelta = 0.0f;

bool isSignedZero(float value) {

    std::uint32_t bits = 0;
    std::memcpy(&bits, &value, sizeof(bits));
    return (bits & 0x7fffffffu) == 0;
}
}

void Host_SetFrameDelta(float dt) {
    gFrameDelta = dt;
}

float Renderer_GetFrameDelta() {
    return gFrameDelta;
}

const CD3DVECTOR* Object_GetForward(void* object) {
    return &field<CD3DVECTOR>(object, off::OBJECT_FORWARD);
}

const CD3DVECTOR* Object_GetPosition(void* object) {
    return &field<CD3DVECTOR>(object, off::OBJECT_BOUNDS_CENTER);
}

void Object_SetPosition(void* object, const CD3DVECTOR& position) {

    const CD3DVECTOR current = field<CD3DVECTOR>(object, off::OBJECT_BOUNDS_CENTER);
    const CD3DVECTOR delta{
        position.x - current.x,
        position.y - current.y,
        position.z - current.z,
    };
    if (isSignedZero(delta.x) && isSignedZero(delta.y) && isSignedZero(delta.z))
        return;

    CD3DMATRIX translation{};
    SetIdentity(translation);
    translation.m[12] = delta.x;
    translation.m[13] = delta.y;
    translation.m[14] = delta.z;
    ComposeTransform(field<CD3DMATRIX>(object, off::OBJECT_PENDING_TRANSFORM),
                     translation);
    field<std::uint8_t>(object, off::OBJECT_STATE_0090) = 1;
}

}
