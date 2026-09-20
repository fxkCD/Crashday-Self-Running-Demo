#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace flydemo {

struct VideoMode {
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::uint32_t bitsPerPixel = 0;
    std::uint32_t flags = 0;

    bool operator==(const VideoMode& o) const {
        return width == o.width && height == o.height &&
               bitsPerPixel == o.bitsPerPixel && flags == o.flags;
    }
};

struct DDPixelFormat {
    std::uint32_t size = 0;
    std::uint32_t flags = 0;
    std::uint32_t fourCC = 0;
    std::uint32_t rgbBitCount = 0;
    std::uint32_t redMask = 0;
    std::uint32_t greenMask = 0;
    std::uint32_t blueMask = 0;
    std::uint32_t alphaMask = 0;

    bool operator==(const DDPixelFormat& o) const {
        return size == o.size && flags == o.flags && fourCC == o.fourCC &&
               rgbBitCount == o.rgbBitCount && redMask == o.redMask &&
               greenMask == o.greenMask && blueMask == o.blueMask &&
               alphaMask == o.alphaMask;
    }
};

enum class TextureFormatEnumAction : std::uint8_t {
    Stop = 0,
    Continue = 1,
};

TextureFormatEnumAction CheckOpaqueFormat(
    const DDPixelFormat& candidate, DDPixelFormat& selected);

TextureFormatEnumAction CheckAlphaFormat(
    const DDPixelFormat& candidate, DDPixelFormat& selected);

TextureFormatEnumAction ConsiderZBufferFormat(
    const DDPixelFormat& candidate, DDPixelFormat& selected);

bool ConsiderDisplayMode(const VideoMode& candidate, std::uint32_t greenMask,
                         std::vector<VideoMode>& accepted,
                         std::uint16_t& defaultMode);

struct DDrawDriver {
    std::string name;
    std::vector<VideoMode> modes;
    std::uint16_t defaultMode = 0;
    std::uint32_t zBufferBitDepth = 0;
    DDPixelFormat zBufferFormat{};
    DDPixelFormat opaqueTextureFormat{};
    DDPixelFormat alphaTextureFormat{};
    std::string description;
};

enum class ScreenFormat : std::uint8_t {
    Normal = 0,
    Broad = 1,
};

struct ViewportDesc {
    std::uint32_t x = 0;
    std::uint32_t y = 0;
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    float clipX = -1.0f;
    float clipY = 1.0f;
    float clipWidth = 2.0f;
    float clipHeight = 2.0f;
    float minZ = 0.0f;
    float maxZ = 1.0f;
};

class DirectXIO {
public:
    virtual ~DirectXIO() = default;
    virtual bool CreateDirectDraw(std::uint16_t driverIndex) = 0;
    virtual bool SetDisplayMode(const VideoMode& mode) = 0;
    virtual bool CreateFrontBackBuffers() = 0;
    virtual bool RestoreBuffers() = 0;
    virtual void DeleteBuffers() = 0;
    virtual bool CreateDirect3D() = 0;

    virtual bool EnumerateTextureFormats(std::vector<DDPixelFormat>& out) = 0;
    virtual bool CreateViewport(const ViewportDesc& viewport) = 0;
    virtual void ReleaseViewport() = 0;
    virtual void ReleaseDirect3DDevice() = 0;
    virtual void ReleaseDirect3D() = 0;
    virtual void ReleaseDirectDraw() = 0;
};

class DirectXState {
public:

    void Reset();
    bool AddEnumeratedDriver(DDrawDriver driver);

    bool SelectDriver(std::int32_t requestedIndex);
    bool StartSelectedDriver(DirectXIO& backend);

    bool SetPreferredMode(std::int32_t requestedIndex);
    bool ActivatePreferredMode(DirectXIO& backend);
    bool ActivateMode(const VideoMode& requested, DirectXIO& backend);

    bool StartDirect3D(DirectXIO& backend);
    void ShutdownDirect3D(DirectXIO& backend);
    void ShutdownDirectDraw(DirectXIO& backend);
    void Shutdown(DirectXIO& backend);

    bool CreateBuffers(DirectXIO& backend);
    bool RestoreAllBuffers(DirectXIO& backend);
    void DeleteBuffers(DirectXIO& backend);
    bool SetScreenFormat(ScreenFormat format, DirectXIO& backend);

    static constexpr std::size_t MaxDDrawDrivers = 5;

    const std::vector<DDrawDriver>& Drivers() const { return DDDriver; }
    const DDrawDriver* CurrentDriver() const {
        return CurrDDDriver >= 0 && static_cast<std::size_t>(CurrDDDriver) < DDDriver.size()
            ? &DDDriver[static_cast<std::size_t>(CurrDDDriver)] : nullptr;
    }
    std::int16_t CurrentDriverIndex() const { return CurrDDDriver; }
    std::int16_t PreferredModeIndex() const { return PreferredVMode; }
    const VideoMode* CurrentMode() const { return CurrVMode; }
    ScreenFormat CurrentScreenFormat() const { return CurrScreenFormat; }
    const ViewportDesc& Viewport() const { return viewport_; }

    bool DirectDrawReady() const { return directDrawReady_; }
    bool Direct3DReady() const { return direct3DReady_; }
    bool DeviceReady() const { return deviceReady_; }
    bool FrontBufferReady() const { return frontBufferReady_; }
    bool BackBufferReady() const { return backBufferReady_; }
    bool ViewportReady() const { return viewportReady_; }

private:
    void RebuildViewportFromMode();

    std::vector<DDrawDriver> DDDriver;
    std::int16_t CurrDDDriver = -1;
    const VideoMode* CurrVMode = nullptr;
    std::int16_t PreferredVMode = 0;
    ScreenFormat CurrScreenFormat = ScreenFormat::Normal;
    ViewportDesc viewport_{};

    bool directDrawReady_ = false;
    bool direct3DReady_ = false;
    bool deviceReady_ = false;
    bool frontBufferReady_ = false;
    bool backBufferReady_ = false;
    bool viewportReady_ = false;
};

}
