#include "world.hpp"

#include <cmath>
#include <cstddef>
#include <limits>
#include <string>

namespace flydemo {

namespace {
constexpr float kMaxFrameDelta = 3.0f;

constexpr float kMaxStaticFrameDelta = 0x1.01p-135f;

bool IsNonZero(WorldCullClass c) {
    return static_cast<std::uint8_t>(c) != 0;
}

constexpr double kSectorEpsilon = 0.0001;

bool AabbOverlaps(const WorldAabb& object, const WorldAabb& segment) {

    return object.max.x >= segment.min.x &&
           object.max.y >= segment.min.y &&
           object.max.z >= segment.min.z &&
           object.min.x <= segment.max.x &&
           object.min.y <= segment.max.y &&
           object.min.z <= segment.max.z;
}

bool TriangleMayTouchAabb(const CD3DVECTOR& a, const CD3DVECTOR& b, const CD3DVECTOR& c,
                          const WorldAabb& box) {

    if (a.x < box.min.x && b.x < box.min.x && c.x < box.min.x) return false;
    if (a.y < box.min.y && b.y < box.min.y && c.y < box.min.y) return false;
    if (a.z < box.min.z && b.z < box.min.z && c.z < box.min.z) return false;
    if (a.x > box.max.x && b.x > box.max.x && c.x > box.max.x) return false;
    if (a.y > box.max.y && b.y > box.max.y && c.y > box.max.y) return false;
    if (a.z > box.max.z && b.z > box.max.z && c.z > box.max.z) return false;
    return true;
}

float DotStored(const CD3DVECTOR& a, const CD3DVECTOR& b) {

    const long double v = static_cast<long double>(a.y) * b.y +
                          static_cast<long double>(a.x) * b.x +
                          static_cast<long double>(a.z) * b.z;
    return static_cast<float>(v);
}

CD3DVECTOR NormalizeVec(CD3DVECTOR v) {
    const long double len = std::sqrt(static_cast<long double>(v.y) * v.y +
                                      static_cast<long double>(v.x) * v.x +
                                      static_cast<long double>(v.z) * v.z);
    const long double inv = 1.0L / len;
    v.x = static_cast<float>(static_cast<long double>(v.x) * inv);
    v.y = static_cast<float>(static_cast<long double>(v.y) * inv);
    v.z = static_cast<float>(static_cast<long double>(v.z) * inv);
    return v;
}

struct PlaneLocal {
    float a = 0.0f, b = 0.0f, c = 0.0f, d = 0.0f;
};

PlaneLocal PlaneFromTriangle(const CD3DVECTOR& p0,
                                         const CD3DVECTOR& p1,
                                         const CD3DVECTOR& p2) {

    const long double ux = static_cast<long double>(p1.x) - p0.x;
    const long double uy = static_cast<long double>(p1.y) - p0.y;
    const long double uz = static_cast<long double>(p1.z) - p0.z;
    const long double vx = static_cast<long double>(p2.x) - p0.x;
    const long double vy = static_cast<long double>(p2.y) - p0.y;
    const long double vz = static_cast<long double>(p2.z) - p0.z;

    PlaneLocal p;
    p.a = static_cast<float>(uy * vz - uz * vy);
    p.b = static_cast<float>(uz * vx - ux * vz);
    p.c = static_cast<float>(ux * vy - uy * vx);
    const long double length = std::sqrt(static_cast<long double>(p.b) * p.b +
                                         static_cast<long double>(p.a) * p.a +
                                         static_cast<long double>(p.c) * p.c);
    const long double inv = 1.0L / length;
    p.a = static_cast<float>(static_cast<long double>(p.a) * inv);
    p.b = static_cast<float>(static_cast<long double>(p.b) * inv);
    p.c = static_cast<float>(static_cast<long double>(p.c) * inv);
    p.d = static_cast<float>(static_cast<long double>(p.b) * p0.y +
                             static_cast<long double>(p.a) * p0.x +
                             static_cast<long double>(p.c) * p0.z);
    return p;
}

float PlaneEval(const PlaneLocal& p, const CD3DVECTOR& v) {
    return static_cast<float>(static_cast<long double>(p.b) * v.y +
                              static_cast<long double>(p.a) * v.x +
                              static_cast<long double>(p.c) * v.z - p.d);
}

bool IntersectPlaneSegment(const PlaneLocal& p,
                                 const CD3DVECTOR& start,
                                 const CD3DVECTOR& end,
                                 CD3DVECTOR& hit) {

    const float d0 = PlaneEval(p, start);
    const float d1 = PlaneEval(p, end);
    if ((d0 < 0.0f && d1 < 0.0f) || (d0 > 0.0f && d1 > 0.0f))
        return false;
    const long double denom = static_cast<long double>(d0) - d1;

    if (denom == 0.0L || std::isnan(static_cast<double>(denom)))
        return false;
    const long double t = static_cast<long double>(d0) / denom;
    hit.x = static_cast<float>(static_cast<long double>(start.x) +
                               (static_cast<long double>(end.x) - start.x) * t);
    hit.y = static_cast<float>(static_cast<long double>(start.y) +
                               (static_cast<long double>(end.y) - start.y) * t);
    hit.z = static_cast<float>(static_cast<long double>(start.z) +
                               (static_cast<long double>(end.z) - start.z) * t);
    return true;
}

bool X87Equal(float a, float b) {

    return a == b || std::isnan(a) || std::isnan(b);
}

bool CandidateIsNoFarther(const CD3DVECTOR& start, const CD3DVECTOR& best,
                          const CD3DVECTOR& candidate) {

    auto axisAccept = [](float s, float b, float c, bool& axisVaries) {
        axisVaries = !X87Equal(b, s);
        if (std::isnan(b) || std::isnan(s) || std::isnan(c))
            return true;
        if (b > s && c > b) return false;
        if (b < s && c < b) return false;
        return true;
    };

    bool varies = false;
    if (!axisAccept(start.x, best.x, candidate.x, varies)) return false;
    if (varies) return true;
    if (!axisAccept(start.z, best.z, candidate.z, varies)) return false;
    if (varies) return true;
    if (!axisAccept(start.y, best.y, candidate.y, varies)) return false;
    return true;
}
}

std::size_t WorldState::DynamicObjectCount() const {
    std::size_t count = 0;
    for (CD3DOBJECT* object : dynamics_)
        if (object != nullptr)
            ++count;
    return count;
}

WorldEditResult WorldState::AddDynamicObject(CD3DOBJECT* object) {
    WorldEditResult result;
    if (object == nullptr) {
        result.diagnostics |= WorldDiag_NullObject;
        return result;
    }

    if (DynamicObjectCount() >= MaxDynamics)
        result.diagnostics |= WorldDiag_DynamicLimit;

    const std::string_view name = object->Name();

    for (const SECTOR& sector : sectors_) {
        for (CD3DOBJECT* existing : sector.staticObjects) {
            if (existing == nullptr)
                continue;
            if (existing->Name() == name)
                result.diagnostics |= WorldDiag_DuplicateName;
            if (existing == object)
                result.diagnostics |= World_DuplicatePointer;
        }
    }

    for (CD3DOBJECT* existing : dynamics_) {
        if (existing == nullptr)
            continue;
        if (existing->Name() == name)
            result.diagnostics |= WorldDiag_DuplicateName;
        if (existing == object)
            result.diagnostics |= World_DuplicatePointer;
    }

    for (std::size_t i = 0; i < dynamics_.size(); ++i) {
        if (dynamics_[i] != nullptr)
            continue;
        dynamics_[i] = object;
        result.applied = true;
        result.slot = i;
        return result;
    }

    result.diagnostics |= WorldDiag_DynamicLimit;
    return result;
}

WorldEditResult WorldState::MakeObjectStatic(CD3DOBJECT* object,
                                                        bool isPolygonalClass) {
    WorldEditResult result;
    if (object == nullptr) {
        result.diagnostics |= WorldDiag_NullObject;
        return result;
    }

    if (!isPolygonalClass)
        result.diagnostics |= WorldDiag_NotPolygonal;

    std::size_t dynamicSlot = dynamics_.size();
    for (std::size_t i = 0; i < dynamics_.size(); ++i) {
        if (dynamics_[i] == object) {
            dynamicSlot = i;
            break;
        }
    }
    if (dynamicSlot == dynamics_.size()) {
        result.diagnostics |= WorldDiag_NotDynamic;
        return result;
    }

    object->Update(0.0f);
    std::int16_t sectorIndex = object->SectorIndex();
    if (sectorIndex == -1)
        sectorIndex = 0;
    if (sectorIndex < 0 || static_cast<std::size_t>(sectorIndex) >= sectors_.size()) {
        result.diagnostics |= WorldDiag_InvalidSector;
        return result;
    }

    SECTOR& sector = sectors_[static_cast<std::size_t>(sectorIndex)];

    if (sector.staticObjects.size() >= 0x24u) {
        result.diagnostics |= World_SectorFull;
        return result;
    }

    result.slot = sector.staticObjects.size();
    sector.staticObjects.push_back(object);
    dynamics_[dynamicSlot] = nullptr;
    result.applied = true;
    return result;
}

bool WorldState::DeleteObjectByName(std::string_view name,
                                          WorldDelete& deleter) {

    for (SECTOR& sector : sectors_) {
        for (std::size_t i = 0; i < sector.staticObjects.size(); ++i) {
            CD3DOBJECT* object = sector.staticObjects[i];
            if (object == nullptr || object->Name() != name)
                continue;

            deleter.Destroy(*object);
            const std::size_t last = sector.staticObjects.size() - 1;
            if (i != last)
                sector.staticObjects[i] = sector.staticObjects[last];
            sector.staticObjects.pop_back();
            return true;
        }
    }

    for (CD3DOBJECT*& object : dynamics_) {
        if (object == nullptr || object->Name() != name)
            continue;

        deleter.Destroy(*object);
        object = nullptr;
        return true;
    }

    return false;
}

void WorldState::Update(float rendererFrameDelta,
                              WorldDelete& deleter) {

    float dt = rendererFrameDelta;
    if (dt > kMaxFrameDelta)
        dt = 0.0f;

    for (CD3DOBJECT* object : dynamics_) {
        if (object == nullptr)
            continue;

        object->Update(dt);
        if (!object->PendingWorldDelete())
            continue;

        const std::string name(object->Name());
        DeleteObjectByName(name, deleter);
    }
}

void WorldState::UpdateStaticObjects(float rendererFrameDelta,
                                           WorldDelete& deleter) {
    float dt = rendererFrameDelta;
    if (dt > kMaxStaticFrameDelta)
        dt = 0.0f;

    for (SECTOR& sector : sectors_) {
        std::size_t i = 0;
        while (i < sector.staticObjects.size()) {
            CD3DOBJECT* object = sector.staticObjects[i];
            if (object == nullptr) {
                ++i;
                continue;
            }

            object->Update(dt);
            if (object->PendingWorldDelete()) {
                const std::string name(object->Name());
                DeleteObjectByName(name, deleter);
            }
            ++i;
        }
    }
}

void WorldState::ClassifySectorTree(std::int16_t sectorIndex,
                                          WorldCullClass requested,
                                          WorldRender& renderer) {

    if (sectorIndex < 0 || static_cast<std::size_t>(sectorIndex) >= sectors_.size())
        return;

    SECTOR& sector = sectors_[static_cast<std::size_t>(sectorIndex)];
    WorldCullClass propagated = requested;

    if (requested == WorldCullClass::Intersect) {
        propagated = renderer.ClassifySphere(sector.boundsCenter,
                                              sector.boundsRadius,
                                              false);
    }
    sector.visibility = propagated;

    for (std::int16_t child : sector.children) {
        if (child != -1)
            ClassifySectorTree(child, propagated, renderer);
    }
}

bool WorldState::DynamicIsVisible(const CD3DOBJECT& object,
                                        WorldRender& renderer) const {

    const std::int16_t sectorIndex = object.SectorIndex();
    if (sectorIndex != -1 &&
        sectorIndex >= 0 &&
        static_cast<std::size_t>(sectorIndex) < sectors_.size()) {
        const WorldCullClass sectorClass =
            sectors_[static_cast<std::size_t>(sectorIndex)].visibility;
        if (sectorClass == WorldCullClass::Outside)
            return false;
        if (sectorClass == WorldCullClass::Inside)
            return true;

    }

    return IsNonZero(renderer.ClassifySphere(object.BoundsCenter(),
                                             object.BoundsRadius(),
                                             true));
}

void WorldState::Render(WorldRender& renderer) {

    if (!sectors_.empty())
        ClassifySectorTree(0, WorldCullClass::Intersect, renderer);

    for (SECTOR& sector : sectors_) {
        if (sector.visibility == WorldCullClass::Outside)
            continue;

        if (sector.visibility == WorldCullClass::Intersect) {

            for (CD3DOBJECT* object : sector.staticObjects) {
                if (object != nullptr &&
                    IsNonZero(renderer.ClassifySphere(object->BoundsCenter(),
                                                      object->BoundsRadius(),
                                                      true))) {
                    object->Render();
                }
            }
        } else if (sector.visibility == WorldCullClass::Inside) {

            for (CD3DOBJECT* object : sector.staticObjects) {
                if (object != nullptr)
                    object->Render();
            }
        }
    }

    for (CD3DOBJECT* object : dynamics_) {
        if (object != nullptr && DynamicIsVisible(*object, renderer))
            object->Render();
    }

    renderer.FlushDeferredGeometry();
}

CD3DOBJECT* WorldState::FindObjectByName(std::string_view name) const {

    for (const SECTOR& sector : sectors_) {
        for (CD3DOBJECT* object : sector.staticObjects) {
            if (object != nullptr && object->Name() == name)
                return object;
        }
    }

    for (CD3DOBJECT* object : dynamics_) {
        if (object != nullptr && object->Name() == name)
            return object;
    }
    return nullptr;
}

bool WorldState::DynamicNameExists(std::string_view name) const {

    for (CD3DOBJECT* object : dynamics_) {
        if (object != nullptr && object->Name() == name)
            return true;
    }
    return false;
}

void WorldState::FindSectorRecursive(std::int16_t sectorIndex,
                                                      const WorldAabb& query,
                                                      std::int16_t& result) const {

    if (sectorIndex < 0 || static_cast<std::size_t>(sectorIndex) >= sectors_.size())
        return;
    const SECTOR& sector = sectors_[static_cast<std::size_t>(sectorIndex)];
    const WorldAabb& box = sector.collisionBounds;

    const long double minusEps = -kSectorEpsilon;
    const long double plusEps = kSectorEpsilon;
    const bool contained =
        static_cast<long double>(query.min.x) >= static_cast<long double>(box.min.x) + minusEps &&
        static_cast<long double>(query.max.x) <= static_cast<long double>(box.max.x) + plusEps &&
        static_cast<long double>(query.min.y) >= static_cast<long double>(box.min.y) + minusEps &&
        static_cast<long double>(query.max.y) <= static_cast<long double>(box.max.y) + plusEps &&
        static_cast<long double>(query.min.z) >= static_cast<long double>(box.min.z) + minusEps &&
        static_cast<long double>(query.max.z) <= static_cast<long double>(box.max.z) + plusEps;
    if (!contained)
        return;

    result = sectorIndex;
    for (std::int16_t child : sector.children) {
        if (child != -1)
            FindSectorRecursive(child, query, result);
    }
}

std::int16_t WorldState::FindContainingSector(const WorldAabb& query) const {
    std::int16_t result = -1;
    if (!sectors_.empty())
        FindSectorRecursive(0, query, result);
    return result;
}

std::int16_t WorldState::FindContainingSector(const CD3DVECTOR& minimum,
                                                     const CD3DVECTOR& maximum) {
    return FindContainingSector(WorldAabb{minimum, maximum});
}

WorldSegmentHit WorldState::TraceStaticSegment(const CD3DVECTOR& start,
                                                      const CD3DVECTOR& end) const {
    WorldSegmentHit result;
    result.point = end;

    WorldAabb segmentBounds{start, start};
    if (end.x > segmentBounds.max.x) segmentBounds.max.x = end.x;
    if (end.y > segmentBounds.max.y) segmentBounds.max.y = end.y;
    if (end.z > segmentBounds.max.z) segmentBounds.max.z = end.z;
    if (end.x < segmentBounds.min.x) segmentBounds.min.x = end.x;
    if (end.y < segmentBounds.min.y) segmentBounds.min.y = end.y;
    if (end.z < segmentBounds.min.z) segmentBounds.min.z = end.z;

    std::int16_t sector = FindContainingSector(segmentBounds);
    if (sector == -1)
        sector = 0;

    if (X87Equal(start.x, end.x) &&
        X87Equal(start.y, end.y) &&
        X87Equal(start.z, end.z)) {
        return result;
    }

    CD3DVECTOR delta{
        static_cast<float>(static_cast<long double>(end.x) - start.x),
        static_cast<float>(static_cast<long double>(end.y) - start.y),
        static_cast<float>(static_cast<long double>(end.z) - start.z),
    };
    const CD3DVECTOR directionNormalized = NormalizeVec(delta);
    result.normal = {-delta.x, -delta.y, -delta.z};

    struct TraceContext {
        const WorldState* world;
        const CD3DVECTOR* start;
        const CD3DVECTOR* end;
        const CD3DVECTOR* direction;
        const WorldAabb* bounds;
        WorldSegmentHit* result;
    } ctx{this, &start, &end, &directionNormalized, &segmentBounds, &result};

    const auto recurse = [&](const auto& self, std::int16_t sectorIndex) -> void {
        if (sectorIndex < 0 || static_cast<std::size_t>(sectorIndex) >= ctx.world->sectors_.size())
            return;
        const SECTOR& sec = ctx.world->sectors_[static_cast<std::size_t>(sectorIndex)];
        for (CD3DOBJECT* object : sec.staticObjects) {
            if (object == nullptr) continue;
            const WorldCollisionMesh* mesh = object->CollisionMesh();
            if (mesh == nullptr || !AabbOverlaps(mesh->bounds, *ctx.bounds)) continue;
            for (const WorldCollisionFace& face : mesh->faces) {
                auto valid = [&](std::int16_t i) {
                    return i >= 0 && static_cast<std::size_t>(i) < mesh->vertices.size();
                };
                if (!valid(face.p0) || !valid(face.p1) || !valid(face.p2)) continue;
                const CD3DVECTOR& p0 = mesh->vertices[static_cast<std::size_t>(face.p0)];
                const CD3DVECTOR& p1 = mesh->vertices[static_cast<std::size_t>(face.p1)];
                const CD3DVECTOR& p2 = mesh->vertices[static_cast<std::size_t>(face.p2)];
                if (!TriangleMayTouchAabb(p0, p1, p2, *ctx.bounds)) continue;
                const float facing = DotStored(*ctx.direction, face.normal);
                if (facing <= 0.0f) continue;
                const PlaneLocal plane = PlaneFromTriangle(p0, p1, p2);
                CD3DVECTOR candidate{};
                if (!IntersectPlaneSegment(plane, *ctx.start, *ctx.end, candidate)) continue;
                if (!CandidateIsNoFarther(*ctx.start, ctx.result->point, candidate)) continue;
                ctx.result->hit = true;
                ctx.result->point = candidate;
                ctx.result->normal = face.normal;
            }
        }
        for (std::int16_t child : sec.children)
            if (child != -1) self(self, child);
    };
    recurse(recurse, sector);
    return result;
}

}
