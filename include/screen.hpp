#pragma once
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include "render.hpp"

namespace flydemo {

constexpr std::uint32_t DDERR_SURFACELOST_CODE = 0x8876021Cu;

class CDFileOperations;

struct ScreenLockedSurface {
    const std::uint8_t* pixels = nullptr;
    std::ptrdiff_t pitch = 0;
    std::uint32_t rgbBitCount = 0;
    std::uint32_t redMask = 0;
    std::uint32_t greenMask = 0;
    std::uint32_t blueMask = 0;
};

class ScreenIO {
public:
    virtual ~ScreenIO() = default;

    virtual bool FrontSurfaceLost() = 0;
    virtual bool RestoreAllBuffers() = 0;

    virtual bool BackBufferReadyForFlip() = 0;

    virtual std::uint32_t FlipFrontBuffer() = 0;

    virtual bool Clear(std::uint32_t flags, std::uint32_t color,
                       float z, std::uint32_t stencil) = 0;

    virtual bool LockFrontSurface(ScreenLockedSurface&) { return false; }
    virtual bool UnlockFrontSurface() { return false; }
};

class ScreenState {
public:
    void SetSceneInProgress(bool value) { sceneInProgress_ = value; }
    void SetFrontBufferAvailable(bool value) { frontBufferAvailable_ = value; }
    void SetBackBufferAvailable(bool value) { backBufferAvailable_ = value; }
    void SetViewportAvailable(bool value) { viewportAvailable_ = value; }

    bool Present(ScreenIO& backend, std::size_t maxReadyPolls = 4096);

    bool ClearColor(std::uint32_t color, ScreenIO& backend);
    bool ClearDepth(RendererState& renderer, RenderDevice& renderBackend,
                    ScreenIO& backend);

    static std::filesystem::path NextScreenshotPath(
        const std::filesystem::path& directory,
        const std::string& prefix = "shot",
        const std::string& extension = ".bmp");

    bool Screenshot(std::int32_t width,
                    std::int32_t height,
                    const std::filesystem::path& templateFile,
                    const std::filesystem::path& outputDirectory,
                    ScreenIO& backend,
                    CDFileOperations& files,
                    std::filesystem::path* writtenPath = nullptr,
                    std::string* error = nullptr);

    std::uint32_t LastResult() const { return lastResult_; }

private:
    bool sceneInProgress_ = false;
    bool frontBufferAvailable_ = false;
    bool backBufferAvailable_ = false;
    bool viewportAvailable_ = false;
    std::uint32_t lastResult_ = 0;
};

}
