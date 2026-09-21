#include "types.hpp"

#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <string_view>

namespace flydemo {
namespace {

constexpr std::string_view kWindowClass = "propsfx_windowclass";
constexpr std::string_view kWindowTitle = "Crashday self-playing Fly-Demo";
constexpr std::string_view kConfigFile = "propsfx.cfg";
constexpr std::string_view kCameraPath = "newflydemo.pth";
constexpr std::string_view kTrackFile = "demo.trk";
constexpr std::string_view kLogFile = "crashfly.log";
constexpr std::string_view kSoundsDirectory = "SOUNDS";
constexpr std::string_view kDemoMusic = "demo1sng.wav";
constexpr std::array<std::string_view, 7> kPreloadResources{{
    "gfx/bullets.cbm",
    "gfx/shotfire.cbm",
    "gfx/smoke.cbm",
    "gfx/corona.cbm",
    "logos/crashfly.cbm",
    "logos/moonbyte.cbm",
    "logos/endtext.cbm",
}};

using LifecycleLogSink = void (*)(std::string_view text);
LifecycleLogSink LifecycleSink = nullptr;

void LifecycleLog(std::string_view text) {
    if (LifecycleSink)
        LifecycleSink(text);
}

std::int32_t OverlayTrunc(long double value) {
    if (!std::isfinite(value) ||
        value > static_cast<long double>(std::numeric_limits<std::int32_t>::max()) ||
        value < static_cast<long double>(std::numeric_limits<std::int32_t>::min()))
        return std::numeric_limits<std::int32_t>::min();
    return static_cast<std::int32_t>(std::trunc(value));
}

std::uint32_t OverlayGray(std::int32_t value) {
    const std::uint32_t c = static_cast<std::uint32_t>(value) & 0xffu;
    return c | (c << 8u) | (c << 16u);
}

constexpr CD3DVECTOR kWolfStart{319.0f, 0.7599999904632568f, -605.0f};
constexpr CD3DVECTOR kCycoreStart{220.0f, 0.7599999904632568f, -91.0f};
constexpr CD3DVECTOR kApacheeStart{47.0f, 0.6000000238418579f, -85.0f};
constexpr CD3DVECTOR kWreckerStart{385.0f, 11.199999809265137f, -550.0f};
constexpr CD3DVECTOR kWrecker2Start{389.0f, 11.199999809265137f, -551.0f};

inline bool closed(float t, float lo, float hi) {
    return t >= lo && t <= hi;
}
inline bool open(float t, float lo, float hi) {
    return t > lo && t < hi;
}
inline bool closedOpen(float t, double lo, double hi) {
    return static_cast<double>(t) >= lo && static_cast<double>(t) < hi;
}


}


}


#include "car_specs.hpp"
#include "car_object.hpp"
#include "dynamic_object.hpp"
#include "memory.hpp"
#include "offsets.hpp"
#include "particle.hpp"
#include "missile.hpp"
#include "minigun.hpp"
#include "explosion.hpp"
#include "ambience.hpp"
#include "camera.hpp"
#include "flyctrl.hpp"
#include "engine.hpp"
#include "options.hpp"
#include "polyobj.hpp"
#include "track.hpp"
#include "render.hpp"
#include "cbm.hpp"
#include "cdfileop.hpp"
#include "fieldobj.hpp"
#include "path.hpp"
#include "world.hpp"
#include "runtime.hpp"
#include "transform.hpp"

#include <algorithm>
#include <cstddef>
#include <array>
#include <cmath>
#include <cstdlib>
#include <ctime>
#include <cstring>
#include <fstream>
#include <iterator>
#include <memory>
#include <system_error>
#include <unordered_map>
#include <utility>
#include <vector>


namespace flydemo {
namespace {

std::uint32_t NativeRandomSeed = 0;
bool NativeRandomSeeded = false;

void SeedNativeRandom() {
    const std::time_t now = std::time(nullptr);
    std::tm local{};
#ifdef _WIN32
    localtime_s(&local, &now);
#else
    localtime_r(&now, &local);
#endif
    NativeRandomSeed = static_cast<std::uint32_t>(local.tm_sec);
    NativeRandomSeeded = true;
}

int NativeRandom() {
    if (!NativeRandomSeeded)
        SeedNativeRandom();
    NativeRandomSeed = NativeRandomSeed * 0x41c64e6du + 0x3039u;
    return static_cast<int>((NativeRandomSeed >> 16u) & 0x7fffu);
}

bool DrawCenteredOverlay(std::string_view texture,
                         std::int32_t width, std::int32_t height,
                         float scale, float halfWidth, float halfHeight,
                         std::uint32_t color,
                         CBMManager& textures, CBMLoader& loader,
                         RendererState& renderer, RenderDevice& backend) {
    const std::int32_t cx = width / 2;
    const std::int32_t cy = height / 2;
    const long double sx = static_cast<long double>(scale) * halfWidth;
    const long double sy = static_cast<long double>(scale) * halfHeight;
    return DrawTexturedRect(
        texture, 1,
        OverlayTrunc(static_cast<long double>(cx) - sx),
        OverlayTrunc(static_cast<long double>(cy) - sy),
        OverlayTrunc(static_cast<long double>(cx) + sx),
        OverlayTrunc(static_cast<long double>(cy) + sy),
        1.0f, BlendMode::Additive, color,
        textures, loader, renderer, backend);
}

bool RenderOverlay(std::int32_t screenWidth,
                   std::int32_t screenHeight,
                   std::int16_t cycle,
                   float pathTime,
                   CBMManager& textures,
                   CBMLoader& loader,
                   RendererState& renderer,
                   RenderDevice& backend) {
    const std::int32_t barHeight = screenHeight / 9;
    if (!DrawTexturedRect("colblck.cbm", 1, 0, 0,
                          screenWidth, barHeight, 1.0f, BlendMode::Opaque,
                          0x00000000u, textures, loader, renderer, backend) ||
        !DrawTexturedRect("colblck.cbm", 1, 0, screenHeight - barHeight,
                          screenWidth, screenHeight, 1.0f, BlendMode::Opaque,
                          0x00000000u, textures, loader, renderer, backend) ||
        !DrawTexturedImage("logos/copyrght.cbm", 1,
                           screenWidth - 236, screenHeight - 18,
                           1.0f, BlendMode::Opaque, 0x00666666u,
                           textures, loader, renderer, backend))
        return false;

    const float scale = static_cast<float>(
        static_cast<long double>(screenWidth) *
        static_cast<long double>(0.0012499999720603228f));

    if (cycle == 0) {
        if (!(pathTime >= 5.0f)) {
            const std::int32_t shade = OverlayTrunc(
                (static_cast<long double>(5.0f) - pathTime) *
                static_cast<long double>(0.20000000298023224f) * 255.0L);
            if (!DrawCenteredOverlay("logos/moonbyte.cbm", screenWidth, screenHeight,
                                     scale, 250.0f, 74.0f, OverlayGray(shade),
                                     textures, loader, renderer, backend))
                return false;
        }
        if (pathTime > 3.0f && !(pathTime > 6.0f)) {
            const std::int32_t shade = OverlayTrunc(
                (static_cast<long double>(pathTime) - 3.0L) *
                static_cast<long double>(0.3333333432674408f) * 205.0L);
            if (!DrawCenteredOverlay("logos/crashfly.cbm", screenWidth, screenHeight,
                                     scale, 220.0f, 54.0f, OverlayGray(shade),
                                     textures, loader, renderer, backend))
                return false;
        }
        if (pathTime > 6.0f && pathTime < 9.0f) {
            const std::int32_t shade = OverlayTrunc(
                (9.0L - static_cast<long double>(pathTime)) *
                static_cast<long double>(0.3333333432674408f) * 205.0L);
            if (!DrawCenteredOverlay("logos/crashfly.cbm", screenWidth, screenHeight,
                                     scale, 220.0f, 54.0f, OverlayGray(shade),
                                     textures, loader, renderer, backend))
                return false;
        }
    }

    if (cycle > 1 || (cycle == 1 && pathTime > 10.0f)) {
        if (!DrawCenteredOverlay("logos/endtext.cbm", screenWidth, screenHeight,
                                 scale, 206.0f, 206.0f, 0x00969696u,
                                 textures, loader, renderer, backend))
            return false;
    } else if (cycle == 1 && !(pathTime > 10.0f)) {
        const std::int32_t shade = OverlayTrunc(
            static_cast<long double>(pathTime) *
            static_cast<long double>(0.10000000149011612f) * 150.0L);
        if (!DrawCenteredOverlay("logos/endtext.cbm", screenWidth, screenHeight,
                                 scale, 206.0f, 206.0f, OverlayGray(shade),
                                 textures, loader, renderer, backend))
            return false;
    }
    return true;
}


bool SceneFail(std::string* error, std::string message) {
    if (error) *error = std::move(message);
    return false;
}

bool ReadBytes(const std::filesystem::path& path,
               std::vector<std::uint8_t>& out) {
    std::ifstream input(path, std::ios::binary);
    if (!input)
        return false;
    out.assign(std::istreambuf_iterator<char>(input),
               std::istreambuf_iterator<char>());
    return true;
}

class SceneEnv final : public EngineEnvIO {
public:
    SceneEnv(CBMManager& textures,
                            CBMLoader& loader,
                            RenderOptionsState& options,
                            RenderDevice& device)
        : textures_(textures), loader_(loader), options_(options), device_(device) {}

    void DeleteTexture(const std::string& filename) override {
        (void)textures_.Delete(filename);
    }

    std::uint8_t LoadTexture(const std::string& filename) override {
        return textures_.Load(filename, loader_);
    }

    void ActivateRenderSettings() override {
        LifecycleLog("Activate current render settings...");
        (void)options_.Activate(device_, true, false, fogColor_);
    }

    void SetFogColor(std::uint32_t color) { fogColor_ = color; }

private:
    CBMManager& textures_;
    CBMLoader& loader_;
    RenderOptionsState& options_;
    RenderDevice& device_;
    std::uint32_t fogColor_ = 0;
};

struct FieldTemplate {
    FieldObjectConfig config;
    P3DModel model;
};

class FieldCollisionProxy final : public CD3DOBJECT {
public:
    explicit FieldCollisionProxy(const CD3DPOLYGONOBJECT& object)
        : object_(&object) {
        mesh_.bounds = WorldAabb{object.boundsMin, object.boundsMax};
        mesh_.vertices.reserve(object.sourceVertices.size());
        for (const ObjectVertex& vertex : object.sourceVertices) {
            mesh_.vertices.push_back(
                CD3DVECTOR{vertex.positionX, vertex.positionY, vertex.positionZ});
        }
        mesh_.faces.reserve(object.faces.size());
        for (const CD3DPOLYGON& face : object.faces) {
            mesh_.faces.push_back(WorldCollisionFace{
                face.p1, face.p2, face.p3,
                CD3DVECTOR{face.normalX, face.normalY, face.normalZ}});
        }
    }

    void Update(float) override {}
    void Render() override {}
    bool PendingWorldDelete() const override { return false; }
    const CD3DVECTOR& BoundsCenter() const override { return object_->center; }
    std::int16_t SectorIndex() const override { return 0; }
    float BoundsRadius() const override { return object_->boundRadius; }
    const WorldCollisionMesh* CollisionMesh() const override { return &mesh_; }

private:
    const CD3DPOLYGONOBJECT* object_ = nullptr;
    WorldCollisionMesh mesh_;
};

bool ReadDynamicBodyP3DName(const std::filesystem::path& gameRoot,
                            std::string_view dynamicConfigName,
                            std::string& p3dName,
                            std::string* error) {

    const std::filesystem::path configPath = gameRoot / "TRKDATA" / "DYNAMICS" /
                                             std::string(dynamicConfigName);
    CDFileOperations file;
    if (file.Open(configPath.string().c_str(), "rb", 0) != 0)
        return SceneFail(error, configPath.string());
    std::string firstLine;
    const bool first = CrashdayDirectory::ReadConfigLine(file, firstLine);
    const bool second = CrashdayDirectory::ReadConfigLine(file, p3dName);
    (void)file.Close();
    if (!first || !second || p3dName.empty())
        return SceneFail(error, configPath.string() + ": invalid dynamic-object header");
    return true;
}

}

bool ReadDynamicBodyModel(const std::filesystem::path& gameRoot,
                                     std::string_view dynamicConfigName,
                                     std::string& bodyP3D,
                                     std::string* error) {
    return ReadDynamicBodyP3DName(gameRoot, dynamicConfigName, bodyP3D, error);
}

CD3DVECTOR ResolveDropGroundCenter(const CD3DVECTOR& dropPosition,
                                      float modelHeight,
                                      bool hit,
                                      const CD3DVECTOR& hitPoint) {
    if (!hit)
        return CD3DVECTOR{dropPosition.x, -10.0f, dropPosition.z};
    return CD3DVECTOR{hitPoint.x, hitPoint.y + modelHeight * 0.5f, hitPoint.z};
}

struct CAR_RENDER {
    CD3DCAROBJECT* object = nullptr;
    std::string_view asset;
    std::string_view objectName;
    std::uint32_t color = 0;
    std::uint8_t spoilerMode = 0;
    std::uint8_t missileMode = 0;
    std::uint8_t minigun = 0;
    std::uint8_t afterburnerVariant = 0;
    CarSpecs specs{};
    CD3DPOLYGONOBJECT body{};
    std::array<CD3DPOLYGONOBJECT, 4> wheels{};
    std::optional<CD3DPOLYGONOBJECT> spoiler{};
    std::optional<CD3DPOLYGONOBJECT> minigunBody{};
    std::optional<CD3DPOLYGONOBJECT> minigunBarrel{};
    std::optional<CD3DTEXPARTICLEOBJECT> minigunShotfire{};
    std::optional<CD3DTEXPARTICLEOBJECT> minigunBullets{};
    std::array<float, 4> wheelRollAngle{};
    std::vector<CarLightState> lightStates{};
    alignas(4) std::array<std::uint8_t, 0xFE0> minigunRuntime{};
    bool loaded = false;
    CD3DVECTOR position{};
    float yaw = 0.0f;
    float speed = 0.0f;
    CD3DVECTOR linearVelocity{};
    CD3DVECTOR linearImpulse{};
    std::int16_t mass = 1;
    std::int8_t currentGear = 0;
    float forwardCommand = 0.0f;
    float reverseCommand = 0.0f;
    bool physics = false;
    bool lights = false;
    std::uint8_t missileCount = 0;
    std::uint32_t minigunResource = 0;
    alignas(16) std::array<std::array<std::byte, CarObjectSizes::Wheel>, 4> nativeWheels{};
    bool nativeRuntimeReady = false;
};

struct PARTICLEFX { CD3DTEXPARTICLEOBJECT runtime{}; };
struct MISSILE_RUNTIME {
    std::string name;
    CD3DPOLYGONOBJECT body{};
    CD3DMISSILEOBJECT object{};
    alignas(4) std::array<std::array<std::byte, 0x20>, 8> localBounds{};
    std::optional<CD3DTEXPARTICLEOBJECT> trail{};
    bool active = false;
};

static CD3DCAROBJECT* Wolf = nullptr;
static CD3DCAROBJECT* Cycore = nullptr;
static CD3DCAROBJECT* Apachee = nullptr;
static CD3DCAROBJECT* Wrecker = nullptr;
static CD3DCAROBJECT* Wrecker2 = nullptr;

static CAR_RENDER WolfRender{};
static CAR_RENDER CycoreRender{};
static CAR_RENDER ApacheeRender{};
static CAR_RENDER WreckerRender{};
static CAR_RENDER Wrecker2Render{};

static std::array<CD3DCAROBJECT*, 5> CarList() {
    return {{Wolf, Cycore, Apachee, Wrecker, Wrecker2}};
}

static CAR_RENDER& CarRender(CD3DCAROBJECT& car) {
    if (&car == Wolf) return WolfRender;
    if (&car == Cycore) return CycoreRender;
    if (&car == Apachee) return ApacheeRender;
    if (&car == Wrecker) return WreckerRender;
    return Wrecker2Render;
}
static const P3DLightingEnvironment* CarLighting = nullptr;
static CBMManager* CarTextures = nullptr;
static CBMLoader* CarLoader = nullptr;
static ParticleUse ParticleMode = ParticleUse::Always;
static std::optional<P3DModel> MissileModel{};
static std::int16_t MissileMass = 1;
static std::vector<PARTICLEFX> Effects{};
static std::vector<std::unique_ptr<MISSILE_RUNTIME>> Missiles{};

class DemoCarMissileIO final : public CarMissileIO {
public:
    std::string_view CarName(void* car) const override;
    bool DynamicNameExists(std::string_view fullName) const override;
    void* CreateMissile(std::size_t nativeBytes,
                                      std::string_view fullName,
                                      void* ownerCar,
                                      const CD3DVECTOR& position,
                                      const CD3DVECTOR& direction) override;
};
static DemoCarMissileIO DemoMissileBackend{};

static float FrameDelta = 0.0f;
static const WorldState* CDWorld = nullptr;
static std::string CarError;
static std::int8_t CycoreYawPhase = 0;
static std::uint8_t CycoreVectorPhase = 0;
static std::uint8_t CycoreExplosionPhase = 0;

static bool InitCars(const std::filesystem::path&, CBMManager&, CBMLoader&,
                     const P3DLightingEnvironment&, ParticleUse, std::string*);
static bool UpdateCars(float, float, bool, const P3DLightingEnvironment&, std::string*);
static bool RenderCars(RendererState&, RenderDevice&, std::string*);
static bool RenderWeapons(RendererState&, RenderDevice&, std::string*);
static void ResetCars();
static void ShutdownCars();
static std::size_t LoadedCars();

static bool LoadCar(CAR_RENDER&, const std::filesystem::path&, CBMManager&, CBMLoader&, const P3DLightingEnvironment&, std::string*);
static bool ApplyColor(CAR_RENDER&, CBMManager&, CBMLoader&, std::string*);
static bool ApplyRigidPose(CAR_RENDER&, const CD3DVECTOR&, float, const P3DLightingEnvironment&, std::string*);
static bool UpdateLightVisuals(CAR_RENDER&, float, bool, const P3DLightingEnvironment&, std::string*);
static void InitNativeCar(CAR_RENDER&);
static void SyncNativeWheelRuntime(CAR_RENDER&);
static void IntegratePhysics(CAR_RENDER&, float);
static void UpdateScripts(float);
static void UpdateWolf(float);
static void UpdateCycore(float);
static void UpdateApachee(float);
static void UpdateWrecker(float);
static void UpdateWrecker2(float);
static void SetPosition(CAR_RENDER&, const CD3DVECTOR&);
static void SetPositionXYZ(CAR_RENDER&, const CD3DVECTOR&);
static void ResetDynamicMotion(CAR_RENDER&);
static void AddLinearImpulse(CAR_RENDER&, const CD3DVECTOR&);
static void NormalizeDynamicBasis(CAR_RENDER&);
static void SetPositionAndYaw(CAR_RENDER&, const CD3DVECTOR&, float);
static void SetPhysics(CAR_RENDER&, bool);
static void SetLights(CAR_RENDER&, bool);
static void SetMissileCount(CAR_RENDER&, std::uint8_t);
static void SetMinigunResource(CAR_RENDER&, std::uint32_t);
static void Forward(CAR_RENDER&, float);
static void Reverse(CAR_RENDER&, float);
static void FireMissile(CAR_RENDER&);
static void FireMinigun(CAR_RENDER&);
static void SpawnCycoreExplosion(const CD3DVECTOR&, float, float, int);
static CD3DVECTOR GetPosition(const CAR_RENDER&);

static void SetPosition(CD3DCAROBJECT&, const CD3DVECTOR&);
static void SetPositionXYZ(CD3DCAROBJECT&, const CD3DVECTOR&);
static void ResetDynamicMotion(CD3DCAROBJECT&);
static void AddLinearImpulse(CD3DCAROBJECT&, const CD3DVECTOR&);
static void NormalizeDynamicBasis(CD3DCAROBJECT&);
static void SetPositionAndYaw(CD3DCAROBJECT&, const CD3DVECTOR&, float);
static void SetPhysics(CD3DCAROBJECT&, bool);
static void SetLights(CD3DCAROBJECT&, bool);
static void SetMissileCount(CD3DCAROBJECT&, std::uint8_t);
static void SetMinigunResource(CD3DCAROBJECT&, std::uint32_t);
static void Forward(CD3DCAROBJECT&, float);
static void Reverse(CD3DCAROBJECT&, float);
static void FireMissile(CD3DCAROBJECT&);
static void FireMinigun(CD3DCAROBJECT&);
static CD3DVECTOR GetPosition(const CD3DCAROBJECT&);
static bool UpdateWheelVisuals(CAR_RENDER&, float, const P3DLightingEnvironment&, std::string*);
static bool UpdateMinigunVisual(CAR_RENDER&, float, const P3DLightingEnvironment&, std::string*);
static bool InitParticle(std::optional<CD3DTEXPARTICLEOBJECT>&, const ParticleConfig&, const CD3DVECTOR&, std::string_view);
static void UpdateMinigunEffects(CAR_RENDER&, bool, std::int32_t, std::int32_t);
static void SpawnParticle(const ParticleConfig&, const CD3DVECTOR&);
static void SpawnExplosionVisual(const CD3DVECTOR&, ExplosionType);
static void ApplyExplosionDynamics(const CD3DVECTOR&, float, float);
static void UpdateEffects(float);
static bool RenderEffects(RendererState&, std::string*);
static bool LoadMissileModel(const std::filesystem::path&, std::string*);

static void UpdateMissiles(float, const P3DLightingEnvironment&);

static std::filesystem::path GameRoot;
static CamFlightController FlyCtrl;
static CD3DCAMERA FlyCamera;
static EngineEnvironment Environment;
static SkyBoxTextures SkyTextures{};
static P3DLightingEnvironment Lighting{};
static std::vector<CD3DPOLYGONOBJECT> FieldObjPtr;
static std::vector<CD3DPOLYGONOBJECT> DropObjPtr;
static TrackFileData Track;
static WorldState CollisionWorld;
static std::vector<std::unique_ptr<CD3DOBJECT>> CollisionObjects;
static std::string RuntimeError;
static std::uint16_t FlyCycle = 0;
static float FlyTime = 0.0f;
static float RenderRange = 400.0f;
static bool FlySceneControllerReady = false;
static bool FlySceneWorldReady = false;

static bool InitFlyScene(const std::filesystem::path& gameRoot,
                                 std::string selectedAmbience,
                                 CBMManager& textures,
                                 CBMLoader& loader,
                                 RendererState& renderer,
                                 RenderOptionsState& options,
                                 RenderDevice& device,
                                 std::string* error) {
    FieldObjPtr.clear();
    DropObjPtr.clear();
    GameRoot = gameRoot;
    FlyCycle = 0;
    FlyTime = 0.0f;
    RenderRange = options.range;
    RuntimeError.clear();

    
    
    
    CollisionWorld = WorldState{};
    CollisionObjects.clear();
    CollisionWorld.Sectors().resize(1);
    CollisionWorld.Sectors().front().staticObjects.reserve(
        WorldState::MaxStaticObjectsPerSector);
    FlySceneWorldReady = true;

    LifecycleLog("Create empty world...");
    LifecycleLog("Create world sectors...");
    LifecycleLog(std::to_string(CollisionWorld.Sectors().size()) +
                 " sectors needed");
    LifecycleLog(std::to_string(CollisionWorld.Sectors().size()) +
                 " sectors with " +
                 std::to_string(CollisionWorld.Sectors().front().staticObjects.capacity()) +
                 " StaticObject pointers per sectors prepared");
    LifecycleLog(std::to_string(CollisionWorld.Dynamics().size()) +
                 " DynamicObject pointers prepared");
    LifecycleLog("World initialised");
    LifecycleLog("");

    LifecycleLog("Startup CamFlightController...");
    FlySceneControllerReady = true;
    LifecycleLog("Load path file...");
    const std::filesystem::path cameraPath = GameRoot /
        std::string(kCameraPath);
    if (!FlyCtrl.LoadPath(cameraPath.string()))
        return SceneFail(error, "newflydemo.pth");
    LifecycleLog(std::to_string(FlyCtrl.GetNumFrames()) + " camera keys");
    if (!FlyCtrl.Update(0.0f, &FlyCamera))
        return SceneFail(error, "newflydemo.pth");
    FlyTime = FlyCtrl.GetTime();
    LifecycleLog("CamFlightController created");
    LifecycleLog("");

    LifecycleLog("Load ambience system...");
    std::size_t ambienceCount = 0;
    std::error_code ambienceDirectoryError;
    const std::filesystem::path ambienceDirectory =
        GameRoot / AmbienceSystem::SourceDirectory;
    for (std::filesystem::directory_iterator it(ambienceDirectory, ambienceDirectoryError), end;
         !ambienceDirectoryError && it != end; it.increment(ambienceDirectoryError)) {
        if (it->is_regular_file(ambienceDirectoryError) &&
            CBMManager::CanonicalName(it->path().extension().string()) == ".amb")
            ++ambienceCount;
    }
    LifecycleLog("AmbienceSystem created: " + std::to_string(ambienceCount) +
                 " times of day available");
    LifecycleLog("");
    LifecycleLog("Choosing ambience '" + selectedAmbience + "'");

    const std::filesystem::path ambiencePath = GameRoot /
        AmbienceSystem::SourceDirectory / selectedAmbience;
    CDFileOperations ambienceFile;
    if (ambienceFile.Open(ambiencePath.string().c_str(), "r+", 1) != 0)
        return SceneFail(error, ambiencePath.string());
    AMBIENCE_ENTRY ambience;
    const AmbienceParseResult ambienceResult =
        ReadAmbienceFile(ambienceFile, selectedAmbience, ambience);
    (void)ambienceFile.Close();
    if (!ambienceResult.ok)
        return SceneFail(error, ambiencePath.string() + ": " + ambienceResult.error);

    SceneEnv environment(textures, loader, options, device);
    environment.SetFogColor(ambience.environmentTail.packedColor8);
    LifecycleLog("sky box setup...");
    Environment.ApplyEnvironment(ambience.ambientColor, ambience.light,
                                  ambience.environmentTail, ambience.resource,
                                  false, environment);
    LifecycleLog("AmbienceSystem shut down");
    const EngineEnvironmentState& env = Environment.State();
    SkyTextures.top = env.skyTextureSlots[0];
    SkyTextures.bottom = env.skyTextureSlots[1];
    SkyTextures.side = env.skyTextureSlots[2];
    renderer.SetEnvironmentTexture(env.environmentTextureSlot);
    Lighting.direction = env.light.position;
    Lighting.directionalColor = env.light.packedColor;
    Lighting.ambientColor = env.ambientColor;

    if (SkyTextures.top == CBMManager::InvalidTexture ||
        SkyTextures.bottom == CBMManager::InvalidTexture ||
        SkyTextures.side == CBMManager::InvalidTexture ||
        env.environmentTextureSlot == CBMManager::InvalidTexture)
        return SceneFail(error, "ambience CBM");

    if (!InitCars(GameRoot, textures, loader, Lighting, options.particles, error))
        return false;

    LifecycleLog("Create track object...");
    LifecycleLog("Create new track...");
    LifecycleLog("Associate track's 3D data...");
    const std::filesystem::path trackPath = GameRoot /
        BuildTrackPath(kTrackFile);
    std::vector<std::uint8_t> trackBytes;
    if (!ReadBytes(trackPath, trackBytes))
        return SceneFail(error, trackPath.string());
    Track = TrackFileData{};
    std::string parseError;
    if (!ParseTrackFile(trackBytes, Track, &parseError))
        return SceneFail(error, trackPath.string() + ": " + parseError);
    Track.trackName = DeriveTrackLoadName(kTrackFile);

    std::vector<FieldTemplate> templates(Track.fieldFiles.size());
    std::vector<float> modelHeights(Track.fieldFiles.size(), 0.0f);
    for (std::size_t i = 0; i < Track.fieldFiles.size(); ++i) {
        const std::filesystem::path configPath = GameRoot /
            kFieldObjectDirectory / Track.fieldFiles[i];
        CDFileOperations fieldFile;
        if (fieldFile.Open(configPath.string().c_str(), kFieldOpenMode, 1) != 0)
            return SceneFail(error, configPath.string());
        if (!ReadFieldObjectConfig(fieldFile, templates[i].config, &parseError)) {
            (void)fieldFile.Close();
            return SceneFail(error, configPath.string() + ": " + parseError);
        }
        (void)fieldFile.Close();

        const std::filesystem::path p3dPath = GameRoot / "EDITOR" /
            templates[i].config.p3dFilename;
        std::vector<std::uint8_t> p3dBytes;
        if (!ReadBytes(p3dPath, p3dBytes))
            return SceneFail(error, p3dPath.string());
        if (!ParseP3DModel(p3dBytes, templates[i].model, &parseError))
            return SceneFail(error, p3dPath.string() + ": " + parseError);
        if (!ValidateFieldP3DExtents(templates[i].model, &parseError))
            return SceneFail(error, p3dPath.string() + ": " + parseError);
        modelHeights[i] = templates[i].model.sizeY;
    }

    std::vector<TrackFieldSpawn> plans;
    if (!BuildTrackFields(Track, modelHeights, plans, &parseError))
        return SceneFail(error, "demo.trk: " + parseError);

    FieldObjPtr.reserve(plans.size());
    for (const TrackFieldSpawn& plan : plans) {
        if (plan.fieldFileIndex >= templates.size())
            return SceneFail(error, "demo.trk: invalid field index");
        const FieldTemplate& fieldTemplate = templates[plan.fieldFileIndex];
        CD3DPOLYGONOBJECT object;
        if (!object.InitializeFromModel(fieldTemplate.model, textures, loader, &parseError))
            return SceneFail(error, fieldTemplate.config.p3dFilename + ": " + parseError);
        if (!PrepareStaticField(object, plan.placement, Lighting, &parseError))
            return SceneFail(error, plan.fieldConfigName + ": " + parseError);

        if (!object.ApplyPendingTransform(Lighting, &parseError))
            return SceneFail(error, plan.fieldConfigName + ": " + parseError);
        FieldObjPtr.push_back(std::move(object));
    }

    std::vector<TrackDropSpawn> dropPlans;
    if (!BuildTrackDrops(Track, dropPlans, &parseError))
        return SceneFail(error, "demo.trk: " + parseError);

    SECTOR& collisionRoot = CollisionWorld.Sectors()[0];
    CollisionObjects.reserve(FieldObjPtr.size());
    if (FieldObjPtr.size() > collisionRoot.staticObjects.capacity())
        collisionRoot.staticObjects.reserve(FieldObjPtr.size());
    for (const CD3DPOLYGONOBJECT& field : FieldObjPtr) {
        auto proxy = std::make_unique<FieldCollisionProxy>(field);
        collisionRoot.staticObjects.push_back(proxy.get());
        CollisionObjects.push_back(std::move(proxy));
    }
    CDWorld = &CollisionWorld;

    std::unordered_map<std::string, P3DModel> dynamicModels;
    DropObjPtr.reserve(dropPlans.size());
    for (const TrackDropSpawn& plan : dropPlans) {
        auto modelIt = dynamicModels.find(plan.dynamicConfigName);
        if (modelIt == dynamicModels.end()) {
            std::string p3dName;
            if (!ReadDynamicBodyModel(GameRoot, plan.dynamicConfigName,
                                                 p3dName, &parseError))
                return SceneFail(error, parseError);
            std::vector<std::uint8_t> p3dBytes;
            const std::filesystem::path p3dPath = GameRoot / "EDITOR" / p3dName;
            if (!ReadBytes(p3dPath, p3dBytes))
                return SceneFail(error, p3dPath.string());
            P3DModel model;
            if (!ParseP3DModel(p3dBytes, model, &parseError))
                return SceneFail(error, p3dPath.string() + ": " + parseError);
            modelIt = dynamicModels.emplace(plan.dynamicConfigName,
                                            std::move(model)).first;
        }

        CD3DVECTOR traceStart = plan.position;
        CD3DVECTOR traceEnd = plan.position;
        traceStart.y = 100.0f;
        traceEnd.y = -10.0f;
        const WorldSegmentHit ground =
            CollisionWorld.TraceStaticSegment(traceStart, traceEnd);
        const CD3DVECTOR groundedCenter = ResolveDropGroundCenter(
            plan.position, modelIt->second.sizeY, ground.hit, ground.point);

        CD3DPOLYGONOBJECT object;
        if (!object.InitializeFromModel(modelIt->second, textures, loader, &parseError))
            return SceneFail(error, plan.dynamicConfigName + ": " + parseError);
        object.SetCenterPosition(groundedCenter);
        if (plan.rotation != 0.0f)
            object.RotateY(groundedCenter, plan.rotation);
        if (!object.ApplyPendingTransform(Lighting, &parseError))
            return SceneFail(error, plan.dynamicConfigName + ": " + parseError);
        DropObjPtr.push_back(std::move(object));
    }

    RuntimeError.clear();
    if (!UpdateCars(FlyTime, 0.0f, false, Lighting, &RuntimeError))
        return SceneFail(error, RuntimeError.empty() ? "car runtime" : RuntimeError);

    for (std::string_view name : kPreloadResources)
        (void)textures.Load(name, loader);

    if (error) error->clear();
    return true;
}

static void UpdateFlyScene(float dt) {
    RuntimeError.clear();
    if (!UpdateCars(FlyTime, dt, false, Lighting, &RuntimeError) &&
        RuntimeError.empty())
        RuntimeError = "car runtime";

    (void)FlyCtrl.Update(dt, &FlyCamera);
    const float now = FlyCtrl.GetTime();
    if (!(now >= FlyTime)) {
        ++FlyCycle;
        ResetCars();
    }
    FlyTime = now;
}

static bool RenderFlyWorld(std::int32_t screenWidth,
                            std::int32_t screenHeight,
                            RendererState& renderer,
                            RenderDevice& device,
                            std::string* error) {
    if (!RuntimeError.empty())
        return SceneFail(error, RuntimeError);
    FrameSetup frame{};
    frame.cameraPosition = FlyCamera.GetPosition();
    frame.cameraRotation = FlyCamera.GetRotation();
    frame.screenWidth = screenWidth;
    frame.screenHeight = screenHeight;
    frame.farClip = RenderRange;
    if (!renderer.BeginScene(device, frame))
        return SceneFail(error, "BEGINSCENE");
    const auto failAfterBegin = [&](const char* stage) {
        if (renderer.SceneInProgress()) (void)renderer.EndScene(device);
        return SceneFail(error, stage);
    };
    if (!renderer.RenderSkyBox(FlyCamera.GetPosition(), SkyTextures, device))
        return failAfterBegin("SKYBOX");
    for (CD3DPOLYGONOBJECT& field : FieldObjPtr) {
        if (renderer.ClassifySphere(field.center, field.boundRadius, true, device) ==
            SphereVisibility::Outside) continue;
        if (!field.Render(renderer, device, error)) return failAfterBegin("FIELD");
    }
    if (!RenderCars(renderer, device, error)) return failAfterBegin("CARS");
    for (CD3DPOLYGONOBJECT& object : DropObjPtr) {
        if (renderer.ClassifySphere(object.center, object.boundRadius, true, device) ==
            SphereVisibility::Outside) continue;
        if (!object.Render(renderer, device, error)) return failAfterBegin("DYNAMIC");
    }
    if (!RenderWeapons(renderer, device, error)) return failAfterBegin("WEAPONS");
    if (!renderer.FlushDeferredTriangles(device)) return failAfterBegin("DEFERRED");
    if (error) error->clear();
    return true;
}

static bool FinishFlyFrame(std::int32_t screenWidth,
                             std::int32_t screenHeight,
                             CBMManager& textures,
                             CBMLoader& loader,
                             RendererState& renderer,
                             RenderDevice& device,
                             std::string* error) {
    if (!RenderOverlay(screenWidth, screenHeight,
                           static_cast<std::int16_t>(FlyCycle),
                           FlyCtrl.GetTime(), textures, loader, renderer, device)) {
        if (renderer.SceneInProgress()) (void)renderer.EndScene(device);
        return SceneFail(error, "OVERLAY");
    }
    if (!renderer.EndScene(device))
        return SceneFail(error, "ENDSCENE");
    if (error) error->clear();
    return true;
}

static bool RenderFlyFrame(std::int32_t screenWidth,
                       std::int32_t screenHeight,
                       CBMManager& textures,
                       CBMLoader& loader,
                       RendererState& renderer,
                       RenderDevice& device,
                       std::string* error) {
    return RenderFlyWorld(screenWidth, screenHeight, renderer, device, error) &&
           FinishFlyFrame(screenWidth, screenHeight, textures, loader,
                        renderer, device, error);
}

static void CloseFlyScene() {
    
    
    
    if (FlySceneControllerReady) {
        FlyCtrl = CamFlightController{};
        FlySceneControllerReady = false;
        LifecycleLog("CamFlightController shut down");
        LifecycleLog("");
    }

    ShutdownCars();
    CDWorld = nullptr;

    if (FlySceneWorldReady) {
        LifecycleLog("World is deleted...");
        LifecycleLog("Remove all static objects from world...");

        for (SECTOR& sector : CollisionWorld.Sectors())
            sector.staticObjects.clear();
        CollisionObjects.clear();

        LifecycleLog("Remove all dynamic objects from world...");
        CollisionWorld.Dynamics().fill(nullptr);
        CollisionWorld = WorldState{};
        FlySceneWorldReady = false;
        LifecycleLog("");
    } else {
        CollisionObjects.clear();
        CollisionWorld = WorldState{};
    }

    DropObjPtr.clear();
    FieldObjPtr.clear();
    Track = TrackFileData{};
    Environment = EngineEnvironment{};
    SkyTextures = {};
    Lighting = {};
    GameRoot.clear();
    FlyCycle = 0;
    FlyTime = 0.0f;
    RuntimeError.clear();
}

}

namespace flydemo {
namespace {

bool CarFail(std::string* error, std::string message) {
    if (error) *error = std::move(message);
    return false;
}

bool ReadP3D(const std::filesystem::path& path, P3DModel& model,
             std::string* error) {
    std::ifstream input(path, std::ios::binary);
    if (!input)
        return CarFail(error, path.string());
    std::vector<std::uint8_t> bytes((std::istreambuf_iterator<char>(input)),
                                    std::istreambuf_iterator<char>());
    std::string parseError;
    if (!ParseP3DModel(bytes, model, &parseError))
        return CarFail(error, path.string() + ": " + parseError);
    return true;
}

CD3DVECTOR MirrorX(CD3DVECTOR p) {
    p.x = -p.x;
    return p;
}

CD3DVECTOR WheelMount(const CarSpecs& specs, unsigned wheel) {
    switch (wheel) {
    case 0: return specs.upperWheelMount;
    case 1: return MirrorX(specs.upperWheelMount);
    case 2: return MirrorX(specs.lowerWheelMount);
    default:return specs.lowerWheelMount;
    }
}

CD3DVECTOR RotateLocalY(const CD3DVECTOR& v, float angle) {
    const long double radians = static_cast<long double>(angle) *
                                static_cast<long double>(kRadiansPerAngleUnit);
    const float s = static_cast<float>(std::sin(radians));
    const float c = static_cast<float>(std::cos(radians));
    return CD3DVECTOR{v.x * c + v.z * s, v.y, -v.x * s + v.z * c};
}

float ClampRate(float rate) {
    return std::max(0.0f, std::min(100.0f, rate));
}

class TexPartSystem final : public TexPartIO {
public:
    TexPartSystem(CBMManager& textures, CBMLoader& loader)
        : textures_(textures), loader_(loader) {}
    int RandomInt() override { return NativeRandom(); }
    bool EnsureTextureLoaded(std::string_view name) override {
        return textures_.Load(name, loader_) != CBMManager::InvalidTexture;
    }
    std::uint8_t TextureFrameCount(std::string_view name) const override {
        const CBMPicture* p = textures_.Get(name);
        return p ? p->FrameCount() : 1u;
    }
    std::uint8_t TextureIndex(std::string_view name) const override {
        return textures_.IndexOf(name);
    }
private:
    CBMManager& textures_;
    CBMLoader& loader_;
};


std::int16_t ReadCarDynamicMass(const std::filesystem::path& gameRoot,
                                std::string_view carFile) {
    const std::filesystem::path path = gameRoot / "TRKDATA" / "CARS" / std::string(carFile);
    CDFileOperations file;
    if (file.Open(path.string().c_str(), "rb", 0) != 0)
        return 1;
    std::string line;
    bool materialSeen = false;
    std::int16_t mass = 1;
    while (CrashdayDirectory::ReadConfigLine(file, line)) {
        if (line == "Crashday-CarObject-File")
            break;
        if (materialSeen) {
            const long value = std::strtol(line.c_str(), nullptr, 10);
            if (value > 0 && value <= 32767)
                mass = static_cast<std::int16_t>(value);
            break;
        }
        materialSeen = line == "METAL" || line == "STONE" || line == "WOOD" ||
                       line == "PLASTIC" || line == "RUBBER";
    }
    (void)file.Close();
    return mass;
}

CD3DVECTOR AddVec(const CD3DVECTOR& a, const CD3DVECTOR& b) {
    return {a.x + b.x, a.y + b.y, a.z + b.z};
}
CD3DVECTOR ScaleVec(const CD3DVECTOR& a, float s) {
    return {a.x * s, a.y * s, a.z * s};
}

CD3DVECTOR TransformByBasis(const CD3DVECTOR& local, const CD3DPOLYGONOBJECT& object) {

    return {
        object.right.x * local.x + object.up.x * local.y + object.forward.x * local.z,
        object.right.y * local.x + object.up.y * local.y + object.forward.y * local.z,
        object.right.z * local.x + object.up.z * local.y + object.forward.z * local.z,
    };
}

}

void MirrorP3DModelX(P3DModel& model) {

    const float twiceCenterX = model.sizeX;
    for (auto& vertex : model.vertices)
        vertex.position.x = twiceCenterX - vertex.position.x;
    for (auto& face : model.faces) {
        std::swap(face.p1, face.p2);
        std::swap(face.u1, face.u2);
        std::swap(face.v1, face.v2);
    }
}

float ComputeWheelRollRate(float wheelSpeed, float wheelDiameter) {

    return static_cast<float>(
        static_cast<long double>(wheelSpeed) /
        (static_cast<long double>(wheelDiameter) * 0.5L) *
        0.15915495087284554L * 256.0L);
}

bool ReadCarBodyModel(const std::filesystem::path& gameRoot,
                                   std::string_view carFile,
                                   std::string& bodyP3D,
                                   std::string* error) {

    const std::filesystem::path ccaPath = gameRoot / "TRKDATA" / "CARS" /
                                          std::string(carFile);
    CDFileOperations file;
    if (file.Open(ccaPath.string().c_str(), "rb", 0) != 0)
        return CarFail(error, ccaPath.string());
    std::string firstLine;
    const bool first = CrashdayDirectory::ReadConfigLine(file, firstLine);
    const bool second = CrashdayDirectory::ReadConfigLine(file, bodyP3D);
    (void)file.Close();
    if (!first || !second || bodyP3D.empty())
        return CarFail(error, ccaPath.string() + ": invalid dynamic-object header");
    if (error) error->clear();
    return true;
}


static void ResetCars() {
    const auto reset = [](CD3DCAROBJECT& car, const CD3DVECTOR& position, float yaw) {
        SetPosition(car, position);
        ResetDynamicMotion(car);
        NormalizeDynamicBasis(car);
        SetPositionAndYaw(car, position, yaw);
    };

    reset(*Wolf, kWolfStart, -3.0f);
    SetLights(*Wolf, false);
    SetPhysics(*Wolf, false);

    reset(*Cycore, kCycoreStart, 192.0f);
    SetLights(*Cycore, false);
    SetPhysics(*Cycore, false);
    SetMissileCount(*Cycore, 20);
    CycoreYawPhase = 0;
    CycoreVectorPhase = 0;
    CycoreExplosionPhase = 0;

    reset(*Apachee, kApacheeStart, 128.0f);
    SetLights(*Apachee, false);
    SetPhysics(*Apachee, false);

    reset(*Wrecker, kWreckerStart, -10.0f);
    SetLights(*Wrecker, true);
    SetPhysics(*Wrecker, false);
    SetMissileCount(*Wrecker, 20);
    SetMinigunResource(*Wrecker, 5000);

    reset(*Wrecker2, kWrecker2Start, 3.0f);
    SetLights(*Wrecker2, true);
    SetPhysics(*Wrecker2, false);
    SetMissileCount(*Wrecker2, 20);
    SetMinigunResource(*Wrecker2, 5000);
}

static void UpdateScripts(float pathTime) {
    UpdateWolf(pathTime);
    UpdateCycore(pathTime);
    UpdateApachee(pathTime);
    UpdateWrecker(pathTime);
    UpdateWrecker2(pathTime);
}

static void UpdateWolf(float t) {
    if (closed(t, 34.0f, 35.0f))
        SetPhysics(*Wolf, true);

    if (static_cast<double>(t) >= 35.5 && static_cast<double>(t) <= 36.5)
        SetLights(*Wolf, true);

    if (open(t, 36.0f, 44.0f))
        Forward(*Wolf, 20.0f);

    if (closed(t, 55.0f, 56.0f))
        SetPhysics(*Wolf, false);
}

static void UpdateCycore(float t) {
    if (closed(t, 133.0f, 134.0f))
        SetPhysics(*Cycore, true);

    if (closedOpen(t, 133.5, 134.5))
        SetPosition(*Cycore,
                         CD3DVECTOR{182.0f, 0.7599999904632568f, -87.0f});

    if (static_cast<double>(t) > 133.5 && t < 141.0f)
        Forward(*Cycore, 5.0f);
    if (t > 141.0f && t < 144.0f)
        Forward(*Cycore, 50.0f);

    if (static_cast<double>(t) >= 138.5 && static_cast<double>(t) <= 139.5)
        SetLights(*Cycore, true);

    if (closed(t, 145.0f, 148.0f))
        FireMinigun(*Cycore);
    if (static_cast<double>(t) >= 148.0 && static_cast<double>(t) <= 148.5)
        Reverse(*Cycore, 100.0f);

    if (static_cast<double>(t) >= 148.1 && t <= 149.0f &&
        CycoreYawPhase == 0) {
        SetPositionAndYaw(*Cycore,
                               GetPosition(*Cycore), -72.0f);
        CycoreYawPhase = static_cast<std::int8_t>(0xb8);
    }

    if (closed(t, 149.0f, 150.0f))
        FireMissile(*Cycore);

    if (static_cast<double>(t) >= 148.5 && CycoreVectorPhase == 0) {
        SetPositionXYZ(
            *Cycore,
            CD3DVECTOR{43.326381683349609f, 0.7599999904632568f, -86.634315490722656f});
        ResetDynamicMotion(*Cycore);
        AddLinearImpulse(*Cycore,
                         CD3DVECTOR{-11.060440063476562f, 0.0f,
                                    3.1265079975128174f});
        CycoreVectorPhase = 1;
    }

    if (closed(t, 149.0f, 153.0f))
        Forward(*Cycore, 60.0f);

    if (static_cast<double>(t) >= 150.2 && CycoreVectorPhase == 1) {
        SetPositionXYZ(
            *Cycore,
            CD3DVECTOR{43.0f, 0.7599999904632568f, -89.493484497070312f});
        ResetDynamicMotion(*Cycore);
        AddLinearImpulse(*Cycore,
                         CD3DVECTOR{0.5032622814178467f, 0.0f,
                                    -6.431215286254883f});
        CycoreVectorPhase = 2;
    }

    if (static_cast<double>(t) >= 150.8 && static_cast<double>(t) <= 151.2 &&
        CycoreYawPhase == static_cast<std::int8_t>(0xb8)) {
        SetPositionAndYaw(*Cycore,
                               GetPosition(*Cycore), 9.5f);
        CycoreYawPhase = static_cast<std::int8_t>(0xc0);
    }

    if (t >= 153.0f && static_cast<double>(t) <= 153.4)
        FireMissile(*Cycore);
    if (static_cast<double>(t) >= 152.4 && t <= 156.0f)
        Forward(*Cycore, 100.0f);

    if (closed(t, 157.0f, 158.0f) && CycoreExplosionPhase == 0) {
        CD3DVECTOR p = GetPosition(*Cycore);
        p.z += -10.0f;
        SpawnCycoreExplosion(p, 10.0f, 0.1f, 2);
        CycoreExplosionPhase = 1;
    }

    if (closed(t, 157.0f, 160.0f) && CycoreExplosionPhase == 1) {
        SetPositionAndYaw(*Cycore,
                               GetPosition(*Cycore), 34.0f * FrameDelta);
    }

    if (static_cast<double>(t) >= 160.0 && static_cast<double>(t) <= 160.9) {
        SetPosition(*Cycore, kCycoreStart);
        ResetDynamicMotion(*Cycore);
    }

    if (closed(t, 161.0f, 162.0f))
        SetPhysics(*Cycore, false);
}

static void UpdateApachee(float t) {
    if (closed(t, 145.0f, 146.0f))
        SetPhysics(*Apachee, true);
    if (closed(t, 147.0f, 153.0f))
        Forward(*Apachee, 50.0f);
    if (closed(t, 153.0f, 154.0f))
        SetLights(*Apachee, true);
    if (static_cast<double>(t) >= 160.0 && static_cast<double>(t) <= 160.9) {
        SetPosition(*Apachee, kApacheeStart);
        ResetDynamicMotion(*Apachee);
    }
    if (closed(t, 161.0f, 162.0f))
        SetPhysics(*Apachee, false);
}

static void UpdateWrecker(float t) {
    if (closed(t, 195.0f, 196.0f)) {
        SetPhysics(*Wrecker, true);
        SetLights(*Wrecker, true);
    }
    if (closed(t, 200.0f, 202.0f))
        FireMinigun(*Wrecker);
    if (closed(t, 203.0f, 205.0f))
        FireMinigun(*Wrecker);
    if (closed(t, 205.0f, 206.0f))
        SetPhysics(*Wrecker, false);
    if (closed(t, 214.0f, 215.0f))
        FireMissile(*Wrecker);
}

static void UpdateWrecker2(float t) {
    if (closed(t, 195.0f, 196.0f)) {
        SetPhysics(*Wrecker2, true);
        SetLights(*Wrecker2, true);
    }
    if (closed(t, 204.0f, 208.0f))
        FireMinigun(*Wrecker2);
    if (closed(t, 205.0f, 206.0f))
        SetPhysics(*Wrecker2, false);
    if (closed(t, 215.0f, 216.0f))
        FireMissile(*Wrecker2);
}


static bool InitCars(const std::filesystem::path& gameRoot,
                                CBMManager& textures,
                                CBMLoader& loader,
                                const P3DLightingEnvironment& lighting,
                                ParticleUse particles,
                                std::string* error) {
    CarLighting = &lighting;
    CarTextures = &textures;
    CarLoader = &loader;
    ParticleMode = particles;
    FrameDelta = 0.0f;
    CDWorld = nullptr;
    CarError.clear();
    Effects.clear();
    Missiles.clear();
    MissileModel.reset();
    MissileMass = 1;
    delete Wolf; delete Cycore; delete Apachee; delete Wrecker; delete Wrecker2;
    Wolf = new CD3DCAROBJECT{};
    Cycore = new CD3DCAROBJECT{};
    Apachee = new CD3DCAROBJECT{};
    Wrecker = new CD3DCAROBJECT{};
    Wrecker2 = new CD3DCAROBJECT{};

    CarRender(*Wolf) = CAR_RENDER{Wolf, "wolf.cca", "car_wolf", 0x00323296u, 1, 0, 0, 0};
    CarRender(*Cycore) = CAR_RENDER{Cycore, "cycore.cca", "car_cycore", 0x000A206Au, 1, 2, 1, 2};
    CarRender(*Apachee) = CAR_RENDER{Apachee, "apachee.cca", "car_apachee", 0x00E0C500u, 0, 1, 1, 2};
    CarRender(*Wrecker) = CAR_RENDER{Wrecker, "wrecker.cca", "car_wrecker", 0x00000000u, 1, 2, 1, 2};
    CarRender(*Wrecker2) = CAR_RENDER{Wrecker2, "wrecker.cca", "car_wrecker2", 0x00000000u, 1, 2, 1, 2};

    for (CD3DCAROBJECT* object : CarList()) {
        if (!LoadCar(CarRender(*object), gameRoot, textures, loader, lighting, error))
            return false;
    }
    if (!LoadMissileModel(gameRoot, error))
        return false;
    Car_SetMissileIO(&DemoMissileBackend);

    ResetCars();
    if (!CarError.empty())
        return CarFail(error, CarError);
    if (error) error->clear();
    return true;
}

static bool LoadCar(CAR_RENDER& car,
                             const std::filesystem::path& gameRoot,
                             CBMManager& textures,
                             CBMLoader& loader,
                             const P3DLightingEnvironment& lighting,
                             std::string* error) {
    const CarSpecsParseResult specResult =
        LoadCarSpecs(gameRoot, std::string(car.asset), car.specs);
    car.mass = ReadCarDynamicMass(gameRoot, car.asset);
    if (car.object != nullptr) {
        ApplyCarSpecsToRaw(car.object, car.specs);
        flydemo::mem::field<std::int16_t>(car.object, off::PHYS_MASS) = car.mass;
        flydemo::mem::field<float>(car.object, off::CONDITION) = 100.0f;
        flydemo::mem::field<std::uint32_t>(car.object, off::CURRENT_COLOR) = car.color;
        flydemo::mem::field<std::uint8_t>(car.object, off::MISSILE_MODE) = car.missileMode;
        flydemo::mem::field<std::uint8_t>(car.object, off::MISSILE_COUNT) =
            car.missileMode != 0 ? car.specs.missileCapacity : 0;
        flydemo::mem::field<CD3DVECTOR>(car.object, off::OBJECT_FORWARD) = {0.0f, 0.0f, 1.0f};
        flydemo::mem::field<CD3DVECTOR>(car.object, off::OBJECT_UP) = {0.0f, 1.0f, 0.0f};
        flydemo::mem::field<CD3DVECTOR>(car.object, off::OBJECT_RIGHT) = {1.0f, 0.0f, 0.0f};
    }
    if (!specResult.parsed)
        return CarFail(error, (gameRoot / "TRKDATA" / "CARS" /
                            std::string(car.asset)).string());

    std::string bodyP3DName;
    if (!ReadCarBodyModel(gameRoot, car.asset, bodyP3DName, error))
        return false;

    const std::filesystem::path editorDir = gameRoot / "EDITOR";
    P3DModel bodyModel;
    P3DModel wheelModel;
    if (!ReadP3D(editorDir / bodyP3DName, bodyModel, error) ||
        !ReadP3D(editorDir / car.specs.resourceStrings[1], wheelModel, error))
        return false;

    std::string runtimeError;
    if (!car.body.InitializeFromModel(bodyModel, textures, loader, &runtimeError))
        return CarFail(error, std::string(car.asset) + " body: " + runtimeError);

    const int totalLightCount = static_cast<int>(car.specs.lightGroup0Count) +
                                static_cast<int>(car.specs.lightGroup1Count);
    if (totalLightCount < 0 ||
        static_cast<std::size_t>(totalLightCount) > car.body.embeddedLights.size())
        return CarFail(error, std::string(car.asset) + " light material table");
    car.lightStates.assign(static_cast<std::size_t>(totalLightCount), {});
    for (int i = 0; i < totalLightCount; ++i)
        car.lightStates[static_cast<std::size_t>(i)].baseColor =
            car.body.embeddedLights[static_cast<std::size_t>(i)].packedColor;
    for (unsigned i = 0; i < car.wheels.size(); ++i) {
        P3DModel model = wheelModel;

        if (i == 0u || i == 2u)
            MirrorP3DModelX(model);
        if (!car.wheels[i].InitializeFromModel(model, textures, loader, &runtimeError))
            return CarFail(error, std::string(car.asset) + " wheel: " + runtimeError);
    }

    if (car.spoilerMode == 1 && !car.specs.resourceStrings[2].empty()) {
        P3DModel spoilerModel;
        if (!ReadP3D(editorDir / car.specs.resourceStrings[2], spoilerModel, error))
            return false;
        car.spoiler.emplace();
        if (!car.spoiler->InitializeFromModel(spoilerModel, textures, loader,
                                              &runtimeError))
            return CarFail(error, std::string(car.asset) + " spoiler: " + runtimeError);
    }

    if (car.minigun == 1u) {
        P3DModel gunModel;
        P3DModel barrelModel;
        if (!ReadP3D(editorDir / "mgbody.p3d", gunModel, error) ||
            !ReadP3D(editorDir / "mgbarrel.p3d", barrelModel, error))
            return false;
        const MinigunSide side = car.specs.minigunMount.x < 0.0f
            ? MinigunSide::Left : MinigunSide::Right;
        if (side == MinigunSide::Right) {

            MirrorP3DModelX(gunModel);

            const float twiceCenterX = gunModel.sizeX;
            for (auto& light : gunModel.materials)
                light.position.x = twiceCenterX - light.position.x;
        }
        car.minigunBody.emplace();
        car.minigunBarrel.emplace();
        if (!car.minigunBody->InitializeFromModel(gunModel, textures, loader, &runtimeError) ||
            !car.minigunBarrel->InitializeFromModel(barrelModel, textures, loader, &runtimeError))
            return CarFail(error, std::string(car.asset) + " minigun: " + runtimeError);
        car.minigunRuntime.fill(0);
        Minigun_Init(car.minigunRuntime.data(), side, 500);
    }

    car.position = car.body.center;
    if (car.object != nullptr) {
        flydemo::mem::field<CD3DVECTOR>(car.object, off::OBJECT_BOUNDS_CENTER) = car.position;
        flydemo::mem::field<CD3DVECTOR>(car.object, off::PHYS_POSITION) = car.position;
    }
    car.yaw = 0.0f;
    car.loaded = true;

    for (unsigned i = 0; i < car.wheels.size(); ++i) {

        const CD3DVECTOR mount = WheelMount(car.specs, i);
        car.wheels[i].SetCenterPosition(CD3DVECTOR{car.position.x + mount.x,
                                             car.position.y + mount.y,
                                             car.position.z + mount.z});
        if (!car.wheels[i].ApplyPendingTransform(lighting, &runtimeError))
            return CarFail(error, std::string(car.asset) + " wheel mount: " + runtimeError);
    }
    if (car.spoiler) {
        const CD3DVECTOR m = car.specs.spoilerMount;
        car.spoiler->SetCenterPosition(CD3DVECTOR{car.position.x + m.x,
                                            car.position.y + m.y,
                                            car.position.z + m.z});
        if (!car.spoiler->ApplyPendingTransform(lighting, &runtimeError))
            return CarFail(error, std::string(car.asset) + " spoiler mount: " + runtimeError);
    }
    if (car.minigunBody && car.minigunBarrel) {
        const CD3DVECTOR m = car.specs.minigunMount;
        const CD3DVECTOR p{car.position.x + m.x, car.position.y + m.y, car.position.z + m.z};
        car.minigunBody->SetCenterPosition(p);
        if (!car.minigunBody->ApplyPendingTransform(lighting, &runtimeError))
            return CarFail(error, std::string(car.asset) + " minigun mount: " + runtimeError);

        const CD3DVECTOR barrelOffset = TransformByBasis(CD3DVECTOR{0.0f, 0.0f, 0.3f},
                                                   *car.minigunBody);
        car.minigunBarrel->SetCenterPosition(AddVec(p, barrelOffset));
        if (!car.minigunBarrel->ApplyPendingTransform(lighting, &runtimeError))
            return CarFail(error, std::string(car.asset) + " minigun mount: " + runtimeError);
    }

    InitNativeCar(car);

    if (!ApplyColor(car, textures, loader, error))
        return false;
    return true;
}

static bool ApplyColor(CAR_RENDER& car, CBMManager& textures,
                                CBMLoader& loader, std::string* error) {
    if (!car.specs.canChangeColors)
        return true;

    for (const std::string& source : car.specs.colorTextures) {
        std::string alias(car.objectName);
        alias += '_';
        alias += source;
        const std::uint8_t index = textures.LoadAs(source, alias, loader);
        if (index == CBMManager::InvalidTexture)
            return CarFail(error, std::string(car.asset) + " color texture: " + source);

        (void)car.body.ReplaceTexture(source, alias, textures);
        if (car.spoiler)
            (void)car.spoiler->ReplaceTexture(source, alias, textures);

        CBMPicture* picture = textures.Get(index);
        std::string colorError;
        if (picture && !picture->SetColor(car.color, &colorError))
            return CarFail(error, alias + ": " + colorError);
    }
    return true;
}

static bool ApplyRigidPose(CAR_RENDER& car, const CD3DVECTOR& newPosition,
                                    float deltaYaw,
                                    const P3DLightingEnvironment& lighting,
                                    std::string* error) {
    if (!car.loaded)
        return true;
    std::string e;
    auto rotate = [&](CD3DPOLYGONOBJECT& o) -> bool {
        if (deltaYaw != 0.0f) {
            o.RotateY(car.position, deltaYaw);
            if (!o.ApplyPendingTransform(lighting, &e))
                return false;
        }
        const CD3DVECTOR d{newPosition.x - car.position.x,
                     newPosition.y - car.position.y,
                     newPosition.z - car.position.z};
        if (d.x != 0.0f || d.y != 0.0f || d.z != 0.0f) {
            o.Translate(d.x, d.y, d.z);
            if (!o.ApplyPendingTransform(lighting, &e))
                return false;
        }
        return true;
    };

    if (!rotate(car.body)) return CarFail(error, e);
    for (auto& wheel : car.wheels)
        if (!rotate(wheel)) return CarFail(error, e);
    if (car.spoiler && !rotate(*car.spoiler)) return CarFail(error, e);
    if (car.minigunBody && !rotate(*car.minigunBody)) return CarFail(error, e);
    if (car.minigunBarrel && !rotate(*car.minigunBarrel)) return CarFail(error, e);

    car.position = newPosition;
    car.yaw += deltaYaw;
    if (car.object != nullptr) {
        flydemo::mem::field<CD3DVECTOR>(car.object, off::OBJECT_BOUNDS_CENTER) = car.position;
        flydemo::mem::field<CD3DVECTOR>(car.object, off::PHYS_POSITION) = car.position;
        CD3DMATRIX& orientation =
            flydemo::mem::field<CD3DMATRIX>(car.object, off::PHYS_MATRIX);
        MakeYRotation(orientation, car.yaw);
        flydemo::mem::field<CD3DVECTOR>(car.object, off::OBJECT_FORWARD) =
            RotateLocalY(CD3DVECTOR{0.0f, 0.0f, 1.0f}, car.yaw);
        flydemo::mem::field<CD3DVECTOR>(car.object, off::OBJECT_UP) =
            RotateLocalY(CD3DVECTOR{0.0f, 1.0f, 0.0f}, car.yaw);
        flydemo::mem::field<CD3DVECTOR>(car.object, off::OBJECT_RIGHT) =
            RotateLocalY(CD3DVECTOR{1.0f, 0.0f, 0.0f}, car.yaw);
    }
    return true;
}

static bool PointerFitsNative32(const void* p) {
    return reinterpret_cast<std::uintptr_t>(p) <=
           static_cast<std::uintptr_t>(std::numeric_limits<std::uint32_t>::max());
}

static void SyncNativeWheelRuntime(CAR_RENDER& car) {
    if (!car.nativeRuntimeReady || car.object == nullptr)
        return;

    const CD3DVECTOR forward =
        flydemo::mem::field<CD3DVECTOR>(car.object, off::OBJECT_FORWARD);
    const CD3DVECTOR up =
        flydemo::mem::field<CD3DVECTOR>(car.object, off::OBJECT_UP);
    const CD3DVECTOR right =
        flydemo::mem::field<CD3DVECTOR>(car.object, off::OBJECT_RIGHT);

    for (unsigned i = 0; i < car.nativeWheels.size(); ++i) {
        void* wheel = car.nativeWheels[i].data();
        const CD3DVECTOR mount = TransformByBasis(WheelMount(car.specs, i), car.body);
        const CD3DVECTOR center = AddVec(
            AddVec(car.body.center, mount),
            ScaleVec(car.body.up, -car.specs.wheelOffset));

        flydemo::mem::field<CD3DVECTOR>(wheel, off::OBJECT_BOUNDS_CENTER) = center;
        flydemo::mem::field<CD3DVECTOR>(wheel, off::PHYS_POSITION) = center;
        flydemo::mem::field<CD3DVECTOR>(wheel, off::OBJECT_FORWARD) = forward;
        flydemo::mem::field<CD3DVECTOR>(wheel, off::OBJECT_UP) = up;
        flydemo::mem::field<CD3DVECTOR>(wheel, off::OBJECT_RIGHT) = right;
        flydemo::mem::field<CD3DMATRIX>(wheel, off::PHYS_MATRIX) =
            flydemo::mem::field<CD3DMATRIX>(car.object, off::PHYS_MATRIX);

        const float lower = center.y -
            flydemo::mem::field<float>(wheel, off::OBJECT_SIZE_Y) * 0.5f;
        flydemo::mem::field<std::uint8_t>(wheel, off::WHEEL_GROUND_CONTACT) =
            static_cast<std::uint8_t>(!(lower > 0.2f));
    }
}

static void InitNativeCar(CAR_RENDER& car) {
    if (car.object == nullptr)
        return;

    car.nativeRuntimeReady = true;
    for (auto& storage : car.nativeWheels) {
        storage.fill(std::byte{0});
        if (!PointerFitsNative32(storage.data()))
            car.nativeRuntimeReady = false;
    }
    if (!car.nativeRuntimeReady)
        return;

    CD3DCAROBJECT* object = car.object;
    flydemo::mem::field<float>(object, off::OBJECT_SIZE_X) = car.body.sizeX;
    flydemo::mem::field<float>(object, off::OBJECT_SIZE_Y) = car.body.sizeY;
    flydemo::mem::field<float>(object, off::OBJECT_SIZE_Z) = car.body.sizeZ;
    flydemo::mem::field<CD3DVECTOR>(object, off::OBJECT_BOUNDS_CENTER) = car.body.center;
    flydemo::mem::field<CD3DVECTOR>(object, off::PHYS_POSITION) = car.body.center;
    SetIdentity(flydemo::mem::field<CD3DMATRIX>(object, off::PHYS_MATRIX));
    SetIdentity(flydemo::mem::field<CD3DMATRIX>(object, off::OBJECT_PENDING_TRANSFORM));

    const float mass = static_cast<float>(std::max<std::int16_t>(1, car.mass));
    constexpr float oneTwelfth = 1.0f / 12.0f;
    const float x2 = car.body.sizeX * car.body.sizeX;
    const float y2 = car.body.sizeY * car.body.sizeY;
    const float z2 = car.body.sizeZ * car.body.sizeZ;
    flydemo::mem::field<CD3DVECTOR>(object, off::PRINCIPAL_INERTIA) = {
        mass * oneTwelfth * (y2 + z2),
        mass * oneTwelfth * (x2 + z2),
        mass * oneTwelfth * (x2 + y2)};

    flydemo::mem::field<std::uint8_t>(object, off::COLLISION_ACTIVE) = 0;
    flydemo::mem::field<std::uint8_t>(object, off::GRAVITY_DAMPING_ACTIVE) = 0;
    flydemo::mem::field<std::uint8_t>(object, off::FORCE_BLOCKED_06A7) = 0;
    reinterpret_cast<CD3DDYNAMICOBJECT*>(object)->ResetForces();
    reinterpret_cast<CD3DDYNAMICOBJECT*>(object)->ResetImpulses();
    reinterpret_cast<CD3DDYNAMICOBJECT*>(object)->ResetVelocities();

    for (unsigned i = 0; i < car.nativeWheels.size(); ++i) {
        void* wheel = car.nativeWheels[i].data();
        flydemo::mem::field<float>(wheel, off::OBJECT_SIZE_X) = car.wheels[i].sizeX;
        flydemo::mem::field<float>(wheel, off::OBJECT_SIZE_Y) = car.wheels[i].sizeY;
        flydemo::mem::field<float>(wheel, off::OBJECT_SIZE_Z) = car.wheels[i].sizeZ;
        SetIdentity(flydemo::mem::field<CD3DMATRIX>(wheel, off::PHYS_MATRIX));
        SetIdentity(flydemo::mem::field<CD3DMATRIX>(wheel, off::OBJECT_PENDING_TRANSFORM));
        flydemo::mem::store_pointer32(
            object, off::WHEEL_PTR_BASE + i * off::WHEEL_SLOT_STRIDE, wheel);
        flydemo::mem::field<float>(object, off::WHEEL_PTR_BASE +
            i * off::WHEEL_SLOT_STRIDE + off::WHEEL_STEER_IN_SLOT) = 0.0f;
        flydemo::mem::field<float>(object, off::WHEEL_PTR_BASE +
            i * off::WHEEL_SLOT_STRIDE + off::WHEEL_SPEED_IN_SLOT) = 0.0f;
        flydemo::mem::field<float>(object, off::WHEEL_PTR_BASE +
            i * off::WHEEL_SLOT_STRIDE + off::WHEEL_OFFSET_IN_SLOT) =
            car.specs.wheelOffset;
    }
    SyncNativeWheelRuntime(car);
}

static bool UpdateLightVisuals(CAR_RENDER& car, float dt, bool reverseActive,
                                       const P3DLightingEnvironment& lighting,
                                       std::string* error) {
    if (!car.loaded || car.lightStates.empty())
        return true;

    Car_UpdateLightStates(
        car.lightStates.data(), car.specs.lightGroup0Count, car.specs.lightGroup1Count,
        car.lights, reverseActive, car.speed, dt);

    for (std::size_t i = 0; i < car.lightStates.size(); ++i)
        car.body.embeddedLights[i].packedColor = Car_ScaleLight(
            car.lightStates[i].baseColor, car.lightStates[i].intensity);

    std::string e;
    if (!car.body.SyncLighting(lighting, &e))
        return CarFail(error, std::move(e));
    return true;
}

static void IntegrateCarPhysics(CAR_RENDER& car, float dt) {
    if (car.nativeRuntimeReady && car.object != nullptr) {
        CD3DCAROBJECT& object = *car.object;

        if (car.forwardCommand > 0.0f)
            object.Forward(car.forwardCommand);
        if (car.reverseCommand > 0.0f)
            object.Reverse(car.reverseCommand);

        Car_ApplyTyrePhysics(&object);
        Dynamic_Integrate(&object, dt);
        object.UpdateWheelSpeeds(dt);
        object.UpdateGear();

        const CD3DVECTOR next =
            flydemo::mem::field<CD3DVECTOR>(&object, off::PHYS_POSITION);
        car.linearVelocity =
            flydemo::mem::field<CD3DVECTOR>(&object, off::LINEAR_VELOCITY);
        car.linearImpulse =
            flydemo::mem::field<CD3DVECTOR>(&object, off::LINEAR_IMPULSE);
        car.speed = flydemo::mem::field<float>(&object, off::SPEED);
        car.currentGear =
            flydemo::mem::field<std::int8_t>(&object, off::CURRENT_GEAR);

        if (CarLighting) {
            std::string e;
            if (!ApplyRigidPose(car, next, 0.0f, *CarLighting, &e) &&
                CarError.empty())
                CarError = std::move(e);
        } else {
            car.position = next;
        }
        return;
    }

    const int gears = std::max<int>(1, car.specs.numGears);
    const CD3DVECTOR forward =
        RotateLocalY(CD3DVECTOR{0.0f, 0.0f, 1.0f}, car.yaw);

    float accel = 0.0f;
    if (car.forwardCommand > 0.0f && car.speed < car.specs.maxSpeed) {
        float drive = car.specs.driveCoefficient;
        if (car.currentGear > 0) {
            const int gear = std::min<int>(gears, car.currentGear);
            drive *= static_cast<float>(gears - gear + 1) /
                     static_cast<float>(gears);
        }
        accel += drive * (car.forwardCommand * 0.01f);
    }

    const float reverseLimit = -car.specs.maxSpeed / static_cast<float>(gears);
    if (car.reverseCommand > 0.0f && car.speed > reverseLimit) {
        const float coeff = car.speed < 0.0f
            ? -car.specs.driveCoefficient
            : car.specs.reverseBrakeCoefficient;
        accel += coeff * (car.reverseCommand * 0.01f);
    }

    if (car.linearImpulse.x != 0.0f || car.linearImpulse.y != 0.0f ||
        car.linearImpulse.z != 0.0f) {
        const float mass = static_cast<float>(std::max<std::int16_t>(1, car.mass));
        car.linearVelocity.x += car.linearImpulse.x / mass;
        car.linearVelocity.y += car.linearImpulse.y / mass;
        car.linearVelocity.z += car.linearImpulse.z / mass;
        car.linearImpulse = {};
    }

    car.linearVelocity.x += forward.x * accel * dt;
    car.linearVelocity.y += forward.y * accel * dt;
    car.linearVelocity.z += forward.z * accel * dt;

    const CD3DVECTOR next{
        car.position.x + car.linearVelocity.x * dt,
        car.position.y + car.linearVelocity.y * dt,
        car.position.z + car.linearVelocity.z * dt};
    if (CarLighting) {
        std::string e;
        if (!ApplyRigidPose(car, next, 0.0f, *CarLighting, &e) && CarError.empty())
            CarError = std::move(e);
    } else {
        car.position = next;
    }

    const long double vx = static_cast<long double>(car.linearVelocity.x);
    const long double vy = static_cast<long double>(car.linearVelocity.y);
    const long double vz = static_cast<long double>(car.linearVelocity.z);
    const long double len = std::sqrt(vx * vx + vy * vy + vz * vz);
    if (len == 0.0L) {
        car.speed = 0.0f;
    } else {
        const long double horizontal = std::sqrt(vx * vx + vz * vz);
        const long double dot =
            (vx * static_cast<long double>(forward.x) +
             vy * static_cast<long double>(forward.y) +
             vz * static_cast<long double>(forward.z)) / len;
        car.speed = static_cast<float>(horizontal * dot);
    }

    if (car.speed == 0.0f) {
        car.currentGear = 0;
    } else if (car.speed < 0.0f) {
        car.currentGear = -1;
    } else {
        int gear = 1;
        const float stageWidth = car.specs.maxSpeed / static_cast<float>(gears);
        while (gear <= gears && stageWidth * static_cast<float>(gear) <= car.speed)
            ++gear;
        if (gear > gears) gear = gears;
        car.currentGear = static_cast<std::int8_t>(gear);
    }

    if (car.object != nullptr) {
        flydemo::mem::field<CD3DVECTOR>(car.object, off::LINEAR_VELOCITY) =
            car.linearVelocity;
        flydemo::mem::field<CD3DVECTOR>(car.object, off::LINEAR_IMPULSE) =
            car.linearImpulse;
        flydemo::mem::field<float>(car.object, off::SPEED) = car.speed;
        flydemo::mem::field<std::int8_t>(car.object, off::CURRENT_GEAR) =
            car.currentGear;
        flydemo::mem::field<CD3DVECTOR>(car.object, off::FORCE_ACCUM) = {};
        flydemo::mem::field<CD3DVECTOR>(car.object, off::TORQUE_ACCUM) = {};
        flydemo::mem::field<CD3DVECTOR>(car.object, off::ANGULAR_VELOCITY) = {};
        flydemo::mem::field<CD3DVECTOR>(car.object, off::ANGULAR_ACCEL) = {};
    }
}

static void IntegratePhysics(CAR_RENDER& car, float dt) {
    if (!car.physics || !(dt > 0.0f)) {
        car.forwardCommand = 0.0f;
        car.reverseCommand = 0.0f;
        return;
    }
    IntegrateCarPhysics(car, dt);
    car.forwardCommand = 0.0f;
    car.reverseCommand = 0.0f;
}

static bool UpdateCars(float pathTime, float dt, bool wrapped,
                             const P3DLightingEnvironment& lighting,
                             std::string* error) {
    CarLighting = &lighting;
    FrameDelta = dt;
    Host_SetFrameDelta(dt);
    CarError.clear();
    for (CD3DCAROBJECT* object : CarList()) {
        CAR_RENDER& car = CarRender(*object);
        SyncNativeWheelRuntime(car);
        car.forwardCommand = 0.0f;
        car.reverseCommand = 0.0f;
        flydemo::mem::field<std::uint8_t>(object, off::FORWARD_ACTIVE) = 0;
        flydemo::mem::field<std::uint8_t>(object, off::REVERSE_ACTIVE) = 0;
        flydemo::mem::field<std::uint8_t>(object, off::BRAKE_ACTIVE) = 0;
        flydemo::mem::field<std::uint8_t>(object, off::STEERING_ACTIVE) = 0;
    }
    if (wrapped)
        ResetCars();
    UpdateScripts(pathTime);
    for (CD3DCAROBJECT* object : CarList()) {
        CAR_RENDER& car = CarRender(*object);
        const bool reverseActive = car.reverseCommand > 0.0f;
        IntegratePhysics(car, dt);
        std::string visualError;
        if (!UpdateLightVisuals(car, dt, reverseActive, lighting, &visualError) &&
            CarError.empty())
            CarError = visualError;
        if (!UpdateWheelVisuals(car, dt, lighting, &visualError) && CarError.empty())
            CarError = visualError;
        if (!UpdateMinigunVisual(car, dt, lighting, &visualError) && CarError.empty())
            CarError = visualError;
    }
    UpdateMissiles(dt, lighting);
    UpdateEffects(dt);
    if (!CarError.empty())
        return CarFail(error, CarError);
    if (error) error->clear();
    return true;
}

static bool RenderCars(RendererState& renderer,
                           RenderDevice& device,
                           std::string* error) {
    for (CD3DCAROBJECT* object : CarList()) {
        CAR_RENDER& car = CarRender(*object);
        if (!car.loaded)
            continue;
        if (renderer.ClassifySphere(car.body.center, car.body.boundRadius,
                                    true, device) == SphereVisibility::Outside)
            continue;
        if (!car.body.Render(renderer, device, error))
            return false;
        for (auto& wheel : car.wheels)
            if (!wheel.Render(renderer, device, error))
                return false;
        if (car.spoiler && !car.spoiler->Render(renderer, device, error))
            return false;
        if (car.minigunBody && !car.minigunBody->Render(renderer, device, error))
            return false;
        if (car.minigunBarrel && !car.minigunBarrel->Render(renderer, device, error))
            return false;
    }
    if (error) error->clear();
    return true;
}

static bool RenderWeapons(RendererState& renderer,
                             RenderDevice& device,
                             std::string* error) {
    for (auto& missile : Missiles)
        if (missile && missile->active && !missile->body.Render(renderer, device, error))
            return false;
    if (!RenderEffects(renderer, error))
        return false;
    if (error) error->clear();
    return true;
}

static bool RenderAllCars(RendererState& renderer,
                      RenderDevice& device,
                      std::string* error) {
    return RenderCars(renderer, device, error) &&
           RenderWeapons(renderer, device, error);
}

static std::size_t LoadedCars() {
    std::size_t count = 0;
    for (CD3DCAROBJECT* object : CarList())
        if (object && CarRender(*object).loaded) ++count;
    return count;
}

static void SetPosition(CAR_RENDER& car, const CD3DVECTOR& position) {
    if (!CarLighting) { car.position = position; return; }
    std::string e;
    if (!ApplyRigidPose(car, position, 0.0f, *CarLighting, &e) && CarError.empty())
        CarError = std::move(e);
}
static void SetPositionXYZ(CAR_RENDER& car, const CD3DVECTOR& position) {
    SetPosition(car, position);
}
static void ResetDynamicMotion(CAR_RENDER& car) {
    car.linearVelocity = {};
}
static void AddLinearImpulse(CAR_RENDER& car, const CD3DVECTOR& impulse) {
    car.linearImpulse = AddVec(car.linearImpulse, impulse);
    if (car.nativeRuntimeReady && car.object != nullptr) {
        auto& raw = flydemo::mem::field<CD3DVECTOR>(car.object, off::LINEAR_IMPULSE);
        raw = AddVec(raw, impulse);
    }
}
static void NormalizeDynamicBasis(CAR_RENDER& car) {
    if (!CarLighting) { car.yaw = 0.0f; return; }
    std::string e;
    if (!ApplyRigidPose(car, car.position, -car.yaw, *CarLighting, &e) &&
        CarError.empty())
        CarError = std::move(e);
}
static void SetPositionAndYaw(CAR_RENDER& car, const CD3DVECTOR& position, float yaw) {
    if (!CarLighting) { car.position = position; car.yaw += yaw; return; }
    std::string e;
    if (!ApplyRigidPose(car, position, yaw, *CarLighting, &e) && CarError.empty())
        CarError = std::move(e);
}
static void SetPhysics(CAR_RENDER& car, bool enabled) {
    car.physics = enabled;
    if (!enabled) {
        car.linearVelocity = {};
        car.linearImpulse = {};
        car.speed = 0.0f;
        car.currentGear = 0;
    }
}
static void SetLights(CAR_RENDER& car, bool enabled) { car.lights = enabled; }
static void SetMissileCount(CAR_RENDER& car, std::uint8_t count) { car.missileCount = count; }
static void SetMinigunResource(CAR_RENDER& car, std::uint32_t value) {
    car.minigunResource = value;
    if (car.minigun == 1u)
        Minigun_SetAmmo(car.minigunRuntime.data(), static_cast<int>(value));
}
static void Forward(CAR_RENDER& car, float rate) { car.forwardCommand = ClampRate(rate); }
static void Reverse(CAR_RENDER& car, float rate) { car.reverseCommand = ClampRate(rate); }
std::string_view DemoCarMissileIO::CarName(void* car) const {
    return CarRender(*static_cast<CD3DCAROBJECT*>(car)).objectName;
}

bool DemoCarMissileIO::DynamicNameExists(std::string_view fullName) const {
    return std::any_of(Missiles.begin(), Missiles.end(),
        [&](const std::unique_ptr<MISSILE_RUNTIME>& missile) {
            return missile && missile->active && missile->name == fullName;
        });
}

void* DemoCarMissileIO::CreateMissile(
    std::size_t nativeBytes, std::string_view fullName, void* ownerCar,
    const CD3DVECTOR& position, const CD3DVECTOR& direction) {
    if (nativeBytes != 0x0fe0u || ownerCar == nullptr || !MissileModel ||
        !CarTextures || !CarLoader || !CarLighting)
        return nullptr;

    CAR_RENDER& owner = CarRender(*static_cast<CD3DCAROBJECT*>(ownerCar));
    auto missile = std::make_unique<MISSILE_RUNTIME>();
    missile->name.assign(fullName.data(), fullName.size());
    std::string e;
    if (!missile->body.InitializeFromModel(*MissileModel, *CarTextures, *CarLoader, &e)) {
        if (CarError.empty()) CarError = e;
        return nullptr;
    }

    missile->body.SetCenterPosition(position);
    const float yaw = static_cast<float>(
        std::atan2(static_cast<long double>(direction.x),
                   static_cast<long double>(direction.z)) /
        static_cast<long double>(kRadiansPerAngleUnit));
    if (yaw != 0.0f)
        missile->body.RotateY(position, yaw);
    if (!missile->body.ApplyPendingTransform(*CarLighting, &e)) {
        if (CarError.empty()) CarError = e;
        return nullptr;
    }

    const CD3DVECTOR size{MissileModel->sizeX, MissileModel->sizeY, MissileModel->sizeZ};
    const CD3DMATRIX orientation =
        flydemo::mem::field<CD3DMATRIX>(ownerCar, off::PHYS_MATRIX);
    const CD3DVECTOR velocity =
        flydemo::mem::field<CD3DVECTOR>(ownerCar, off::LINEAR_VELOCITY);
    missile->object.Initialize(ownerCar, position, direction,
                               NativeRandom(), NativeRandom(), MissileMass, size,
                               orientation, velocity, missile->localBounds.data());
    missile->active = true;

    AddLinearImpulse(owner, missile->object.Recoil());

    if (ParticleMode != ParticleUse::Never) {
        const ParticleConfig smoke = Missile_TrailConfig(missile->object.Direction());
        (void)InitParticle(missile->trail, smoke, position, "missile smoke");
        if (missile->trail)
            missile->object.SetTrail(&*missile->trail);
    }

    MISSILE_RUNTIME* created = missile.get();
    Missiles.push_back(std::move(missile));
    return &created->object;
}

static void FireMissile(CAR_RENDER& car) {
    if (car.object == nullptr)
        return;
    (void)car.object->FireMissile();
    car.missileCount = flydemo::mem::field<std::uint8_t>(car.object, off::MISSILE_COUNT);
}

static void FireMinigun(CAR_RENDER& car) {
    if (car.minigun != 1u || !car.minigunBody || !car.minigunBarrel)
        return;

    (void)Minigun_Fire(car.minigunRuntime.data());
}

static void SpawnCycoreExplosion(const CD3DVECTOR& position,
                                 float strength, float radius, int) {
    SpawnExplosionVisual(position, ExplosionType::Big);
    ApplyExplosionDynamics(position, strength, radius);
}
static CD3DVECTOR GetPosition(const CAR_RENDER& car) { return car.position; }

static void SetPosition(CD3DCAROBJECT& object, const CD3DVECTOR& position) {
    CAR_RENDER& car = CarRender(object);
    SetPosition(car, position);
    flydemo::mem::field<CD3DVECTOR>(&object, off::OBJECT_BOUNDS_CENTER) = car.position;
    flydemo::mem::field<CD3DVECTOR>(&object, off::PHYS_POSITION) = car.position;
}
static void SetPositionXYZ(CD3DCAROBJECT& object, const CD3DVECTOR& position) {
    SetPosition(object, position);
}
static void ResetDynamicMotion(CD3DCAROBJECT& object) {
    CAR_RENDER& car = CarRender(object);
    ResetDynamicMotion(car);
    flydemo::mem::field<CD3DVECTOR>(&object, off::LINEAR_VELOCITY) = {};
    flydemo::mem::field<CD3DVECTOR>(&object, off::ANGULAR_VELOCITY) = {};
}
static void AddLinearImpulse(CD3DCAROBJECT& object, const CD3DVECTOR& impulse) {
    CAR_RENDER& car = CarRender(object);
    AddLinearImpulse(car, impulse);
    if (!car.nativeRuntimeReady) {
        auto& raw = flydemo::mem::field<CD3DVECTOR>(&object, off::LINEAR_IMPULSE);
        raw = AddVec(raw, impulse);
    }
}
static void NormalizeDynamicBasis(CD3DCAROBJECT& object) {
    NormalizeDynamicBasis(CarRender(object));
    reinterpret_cast<CD3DDYNAMICOBJECT*>(&object)->ResetOrientation();
}
static void SetPositionAndYaw(CD3DCAROBJECT& object, const CD3DVECTOR& position, float yaw) {
    CAR_RENDER& car = CarRender(object);
    SetPositionAndYaw(car, position, yaw);
    flydemo::mem::field<CD3DVECTOR>(&object, off::OBJECT_BOUNDS_CENTER) = car.position;
    flydemo::mem::field<CD3DVECTOR>(&object, off::PHYS_POSITION) = car.position;
    flydemo::mem::field<CD3DVECTOR>(&object, off::OBJECT_FORWARD) =
        RotateLocalY(CD3DVECTOR{0.0f, 0.0f, 1.0f}, car.yaw);
}
static void SetPhysics(CD3DCAROBJECT& object, bool enabled) {
    SetPhysics(CarRender(object), enabled);
    reinterpret_cast<CD3DDYNAMICOBJECT*>(&object)->SetActivePhysics(enabled);
}
static void SetLights(CD3DCAROBJECT& object, bool enabled) {
    SetLights(CarRender(object), enabled);
    object.SetLights(enabled ? 1u : 0u);
}
static void SetMissileCount(CD3DCAROBJECT& object, std::uint8_t count) {
    SetMissileCount(CarRender(object), count);
    object.SetMissileCount(count);
}
static void SetMinigunResource(CD3DCAROBJECT& object, std::uint32_t value) {
    SetMinigunResource(CarRender(object), value);
}
static void Forward(CD3DCAROBJECT& object, float rate) {
    Forward(CarRender(object), rate);
}
static void Reverse(CD3DCAROBJECT& object, float rate) {
    Reverse(CarRender(object), rate);
}
static void FireMissile(CD3DCAROBJECT& object) {
    CAR_RENDER& car = CarRender(object);
    FireMissile(car);
    flydemo::mem::field<std::uint8_t>(&object, off::MISSILE_COUNT) = car.missileCount;
}
static void FireMinigun(CD3DCAROBJECT& object) {
    FireMinigun(CarRender(object));
}
static CD3DVECTOR GetPosition(const CD3DCAROBJECT& object) {
    return flydemo::mem::field<CD3DVECTOR>(&object, off::OBJECT_BOUNDS_CENTER);
}

static bool UpdateWheelVisuals(CAR_RENDER& car, float dt,
                                        const P3DLightingEnvironment& lighting,
                                        std::string* error) {
    if (!car.loaded) return true;
    std::string e;
    for (unsigned i = 0; i < car.wheels.size(); ++i) {

        const CD3DVECTOR mount = TransformByBasis(WheelMount(car.specs, i), car.body);
        const CD3DVECTOR target = AddVec(
            AddVec(car.body.center, mount),
            ScaleVec(car.body.up, -car.specs.wheelOffset));
        CD3DPOLYGONOBJECT& wheel = car.wheels[i];
        wheel.SetCenterPosition(target);
        if (!wheel.ApplyPendingTransform(lighting, &e)) return CarFail(error, e);

        const float diameter = wheel.sizeY;
        if (diameter != 0.0f && dt > 0.0f) {
            const float rollRate = ComputeWheelRollRate(car.speed, diameter);
            const float delta = rollRate * dt;
            car.wheelRollAngle[i] += delta;
            if (car.wheelRollAngle[i] > 25600.0f) car.wheelRollAngle[i] -= 25600.0f;

            wheel.RotateY(target, -car.yaw);
            wheel.RotateX(target, delta);
            wheel.RotateY(target, car.yaw);
            if (!wheel.ApplyPendingTransform(lighting, &e)) return CarFail(error, e);
        }
    }
    if (error) error->clear();
    return true;
}

static bool UpdateMinigunVisual(CAR_RENDER& car, float dt,
                                         const P3DLightingEnvironment& lighting,
                                         std::string* error) {
    if (!car.minigunBody || !car.minigunBarrel) return true;
    const bool firing = car.minigunRuntime[minigun_off::FireLatch] != 0u;

    if (firing)
        (void)Minigun_BuildLight(true, NativeRandom());
    std::int32_t randomOffset = 0;
    std::int32_t randomColor = 0;
    if (ParticleMode != ParticleUse::Never) {
        randomOffset = NativeRandom();
        randomColor = NativeRandom();
    }

    float before = 0.0f;
    std::memcpy(&before, car.minigunRuntime.data() + minigun_off::BarrelAngle, sizeof(before));
    Minigun_Update(car.minigunRuntime.data(), dt);
    float after = 0.0f;
    std::memcpy(&after, car.minigunRuntime.data() + minigun_off::BarrelAngle, sizeof(after));
    const float delta = after - before;
    std::string e;

    const CD3DVECTOR target = AddVec(car.minigunBody->center,
                               TransformByBasis(CD3DVECTOR{0.0f, 0.0f, 0.3f},
                                                *car.minigunBody));
    car.minigunBarrel->SetCenterPosition(target);
    if (!car.minigunBarrel->ApplyPendingTransform(lighting, &e))
        return CarFail(error, e);

    if (delta != 0.0f) {

        car.minigunBarrel->RotateY(target, -car.yaw);
        car.minigunBarrel->RotateZ(target, delta);
        car.minigunBarrel->RotateY(target, car.yaw);
        if (!car.minigunBarrel->ApplyPendingTransform(lighting, &e))
            return CarFail(error, e);
    }

    UpdateMinigunEffects(car, firing, randomOffset, randomColor);
    if (error) error->clear();
    return true;
}

static bool InitParticle(
    std::optional<CD3DTEXPARTICLEOBJECT>& slot,
    const ParticleConfig& config,
    const CD3DVECTOR& position,
    std::string_view diagnosticName) {
    if (!CarTextures || !CarLoader)
        return false;
    TexPartSystem backend(*CarTextures, *CarLoader);
    slot.emplace();
    slot->SetEmitterPosition(position);
    std::string e;
    if (slot->Initialize(config, backend, &e))
        return true;
    slot.reset();
    if (CarError.empty())
        CarError = std::string(diagnosticName) + ": " + e;
    return false;
}

static void UpdateMinigunEffects(CAR_RENDER& car,
                                           bool firing,
                                           std::int32_t randomOffset,
                                           std::int32_t randomColor) {
    if (!car.minigunBody || !CarTextures || !CarLoader)
        return;

    const auto side = static_cast<MinigunSide>(
        car.minigunRuntime[minigun_off::Side]);
    const CD3DVECTOR gunOrigin = car.minigunBody->center;
    const CD3DVECTOR gunForward = car.minigunBody->forward;

    const auto shot = Minigun_BuildFlash(
        ParticleMode, firing, car.minigunShotfire.has_value(),
        randomOffset, randomColor);
    if (shot.process) {
        const CD3DVECTOR shotPosition = AddVec(
            gunOrigin, TransformByBasis(shot.localOffset, *car.minigunBody));
        if (shot.createEmitter) {
            const ParticleConfig cfg = Minigun_ShotConfig(
                gunForward, shot.color);
            (void)InitParticle(car.minigunShotfire, cfg,
                                               shotPosition, "minigun shotfire");
        }
        if (car.minigunShotfire && shot.setRespawn) {
            car.minigunShotfire->SetRespawn(shot.respawn);
            if (shot.updatePosition)
                car.minigunShotfire->SetEmitterPosition(shotPosition);
            if (shot.updateDirection)
                car.minigunShotfire->SetDirection(gunForward);
            if (shot.updateColor)
                car.minigunShotfire->SetStartColor(shot.color);
        }
    }

    const auto bullets = Minigun_BuildBulletPlan(
        ParticleMode, side, firing, car.minigunBullets.has_value());
    if (bullets.process) {
        const CD3DVECTOR bulletPosition = AddVec(
            gunOrigin, TransformByBasis(bullets.localPosition, *car.minigunBody));
        const CD3DVECTOR bulletDirection = TransformByBasis(
            bullets.localDirection, *car.minigunBody);
        if (bullets.createEmitter) {
            const ParticleConfig cfg = Minigun_BulletConfig(
                bulletDirection, bullets.offsetVector,
                CarLighting ? CarLighting->ambientColor : 0u);
            (void)InitParticle(car.minigunBullets, cfg,
                                               bulletPosition, "minigun bullets");
        }
        if (car.minigunBullets && bullets.setRespawn) {
            car.minigunBullets->SetRespawn(bullets.respawn);
            if (bullets.updatePosition)
                car.minigunBullets->SetEmitterPosition(bulletPosition);
            if (bullets.updateDirection)
                car.minigunBullets->SetDirection(bulletDirection);
        }
    }
}

static void SpawnParticle(const ParticleConfig& config, const CD3DVECTOR& position) {
    if (!CarTextures || !CarLoader || ParticleMode == ParticleUse::Never) return;
    TexPartSystem backend(*CarTextures, *CarLoader);
    PARTICLEFX fx;
    fx.runtime.SetEmitterPosition(position);
    std::string e;
    if (fx.runtime.Initialize(config, backend, &e))
        Effects.push_back(std::move(fx));
    else if (CarError.empty())
        CarError = "particle " + config.textureName + ": " + e;
}

static void SpawnExplosionVisual(const CD3DVECTOR& position, ExplosionType type) {
    const auto recipe = BuildExplosionFx(type, ParticleMode);
    for (const auto& spec : recipe) {
        ParticleConfig cfg;
        cfg.type = ParticleType::Texture;
        cfg.startCount = static_cast<std::int16_t>(spec.count);
        cfg.maxCount = static_cast<std::int16_t>(spec.count);
        cfg.respawn = false;
        cfg.lifetime = spec.paramA;
        cfg.minSpeed = spec.paramB;
        cfg.maxSpeed = spec.paramC;
        cfg.acceleration = {0.0f, 0.0f, 0.0f};
        cfg.direction = {0.0f, 1.0f, 0.0f};
        cfg.variance = 1.0f;
        cfg.textureName = spec.texture;
        cfg.renderMode = ParticleRenderMode::Add;
        cfg.startColor = spec.packedColor;
        cfg.endColor = spec.packedColor;
        cfg.startSize = spec.sizeA;
        cfg.endSize = spec.sizeB;
        SpawnParticle(cfg, position);
    }
}

namespace {

class DemoCarExplosionBody final : public ExplosionDynamicBody {
public:
    explicit DemoCarExplosionBody(CD3DCAROBJECT* object) : object_(object) {}

    bool IsDynamicObjectClass() const override { return object_ != nullptr; }

    const CD3DVECTOR& BoundsCenter() const override {
        return CarRender(*object_).body.center;
    }

    const std::array<CD3DVECTOR, 8>& BoundsPoints() const override {
        return CarRender(*object_).body.boundsPoints;
    }

    void ApplyImpulseAtPoint(const CD3DVECTOR&, const CD3DVECTOR& impulse) override {
        if (object_ == nullptr || !CarRender(*object_).physics)
            return;
        AddLinearImpulse(*object_, impulse);
    }

private:
    CD3DCAROBJECT* object_ = nullptr;
};

class DemoExplosionWorld final : public ExplosionWorld {
public:
    DemoExplosionWorld()
        : bodies_{DemoCarExplosionBody(Wolf), DemoCarExplosionBody(Cycore),
                  DemoCarExplosionBody(Apachee), DemoCarExplosionBody(Wrecker),
                  DemoCarExplosionBody(Wrecker2)} {}

    ExplosionDynamicBody* DynamicAt(std::size_t slot) override {
        return slot < bodies_.size() ? &bodies_[slot] : nullptr;
    }

private:
    std::array<DemoCarExplosionBody, 5> bodies_;
};

}

static void ApplyExplosionDynamics(const CD3DVECTOR& position,
                                   float strength, float radius) {
    ExplosionState state;
    state.position = position;
    state.strength = strength;
    state.radius = radius;
    DemoExplosionWorld world;
    (void)ApplyExplosionForce(state, world);
}

static void UpdateEffects(float dt) {
    if (!CarTextures || !CarLoader) return;
    TexPartSystem backend(*CarTextures, *CarLoader);

    for (CD3DCAROBJECT* object : CarList()) {
        CAR_RENDER& car = CarRender(*object);
        if (car.minigunShotfire)
            car.minigunShotfire->Update(dt, backend);
        if (car.minigunBullets)
            car.minigunBullets->Update(dt, backend);
    }
    for (auto& missile : Missiles) {
        if (missile && missile->trail)
            missile->trail->Update(dt, backend);
    }
    for (auto& fx : Effects) fx.runtime.Update(dt, backend);
    Effects.erase(std::remove_if(Effects.begin(), Effects.end(),
        [](const PARTICLEFX& fx) { return fx.runtime.Hidden(); }), Effects.end());
}

static bool RenderEffects(RendererState& renderer, std::string* error) {
    if (!CarTextures || !CarLoader) return true;
    TexPartSystem backend(*CarTextures, *CarLoader);

    for (CD3DCAROBJECT* object : CarList()) {
        CAR_RENDER& car = CarRender(*object);
        if (car.minigunShotfire && !car.minigunShotfire->Render(renderer, backend))
            return CarFail(error, "minigun shotfire render");
        if (car.minigunBullets && !car.minigunBullets->Render(renderer, backend))
            return CarFail(error, "minigun bullets render");
    }
    for (auto& missile : Missiles) {
        if (missile && missile->trail && !missile->trail->Render(renderer, backend))
            return CarFail(error, "missile smoke render");
    }
    for (auto& fx : Effects) {
        if (!fx.runtime.Render(renderer, backend))
            return CarFail(error, "particle render");
    }
    return true;
}

static bool LoadMissileModel(const std::filesystem::path& gameRoot,
                                      std::string* error) {
    const std::filesystem::path cdo = gameRoot / "TRKDATA" / "DYNAMICS" / "missile.cdo";
    CDFileOperations file;
    if (file.Open(cdo.string().c_str(), "rb", 0) != 0)
        return CarFail(error, cdo.string());
    std::string firstLine, p3d;
    const bool ok = CrashdayDirectory::ReadConfigLine(file, firstLine) &&
                    CrashdayDirectory::ReadConfigLine(file, p3d);
    std::string line;
    bool materialSeen = false;
    std::int16_t mass = 1;
    if (ok) {
        while (CrashdayDirectory::ReadConfigLine(file, line)) {
            if (materialSeen) {
                const long value = std::strtol(line.c_str(), nullptr, 10);
                if (value > 0 && value <= 32767)
                    mass = static_cast<std::int16_t>(value);
                break;
            }
            materialSeen = line == "METAL" || line == "STONE" || line == "WOOD" ||
                           line == "PLASTIC" || line == "RUBBER";
        }
    }
    (void)file.Close();
    if (!ok || p3d.empty())
        return CarFail(error, cdo.string() + ": invalid dynamic-object header");
    P3DModel model;
    if (!ReadP3D(gameRoot / "EDITOR" / p3d, model, error))
        return false;
    MissileMass = mass;
    MissileModel = std::move(model);
    return true;
}

static void UpdateMissiles(float dt, const P3DLightingEnvironment& lighting) {
    if (!(dt > 0.0f)) return;
    for (auto& entry : Missiles) {
        if (!entry || !entry->active) continue;
        MISSILE_RUNTIME& missile = *entry;
        const CD3DVECTOR old =
            flydemo::mem::field<CD3DVECTOR>(&missile.object, off::OBJECT_BOUNDS_CENTER);
        missile.body.RotateZ(old, dt * 256.0f);

        const MissileStepResult move = missile.object.UpdateMissile(dt, CDWorld);
        const CD3DVECTOR position = move.spawnExplosion
            ? move.explosionPosition : move.collisionEnd;
        missile.body.Translate(position.x - old.x,
                               position.y - old.y,
                               position.z - old.z);
        std::string e;
        if (!missile.body.ApplyPendingTransform(lighting, &e) && CarError.empty())
            CarError = e;

        if (missile.trail)
            missile.trail->SetEmitterPosition(position);
        if (move.hidden) {
            missile.active = false;
            if (missile.trail) {
                missile.trail->SetRespawn(false);
                PARTICLEFX tail;
                tail.runtime = std::move(*missile.trail);
                Effects.push_back(std::move(tail));
                missile.trail.reset();
                missile.object.SetTrail(nullptr);
            }
            if (move.spawnExplosion)
                SpawnExplosionVisual(move.explosionPosition, ExplosionType::Normal);
        }
    }
    Missiles.erase(std::remove_if(Missiles.begin(), Missiles.end(),
        [](const std::unique_ptr<MISSILE_RUNTIME>& m) { return !m || !m->active; }),
        Missiles.end());
}

static void ShutdownCars() {

    Car_SetMissileIO(nullptr);
    Effects.clear();
    Missiles.clear();
    MissileModel.reset();
    MissileMass = 1;
    delete Wolf; Wolf = nullptr;
    delete Cycore; Cycore = nullptr;
    delete Apachee; Apachee = nullptr;
    delete Wrecker; Wrecker = nullptr;
    delete Wrecker2; Wrecker2 = nullptr;
    CarLighting = nullptr;
    CarTextures = nullptr;
    CarLoader = nullptr;
    FrameDelta = 0.0f;
    CarError.clear();
}

}

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef DIRECTINPUT_VERSION
#define DIRECTINPUT_VERSION 0x0500
#endif

#include <windows.h>
#include <ddraw.h>
#include <d3d.h>
#include <dinput.h>
#include <mmsystem.h>

#include "cbm.hpp"
#include "cdfileop.hpp"
#include "control.hpp"
#include "directx.hpp"
#include "engine.hpp"
#include "options.hpp"
#include "path.hpp"
#include "render.hpp"
#include "screen.hpp"
#include "win32.hpp"

#include <algorithm>
#include <cstddef>
#include <array>
#include <cstdint>
#include <cstdio>
#include <ctime>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <limits>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

namespace flydemo {
namespace {

template <class T>
void releaseCom(T*& value) {
    if (value) {
        value->Release();
        value = nullptr;
    }
}

DDPixelFormat toPixelFormat(const DDPIXELFORMAT& in) {
    DDPixelFormat out{};
    out.size = in.dwSize;
    out.flags = in.dwFlags;
    out.fourCC = in.dwFourCC;
    out.rgbBitCount = in.dwRGBBitCount;
    out.redMask = in.dwRBitMask;
    out.greenMask = in.dwGBitMask;
    out.blueMask = in.dwBBitMask;
    out.alphaMask = in.dwRGBAlphaBitMask;
    return out;
}

DDPIXELFORMAT toNative(const DDPixelFormat& in) {
    DDPIXELFORMAT out{};
    out.dwSize = in.size ? in.size : sizeof(DDPIXELFORMAT);
    out.dwFlags = in.flags;
    out.dwFourCC = in.fourCC;
    out.dwRGBBitCount = in.rgbBitCount;
    out.dwRBitMask = in.redMask;
    out.dwGBitMask = in.greenMask;
    out.dwBBitMask = in.blueMask;
    out.dwRGBAlphaBitMask = in.alphaMask;
    return out;
}

DDPIXELFORMAT toNative(const CBMPixelFormat& in) {
    DDPIXELFORMAT out{};
    out.dwSize = in.size ? in.size : sizeof(DDPIXELFORMAT);
    out.dwFlags = in.flags;
    out.dwFourCC = in.fourCC;
    out.dwRGBBitCount = in.bitsPerPixel;
    out.dwRBitMask = in.redMask;
    out.dwGBitMask = in.greenMask;
    out.dwBBitMask = in.blueMask;
    out.dwRGBAlphaBitMask = in.alphaMask;
    return out;
}

struct EnumeratedNativeDriver {
    bool primary = false;
    GUID guid{};
    DDrawDriver info{};
};

struct DriverEnumContext {
    std::vector<EnumeratedNativeDriver>* drivers = nullptr;
};

HRESULT CALLBACK enumModeCallback(LPDDSURFACEDESC2 desc, LPVOID context) {
    auto* driver = static_cast<EnumeratedNativeDriver*>(context);
    if (!driver || !desc)
        return DDENUMRET_OK;
    VideoMode mode{};
    mode.width = desc->dwWidth;
    mode.height = desc->dwHeight;
    mode.bitsPerPixel = desc->ddpfPixelFormat.dwRGBBitCount;

    if (ConsiderDisplayMode(mode, desc->ddpfPixelFormat.dwGBitMask,
                            driver->info.modes, driver->info.defaultMode))
        return DDENUMRET_OK;
    return DDENUMRET_OK;
}

HRESULT CALLBACK enumZCallback(LPDDPIXELFORMAT format, LPVOID context) {
    auto* driver = static_cast<EnumeratedNativeDriver*>(context);
    if (!driver || !format)
        return D3DENUMRET_OK;
    const auto action = ConsiderZBufferFormat(toPixelFormat(*format),
                                              driver->info.zBufferFormat);
    driver->info.zBufferBitDepth = driver->info.zBufferFormat.rgbBitCount;
    return action == TextureFormatEnumAction::Stop ? D3DENUMRET_CANCEL : D3DENUMRET_OK;
}

BOOL WINAPI enumDriverCallback(GUID* guid, LPSTR description, LPSTR name,
                               LPVOID context, HMONITOR) {
    auto* ctx = static_cast<DriverEnumContext*>(context);
    if (!ctx || !ctx->drivers || ctx->drivers->size() >= DirectXState::MaxDDrawDrivers)
        return DDENUMRET_CANCEL;

    EnumeratedNativeDriver candidate{};
    candidate.primary = (guid == nullptr);
    if (guid)
        candidate.guid = *guid;
    candidate.info.name = name && *name ? name : "display";
    candidate.info.description = description && *description
        ? description : candidate.info.name;
    candidate.info.defaultMode = 0xFFFFu;

    LPDIRECTDRAW oldDD = nullptr;
    LPDIRECTDRAW4 dd4 = nullptr;
    LPDIRECT3D3 d3d3 = nullptr;
    const GUID* nativeGuid = candidate.primary ? nullptr : &candidate.guid;
    if (FAILED(DirectDrawCreate(const_cast<GUID*>(nativeGuid), &oldDD, nullptr)) || !oldDD)
        return DDENUMRET_OK;
    if (FAILED(oldDD->QueryInterface(IID_IDirectDraw4, reinterpret_cast<void**>(&dd4))) || !dd4) {
        releaseCom(oldDD);
        return DDENUMRET_OK;
    }
    releaseCom(oldDD);

    DDCAPS halCaps{};
    DDCAPS helCaps{};
    halCaps.dwSize = sizeof(halCaps);
    helCaps.dwSize = sizeof(helCaps);
    if (FAILED(dd4->GetCaps(&halCaps, &helCaps)) ||
        (halCaps.dwCaps & DDCAPS_3D) == 0) {
        releaseCom(dd4);
        return DDENUMRET_OK;
    }

    if (FAILED(dd4->EnumDisplayModes(0, nullptr, &candidate, enumModeCallback)) ||
        candidate.info.modes.empty() ||
        candidate.info.defaultMode == 0xFFFFu) {
        releaseCom(dd4);
        return DDENUMRET_OK;
    }

    if (FAILED(dd4->QueryInterface(IID_IDirect3D3, reinterpret_cast<void**>(&d3d3))) || !d3d3) {
        releaseCom(dd4);
        return DDENUMRET_OK;
    }
    if (FAILED(d3d3->EnumZBufferFormats(IID_IDirect3DHALDevice,
                                        enumZCallback, &candidate)) ||
        candidate.info.zBufferBitDepth == 0) {
        releaseCom(d3d3);
        releaseCom(dd4);
        return DDENUMRET_OK;
    }

    releaseCom(d3d3);
    releaseCom(dd4);
    ctx->drivers->push_back(candidate);
    return ctx->drivers->size() >= DirectXState::MaxDDrawDrivers
        ? DDENUMRET_CANCEL : DDENUMRET_OK;
}

HRESULT CALLBACK enumTextureCallback(LPDDPIXELFORMAT format, LPVOID context) {
    auto* out = static_cast<std::vector<DDPixelFormat>*>(context);
    if (out && format)
        out->push_back(toPixelFormat(*format));
    return D3DENUMRET_OK;
}

GUID iidFromBytes(const std::array<std::uint8_t, 16>& bytes) {
    GUID guid{};
    static_assert(sizeof(GUID) == 16, "GUID size");
    std::memcpy(&guid, bytes.data(), 16);
    return guid;
}

LPDIRECTINPUTDEVICEA* deviceSlot(ControllerDevice device,
                                 LPDIRECTINPUTDEVICEA& keyboard,
                                 LPDIRECTINPUTDEVICEA& mouse,
                                 LPDIRECTINPUTDEVICEA& joystickBase) {
    switch (device) {
        case ControllerDevice::Keyboard: return &keyboard;
        case ControllerDevice::Mouse: return &mouse;
        case ControllerDevice::Joystick: return &joystickBase;
    }
    return nullptr;
}

struct DirectInputFormats {
    std::array<DIOBJECTDATAFORMAT, 256> keyboardObjects{};
    std::array<DIOBJECTDATAFORMAT, 7> mouseObjects{};
    std::array<DIOBJECTDATAFORMAT, 44> joystickObjects{};
    DIDATAFORMAT keyboard{};
    DIDATAFORMAT mouse{};
    DIDATAFORMAT joystick{};

    DirectInputFormats() {
        for (DWORD i = 0; i < keyboardObjects.size(); ++i) {
            keyboardObjects[i] = DIOBJECTDATAFORMAT{
                &GUID_Key,
                i,
                static_cast<DWORD>(DIDFT_BUTTON | DIDFT_MAKEINSTANCE(i) | 0x80000000UL),
                0};
        }
        keyboard = DIDATAFORMAT{
            sizeof(DIDATAFORMAT),
            sizeof(DIOBJECTDATAFORMAT),
            DIDF_RELAXIS,
            256u,
            static_cast<DWORD>(keyboardObjects.size()),
            keyboardObjects.data()};

        mouseObjects[0] = DIOBJECTDATAFORMAT{
            &GUID_XAxis, DIMOFS_X,
            static_cast<DWORD>(DIDFT_AXIS | DIDFT_ANYINSTANCE), 0};
        mouseObjects[1] = DIOBJECTDATAFORMAT{
            &GUID_YAxis, DIMOFS_Y,
            static_cast<DWORD>(DIDFT_AXIS | DIDFT_ANYINSTANCE), 0};
        mouseObjects[2] = DIOBJECTDATAFORMAT{
            &GUID_ZAxis, DIMOFS_Z,
            static_cast<DWORD>(DIDFT_AXIS | DIDFT_ANYINSTANCE | 0x80000000UL), 0};
        mouseObjects[3] = DIOBJECTDATAFORMAT{
            nullptr, DIMOFS_BUTTON0,
            static_cast<DWORD>(DIDFT_BUTTON | DIDFT_ANYINSTANCE), 0};
        mouseObjects[4] = DIOBJECTDATAFORMAT{
            nullptr, DIMOFS_BUTTON1,
            static_cast<DWORD>(DIDFT_BUTTON | DIDFT_ANYINSTANCE), 0};
        mouseObjects[5] = DIOBJECTDATAFORMAT{
            nullptr, DIMOFS_BUTTON2,
            static_cast<DWORD>(DIDFT_BUTTON | DIDFT_ANYINSTANCE | 0x80000000UL), 0};
        mouseObjects[6] = DIOBJECTDATAFORMAT{
            nullptr, DIMOFS_BUTTON3,
            static_cast<DWORD>(DIDFT_BUTTON | DIDFT_ANYINSTANCE | 0x80000000UL), 0};
        mouse = DIDATAFORMAT{
            sizeof(DIDATAFORMAT),
            sizeof(DIOBJECTDATAFORMAT),
            DIDF_RELAXIS,
            sizeof(DIMOUSESTATE),
            static_cast<DWORD>(mouseObjects.size()),
            mouseObjects.data()};

        const GUID* const axisGuids[8] = {
            &GUID_XAxis, &GUID_YAxis, &GUID_ZAxis,
            &GUID_RxAxis, &GUID_RyAxis, &GUID_RzAxis,
            &GUID_Slider, &GUID_Slider};
        const DWORD axisOffsets[8] = {
            DIJOFS_X, DIJOFS_Y, DIJOFS_Z,
            DIJOFS_RX, DIJOFS_RY, DIJOFS_RZ,
            DIJOFS_SLIDER(0), DIJOFS_SLIDER(1)};
        for (DWORD i = 0; i < 8; ++i) {
            joystickObjects[i] = DIOBJECTDATAFORMAT{
                axisGuids[i],
                axisOffsets[i],
                static_cast<DWORD>(DIDFT_AXIS | DIDFT_ANYINSTANCE | 0x80000000UL),
                0x00000100u};
        }
        for (DWORD i = 0; i < 4; ++i) {
            joystickObjects[8 + i] = DIOBJECTDATAFORMAT{
                &GUID_POV,
                DIJOFS_POV(i),
                static_cast<DWORD>(DIDFT_POV | DIDFT_ANYINSTANCE | 0x80000000UL),
                0};
        }
        for (DWORD i = 0; i < 32; ++i) {
            joystickObjects[12 + i] = DIOBJECTDATAFORMAT{
                nullptr,
                DIJOFS_BUTTON(i),
                static_cast<DWORD>(DIDFT_BUTTON | DIDFT_ANYINSTANCE | 0x80000000UL),
                0};
        }
        joystick = DIDATAFORMAT{
            sizeof(DIDATAFORMAT),
            sizeof(DIOBJECTDATAFORMAT),
            DIDF_ABSAXIS,
            sizeof(DIJOYSTATE),
            static_cast<DWORD>(joystickObjects.size()),
            joystickObjects.data()};
    }
};

const DIDATAFORMAT* dataFormatFor(ControllerDevice device) {

    static DirectInputFormats formats;
    switch (device) {
        case ControllerDevice::Keyboard: return &formats.keyboard;
        case ControllerDevice::Mouse: return &formats.mouse;
        case ControllerDevice::Joystick: return &formats.joystick;
    }
    return nullptr;
}

}

struct Win32Graphics::Impl {
    explicit Impl(HWND value) : hwnd(value) {}

    HWND hwnd = nullptr;
    std::vector<EnumeratedNativeDriver> drivers;
    std::uint16_t selectedDriver = 0;
    VideoMode currentMode{};
    CBMManager* textures = nullptr;
    std::string lastError;

    std::uint32_t frameDrawCalls = 0;
    std::uint32_t frameGoodDraws = 0;
    std::uint32_t frameFailedCalls = 0;
    std::uint32_t lastFailure = 0;
    std::string lastFailureOperation;

    void RecordFailure(const char* operation, HRESULT hr) {
        if (SUCCEEDED(hr))
            return;
        ++frameFailedCalls;
        lastFailure = static_cast<std::uint32_t>(hr);
        lastFailureOperation = operation ? operation : "Direct3D";
    }

    LPDIRECTDRAW4 dd4 = nullptr;
    LPDIRECTDRAWSURFACE4 front = nullptr;
    LPDIRECTDRAWSURFACE4 back = nullptr;
    LPDIRECTDRAWSURFACE4 zbuffer = nullptr;
    LPDIRECT3D3 d3d3 = nullptr;
    LPDIRECT3DDEVICE3 device3 = nullptr;
    LPDIRECT3DVIEWPORT3 viewport3 = nullptr;
    bool frontLocked = false;
};

Win32Graphics::Win32Graphics(std::uintptr_t windowHandle)
    : impl_(std::make_unique<Impl>(reinterpret_cast<HWND>(windowHandle))) {}

Win32Graphics::~Win32Graphics() {
    ReleaseViewport();
    ReleaseDirect3DDevice();
    ReleaseDirect3D();
    DeleteBuffers();
    ReleaseDirectDraw();
}

bool Win32Graphics::EnumerateDDrawDrivers(DirectXState& state) {
    impl_->drivers.clear();
    impl_->lastError.clear();
    DriverEnumContext context{&impl_->drivers};
    const HRESULT hr = DirectDrawEnumerateExA(enumDriverCallback, &context, 7u);
    if (FAILED(hr)) {
        impl_->lastError = "DirectDrawEnumerateExA itself returned a failure HRESULT.";
        return false;
    }
    state.Reset();
    for (const auto& driver : impl_->drivers) {
        if (!state.AddEnumeratedDriver(driver.info))
            break;
    }
    if (impl_->drivers.empty()) {
        impl_->lastError =
            "DirectDrawEnumerateExA succeeded, but no driver survived the "
            "3D/display-mode/Z-buffer filter.";
        return false;
    }
    return true;
}

const std::string& Win32Graphics::LastError() const {
    return impl_->lastError;
}

void Win32Graphics::SetTextureManager(CBMManager* textures) {
    impl_->textures = textures;
}

void Win32Graphics::ResetFrameStats() {
    impl_->frameDrawCalls = 0;
    impl_->frameGoodDraws = 0;
    impl_->frameFailedCalls = 0;
    impl_->lastFailure = 0;
    impl_->lastFailureOperation.clear();
}

bool Win32Graphics::FrameHadNativeFailure() const {
    return impl_->frameFailedCalls != 0;
}

std::uint32_t Win32Graphics::FrameDrawCalls() const {
    return impl_->frameDrawCalls;
}

std::uint32_t Win32Graphics::SuccessfulDrawCalls() const {
    return impl_->frameGoodDraws;
}

std::string Win32Graphics::FrameStats() const {
    std::string out = "D3D frame: draws=" + std::to_string(impl_->frameDrawCalls) +
        ", ok=" + std::to_string(impl_->frameGoodDraws) +
        ", failed calls=" + std::to_string(impl_->frameFailedCalls);
    if (impl_->frameFailedCalls != 0) {
        char buffer[32]{};
        std::snprintf(buffer, sizeof(buffer), "0x%08lX",
                      static_cast<unsigned long>(impl_->lastFailure));
        out += ", last=" + impl_->lastFailureOperation + " ";
        out += buffer;
    }
    return out;
}

bool Win32Graphics::CreateDirectDraw(std::uint16_t driverIndex) {
    ReleaseDirectDraw();
    if (driverIndex >= impl_->drivers.size())
        return false;
    impl_->selectedDriver = driverIndex;
    const auto& selected = impl_->drivers[driverIndex];
    const GUID* guid = selected.primary ? nullptr : &selected.guid;

    LPDIRECTDRAW oldDD = nullptr;
    if (FAILED(DirectDrawCreate(const_cast<GUID*>(guid), &oldDD, nullptr)) || !oldDD)
        return false;
    const HRESULT qi = oldDD->QueryInterface(IID_IDirectDraw4,
                                              reinterpret_cast<void**>(&impl_->dd4));
    oldDD->Release();
    if (FAILED(qi) || !impl_->dd4)
        return false;

    if (FAILED(impl_->dd4->SetCooperativeLevel(impl_->hwnd,
                                               DDSCL_EXCLUSIVE | DDSCL_FULLSCREEN))) {
        ReleaseDirectDraw();
        return false;
    }
    return true;
}

bool Win32Graphics::SetDisplayMode(const VideoMode& mode) {
    if (!impl_->dd4)
        return false;
    const HRESULT hr = impl_->dd4->SetDisplayMode(mode.width, mode.height,
                                                   mode.bitsPerPixel, 0, 0);
    if (SUCCEEDED(hr))
        impl_->currentMode = mode;
    return SUCCEEDED(hr);
}

bool Win32Graphics::CreateFrontBackBuffers() {
    if (!impl_->dd4 || impl_->selectedDriver >= impl_->drivers.size() ||
        impl_->currentMode.width == 0 || impl_->currentMode.height == 0)
        return false;
    DeleteBuffers();

    DDSURFACEDESC2 frontDesc{};
    frontDesc.dwSize = sizeof(frontDesc);
    frontDesc.dwFlags = DDSD_CAPS | DDSD_BACKBUFFERCOUNT;
    frontDesc.ddsCaps.dwCaps = DDSCAPS_PRIMARYSURFACE | DDSCAPS_FLIP |
                               DDSCAPS_COMPLEX | DDSCAPS_3DDEVICE;
    frontDesc.dwBackBufferCount = 1;
    HRESULT hr = impl_->dd4->CreateSurface(&frontDesc, &impl_->front, nullptr);
    if (FAILED(hr) || !impl_->front)
        return false;

    DDSCAPS2 backCaps{};
    backCaps.dwCaps = DDSCAPS_BACKBUFFER;
    hr = impl_->front->GetAttachedSurface(&backCaps, &impl_->back);
    if (FAILED(hr) || !impl_->back) {
        DeleteBuffers();
        return false;
    }

    DDSURFACEDESC2 zDesc{};
    zDesc.dwSize = sizeof(zDesc);
    zDesc.dwFlags = DDSD_CAPS | DDSD_WIDTH | DDSD_HEIGHT | DDSD_PIXELFORMAT;
    zDesc.dwWidth = impl_->currentMode.width;
    zDesc.dwHeight = impl_->currentMode.height;
    zDesc.ddsCaps.dwCaps = DDSCAPS_ZBUFFER;
    zDesc.ddpfPixelFormat = toNative(
        impl_->drivers[impl_->selectedDriver].info.zBufferFormat);
    hr = impl_->dd4->CreateSurface(&zDesc, &impl_->zbuffer, nullptr);
    if (FAILED(hr) || !impl_->zbuffer) {
        DeleteBuffers();
        return false;
    }
    hr = impl_->back->AddAttachedSurface(impl_->zbuffer);
    if (FAILED(hr)) {
        DeleteBuffers();
        return false;
    }
    return true;
}

bool Win32Graphics::RestoreBuffers() {
    return impl_->dd4 && SUCCEEDED(impl_->dd4->RestoreAllSurfaces());
}

void Win32Graphics::DeleteBuffers() {
    if (impl_->frontLocked && impl_->front) {
        impl_->front->Unlock(nullptr);
        impl_->frontLocked = false;
    }

    releaseCom(impl_->zbuffer);
    releaseCom(impl_->front);
    impl_->back = nullptr;
}

bool Win32Graphics::CreateDirect3D() {
    if (!impl_->dd4 || !impl_->back)
        return false;
    ReleaseDirect3DDevice();
    ReleaseDirect3D();
    if (FAILED(impl_->dd4->QueryInterface(IID_IDirect3D3,
                                          reinterpret_cast<void**>(&impl_->d3d3))) ||
        !impl_->d3d3)
        return false;
    if (FAILED(impl_->d3d3->CreateDevice(IID_IDirect3DHALDevice, impl_->back,
                                          &impl_->device3, nullptr)) ||
        !impl_->device3) {
        ReleaseDirect3D();
        return false;
    }
    return true;
}

bool Win32Graphics::EnumerateTextureFormats(
    std::vector<DDPixelFormat>& out) {
    out.clear();
    return impl_->device3 &&
        SUCCEEDED(impl_->device3->EnumTextureFormats(enumTextureCallback, &out));
}

bool Win32Graphics::CreateViewport(const ViewportDesc& viewport) {
    if (!impl_->d3d3 || !impl_->device3)
        return false;
    ReleaseViewport();
    if (FAILED(impl_->d3d3->CreateViewport(&impl_->viewport3, nullptr)) ||
        !impl_->viewport3)
        return false;
    if (FAILED(impl_->device3->AddViewport(impl_->viewport3)) ||
        FAILED(impl_->device3->SetCurrentViewport(impl_->viewport3))) {
        ReleaseViewport();
        return false;
    }

    D3DVIEWPORT2 native{};
    native.dwSize = sizeof(native);
    native.dwX = viewport.x;
    native.dwY = viewport.y;
    native.dwWidth = viewport.width;
    native.dwHeight = viewport.height;
    native.dvClipX = viewport.clipX;
    native.dvClipY = viewport.clipY;
    native.dvClipWidth = viewport.clipWidth;
    native.dvClipHeight = viewport.clipHeight;
    native.dvMinZ = viewport.minZ;
    native.dvMaxZ = viewport.maxZ;
    if (FAILED(impl_->viewport3->SetViewport2(&native))) {
        ReleaseViewport();
        return false;
    }
    return true;
}

void Win32Graphics::ReleaseViewport() {
    if (impl_->device3 && impl_->viewport3)
        impl_->device3->DeleteViewport(impl_->viewport3);
    releaseCom(impl_->viewport3);
}

void Win32Graphics::ReleaseDirect3DDevice() { releaseCom(impl_->device3); }
void Win32Graphics::ReleaseDirect3D() { releaseCom(impl_->d3d3); }
void Win32Graphics::ReleaseDirectDraw() {

    if (impl_->dd4)
        (void)impl_->dd4->SetCooperativeLevel(impl_->hwnd, DDSCL_NORMAL);
    releaseCom(impl_->dd4);
}

bool Win32Graphics::BeginScene() {
    if (!impl_->device3)
        return false;
    const HRESULT hr = impl_->device3->BeginScene();
    impl_->RecordFailure("BeginScene", hr);
    return SUCCEEDED(hr);
}

bool Win32Graphics::EndScene() {
    if (!impl_->device3)
        return false;
    const HRESULT hr = impl_->device3->EndScene();
    impl_->RecordFailure("EndScene", hr);
    return SUCCEEDED(hr);
}

void Win32Graphics::SetRenderState(std::uint32_t state, std::uint32_t value) {
    if (impl_->device3) {
        const HRESULT hr = impl_->device3->SetRenderState(
            static_cast<D3DRENDERSTATETYPE>(state), value);
        impl_->RecordFailure("SetRenderState", hr);
    }
}

void Win32Graphics::SetTextureStageState(std::uint32_t stage,
                                                std::uint32_t state,
                                                std::uint32_t value) {
    if (impl_->device3) {
        const HRESULT hr = impl_->device3->SetTextureStageState(
            stage, static_cast<D3DTEXTURESTAGESTATETYPE>(state), value);
        impl_->RecordFailure("SetTextureStageState", hr);
    }
}

void Win32Graphics::SetLightState(std::uint32_t state, std::uint32_t value) {
    if (impl_->device3) {
        const HRESULT hr = impl_->device3->SetLightState(
            static_cast<D3DLIGHTSTATETYPE>(state), value);
        impl_->RecordFailure("SetLightState", hr);
    }
}

void Win32Graphics::SetTexture(std::uint8_t textureIndex, std::uint8_t variant) {
    if (!impl_->device3)
        return;
    LPDIRECT3DTEXTURE2 texture = nullptr;
    if (impl_->textures) {
        texture = reinterpret_cast<LPDIRECT3DTEXTURE2>(
            CBM_GetTextureInterface(*impl_->textures,
                                                  textureIndex, variant));
    }
    const HRESULT hr = impl_->device3->SetTexture(0, texture);
    impl_->RecordFailure("SetTexture", hr);
}

bool Win32Graphics::TextureHasAlpha(std::uint8_t textureIndex) const {
    if (!impl_->textures)
        return false;
    const CBMPicture* picture = impl_->textures->Get(textureIndex);
    return picture && picture->HasAlpha();
}

void Win32Graphics::DrawPrimitive(std::uint32_t primitiveType,
                                         std::uint32_t vertexType,
                                         const ImmediateVertex* vertices,
                                         std::uint32_t vertexCount,
                                         std::uint32_t flags) {
    if (impl_->device3 && vertices) {
        ++impl_->frameDrawCalls;
        const HRESULT hr = impl_->device3->DrawPrimitive(
            static_cast<D3DPRIMITIVETYPE>(primitiveType), vertexType,
            const_cast<ImmediateVertex*>(vertices), vertexCount, flags);
        if (SUCCEEDED(hr))
            ++impl_->frameGoodDraws;
        else
            impl_->RecordFailure("DrawPrimitive", hr);
    }
}

void Win32Graphics::SetTransform(std::uint32_t state,
                                        const CD3DMATRIX& transform) {
    if (!impl_->device3)
        return;
    D3DMATRIX matrix{};
    static_assert(sizeof(matrix) == sizeof(transform.m), "D3D matrix size");
    std::memcpy(&matrix, transform.m, sizeof(matrix));
    const HRESULT hr = impl_->device3->SetTransform(
        static_cast<D3DTRANSFORMSTATETYPE>(state), &matrix);
    impl_->RecordFailure("SetTransform", hr);
}

std::uint32_t Win32Graphics::ComputeSphereVisibility(const CD3DVECTOR& center,
                                                             float radius) {
    if (!impl_->device3)
        return 0;
    D3DVECTOR nativeCenter{center.x, center.y, center.z};
    D3DVALUE nativeRadius = radius;
    DWORD status = 0;
    const HRESULT hr = impl_->device3->ComputeSphereVisibility(
        &nativeCenter, &nativeRadius, 1, 0, &status);
    if (FAILED(hr)) {
        impl_->RecordFailure("ComputeSphereVisibility", hr);
        return 0;
    }
    return status;
}

bool Win32Graphics::FrontSurfaceLost() {
    return !impl_->front || impl_->front->IsLost() != DD_OK;
}

bool Win32Graphics::RestoreAllBuffers() {
    return RestoreBuffers();
}

bool Win32Graphics::BackBufferReadyForFlip() {
    return impl_->back && impl_->back->GetFlipStatus(DDGFS_ISFLIPDONE) == DD_OK;
}

std::uint32_t Win32Graphics::FlipFrontBuffer() {
    if (!impl_->front)
        return static_cast<std::uint32_t>(E_FAIL);
    const HRESULT hr = impl_->front->Flip(nullptr, DDFLIP_WAIT);
    impl_->RecordFailure("Flip", hr);
    return static_cast<std::uint32_t>(hr);
}

bool Win32Graphics::Clear(std::uint32_t flags, std::uint32_t color,
                                 float z, std::uint32_t stencil) {
    if (!impl_->viewport3)
        return false;

    D3DRECT rect{};
    rect.x1 = 0;
    rect.y1 = 0;
    rect.x2 = static_cast<LONG>(impl_->currentMode.width);
    rect.y2 = static_cast<LONG>(impl_->currentMode.height);
    const HRESULT hr = impl_->viewport3->Clear2(1, &rect, flags, color, z, stencil);
    impl_->RecordFailure((flags & D3DCLEAR_ZBUFFER) ? "ClearZBuf" : "ClearFrame", hr);
    return SUCCEEDED(hr);
}

bool Win32Graphics::LockFrontSurface(ScreenLockedSurface& out) {
    out = {};
    if (!impl_->front || impl_->frontLocked)
        return false;
    DDSURFACEDESC2 desc{};
    desc.dwSize = sizeof(desc);
    if (FAILED(impl_->front->Lock(nullptr, &desc, DDLOCK_WAIT, nullptr)))
        return false;
    impl_->frontLocked = true;
    out.pixels = static_cast<const std::uint8_t*>(desc.lpSurface);
    out.pitch = desc.lPitch;
    out.rgbBitCount = desc.ddpfPixelFormat.dwRGBBitCount;
    out.redMask = desc.ddpfPixelFormat.dwRBitMask;
    out.greenMask = desc.ddpfPixelFormat.dwGBitMask;
    out.blueMask = desc.ddpfPixelFormat.dwBBitMask;
    return true;
}

bool Win32Graphics::UnlockFrontSurface() {
    if (!impl_->front || !impl_->frontLocked)
        return false;
    const HRESULT hr = impl_->front->Unlock(nullptr);
    if (SUCCEEDED(hr))
        impl_->frontLocked = false;
    return SUCCEEDED(hr);
}

std::uint32_t Win32Graphics::CreatePalette(
    std::uint32_t flags, const std::array<std::uint8_t, 256 * 4>& rgba,
    CBMNativeHandle& outPalette) {
    outPalette = 0;
    if (!impl_->dd4)
        return static_cast<std::uint32_t>(E_FAIL);
    std::array<PALETTEENTRY, 256> entries{};
    for (std::size_t i = 0; i < entries.size(); ++i) {
        entries[i].peRed = rgba[i * 4 + 0];
        entries[i].peGreen = rgba[i * 4 + 1];
        entries[i].peBlue = rgba[i * 4 + 2];
        entries[i].peFlags = rgba[i * 4 + 3];
    }
    LPDIRECTDRAWPALETTE palette = nullptr;
    const HRESULT hr = impl_->dd4->CreatePalette(flags, entries.data(), &palette, nullptr);
    if (SUCCEEDED(hr))
        outPalette = reinterpret_cast<CBMNativeHandle>(palette);
    return static_cast<std::uint32_t>(hr);
}

std::uint32_t Win32Graphics::CreateSurface(
    const CBMSurfaceDesc& desc, CBMNativeHandle& outSurface) {
    outSurface = 0;
    if (!impl_->dd4)
        return static_cast<std::uint32_t>(E_FAIL);
    DDSURFACEDESC2 native{};
    native.dwSize = desc.size ? desc.size : sizeof(native);
    native.dwFlags = desc.flags;
    native.dwWidth = desc.width;
    native.dwHeight = desc.height;
    native.dwMipMapCount = desc.mipMapCount;
    native.ddpfPixelFormat = toNative(desc.pixelFormat);
    native.ddsCaps.dwCaps = desc.caps;
    native.ddsCaps.dwCaps2 = desc.caps2;
    LPDIRECTDRAWSURFACE4 surface = nullptr;
    const HRESULT hr = impl_->dd4->CreateSurface(&native, &surface, nullptr);
    if (SUCCEEDED(hr))
        outSurface = reinterpret_cast<CBMNativeHandle>(surface);
    return static_cast<std::uint32_t>(hr);
}

std::uint32_t Win32Graphics::SetPalette(CBMNativeHandle surface,
                                                CBMNativeHandle palette) {
    auto* s = reinterpret_cast<LPDIRECTDRAWSURFACE4>(surface);
    auto* p = reinterpret_cast<LPDIRECTDRAWPALETTE>(palette);
    return static_cast<std::uint32_t>(s ? s->SetPalette(p) : E_FAIL);
}

std::uint32_t Win32Graphics::SetPaletteEntries(
    CBMNativeHandle palette, std::uint32_t flags, std::uint32_t start,
    std::uint32_t count, const std::array<std::uint8_t, 256 * 4>& rgba) {
    auto* p = reinterpret_cast<LPDIRECTDRAWPALETTE>(palette);
    if (!p || start > 255 || count > 256 - start)
        return static_cast<std::uint32_t>(E_FAIL);
    std::array<PALETTEENTRY, 256> entries{};
    for (std::size_t i = 0; i < count; ++i) {
        const std::size_t src = static_cast<std::size_t>(start) + i;
        entries[i].peRed = rgba[src * 4 + 0];
        entries[i].peGreen = rgba[src * 4 + 1];
        entries[i].peBlue = rgba[src * 4 + 2];
        entries[i].peFlags = rgba[src * 4 + 3];
    }
    return static_cast<std::uint32_t>(p->SetEntries(flags, start, count, entries.data()));
}

std::uint32_t Win32Graphics::QueryTexture2(
    CBMNativeHandle surface, const std::array<std::uint8_t, 16>& iid,
    CBMNativeHandle& outTexture2) {
    outTexture2 = 0;
    auto* s = reinterpret_cast<LPDIRECTDRAWSURFACE4>(surface);
    if (!s)
        return static_cast<std::uint32_t>(E_FAIL);
    const GUID guid = iidFromBytes(iid);
    LPDIRECT3DTEXTURE2 texture = nullptr;
    const HRESULT hr = s->QueryInterface(guid, reinterpret_cast<void**>(&texture));
    if (SUCCEEDED(hr))
        outTexture2 = reinterpret_cast<CBMNativeHandle>(texture);
    return static_cast<std::uint32_t>(hr);
}

std::uint32_t Win32Graphics::Lock(CBMNativeHandle surface,
                                          std::uint32_t flags,
                                          CBMLockedSurface& out) {
    out = {};
    auto* s = reinterpret_cast<LPDIRECTDRAWSURFACE4>(surface);
    if (!s)
        return static_cast<std::uint32_t>(E_FAIL);
    DDSURFACEDESC2 desc{};
    desc.dwSize = sizeof(desc);
    const HRESULT hr = s->Lock(nullptr, &desc, flags, nullptr);
    if (SUCCEEDED(hr)) {
        out.pixels = static_cast<std::uint8_t*>(desc.lpSurface);
        out.pitch = desc.lPitch >= 0 ? static_cast<std::size_t>(desc.lPitch)
                                    : static_cast<std::size_t>(-desc.lPitch);
        const std::size_t height = static_cast<std::size_t>(desc.dwHeight);
        out.capacity = (height != 0 && out.pitch <= std::numeric_limits<std::size_t>::max() / height)
            ? out.pitch * height : std::numeric_limits<std::size_t>::max();
    }
    return static_cast<std::uint32_t>(hr);
}

std::uint32_t Win32Graphics::Unlock(CBMNativeHandle surface) {
    auto* s = reinterpret_cast<LPDIRECTDRAWSURFACE4>(surface);
    return static_cast<std::uint32_t>(s ? s->Unlock(nullptr) : E_FAIL);
}

std::uint32_t Win32Graphics::GetAttachedMipSurface(
    CBMNativeHandle surface, std::uint32_t caps, CBMNativeHandle& outSurface) {
    outSurface = 0;
    auto* s = reinterpret_cast<LPDIRECTDRAWSURFACE4>(surface);
    if (!s)
        return static_cast<std::uint32_t>(E_FAIL);
    DDSCAPS2 nativeCaps{};
    nativeCaps.dwCaps = caps;
    LPDIRECTDRAWSURFACE4 child = nullptr;
    const HRESULT hr = s->GetAttachedSurface(&nativeCaps, &child);
    if (SUCCEEDED(hr))
        outSurface = reinterpret_cast<CBMNativeHandle>(child);
    return static_cast<std::uint32_t>(hr);
}

std::uint32_t Win32Graphics::Release(CBMNativeHandle handle) {
    auto* object = reinterpret_cast<IUnknown*>(handle);
    return object ? object->Release() : 0u;
}

bool Win32Graphics::NativeDeviceReady() const {
    return impl_->dd4 && impl_->front && impl_->back && impl_->d3d3 &&
           impl_->device3 && impl_->viewport3;
}

Win32CBMLoader::Win32CBMLoader(
    std::filesystem::path texturesDirectory, const DirectXState& directx,
    Win32Graphics& backend, std::uint8_t textureQuality)
    : texturesDirectory_(std::move(texturesDirectory)), directx_(directx),
      backend_(backend), textureQuality_(textureQuality) {}

std::unique_ptr<CBMPicture> Win32CBMLoader::LoadCBM(
    std::string_view canonicalName) {
    lastError_.clear();
    const DDrawDriver* driver = directx_.CurrentDriver();
    if (!driver) {
        lastError_ = "No active DirectDraw driver";
        return nullptr;
    }

    const auto convert = [](const DDPixelFormat& in) {
        CBMPixelFormat out{};
        out.size = in.size;
        out.flags = in.flags;
        out.fourCC = in.fourCC;
        out.bitsPerPixel = in.rgbBitCount;
        out.redMask = in.redMask;
        out.greenMask = in.greenMask;
        out.blueMask = in.blueMask;
        out.alphaMask = in.alphaMask;
        return out;
    };

    FileCBMLoader loader(
        texturesDirectory_, convert(driver->opaqueTextureFormat),
        convert(driver->alphaTextureFormat), backend_, textureQuality_);
    auto picture = loader.LoadCBM(canonicalName);
    lastError_ = loader.LastError();
    return picture;
}

struct Win32Input::Impl {
    using DirectInputCreateAFn = HRESULT (WINAPI *)(
        HINSTANCE, DWORD, LPDIRECTINPUTA*, LPUNKNOWN);

    Impl(HINSTANCE i, HWND w) : instance(i), hwnd(w) {}
    HINSTANCE instance = nullptr;
    HWND hwnd = nullptr;
    HMODULE dinputModule = nullptr;
    DirectInputCreateAFn directInputCreateA = nullptr;
    LPDIRECTINPUTA directInput = nullptr;
    LPDIRECTINPUTDEVICEA keyboard = nullptr;
    LPDIRECTINPUTDEVICEA mouse = nullptr;
    LPDIRECTINPUTDEVICEA joystickBase = nullptr;
    LPDIRECTINPUTDEVICEA enumeratedJoystick = nullptr;
    LPDIRECTINPUTDEVICE2A joystick2 = nullptr;
};

Win32Input::Win32Input(std::uintptr_t instanceHandle,
                                                 std::uintptr_t windowHandle)
    : impl_(std::make_unique<Impl>(reinterpret_cast<HINSTANCE>(instanceHandle),
                                   reinterpret_cast<HWND>(windowHandle))) {}

Win32Input::~Win32Input() {
    ReleaseJoystick();
    ReleaseDevice(ControllerDevice::Joystick);
    ReleaseDevice(ControllerDevice::Mouse);
    ReleaseDevice(ControllerDevice::Keyboard);
    ReleaseDirectInput();
    if (impl_->dinputModule) {
        FreeLibrary(impl_->dinputModule);
        impl_->dinputModule = nullptr;
        impl_->directInputCreateA = nullptr;
    }
}

bool Win32Input::CreateDirectInput(std::uint32_t version) {
    ReleaseDirectInput();

    if (!impl_->dinputModule) {
        impl_->dinputModule = LoadLibraryW(L"dinput.dll");
        if (!impl_->dinputModule)
            return false;
    }
    if (!impl_->directInputCreateA) {
        impl_->directInputCreateA = reinterpret_cast<Impl::DirectInputCreateAFn>(
            GetProcAddress(impl_->dinputModule, "DirectInputCreateA"));
        if (!impl_->directInputCreateA)
            return false;
    }

    const HRESULT hr = impl_->directInputCreateA(
        impl_->instance, version, &impl_->directInput, nullptr);
    if (FAILED(hr) || !impl_->directInput) {
        releaseCom(impl_->directInput);
        return false;
    }
    return true;
}

void Win32Input::ReleaseDirectInput() { releaseCom(impl_->directInput); }

bool Win32Input::CreateDevice(ControllerDevice device) {
    if (!impl_->directInput)
        return false;
    LPDIRECTINPUTDEVICEA* slot = deviceSlot(device, impl_->keyboard,
                                            impl_->mouse, impl_->joystickBase);
    if (!slot || *slot)
        return false;
    const GUID* guid = nullptr;
    switch (device) {
        case ControllerDevice::Keyboard: guid = &GUID_SysKeyboard; break;
        case ControllerDevice::Mouse: guid = &GUID_SysMouse; break;
        case ControllerDevice::Joystick: return impl_->joystick2 != nullptr;
    }
    return SUCCEEDED(impl_->directInput->CreateDevice(*guid, slot, nullptr)) && *slot;
}

bool Win32Input::SetDataFormat(ControllerDevice device) {
    const DIDATAFORMAT* format = dataFormatFor(device);
    if (!format)
        return false;
    if (device == ControllerDevice::Joystick)
        return impl_->joystick2 && SUCCEEDED(impl_->joystick2->SetDataFormat(format));
    auto** slot = deviceSlot(device, impl_->keyboard, impl_->mouse, impl_->joystickBase);
    return slot && *slot && SUCCEEDED((*slot)->SetDataFormat(format));
}

bool Win32Input::SetCooperativeLevel(ControllerDevice device,
                                                   std::uint32_t flags) {
    if (device == ControllerDevice::Joystick)
        return impl_->joystick2 && SUCCEEDED(impl_->joystick2->SetCooperativeLevel(impl_->hwnd, flags));
    auto** slot = deviceSlot(device, impl_->keyboard, impl_->mouse, impl_->joystickBase);
    return slot && *slot && SUCCEEDED((*slot)->SetCooperativeLevel(impl_->hwnd, flags));
}

bool Win32Input::Acquire(ControllerDevice device) {
    if (device == ControllerDevice::Joystick)
        return impl_->joystick2 && SUCCEEDED(impl_->joystick2->Acquire());
    auto** slot = deviceSlot(device, impl_->keyboard, impl_->mouse, impl_->joystickBase);
    return slot && *slot && SUCCEEDED((*slot)->Acquire());
}

void Win32Input::Unacquire(ControllerDevice device) {
    if (device == ControllerDevice::Joystick) {
        if (impl_->joystick2) impl_->joystick2->Unacquire();
        return;
    }
    auto** slot = deviceSlot(device, impl_->keyboard, impl_->mouse, impl_->joystickBase);
    if (slot && *slot) (*slot)->Unacquire();
}

void Win32Input::ReleaseDevice(ControllerDevice device) {
    if (device == ControllerDevice::Joystick) {
        releaseCom(impl_->joystick2);
        releaseCom(impl_->joystickBase);
        return;
    }
    auto** slot = deviceSlot(device, impl_->keyboard, impl_->mouse, impl_->joystickBase);
    if (slot) releaseCom(*slot);
}

namespace {
struct JoystickEnumContext {
    LPDIRECTINPUTA directInput = nullptr;
    LPDIRECTINPUTDEVICEA* result = nullptr;
};

BOOL CALLBACK enumJoystickCallback(LPCDIDEVICEINSTANCEA instance, LPVOID context) {
    auto* ctx = static_cast<JoystickEnumContext*>(context);
    if (!ctx || !ctx->directInput || !ctx->result || !instance)
        return DIENUM_CONTINUE;
    if (SUCCEEDED(ctx->directInput->CreateDevice(instance->guidInstance,
                                                  ctx->result, nullptr)) &&
        *ctx->result)
        return DIENUM_STOP;
    return DIENUM_CONTINUE;
}
}

bool Win32Input::EnumerateJoystick() {
    ReleaseJoystick();
    if (!impl_->directInput)
        return false;
    JoystickEnumContext context{impl_->directInput, &impl_->enumeratedJoystick};
    impl_->directInput->EnumDevices(DIDEVTYPE_JOYSTICK, enumJoystickCallback,
                                    &context, DIEDFL_ATTACHEDONLY);
    return impl_->enumeratedJoystick != nullptr;
}

bool Win32Input::QueryJoystickDevice2() {
    releaseCom(impl_->joystick2);
    if (!impl_->enumeratedJoystick)
        return false;
    const HRESULT hr = impl_->enumeratedJoystick->QueryInterface(
        IID_IDirectInputDevice2A, reinterpret_cast<void**>(&impl_->joystick2));
    return SUCCEEDED(hr) && impl_->joystick2;
}

void Win32Input::ReleaseJoystick() {
    releaseCom(impl_->enumeratedJoystick);
}

bool Win32Input::SetJoystickRange(ControllerAxis axis,
                                                std::int32_t minimum,
                                                std::int32_t maximum) {
    if (!impl_->joystick2)
        return false;
    DIPROPRANGE range{};
    range.diph.dwSize = sizeof(range);
    range.diph.dwHeaderSize = sizeof(range.diph);
    range.diph.dwHow = DIPH_BYOFFSET;
    range.diph.dwObj = axis == ControllerAxis::X ? DIJOFS_X : DIJOFS_Y;
    range.lMin = minimum;
    range.lMax = maximum;
    return SUCCEEDED(impl_->joystick2->SetProperty(DIPROP_RANGE, &range.diph));
}

bool Win32Input::SetJoystickDeadZone(ControllerAxis axis,
                                                   std::uint32_t value) {
    if (!impl_->joystick2)
        return false;
    DIPROPDWORD dead{};
    dead.diph.dwSize = sizeof(dead);
    dead.diph.dwHeaderSize = sizeof(dead.diph);
    dead.diph.dwHow = DIPH_BYOFFSET;
    dead.diph.dwObj = axis == ControllerAxis::X ? DIJOFS_X : DIJOFS_Y;
    dead.dwData = value;
    return SUCCEEDED(impl_->joystick2->SetProperty(DIPROP_DEADZONE, &dead.diph));
}

bool Win32Input::PollJoystick() {
    return impl_->joystick2 && SUCCEEDED(impl_->joystick2->Poll());
}

bool Win32Input::GetKeyboardState(std::uint8_t out[256]) {
    return impl_->keyboard && SUCCEEDED(impl_->keyboard->GetDeviceState(256, out));
}

bool Win32Input::GetMouseState(std::uint8_t out[16]) {
    return impl_->mouse && SUCCEEDED(impl_->mouse->GetDeviceState(16, out));
}

bool Win32Input::GetJoystickState(std::uint8_t out[0x50]) {
    return impl_->joystick2 && SUCCEEDED(impl_->joystick2->GetDeviceState(0x50, out));
}

Win32Timer::Win32Timer() {

    TIMECAPS caps{};
    (void)timeGetDevCaps(&caps, sizeof(caps));
    periodMilliseconds_ = static_cast<std::uint32_t>(caps.wPeriodMin);
    (void)timeBeginPeriod(static_cast<UINT>(periodMilliseconds_));
    periodActive_ = true;
}

Win32Timer::~Win32Timer() {

    if (periodActive_)
        (void)timeEndPeriod(static_cast<UINT>(periodMilliseconds_));
}

void Win32Timer::ResetTimer() {
    resetTick_ = static_cast<std::uint32_t>(timeGetTime());
}

float Win32Timer::ElapsedSeconds() {

    constexpr float kMillisecondsToSeconds = 0.0010000000474974513f;
    const std::uint32_t now = static_cast<std::uint32_t>(timeGetTime());
    const std::uint32_t delta = now - resetTick_;
    return static_cast<float>(static_cast<double>(delta) *
                              static_cast<double>(kMillisecondsToSeconds));
}

}

using namespace flydemo;

namespace {

constexpr DWORD WindowClassStyle = 0x00000003u;
constexpr DWORD WindowExStyle = 0x00040000u;
constexpr DWORD WindowStyle = 0x30CF0000u;
constexpr UINT IconResource = 112u;
constexpr UINT CursorResource = 0x7F00u;
constexpr int BackgroundStockObject = 4;
constexpr UINT StartupDialogResource = 107u;
constexpr UINT TimeOfDayDialogResource = 101u;
constexpr WORD TimeEvening = 1000u;
constexpr WORD TimeNight = 1001u;
constexpr WORD TimeDay = 1002u;
constexpr WORD TimeRun = 1003u;
constexpr WORD StartupContinue = 1007u;

HINSTANCE gInstance = nullptr;
HWND gMainWindow = nullptr;
bool gStartupAborted = false;
bool gCursorHidden = false;
const char* SelectedAmbience = "evening.amb";
bool DayChecked = false;
bool EveningChecked = true;
bool NightChecked = false;
bool RunRequested = false;

void ApplyDialogChecks(HWND dialog) {

    SendMessageA(GetDlgItem(dialog, TimeDay),
                 BM_SETCHECK, DayChecked ? 1 : 0, 0);
    SendMessageA(GetDlgItem(dialog, TimeEvening),
                 BM_SETCHECK, EveningChecked ? 1 : 0, 0);
    SendMessageA(GetDlgItem(dialog, TimeNight),
                 BM_SETCHECK, NightChecked ? 1 : 0, 0);
}

INT_PTR CALLBACK TimeOfDayDialogProc(HWND dialog, UINT message,
                                     WPARAM wParam, LPARAM) {
    if (message == WM_INITDIALOG) {

        SelectedAmbience = "evening.amb";
        DayChecked = false;
        EveningChecked = true;
        NightChecked = false;
        RunRequested = false;
        ApplyDialogChecks(dialog);
        return FALSE;
    }
    if (message == WM_CLOSE) {

        gStartupAborted = true;
        EndDialog(dialog, 0);
        return TRUE;
    }
    if (message != WM_COMMAND)
        return FALSE;

    const std::uint16_t command = LOWORD(wParam);
    switch (command) {
    case TimeEvening:
        SelectedAmbience = "evening.amb";
        DayChecked = false;
        EveningChecked = true;
        NightChecked = false;
        break;
    case TimeNight:
        SelectedAmbience = "night.amb";
        DayChecked = false;
        EveningChecked = false;
        NightChecked = true;
        break;
    case TimeDay:
        SelectedAmbience = "day.amb";
        DayChecked = true;
        EveningChecked = false;
        NightChecked = false;
        break;
    case TimeRun:
        RunRequested = true;
        EndDialog(dialog, 0);
        return TRUE;
    default:
        return FALSE;
    }
    ApplyDialogChecks(dialog);
    return TRUE;
}

INT_PTR CALLBACK StartupInfoDialogProc(HWND dialog, UINT message,
                                       WPARAM wParam, LPARAM) {
    if (message == WM_CLOSE) {

        gStartupAborted = true;
        EndDialog(dialog, 0);
        return TRUE;
    }
    if (message == WM_COMMAND &&
        LOWORD(wParam) == StartupContinue) {

        EndDialog(dialog, 0);
        if (DialogBoxParamA(gInstance,
                            MAKEINTRESOURCEA(TimeOfDayDialogResource),
                            gMainWindow, TimeOfDayDialogProc, 0) == -1)
            gStartupAborted = true;
        return TRUE;
    }
    return FALSE;
}

LRESULT CALLBACK FlyWndProc(HWND hwnd, UINT message,
                                          WPARAM wParam, LPARAM lParam) {

    if (message == WM_DESTROY)
        PostQuitMessage(0);
    return DefWindowProcA(hwnd, message, wParam, lParam);
}

std::ofstream LogFile;
std::filesystem::path LogFilePath;

void OpenLogFile() {
    if (LogFile.is_open())
        return;

    if (LogFilePath.empty()) {
        std::error_code error;
        const std::filesystem::path current = std::filesystem::current_path(error);
        LogFilePath = error
            ? std::filesystem::path(std::string(kLogFile))
            : current / std::string(kLogFile);
    }

    LogFile.open(LogFilePath, std::ios::out | std::ios::trunc);
    if (LogFile) {
        LogFile << "Crashday - LogFile\n";
        const std::time_t now = std::time(nullptr);
        if (const char* stamp = std::ctime(&now))
            LogFile << stamp;
        else
            LogFile << "unknown time\n";
        LogFile << "\n";
        LogFile.flush();
    }
}

void WriteLog(const std::string& text) {
    if (!LogFile.is_open())
        OpenLogFile();
    if (LogFile) {
        LogFile << text << "\n";
        LogFile.flush();
    }
}

void LifecycleWrite(std::string_view text) {
    WriteLog(std::string(text));
}

void LogTextureLoad(void*, std::string_view sourceName,
                    std::string_view aliasName, const CBMPicture* picture,
                    bool success) {
    std::string line = "New PfxTex: ";
    if (!aliasName.empty()) {
        line += "(LoadAs ";
        line.append(aliasName.data(), aliasName.size());
        line += ") ";
    }
    line += "Load CBM file '";
    line.append(sourceName.data(), sourceName.size());
    line += "'... ";
    if (picture && picture->HasAlpha())
        line += "(AlphaTex) ";
    line += success ? "-> ok!" : "failed!";
    WriteLog(line);
}

void LogTextureDelete(void*, std::string_view name, bool wasLoaded) {
    std::string line;
    if (wasLoaded) {
        line = "Delete texture '";
        line.append(name.data(), name.size());
        line += "' from texture list...";
    } else {
        line = "Texture '";
        line.append(name.data(), name.size());
        line += "' couldn't be deleted (not loaded)";
    }
    WriteLog(line);
}

void LogTextureDeleteAll(void*) {
    WriteLog("Delete all textures from texture list...");
}

std::string DriverDisplayName(const DDrawDriver& driver) {
    return driver.description.empty() ? driver.name : driver.description;
}

void LogGraphicsDrivers(const DirectXState& directx) {
    for (const DDrawDriver& driver : directx.Drivers()) {
        std::string line = " - " + DriverDisplayName(driver);
        if (!driver.name.empty())
            line += " (" + driver.name + ")";
        WriteLog(line);
        WriteLog("=> Device suitable!");

        line = "   Video modes: ";
        for (const VideoMode& mode : driver.modes) {
            line += " [" + std::to_string(mode.width) + "x" +
                    std::to_string(mode.height) + "x" +
                    std::to_string(mode.bitsPerPixel) + " G:" +
                    std::to_string(mode.flags) + "]";
        }
        WriteLog(line);

        if (!driver.modes.empty()) {
            std::size_t index = driver.defaultMode;
            if (index >= driver.modes.size())
                index = 0;
            const VideoMode& mode = driver.modes[index];
            WriteLog("   Default mode: " + std::to_string(mode.width) + "x" +
                     std::to_string(mode.height) + "x" +
                     std::to_string(mode.bitsPerPixel));
        }
        WriteLog("   ZBuffer: " + std::to_string(driver.zBufferBitDepth) + "bit");
        WriteLog("");
    }
    WriteLog(std::to_string(directx.Drivers().size()) + " device(s) available");
}

std::string GameDirectoryForLog(const std::filesystem::path& path) {
    std::string value = path.string();
    if (!value.empty() && value.back() != '\\' && value.back() != '/')
        value.push_back('\\');
    return value;
}

void CloseLogFile() {
    if (!LogFile.is_open())
        return;
    LogFile << "\n\nLogFile closed\n";
    LogFile.flush();
    LogFile.close();
}

class LogFileLifetime final {
public:
    LogFileLifetime() { OpenLogFile(); }
    ~LogFileLifetime() { CloseLogFile(); }
};

void ShowCursorForFatalError() {

    ShowCursor(TRUE);
    gCursorHidden = false;
}

void ShowOriginalFatal(const char* message, const char* sourceFile = nullptr,
                       int sourceLine = -1) {
    std::string body = (message && *message) ? message : "!!! Fatal error !!!";

    if (sourceFile && *sourceFile && sourceLine >= 0) {
        body += "\n(file ";
        body += sourceFile;
        body += ", line ";
        body += std::to_string(sourceLine);
        body += ")\n";
    }
    ShowCursorForFatalError();
    MessageBoxA(nullptr, body.c_str(), "Error!", MB_OK | MB_ICONWARNING);
}

void ShowOriginalError(const char* category, const char* detail = nullptr) {
    std::string body;
    if (category && *category) body = category;
    if (detail && *detail) {
        if (!body.empty()) body += "\n";
        body += detail;
    }
    ShowOriginalFatal(body.c_str());
}

void ShowPropsFxError(const char* detail) {
    std::string body = "Error in PropsFX-Engine!";
    if (detail && *detail) { body += "\n"; body += detail; }
    ShowOriginalFatal(body.c_str());
}

void ShowFileNotFound(const std::string& file) {
    WriteLog("The file was not found: " + file);
    ShowOriginalError("The file was not found.");
}

bool CaptureScreenshotLikeExe(ScreenState& screen,
                              const DirectXState& directx,
                              Win32Graphics& native,
                              const std::filesystem::path& dataRoot) {
    const VideoMode* mode = directx.CurrentMode();
    if (!mode) {
        WriteLog("Screenshot failed: no active display mode");
        return false;
    }

    CDFileOperations screenshotFiles;
    std::filesystem::path writtenPath;
    std::string screenshotError;
    const bool written = screen.Screenshot(
        static_cast<std::int32_t>(mode->width),
        static_cast<std::int32_t>(mode->height),
        dataRoot / "TEXTURES" / "scrnshot.bmt",
        dataRoot, native, screenshotFiles, &writtenPath, &screenshotError);

    if (written) {
        WriteLog("Screenshot written to '" +
                 writtenPath.filename().string() + "'");
        return true;
    }

    if (screenshotError.empty())
        screenshotError = "Unknown screenshot error.";
    WriteLog(screenshotError);
    ShowOriginalError("Engine-Fehler!", screenshotError.c_str());
    return false;
}

void ShutdownControllerLikeExe(void* controller,
                               ControllerDeviceState& devices,
                               ControlInput& input) {
    const bool hadInput = devices.DInputObj;
    const bool hadKeyboard = devices.DIKeyb;
    const bool hadMouse = devices.DIMouse;
    const bool hadJoystick = devices.DIJoystick;

    if (hadInput)
        WriteLog("Shut down DirectInput...");

    Controller_Shutdown(controller, devices, input);

    if (hadKeyboard && !devices.DIKeyb)
        WriteLog("Keyboard released");
    if (hadMouse && !devices.DIMouse)
        WriteLog("Mouse released");
    if (hadJoystick && !devices.DIJoystick)
        WriteLog("Joystick released");
    if (hadInput && !devices.DInputObj)
        WriteLog("DirectInput object deleted");
    if (hadInput)
        WriteLog("DirectInput shut down");

    WriteLog("ControllerInterface shut down");
    WriteLog("");
}

void ShutdownPropsFxLikeExe(CBMManager& textures,
                            DirectXState& directx,
                            Win32Graphics& native) {
    const bool hadDirect3D = directx.Direct3DReady() || directx.DeviceReady() ||
                             directx.ViewportReady() || directx.FrontBufferReady() ||
                             directx.BackBufferReady();
    const bool hadBuffers = directx.FrontBufferReady() || directx.BackBufferReady();
    const bool hadDirectDraw = directx.DirectDrawReady();

    WriteLog("Shut down PropsFX-Engine...");
    WriteLog("Shut down DirectX...");
    textures.DeleteAll();

    if (hadDirect3D) {
        WriteLog("Shut down Direct3D...");
        if (hadBuffers)
            WriteLog("Delete all buffers...");
        directx.ShutdownDirect3D(native);
    }
    if (hadDirectDraw) {
        WriteLog("Shut down DirectDraw...");
        directx.ShutdownDirectDraw(native);
        if (gCursorHidden) {
            ShowCursor(TRUE);
            gCursorHidden = false;
        }
    }

    WriteLog("DirectX shut down");
    WriteLog("");
    WriteLog("PropsFX-Engine shut down");
    WriteLog("");
}

std::filesystem::path ExecutableDirectory() {
    std::array<char, 0x208> buffer{};
    const DWORD n = GetModuleFileNameA(nullptr, buffer.data(),
                                       static_cast<DWORD>(buffer.size()));
    if (n == 0 || n >= buffer.size())
        return {};

    const std::string modulePath(buffer.data(), n);
    const std::size_t slash = modulePath.find_last_of('\\');
    if (slash == std::string::npos)
        return {};
    return std::filesystem::path(modulePath.substr(0, slash + 1));
}

}

int WINAPI WinMain(HINSTANCE instance, HINSTANCE, LPSTR, int showCommand) {
    LogFileLifetime logFile;
    LifecycleSink = &LifecycleWrite;
    WriteLog("-- Crashday FlyAround Demo --");
    WriteLog("");

    WNDCLASSA wc{};
    wc.style = WindowClassStyle;
    wc.lpfnWndProc = FlyWndProc;
    wc.hInstance = instance;
    wc.hIcon = LoadIconA(instance, MAKEINTRESOURCEA(IconResource));
    wc.hCursor = LoadCursorA(nullptr, MAKEINTRESOURCEA(CursorResource));
    wc.hbrBackground = static_cast<HBRUSH>(GetStockObject(BackgroundStockObject));
    wc.lpszClassName = kWindowClass.data();
    if (!RegisterClassA(&wc)) {
        WriteLog("RegisterClassA failed");
        ShowOriginalError("!!! Fatal error !!!");
        return 1;
    }

    const int desktopWidth = GetSystemMetrics(SM_CXSCREEN);
    const int desktopHeight = GetSystemMetrics(SM_CYSCREEN);
    HWND hwnd = CreateWindowExA(WindowExStyle, wc.lpszClassName,
                                kWindowTitle.data(), WindowStyle,
                                0, 0, desktopWidth, desktopHeight,
                                nullptr, nullptr, instance, nullptr);
    if (!hwnd) {
        WriteLog("CreateWindowExA failed");
        ShowOriginalError("!!! Fatal error !!!");
        return 2;
    }
    ShowWindow(hwnd, showCommand == SW_HIDE ? SW_SHOW : showCommand);
    UpdateWindow(hwnd);

    gInstance = instance;
    gMainWindow = hwnd;
    gStartupAborted = false;
    SelectedAmbience = "evening.amb";
    DayChecked = false;
    EveningChecked = true;
    NightChecked = false;
    RunRequested = false;
    const INT_PTR startupResult = DialogBoxParamA(
        instance, MAKEINTRESOURCEA(StartupDialogResource), hwnd,
        StartupInfoDialogProc, 0);
    if (startupResult == -1) {
        WriteLog("DialogBoxParamA startup resources failed");
        ShowOriginalError("!!! Fatal error !!!");
        return 3;
    }
    if (gStartupAborted || !RunRequested) {
        return 0;
    }

    const std::filesystem::path dataRoot = ExecutableDirectory();
    WriteLog("Create CrashdayDirectory object...");
    WriteLog("Use parameter string for game directory...");
    auto gameDirectory = std::make_unique<CrashdayDirectory>(dataRoot);
    if (!gameDirectory->Exists()) {
        WriteLog("Given path string: The game directory doesn't exist!");
        ShowOriginalFatal("Given path string: The game directory doesn't exist!");
        return 4;
    }
    WriteLog("Crashday directory: " + GameDirectoryForLog(dataRoot));
    WriteLog("");

    WriteLog("Create ControllerInterface...");
    WriteLog("Starting DirectInput...");
    std::array<std::uint8_t, control_off::ObjectStateSpan> controllerImage{};
    ControllerDeviceState controllerDevices{};
    Win32Input input(reinterpret_cast<std::uintptr_t>(instance),
                     reinterpret_cast<std::uintptr_t>(hwnd));
    const bool directInputStarted = Controller_StartInput(
        controllerImage.data(), controllerDevices, input);
    if (controllerDevices.DInputObj)
        WriteLog("DirectInput object ok!");
    if (controllerDevices.DIKeyb)
        WriteLog("Keyboard ok!");
    if (controllerDevices.DIMouse)
        WriteLog("Mouse ok!");
    if (controllerDevices.DIJoystick) {
        WriteLog("Joystick found.");
        WriteLog("Joystick ok!");
    } else {
        WriteLog("No joystick found.");
    }
    if (directInputStarted) {
        WriteLog("DirectInput initialised");
    } else {
        WriteLog("DirectInput initialisation failed");
    }
    WriteLog(controllerImage[control_off::JoystickActive] != 0
                 ? "Joystick support activated"
                 : "Joystick support deactivated");
    if (directInputStarted)
        WriteLog("ControllerInterface initialised");
    WriteLog("");

    WriteLog("Initialise PropsFX-Engine...");
    WriteLog("Initialise DirectX...");
    WriteLog("Starting DirectDraw...");
    WriteLog("List graphics devices...");
    WriteLog("");

    Win32Graphics native(reinterpret_cast<std::uintptr_t>(hwnd));
    DirectXState directx;
    if (!native.EnumerateDDrawDrivers(directx)) {
        if (!native.LastError().empty())
            WriteLog(native.LastError());
        ShowPropsFxError("You system doesn't contain a suitable graphics card which is needed for the program!");
        if (controllerDevices.DInputObj)
            Controller_Shutdown(controllerImage.data(), controllerDevices, input);
        return 4;
    }
    LogGraphicsDrivers(directx);

    WriteLog("Set default render options...");
    RenderOptionsState options;
    options.ResetDefaults();

    WriteLog("Initialising texture list...");
    CBMManager textures;
    textures.SetObserver(nullptr, &LogTextureLoad, &LogTextureDelete,
                         &LogTextureDeleteAll);
    native.SetTextureManager(&textures);

    EngineSettings graphicsSettings;
    CDFileOperations settingsFile;
    const EngineSettings::LoadResult settingsResult = graphicsSettings.Load(
        0, *gameDirectory, settingsFile, directx, options);
    if (settingsResult == EngineSettings::LoadResult::Missing) {
        WriteLog("Load engine settings... -> propsfx.cfg couldn't be found!");
        ShowOriginalError("Error in PropsFX-Engine!",
                          "The engine needs graphics settings to be set.\r\nPlease run the 3D-Setup.");
        if (controllerDevices.DInputObj)
            Controller_Shutdown(controllerImage.data(), controllerDevices, input);
        return 5;
    }
    if (settingsResult == EngineSettings::LoadResult::OpenFailed) {
        WriteLog("Load engine settings... failed!");
        ShowOriginalError("Error in PropsFX-Engine!",
                          "The engine needs video mode settings to be set.\r\nPlease start the 3D-Setup.");
        if (controllerDevices.DInputObj)
            Controller_Shutdown(controllerImage.data(), controllerDevices, input);
        return 5;
    }

    const DDrawDriver* selectedDriver = directx.CurrentDriver();
    const std::string selectedDriverName = selectedDriver
        ? DriverDisplayName(*selectedDriver) : std::string("DirectDraw");
    WriteLog("Load engine settings... Select display driver '" +
             selectedDriverName + "'...");
    WriteLog("Select mode " + std::to_string(directx.PreferredModeIndex()) +
             " for ingame rendermode...");
    WriteLog("ok");
    WriteLog("");

    WriteLog("Activate display driver '" + selectedDriverName + "'...");
    if (!directx.StartSelectedDriver(native)) {
        WriteLog("failed!");
        ShowOriginalError("Error in PropsFX-Engine:",
                          "The creation of a DirectDraw object for the selected display driver failed.\r\n(Should theoretically not happen)");
        if (controllerDevices.DInputObj)
            Controller_Shutdown(controllerImage.data(), controllerDevices, input);
        return 6;
    }

    WriteLog("Activate ingame rendermode: ");
    if (selectedDriver && !selectedDriver->modes.empty()) {
        std::size_t requestedMode = directx.PreferredModeIndex() < 0
            ? 0u : static_cast<std::size_t>(directx.PreferredModeIndex());
        if (requestedMode >= selectedDriver->modes.size())
            requestedMode = selectedDriver->defaultMode < selectedDriver->modes.size()
                ? selectedDriver->defaultMode : 0u;
        const VideoMode& mode = selectedDriver->modes[requestedMode];
        WriteLog("Activate resolution " + std::to_string(mode.width) + "x" +
                 std::to_string(mode.height) + "x" +
                 std::to_string(mode.bitsPerPixel) + "...");
    }
    if (!directx.ActivatePreferredMode(native)) {
        WriteLog("failed!");
        ShowOriginalError("Error in PropsFX-Engine", "Setting resolution failed!");
        directx.Shutdown(native);
        if (controllerDevices.DInputObj)
            Controller_Shutdown(controllerImage.data(), controllerDevices, input);
        return 7;
    }

    ShowCursor(FALSE);
    gCursorHidden = true;
    WriteLog("Starting Direct3D...");
    WriteLog("Delete all buffers...");
    WriteLog("Create front- and backbuffer...");
    if (!directx.StartDirect3D(native)) {
        WriteLog("failed!");
        ShowPropsFxError("Creation of a hardware D3D-renderer\r\nfailed!");
        directx.Shutdown(native);
        if (controllerDevices.DInputObj)
            Controller_Shutdown(controllerImage.data(), controllerDevices, input);
        return 8;
    }
    WriteLog("D3D-renderer with hardware support created");

    selectedDriver = directx.CurrentDriver();
    if (selectedDriver) {
        const std::uint32_t opaqueBits = selectedDriver->opaqueTextureFormat.rgbBitCount;
        const std::uint32_t alphaBits = selectedDriver->alphaTextureFormat.rgbBitCount;
        WriteLog(opaqueBits != 0
                     ? "Search for a nonalpha-texture format... 555 565 888 -> Use " +
                           std::to_string(opaqueBits) + "bit format!"
                     : "Search for a nonalpha-texture format... none found!");
        WriteLog(alphaBits != 0
                     ? "Search for alpha-texture format... 1555 4444 8888 -> Use " +
                           std::to_string(alphaBits) + "bit format!"
                     : "Search for alpha-texture format... none found!");
    }

    WriteLog("Set screen format tonormal...");
    if (!directx.SetScreenFormat(ScreenFormat::Normal, native)) {
        ShowPropsFxError("Creation of viewport failed!");
        directx.Shutdown(native);
        if (controllerDevices.DInputObj)
            Controller_Shutdown(controllerImage.data(), controllerDevices, input);
        return 8;
    }
    WriteLog("Create viewport...");

    Win32CBMLoader textureLoader(
        dataRoot / "TEXTURES", directx, native,
        static_cast<std::uint8_t>(options.textureQuality));

    RendererState renderer;
    renderer.SetDeviceAvailable(true);
    renderer.SetViewportAvailable(true);
    renderer.SetState1DOptions(options.flag4E, options.flag51);

    WriteLog("Activate current render settings...");
    if (!options.Activate(native, true, false, 0x00000000u)) {
        ShowPropsFxError("The Direct3D render states couldn't be activated!");
        directx.Shutdown(native);
        if (controllerDevices.DInputObj)
            Controller_Shutdown(controllerImage.data(), controllerDevices, input);
        return 8;
    }
    WriteLog("");

    SceneEnv defaultEnvironment(textures, textureLoader, options, native);
    defaultEnvironment.SetFogColor(0x0096AAB4u);
    WriteLog("sky box setup...");
    Environment.ApplyDefault(false, defaultEnvironment);
    WriteLog("Activate current render settings...");
    (void)options.Activate(native, true, false,
                           Environment.State().tail.packedColor8);
    WriteLog("PropsFX-Engine initialised");
    WriteLog("");

    ScreenState screen;
    screen.SetFrontBufferAvailable(true);
    screen.SetBackBufferAvailable(true);
    screen.SetViewportAvailable(true);

    std::string sceneError;
    if (!InitFlyScene(dataRoot, std::string(SelectedAmbience),
                      textures, textureLoader, renderer, options, native,
                      &sceneError)) {
        ShowFileNotFound(sceneError);
        CloseFlyScene();
        textures.DeleteAll();
        directx.Shutdown(native);
        if (controllerDevices.DInputObj)
            Controller_Shutdown(controllerImage.data(), controllerDevices, input);
        return 9;
    }

    (void)gameDirectory->ChangeTo(std::string(kSoundsDirectory));
    const std::string musicPath = std::string(kDemoMusic);
    (void)PlaySoundA(musicPath.c_str(), nullptr,
                     SND_FILENAME | SND_ASYNC | SND_LOOP);

    auto timer = std::make_unique<Win32Timer>();
    EngineTiming engineTiming;
    engineTiming.ResetFrameTimer(*timer);

    MSG message{};
    bool running = true;
    while (running) {
        while (PeekMessageA(&message, nullptr, 0, 0, PM_REMOVE)) {
            if (message.message == WM_QUIT) {
                running = false;
                break;
            }
            TranslateMessage(&message);
            DispatchMessageA(&message);
        }
        if (!running)
            break;

        if (controllerDevices.DInputObj) {
            Controller_Update(controllerImage.data(), controllerDevices, input);
            if (Controller_GetCode(controllerImage.data()) == 1)
                break;
            if (Controller_GetCode(controllerImage.data()) == 0x3F)
                (void)CaptureScreenshotLikeExe(screen, directx, native, dataRoot);
        }

        const VideoMode* mode = directx.CurrentMode();
        if (!mode) {
            ShowPropsFxError("A display mode couldn't be initialized!");
            break;
        }

        native.ResetFrameStats();
        bool depthCleared = screen.ClearDepth(renderer, native, native);
        if (!depthCleared && native.FrontSurfaceLost()) {
            if (native.RestoreAllBuffers())
                depthCleared = screen.ClearDepth(renderer, native, native);
        }
        if (!depthCleared) {
            ShowOriginalFatal("Error in PropsFX-Engine\nClearZBuf() failed!",
                              "..\\propcore\\screen.cpp", 93);
            break;
        }

        UpdateFlyScene(engineTiming.FrameDelta());
        sceneError.clear();
        if (!RenderFlyWorld(mode->width, mode->height, renderer, native,
                            &sceneError)) {
            if (sceneError == "BEGINSCENE") {
                ShowOriginalFatal(
                    "Error in PropsFX-Engine!\nRendering of the scene couldn't be started! Did you press ALT-TAB? ;)",
                    "..\\propcore\\render.cpp", 116);
            } else if (sceneError == "ENDSCENE") {
                ShowOriginalFatal(
                    "Error in PropsFX-Engine!\nRendering of the scene couldn't be finished!",
                    "..\\propcore\\render.cpp", 160);
            } else {
                ShowPropsFxError(nullptr);
            }
            break;
        }
        if (controllerDevices.DInputObj)
            Controller_EndFrame(controllerImage.data());
        if (!FinishFlyFrame(mode->width, mode->height, textures, textureLoader,
                            renderer, native, &sceneError))
            break;
        (void)engineTiming.EndFrame(*timer, false);
        if (!screen.Present(native)) {
            ShowPropsFxError(nullptr);
            break;
        }
    }

    (void)gameDirectory->ChangeTo(std::string(kSoundsDirectory));
    (void)PlaySoundA(nullptr, nullptr, SND_FILENAME);

    CloseFlyScene();

    ShutdownPropsFxLikeExe(textures, directx, native);
    timer.reset();
    ShutdownControllerLikeExe(controllerImage.data(), controllerDevices, input);

    gameDirectory.reset();
    WriteLog("CrashdayDirectory object deleted");
    LifecycleSink = nullptr;
    return 1;
}
#endif
