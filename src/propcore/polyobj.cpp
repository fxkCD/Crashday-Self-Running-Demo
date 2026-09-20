#include "polyobj.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>

namespace flydemo {
namespace {
class Reader {
public:
    explicit Reader(const std::vector<std::uint8_t>& b) : b_(b) {}
    bool U8(std::uint8_t& v) {
        if (p_ >= b_.size()) return false;
        v = b_[p_++]; return true;
    }
    bool U16(std::uint16_t& v) {
        if (b_.size() - p_ < 2) return false;
        v = static_cast<std::uint16_t>(b_[p_]) |
            (static_cast<std::uint16_t>(b_[p_ + 1]) << 8);
        p_ += 2; return true;
    }
    bool U32(std::uint32_t& v) {
        if (b_.size() - p_ < 4) return false;
        v = static_cast<std::uint32_t>(b_[p_]) |
            (static_cast<std::uint32_t>(b_[p_ + 1]) << 8) |
            (static_cast<std::uint32_t>(b_[p_ + 2]) << 16) |
            (static_cast<std::uint32_t>(b_[p_ + 3]) << 24);
        p_ += 4; return true;
    }
    bool I16(std::int16_t& v) {
        std::uint16_t x = 0; if (!U16(x)) return false;
        v = static_cast<std::int16_t>(x); return true;
    }
    bool F32(float& v) {
        if (b_.size() - p_ < 4) return false;
        std::uint32_t x = static_cast<std::uint32_t>(b_[p_]) |
                          (static_cast<std::uint32_t>(b_[p_ + 1]) << 8) |
                          (static_cast<std::uint32_t>(b_[p_ + 2]) << 16) |
                          (static_cast<std::uint32_t>(b_[p_ + 3]) << 24);
        std::memcpy(&v, &x, sizeof(v)); p_ += 4; return true;
    }
    bool Raw(std::uint8_t* dst, std::size_t n) {
        if (n > b_.size() - p_) return false;
        std::copy_n(b_.data() + p_, n, dst); p_ += n; return true;
    }
    bool Tag3(char a, char b, char c) {
        std::uint8_t x=0,y=0,z=0;
        return U8(x) && U8(y) && U8(z) && x==static_cast<std::uint8_t>(a) &&
               y==static_cast<std::uint8_t>(b) && z==static_cast<std::uint8_t>(c);
    }
    std::size_t Pos() const { return p_; }
private:
    const std::vector<std::uint8_t>& b_;
    std::size_t p_ = 0;
};

bool Fail(std::string* error, const char* msg) {
    if (error) *error = msg;
    return false;
}

std::string FixedCString18(const std::array<std::uint8_t, 18>& raw) {
    const auto it = std::find(raw.begin(), raw.end(), std::uint8_t{0});
    return std::string(reinterpret_cast<const char*>(raw.data()),
                       static_cast<std::size_t>(it - raw.begin()));
}

CD3DVECTOR VertexPosition(const ObjectVertex& v) {
    return {v.positionX, v.positionY, v.positionZ};
}

void SetVertexPosition(ObjectVertex& v, const CD3DVECTOR& p) {
    v.positionX = p.x;
    v.positionY = p.y;
    v.positionZ = p.z;
}

CD3DVECTOR VertexNormal(const ObjectVertex& v) {
    return {v.normalX, v.normalY, v.normalZ};
}

void SetVertexNormal(ObjectVertex& v, const CD3DVECTOR& n) {
    v.normalX = n.x;
    v.normalY = n.y;
    v.normalZ = n.z;
}

CD3DVECTOR FaceNormal(const CD3DPOLYGON& f) {
    return {f.normalX, f.normalY, f.normalZ};
}

void SetFaceNormal(CD3DPOLYGON& f, const CD3DVECTOR& n) {
    f.normalX = n.x;
    f.normalY = n.y;
    f.normalZ = n.z;
}

CD3DVECTOR Cross(const CD3DVECTOR& a, const CD3DVECTOR& b) {
    return {
        static_cast<float>(static_cast<long double>(a.y) * b.z -
                           static_cast<long double>(a.z) * b.y),
        static_cast<float>(static_cast<long double>(a.z) * b.x -
                           static_cast<long double>(a.x) * b.z),
        static_cast<float>(static_cast<long double>(a.x) * b.y -
                           static_cast<long double>(a.y) * b.x),
    };
}

void NormalizeUnchecked(CD3DVECTOR& v) {

    const long double length = std::sqrt(
        static_cast<long double>(v.x) * v.x +
        static_cast<long double>(v.y) * v.y +
        static_cast<long double>(v.z) * v.z);
    const long double inv = 1.0L / length;
    v.x = static_cast<float>(static_cast<long double>(v.x) * inv);
    v.y = static_cast<float>(static_cast<long double>(v.y) * inv);
    v.z = static_cast<float>(static_cast<long double>(v.z) * inv);
}

std::uint16_t WrapAdd(std::uint16_t value, std::int16_t add) {
    return static_cast<std::uint16_t>(value + static_cast<std::uint16_t>(add));
}

std::int16_t WrappedThree(std::uint16_t a, std::int16_t b, std::int16_t c) {
    return static_cast<std::int16_t>(WrapAdd(WrapAdd(a, b), c));
}

RGBColor UnpackRGB(std::uint32_t c) {
    return {static_cast<std::uint8_t>(c),
            static_cast<std::uint8_t>(c >> 8),
            static_cast<std::uint8_t>(c >> 16)};
}

RGBColor AmbientFloor(RGBColor value, std::uint32_t packedAmbient) {
    const RGBColor ambient = UnpackRGB(packedAmbient);
    value.c0 = std::max(value.c0, ambient.c0);
    value.c1 = std::max(value.c1, ambient.c1);
    value.c2 = std::max(value.c2, ambient.c2);
    return value;
}

bool ValidFaceIndices(const CD3DPOLYGON& f, std::size_t vertexCount) {
    return f.p1 >= 0 && f.p2 >= 0 && f.p3 >= 0 &&
           static_cast<std::size_t>(f.p1) < vertexCount &&
           static_cast<std::size_t>(f.p2) < vertexCount &&
           static_cast<std::size_t>(f.p3) < vertexCount;
}

bool HasMagnitudeBits(float value) {
    std::uint32_t bits = 0;
    std::memcpy(&bits, &value, sizeof(bits));
    return (bits & 0x7FFFFFFFu) != 0;
}

CD3DMATRIX TranslationTransform(float x, float y, float z) {
    CD3DMATRIX out{};
    SetIdentity(out);
    out.m[12] = x;
    out.m[13] = y;
    out.m[14] = z;
    return out;
}

CD3DVECTOR FaceCentroid(const CD3DPOLYGON& f,
                  const std::vector<ObjectVertex>& vertices) {
    const CD3DVECTOR a = VertexPosition(vertices[static_cast<std::size_t>(f.p1)]);
    const CD3DVECTOR b = VertexPosition(vertices[static_cast<std::size_t>(f.p2)]);
    const CD3DVECTOR c = VertexPosition(vertices[static_cast<std::size_t>(f.p3)]);
    constexpr float oneThird = 1.0f / 3.0f;
    return {(a.x + b.x + c.x) * oneThird,
            (a.y + b.y + c.y) * oneThird,
            (a.z + b.z + c.z) * oneThird};
}

CD3DPLANE PlaneFromPoints(const CD3DVECTOR& p0, const CD3DVECTOR& p1, const CD3DVECTOR& p2) {

    const double ux = static_cast<double>(p1.x) - p0.x;
    const double uy = static_cast<double>(p1.y) - p0.y;
    const double uz = static_cast<double>(p1.z) - p0.z;
    const double vx = static_cast<double>(p2.x) - p0.x;
    const double vy = static_cast<double>(p2.y) - p0.y;
    const double vz = static_cast<double>(p2.z) - p0.z;
    double nx = uy * vz - uz * vy;
    double ny = uz * vx - ux * vz;
    double nz = ux * vy - uy * vx;
    const double length = std::sqrt(nx * nx + ny * ny + nz * nz);
    nx /= length;
    ny /= length;
    nz /= length;
    CD3DPLANE out;
    out.a = static_cast<float>(nx);
    out.b = static_cast<float>(ny);
    out.c = static_cast<float>(nz);
    out.d = static_cast<float>(static_cast<double>(out.a) * p0.x +
                               static_cast<double>(out.b) * p0.y +
                               static_cast<double>(out.c) * p0.z);
    return out;
}

void InitBoundsFaces(CD3DPOLYGONOBJECT& object) {

    static constexpr std::array<std::array<std::int16_t, 3>, 12> kIndices{{
        {{4,5,6}}, {{4,6,7}},
        {{0,2,1}}, {{0,3,2}},
        {{4,1,5}}, {{4,0,1}},
        {{7,6,2}}, {{7,2,3}},
        {{4,3,0}}, {{4,7,3}},
        {{5,1,2}}, {{5,2,6}}
    }};
    for (std::size_t i = 0; i < object.boundsFaces.size(); ++i) {
        CD3DPOLYGON face{};
        face.p1 = kIndices[i][0];
        face.p2 = kIndices[i][1];
        face.p3 = kIndices[i][2];
        face.u1 = 0.0f; face.v1 = 0.0f;
        face.u2 = 1.0f; face.v2 = 0.0f;
        face.u3 = 1.0f; face.v3 = 1.0f;
        object.boundsFaces[i] = face;
    }
}

void RecomputeRuntimeBounds(CD3DPOLYGONOBJECT& object) {
    CD3DVECTOR minimum = object.center;
    CD3DVECTOR maximum = object.center;
    for (const CD3DVECTOR& p : object.boundsPoints) {

        if (!(p.x >= minimum.x)) minimum.x = p.x;
        if (!(p.y >= minimum.y)) minimum.y = p.y;
        if (!(p.z >= minimum.z)) minimum.z = p.z;
        if (p.x > maximum.x) maximum.x = p.x;
        if (p.y > maximum.y) maximum.y = p.y;
        if (p.z > maximum.z) maximum.z = p.z;
    }
    object.boundsMin = minimum;
    object.boundsMax = maximum;
    const long double sx = object.sizeX;
    const long double sy = object.sizeY;
    const long double sz = object.sizeZ;
    object.boundRadius = static_cast<float>(
        std::sqrt((sx * sx + sy * sy + sz * sz) * 0.25L));

    for (std::size_t planeIndex = 0; planeIndex < object.boundsPlanes.size(); ++planeIndex) {
        const CD3DPOLYGON& face = object.boundsFaces[planeIndex * 2];
        const CD3DVECTOR& p0 = object.boundsPoints[static_cast<std::size_t>(face.p1)];
        const CD3DVECTOR& p1 = object.boundsPoints[static_cast<std::size_t>(face.p2)];
        const CD3DVECTOR& p2 = object.boundsPoints[static_cast<std::size_t>(face.p3)];
        CD3DPLANE plane = PlaneFromPoints(p0, p1, p2);
        plane.OrientPlane(object.center);
        object.boundsPlanes[planeIndex] = plane;
    }
}
}

CD3DVECTOR P3DModel::InitialCenter() const {

    return {sizeX * 0.5f, sizeY * 0.5f, -sizeZ * 0.5f};
}

std::array<CD3DVECTOR, 8> P3DModel::InitialBoundsCorners() const {

    const float x0 = 0.0f, x1 = sizeX;
    const float y0 = 0.0f, y1 = sizeY;
    const float z0 = 0.0f, z1 = -sizeZ;
    return {{{x0,y0,z0}, {x1,y0,z0}, {x1,y0,z1}, {x0,y0,z1},
             {x0,y1,z0}, {x1,y1,z0}, {x1,y1,z1}, {x0,y1,z1}}};
}

bool ParseP3DModel(const std::vector<std::uint8_t>& bytes,
                   P3DModel& out,
                   std::string* error) {
    out = {};
    Reader r(bytes);
    if (!r.Tag3('P','3','D')) return Fail(error, "Wrong P3D file format");
    if (!r.U8(out.version)) return Fail(error, "Truncated P3D version");
    if (out.version != 1) return Fail(error, "Wrong P3D version");
    if (!r.Raw(out.headerBytes.data(), out.headerBytes.size()))
        return Fail(error, "Truncated P3D header");

    if (!r.F32(out.sizeX) || !r.F32(out.sizeY) || !r.F32(out.sizeZ) ||
        !r.U8(out.postSizeByte))
        return Fail(error, "Truncated P3D dimensions");

    std::uint8_t groupCount = 0;
    if (!r.U8(groupCount)) return Fail(error, "Truncated P3D texture count");
    if (groupCount == 0) return Fail(error, "P3D NumTextures must be > 0");
    out.textureGroups.resize(groupCount);
    for (auto& g : out.textureGroups) {
        if (!r.U16(g.field12)) return Fail(error, "Truncated P3D texture group");
        std::array<std::uint8_t,18> name{};
        if (!r.Raw(name.data(), name.size())) return Fail(error, "Truncated P3D texture name");
        g.textureName = FixedCString18(name) + ".cbm";
        for (auto& f : g.fields14To1E)
            if (!r.I16(f)) return Fail(error, "Truncated P3D texture group fields");
    }

    std::int16_t vertexCount = 0;
    if (!r.I16(vertexCount)) return Fail(error, "Truncated P3D vertex count");
    if (vertexCount <= 0) return Fail(error, "P3D NumVertices must be > 0");
    out.vertices.resize(static_cast<std::size_t>(vertexCount));
    for (auto& v : out.vertices) {
        if (!r.F32(v.position.x) || !r.F32(v.position.y) || !r.F32(v.position.z))
            return Fail(error, "Truncated P3D vertices");
    }

    std::int16_t faceCount = 0;
    if (!r.I16(faceCount)) return Fail(error, "Truncated P3D polygon count");
    if (faceCount <= 0) return Fail(error, "P3D NumPolys must be > 0");
    out.faces.resize(static_cast<std::size_t>(faceCount));
    for (auto& f : out.faces) {
        if (!r.I16(f.p1) || !r.F32(f.u1) || !r.F32(f.v1) ||
            !r.I16(f.p2) || !r.F32(f.u2) || !r.F32(f.v2) ||
            !r.I16(f.p3) || !r.F32(f.u3) || !r.F32(f.v3))
            return Fail(error, "Truncated P3D faces");
        const auto n = static_cast<std::int32_t>(out.vertices.size());
        if (f.p1 < 0 || f.p1 >= n) return Fail(error, "P3D face P1 out of range");
        if (f.p2 < 0 || f.p2 >= n) return Fail(error, "P3D face P2 out of range");
        if (f.p3 < 0 || f.p3 >= n) return Fail(error, "P3D face P3 out of range");
    }

    std::int16_t materialCount = 0;
    if (!r.I16(materialCount)) return Fail(error, "Truncated P3D material count");
    if (materialCount < 0) return Fail(error, "Negative P3D material count");
    out.materials.resize(static_cast<std::size_t>(materialCount));
    for (auto& m : out.materials) {
        if (!r.F32(m.position.x) || !r.F32(m.position.y) || !r.F32(m.position.z) ||
            !r.F32(m.range) || !r.U32(m.packedColor) ||
            !r.U8(m.coronas) || !r.U8(m.lensFlares) || !r.U8(m.lightUp))
            return Fail(error, "Truncated P3D materials");
    }

    out.consumedBytes = r.Pos();
    if (error) error->clear();
    return true;
}

std::vector<P3DDrawBatch> BuildP3DDrawBatches(const P3DModel& model) {

    std::vector<P3DDrawBatch> batches;
    batches.reserve(model.textureGroups.size() * 6u);
    for (std::size_t groupIndex = 0; groupIndex < model.textureGroups.size(); ++groupIndex) {
        const auto& group = model.textureGroups[groupIndex];
        std::uint16_t cursor = group.field12;
        for (std::uint8_t mode = 0; mode < 6; ++mode) {
            const std::int16_t count = group.fields14To1E[mode];
            batches.push_back(P3DDrawBatch{
                groupIndex, mode, static_cast<std::int16_t>(cursor), count
            });

            cursor = static_cast<std::uint16_t>(
                cursor + static_cast<std::uint16_t>(count));
        }
    }
    return batches;
}

bool CD3DPOLYGONOBJECT::InitializeFromModel(const P3DModel& model,
                                           CBMManager& textures,
                                           CBMLoader& loader,
                                           std::string* error) {
    *this = CD3DPOLYGONOBJECT{};
    sizeX = model.sizeX;
    sizeY = model.sizeY;
    sizeZ = model.sizeZ;
    lightingMode = 0;

    textureGroups.reserve(model.textureGroups.size());
    for (const auto& source : model.textureGroups) {
        P3DBoundTextureGroup group;
        group.textureName = source.textureName;
        group.textureIndex = textures.Load(group.textureName, loader);
        group.textureSlotState = P3DTextureSlotState::Bound;
        group.firstFace = source.field12;
        group.modeCounts = source.fields14To1E;
        textureGroups.push_back(std::move(group));
    }

    sourceVertices.resize(model.vertices.size());
    transformedVertices.resize(model.vertices.size());
    for (std::size_t i = 0; i < model.vertices.size(); ++i)
        SetVertexPosition(sourceVertices[i], model.vertices[i].position);

    faces.resize(model.faces.size());
    for (std::size_t i = 0; i < model.faces.size(); ++i) {
        const P3DFace& in = model.faces[i];
        CD3DPOLYGON& out = faces[i];
        out.p1 = in.p1; out.p2 = in.p2; out.p3 = in.p3;
        out.u1 = in.u1; out.v1 = in.v1;
        out.u2 = in.u2; out.v2 = in.v2;
        out.u3 = in.u3; out.v3 = in.v3;
    }

    embeddedLights.resize(model.materials.size());
    for (std::size_t i = 0; i < model.materials.size(); ++i) {
        const P3DMaterial& in = model.materials[i];
        CD3DLIGHT& out = embeddedLights[i];
        out.position = in.position;
        out.range = in.range;
        out.packedColor = in.packedColor;
        out.cachedVectorLength = in.cachedVectorLength;
        out.coronas = in.coronas != 0;
        out.lensFlares = in.lensFlares != 0;
        out.scale = in.scale;
        out.lightUp = in.lightUp != 0;
    }

    center = model.InitialCenter();
    boundsPoints = model.InitialBoundsCorners();
    InitBoundsFaces(*this);
    forward = {0.0f, 0.0f, 1.0f};
    up = {0.0f, 1.0f, 0.0f};
    right = {1.0f, 0.0f, 0.0f};
    SetIdentity(pendingTransform);
    transformState = 0;
    RecomputeRuntimeBounds(*this);
    if (error) error->clear();
    return true;
}

bool CD3DPOLYGONOBJECT::CopyFromNative(const CD3DPOLYGONOBJECT* source,
                                      std::string* error) {

    if (source == nullptr)
        return Fail(error, "P3D native Copy source is null");

    sizeX = source->sizeX;
    sizeY = source->sizeY;
    sizeZ = source->sizeZ;

    textureGroups.clear();
    textureGroups.reserve(source->textureGroups.size());
    for (const P3DBoundTextureGroup& src : source->textureGroups) {
        P3DBoundTextureGroup dst;
        dst.textureName = src.textureName;
        dst.firstFace = src.firstFace;
        dst.modeCounts = src.modeCounts;

        dst.textureIndex = CBMManager::InvalidTexture;
        dst.textureSlotState = P3DTextureSlotState::IndeterminateNativeCopy;
        textureGroups.push_back(std::move(dst));
    }

    sourceVertices = source->sourceVertices;
    transformedVertices.clear();
    transformedVertices.resize(source->sourceVertices.size());

    faces = source->faces;
    embeddedLights = source->embeddedLights;

    forward = {0.0f, 0.0f, 1.0f};
    up = {0.0f, 1.0f, 0.0f};
    right = {1.0f, 0.0f, 0.0f};
    center = {sizeX * 0.5f, sizeY * 0.5f, -sizeZ * 0.5f};
    boundsPoints = {{{0.0f, 0.0f, 0.0f},
                     {sizeX, 0.0f, 0.0f},
                     {sizeX, 0.0f, -sizeZ},
                     {0.0f, 0.0f, -sizeZ},
                     {0.0f, sizeY, 0.0f},
                     {sizeX, sizeY, 0.0f},
                     {sizeX, sizeY, -sizeZ},
                     {0.0f, sizeY, -sizeZ}}};
    InitBoundsFaces(*this);
    SetIdentity(pendingTransform);
    RecomputeRuntimeBounds(*this);
    if (error) error->clear();
    return true;
}

void CD3DPOLYGONOBJECT::UnloadRuntime() {

    textureGroups.clear();
    sourceVertices.clear();
    transformedVertices.clear();
    faces.clear();
    embeddedLights.clear();
    sizeX = 0.0f;
    sizeY = 0.0f;
    sizeZ = 0.0f;
}

bool CD3DPOLYGONOBJECT::RebuildGeometry(std::string* error) {

    for (CD3DPOLYGON& face : faces) {
        if (!ValidFaceIndices(face, sourceVertices.size()))
            return Fail(error, "P3D runtime face index out of range");
        const CD3DVECTOR v0 = VertexPosition(sourceVertices[static_cast<std::size_t>(face.p1)]);
        const CD3DVECTOR v1 = VertexPosition(sourceVertices[static_cast<std::size_t>(face.p2)]);
        const CD3DVECTOR v2 = VertexPosition(sourceVertices[static_cast<std::size_t>(face.p3)]);
        CD3DVECTOR a{v0.x-v1.x, v0.y-v1.y, v0.z-v1.z};
        CD3DVECTOR b{v2.x-v1.x, v2.y-v1.y, v2.z-v1.z};
        CD3DVECTOR normal = Cross(a, b);
        NormalizeUnchecked(normal);
        SetFaceNormal(face, normal);
    }

    for (ObjectVertex& vertex : sourceVertices)
        SetVertexNormal(vertex, {0.0f, 0.0f, 0.0f});

    for (const P3DBoundTextureGroup& group : textureGroups) {
        const std::int16_t first = WrappedThree(
            group.firstFace, group.modeCounts[0], group.modeCounts[1]);
        const std::int16_t count = WrappedThree(
            static_cast<std::uint16_t>(group.modeCounts[2]),
            group.modeCounts[3], group.modeCounts[4]);
        if (count <= 0)
            continue;
        if (first < 0 || static_cast<std::size_t>(first) > faces.size() ||
            static_cast<std::size_t>(count) > faces.size() - static_cast<std::size_t>(first))
            return Fail(error, "P3D smoothing range out of bounds");
        for (int n = 0; n < count; ++n) {
            const CD3DPOLYGON& face = faces[static_cast<std::size_t>(first + n)];
            const CD3DVECTOR normal = FaceNormal(face);
            for (std::int16_t index : {face.p1, face.p2, face.p3}) {
                if (index < 0 || static_cast<std::size_t>(index) >= sourceVertices.size())
                    return Fail(error, "P3D smoothing vertex out of bounds");
                ObjectVertex& vertex = sourceVertices[static_cast<std::size_t>(index)];
                CD3DVECTOR accumulated = VertexNormal(vertex);
                accumulated.x = static_cast<float>(static_cast<long double>(accumulated.x) + normal.x);
                accumulated.y = static_cast<float>(static_cast<long double>(accumulated.y) + normal.y);
                accumulated.z = static_cast<float>(static_cast<long double>(accumulated.z) + normal.z);
                SetVertexNormal(vertex, accumulated);
            }
        }
    }

    for (ObjectVertex& vertex : sourceVertices) {
        CD3DVECTOR normal = VertexNormal(vertex);
        NormalizeUnchecked(normal);
        SetVertexNormal(vertex, normal);
    }

    for (CD3DLIGHT& light : embeddedLights)
        light.RecomputeVectorLength();

    if (error) error->clear();
    return true;
}

bool CD3DPOLYGONOBJECT::SyncLighting(const P3DLightingEnvironment& environment,
                                    std::string* error) {
    if (transformedVertices.size() != sourceVertices.size())
        transformedVertices.resize(sourceVertices.size());

    LightingState lighting;
    if (!lighting.SetMode(static_cast<LightingMode>(lightingMode), error))
        return false;
    const bool full = lighting.Mode() == LightingMode::Full;

    for (TransformedVertex& transformed : transformedVertices) {
        transformed.diffuse = 0;
        transformed.specular = 0xFF000000u;
    }
    if (full)
        for (CD3DPOLYGON& face : faces)
            face.diffuse = 0;

    for (std::size_t i = 0; i < sourceVertices.size(); ++i) {
        const RGBColor value = lighting.ApplyDirectional(
            VertexNormal(sourceVertices[i]), environment.direction,
            environment.directionalColor);
        transformedVertices[i].diffuse = PackRGBColor(value.c0, value.c1, value.c2);
    }
    if (full) {
        for (CD3DPOLYGON& face : faces) {
            const RGBColor value = lighting.ApplyDirectional(
                FaceNormal(face), environment.direction, environment.directionalColor);
            face.diffuse = PackRGBColor(value.c0, value.c1, value.c2);
        }
    }

    for (std::size_t i = 0; i < sourceVertices.size(); ++i) {
        const RGBColor initial = UnpackRGB(transformedVertices[i].diffuse);
        LightingVertex vertex{VertexPosition(sourceVertices[i]),
                              VertexNormal(sourceVertices[i]), initial};
        const RGBColor value = lighting.ApplyLights(vertex, embeddedLights, initial);
        transformedVertices[i].diffuse = PackRGBColor(value.c0, value.c1, value.c2);
    }
    if (full) {
        for (CD3DPOLYGON& face : faces) {
            if (!ValidFaceIndices(face, sourceVertices.size()))
                return Fail(error, "P3D lighting face index out of range");
            const RGBColor initial = UnpackRGB(face.diffuse);
            LightingVertex vertex{FaceCentroid(face, sourceVertices), FaceNormal(face), initial};
            const RGBColor value = lighting.ApplyLights(vertex, embeddedLights, initial);
            face.diffuse = PackRGBColor(value.c0, value.c1, value.c2);
        }
    }

    for (TransformedVertex& transformed : transformedVertices) {
        const RGBColor value = AmbientFloor(UnpackRGB(transformed.diffuse), environment.ambientColor);
        transformed.diffuse = PackRGBColor(value.c0, value.c1, value.c2);
    }
    if (full) {
        for (CD3DPOLYGON& face : faces) {
            const RGBColor value = AmbientFloor(UnpackRGB(face.diffuse), environment.ambientColor);
            face.diffuse = PackRGBColor(value.c0, value.c1, value.c2);
        }
    }

    if (error) error->clear();
    return true;
}

void CD3DPOLYGONOBJECT::SetCenterPosition(const CD3DVECTOR& position) {

    Translate(position.x - center.x,
              position.y - center.y,
              position.z - center.z);
}

void CD3DPOLYGONOBJECT::Translate(float x, float y, float z) {

    if (!HasMagnitudeBits(x) && !HasMagnitudeBits(y) && !HasMagnitudeBits(z))
        return;
    CD3DMATRIX translation = TranslationTransform(x, y, z);
    ComposeTransform(pendingTransform, translation);
    transformState = 1;
}

void CD3DPOLYGONOBJECT::RotateX(const CD3DVECTOR& pivot, float angle) {
    if (!HasMagnitudeBits(angle))
        return;

    CD3DMATRIX before = TranslationTransform(0.0f, -pivot.y, -pivot.z);
    ComposeTransform(pendingTransform, before);
    CD3DMATRIX rotation{};
    MakeXRotation(rotation, angle);
    ComposeTransform(pendingTransform, rotation);
    CD3DMATRIX after = TranslationTransform(0.0f, pivot.y, pivot.z);
    ComposeTransform(pendingTransform, after);
    transformState = 1;
}

void CD3DPOLYGONOBJECT::RotateY(const CD3DVECTOR& pivot, float angle) {
    if (!HasMagnitudeBits(angle))
        return;
    CD3DMATRIX before = TranslationTransform(-pivot.x, 0.0f, -pivot.z);
    ComposeTransform(pendingTransform, before);
    CD3DMATRIX rotation{};
    MakeYRotation(rotation, angle);
    ComposeTransform(pendingTransform, rotation);
    CD3DMATRIX after = TranslationTransform(pivot.x, 0.0f, pivot.z);
    ComposeTransform(pendingTransform, after);
    transformState = 1;
}

void CD3DPOLYGONOBJECT::RotateZ(const CD3DVECTOR& pivot, float angle) {
    if (!HasMagnitudeBits(angle))
        return;
    CD3DMATRIX before = TranslationTransform(-pivot.x, -pivot.y, 0.0f);
    ComposeTransform(pendingTransform, before);
    CD3DMATRIX rotation{};
    MakeZRotation(rotation, angle);
    ComposeTransform(pendingTransform, rotation);
    CD3DMATRIX after = TranslationTransform(pivot.x, pivot.y, 0.0f);
    ComposeTransform(pendingTransform, after);
    transformState = 1;
}

bool CD3DPOLYGONOBJECT::ApplyPendingTransform(const P3DLightingEnvironment& environment,
                                             std::string* error) {
    if (transformState == 0) {
        if (error) error->clear();
        return true;
    }

    for (ObjectVertex& vertex : sourceVertices) {
        CD3DVECTOR position = VertexPosition(vertex);
        TransformPoint(pendingTransform, position);
        SetVertexPosition(vertex, position);
    }
    for (CD3DLIGHT& light : embeddedLights)
        TransformPoint(pendingTransform, light.position);
    for (CD3DVECTOR& point : boundsPoints)
        TransformPoint(pendingTransform, point);
    TransformPoint(pendingTransform, center);

    ClearTranslation(pendingTransform);
    TransformPoint(pendingTransform, forward);
    TransformPoint(pendingTransform, up);
    TransformPoint(pendingTransform, right);
    NormalizeUnchecked(forward);
    NormalizeUnchecked(up);
    NormalizeUnchecked(right);

    if (!RebuildGeometry(error) || !SyncLighting(environment, error))
        return false;
    RecomputeRuntimeBounds(*this);
    SetIdentity(pendingTransform);
    if (error) error->clear();
    return true;
}

bool CD3DPOLYGONOBJECT::Render(RendererState& renderer,
                              RenderDevice& backend,
                              std::string* error) {
    if (!renderer.HasGetFrameTransforms())
        return Fail(error, "renderer frame transforms are not prepared");
    if (transformedVertices.size() != sourceVertices.size())
        transformedVertices.resize(sourceVertices.size());

    TransformMatrix matrix{};
    for (std::size_t i = 0; i < matrix.size(); ++i)
        matrix[i] = renderer.GetFrameTransforms().combined.m[i];

    for (std::size_t i = 0; i < sourceVertices.size(); ++i) {
        const ProjectedPosition projected =
            ProjectPosition(VertexPosition(sourceVertices[i]), matrix);
        transformedVertices[i].x = projected.x;
        transformedVertices[i].y = projected.y;
        transformedVertices[i].z = projected.z;
        transformedVertices[i].rhw = projected.rhw;
    }

    renderer.SetZWrite(true, backend);
    for (const P3DBoundTextureGroup& group : textureGroups) {
        if (group.textureSlotState == P3DTextureSlotState::IndeterminateNativeCopy)
            return Fail(error, "P3D native Copy texture slot is allocator residue");
        std::uint16_t cursor = group.firstFace;
        for (std::uint8_t mode = 0; mode < 6; ++mode) {
            const std::int16_t count = group.modeCounts[mode];
            const std::int16_t first = static_cast<std::int16_t>(cursor);
            const CD3DPOLYGON* facePtr = faces.empty() ? nullptr : faces.data();
            std::size_t storageCount = faces.size();
            if (count > 0) {
                if (first < 0 || static_cast<std::size_t>(first) > faces.size() ||
                    static_cast<std::size_t>(count) > faces.size() - static_cast<std::size_t>(first))
                    return Fail(error, "P3D render batch out of bounds");
                facePtr = faces.data() + static_cast<std::size_t>(first);
                storageCount = faces.size() - static_cast<std::size_t>(first);
            }
            if (!renderer.SubmitTriangles(
                    facePtr, storageCount, count,
                    sourceVertices.empty() ? nullptr : sourceVertices.data(), sourceVertices.size(),
                    transformedVertices.empty() ? nullptr : transformedVertices.data(), transformedVertices.size(),
                    group.textureIndex, mode, backend))
                return Fail(error, "renderer rejected P3D batch");
            cursor = WrapAdd(cursor, count);
        }
    }
    if (error) error->clear();
    return true;
}

TextureReplaceResult CD3DPOLYGONOBJECT::ReplaceTexture(
    std::string_view from, std::string_view to, const CBMManager& textures) {
    TextureReplaceResult result;
    const std::string canonicalFrom = CBMManager::CanonicalName(from);
    const std::string canonicalTo = CBMManager::CanonicalName(to);
    for (P3DBoundTextureGroup& group : textureGroups) {
        if (CBMManager::CanonicalName(group.textureName) != canonicalFrom)
            continue;
        ++result.matchedGroups;
        const bool loaded = textures.Contains(canonicalTo);
        result.targetWasLoaded = result.targetWasLoaded && loaded;

        group.textureName = canonicalTo;
        group.textureIndex = textures.IndexOf(canonicalTo);
        group.textureSlotState = P3DTextureSlotState::Bound;
    }
    return result;
}

}
