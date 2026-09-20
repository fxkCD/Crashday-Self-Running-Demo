#pragma once
#include "transform.hpp"
#include "types.hpp"
#include <array>
#include <cstdint>
#include <string_view>
#include <vector>

namespace flydemo {

class CBMManager;
class CBMLoader;

namespace D3DStateId {
constexpr std::uint32_t ZWriteEnable = 0x0E;
constexpr std::uint32_t SrcBlend = 0x13;
constexpr std::uint32_t DestBlend = 0x14;
constexpr std::uint32_t AlphaBlendEnable = 0x1B;
constexpr std::uint32_t State1D = 0x1D;
}

namespace D3DBlendValue {
constexpr std::uint32_t Zero = 1;
constexpr std::uint32_t One = 2;
constexpr std::uint32_t SrcAlpha = 5;
constexpr std::uint32_t InvSrcAlpha = 6;
}

namespace D3DTransformStateId {
constexpr std::uint32_t View = 2u;
constexpr std::uint32_t Projection = 3u;
}

enum class SphereVisibility : std::uint8_t {
    Outside = 0,
    Intersect = 1,
    Inside = 2,
};

struct FrameSetup {
    CD3DVECTOR cameraPosition{0.0f, 0.0f, 0.0f};
    CD3DVECTOR cameraRotation{0.0f, 0.0f, 0.0f};
    std::int32_t screenWidth = 0;
    std::int32_t screenHeight = 0;
    float farClip = 0.0f;
};

struct FrameTransforms {
    CD3DMATRIX view{};
    CD3DMATRIX projection{};
    CD3DMATRIX viewport{};
    CD3DMATRIX combined{};
    CD3DVECTOR clipForward{0.0f, 0.0f, 1.0f};
};

FrameTransforms BuildFrameTransforms(const FrameSetup& setup);

struct TriangleDraw {
    static constexpr std::uint32_t PrimitiveType = 4u;
    static constexpr std::uint32_t VertexTypeDesc = 0x000002C4u;
    static constexpr std::uint32_t DrawFlags = 8u;
    static constexpr std::int16_t MaxTriangles = 0x0200;

    static constexpr std::uint32_t VertexCount(std::int16_t triangleCount) {
        return triangleCount > 0 ? static_cast<std::uint32_t>(triangleCount) * 3u : 0u;
    }
};

using TransformMatrix = std::array<float, 16>;

struct ProjectedPosition {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    float rhw = 0.0f;
};

struct TransformedVertex {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    float rhw = 0.0f;
    std::uint32_t diffuse = 0;
    std::uint32_t specular = 0;
    float field18 = 0.0f;
    float field1C = 0.0f;
    float environmentU = 0.0f;
    float environmentV = 0.0f;
};
static_assert(sizeof(TransformedVertex) == 0x28, "transformed vertex stride");

struct ObjectVertex {
    float positionX = 0.0f;
    float positionY = 0.0f;
    float positionZ = 0.0f;
    float normalX = 0.0f;
    float normalY = 0.0f;
    float normalZ = 0.0f;
    std::array<std::uint8_t, 0x08> reserved18To1F{};
};
static_assert(sizeof(ObjectVertex) == 0x20, "object vertex stride");

struct CD3DPOLYGON {
    std::int16_t p1 = 0;
    std::int16_t p2 = 0;
    std::int16_t p3 = 0;
    std::uint16_t field06 = 0;
    float u1 = 0.0f, v1 = 0.0f;
    float u2 = 0.0f, v2 = 0.0f;
    float u3 = 0.0f, v3 = 0.0f;
    std::uint32_t diffuse = 0;
    float normalX = 0.0f;
    float normalY = 0.0f;
    float normalZ = 0.0f;
};
static_assert(sizeof(CD3DPOLYGON) == 0x30, "runtime face stride");

struct ImmediateVertex {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    float rhw = 0.0f;
    std::uint32_t diffuse = 0;
    std::uint32_t specular = 0;
    float u0 = 0.0f;
    float v0 = 0.0f;
    float untouched20 = 0.0f;
    float untouched24 = 0.0f;
};
static_assert(sizeof(ImmediateVertex) == 0x28, "immediate vertex stride");

struct DeferredTriangle {
    std::array<ImmediateVertex, 3> vertices{};
    float depthAccumulator = 0.0f;
    std::uint8_t textureIndex = 0;
    std::uint8_t textureVariant = 0;
    std::uint8_t blendMode = 0;
    std::uint8_t untouched7F = 0;
};
static_assert(sizeof(DeferredTriangle) == 0x80, "deferred triangle stride");

class DeferredTriangleQueue {
public:
    static constexpr std::uint16_t MaxTriangles = 0x07FFu;

    DeferredTriangleQueue();

    void Reset();
    std::uint16_t Count() const { return count_; }
    const DeferredTriangle& Entry(std::uint16_t index) const;

    bool QueuePolyTriangles(const CD3DPOLYGON* faces, std::size_t faceStorageCount,
                            std::int16_t triangleCount,
                            const TransformedVertex* transformed,
                            std::size_t transformedCount,
                            std::uint8_t effectiveState1DMode,
                            std::uint8_t textureIndex,
                            std::size_t* queued = nullptr);

    bool QueueParticleBillboard(float projectedX, float projectedY,
                                       float projectedZ, float rhw, float size,
                                       std::uint32_t color,
                                       std::uint8_t textureIndex,
                                       std::uint8_t textureVariant,
                                       std::uint8_t blendMode,
                                       std::int32_t screenWidth,
                                       std::size_t* queued = nullptr);

    std::vector<std::uint16_t> FlushOrder() const;

private:
    std::vector<DeferredTriangle> entries_;
    std::uint16_t count_ = 0;
};

struct DeferredFlushSpec {
    static constexpr float GlobalAlpha = 1.0f;
    static constexpr bool ZWrite = false;
    static constexpr std::uint32_t PrimitiveType = 4u;
    static constexpr std::uint32_t VertexTypeDesc = 0x000002C4u;
    static constexpr std::uint32_t VertexCount = 3u;
    static constexpr std::uint32_t DrawFlags = 8u;
};

bool BuildImmediateTriangles(
    const CD3DPOLYGON* faces, std::size_t faceStorageCount,
    std::int16_t triangleCount,
    const TransformedVertex* transformed, std::size_t transformedCount,
    std::uint8_t effectiveState1DMode,
    std::vector<ImmediateVertex>& scratch);

bool BuildMode4SecondPass(
    const CD3DPOLYGON* faces, std::size_t faceStorageCount,
    std::int16_t triangleCount,
    const ObjectVertex* sourceVertices, std::size_t sourceVertexCount,
    TransformedVertex* transformed, std::size_t transformedCount,
    const CD3DMATRIX& view,
    std::vector<ImmediateVertex>& scratch);

ProjectedPosition ProjectPosition(
    const CD3DVECTOR& source, const TransformMatrix& matrix);
std::vector<ProjectedPosition> ProjectPositions(
    const std::vector<CD3DVECTOR>& source, const TransformMatrix& matrix,
    std::int16_t vertexCount);

enum class BlendMode : std::uint8_t {
    Opaque = 0,
    Additive = 1,
    Alpha = 2,
    AlphaAdditive = 3,
};

struct SkyBoxTextures {
    std::uint8_t top = 0;
    std::uint8_t bottom = 0;
    std::uint8_t side = 0;
};

enum class SkyBoxSurface : std::uint8_t {
    Side = 0,
    Top = 1,
    Bottom = 2,
};

struct SkyBoxBatch {
    SkyBoxSurface surface = SkyBoxSurface::Side;
    std::uint8_t textureIndex = 0;
    std::uint8_t textureVariant = 1;
    std::uint8_t state1DMode = 5;
    std::int16_t triangleCount = 0;
    BlendMode requestedBlend = BlendMode::Opaque;
};

struct SkyBoxFaces {
    std::array<CD3DPOLYGON, 8> sides{};
    std::array<CD3DPOLYGON, 2> top{};
    std::array<CD3DPOLYGON, 2> bottom{};
};

struct SkyBoxSetup {
    std::array<CD3DVECTOR, 8> positions{};
    std::array<ProjectedPosition, 8> projected{};
    SkyBoxFaces faces{};
    std::array<SkyBoxBatch, 3> batches{};

    std::uint32_t transformedSpecular = 0xFF000000u;
    bool zWrite = false;
};

const SkyBoxFaces& GetSkyBoxFaces();
std::array<CD3DVECTOR, 8> BuildSkyBoxPositions(const CD3DVECTOR& cameraPosition);
SkyBoxSetup BuildSkyBoxPlan(const CD3DVECTOR& cameraPosition,
                                       const TransformMatrix& combinedMatrix,
                                       const SkyBoxTextures& textures);

class RenderDevice {
public:
    virtual ~RenderDevice() = default;
    virtual bool BeginScene() = 0;
    virtual bool EndScene() = 0;
    virtual void SetRenderState(std::uint32_t state, std::uint32_t value) = 0;

    virtual void SetTextureStageState(std::uint32_t, std::uint32_t, std::uint32_t) {}
    virtual void SetLightState(std::uint32_t, std::uint32_t) {}
    virtual void SetTexture(std::uint8_t textureIndex, std::uint8_t variant) = 0;

    virtual bool TextureHasAlpha(std::uint8_t) const { return false; }

    virtual void DrawPrimitive(std::uint32_t, std::uint32_t,
                               const ImmediateVertex*, std::uint32_t,
                               std::uint32_t) {}

    virtual void SetTransform(std::uint32_t, const CD3DMATRIX&) {}

    virtual std::uint32_t ComputeSphereVisibility(const CD3DVECTOR&, float) { return 0; }
};

void ApplyFrameTransforms(const FrameTransforms& transforms,
                                RenderDevice& backend);

class RendererState {
public:
    RendererState();

    void SetDeviceAvailable(bool value) { deviceAvailable_ = value; }
    void SetViewportAvailable(bool value) { viewportAvailable_ = value; }

    bool BeginScene(RenderDevice& backend);

    bool BeginScene(RenderDevice& backend, const FrameSetup& setup);
    bool EndScene(RenderDevice& backend);

    void SelectTexture(std::uint8_t textureIndex, std::uint8_t variant,
                       RenderDevice& backend);
    BlendMode SetBlendMode(BlendMode requested, bool selectedTextureHasAlpha,
                           RenderDevice& backend);
    void SetGlobalAlpha(float alpha);
    void SetZWrite(bool enabled, RenderDevice& backend);

    SphereVisibility ClassifySphere(const CD3DVECTOR& center, float radius,
                                           bool objectTest,
                                           RenderDevice& backend) const;

    bool PrepareSkyBox(const CD3DVECTOR& cameraPosition,
                       const SkyBoxTextures& textures,
                       RenderDevice& backend,
                       SkyBoxSetup& out);

    bool SubmitTriangles(const CD3DPOLYGON* faces, std::size_t faceStorageCount,
                         std::int16_t triangleCount,
                         const ObjectVertex* sourceVertices,
                         std::size_t sourceVertexCount,
                         TransformedVertex* transformed,
                         std::size_t transformedCount,
                         std::uint8_t textureIndex,
                         std::uint8_t requestedState1DMode,
                         RenderDevice& backend);
    bool RenderSkyBox(const CD3DVECTOR& cameraPosition,
                      const SkyBoxTextures& textures,
                      RenderDevice& backend,
                      SkyBoxSetup* capturedPlan = nullptr);

    bool FlushDeferredTriangles(RenderDevice& backend);

    bool QueueParticleBillboard(float projectedX, float projectedY,
                                       float projectedZ, float rhw, float size,
                                       std::uint32_t color,
                                       std::uint8_t textureIndex,
                                       std::uint8_t textureVariant,
                                       std::uint8_t blendMode,
                                       std::size_t* queued = nullptr);

    void SetState1DOptions(bool flag4E, bool flag51) {
        optionFlag4E_ = flag4E;
        optionFlag51_ = flag51;
    }
    void SetEnvironmentTexture(std::uint8_t textureIndex) { environmentTexture_ = textureIndex; }

    std::uint8_t SetState1DMode(std::uint8_t requested,
                                bool flag4E,
                                bool flag51,
                                RenderDevice& backend);

    bool SceneInProgress() const { return sceneInProgress_; }

    std::uint16_t DeferredTriangleCount() const { return deferredQueue_.Count(); }
    std::uint8_t TextureIndex() const { return textureIndex_; }
    std::uint8_t TextureVariant() const { return textureVariant_; }
    float GlobalAlpha() const { return globalAlpha_; }
    bool ZWriteEnabled() const { return zWriteEnabled_; }
    BlendMode CurrentBlendMode() const { return blendMode_; }
    std::uint8_t CurrentState1DMode() const { return state1DMode_; }
    bool HasGetFrameTransforms() const { return hasFrameTransforms_; }
    const FrameTransforms& GetFrameTransforms() const { return frameTransforms_; }
    const CD3DVECTOR& FrameCameraPosition() const { return frameCameraPosition_; }
    std::int32_t FrameScreenWidth() const { return frameScreenWidth_; }
    float RenderRange() const { return renderRange_; }

private:
    bool deviceAvailable_ = false;
    bool viewportAvailable_ = false;
    bool sceneInProgress_ = false;

    std::uint8_t textureIndex_ = 0;
    std::uint8_t textureVariant_ = 1;
    float globalAlpha_ = 1.0f;
    bool zWriteEnabled_ = true;
    BlendMode blendMode_ = BlendMode::Opaque;
    std::uint8_t state1DMode_ = 0;
    FrameTransforms frameTransforms_{};
    CD3DVECTOR frameCameraPosition_{};
    std::int32_t frameScreenWidth_ = 0;
    float renderRange_ = 400.0f;
    bool hasFrameTransforms_ = false;
    bool optionFlag4E_ = false;
    bool optionFlag51_ = false;
    std::uint8_t environmentTexture_ = 0;
    DeferredTriangleQueue deferredQueue_{};
    std::vector<ImmediateVertex> immediateScratch_ =
        std::vector<ImmediateVertex>(TriangleDraw::MaxTriangles * 3u);
};

bool DrawTexturedRect(std::string_view textureName,
                            std::uint8_t textureVariant,
                            std::int32_t x1, std::int32_t y1,
                            std::int32_t x2, std::int32_t y2,
                            float unusedScale,
                            BlendMode requestedBlend,
                            std::uint32_t color,
                            CBMManager& textures, CBMLoader& loader,
                            RendererState& renderer,
                            RenderDevice& backend);

bool DrawTexturedImage(std::string_view textureName,
                               std::uint8_t textureVariant,
                               std::int32_t x, std::int32_t y,
                               float unusedScale,
                               BlendMode requestedBlend,
                               std::uint32_t color,
                               CBMManager& textures, CBMLoader& loader,
                               RendererState& renderer,
                               RenderDevice& backend);

}
