#include "screen.hpp"
#include "cdfileop.hpp"
#include <array>
#include <cstring>
#include <limits>
#include <system_error>

namespace flydemo {

bool ScreenState::Present(ScreenIO& backend, std::size_t maxReadyPolls) {
    if (sceneInProgress_ || !frontBufferAvailable_ || !backBufferAvailable_)
        return false;

    if (backend.FrontSurfaceLost()) {
        if (!backend.RestoreAllBuffers())
            return false;
    }

    std::size_t polls = 0;
    while (!backend.BackBufferReadyForFlip()) {
        if (++polls >= maxReadyPolls)
            return false;
    }

    lastResult_ = backend.FlipFrontBuffer();

    return lastResult_ == 0;
}

bool ScreenState::ClearColor(std::uint32_t color, ScreenIO& backend) {
    if (!viewportAvailable_ || sceneInProgress_)
        return false;

    return backend.Clear(1u, color, 0.0f, 0u);
}

bool ScreenState::ClearDepth(RendererState& renderer,
                                   RenderDevice& renderBackend,
                                   ScreenIO& backend) {
    if (!viewportAvailable_ || sceneInProgress_)
        return false;

    renderer.SetZWrite(true, renderBackend);

    return backend.Clear(2u, 0u, 1.0f, 0u);
}

std::filesystem::path ScreenState::NextScreenshotPath(
    const std::filesystem::path& directory,
    const std::string& prefix,
    const std::string& extension) {

    std::error_code ec;
    for (std::uint32_t i = 0; ; ++i) {
        auto candidate = directory / (prefix + std::to_string(i) + extension);
        const bool exists = std::filesystem::exists(candidate, ec);
        if (ec || !exists)
            return candidate;
    }
}

namespace {

std::uint32_t BitCount(std::uint32_t value) {

    std::uint32_t count = 0;
    while (value != 0) {
        count += value & 1u;
        value >>= 1u;
    }
    return count;
}

void StoreLE32(std::uint8_t* dst, std::uint32_t value) {
    dst[0] = static_cast<std::uint8_t>(value);
    dst[1] = static_cast<std::uint8_t>(value >> 8u);
    dst[2] = static_cast<std::uint8_t>(value >> 16u);
    dst[3] = static_cast<std::uint8_t>(value >> 24u);
}

std::uint32_t LoadPixelLittleEndian(const std::uint8_t* src,
                                    std::uint32_t bytesPerPixel) {

    std::uint32_t value = 0;
    const std::uint32_t n = bytesPerPixel < 4u ? bytesPerPixel : 4u;
    for (std::uint32_t i = 0; i < n; ++i)
        value |= static_cast<std::uint32_t>(src[i]) << (i * 8u);
    return value;
}

}

bool ScreenState::Screenshot(std::int32_t width,
                                   std::int32_t height,
                                   const std::filesystem::path& templateFile,
                                   const std::filesystem::path& outputDirectory,
                                   ScreenIO& backend,
                                   CDFileOperations& files,
                                   std::filesystem::path* writtenPath,
                                   std::string* error) {
    auto fail = [&](const char* text, bool closeFile = false) {
        if (closeFile && files.IsOpen())
            (void)files.Close();
        if (error)
            *error = text;
        return false;
    };

    if (!frontBufferAvailable_ || !backBufferAvailable_ || width <= 0 || height <= 0)
        return fail("screen state is not ready for screenshot");
    if (files.IsOpen())
        return fail("CDFileOperations already has an open file");

    std::array<std::uint8_t, 54> header{};
    const std::string templateName = templateFile.string();
    if (files.Open(templateName.c_str(), "r+", 0) != 0)
        return fail("scrnshot.bmt could not be opened");
    (void)files.Read(header.data(), 1, header.size());
    (void)files.Close();

    header[0] = 0x42;
    header[1] = 0x4D;
    StoreLE32(header.data() + 18, static_cast<std::uint32_t>(width));
    StoreLE32(header.data() + 22, static_cast<std::uint32_t>(height));

    const std::filesystem::path outputPath = NextScreenshotPath(outputDirectory);
    const std::string outputName = outputPath.string();
    if (files.Open(outputName.c_str(), "w+", 0) != 0)
        return fail("screenshot output could not be opened");
    (void)files.Write(header.data(), 1, header.size());

    ScreenLockedSurface surface{};
    if (!backend.LockFrontSurface(surface))
        return fail("Error while locking the frame buffer for screenshot!", true);

    const std::uint32_t rBits = BitCount(surface.redMask);
    const std::uint32_t gBits = BitCount(surface.greenMask);
    const std::uint32_t bBits = BitCount(surface.blueMask);
    const std::uint32_t bytesPerPixel = surface.rgbBitCount >> 3u;

    if (!surface.pixels || bytesPerPixel == 0u || bytesPerPixel > 4u ||
        rBits > 8u || gBits > 8u || bBits > 8u ||
        rBits + gBits + bBits > 32u) {
        (void)backend.UnlockFrontSurface();
        return fail("unsupported screenshot framebuffer format", true);
    }

    const std::uint32_t rUp = 8u - rBits;
    const std::uint32_t gUp = 8u - gBits;
    const std::uint32_t bUp = 8u - bBits;
    const std::uint32_t rShift = gBits + bBits;
    const std::uint32_t gShift = bBits;

    for (std::int32_t y = height - 1; y >= 0; --y) {
        const std::uint8_t* src = surface.pixels +
            static_cast<std::ptrdiff_t>(y) * surface.pitch;
        for (std::int32_t x = 0; x < width; ++x) {
            const std::uint32_t pixel = LoadPixelLittleEndian(src, bytesPerPixel);
            const std::uint32_t r = ((pixel >> rShift) << rUp) & 0xFFu;
            const std::uint32_t g = ((pixel >> gShift) << gUp) & 0xFFu;
            const std::uint32_t b = (pixel << bUp) & 0xFFu;

            const std::array<std::uint8_t, 3> bgr{
                static_cast<std::uint8_t>(b),
                static_cast<std::uint8_t>(g),
                static_cast<std::uint8_t>(r)};
            (void)files.Write(bgr.data(), 3, 1);
            src += bytesPerPixel;
        }
    }

    if (!backend.UnlockFrontSurface())
        return fail("Error while unlocking the frame buffer for screenshot!", true);

    (void)files.Close();
    if (writtenPath)
        *writtenPath = outputPath;
    if (error)
        error->clear();
    return true;
}

}
