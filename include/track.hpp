#pragma once

#include "fieldobj.hpp"
#include "types.hpp"
#include "world.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace flydemo {

struct TrackCell {
    std::uint16_t fieldFileIndex = 0;
    std::uint8_t rotationOrVariant = 0;
    std::uint8_t field31 = 0;
};

struct TrackDropObject {
    std::uint16_t dynamicFileIndex = 0;
    CD3DVECTOR position{};
    float rotation = 0.0f;
};

struct CARSTART {
    CD3DVECTOR position{};
    float rotation = 0.0f;
};

struct TrackFileData {

    std::string trackName;
    std::string author;
    std::uint8_t flags = 0;
    std::vector<std::string> fieldFiles;

    std::optional<std::uint16_t> declaredFieldFileCount;
    std::uint16_t NumFieldsX = 0;
    std::uint16_t NumFieldsY = 0;
    std::vector<TrackCell> cells;
    std::vector<std::string> dynamicFiles;
    std::vector<TrackDropObject> dropObjects;
    std::uint16_t NumDropObjects = 0;
    std::array<CARSTART, 6> pcOpponentStarts{};
    std::size_t consumedBytes = 0;
};

bool ParseTrackFile(const std::vector<std::uint8_t>& bytes,
                    TrackFileData& out,
                    std::string* error = nullptr);

std::string BuildTrackPath(std::string_view requestedFilename);

std::string DeriveTrackLoadName(std::string_view requestedFilename);

bool BuildTrackFileBytes(const TrackFileData& track,
                         std::vector<std::uint8_t>& out,
                         std::string* error = nullptr);

std::string BuildTrackSavePath(const TrackFileData& track);

std::uint16_t Track_FieldFileCount(const TrackFileData& track);

class TrackRandomSource {
public:
    virtual ~TrackRandomSource() = default;
    virtual std::int32_t Next() = 0;
};

enum class TrackGenerationStyle : std::uint8_t {
    NoHills = 0,
    SomeHills = 1,
    ManyHills = 2,
};

bool Track_CreateNewData(TrackFileData& track,
                         std::int16_t fieldsX,
                         std::int16_t fieldsY,
                         TrackGenerationStyle style,
                         TrackRandomSource& random,
                         std::string* error = nullptr);

bool Track_PaintHills(TrackFileData& track,
                                std::int32_t repetitions,
                                std::int32_t level,
                                std::int16_t maxSize,
                                TrackRandomSource& random,
                                std::string* error = nullptr);

void Track_RebuildHills(TrackFileData& track);

enum class TrackFieldKind {
    Mount1,
    Mount2,
    Mount3,
    Mount4,
    MountStreet,
    MountHighway,
    MountRail,
    Generic
};
TrackFieldKind GetFieldConfigType(const std::string& name);

float TrackFieldYOffset(const TrackCell& cell, TrackFieldKind kind);

bool BuildTrackPlacement(const TrackCell& cell,
                              std::uint16_t gridX,
                              std::uint16_t gridY,
                              const std::string& fieldConfigName,
                              float modelHeight,
                              FieldObjectPlacement& out,
                              std::string* error = nullptr);

struct TrackFieldSpawn {
    std::size_t cellIndex = 0;
    std::uint16_t fieldFileIndex = 0;
    std::string objectName;
    std::string fieldConfigName;
    TrackFieldKind association = TrackFieldKind::Generic;
    FieldObjectPlacement placement{};
};

bool BuildTrackFields(const TrackFileData& track,
                                  const std::vector<float>& modelHeights,
                                  std::vector<TrackFieldSpawn>& out,
                                  std::string* error = nullptr);

struct TrackDropSpawn {
    std::size_t dropIndex = 0;
    std::uint16_t dynamicFileIndex = 0;
    std::string objectName;
    std::string dynamicConfigName;
    CD3DVECTOR position{};
    float rotation = 0.0f;
};

bool BuildTrackDrops(const TrackFileData& track,
                                 std::vector<TrackDropSpawn>& out,
                                 std::string* error = nullptr);

struct FieldFileResult {
    bool applied = false;
    bool badPreviousId = false;
    bool hitLimit = false;
    std::int16_t resultingIndex = -1;
};
FieldFileResult Track_ReplaceFieldFile(
    TrackFileData& track,
    std::string_view newFile,
    std::string_view previousFile);

struct DynamicFileAdd {
    bool applied = false;
    bool hitLimit = false;
    std::int16_t index = -1;
};
DynamicFileAdd Track_AddDynamicFile(
    TrackFileData& track,
    std::string_view dynamicFile);

struct DynamicFileRemove {
    bool completed = false;
    bool badPreviousId = false;
    bool removed = false;
    std::uint16_t referenceCount = 0;
    std::int16_t removedIndex = -1;
};
DynamicFileRemove Track_RemoveDynamicFile(
    TrackFileData& track,
    std::string_view previousFile);

struct TrackSceneState {
    std::uint8_t dataAssociated = 0;
    std::vector<CD3DOBJECT*> fieldObjects;
    std::array<CD3DOBJECT*, 150> dropObjects{};
    std::size_t dropObjectCount = 0;
};

enum TrackDetachDiag : std::uint32_t {
    TrackDetachDiag_None = 0,
    TrackDetach_BadState = 1u << 0,

    TrackDetach_NullObject = 1u << 1,
    TrackDetach_TooManyDrops = 1u << 2,
};

enum TrackEditDiag : std::uint32_t {
    TrackEditDiag_None = 0,
    TrackEditDiag_XInterior = 1u << 0,
    TrackEditDiag_YInterior = 1u << 1,
    TrackEdit_BadRotation = 1u << 2,
    TrackEdit_BadCell = 1u << 3,
    TrackEdit_NoField = 1u << 4,
    TrackEdit_NoRuntime = 1u << 5,
    TrackEdit_FieldFileError = 1u << 6,
    TrackEdit_NoDrop = 1u << 7,
    TrackEdit_TooManyDrops = 1u << 8,
    TrackEdit_BadDropId = 1u << 9,
    TrackEdit_BadDrop = 1u << 10,
    TrackEdit_BadDropConfig = 1u << 11,
    TrackEdit_DropFull = 1u << 12,
    TrackEdit_BadState = 1u << 13,
};

struct TrackFieldCreateReq {
    std::string objectName;
    std::string fieldConfigName;
    std::int16_t gridX = 0;
    std::int16_t gridY = 0;
    std::uint8_t rotationOrVariant = 0;
    std::uint8_t field31 = 0;

    bool mountHeightAdjusted = false;
    float verticalOffset = 0.0f;
};

struct TrackDropCreateReq {
    std::string objectName;
    std::string dynamicConfigName;
    CD3DVECTOR position{};
    float rotation = 0.0f;
};

class TrackObjects {
public:
    virtual ~TrackObjects() = default;
    virtual CD3DOBJECT* CreateFieldObject(const TrackFieldCreateReq& request) = 0;
    virtual CD3DOBJECT* CreateDropObject(const TrackDropCreateReq& request) = 0;
    virtual CD3DVECTOR GetPosition(const CD3DOBJECT& object) const = 0;
    virtual void SetPosition(CD3DOBJECT& object, const CD3DVECTOR& position) = 0;
    virtual void Translate(CD3DOBJECT& object, const CD3DVECTOR& delta) = 0;
    virtual void RotateAround(CD3DOBJECT& object, const CD3DVECTOR& pivot, float yawUnits) = 0;
    virtual float ModelHeight(const CD3DOBJECT& object) const = 0;
};

struct TrackContext {
    WorldState* world = nullptr;
    WorldDelete* deleter = nullptr;
    TrackObjects* objects = nullptr;
    float rendererFrameDelta = 0.0f;
};

struct TrackGroundResult {
    bool completed = false;
    std::uint32_t diagnostics = TrackEditDiag_None;
    std::size_t processed = 0;
    std::size_t hits = 0;
};

TrackGroundResult Track_DropToGround(
    const TrackFileData& track,
    TrackSceneState& state,
    TrackContext runtime);

struct TrackFieldEdit {
    bool completed = false;
    std::uint8_t returnCode = 0;
    std::uint32_t diagnostics = TrackEditDiag_None;
    std::size_t cellIndex = static_cast<std::size_t>(-1);
    std::string normalizedRequestedFile;
    std::string normalizedPreviousFile;
    bool preservedRotation = false;
    bool recreatedRuntimeObject = false;
};

TrackFieldEdit Track_EditField(
    TrackFileData& track,
    TrackSceneState& state,
    TrackContext runtime,
    std::int16_t x,
    std::int16_t y,
    std::string& requestedFile,
    std::int8_t rotation);

struct TrackDropAdd {
    bool completed = false;
    std::uint32_t diagnostics = TrackEditDiag_None;
    std::int16_t id = -1;
    bool runtimeObjectCreated = false;
};

TrackDropAdd Track_AddDrop(
    TrackFileData& track,
    TrackSceneState& state,
    TrackContext runtime,
    std::string& dynamicFile,
    float x,
    float z,
    float rotation);

struct TrackDropEdit {
    bool completed = false;
    std::uint32_t diagnostics = TrackEditDiag_None;
    bool runtimeObjectUpdated = false;
};

TrackDropEdit Track_EditDrop(
    TrackFileData& track,
    TrackSceneState& state,
    TrackContext runtime,
    std::int16_t id,
    float x,
    float z,
    float rotation);

struct TrackDropDelete {
    bool completed = false;
    std::uint32_t diagnostics = TrackEditDiag_None;
    bool runtimeObjectDeleted = false;
    bool movedLastRecord = false;
};

TrackDropDelete Track_DeleteDrop(
    TrackFileData& track,
    TrackSceneState& state,
    TrackContext runtime,
    std::int16_t id);

enum OpponentStartDiag : std::uint32_t {
    OpponentStartDiag_None = 0,
    OpponentStart_BadId = 1u << 0,
    OpponentStart_BadX = 1u << 1,
    OpponentStart_BadZ = 1u << 2,
    OpponentStart_NoRuntime = 1u << 3,
    OpponentStart_BadState = 1u << 4,
};

struct StartSetResult {
    bool completed = false;
    std::uint32_t diagnostics = OpponentStartDiag_None;
};

StartSetResult Track_SetOpponentStart(
    TrackFileData& track,
    std::int16_t id,
    float x,
    float z,
    float rotation);

struct StartGroundResult {
    bool completed = false;
    std::uint32_t diagnostics = OpponentStartDiag_None;
    std::size_t hits = 0;
};

StartGroundResult Track_UpdateStartGround(
    TrackFileData& track,
    const TrackSceneState& state,
    TrackContext runtime);

struct StartLookup {
    bool completed = false;
    std::uint32_t diagnostics = OpponentStartDiag_None;
    CARSTART* start = nullptr;
    bool refreshedAllStarts = false;
};

StartLookup Track_GetOpponentStart(
    TrackFileData& track,
    const TrackSceneState& state,
    TrackContext runtime,
    std::int16_t id);

void Track_CopyData(const TrackFileData& source, TrackFileData& destination);

std::uint8_t Track_IsAssociated(const TrackSceneState& state);

enum TrackRuntimeDiag : std::uint32_t {
    TrackRuntimeDiag_None = 0,
    TrackRuntime_BadState = 1u << 0,
    TrackRuntime_BadX = 1u << 1,
    TrackRuntime_BadY = 1u << 2,
    TrackRuntime_BadDrop = 1u << 3,

    TrackRuntime_BadField = 1u << 4,
};

struct TrackFieldRef {
    bool completed = false;
    std::uint32_t diagnostics = TrackRuntimeDiag_None;
    CD3DOBJECT* object = nullptr;
    std::size_t cellIndex = static_cast<std::size_t>(-1);
};

TrackFieldRef Track_GetFieldObject(
    const TrackFileData& track,
    const TrackSceneState& state,
    std::int16_t x,
    std::int16_t y);

struct TrackDropRef {
    bool completed = false;
    std::uint32_t diagnostics = TrackRuntimeDiag_None;
    CD3DOBJECT* object = nullptr;
};

TrackDropRef Track_GetDropObject(
    const TrackFileData& track,
    const TrackSceneState& state,
    std::int16_t id);

class TrackResourceProbe {
public:
    virtual ~TrackResourceProbe() = default;
    virtual bool Exists(std::string_view relativePath) const = 0;
};

enum TrackReplaceDiag : std::uint32_t {
    TrackReplaceDiag_None = 0,
    TrackReplace_NoRuntime = 1u << 0,
    TrackReplace_DetachError = 1u << 1,

    TrackReplace_Rollback = 1u << 2,
};

struct TrackReplaceResult {
    bool failed = false;
    bool deassociated = false;
    bool rolledBack = false;
    std::uint32_t diagnostics = TrackReplaceDiag_None;
    std::vector<std::string> missingFiles;
};

TrackReplaceResult Track_ReplaceData(
    TrackFileData& current,
    TrackSceneState& live,
    TrackContext runtime,
    const TrackFileData& candidate,
    const TrackResourceProbe& resources,
    std::string& missingList);

enum TrackLoadDiag : std::uint32_t {
    TrackLoadDiag_None = 0,
    TrackLoad_NoRuntime = 1u << 0,
    TrackLoad_DetachFailed = 1u << 1,
    TrackLoad_BadMagic = 1u << 2,

    TrackLoad_ParseError = 1u << 3,

    TrackLoad_ShortFilename = 1u << 4,
    TrackLoad_BadRollback = 1u << 5,
};

struct TrackLoadResult {
    bool failed = true;
    bool deassociated = false;
    bool rolledBack = false;
    std::uint32_t diagnostics = TrackLoadDiag_None;
    std::string relativeTrackPath;
    std::string derivedTrackName;
    std::vector<std::string> missingFiles;
};

TrackLoadResult Track_LoadBytes(
    TrackFileData& current,
    TrackSceneState& live,
    TrackContext runtime,
    std::string_view requestedFilename,
    const std::vector<std::uint8_t>& bytes,
    const TrackResourceProbe& resources,
    std::string& missingList);

struct TrackDetachResult {
    bool completed = false;
    std::uint32_t diagnostics = TrackDetachDiag_None;
    std::size_t fieldDeleteAttempts = 0;
    std::size_t fieldDeleteSuccesses = 0;
    std::size_t dropDeleteAttempts = 0;
    std::size_t dropDeleteSuccesses = 0;
    bool fieldPointerArrayFreed = false;
};

TrackDetachResult Track_Deassociate3DData(
    TrackSceneState& state,
    WorldState& world,
    WorldDelete& deleter);

enum TrackLifeDiag : std::uint32_t {
    TrackLifeDiag_None = 0,
    TrackLife_NoRuntime = 1u << 0,
    TrackLife_DetachFailed = 1u << 1,
    TrackLife_GenFailed = 1u << 2,
};

struct TrackLifeResult {
    bool completed = false;
    bool deassociated = false;
    std::uint32_t diagnostics = TrackLifeDiag_None;
};

TrackLifeResult Track_InitState(
    TrackFileData& track,
    TrackSceneState& live,
    std::int16_t fieldsX,
    std::int16_t fieldsY,
    TrackGenerationStyle style,
    TrackRandomSource& random,
    std::string* error = nullptr);

TrackLifeResult Track_NewTrack(
    TrackFileData& track,
    TrackSceneState& live,
    TrackContext runtime,
    std::int16_t fieldsX,
    std::int16_t fieldsY,
    TrackGenerationStyle style,
    TrackRandomSource& random,
    std::string* error = nullptr);

TrackLifeResult Track_PrepareDestroy(
    TrackSceneState& live,
    TrackContext runtime);

}
