#include "cbm.hpp"
#include <algorithm>
#include <cstdint>
#include <limits>

namespace flydemo {
namespace {
std::uint32_t TruncatePositive(double value) {
    if (!(value > 0.0))
        return 0;
    if (value >= static_cast<double>(std::numeric_limits<std::uint32_t>::max()))
        return std::numeric_limits<std::uint32_t>::max();

    return static_cast<std::uint32_t>(value);
}

bool Fail(std::string* error, const char* message) {
    if (error)
        *error = message;
    return false;
}

class Reader {
public:
    explicit Reader(const std::vector<std::uint8_t>& bytes) : bytes_(bytes) {}

    bool Byte(std::uint8_t& out) {
        if (pos_ >= bytes_.size())
            return false;
        out = bytes_[pos_++];
        return true;
    }

    bool U16(std::uint16_t& out) {
        if (bytes_.size() - pos_ < 2)
            return false;
        out = static_cast<std::uint16_t>(bytes_[pos_]) |
              static_cast<std::uint16_t>(bytes_[pos_ + 1] << 8);
        pos_ += 2;
        return true;
    }

    bool Tag3(char a, char b, char c, std::uint8_t* fourth = nullptr) {
        if (bytes_.size() - pos_ < (fourth ? 4u : 3u))
            return false;
        if (bytes_[pos_] != static_cast<std::uint8_t>(a) ||
            bytes_[pos_ + 1] != static_cast<std::uint8_t>(b) ||
            bytes_[pos_ + 2] != static_cast<std::uint8_t>(c))
            return false;
        pos_ += 3;
        if (fourth)
            *fourth = bytes_[pos_++];
        return true;
    }

    bool Block(std::uint8_t* dst, std::size_t n) {
        if (n > bytes_.size() - pos_)
            return false;
        std::copy_n(bytes_.data() + pos_, n, dst);
        pos_ += n;
        return true;
    }

    bool Vector(std::vector<std::uint8_t>& dst, std::size_t n) {
        if (n > bytes_.size() - pos_)
            return false;
        dst.assign(bytes_.begin() + static_cast<std::ptrdiff_t>(pos_),
                   bytes_.begin() + static_cast<std::ptrdiff_t>(pos_ + n));
        pos_ += n;
        return true;
    }

    std::size_t Position() const { return pos_; }

private:
    const std::vector<std::uint8_t>& bytes_;
    std::size_t pos_ = 0;
};

bool IsTextureDimension(std::uint16_t n) {
    switch (n) {
    case 4: case 8: case 16: case 32: case 64: case 128: case 256:
        return true;
    default:
        return false;
    }
}
}

bool ParseCBMIndexedFile(const std::vector<std::uint8_t>& bytes,
                         CBMIndexedFile& out,
                         std::string* error) {
    out = {};
    Reader r(bytes);

    if (!r.Tag3('C', 'B', 'M'))
        return Fail(error, "Wrong CBM file format");
    if (!r.Byte(out.header.version))
        return Fail(error, "Truncated CBM version");
    if (out.header.version != 1)
        return Fail(error, "Wrong CBM version");

    std::uint8_t type0 = 0, type1 = 0, type2 = 0;
    if (!r.Byte(type0) || !r.Byte(type1) || !r.Byte(type2))
        return Fail(error, "Truncated CBM type");
    if (type0 == 'P' && type1 == 'I' && type2 == 'C') {
        out.header.kind = CBMKind::Picture;
    } else if (type0 == 'T' && type1 == 'E' && type2 == 'X') {
        out.header.kind = CBMKind::Texture;
    } else {
        return Fail(error, "Wrong CBM file format");
    }

    if (!r.Byte(out.header.count) || !r.U16(out.header.width) || !r.U16(out.header.height))
        return Fail(error, "Truncated CBM dimensions");

    if (out.header.kind == CBMKind::Texture && out.header.count != 5)
        return Fail(error, "TEX CBM does not contain 5 mip levels");

    std::uint8_t alpha = 0;
    if (!r.Byte(out.header.field314) || !r.Byte(alpha) || !r.Byte(out.header.alphaField))
        return Fail(error, "Truncated CBM metadata");
    out.header.hasAlpha = alpha != 0;

    if (!r.Tag3('P', 'A', 'L'))
        return Fail(error, "Couldn't find palette signature PAL");
    if (!r.Block(out.palette6.data(), out.palette6.size()))
        return Fail(error, "Truncated CBM palette");
    for (std::size_t i = 0; i < out.palette6.size(); ++i)
        out.palette8[i] = static_cast<std::uint8_t>(out.palette6[i] << 2);

    if (out.header.kind == CBMKind::Texture &&
        (!IsTextureDimension(out.header.width) || !IsTextureDimension(out.header.height))) {
        return Fail(error, "Invalid TEX dimensions");
    }

    const std::size_t chunkCount = out.header.count;
    out.chunks.reserve(chunkCount);
    for (std::size_t i = 0; i < chunkCount; ++i) {
        CBMIndexedChunk chunk;
        const bool pic = out.header.kind == CBMKind::Picture;
        if (!r.Tag3(pic ? 'P' : 'T', pic ? 'I' : 'E', pic ? 'C' : 'X', &chunk.marker))
            return Fail(error, pic ? "PICx signature not found" : "TEXx signature not found");

        if (pic) {
            chunk.width = out.header.width;
            chunk.height = out.header.height;
        } else {

            chunk.width = static_cast<std::uint16_t>(out.header.width >> i);
            chunk.height = static_cast<std::uint16_t>(out.header.height >> i);
        }

        const std::size_t pixels = static_cast<std::size_t>(chunk.width) * chunk.height;
        if (!r.Vector(chunk.indices, pixels))
            return Fail(error, "Truncated CBM indexed pixel data");
        out.chunks.push_back(std::move(chunk));
    }

    out.consumedBytes = r.Position();
    if (error)
        error->clear();
    return true;
}

bool MakeCBMHeaderState(const CBMIndexedFile& file,
                        std::uint8_t quality,
                        std::uint32_t targetPixelBits,
                        CBMHeaderState& out,
                        std::string* error) {
    if (quality > 2)
        return Fail(error, "Unsupported TEX quality");

    out = {};
    out.kind = file.header.kind;
    out.pixelBits = targetPixelBits;
    out.colorKey16 = file.header.field314;
    out.hasAlpha = file.header.hasAlpha;
    out.alphaField = file.header.alphaField;

    if (file.header.kind == CBMKind::Picture) {
        out.frameCount = file.header.count;
        out.mipLayout = 0;
        out.field03 = 0;
        out.width = file.header.width;
        out.height = file.header.height;
    } else {
        if (file.header.count != 5)
            return Fail(error, "TEX CBM does not contain 5 mip levels");
        out.frameCount = 1;
        out.mipLayout = quality;
        out.field03 = static_cast<std::uint8_t>(5 - quality);
        out.width = static_cast<std::uint32_t>(file.header.width) >> quality;
        out.height = static_cast<std::uint32_t>(file.header.height) >> quality;
    }

    if (error)
        error->clear();
    return true;
}

std::vector<const CBMIndexedChunk*> SelectCBMChunks(const CBMIndexedFile& file,
                                                    std::uint8_t quality) {
    std::vector<const CBMIndexedChunk*> result;
    if (file.header.kind == CBMKind::Picture) {
        result.reserve(file.chunks.size());
        for (const auto& chunk : file.chunks)
            result.push_back(&chunk);
        return result;
    }
    if (quality > 2)
        return result;
    const std::size_t start = quality;
    if (start >= file.chunks.size())
        return result;
    result.reserve(file.chunks.size() - start);
    for (std::size_t i = start; i < file.chunks.size(); ++i)
        result.push_back(&file.chunks[i]);
    return result;
}

bool ConvertCBMIndexedPixels(const CBMIndexedFile& file,
                             const CBMIndexedChunk& chunk,
                             const CBMPixelFormat& format,
                             std::vector<std::uint8_t>& out,
                             std::string* error) {
    if (format.bitsPerPixel == 8) {
        out = chunk.indices;
        if (error)
            error->clear();
        return true;
    }
    if (format.bitsPerPixel <= 8 || format.bitsPerPixel > 32 ||
        (format.bitsPerPixel & 7u) != 0)
        return Fail(error, "Unsupported CBM target pixel depth");

    auto bitCount = [](std::uint32_t mask) {
        std::uint32_t n = 0;
        while (mask != 0) {
            n += mask & 1u;
            mask >>= 1u;
        }
        return n;
    };
    const std::uint32_t redBits = bitCount(format.redMask);
    const std::uint32_t greenBits = bitCount(format.greenMask);
    const std::uint32_t blueBits = bitCount(format.blueMask);
    if (redBits > 8 || greenBits > 8 || blueBits > 8)
        return Fail(error, "Unsupported CBM target channel width");

    const std::size_t bytesPerPixel = format.bitsPerPixel >> 3u;
    out.assign(chunk.indices.size() * bytesPerPixel, 0);
    for (std::size_t i = 0; i < chunk.indices.size(); ++i) {
        const std::uint8_t index = chunk.indices[i];
        const std::size_t palette = static_cast<std::size_t>(index) * 3u;
        const std::uint32_t r = file.palette8[palette + 0];
        const std::uint32_t g = file.palette8[palette + 1];
        const std::uint32_t b = file.palette8[palette + 2];

        std::uint32_t packed = 0;
        if (redBits != 0)
            packed += (((r >> (8u - redBits)) << greenBits) << blueBits) & format.redMask;
        if (greenBits != 0)
            packed += ((g >> (8u - greenBits)) << blueBits) & format.greenMask;
        if (blueBits != 0)
            packed += (b >> (8u - blueBits)) & format.blueMask;

        if (file.header.hasAlpha && index != file.header.alphaField)
            packed += format.alphaMask;

        const std::size_t dst = i * bytesPerPixel;
        for (std::size_t byte = 0; byte < bytesPerPixel; ++byte)
            out[dst + byte] = static_cast<std::uint8_t>(packed >> (byte * 8u));
    }

    if (error)
        error->clear();
    return true;
}

bool CBM_UploadIndexed(const CBMIndexedFile& file,
                                     const CBMIndexedChunk& chunk,
                                     const CBMPixelFormat& format,
                                     std::size_t pitch,
                                     std::vector<std::uint8_t>& surface,
                                     std::string* error) {
    std::vector<std::uint8_t> converted;
    if (!ConvertCBMIndexedPixels(file, chunk, format, converted, error))
        return false;

    const std::size_t bytesPerPixel = format.bitsPerPixel >> 3u;
    const std::size_t rowBytes = static_cast<std::size_t>(chunk.width) * bytesPerPixel;
    const std::size_t requiredSource = rowBytes * static_cast<std::size_t>(chunk.height);
    if (converted.size() < requiredSource)
        return Fail(error, "CBM indexed chunk is shorter than its dimensions");
    if (pitch < rowBytes)
        return Fail(error, "Locked DirectDraw pitch is smaller than the CBM row");
    if (chunk.height != 0 && pitch > std::numeric_limits<std::size_t>::max() / chunk.height)
        return Fail(error, "Locked DirectDraw surface size overflow");
    const std::size_t requiredSurface = pitch * static_cast<std::size_t>(chunk.height);
    if (surface.size() < requiredSurface)
        return Fail(error, "Locked DirectDraw surface buffer is too small");

    for (std::size_t y = 0; y < chunk.height; ++y) {
        const auto* src = converted.data() + y * rowBytes;
        auto* dst = surface.data() + y * pitch;
        std::copy_n(src, rowBytes, dst);
    }

    if (error)
        error->clear();
    return true;
}

namespace {
const CBMPixelFormat& SelectPixelFormat(const CBMIndexedFile& file,
                                                const CBMPixelFormat& opaque,
                                                const CBMPixelFormat& alpha,
                                                bool& alphaEnabled) {
    alphaEnabled = file.header.hasAlpha;
    if (alphaEnabled && alpha.bitsPerPixel != 0)
        return alpha;
    if (alphaEnabled)
        alphaEnabled = false;
    return opaque;
}

std::array<std::uint8_t, 256 * 4> MakeDirectDrawPalette(const CBMIndexedFile& file) {
    std::array<std::uint8_t, 256 * 4> rgba{};
    for (std::size_t i = 0; i < 256; ++i) {
        rgba[i * 4 + 0] = file.palette8[i * 3 + 0];
        rgba[i * 4 + 1] = file.palette8[i * 3 + 1];
        rgba[i * 4 + 2] = file.palette8[i * 3 + 2];

        rgba[i * 4 + 3] = 0;
    }
    return rgba;
}

bool UploadChunkToNativeLock(const CBMIndexedFile& file,
                             const CBMIndexedChunk& chunk,
                             const CBMPixelFormat& format,
                             const CBMLockedSurface& lock,
                             std::string* error) {
    if (!lock.pixels)
        return Fail(error, "DirectDraw Lock returned a null surface pointer");
    std::vector<std::uint8_t> converted;
    if (!ConvertCBMIndexedPixels(file, chunk, format, converted, error))
        return false;
    const std::size_t bytesPerPixel = format.bitsPerPixel >> 3u;
    const std::size_t rowBytes = static_cast<std::size_t>(chunk.width) * bytesPerPixel;
    if (lock.pitch < rowBytes)
        return Fail(error, "Locked DirectDraw pitch is smaller than the CBM row");
    if (chunk.height != 0 && lock.pitch > std::numeric_limits<std::size_t>::max() / chunk.height)
        return Fail(error, "Locked DirectDraw surface size overflow");
    const std::size_t required = lock.pitch * static_cast<std::size_t>(chunk.height);
    if (lock.capacity < required)
        return Fail(error, "Locked DirectDraw surface capacity is too small");
    for (std::size_t y = 0; y < chunk.height; ++y) {
        const auto* src = converted.data() + y * rowBytes;
        auto* dst = lock.pixels + y * lock.pitch;
        std::copy_n(src, rowBytes, dst);
    }
    return true;
}
}

bool CreateCBMResources(const CBMIndexedFile& file,
                                  std::uint8_t quality,
                                  const CBMPixelFormat& opaque,
                                  const CBMPixelFormat& alpha,
                                  CBMDirectDraw& backend,
                                  CBMHeaderState& runtimeHeader,
                                  CBMResources& resources,
                                  std::string* error) {
    resources = {};
    runtimeHeader = {};

    bool alphaEnabled = false;
    const CBMPixelFormat& format = SelectPixelFormat(file, opaque, alpha, alphaEnabled);
    if (format.bitsPerPixel == 0)
        return Fail(error, "CBM DirectDraw target format has zero bits per pixel");
    if (!MakeCBMHeaderState(file, quality, format.bitsPerPixel, runtimeHeader, error))
        return false;
    runtimeHeader.hasAlpha = alphaEnabled;

    if (format.bitsPerPixel == 8) {
        const auto rgba = MakeDirectDrawPalette(file);
        if (backend.CreatePalette(CBMSurfaceSpec::PaletteCreateFlags,
                                  rgba, resources.palette) != 0)
            return Fail(error, "Palette creation failed");
    }

    CBMSurfaceDesc desc;
    desc.width = runtimeHeader.width;
    desc.height = runtimeHeader.height;
    desc.pixelFormat = format;
    if (runtimeHeader.kind == CBMKind::Texture) {
        desc.flags |= CBMSurfaceSpec::MipMapCountFlag;
        desc.caps |= CBMSurfaceSpec::ComplexMipCaps;
        desc.mipMapCount = runtimeHeader.field03;
    }

    resources.slots.assign(runtimeHeader.frameCount, {});
    for (std::size_t i = 0; i < resources.slots.size(); ++i) {
        auto& slot = resources.slots[i];
        std::uint32_t hr = backend.CreateSurface(desc, slot.surface);
        if (hr == CBMSurfaceSpec::DDERR_NoMipMapHW) {

            resources.mipMapFallback = true;
            runtimeHeader.kind = CBMKind::Picture;
            runtimeHeader.field03 = 0;
            desc.flags = CBMSurfaceSpec::SurfaceDescFlags;
            desc.caps = CBMSurfaceSpec::SurfaceCaps;
            desc.mipMapCount = 0;
            hr = backend.CreateSurface(desc, slot.surface);
        }
        if (hr != 0 || slot.surface == 0)
            return Fail(error, "Picture surface creation failed");

        if (runtimeHeader.pixelBits == 8 && resources.palette != 0) {
            if (backend.SetPalette(slot.surface, resources.palette) != 0)
                return Fail(error, "SetPalette failed for 8Bit texture");
        }

        if (backend.QueryTexture2(slot.surface, CBMSurfaceSpec::Texture2IID,
                                  slot.texture2) != 0 || slot.texture2 == 0)
            return Fail(error, "Texture interface couldn't be queried");
    }

    if (error)
        error->clear();
    return true;
}

bool UploadCBMResources(const CBMIndexedFile& file,
                                  std::uint8_t quality,
                                  const CBMPixelFormat& opaque,
                                  const CBMPixelFormat& alpha,
                                  const CBMHeaderState& runtimeHeader,
                                  const CBMResources& resources,
                                  CBMDirectDraw& backend,
                                  std::string* error) {
    const CBMPixelFormat& format =
        (runtimeHeader.hasAlpha && alpha.bitsPerPixel != 0) ? alpha : opaque;

    if (runtimeHeader.kind != file.header.kind)
        return Fail(error, "Native NoMipMap fallback changes TEX chunk parsing to PICx");

    const auto chunks = SelectCBMChunks(file, quality);
    const std::size_t expected = runtimeHeader.kind == CBMKind::Picture
        ? static_cast<std::size_t>(runtimeHeader.frameCount)
        : static_cast<std::size_t>(runtimeHeader.field03);
    if (chunks.size() != expected)
        return Fail(error, "CBM runtime chunk count does not match native object state");
    if (chunks.empty()) {
        if (error) error->clear();
        return true;
    }
    if (resources.slots.empty() || resources.slots[0].surface == 0)
        return Fail(error, "CBM DirectDraw resources have no root surface");

    CBMNativeHandle currentSurface = resources.slots[0].surface;
    for (std::size_t i = 0; i < chunks.size(); ++i) {
        const auto& chunk = *chunks[i];
        CBMLockedSurface lock{};
        if (backend.Lock(currentSurface, CBMSurfaceSpec::LockFlags, lock) != 0)
            return Fail(error, "An error occured while filling the pixel data! (Lock() failed)");
        const bool copied = UploadChunkToNativeLock(file, chunk, format, lock, error);
        const std::uint32_t unlockHr = backend.Unlock(currentSurface);
        if (!copied)
            return false;
        if (unlockHr != 0)
            return Fail(error, "An error occured while filling the pixel data! (Unlock() failed)");

        if (i + 1 == chunks.size())
            continue;

        if (runtimeHeader.kind == CBMKind::Picture) {
            if (i + 1 >= resources.slots.size() || resources.slots[i + 1].surface == 0)
                return Fail(error, "CBM picture surface slot is missing");
            currentSurface = resources.slots[i + 1].surface;
        } else {
            CBMNativeHandle child = 0;
            if (backend.GetAttachedMipSurface(currentSurface,
                    CBMSurfaceSpec::AttachedMipCaps, child) != 0 || child == 0)
                return Fail(error, "An error occured while filling the pixel data! (GetAttachedSurface() failed)");

            backend.Release(child);
            currentSurface = child;
        }
    }

    if (error)
        error->clear();
    return true;
}

void ReleaseCBMResources(CBMDirectDraw& backend,
                                   CBMHeaderState& runtimeHeader,
                                   CBMResources& resources) {

    for (auto& slot : resources.slots) {
        if (slot.texture2 != 0)
            backend.Release(slot.texture2);
        if (slot.surface != 0)
            backend.Release(slot.surface);
        slot = {};
    }
    resources.slots.clear();
    if (resources.palette != 0)
        backend.Release(resources.palette);
    resources.palette = 0;
    resources.mipMapFallback = false;
    runtimeHeader = {};

    runtimeHeader.frameCount = 0;
}

CBMPicture::~CBMPicture() {
    ReleaseCBMSurfaces();
}

void CBMPicture::AdoptCBMResources(CBMDirectDraw& backend,
                                          CBMResources resources) {
    ReleaseCBMSurfaces();
    directDrawBackend_ = &backend;
    directDrawResources_ = std::move(resources);
    textureInterfaces_.clear();
}

void CBMPicture::ReleaseCBMSurfaces() {
    if (!directDrawBackend_)
        return;
    ReleaseCBMResources(*directDrawBackend_, header_, directDrawResources_);
    directDrawBackend_ = nullptr;
    textureInterfaces_.clear();
}

std::unique_ptr<CBMPicture> CreateCBMPicture(
    const CBMIndexedFile& file,
    std::uint8_t quality,
    const CBMPixelFormat& opaque,
    const CBMPixelFormat& alpha,
    CBMDirectDraw& backend,
    std::string* error) {
    CBMHeaderState header;
    CBMResources resources;
    if (!CreateCBMResources(file, quality, opaque, alpha, backend,
                                      header, resources, error)) {
        ReleaseCBMResources(backend, header, resources);
        return {};
    }
    if (!UploadCBMResources(file, quality, opaque, alpha, header,
                                      resources, backend, error)) {
        ReleaseCBMResources(backend, header, resources);
        return {};
    }

    auto picture = std::make_unique<CBMPicture>(header);
    const CBMPixelFormat& runtimeFormat =
        (header.hasAlpha && alpha.bitsPerPixel != 0) ? alpha : opaque;
    picture->SetRecolorSourceState(file.palette8, runtimeFormat);
    picture->AdoptCBMResources(backend, std::move(resources));
    if (error)
        error->clear();
    return picture;
}

namespace {
unsigned BitCount32(std::uint32_t value) {
    unsigned count = 0;
    while (value != 0) {
        count += value & 1u;
        value >>= 1u;
    }
    return count;
}

std::uint8_t ScaleColorByte(std::uint8_t value, float factor) {

    return static_cast<std::uint8_t>(static_cast<unsigned>(
        static_cast<long double>(value) * static_cast<long double>(factor)));
}

bool NearlyGrey(std::uint8_t r, std::uint8_t g, std::uint8_t b) {

    const int rg = static_cast<int>(r) - static_cast<int>(g);
    const int rb = static_cast<int>(r) - static_cast<int>(b);
    const int gb = static_cast<int>(g) - static_cast<int>(b);
    return rg > -9 && rg < 9 && rb > -9 && rb < 9 && gb > -9 && gb < 9;
}

std::array<std::uint8_t, 256 * 4> PaletteRGBA(
    const std::array<std::uint8_t, 256 * 3>& rgb) {
    std::array<std::uint8_t, 256 * 4> out{};
    for (std::size_t i = 0; i < 256; ++i) {
        out[i * 4 + 0] = rgb[i * 3 + 0];
        out[i * 4 + 1] = rgb[i * 3 + 1];
        out[i * 4 + 2] = rgb[i * 3 + 2];
        out[i * 4 + 3] = 0;
    }
    return out;
}

std::uint32_t ReadPackedLittle(const std::uint8_t* p, unsigned bytes) {
    std::uint32_t value = 0;
    for (unsigned i = 0; i < bytes && i < 4; ++i)
        value |= static_cast<std::uint32_t>(p[i]) << (i * 8u);
    return value;
}

void WritePackedLittle(std::uint8_t* p, unsigned bytes, std::uint32_t value) {
    for (unsigned i = 0; i < bytes && i < 4; ++i)
        p[i] = static_cast<std::uint8_t>(value >> (i * 8u));
}
}

bool CBMPicture::SetColor(std::uint32_t rgb, std::string* error) {

    const std::uint8_t targetR = static_cast<std::uint8_t>((rgb >> 16u) & 0xffu);
    const std::uint8_t targetG = static_cast<std::uint8_t>((rgb >> 8u) & 0xffu);
    const std::uint8_t targetB = static_cast<std::uint8_t>(rgb & 0xffu);

    constexpr float kColorScale = 0.0029411765281111f;
    const float rFactor = static_cast<float>(
        static_cast<long double>(targetR) * kColorScale + 0.25L);
    const float gFactor = static_cast<float>(
        static_cast<long double>(targetG) * kColorScale + 0.25L);
    const float bFactor = static_cast<float>(
        static_cast<long double>(targetB) * kColorScale + 0.25L);

    for (std::size_t i = 0; i < 256; ++i) {
        auto& r = palette8_[i * 3 + 0];
        auto& g = palette8_[i * 3 + 1];
        auto& b = palette8_[i * 3 + 2];
        if (NearlyGrey(r, g, b)) {
            r = ScaleColorByte(r, rFactor);
            g = ScaleColorByte(g, gFactor);
            b = ScaleColorByte(b, bFactor);
        } else if (r < 4u && b < 4u) {

            r = g;
            b = g;
        }
    }

    if (!directDrawBackend_) {
        if (error) *error = "CBM SetColor requires DirectDraw resources";
        return false;
    }

    if (header_.pixelBits == 8u) {
        if (directDrawResources_.palette == 0) {
            if (error) *error = "CBM SetColor has no 8-bit DirectDraw palette";
            return false;
        }
        const auto rgba = PaletteRGBA(palette8_);
        if (directDrawBackend_->SetPaletteEntries(directDrawResources_.palette,
                                                   0u, 0u, 256u, rgba) != 0) {
            if (error) *error = "Changing of the 8Bit palette failed";
            return false;
        }
        if (error) error->clear();
        return true;
    }

    if (header_.pixelBits < 16u || runtimePixelFormat_.bitsPerPixel < 16u) {
        if (error) *error = "CBM SetColor unsupported packed pixel depth";
        return false;
    }
    if (directDrawResources_.slots.empty() || directDrawResources_.slots[0].surface == 0) {
        if (error) *error = "CBM SetColor has no root surface";
        return false;
    }

    const unsigned rBits = BitCount32(runtimePixelFormat_.redMask);
    const unsigned gBits = BitCount32(runtimePixelFormat_.greenMask);
    const unsigned bBits = BitCount32(runtimePixelFormat_.blueMask);
    if (rBits == 0 || gBits == 0 || bBits == 0 ||
        rBits > 8 || gBits > 8 || bBits > 8 || rBits + gBits + bBits > 32u) {
        if (error) *error = "CBM SetColor unsupported RGB masks";
        return false;
    }

    const unsigned bShift = 0;
    const unsigned gShift = bBits;
    const unsigned rShift = bBits + gBits;
    const unsigned bytesPerPixel = header_.pixelBits >> 3u;
    if (bytesPerPixel == 0 || bytesPerPixel > 4u) {
        if (error) *error = "CBM SetColor unsupported bytes per pixel";
        return false;
    }

    CBMNativeHandle surface = directDrawResources_.slots[0].surface;
    std::uint32_t width = header_.width;
    std::uint32_t height = header_.height;
    const unsigned levels = header_.field03;
    for (unsigned level = 0; level < levels; ++level) {
        CBMLockedSurface lock{};
        if (directDrawBackend_->Lock(surface, CBMSurfaceSpec::LockFlags, lock) != 0) {
            if (error) *error = "While reading the pixel data an error occured! (Lock() failed)";
            return false;
        }

        bool validLock = lock.pixels != nullptr;
        const std::size_t rowBytes = static_cast<std::size_t>(width) * bytesPerPixel;
        if (lock.pitch < rowBytes ||
            (height != 0 && lock.pitch > std::numeric_limits<std::size_t>::max() / height) ||
            lock.capacity < lock.pitch * static_cast<std::size_t>(height))
            validLock = false;

        if (validLock) {
            const std::uint32_t bMask = bBits == 32 ? 0xffffffffu : ((1u << bBits) - 1u);
            const std::uint32_t gMask = gBits == 32 ? 0xffffffffu : ((1u << gBits) - 1u);
            const std::uint32_t rMask = rBits == 32 ? 0xffffffffu : ((1u << rBits) - 1u);
            for (std::uint32_t y = 0; y < height; ++y) {
                std::uint8_t* pixel = lock.pixels + static_cast<std::size_t>(y) * lock.pitch;
                for (std::uint32_t x = 0; x < width; ++x, pixel += bytesPerPixel) {
                    const std::uint32_t packed = ReadPackedLittle(pixel, bytesPerPixel);
                    std::uint8_t b = static_cast<std::uint8_t>(
                        ((packed >> bShift) & bMask) << (8u - bBits));
                    std::uint8_t g = static_cast<std::uint8_t>(
                        ((packed >> gShift) & gMask) << (8u - gBits));
                    std::uint8_t r = static_cast<std::uint8_t>(
                        ((packed >> rShift) & rMask) << (8u - rBits));

                    bool write = false;
                    if (NearlyGrey(r, g, b)) {
                        r = ScaleColorByte(r, rFactor);
                        g = ScaleColorByte(g, gFactor);
                        b = ScaleColorByte(b, bFactor);
                        write = true;
                    } else if (r < 4u && b < 4u) {
                        r = g;
                        b = g;
                        write = true;
                    }

                    if (!write)
                        continue;
                    const std::uint32_t repacked =
                        ((static_cast<std::uint32_t>(b) >> (8u - bBits)) << bShift) +
                        ((static_cast<std::uint32_t>(g) >> (8u - gBits)) << gShift) +
                        ((static_cast<std::uint32_t>(r) >> (8u - rBits)) << rShift);
                    WritePackedLittle(pixel, bytesPerPixel, repacked);
                }
            }
        }

        const std::uint32_t unlockHr = directDrawBackend_->Unlock(surface);
        if (!validLock) {
            if (error) *error = "CBM SetColor locked surface geometry is invalid";
            return false;
        }
        if (unlockHr != 0) {
            if (error) *error = "While changing the pixel data an error occured! (Unlock() failed)";
            return false;
        }

        if (level + 1u >= levels)
            break;
        CBMNativeHandle child = 0;
        if (directDrawBackend_->GetAttachedMipSurface(
                surface, CBMSurfaceSpec::AttachedMipCaps, child) != 0 || child == 0) {
            if (error) *error = "While changing the pixel data an error occured! (GetAttachedSurface() failed)";
            return false;
        }

        directDrawBackend_->Release(child);
        surface = child;
        width /= 2u;
        height /= 2u;
    }

    if (error) error->clear();
    return true;
}

std::uint32_t CBMPicture::GetCBMSize() const {
    const std::uint32_t bytesPerPixel = header_.pixelBits >> 3;
    const std::uint64_t basePixels =
        static_cast<std::uint64_t>(header_.width) * header_.height;

    if (header_.kind == CBMKind::Picture) {

        const std::uint64_t bytes = basePixels * header_.frameCount * bytesPerPixel;
        return bytes > std::numeric_limits<std::uint32_t>::max()
                   ? std::numeric_limits<std::uint32_t>::max()
                   : static_cast<std::uint32_t>(bytes);
    }

    if (header_.kind != CBMKind::Texture)
        return 0;

    double factor = 0.0;
    switch (header_.mipLayout) {
    case 0: factor = 1.33203125; break;
    case 1: factor = 1.328125;   break;
    case 2: factor = 1.3125;     break;
    default: return 0;
    }

    return TruncatePositive(static_cast<double>(basePixels) *
                            static_cast<double>(bytesPerPixel) * factor);
}

std::uintptr_t CBMPicture::TextureInterface(std::uint8_t oneBasedIndex) const {

    if (oneBasedIndex == 0 || oneBasedIndex > header_.frameCount)
        return 0;
    const std::size_t i = static_cast<std::size_t>(oneBasedIndex - 1);
    if (directDrawBackend_) {
        if (i >= directDrawResources_.slots.size())
            return 0;
        return directDrawResources_.slots[i].texture2;
    }
    if (i >= textureInterfaces_.size())
        return 0;
    return textureInterfaces_[i];
}

}
