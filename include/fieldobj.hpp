#pragma once

#include "cdfileop.hpp"
#include "polyobj.hpp"
#include "types.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>

namespace flydemo {

constexpr float kTrackFieldSize = 20.0f;
constexpr const char* kFieldObjectDirectory = "TRKDATA/FIELDS";
constexpr const char* kFieldOpenMode = "r+";

enum class FieldEdge : std::uint8_t { Top = 0, Bottom = 1, Left = 2, Right = 3 };

struct FieldObjectFlags {
    bool allowInTrackEditor = false;
    bool recover = false;
    bool runIntoTop = false;
    bool runIntoBottom = false;
    bool runIntoLeft = false;
    bool runIntoRight = false;
};

struct FieldObjectConfig {
    std::string discardedHeaderLine;
    std::string infoLine;
    std::string p3dFilename;
    FieldObjectFlags flags{};
    std::array<std::uint8_t, 6> rawFlagBytes{};
};

struct FieldObjectPlacement {
    std::int16_t gridX = 0;
    std::int16_t gridY = 0;
    std::uint8_t rotation = 0;
    float verticalOffset = 0.0f;
    float effectiveHeight = 0.0f;
    CD3DVECTOR position{};
    float yawUnits = 0.0f;
};

struct FieldCleanup {
    bool arrayDeleteHelper = false;
    bool destroyMembersAndBase = true;
    bool deallocateObject = false;
};

bool DecodeFieldObjectFlags(const std::array<int, 6>& values,
                            FieldObjectFlags& out,
                            std::string* error = nullptr,
                            std::array<std::uint8_t, 6>* rawBytes = nullptr);

bool ReadFieldObjectConfig(CDFileOperations& file,
                           FieldObjectConfig& out,
                           std::string* error = nullptr);

std::uint8_t RotatedRunInto(const FieldObjectFlags& flags,
                            std::uint8_t rotation,
                            FieldEdge worldEdge);

bool ValidateFieldP3DExtents(const P3DModel& model, std::string* error = nullptr);

bool BuildFieldPlacement(std::int16_t gridX,
                               std::int16_t gridY,
                               std::uint8_t rotation,
                               float verticalOffset,
                               float objectHeight,
                               float modelHeight,
                               FieldObjectPlacement& out,
                               std::string* error = nullptr);

bool ScaleFieldObjectHeight(P3DModel& model,
                            float objectHeight,
                            std::string* error = nullptr);

bool ScaleFieldObjectHeight(CD3DPOLYGONOBJECT& object,
                                   float objectHeight,
                                   std::string* error = nullptr);

bool PrepareStaticField(CD3DPOLYGONOBJECT& object,
                                  const FieldObjectPlacement& placement,
                                  const P3DLightingEnvironment& lighting,
                                  std::string* error = nullptr);

FieldCleanup DecodeFieldDtorFlags(std::uint8_t flags);

}
