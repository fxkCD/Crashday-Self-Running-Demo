#include "engine.hpp"

#include <cstring>
#include <filesystem>
#include <cmath>
#include <limits>
#include <string>

namespace flydemo {
namespace {

constexpr std::size_t kTextureQuality = 0x00;
constexpr std::size_t kRange = 0x04;
constexpr std::size_t kParticles = 0x08;
constexpr std::size_t kFlag4D = 0x09;
constexpr std::size_t kFlag4E = 0x0A;
constexpr std::size_t kFlag4F = 0x0B;
constexpr std::size_t kFlag50 = 0x0C;
constexpr std::size_t kFlag51 = 0x0D;
constexpr std::size_t kFlag52 = 0x0E;

static_assert(sizeof(float) == 4, "Crashday 2001 config stores 32-bit IEEE float bytes");
static_assert(sizeof(std::int16_t) == 2, "Crashday 2001 config stores 16-bit signed indices");

std::uint8_t BoolByte(bool v) {
    return v ? 1u : 0u;
}

}

void EngineEnvironment::SetupSkyBox(std::string_view resourceBase,
                                             bool sceneInProgress,
                                             EngineEnvIO& backend) {

    if (resourceBase == state_.skyResourceBase)
        return;

    if (sceneInProgress)
        ++sceneProgress_;

    backend.DeleteTexture(state_.skyResourceBase + "t.cbm");
    backend.DeleteTexture(state_.skyResourceBase + "b.cbm");
    backend.DeleteTexture(state_.skyResourceBase + "s.cbm");

    state_.skyResourceBase.assign(resourceBase.data(), resourceBase.size());

    state_.skyTextureSlots[0] = backend.LoadTexture(state_.skyResourceBase + "t.cbm");
    state_.skyTextureSlots[1] = backend.LoadTexture(state_.skyResourceBase + "b.cbm");
    state_.skyTextureSlots[2] = backend.LoadTexture(state_.skyResourceBase + "s.cbm");
}

void EngineEnvironment::ApplyEnvironment(std::uint32_t ambientColor,
                                                CD3DLIGHT& light,
                                                const EnvironmentState& tail,
                                                std::string_view resourceBase,
                                                bool sceneInProgress,
                                                EngineEnvIO& backend) {

    if (sceneInProgress)
        ++sceneProgress_;

    const long double x = static_cast<long double>(light.position.x);
    const long double y = static_cast<long double>(light.position.y);
    const long double z = static_cast<long double>(light.position.z);
    const long double length = std::sqrt(x * x + y * y + z * z);
    light.cachedVectorLength = static_cast<float>(length);
    const long double inv = 1.0L / length;
    light.position.x = static_cast<float>(x * inv);
    light.position.y = static_cast<float>(y * inv);
    light.position.z = static_cast<float>(z * inv);

    state_.ambientColor = ambientColor;
    state_.light = light;

    const long double sx = static_cast<long double>(state_.light.position.x);
    const long double sy = static_cast<long double>(state_.light.position.y);
    const long double sz = static_cast<long double>(state_.light.position.z);
    state_.light.cachedVectorLength = static_cast<float>(
        std::sqrt(sx * sx + sy * sy + sz * sz));
    state_.tail = tail;

    SetupSkyBox(resourceBase, sceneInProgress, backend);

    const std::string wanted(resourceBase.data(), resourceBase.size());
    const std::string environmentName = wanted + "e.cbm";
    if (environmentName != state_.environmentTextureName) {

        backend.DeleteTexture(state_.environmentTextureName);
        state_.environmentTextureName = environmentName;
        state_.environmentTextureSlot = backend.LoadTexture(environmentName);
    }

    backend.ActivateRenderSettings();
}

void EngineEnvironment::ApplyDefault(bool sceneInProgress,
                                            EngineEnvIO& backend) {

    CD3DLIGHT light{};
    light.position = {-0.2f, -0.6f, 0.5f};
    light.range = 4294967296.0f;
    light.packedColor = 0x00FFFFFFu;
    light.coronas = false;
    light.lensFlares = false;
    light.scale = 1.0f;
    light.lightUp = true;

    EnvironmentState tail{};
    tail.value0 = 0.0f;
    tail.value4 = 0.2f;
    tail.packedColor8 = 0x0096AAB4u;

    ApplyEnvironment(0x00808080u, light, tail, "ambience/default",
                     sceneInProgress, backend);
}

void EngineSettings::SyncKnownOptionsToImage(const RenderOptionsState& options) {
    optionImage_[kTextureQuality] = static_cast<std::uint8_t>(options.textureQuality);
    std::memcpy(optionImage_.data() + kRange, &options.range, sizeof(options.range));
    optionImage_[kParticles] = static_cast<std::uint8_t>(options.particles);
    optionImage_[kFlag4D] = BoolByte(options.flag4D);
    optionImage_[kFlag4E] = BoolByte(options.flag4E);
    optionImage_[kFlag4F] = BoolByte(options.flag4F);
    optionImage_[kFlag50] = BoolByte(options.flag50);
    optionImage_[kFlag51] = BoolByte(options.flag51);
    optionImage_[kFlag52] = BoolByte(options.flag52);

}

void EngineSettings::ApplyOptionsImage(RenderOptionsState& options) const {
    options.textureQuality = static_cast<TextureQuality>(optionImage_[kTextureQuality]);
    std::memcpy(&options.range, optionImage_.data() + kRange, sizeof(options.range));
    options.particles = static_cast<ParticleUse>(optionImage_[kParticles]);
    options.flag4D = optionImage_[kFlag4D] != 0u;
    options.flag4E = optionImage_[kFlag4E] != 0u;
    options.flag4F = optionImage_[kFlag4F] != 0u;
    options.flag50 = optionImage_[kFlag50] != 0u;
    options.flag51 = optionImage_[kFlag51] != 0u;
    options.flag52 = optionImage_[kFlag52] != 0u;
}

EngineSettings::LoadResult EngineSettings::Load(
    std::uint8_t startupMode,
    CrashdayDirectory& directory,
    CDFileOperations& file,
    DirectXState& directX,
    RenderOptionsState& options) {

    (void)directory.ChangeTo(RootDirectory);

    std::error_code ec;
    const bool exists = std::filesystem::exists(Filename, ec) && !ec;
    if (!exists) {

        if (startupMode == 1u) {
            (void)directX.SelectDriver(0);
            (void)Save(directory, file, directX, options);
            return LoadResult::CreatedFor3DSetup;
        }
        return LoadResult::Missing;
    }

    const int openResult = file.Open(Filename, "r+", 0);

    std::int16_t driver = directX.CurrentDriverIndex();
    (void)file.Read(&driver, 2, 1);
    (void)directX.SelectDriver(driver);

    std::int16_t preferredMode = directX.PreferredModeIndex();
    (void)file.Read(&preferredMode, 2, 1);
    (void)directX.SetPreferredMode(preferredMode);

    SyncKnownOptionsToImage(options);
    (void)file.Read(optionImage_.data(), RenderOptionsImageSize, 1);
    ApplyOptionsImage(options);

    (void)file.Close();
    return openResult == 0 ? LoadResult::Loaded : LoadResult::OpenFailed;
}

bool EngineSettings::Save(CrashdayDirectory& directory,
                                CDFileOperations& file,
                                const DirectXState& directX,
                                const RenderOptionsState& options) {

    (void)directory.ChangeTo(RootDirectory);

    const int openResult = file.Open(Filename, "w+", 0);

    std::int16_t driver = directX.CurrentDriverIndex();
    std::int16_t preferredMode = directX.PreferredModeIndex();
    SyncKnownOptionsToImage(options);

    const int w0 = file.Write(&driver, 2, 1);
    const int w1 = file.Write(&preferredMode, 2, 1);
    const int w2 = file.Write(optionImage_.data(), RenderOptionsImageSize, 1);
    const int closeResult = file.Close();
    return openResult == 0 && w0 == 0 && w1 == 0 && w2 == 0 && closeResult == 0;
}

void EngineLifecycle::Initialise(bool hwndPresent,
                                       std::uint8_t startupMode,
                                       CrashdayDirectory& directory,
                                       CDFileOperations& file,
                                       DirectXState& directX,
                                       RenderOptionsState& options,
                                       DirectXIO& directXBackend,
                                       EngineLifecycleHooks& hooks) {
    state_ = {};
    state_.windowWasNull = !hwndPresent;
    state_.startupModeInvalid = startupMode != 0u && startupMode != 1u;
    state_.startupMode = startupMode;

    directX.Reset();
    hooks.EnumerateDDrawDrivers(directX);
    options = RenderOptionsState{};
    hooks.ResetCBMManager();
    hooks.ResetRendererState();

    state_.settingsResult = settings_.Load(startupMode, directory, file, directX, options);

    if (startupMode == 0u) {
        (void)directX.StartSelectedDriver(directXBackend);
        (void)directX.ActivatePreferredMode(directXBackend);
        hooks.ApplyDefaultEnvironment();
        hooks.ActivateRenderSettings();
    }

    state_.initialised = true;
}

void EngineLifecycle::Shutdown(DirectXState& directX,
                                     DirectXIO& directXBackend) {

    directX.Shutdown(directXBackend);
    state_.initialised = false;
}

void EngineTiming::ResetFrameTimer(FrameTimer& timer) {

    frameDelta_ = 0.0f;
    timer.ResetTimer();
}

bool EngineTiming::EndFrame(FrameTimer& timer, bool sceneInProgress) {

    if (sceneInProgress)
        return false;

    frameDelta_ = timer.ElapsedSeconds();
    timer.ResetTimer();

    accumulatedSeconds_ += frameDelta_;
    ++accumulatedFrames_;

    if (accumulatedSeconds_ > 3.0f) {

        fps_ = static_cast<std::uint16_t>(accumulatedFrames_ / 3u);
        accumulatedFrames_ = 0;
        accumulatedSeconds_ = 0.0f;
    }
    return true;
}

}
