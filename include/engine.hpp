#pragma once

#include "cdfileop.hpp"
#include "directx.hpp"
#include "options.hpp"
#include "lighting.hpp"
#include "path.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

namespace flydemo {

struct EnvironmentState {
    float value0 = 0.0f;
    float value4 = 0.0f;
    std::uint32_t packedColor8 = 0;
};
static_assert(sizeof(EnvironmentState) == 0x0C,
              "engine.cpp copies this tail as exactly 12 bytes");

class EngineEnvIO {
public:
    virtual ~EngineEnvIO() = default;
    virtual void DeleteTexture(const std::string& filename) = 0;
    virtual std::uint8_t LoadTexture(const std::string& filename) = 0;
    virtual void ActivateRenderSettings() = 0;
};

struct EngineEnvironmentState {
    std::uint32_t ambientColor = 0;
    CD3DLIGHT light{};
    EnvironmentState tail{};
    std::string skyResourceBase;
    std::array<std::uint8_t, 3> skyTextureSlots{};
    std::string environmentTextureName;
    std::uint8_t environmentTextureSlot = 0;
};

class EngineEnvironment {
public:
    const EngineEnvironmentState& State() const { return state_; }
    std::uint32_t AmbientColor() const { return state_.ambientColor; }
    unsigned SceneProgress() const { return sceneProgress_; }

    void ApplyEnvironment(std::uint32_t ambientColor,
                          CD3DLIGHT& light,
                          const EnvironmentState& tail,
                          std::string_view resourceBase,
                          bool sceneInProgress,
                          EngineEnvIO& backend);

    void ApplyDefault(bool sceneInProgress,
                      EngineEnvIO& backend);

    void SetupSkyBox(std::string_view resourceBase,
                     bool sceneInProgress,
                     EngineEnvIO& backend);

private:
    EngineEnvironmentState state_{};
    unsigned sceneProgress_ = 0;
};

class FrameTimer {
public:
    virtual ~FrameTimer() = default;

    virtual void ResetTimer() = 0;
    virtual float ElapsedSeconds() = 0;
};

class EngineTiming {
public:
    void ResetFrameTimer(FrameTimer& timer);
    bool EndFrame(FrameTimer& timer, bool sceneInProgress);

    float FrameDelta() const { return frameDelta_; }
    std::uint16_t FramesPerSecond() const { return fps_; }
    float AccumulatedSeconds() const { return accumulatedSeconds_; }
    std::uint16_t AccumulatedFrames() const { return accumulatedFrames_; }

private:
    float frameDelta_ = 0.0f;
    std::uint16_t fps_ = 0;
    float accumulatedSeconds_ = 0.0f;
    std::uint16_t accumulatedFrames_ = 0;
};

class EngineSettings {
public:
    static constexpr const char* Filename = "propsfx.cfg";
    static constexpr const char* RootDirectory = "ROOT";
    static constexpr std::size_t SerializedSize = 0x14;
    static constexpr std::size_t RenderOptionsImageSize = 0x10;

    enum class LoadResult : std::uint8_t {
        Loaded,
        Missing,
        CreatedFor3DSetup,
        OpenFailed,
    };

    LoadResult Load(std::uint8_t startupMode,
                    CrashdayDirectory& directory,
                    CDFileOperations& file,
                    DirectXState& directX,
                    RenderOptionsState& options);

    bool Save(CrashdayDirectory& directory,
              CDFileOperations& file,
              const DirectXState& directX,
              const RenderOptionsState& options);

    const std::array<std::uint8_t, RenderOptionsImageSize>& RenderOptionsImage() const {
        return optionImage_;
    }

private:
    void SyncKnownOptionsToImage(const RenderOptionsState& options);
    void ApplyOptionsImage(RenderOptionsState& options) const;

    std::array<std::uint8_t, RenderOptionsImageSize> optionImage_{};
};

class EngineLifecycleHooks {
public:
    virtual ~EngineLifecycleHooks() = default;

    virtual void EnumerateDDrawDrivers(DirectXState& directX) = 0;

    virtual void ResetCBMManager() = 0;
    virtual void ResetRendererState() = 0;

    virtual void ApplyDefaultEnvironment() = 0;

    virtual void ActivateRenderSettings() = 0;
};

struct EngineLifecycleState {
    bool windowWasNull = false;
    bool startupModeInvalid = false;
    bool initialised = false;
    std::uint8_t startupMode = 0;
    EngineSettings::LoadResult settingsResult = EngineSettings::LoadResult::Missing;
};

class EngineLifecycle {
public:
    const EngineLifecycleState& State() const { return state_; }
    EngineSettings& Settings() { return settings_; }
    const EngineSettings& Settings() const { return settings_; }

    void Initialise(bool hwndPresent,
                    std::uint8_t startupMode,
                    CrashdayDirectory& directory,
                    CDFileOperations& file,
                    DirectXState& directX,
                    RenderOptionsState& options,
                    DirectXIO& directXBackend,
                    EngineLifecycleHooks& hooks);

    void Shutdown(DirectXState& directX,
                  DirectXIO& directXBackend);

private:
    EngineLifecycleState state_{};
    EngineSettings settings_{};
};

}
