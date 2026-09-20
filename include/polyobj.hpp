#pragma once

#include "cbm.hpp"
#include "lighting.hpp"
#include "plane.hpp"
#include "render.hpp"
#include "transform.hpp"
#include "types.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace flydemo {

struct P3DTextureGroup {
    std::uint16_t field12 = 0;
    std::string textureName;
    std::array<std::int16_t, 6> fields14To1E{};
};

struct P3DVertex {
    CD3DVECTOR position{};
};

struct P3DFace {
    std::int16_t p1 = 0;
    std::int16_t p2 = 0;
    std::int16_t p3 = 0;
    float u1 = 0.0f, v1 = 0.0f;
    float u2 = 0.0f, v2 = 0.0f;
    float u3 = 0.0f, v3 = 0.0f;
};

struct P3DMaterial {
    CD3DVECTOR position{};
    float range = 10.0f;
    std::uint32_t packedColor = 0x00FFFFFFu;
    float cachedVectorLength = 0.0f;
    std::uint8_t coronas = 1;
    std::uint8_t lensFlares = 1;
    float scale = 1.0f;
    std::uint8_t lightUp = 1;
};

struct P3DModel {
    std::uint8_t version = 0;
    std::array<std::uint8_t, 0x12> headerBytes{};
    float sizeX = 0.0f;
    float sizeY = 0.0f;
    float sizeZ = 0.0f;

    std::uint8_t postSizeByte = 0;
    std::vector<P3DTextureGroup> textureGroups;
    std::vector<P3DVertex> vertices;
    std::vector<P3DFace> faces;
    std::vector<P3DMaterial> materials;
    std::size_t consumedBytes = 0;

    CD3DVECTOR InitialCenter() const;
    std::array<CD3DVECTOR, 8> InitialBoundsCorners() const;
};

bool ParseP3DModel(const std::vector<std::uint8_t>& bytes,
                   P3DModel& out,
                   std::string* error = nullptr);

struct P3DDrawBatch {
    std::size_t textureGroupIndex = 0;
    std::uint8_t renderMode = 0;
    std::int16_t firstFace = 0;
    std::int16_t faceCount = 0;
};

std::vector<P3DDrawBatch> BuildP3DDrawBatches(const P3DModel& model);

enum class P3DTextureSlotState : std::uint8_t {
    Bound = 0,
    IndeterminateNativeCopy = 1,
};

struct P3DBoundTextureGroup {
    std::string textureName;
    std::uint8_t textureIndex = CBMManager::InvalidTexture;

    P3DTextureSlotState textureSlotState = P3DTextureSlotState::Bound;
    std::uint16_t firstFace = 0;
    std::array<std::int16_t, 6> modeCounts{};
};

struct P3DLightingEnvironment {
    CD3DVECTOR direction{};
    std::uint32_t directionalColor = 0;
    std::uint32_t ambientColor = 0;
};

struct TextureReplaceResult {
    std::size_t matchedGroups = 0;
    bool targetWasLoaded = true;
};

struct CD3DPOLYGONOBJECT {
    float sizeX = 0.0f;
    float sizeY = 0.0f;
    float sizeZ = 0.0f;
    std::uint8_t lightingMode = 0;

    CD3DVECTOR center{};
    CD3DVECTOR forward{0.0f, 0.0f, 1.0f};
    CD3DVECTOR up{0.0f, 1.0f, 0.0f};
    CD3DVECTOR right{1.0f, 0.0f, 0.0f};
    std::array<CD3DVECTOR, 8> boundsPoints{};

    std::array<CD3DPOLYGON, 12> boundsFaces{};
    std::array<CD3DPLANE, 6> boundsPlanes{};
    CD3DVECTOR boundsMin{};
    CD3DVECTOR boundsMax{};
    float boundRadius = 0.0f;

    std::vector<P3DBoundTextureGroup> textureGroups;
    std::vector<ObjectVertex> sourceVertices;
    std::vector<TransformedVertex> transformedVertices;
    std::vector<CD3DPOLYGON> faces;
    std::vector<CD3DLIGHT> embeddedLights;

    CD3DMATRIX pendingTransform{};
    std::uint8_t transformState = 0;

    bool InitializeFromModel(const P3DModel& model,
                             CBMManager& textures,
                             CBMLoader& loader,
                             std::string* error = nullptr);

    void UnloadRuntime();

    bool CopyFromNative(const CD3DPOLYGONOBJECT* source,
                        std::string* error = nullptr);

    bool RebuildGeometry(std::string* error = nullptr);

    bool SyncLighting(const P3DLightingEnvironment& environment,
                      std::string* error = nullptr);

    void SetCenterPosition(const CD3DVECTOR& position);
    void Translate(float x, float y, float z);
    void RotateX(const CD3DVECTOR& pivot, float angle);
    void RotateY(const CD3DVECTOR& pivot, float angle);
    void RotateZ(const CD3DVECTOR& pivot, float angle);

    bool ApplyPendingTransform(const P3DLightingEnvironment& environment,
                               std::string* error = nullptr);

    bool Render(RendererState& renderer,
                RenderDevice& backend,
                std::string* error = nullptr);

    TextureReplaceResult ReplaceTexture(std::string_view from,
                                           std::string_view to,
                                           const CBMManager& textures);
};

}
