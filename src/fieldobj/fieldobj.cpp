#include "fieldobj.hpp"
#include "path.hpp"

#include <array>
#include <cstdint>
#include <cstring>
#include <string>

namespace flydemo {
namespace {
bool Fail(std::string* error, const char* msg) {
    if (error) *error = msg;
    return false;
}

std::uint8_t FlagValue(bool v) { return v ? 1u : 0u; }

std::uint32_t FloatBits(float value) {
    std::uint32_t bits = 0;
    std::memcpy(&bits, &value, sizeof(bits));
    return bits;
}

bool HasMagnitudeBits(float value) {
    return (FloatBits(value) & 0x7FFFFFFFu) != 0;
}

bool IsOrderedNegative(float value) {
    return value < 0.0f;
}

std::int32_t WatcomAtoi32(const std::string& text) {

    std::size_t i = 0;
    auto isSpace = [](unsigned char c) {
        return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f';
    };
    while (i < text.size() && isSpace(static_cast<unsigned char>(text[i]))) ++i;
    bool negative = false;
    if (i < text.size() && (text[i] == '+' || text[i] == '-')) {
        negative = text[i] == '-';
        ++i;
    }
    std::uint32_t value = 0;
    while (i < text.size()) {
        const unsigned char c = static_cast<unsigned char>(text[i]);
        if (c < '0' || c > '9') break;
        value = value * 10u + static_cast<std::uint32_t>(c - '0');
        ++i;
    }
    if (negative) value = 0u - value;
    return static_cast<std::int32_t>(value);
}

bool DecodeFlagText(const std::string& text, std::uint8_t& byte,
                    const char* label, std::string* error) {
    const std::int32_t parsed = WatcomAtoi32(text);
    byte = static_cast<std::uint8_t>(static_cast<std::uint32_t>(parsed));
    if (byte != 0u && byte != 1u) {
        if (error) *error = std::string(label) + " must narrow to 0 or 1";
        return false;
    }
    return true;
}

bool RuntimeHeightScale(CD3DPOLYGONOBJECT& object, float objectHeight,
                        std::string* error) {
    if (IsOrderedNegative(objectHeight))
        return Fail(error, "Field object height must be >= 0 under native x87 comparison");
    if (!HasMagnitudeBits(objectHeight)) {
        if (error) error->clear();
        return true;
    }

    const float factor = objectHeight / object.sizeY;
    object.sizeY = objectHeight;
    object.center.y = objectHeight * 0.5f;
    for (ObjectVertex& vertex : object.sourceVertices)
        vertex.positionY *= factor;
    for (CD3DLIGHT& light : object.embeddedLights)
        light.position.y *= factor;
    for (CD3DVECTOR& point : object.boundsPoints)
        point.y *= factor;
    if (error) error->clear();
    return true;
}
}

bool DecodeFieldObjectFlags(const std::array<int, 6>& values,
                            FieldObjectFlags& out,
                            std::string* error,
                            std::array<std::uint8_t, 6>* rawBytes) {
    std::array<std::uint8_t, 6> narrowed{};
    for (std::size_t i = 0; i < values.size(); ++i) {
        narrowed[i] = static_cast<std::uint8_t>(static_cast<std::uint32_t>(values[i]));
        if (narrowed[i] != 0u && narrowed[i] != 1u)
            return Fail(error, "Field object option must narrow to BYTE 0 or 1");
    }
    out.allowInTrackEditor = narrowed[0] != 0;
    out.recover            = narrowed[1] != 0;
    out.runIntoTop         = narrowed[2] != 0;
    out.runIntoBottom      = narrowed[3] != 0;
    out.runIntoLeft        = narrowed[4] != 0;
    out.runIntoRight       = narrowed[5] != 0;
    if (rawBytes) *rawBytes = narrowed;
    if (error) error->clear();
    return true;
}

bool ReadFieldObjectConfig(CDFileOperations& file,
                           FieldObjectConfig& out,
                           std::string* error) {
    if (!file.IsOpen()) return Fail(error, "Field object config file is not open");

    FieldObjectConfig parsed;
    std::string allowText, recoverText;
    std::array<std::string, 4> edgeText{};
    if (!CrashdayDirectory::ReadConfigLine(file, parsed.discardedHeaderLine) ||
        !CrashdayDirectory::ReadConfigLine(file, parsed.infoLine) ||
        !CrashdayDirectory::ReadConfigLine(file, parsed.p3dFilename) ||
        !CrashdayDirectory::ReadConfigLine(file, allowText) ||
        !CrashdayDirectory::ReadConfigLine(file, recoverText))
        return Fail(error, "Field object config ended before required line fields");
    for (std::string& token : edgeText) {
        if (!CrashdayDirectory::ReadConfigToken(file, token))
            return Fail(error, "Field object config ended before required edge flags");
    }

    const char* labels[6] = {
        "AllowInTrackEditor", "Recover", "RunIntoTop", "RunIntoBottom",
        "RunIntoLeft", "RunIntoRight"
    };
    std::array<std::string, 6> texts = {
        allowText, recoverText, edgeText[0], edgeText[1], edgeText[2], edgeText[3]
    };
    for (std::size_t i = 0; i < texts.size(); ++i) {
        if (!DecodeFlagText(texts[i], parsed.rawFlagBytes[i], labels[i], error))
            return false;
    }
    parsed.flags.allowInTrackEditor = parsed.rawFlagBytes[0] != 0;
    parsed.flags.recover            = parsed.rawFlagBytes[1] != 0;
    parsed.flags.runIntoTop         = parsed.rawFlagBytes[2] != 0;
    parsed.flags.runIntoBottom      = parsed.rawFlagBytes[3] != 0;
    parsed.flags.runIntoLeft        = parsed.rawFlagBytes[4] != 0;
    parsed.flags.runIntoRight       = parsed.rawFlagBytes[5] != 0;

    out = std::move(parsed);
    if (error) error->clear();
    return true;
}

std::uint8_t RotatedRunInto(const FieldObjectFlags& f,
                            std::uint8_t rotation,
                            FieldEdge edge) {
    if (rotation >= 4) return 0xffu;

    switch (edge) {
    case FieldEdge::Top: {
        const bool v[4] = {f.runIntoTop, f.runIntoLeft, f.runIntoBottom, f.runIntoRight};
        return FlagValue(v[rotation]);
    }
    case FieldEdge::Bottom: {
        const bool v[4] = {f.runIntoBottom, f.runIntoRight, f.runIntoTop, f.runIntoLeft};
        return FlagValue(v[rotation]);
    }
    case FieldEdge::Left: {
        const bool v[4] = {f.runIntoLeft, f.runIntoTop, f.runIntoRight, f.runIntoBottom};
        return FlagValue(v[rotation]);
    }
    case FieldEdge::Right: {
        const bool v[4] = {f.runIntoRight, f.runIntoBottom, f.runIntoLeft, f.runIntoTop};
        return FlagValue(v[rotation]);
    }
    }
    return 0xffu;
}

bool ValidateFieldP3DExtents(const P3DModel& model, std::string* error) {
    constexpr std::uint32_t kTwentyBits = 0x41A00000u;
    if (FloatBits(model.sizeX) != kTwentyBits)
        return Fail(error, "Field P3D Length must equal exact float 20.0");
    if (FloatBits(model.sizeZ) != kTwentyBits)
        return Fail(error, "Field P3D Depth must equal exact float 20.0");
    if (error) error->clear();
    return true;
}

bool BuildFieldPlacement(std::int16_t gridX,
                               std::int16_t gridY,
                               std::uint8_t rotation,
                               float verticalOffset,
                               float objectHeight,
                               float modelHeight,
                               FieldObjectPlacement& out,
                               std::string* error) {
    if (rotation >= 4) return Fail(error, "Field rotation must be in [0,3]");
    if (IsOrderedNegative(objectHeight))
        return Fail(error, "Field object height must be >= 0 under native x87 comparison");

    const float effectiveHeight = HasMagnitudeBits(objectHeight) ? objectHeight : modelHeight;
    out = {};
    out.gridX = gridX;
    out.gridY = gridY;
    out.rotation = rotation;
    out.verticalOffset = verticalOffset;
    out.effectiveHeight = effectiveHeight;
    out.yawUnits = static_cast<float>(rotation) * 64.0f;
    out.position.x = static_cast<float>(gridX) * kTrackFieldSize + 10.0f;
    out.position.y = effectiveHeight * 0.5f + verticalOffset;
    out.position.z = -static_cast<float>(gridY) * kTrackFieldSize - 10.0f;
    if (error) error->clear();
    return true;
}

bool ScaleFieldObjectHeight(P3DModel& model,
                            float objectHeight,
                            std::string* error) {
    if (IsOrderedNegative(objectHeight))
        return Fail(error, "Field object height must be >= 0 under native x87 comparison");
    if (!HasMagnitudeBits(objectHeight)) {
        if (error) error->clear();
        return true;
    }
    const float factor = objectHeight / model.sizeY;
    model.sizeY = objectHeight;
    for (auto& vertex : model.vertices) vertex.position.y *= factor;
    for (auto& light : model.materials) light.position.y *= factor;
    if (error) error->clear();
    return true;
}

bool ScaleFieldObjectHeight(CD3DPOLYGONOBJECT& object,
                                   float objectHeight,
                                   std::string* error) {
    return RuntimeHeightScale(object, objectHeight, error);
}

bool PrepareStaticField(CD3DPOLYGONOBJECT& object,
                                  const FieldObjectPlacement& placement,
                                  const P3DLightingEnvironment& lighting,
                                  std::string* error) {
    if (placement.rotation >= 4)
        return Fail(error, "Field rotation must be in [0,3]");

    object.SetCenterPosition({0.0f, 0.0f, 0.0f});
    object.RotateY({0.0f, 0.0f, 0.0f}, placement.yawUnits);
    if (!object.ApplyPendingTransform(lighting, error)) return false;

    object.SetCenterPosition(placement.position);
    if (error) error->clear();
    return true;
}

FieldCleanup DecodeFieldDtorFlags(std::uint8_t flags) {
    FieldCleanup out;
    if ((flags & 0x04u) != 0u) {
        out.arrayDeleteHelper = true;
        out.destroyMembersAndBase = false;
        out.deallocateObject = false;
    } else {
        out.arrayDeleteHelper = false;
        out.destroyMembersAndBase = true;
        out.deallocateObject = (flags & 0x02u) != 0u;
    }
    return out;
}

}
