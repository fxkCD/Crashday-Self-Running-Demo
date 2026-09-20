#include "render.hpp"
#include "cbm.hpp"

#include <algorithm>
#include <cstdint>

namespace flydemo {

namespace {

long double X87Sum4Extended(float x, float mx, float y, float my,
                            float z, float mz, float add) {
    return static_cast<long double>(x) * static_cast<long double>(mx) +
           static_cast<long double>(y) * static_cast<long double>(my) +
           static_cast<long double>(z) * static_cast<long double>(mz) +
           static_cast<long double>(add);
}
}

FrameTransforms BuildFrameTransforms(const FrameSetup& setup) {

    FrameTransforms out;
    CD3DMATRIX temporary{};

    SetIdentity(out.view);
    out.view.m[12] = -setup.cameraPosition.x;
    out.view.m[13] = -setup.cameraPosition.y;
    out.view.m[14] = -setup.cameraPosition.z;

    MakeYRotation(temporary, setup.cameraRotation.y);
    ComposeTransform(out.view, temporary);
    MakeXRotation(temporary, setup.cameraRotation.x);
    ComposeTransform(out.view, temporary);
    MakeZRotation(temporary, setup.cameraRotation.z);
    ComposeTransform(out.view, temporary);

    SetIdentity(temporary);
    temporary.m[0] = 0.85000002384185791015625f;
    ComposeTransform(out.view, temporary);

    MakeProjection(out.projection, 54.0f, 0.05f, setup.farClip);

    const std::int32_t halfWidthInt = setup.screenWidth / 2;
    const std::int32_t halfHeightInt = setup.screenHeight / 2;
    const float halfWidth = static_cast<float>(halfWidthInt);
    const float halfHeight = static_cast<float>(halfHeightInt);

    SetIdentity(out.viewport);
    out.viewport.m[0] = halfWidth;
    out.viewport.m[5] = -halfHeight;
    out.viewport.m[10] = 1.0f;
    SetIdentity(temporary);
    temporary.m[12] = halfWidth;
    temporary.m[13] = halfHeight;
    temporary.m[14] = 0.0f;
    ComposeTransform(out.viewport, temporary);

    SetIdentity(out.combined);
    ComposeTransform(out.combined, out.view);
    ComposeTransform(out.combined, out.projection);
    ComposeTransform(out.combined, out.viewport);
    out.clipForward = {0.0f, 0.0f, 1.0f};
    return out;
}

void ApplyFrameTransforms(const FrameTransforms& transforms,
                                RenderDevice& backend) {

    backend.SetTransform(D3DTransformStateId::View, transforms.view);
    backend.SetTransform(D3DTransformStateId::Projection, transforms.projection);
}

namespace {
CD3DPOLYGON MakeSkyFace(std::int16_t p1, float u1, float v1,
                              std::int16_t p2, float u2, float v2,
                              std::int16_t p3, float u3, float v3) {
    CD3DPOLYGON out{};
    out.p1 = p1; out.p2 = p2; out.p3 = p3;
    out.u1 = u1; out.v1 = v1;
    out.u2 = u2; out.v2 = v2;
    out.u3 = u3; out.v3 = v3;
    return out;
}
}

const SkyBoxFaces& GetSkyBoxFaces() {

    static const SkyBoxFaces faces = [] {
        constexpr float lo = 0.0199999995529651641845703125f;
        constexpr float hi = 0.980000019073486328125f;
        SkyBoxFaces f{};
        f.sides[0] = MakeSkyFace(0,0.0f,lo, 1,1.0f,lo, 5,1.0f,hi);
        f.sides[1] = MakeSkyFace(0,0.0f,lo, 5,1.0f,hi, 4,0.0f,hi);
        f.sides[2] = MakeSkyFace(2,0.0f,lo, 3,1.0f,lo, 7,1.0f,hi);
        f.sides[3] = MakeSkyFace(2,0.0f,lo, 7,1.0f,hi, 6,0.0f,hi);
        f.sides[4] = MakeSkyFace(3,0.0f,lo, 0,1.0f,lo, 4,1.0f,hi);
        f.sides[5] = MakeSkyFace(3,0.0f,lo, 4,1.0f,hi, 7,0.0f,hi);
        f.sides[6] = MakeSkyFace(1,0.0f,lo, 2,1.0f,lo, 6,1.0f,hi);
        f.sides[7] = MakeSkyFace(1,0.0f,lo, 6,1.0f,hi, 5,0.0f,hi);
        f.top[0] = MakeSkyFace(3,0.0f,0.0f, 2,1.0f,0.0f, 1,1.0f,1.0f);
        f.top[1] = MakeSkyFace(3,0.0f,0.0f, 1,1.0f,1.0f, 0,0.0f,1.0f);
        f.bottom[0] = MakeSkyFace(4,0.0f,0.0f, 5,1.0f,0.0f, 6,1.0f,1.0f);
        f.bottom[1] = MakeSkyFace(4,0.0f,0.0f, 6,1.0f,1.0f, 7,0.0f,1.0f);
        return f;
    }();
    return faces;
}

std::array<CD3DVECTOR, 8> BuildSkyBoxPositions(const CD3DVECTOR& cameraPosition) {

    constexpr float extentNeg = -10.0f;
    constexpr float extentPos = 10.0f;
    constexpr double floorOffset = -0.1;
    const float lowY = static_cast<float>(
        static_cast<long double>(cameraPosition.y) + static_cast<long double>(floorOffset));
    const float highY = static_cast<float>(
        static_cast<long double>(cameraPosition.y) + static_cast<long double>(extentPos));
    return {{
        {cameraPosition.x + extentNeg, highY, cameraPosition.z + extentPos},
        {cameraPosition.x + extentPos, highY, cameraPosition.z + extentPos},
        {cameraPosition.x + extentPos, highY, cameraPosition.z + extentNeg},
        {cameraPosition.x + extentNeg, highY, cameraPosition.z + extentNeg},
        {cameraPosition.x + extentNeg, lowY,  cameraPosition.z + extentPos},
        {cameraPosition.x + extentPos, lowY,  cameraPosition.z + extentPos},
        {cameraPosition.x + extentPos, lowY,  cameraPosition.z + extentNeg},
        {cameraPosition.x + extentNeg, lowY,  cameraPosition.z + extentNeg},
    }};
}

SkyBoxSetup BuildSkyBoxPlan(const CD3DVECTOR& cameraPosition,
                                       const TransformMatrix& combinedMatrix,
                                       const SkyBoxTextures& textures) {
    SkyBoxSetup out{};
    out.positions = BuildSkyBoxPositions(cameraPosition);
    for (std::size_t i = 0; i < out.positions.size(); ++i)
        out.projected[i] = ProjectPosition(out.positions[i], combinedMatrix);
    out.faces = GetSkyBoxFaces();
    out.batches = {{
        {SkyBoxSurface::Side, textures.side, 1u, 5u, 8, BlendMode::Opaque},
        {SkyBoxSurface::Top, textures.top, 1u, 5u, 2, BlendMode::Opaque},
        {SkyBoxSurface::Bottom, textures.bottom, 1u, 5u, 2, BlendMode::Opaque},
    }};
    return out;
}

ProjectedPosition ProjectPosition(
    const CD3DVECTOR& source, const TransformMatrix& matrix) {

    const long double w = X87Sum4Extended(
        source.x, matrix[3], source.y, matrix[7],
        source.z, matrix[11], matrix[15]);
    const float rhw = static_cast<float>(1.0L / w);

    const long double rhw80 = static_cast<long double>(rhw);
    ProjectedPosition out;
    out.rhw = rhw;
    out.x = static_cast<float>(X87Sum4Extended(
        source.x, matrix[0], source.y, matrix[4],
        source.z, matrix[8], matrix[12]) * rhw80);
    out.y = static_cast<float>(X87Sum4Extended(
        source.x, matrix[1], source.y, matrix[5],
        source.z, matrix[9], matrix[13]) * rhw80);
    out.z = static_cast<float>(X87Sum4Extended(
        source.x, matrix[2], source.y, matrix[6],
        source.z, matrix[10], matrix[14]) * rhw80);
    return out;
}

bool BuildImmediateTriangles(
    const CD3DPOLYGON* faces, std::size_t faceStorageCount,
    std::int16_t triangleCount,
    const TransformedVertex* transformed, std::size_t transformedCount,
    std::uint8_t effectiveState1DMode,
    std::vector<ImmediateVertex>& scratch) {
    if (triangleCount <= 0)
        return true;
    const std::size_t count = static_cast<std::size_t>(triangleCount);
    if (faces == nullptr || transformed == nullptr || faceStorageCount < count ||
        scratch.size() < count * 3u)
        return false;

    for (std::size_t faceIndex = 0; faceIndex < count; ++faceIndex) {
        const CD3DPOLYGON& face = faces[faceIndex];
        const std::int16_t indices[3] = {face.p1, face.p2, face.p3};
        const float us[3] = {face.u1, face.u2, face.u3};
        const float vs[3] = {face.v1, face.v2, face.v3};
        for (std::size_t corner = 0; corner < 3; ++corner) {
            const std::int16_t signedIndex = indices[corner];
            if (signedIndex < 0 || static_cast<std::size_t>(signedIndex) >= transformedCount)
                return false;
            const TransformedVertex& source =
                transformed[static_cast<std::size_t>(signedIndex)];
            ImmediateVertex& destination = scratch[faceIndex * 3u + corner];

            destination.x = source.x;
            destination.y = source.y;
            destination.z = source.z;
            destination.rhw = source.rhw;
            destination.u0 = us[corner];
            destination.v0 = vs[corner];
            destination.specular = source.specular;

            if (effectiveState1DMode == 0 || effectiveState1DMode == 1)
                destination.diffuse = face.diffuse;
            else if (effectiveState1DMode == 5)
                destination.diffuse = 0x00FFFFFFu;
            else
                destination.diffuse = source.diffuse;

        }
    }
    return true;
}

bool BuildMode4SecondPass(
    const CD3DPOLYGON* faces, std::size_t faceStorageCount,
    std::int16_t triangleCount,
    const ObjectVertex* sourceVertices, std::size_t sourceVertexCount,
    TransformedVertex* transformed, std::size_t transformedCount,
    const CD3DMATRIX& view,
    std::vector<ImmediateVertex>& scratch) {

    if (triangleCount <= 0)
        return true;
    const std::size_t count = static_cast<std::size_t>(triangleCount);
    if (count > static_cast<std::size_t>(TriangleDraw::MaxTriangles) ||
        faces == nullptr || sourceVertices == nullptr || transformed == nullptr ||
        faceStorageCount < count || scratch.size() < count * 3u)
        return false;

    const auto halfCoefficient = [](float value) {
        return static_cast<float>(static_cast<long double>(value) * 0.5L);
    };
    const float ux = halfCoefficient(view.m[0]);
    const float uy = halfCoefficient(view.m[4]);
    const float uz = halfCoefficient(view.m[8]);
    const float vx = halfCoefficient(view.m[1]);
    const float vy = halfCoefficient(view.m[5]);
    const float vz = halfCoefficient(view.m[9]);

    std::array<std::uint8_t, 0x200> touched{};

    const auto prepareVertex = [&](std::int16_t signedIndex) -> bool {
        if (signedIndex < 0)
            return false;
        const std::size_t index = static_cast<std::size_t>(signedIndex);
        if (index >= touched.size() || index >= sourceVertexCount ||
            index >= transformedCount)
            return false;
        if (touched[index] != 0)
            return true;

        const ObjectVertex& source = sourceVertices[index];
        TransformedVertex& destination = transformed[index];
        const long double nx = static_cast<long double>(source.normalX);
        const long double ny = static_cast<long double>(source.normalY);
        const long double nz = static_cast<long double>(source.normalZ);

        destination.environmentU = static_cast<float>(
            0.5L + nx * static_cast<long double>(ux) +
            ny * static_cast<long double>(uy) +
            nz * static_cast<long double>(uz));
        destination.environmentV = static_cast<float>(
            0.5L - nx * static_cast<long double>(vx) -
            ny * static_cast<long double>(vy) -
            nz * static_cast<long double>(vz));
        touched[index] = 1;
        return true;
    };

    for (std::size_t faceIndex = 0; faceIndex < count; ++faceIndex) {
        const CD3DPOLYGON& face = faces[faceIndex];
        const std::int16_t indices[3] = {face.p1, face.p2, face.p3};
        for (std::size_t corner = 0; corner < 3; ++corner) {
            if (!prepareVertex(indices[corner]))
                return false;
            const std::size_t index = static_cast<std::size_t>(indices[corner]);
            const TransformedVertex& source = transformed[index];
            ImmediateVertex& destination = scratch[faceIndex * 3u + corner];
            destination.x = source.x;
            destination.y = source.y;
            destination.z = source.z;
            destination.rhw = source.rhw;
            destination.diffuse = 0x00404040u;
            destination.specular = 0xFF000000u;
            destination.u0 = source.environmentU;
            destination.v0 = source.environmentV;

        }
    }
    return true;
}

DeferredTriangleQueue::DeferredTriangleQueue()
    : entries_(MaxTriangles) {}

void DeferredTriangleQueue::Reset() {

    count_ = 0;
}

const DeferredTriangle& DeferredTriangleQueue::Entry(std::uint16_t index) const {
    return entries_.at(index);
}

bool DeferredTriangleQueue::QueuePolyTriangles(
    const CD3DPOLYGON* faces, std::size_t faceStorageCount,
    std::int16_t triangleCount,
    const TransformedVertex* transformed, std::size_t transformedCount,
    std::uint8_t effectiveState1DMode,
    std::uint8_t textureIndex,
    std::size_t* queued) {
    if (queued)
        *queued = 0;
    if (triangleCount <= 0)
        return true;
    const std::size_t requested = static_cast<std::size_t>(triangleCount);
    if (faces == nullptr || transformed == nullptr || faceStorageCount < requested)
        return false;

    const std::size_t available = MaxTriangles - count_;
    const std::size_t toQueue = std::min(requested, available);

    for (std::size_t i = 0; i < toQueue; ++i) {
        const CD3DPOLYGON& face = faces[i];
        const std::int16_t indices[3] = {face.p1, face.p2, face.p3};
        for (std::int16_t index : indices) {
            if (index < 0 || static_cast<std::size_t>(index) >= transformedCount)
                return false;
        }
    }

    constexpr float kOneThird = 0.3333333432674407958984375f;
    for (std::size_t faceIndex = 0; faceIndex < toQueue; ++faceIndex) {

        DeferredTriangle& entry = entries_[count_++];
        const CD3DPOLYGON& face = faces[faceIndex];
        const std::int16_t indices[3] = {face.p1, face.p2, face.p3};
        const float us[3] = {face.u1, face.u2, face.u3};
        const float vs[3] = {face.v1, face.v2, face.v3};

        for (std::size_t corner = 0; corner < 3; ++corner) {
            const TransformedVertex& source =
                transformed[static_cast<std::size_t>(indices[corner])];
            ImmediateVertex& destination = entry.vertices[corner];
            destination.x = source.x;
            destination.y = source.y;
            destination.z = source.z;
            destination.rhw = source.rhw;
            destination.u0 = us[corner];
            destination.v0 = vs[corner];
            destination.specular = source.specular;

            if (effectiveState1DMode == 0 || effectiveState1DMode == 1)
                destination.diffuse = face.diffuse;
            else if (effectiveState1DMode == 5)
                destination.diffuse = 0x00FFFFFFu;
            else
                destination.diffuse = source.diffuse;

            entry.depthAccumulator = static_cast<float>(
                static_cast<long double>(source.z) +
                static_cast<long double>(entry.depthAccumulator));
        }
        entry.depthAccumulator = static_cast<float>(
            static_cast<long double>(entry.depthAccumulator) *
            static_cast<long double>(kOneThird));
        entry.textureIndex = textureIndex;
        entry.textureVariant = 1;
        entry.blendMode = static_cast<std::uint8_t>(BlendMode::Alpha);

    }

    if (queued)
        *queued = toQueue;
    return true;
}

bool DeferredTriangleQueue::QueueParticleBillboard(
    float projectedX, float projectedY, float projectedZ, float rhw, float size,
    std::uint32_t color, std::uint8_t textureIndex, std::uint8_t textureVariant,
    std::uint8_t blendMode, std::int32_t screenWidth, std::size_t* queued) {
    if (queued)
        *queued = 0;

    constexpr float kScreenScale = 0.0012499999720603228f;
    const float scale = static_cast<float>(
        static_cast<long double>(screenWidth) * static_cast<long double>(kScreenScale));
    const long double radius = static_cast<long double>(scale) *
                               static_cast<long double>(size) *
                               static_cast<long double>(rhw);
    const float left = static_cast<float>(static_cast<long double>(projectedX) - radius);
    const float right = static_cast<float>(static_cast<long double>(projectedX) + radius);
    const float top = static_cast<float>(static_cast<long double>(projectedY) - radius);
    const float bottom = static_cast<float>(static_cast<long double>(projectedY) + radius);

    const auto fillVertex = [&](ImmediateVertex& vertex,
                                float x, float y, float u, float v) {
        vertex.x = x;
        vertex.y = y;
        vertex.z = projectedZ;
        vertex.rhw = rhw;
        vertex.diffuse = color;
        vertex.specular = 0xFF000000u;
        vertex.u0 = u;
        vertex.v0 = v;

    };
    const auto finishEntry = [&](DeferredTriangle& entry) {
        entry.depthAccumulator = rhw;
        entry.textureIndex = textureIndex;
        entry.textureVariant = textureVariant;
        entry.blendMode = blendMode;

    };

    if (count_ == MaxTriangles)
        return true;
    DeferredTriangle& first = entries_[count_++];
    fillVertex(first.vertices[0], left,  top,    0.0f, 0.0f);
    fillVertex(first.vertices[1], right, top,    1.0f, 0.0f);
    fillVertex(first.vertices[2], left,  bottom, 0.0f, 1.0f);
    finishEntry(first);
    if (queued)
        *queued = 1;

    if (count_ == MaxTriangles)
        return true;
    DeferredTriangle& second = entries_[count_++];
    fillVertex(second.vertices[0], right, top,    1.0f, 0.0f);
    fillVertex(second.vertices[1], right, bottom, 1.0f, 1.0f);
    fillVertex(second.vertices[2], left,  bottom, 0.0f, 1.0f);
    finishEntry(second);
    if (queued)
        *queued = 2;
    return true;
}

std::vector<std::uint16_t> DeferredTriangleQueue::FlushOrder() const {
    std::vector<std::uint16_t> result;
    result.reserve(count_);
    for (std::uint16_t i = 0; i < count_; ++i)
        result.push_back(i);
    return result;
}

std::vector<ProjectedPosition> ProjectPositions(
    const std::vector<CD3DVECTOR>& source, const TransformMatrix& matrix,
    std::int16_t vertexCount) {

    std::vector<ProjectedPosition> out;
    if (vertexCount <= 0)
        return out;
    const auto count = std::min<std::size_t>(
        static_cast<std::size_t>(vertexCount), source.size());
    out.reserve(count);
    for (std::size_t i = 0; i < count; ++i)
        out.push_back(ProjectPosition(source[i], matrix));
    return out;
}

RendererState::RendererState() = default;

bool RendererState::BeginScene(RenderDevice& backend) {
    if (!deviceAvailable_ || !viewportAvailable_ || sceneInProgress_)
        return false;
    if (!backend.BeginScene())
        return false;
    sceneInProgress_ = true;
    deferredQueue_.Reset();
    hasFrameTransforms_ = false;

    return true;
}

bool RendererState::BeginScene(RenderDevice& backend,
                                     const FrameSetup& setup) {
    if (!BeginScene(backend))
        return false;
    frameTransforms_ = BuildFrameTransforms(setup);
    frameCameraPosition_ = setup.cameraPosition;
    frameScreenWidth_ = setup.screenWidth;
    renderRange_ = setup.farClip;
    hasFrameTransforms_ = true;
    ApplyFrameTransforms(frameTransforms_, backend);
    return true;
}

bool RendererState::EndScene(RenderDevice& backend) {
    if (!deviceAvailable_ || !viewportAvailable_ || !sceneInProgress_)
        return false;
    if (!backend.EndScene())
        return false;
    sceneInProgress_ = false;
    return true;
}

void RendererState::SelectTexture(std::uint8_t textureIndex,
                                        std::uint8_t variant,
                                        RenderDevice& backend) {
    if (textureIndex == textureIndex_ && variant == textureVariant_)
        return;
    textureIndex_ = textureIndex;
    textureVariant_ = variant;
    backend.SetTexture(textureIndex, variant);
}

BlendMode RendererState::SetBlendMode(BlendMode requested,
                                             bool selectedTextureHasAlpha,
                                             RenderDevice& backend) {
    auto effective = requested;

    if (requested == BlendMode::Opaque &&
        (selectedTextureHasAlpha || globalAlpha_ != 1.0f))
        effective = BlendMode::Alpha;
    else if (requested == BlendMode::Additive &&
             (selectedTextureHasAlpha || globalAlpha_ != 1.0f))
        effective = BlendMode::AlphaAdditive;

    if (effective == blendMode_)
        return effective;
    blendMode_ = effective;

    switch (effective) {
    case BlendMode::Opaque:
        backend.SetRenderState(D3DStateId::AlphaBlendEnable, 0);
        backend.SetRenderState(D3DStateId::SrcBlend, D3DBlendValue::One);
        backend.SetRenderState(D3DStateId::DestBlend, D3DBlendValue::Zero);
        break;
    case BlendMode::Additive:
        backend.SetRenderState(D3DStateId::AlphaBlendEnable, 1);
        backend.SetRenderState(D3DStateId::SrcBlend, D3DBlendValue::One);
        backend.SetRenderState(D3DStateId::DestBlend, D3DBlendValue::One);
        break;
    case BlendMode::Alpha:
        backend.SetRenderState(D3DStateId::AlphaBlendEnable, 1);
        backend.SetRenderState(D3DStateId::SrcBlend, D3DBlendValue::SrcAlpha);
        backend.SetRenderState(D3DStateId::DestBlend, D3DBlendValue::InvSrcAlpha);
        break;
    case BlendMode::AlphaAdditive:
        backend.SetRenderState(D3DStateId::AlphaBlendEnable, 1);
        backend.SetRenderState(D3DStateId::SrcBlend, D3DBlendValue::SrcAlpha);
        backend.SetRenderState(D3DStateId::DestBlend, D3DBlendValue::One);
        break;
    }
    return effective;
}

void RendererState::SetGlobalAlpha(float alpha) {
    if (alpha != globalAlpha_)
        globalAlpha_ = alpha;
}

void RendererState::SetZWrite(bool enabled, RenderDevice& backend) {
    if (enabled == zWriteEnabled_)
        return;
    zWriteEnabled_ = enabled;
    backend.SetRenderState(D3DStateId::ZWriteEnable, enabled ? 1u : 0u);
}

SphereVisibility RendererState::ClassifySphere(
    const CD3DVECTOR& center, float radius, bool objectTest,
    RenderDevice& backend) const {

    const float dx = static_cast<float>(
        static_cast<long double>(center.x) - frameCameraPosition_.x);
    const float dz = static_cast<float>(
        static_cast<long double>(center.z) - frameCameraPosition_.z);
    const float distanceSquared = static_cast<float>(
        static_cast<long double>(dz) * dz +
        static_cast<long double>(dx) * dx);
    const long double range = renderRange_;
    const long double rangeSquared = range * range;

    if (objectTest) {

        if (!(rangeSquared > static_cast<long double>(distanceSquared)))
            return SphereVisibility::Outside;
    } else if (!(rangeSquared > static_cast<long double>(distanceSquared))) {

        const long double expanded = range + static_cast<long double>(radius);
        if (!(expanded * expanded > static_cast<long double>(distanceSquared)))
            return SphereVisibility::Outside;
        return SphereVisibility::Intersect;
    }

    const std::uint32_t status = backend.ComputeSphereVisibility(center, radius);
    constexpr std::uint32_t kOutsideMask = 0x00000828u;
    constexpr std::uint32_t kIntersectMask = 0x00000414u;
    if ((status & kOutsideMask) != 0)
        return SphereVisibility::Outside;
    if ((status & kIntersectMask) != 0)
        return SphereVisibility::Intersect;
    return SphereVisibility::Inside;
}

bool RendererState::PrepareSkyBox(const CD3DVECTOR& cameraPosition,
                                        const SkyBoxTextures& textures,
                                        RenderDevice& backend,
                                        SkyBoxSetup& out) {
    if (!sceneInProgress_ || !hasFrameTransforms_)
        return false;
    SetZWrite(false, backend);
    TransformMatrix matrix{};
    for (std::size_t i = 0; i < matrix.size(); ++i)
        matrix[i] = frameTransforms_.combined.m[i];
    out = BuildSkyBoxPlan(cameraPosition, matrix, textures);
    return true;
}

bool RendererState::SubmitTriangles(
    const CD3DPOLYGON* faces, std::size_t faceStorageCount,
    std::int16_t triangleCount,
    const ObjectVertex* sourceVertices, std::size_t sourceVertexCount,
    TransformedVertex* transformed, std::size_t transformedCount,
    std::uint8_t textureIndex, std::uint8_t requestedState1DMode,
    RenderDevice& backend) {

    if (!sceneInProgress_)
        return false;
    if (triangleCount <= 0)
        return true;
    if (triangleCount > TriangleDraw::MaxTriangles ||
        faces == nullptr || transformed == nullptr)
        return false;

    SelectTexture(textureIndex, 1u, backend);
    const std::uint8_t effectiveMode = SetState1DMode(
        requestedState1DMode, optionFlag4E_, optionFlag51_, backend);
    SetGlobalAlpha(1.0f);
    const BlendMode effectiveBlend = SetBlendMode(
        BlendMode::Opaque, backend.TextureHasAlpha(textureIndex_), backend);

    if (effectiveBlend == BlendMode::Alpha) {

        return deferredQueue_.QueuePolyTriangles(
            faces, faceStorageCount, triangleCount,
            transformed, transformedCount, effectiveMode, textureIndex, nullptr);
    }

    if (!BuildImmediateTriangles(
            faces, faceStorageCount, triangleCount,
            transformed, transformedCount, effectiveMode, immediateScratch_))
        return false;
    backend.DrawPrimitive(TriangleDraw::PrimitiveType,
                          TriangleDraw::VertexTypeDesc,
                          immediateScratch_.data(),
                          TriangleDraw::VertexCount(triangleCount),
                          TriangleDraw::DrawFlags);

    if (effectiveMode != 4u)
        return true;
    if (!hasFrameTransforms_ || sourceVertices == nullptr)
        return false;

    SelectTexture(environmentTexture_, 1u, backend);
    SetBlendMode(BlendMode::Additive,
                 backend.TextureHasAlpha(environmentTexture_), backend);
    if (!BuildMode4SecondPass(
            faces, faceStorageCount, triangleCount,
            sourceVertices, sourceVertexCount,
            transformed, transformedCount, frameTransforms_.view,
            immediateScratch_))
        return false;
    backend.DrawPrimitive(TriangleDraw::PrimitiveType,
                          TriangleDraw::VertexTypeDesc,
                          immediateScratch_.data(),
                          TriangleDraw::VertexCount(triangleCount),
                          TriangleDraw::DrawFlags);
    return true;
}

bool RendererState::RenderSkyBox(const CD3DVECTOR& cameraPosition,
                                       const SkyBoxTextures& textures,
                                       RenderDevice& backend,
                                       SkyBoxSetup* capturedPlan) {
    SkyBoxSetup plan{};
    if (!PrepareSkyBox(cameraPosition, textures, backend, plan))
        return false;

    std::array<TransformedVertex, 8> transformed{};
    for (std::size_t i = 0; i < transformed.size(); ++i) {
        transformed[i].x = plan.projected[i].x;
        transformed[i].y = plan.projected[i].y;
        transformed[i].z = plan.projected[i].z;
        transformed[i].rhw = plan.projected[i].rhw;
        transformed[i].specular = plan.transformedSpecular;
    }

    const auto submit = [&](const CD3DPOLYGON* faces, std::size_t count,
                            const SkyBoxBatch& batch) {
        return SubmitTriangles(faces, count, batch.triangleCount,
                               nullptr, 0, transformed.data(), transformed.size(),
                               batch.textureIndex, batch.state1DMode, backend);
    };
    if (!submit(plan.faces.sides.data(), plan.faces.sides.size(), plan.batches[0]) ||
        !submit(plan.faces.top.data(), plan.faces.top.size(), plan.batches[1]) ||
        !submit(plan.faces.bottom.data(), plan.faces.bottom.size(), plan.batches[2]))
        return false;
    if (capturedPlan != nullptr)
        *capturedPlan = plan;
    return true;
}

bool RendererState::QueueParticleBillboard(
    float projectedX, float projectedY, float projectedZ, float rhw, float size,
    std::uint32_t color, std::uint8_t textureIndex, std::uint8_t textureVariant,
    std::uint8_t blendMode, std::size_t* queued) {
    if (queued)
        *queued = 0;

    if (!sceneInProgress_ || !hasFrameTransforms_)
        return false;
    return deferredQueue_.QueueParticleBillboard(
        projectedX, projectedY, projectedZ, rhw, size, color,
        textureIndex, textureVariant, blendMode, frameScreenWidth_, queued);
}

bool RendererState::FlushDeferredTriangles(RenderDevice& backend) {

    if (deferredQueue_.Count() == 0)
        return true;
    if (!sceneInProgress_)
        return false;

    SetGlobalAlpha(1.0f);
    SetZWrite(false, backend);
    const auto order = deferredQueue_.FlushOrder();
    for (std::uint16_t index : order) {
        const DeferredTriangle& entry = deferredQueue_.Entry(index);
        if (entry.blendMode > static_cast<std::uint8_t>(BlendMode::AlphaAdditive))
            return false;
        SelectTexture(entry.textureIndex, entry.textureVariant, backend);
        SetBlendMode(static_cast<BlendMode>(entry.blendMode),
                     backend.TextureHasAlpha(entry.textureIndex), backend);
        backend.DrawPrimitive(DeferredFlushSpec::PrimitiveType,
                              DeferredFlushSpec::VertexTypeDesc,
                              entry.vertices.data(),
                              DeferredFlushSpec::VertexCount,
                              DeferredFlushSpec::DrawFlags);
    }
    return true;
}

std::uint8_t RendererState::SetState1DMode(std::uint8_t requested,
                                                  bool flag4E,
                                                  bool flag51,
                                                  RenderDevice& backend) {

    std::uint8_t effective = requested;
    if (effective == 4 && !flag4E)
        effective = 3;
    if (effective == 3 && !flag51)
        effective = 2;
    else if (effective == 1 && !flag51)
        effective = 0;

    if (effective == state1DMode_)
        return effective;
    state1DMode_ = effective;

    const bool enabled = effective == 1 || effective == 3 ||
                         (effective == 4 && flag51);
    backend.SetRenderState(D3DStateId::State1D, enabled ? 1u : 0u);
    return effective;
}

}


namespace flydemo {
namespace {

bool Resolve2DTexture(std::string_view textureName,
                      CBMManager& textures, CBMLoader& loader,
                      std::uint8_t& textureIndex,
                      const CBMPicture*& picture) {
    if (!textures.Contains(textureName)) {
        if (textures.Load(textureName, loader) == CBMManager::InvalidTexture)
            return false;
    }
    textureIndex = textures.IndexOf(textureName);
    if (textureIndex == CBMManager::InvalidTexture)
        return false;
    picture = textures.Get(textureIndex);
    return picture != nullptr;
}

std::array<ImmediateVertex, 4> Build2DQuad(
    std::int32_t x1, std::int32_t y1,
    std::int32_t x2, std::int32_t y2,
    std::uint32_t color) {
    std::array<ImmediateVertex, 4> scratch{};
    for (auto& v : scratch) {
        v.z = 9.9999997473787516e-06f;
        v.rhw = 1.0f;
        v.diffuse = color;
        v.specular = 0xFF000000u;
    }
    scratch[0].x = static_cast<float>(x1); scratch[0].y = static_cast<float>(y1);
    scratch[0].u0 = 0.0f; scratch[0].v0 = 0.0f;
    scratch[1].x = static_cast<float>(x2); scratch[1].y = static_cast<float>(y1);
    scratch[1].u0 = 1.0f; scratch[1].v0 = 0.0f;
    scratch[2].x = static_cast<float>(x2); scratch[2].y = static_cast<float>(y2);
    scratch[2].u0 = 1.0f; scratch[2].v0 = 1.0f;
    scratch[3].x = static_cast<float>(x1); scratch[3].y = static_cast<float>(y2);
    scratch[3].u0 = 0.0f; scratch[3].v0 = 1.0f;
    return scratch;
}

void Apply2DState(std::uint8_t textureIndex,
                  std::uint8_t textureVariant,
                  BlendMode requestedBlend,
                  RendererState& renderer,
                  RenderDevice& backend) {
    renderer.SelectTexture(textureIndex, textureVariant, backend);
    renderer.SetGlobalAlpha(1.0f);
    renderer.SetZWrite(false, backend);
    renderer.SetBlendMode(requestedBlend,
                          backend.TextureHasAlpha(textureIndex), backend);
}

}

bool DrawTexturedRect(std::string_view textureName,
                      std::uint8_t textureVariant,
                      std::int32_t x1, std::int32_t y1,
                      std::int32_t x2, std::int32_t y2,
                      float unusedScale,
                      BlendMode requestedBlend,
                      std::uint32_t color,
                      CBMManager& textures, CBMLoader& loader,
                      RendererState& renderer,
                      RenderDevice& backend) {
    (void)unusedScale;
    if (!renderer.SceneInProgress())
        return false;

    std::uint8_t textureIndex = CBMManager::InvalidTexture;
    const CBMPicture* picture = nullptr;
    if (!Resolve2DTexture(textureName, textures, loader, textureIndex, picture))
        return false;
    (void)picture;

    Apply2DState(textureIndex, textureVariant, requestedBlend, renderer, backend);
    const auto quad = Build2DQuad(x1, y1, x2, y2, color);
    backend.DrawPrimitive(6u, 0x000002C4u, quad.data(), 4u, 8u);
    return true;
}

bool DrawTexturedImage(std::string_view textureName,
                       std::uint8_t textureVariant,
                       std::int32_t x, std::int32_t y,
                       float unusedScale,
                       BlendMode requestedBlend,
                       std::uint32_t color,
                       CBMManager& textures, CBMLoader& loader,
                       RendererState& renderer,
                       RenderDevice& backend) {
    (void)unusedScale;
    if (!renderer.SceneInProgress())
        return false;

    std::uint8_t textureIndex = CBMManager::InvalidTexture;
    const CBMPicture* picture = nullptr;
    if (!Resolve2DTexture(textureName, textures, loader, textureIndex, picture))
        return false;

    Apply2DState(textureIndex, textureVariant, requestedBlend, renderer, backend);
    const std::int32_t x2 = x + static_cast<std::int32_t>(picture->Width());
    const std::int32_t y2 = y + static_cast<std::int32_t>(picture->Height());
    const auto quad = Build2DQuad(x, y, x2, y2, color);
    backend.DrawPrimitive(6u, 0x000002C4u, quad.data(), 4u, 8u);
    return true;
}

}
