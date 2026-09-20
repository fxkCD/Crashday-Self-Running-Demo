#pragma once

#include "cbm.hpp"
#include "control.hpp"
#include "directx.hpp"
#include "engine.hpp"
#include "render.hpp"
#include "screen.hpp"

#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>

namespace flydemo {

class Win32Graphics final : public DirectXIO,
                                   public RenderDevice,
                                   public ScreenIO,
                                   public CBMDirectDraw {
public:
    explicit Win32Graphics(std::uintptr_t windowHandle);
    ~Win32Graphics() override;

    Win32Graphics(const Win32Graphics&) = delete;
    Win32Graphics& operator=(const Win32Graphics&) = delete;

    bool EnumerateDDrawDrivers(DirectXState& state);
    const std::string& LastError() const;
    void SetTextureManager(CBMManager* textures);

    void ResetFrameStats();
    bool FrameHadNativeFailure() const;
    std::uint32_t FrameDrawCalls() const;
    std::uint32_t SuccessfulDrawCalls() const;
    std::string FrameStats() const;

    bool CreateDirectDraw(std::uint16_t driverIndex) override;
    bool SetDisplayMode(const VideoMode& mode) override;
    bool CreateFrontBackBuffers() override;
    bool RestoreBuffers() override;
    void DeleteBuffers() override;
    bool CreateDirect3D() override;
    bool EnumerateTextureFormats(std::vector<DDPixelFormat>& out) override;
    bool CreateViewport(const ViewportDesc& viewport) override;
    void ReleaseViewport() override;
    void ReleaseDirect3DDevice() override;
    void ReleaseDirect3D() override;
    void ReleaseDirectDraw() override;

    bool BeginScene() override;
    bool EndScene() override;
    void SetRenderState(std::uint32_t state, std::uint32_t value) override;
    void SetTextureStageState(std::uint32_t stage, std::uint32_t state,
                              std::uint32_t value) override;
    void SetLightState(std::uint32_t state, std::uint32_t value) override;
    void SetTexture(std::uint8_t textureIndex, std::uint8_t variant) override;
    bool TextureHasAlpha(std::uint8_t textureIndex) const override;
    void DrawPrimitive(std::uint32_t primitiveType, std::uint32_t vertexType,
                       const ImmediateVertex* vertices,
                       std::uint32_t vertexCount, std::uint32_t flags) override;
    void SetTransform(std::uint32_t state, const CD3DMATRIX& transform) override;
    std::uint32_t ComputeSphereVisibility(const CD3DVECTOR& center, float radius) override;

    bool FrontSurfaceLost() override;
    bool RestoreAllBuffers() override;
    bool BackBufferReadyForFlip() override;
    std::uint32_t FlipFrontBuffer() override;
    bool Clear(std::uint32_t flags, std::uint32_t color,
               float z, std::uint32_t stencil) override;
    bool LockFrontSurface(ScreenLockedSurface& out) override;
    bool UnlockFrontSurface() override;

    std::uint32_t CreatePalette(
        std::uint32_t flags,
        const std::array<std::uint8_t, 256 * 4>& rgba,
        CBMNativeHandle& outPalette) override;
    std::uint32_t CreateSurface(
        const CBMSurfaceDesc& desc, CBMNativeHandle& outSurface) override;
    std::uint32_t SetPalette(CBMNativeHandle surface,
                             CBMNativeHandle palette) override;
    std::uint32_t SetPaletteEntries(
        CBMNativeHandle palette, std::uint32_t flags, std::uint32_t start,
        std::uint32_t count,
        const std::array<std::uint8_t, 256 * 4>& rgba) override;
    std::uint32_t QueryTexture2(
        CBMNativeHandle surface, const std::array<std::uint8_t, 16>& iid,
        CBMNativeHandle& outTexture2) override;
    std::uint32_t Lock(CBMNativeHandle surface, std::uint32_t flags,
                       CBMLockedSurface& out) override;
    std::uint32_t Unlock(CBMNativeHandle surface) override;
    std::uint32_t GetAttachedMipSurface(
        CBMNativeHandle surface, std::uint32_t caps,
        CBMNativeHandle& outSurface) override;
    std::uint32_t Release(CBMNativeHandle handle) override;

    bool NativeDeviceReady() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

class Win32CBMLoader final : public CBMLoader {
public:
    Win32CBMLoader(std::filesystem::path texturesDirectory,
                          const DirectXState& directx,
                          Win32Graphics& backend,
                          std::uint8_t textureQuality = 0);
    std::unique_ptr<CBMPicture> LoadCBM(std::string_view canonicalName) override;
    const std::string& LastError() const { return lastError_; }

private:
    std::filesystem::path texturesDirectory_;
    const DirectXState& directx_;
    Win32Graphics& backend_;
    std::uint8_t textureQuality_ = 0;
    std::string lastError_;
};

class Win32Input final : public ControlInput {
public:
    Win32Input(std::uintptr_t instanceHandle,
                            std::uintptr_t windowHandle);
    ~Win32Input() override;

    Win32Input(const Win32Input&) = delete;
    Win32Input& operator=(const Win32Input&) = delete;

    bool CreateDirectInput(std::uint32_t version) override;
    void ReleaseDirectInput() override;
    bool CreateDevice(ControllerDevice device) override;
    bool SetDataFormat(ControllerDevice device) override;
    bool SetCooperativeLevel(ControllerDevice device, std::uint32_t flags) override;
    bool Acquire(ControllerDevice device) override;
    void Unacquire(ControllerDevice device) override;
    void ReleaseDevice(ControllerDevice device) override;
    bool EnumerateJoystick() override;
    bool QueryJoystickDevice2() override;
    void ReleaseJoystick() override;
    bool SetJoystickRange(ControllerAxis axis, std::int32_t minimum,
                          std::int32_t maximum) override;
    bool SetJoystickDeadZone(ControllerAxis axis, std::uint32_t value) override;
    bool PollJoystick() override;
    bool GetKeyboardState(std::uint8_t out[256]) override;
    bool GetMouseState(std::uint8_t out[16]) override;
    bool GetJoystickState(std::uint8_t out[0x50]) override;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

class Win32Timer final : public FrameTimer {
public:
    Win32Timer();
    ~Win32Timer() override;

    Win32Timer(const Win32Timer&) = delete;
    Win32Timer& operator=(const Win32Timer&) = delete;

    void ResetTimer() override;
    float ElapsedSeconds() override;

    std::uint32_t PeriodMilliseconds() const { return periodMilliseconds_; }

private:
    std::uint32_t resetTick_ = 0;
    std::uint32_t periodMilliseconds_ = 0;
    bool periodActive_ = false;
};

}
