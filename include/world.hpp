#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>
#include <vector>

#include "types.hpp"
#include "dynamic_object.hpp"

namespace flydemo {

enum class WorldCullClass : std::uint8_t {
    Outside = 0,
    Intersect = 1,
    Inside = 2,
};

struct WorldAabb {
    CD3DVECTOR min{};
    CD3DVECTOR max{};
};

struct WorldCollisionFace {
    std::int16_t p0 = 0;
    std::int16_t p1 = 0;
    std::int16_t p2 = 0;
    CD3DVECTOR normal{};
};

struct WorldCollisionMesh {
    WorldAabb bounds{};
    std::vector<CD3DVECTOR> vertices;
    std::vector<WorldCollisionFace> faces;
};

struct WorldSegmentHit {
    bool hit = false;
    CD3DVECTOR point{};
    CD3DVECTOR normal{};
};

enum WorldDiag : std::uint32_t {
    WorldDiag_None             = 0,
    WorldDiag_NullObject       = 1u << 0,
    WorldDiag_DynamicLimit     = 1u << 1,
    WorldDiag_DuplicateName    = 1u << 2,
    World_DuplicatePointer = 1u << 3,
    WorldDiag_NotPolygonal     = 1u << 4,
    WorldDiag_NotDynamic       = 1u << 5,
    World_SectorFull = 1u << 6,
    WorldDiag_InvalidSector    = 1u << 7,
};

struct WorldEditResult {
    bool applied = false;
    std::uint32_t diagnostics = WorldDiag_None;
    std::size_t slot = static_cast<std::size_t>(-1);
};

class CD3DOBJECT {
public:
    virtual ~CD3DOBJECT() = default;

    virtual void Update(float dt) = 0;
    virtual void Render() = 0;

    virtual bool PendingWorldDelete() const = 0;
    virtual const CD3DVECTOR& BoundsCenter() const = 0;
    virtual std::int16_t SectorIndex() const = 0;
    virtual float BoundsRadius() const = 0;

    virtual std::string_view Name() const { return {}; }

    virtual const WorldCollisionMesh* CollisionMesh() const { return nullptr; }
};

struct SECTOR {

    WorldAabb collisionBounds{};

    float boundsRadius = 0.0f;
    CD3DVECTOR boundsCenter{};
    WorldCullClass visibility = WorldCullClass::Outside;
    std::array<std::int16_t, 4> children{{-1, -1, -1, -1}};
    std::vector<CD3DOBJECT*> staticObjects;
};

class WorldDelete {
public:
    virtual ~WorldDelete() = default;

    virtual void Destroy(CD3DOBJECT& object) = 0;
};

class WorldRender {
public:
    virtual ~WorldRender() = default;

    virtual WorldCullClass ClassifySphere(const CD3DVECTOR& center,
                                          float radius,
                                          bool objectTest) = 0;

    virtual void FlushDeferredGeometry() = 0;
};

class WorldState : public WorldQuery {
public:

    static constexpr std::size_t MaxDynamics = 0x1FF;

    std::array<CD3DOBJECT*, MaxDynamics>& Dynamics() { return dynamics_; }
    const std::array<CD3DOBJECT*, MaxDynamics>& Dynamics() const { return dynamics_; }
    std::vector<SECTOR>& Sectors() { return sectors_; }
    const std::vector<SECTOR>& Sectors() const { return sectors_; }

    void Update(float rendererFrameDelta, WorldDelete& deleter);

    void UpdateStaticObjects(float rendererFrameDelta, WorldDelete& deleter);

    WorldEditResult AddDynamicObject(CD3DOBJECT* object);

    WorldEditResult MakeObjectStatic(CD3DOBJECT* object, bool isPolygonalClass);

    std::size_t DynamicObjectCount() const;

    bool DeleteObjectByName(std::string_view name, WorldDelete& deleter);

    void Render(WorldRender& renderer);

    CD3DOBJECT* FindObjectByName(std::string_view name) const;

    bool DynamicNameExists(std::string_view name) const;

    std::int16_t FindContainingSector(const CD3DVECTOR& minimum,
                                      const CD3DVECTOR& maximum) override;

    WorldSegmentHit TraceStaticSegment(const CD3DVECTOR& start, const CD3DVECTOR& end) const;

private:
    std::int16_t FindContainingSector(const WorldAabb& query) const;
    void FindSectorRecursive(std::int16_t sector,
                                       const WorldAabb& query,
                                       std::int16_t& result) const;
    void ClassifySectorTree(std::int16_t sector,
                            WorldCullClass requested,
                            WorldRender& renderer);
    bool DynamicIsVisible(const CD3DOBJECT& object,
                          WorldRender& renderer) const;

    std::array<CD3DOBJECT*, MaxDynamics> dynamics_{};
    std::vector<SECTOR> sectors_;
};

}
