#include "directx.hpp"
#include <algorithm>

namespace flydemo {

namespace {
std::uint32_t CountBitsLikeExe(std::uint32_t value) {
    std::uint32_t count = 0;
    while (value != 0) {
        count += value & 1u;
        value >>= 1u;
    }
    return count;
}
}

TextureFormatEnumAction CheckOpaqueFormat(
    const DDPixelFormat& candidate, DDPixelFormat& selected) {

    if (candidate.flags != 0x40u && candidate.flags != 0x60u)
        return TextureFormatEnumAction::Continue;
    if (candidate.rgbBitCount != 8u && candidate.rgbBitCount != 16u &&
        candidate.rgbBitCount != 24u && candidate.rgbBitCount != 32u)
        return TextureFormatEnumAction::Continue;

    const bool palette8 = (candidate.flags & 0x20u) != 0;
    if (!palette8 && candidate.rgbBitCount == 8u)
        return TextureFormatEnumAction::Continue;

    if (palette8) {
        selected = candidate;
        return TextureFormatEnumAction::Stop;
    }

    if (selected.rgbBitCount == 0u || candidate.rgbBitCount < selected.rgbBitCount)
        selected = candidate;
    return TextureFormatEnumAction::Continue;
}

TextureFormatEnumAction CheckAlphaFormat(
    const DDPixelFormat& candidate, DDPixelFormat& selected) {

    const std::uint32_t alphaBits = CountBitsLikeExe(candidate.alphaMask);
    const std::uint32_t redBits = CountBitsLikeExe(candidate.redMask);
    const std::uint32_t greenBits = CountBitsLikeExe(candidate.greenMask);
    const std::uint32_t blueBits = CountBitsLikeExe(candidate.blueMask);

    if (candidate.flags != 0x41u || redBits + greenBits + blueBits < 12u)
        return TextureFormatEnumAction::Continue;

    if (alphaBits == 6u && redBits == 6u && greenBits == 6u && blueBits == 6u) {
        selected = candidate;
        return TextureFormatEnumAction::Stop;
    }

    if (selected.rgbBitCount == 0u) {
        selected = candidate;
        return TextureFormatEnumAction::Continue;
    }

    const std::uint32_t oldAlphaBits = CountBitsLikeExe(selected.alphaMask);
    bool replace = false;
    if (alphaBits > oldAlphaBits) {
        if (alphaBits <= 6u || oldAlphaBits == 1u)
            replace = true;
    }
    if (!replace && alphaBits >= 4u && alphaBits <= 6u && oldAlphaBits > 6u)
        replace = true;

    if (replace)
        selected = candidate;
    return TextureFormatEnumAction::Continue;
}

TextureFormatEnumAction ConsiderZBufferFormat(
    const DDPixelFormat& candidate, DDPixelFormat& selected) {

    if (candidate.flags != 0x400u)
        return TextureFormatEnumAction::Continue;

    if (selected.rgbBitCount != 0u && candidate.rgbBitCount <= selected.rgbBitCount)
        return TextureFormatEnumAction::Continue;
    selected = candidate;
    return TextureFormatEnumAction::Continue;
}

bool ConsiderDisplayMode(const VideoMode& candidate, std::uint32_t greenMask,
                         std::vector<VideoMode>& accepted,
                         std::uint16_t& defaultMode) {

    if (accepted.size() >= 32u)
        return false;
    if (candidate.width < 512u || candidate.height < 384u ||
        candidate.bitsPerPixel < 16u || candidate.bitsPerPixel > 32u)
        return true;

    VideoMode mode = candidate;
    if (greenMask == 0x000003E0u)
        mode.flags = 5u;
    else if (greenMask == 0x000007E0u)
        mode.flags = 6u;
    else if (greenMask == 0x0000FF00u)
        mode.flags = 8u;
    else
        return true;

    const std::uint16_t newIndex = static_cast<std::uint16_t>(accepted.size());
    accepted.push_back(mode);

    const auto ideal = [](const VideoMode& m) {
        return m.width == 640u && m.height == 480u && m.bitsPerPixel == 16u;
    };
    if (defaultMode < accepted.size() - 1u && ideal(accepted[defaultMode]))
        return true;
    if (mode.width < 640u || mode.width > 800u ||
        mode.height < 480u || mode.height > 600u)
        return true;

    if (defaultMode >= accepted.size() - 1u) {
        defaultMode = newIndex;
        return true;
    }

    const VideoMode& old = accepted[defaultMode];
    if ((mode.width < old.width && mode.height < old.height) ||
        (mode.width == old.width && mode.height == old.height &&
         mode.bitsPerPixel < old.bitsPerPixel))
        defaultMode = newIndex;
    return true;
}

void DirectXState::Reset() {
    DDDriver.clear();
    CurrDDDriver = -1;
    CurrVMode = nullptr;
    PreferredVMode = 0;
    CurrScreenFormat = ScreenFormat::Normal;
    viewport_ = {};
    directDrawReady_ = false;
    direct3DReady_ = false;
    deviceReady_ = false;
    frontBufferReady_ = false;
    backBufferReady_ = false;
    viewportReady_ = false;
}

bool DirectXState::AddEnumeratedDriver(DDrawDriver driver) {

    if (DDDriver.size() >= MaxDDrawDrivers)
        return false;
    if (!driver.modes.empty() && driver.defaultMode >= driver.modes.size())
        driver.defaultMode = 0;
    if (driver.zBufferBitDepth == 0u)
        driver.zBufferBitDepth = driver.zBufferFormat.rgbBitCount;
    DDDriver.push_back(std::move(driver));
    return true;
}

bool DirectXState::SelectDriver(std::int32_t requestedIndex) {
    if (DDDriver.empty()) {
        CurrDDDriver = -1;
        CurrVMode = nullptr;
        PreferredVMode = 0;
        return false;
    }

    if (requestedIndex < 0 || static_cast<std::size_t>(requestedIndex) >= DDDriver.size())
        requestedIndex = 0;

    CurrDDDriver = static_cast<std::int16_t>(requestedIndex);
    CurrVMode = nullptr;
    const auto& driver = DDDriver[static_cast<std::size_t>(CurrDDDriver)];
    PreferredVMode = driver.modes.empty() ? 0 : static_cast<std::int16_t>(driver.defaultMode);
    return true;
}

bool DirectXState::StartSelectedDriver(DirectXIO& backend) {
    if (CurrDDDriver < 0 || static_cast<std::size_t>(CurrDDDriver) >= DDDriver.size())
        return false;
    if (directDrawReady_)
        return true;
    directDrawReady_ = backend.CreateDirectDraw(static_cast<std::uint16_t>(CurrDDDriver));
    return directDrawReady_;
}

bool DirectXState::SetPreferredMode(std::int32_t requestedIndex) {
    if (CurrDDDriver < 0 || static_cast<std::size_t>(CurrDDDriver) >= DDDriver.size())
        return false;
    const auto& modes = DDDriver[static_cast<std::size_t>(CurrDDDriver)].modes;
    if (modes.empty()) {
        PreferredVMode = 0;
        return false;
    }
    if (requestedIndex < 0 || static_cast<std::size_t>(requestedIndex) >= modes.size())
        requestedIndex = DDDriver[static_cast<std::size_t>(CurrDDDriver)].defaultMode;
    if (static_cast<std::size_t>(requestedIndex) >= modes.size())
        requestedIndex = 0;
    PreferredVMode = static_cast<std::int16_t>(requestedIndex);
    return true;
}

bool DirectXState::ActivatePreferredMode(DirectXIO& backend) {
    if (CurrDDDriver < 0 || static_cast<std::size_t>(CurrDDDriver) >= DDDriver.size())
        return false;
    const auto& modes = DDDriver[static_cast<std::size_t>(CurrDDDriver)].modes;
    if (modes.empty())
        return false;
    std::size_t i = PreferredVMode < 0 ? 0 : static_cast<std::size_t>(PreferredVMode);
    if (i >= modes.size())
        i = DDDriver[static_cast<std::size_t>(CurrDDDriver)].defaultMode;
    if (i >= modes.size())
        i = 0;
    PreferredVMode = static_cast<std::int16_t>(i);
    return ActivateMode(modes[i], backend);
}

bool DirectXState::ActivateMode(const VideoMode& requested, DirectXIO& backend) {
    if (!directDrawReady_ || CurrDDDriver < 0 ||
        static_cast<std::size_t>(CurrDDDriver) >= DDDriver.size())
        return false;

    auto& modes = DDDriver[static_cast<std::size_t>(CurrDDDriver)].modes;
    auto it = std::find_if(modes.begin(), modes.end(), [&](const VideoMode& m) {

        return m.width == requested.width && m.height == requested.height &&
               m.bitsPerPixel == requested.bitsPerPixel;
    });
    if (it == modes.end())
        return false;

    if (direct3DReady_ || deviceReady_ || viewportReady_)
        ShutdownDirect3D(backend);
    else if (frontBufferReady_ || backBufferReady_)
        DeleteBuffers(backend);

    if (!backend.SetDisplayMode(*it))
        return false;

    CurrVMode = &*it;
    PreferredVMode = static_cast<std::int16_t>(std::distance(modes.begin(), it));
    RebuildViewportFromMode();
    return true;
}

bool DirectXState::CreateBuffers(DirectXIO& backend) {
    if (!directDrawReady_ || CurrVMode == nullptr)
        return false;
    if (!backend.CreateFrontBackBuffers())
        return false;
    frontBufferReady_ = true;
    backBufferReady_ = true;
    return true;
}

bool DirectXState::RestoreAllBuffers(DirectXIO& backend) {
    if (!frontBufferReady_ || !backBufferReady_)
        return false;
    return backend.RestoreBuffers();
}

void DirectXState::DeleteBuffers(DirectXIO& backend) {
    if (frontBufferReady_ || backBufferReady_)
        backend.DeleteBuffers();
    frontBufferReady_ = false;
    backBufferReady_ = false;
}

bool DirectXState::StartDirect3D(DirectXIO& backend) {
    if (!directDrawReady_)
        return false;

    if (direct3DReady_ || deviceReady_ || viewportReady_)
        ShutdownDirect3D(backend);

    if (!frontBufferReady_ || !backBufferReady_) {
        if (!CreateBuffers(backend))
            return false;
    }

    if (!backend.CreateDirect3D())
        return false;
    direct3DReady_ = true;
    deviceReady_ = true;

    if (CurrDDDriver >= 0 &&
        static_cast<std::size_t>(CurrDDDriver) < DDDriver.size()) {
        auto& driver = DDDriver[static_cast<std::size_t>(CurrDDDriver)];
        std::vector<DDPixelFormat> formats;
        if (backend.EnumerateTextureFormats(formats)) {
            for (const DDPixelFormat& candidate : formats) {
                if (CheckOpaqueFormat(candidate, driver.opaqueTextureFormat) ==
                    TextureFormatEnumAction::Stop)
                    break;
            }
        }

        formats.clear();
        if (backend.EnumerateTextureFormats(formats)) {
            for (const DDPixelFormat& candidate : formats) {
                if (CheckAlphaFormat(candidate, driver.alphaTextureFormat) ==
                    TextureFormatEnumAction::Stop)
                    break;
            }
        }
    }

    RebuildViewportFromMode();
    if (!backend.CreateViewport(viewport_)) {
        backend.ReleaseDirect3DDevice();
        backend.ReleaseDirect3D();
        deviceReady_ = false;
        direct3DReady_ = false;
        return false;
    }
    viewportReady_ = true;
    return true;
}

void DirectXState::ShutdownDirect3D(DirectXIO& backend) {

    if (viewportReady_)
        backend.ReleaseViewport();
    viewportReady_ = false;
    if (deviceReady_)
        backend.ReleaseDirect3DDevice();
    deviceReady_ = false;
    if (direct3DReady_)
        backend.ReleaseDirect3D();
    direct3DReady_ = false;
    DeleteBuffers(backend);
}

void DirectXState::ShutdownDirectDraw(DirectXIO& backend) {
    if (directDrawReady_)
        backend.ReleaseDirectDraw();
    directDrawReady_ = false;
    CurrVMode = nullptr;
    CurrDDDriver = -1;
    PreferredVMode = 0;
    DDDriver.clear();
}

void DirectXState::Shutdown(DirectXIO& backend) {
    ShutdownDirect3D(backend);
    ShutdownDirectDraw(backend);
}

bool DirectXState::SetScreenFormat(ScreenFormat format, DirectXIO& backend) {
    if (!direct3DReady_ || !deviceReady_ || CurrVMode == nullptr)
        return false;
    if (format != ScreenFormat::Normal && format != ScreenFormat::Broad)
        return false;

    CurrScreenFormat = format;
    RebuildViewportFromMode();

    if (viewportReady_)
        backend.ReleaseViewport();
    viewportReady_ = false;
    if (!backend.CreateViewport(viewport_))
        return false;
    viewportReady_ = true;
    return true;
}

void DirectXState::RebuildViewportFromMode() {
    if (!CurrVMode) {
        viewport_ = {};
        return;
    }
    viewport_.x = 0;
    viewport_.y = 0;
    viewport_.width = CurrVMode->width;
    viewport_.height = CurrVMode->height;
    viewport_.clipX = -1.0f;
    viewport_.clipY = 1.0f;
    viewport_.clipWidth = 2.0f;
    viewport_.clipHeight = 2.0f;
    viewport_.minZ = 0.0f;
    viewport_.maxZ = 1.0f;
}

}
