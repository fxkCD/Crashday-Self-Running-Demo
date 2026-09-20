#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <string_view>
#include <vector>
#include <utility>

namespace flydemo {

enum class CBMKind : std::uint8_t {
    Picture = 0,
    Texture = 1,
};

struct CBMHeaderState {
    CBMKind kind = CBMKind::Picture;
    std::uint8_t frameCount = 1;
    std::uint8_t mipLayout = 0;
    std::uint8_t field03 = 0;
    std::uint32_t pixelBits = 0;
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::uint16_t colorKey16 = 0;
    bool hasAlpha = false;
    std::uint8_t alphaField = 0;
};

struct CBMFileHeader {
    std::uint8_t version = 0;
    CBMKind kind = CBMKind::Picture;
    std::uint8_t count = 0;
    std::uint16_t width = 0;
    std::uint16_t height = 0;
    std::uint8_t field314 = 0;
    bool hasAlpha = false;
    std::uint8_t alphaField = 0;
};

struct CBMIndexedChunk {
    std::uint8_t marker = 0;
    std::uint16_t width = 0;
    std::uint16_t height = 0;
    std::vector<std::uint8_t> indices;
};

struct CBMIndexedFile {
    CBMFileHeader header{};

    std::array<std::uint8_t, 256 * 3> palette6{};
    std::array<std::uint8_t, 256 * 3> palette8{};
    std::vector<CBMIndexedChunk> chunks;
    std::size_t consumedBytes = 0;
};

bool ParseCBMIndexedFile(const std::vector<std::uint8_t>& bytes,
                         CBMIndexedFile& out,
                         std::string* error = nullptr);

bool MakeCBMHeaderState(const CBMIndexedFile& file,
                        std::uint8_t quality,
                        std::uint32_t targetPixelBits,
                        CBMHeaderState& out,
                        std::string* error = nullptr);

std::vector<const CBMIndexedChunk*> SelectCBMChunks(const CBMIndexedFile& file,
                                                    std::uint8_t quality);

struct CBMPixelFormat {

    std::uint32_t size = 0x20u;
    std::uint32_t flags = 0;
    std::uint32_t fourCC = 0;
    std::uint32_t bitsPerPixel = 0;
    std::uint32_t redMask = 0;
    std::uint32_t greenMask = 0;
    std::uint32_t blueMask = 0;
    std::uint32_t alphaMask = 0;

    constexpr CBMPixelFormat() = default;
    constexpr CBMPixelFormat(std::uint32_t bits,
                             std::uint32_t red,
                             std::uint32_t green,
                             std::uint32_t blue,
                             std::uint32_t alpha,
                             std::uint32_t formatFlags = 0,
                             std::uint32_t formatFourCC = 0,
                             std::uint32_t formatSize = 0x20u)
        : size(formatSize), flags(formatFlags), fourCC(formatFourCC),
          bitsPerPixel(bits), redMask(red), greenMask(green), blueMask(blue),
          alphaMask(alpha) {}
};
static_assert(sizeof(CBMPixelFormat) == 0x20, "DDPIXELFORMAT payload must stay 32 bytes");

bool ConvertCBMIndexedPixels(const CBMIndexedFile& file,
                             const CBMIndexedChunk& chunk,
                             const CBMPixelFormat& format,
                             std::vector<std::uint8_t>& out,
                             std::string* error = nullptr);

struct CBMSurfaceSpec {
    static constexpr std::uint32_t PaletteCreateFlags = 0x00000004u;
    static constexpr std::uint32_t SurfaceDescSize = 0x0000007Cu;
    static constexpr std::uint32_t SurfaceDescFlags = 0x00001007u;
    static constexpr std::uint32_t MipMapCountFlag = 0x00020000u;
    static constexpr std::uint32_t PixelFormatSize = 0x00000020u;
    static constexpr std::uint32_t SurfaceCaps = 0x00001000u;
    static constexpr std::uint32_t ComplexMipCaps = 0x00400008u;
    static constexpr std::uint32_t AttachedMipCaps = 0x00401000u;
    static constexpr std::uint32_t SurfaceCaps2 = 0x00000010u;
    static constexpr std::uint32_t LockFlags = 0x00000001u;
    static constexpr std::uint32_t DDERR_NoMipMapHW = 0x8876024Fu;

    static constexpr std::array<std::uint8_t, 16> Texture2IID = {
        0x02,0x15,0x28,0x93,0xF8,0x8C,0xD0,0x11,
        0x89,0xAB,0x00,0xA0,0xC9,0x05,0x41,0x29
    };
};

using CBMNativeHandle = std::uintptr_t;

struct CBMSurfaceDesc {
    std::uint32_t size = CBMSurfaceSpec::SurfaceDescSize;
    std::uint32_t flags = CBMSurfaceSpec::SurfaceDescFlags;
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::uint32_t mipMapCount = 0;
    CBMPixelFormat pixelFormat{};
    std::uint32_t caps = CBMSurfaceSpec::SurfaceCaps;
    std::uint32_t caps2 = CBMSurfaceSpec::SurfaceCaps2;
};

struct CBMLockedSurface {
    std::uint8_t* pixels = nullptr;
    std::size_t pitch = 0;
    std::size_t capacity = 0;
};

struct CBMSurface {
    CBMNativeHandle surface = 0;
    CBMNativeHandle texture2 = 0;
};

struct CBMResources {
    CBMNativeHandle palette = 0;
    std::vector<CBMSurface> slots;
    bool mipMapFallback = false;
};

class CBMDirectDraw {
public:
    virtual ~CBMDirectDraw() = default;
    virtual std::uint32_t CreatePalette(
        std::uint32_t flags,
        const std::array<std::uint8_t, 256 * 4>& rgba,
        CBMNativeHandle& outPalette) = 0;
    virtual std::uint32_t CreateSurface(
        const CBMSurfaceDesc& desc, CBMNativeHandle& outSurface) = 0;
    virtual std::uint32_t SetPalette(
        CBMNativeHandle surface, CBMNativeHandle palette) = 0;

    virtual std::uint32_t SetPaletteEntries(
        CBMNativeHandle palette, std::uint32_t flags, std::uint32_t start,
        std::uint32_t count, const std::array<std::uint8_t, 256 * 4>& rgba) {
        (void)palette; (void)flags; (void)start; (void)count; (void)rgba;
        return 1u;
    }
    virtual std::uint32_t QueryTexture2(
        CBMNativeHandle surface,
        const std::array<std::uint8_t, 16>& iid,
        CBMNativeHandle& outTexture2) = 0;
    virtual std::uint32_t Lock(
        CBMNativeHandle surface, std::uint32_t flags, CBMLockedSurface& out) = 0;
    virtual std::uint32_t Unlock(CBMNativeHandle surface) = 0;
    virtual std::uint32_t GetAttachedMipSurface(
        CBMNativeHandle surface, std::uint32_t caps, CBMNativeHandle& outSurface) = 0;
    virtual std::uint32_t Release(CBMNativeHandle handle) = 0;
};

bool CreateCBMResources(const CBMIndexedFile& file,
                                  std::uint8_t quality,
                                  const CBMPixelFormat& opaque,
                                  const CBMPixelFormat& alpha,
                                  CBMDirectDraw& backend,
                                  CBMHeaderState& runtimeHeader,
                                  CBMResources& resources,
                                  std::string* error = nullptr);

bool UploadCBMResources(const CBMIndexedFile& file,
                                  std::uint8_t quality,
                                  const CBMPixelFormat& opaque,
                                  const CBMPixelFormat& alpha,
                                  const CBMHeaderState& runtimeHeader,
                                  const CBMResources& resources,
                                  CBMDirectDraw& backend,
                                  std::string* error = nullptr);

void ReleaseCBMResources(CBMDirectDraw& backend,
                                   CBMHeaderState& runtimeHeader,
                                   CBMResources& resources);

bool CBM_UploadIndexed(const CBMIndexedFile& file,
                                     const CBMIndexedChunk& chunk,
                                     const CBMPixelFormat& format,
                                     std::size_t pitch,
                                     std::vector<std::uint8_t>& surface,
                                     std::string* error = nullptr);

class CBMPicture {
public:
    CBMPicture() = default;
    explicit CBMPicture(CBMHeaderState header) : header_(header) {}
    ~CBMPicture();
    CBMPicture(const CBMPicture&) = delete;
    CBMPicture& operator=(const CBMPicture&) = delete;
    CBMPicture(CBMPicture&&) = delete;
    CBMPicture& operator=(CBMPicture&&) = delete;

    CBMKind Kind() const { return header_.kind; }
    std::uint8_t FrameCount() const { return header_.frameCount; }
    std::uint8_t MipLayout() const { return header_.mipLayout; }
    std::uint8_t Field03() const { return header_.field03; }
    std::uint32_t PixelBits() const { return header_.pixelBits; }
    std::uint32_t Width() const { return header_.width; }
    std::uint32_t Height() const { return header_.height; }
    bool HasAlpha() const { return header_.hasAlpha; }
    std::uint8_t AlphaField() const { return header_.hasAlpha ? header_.alphaField : 0; }
    std::uint16_t ColorKey16() const { return header_.colorKey16; }

    std::uint32_t GetCBMSize() const;

    void SetTextureInterfaces(std::vector<std::uintptr_t> interfaces) {
        textureInterfaces_ = std::move(interfaces);
    }
    std::uintptr_t TextureInterface(std::uint8_t oneBasedIndex) const;

    void AdoptCBMResources(CBMDirectDraw& backend,
                                  CBMResources resources);
    void ReleaseCBMSurfaces();
    bool HasDirectDrawResources() const { return directDrawBackend_ != nullptr; }
    const CBMResources& DirectDrawResources() const { return directDrawResources_; }

    const CBMHeaderState& Header() const { return header_; }

    bool SetColor(std::uint32_t rgb, std::string* error = nullptr);
    const std::array<std::uint8_t, 256 * 3>& RecolorPalette() const { return palette8_; }
    const CBMPixelFormat& RuntimePixelFormat() const { return runtimePixelFormat_; }

    void SetRecolorSourceState(const std::array<std::uint8_t, 256 * 3>& palette8,
                               const CBMPixelFormat& format) {
        palette8_ = palette8;
        runtimePixelFormat_ = format;
    }

private:
    CBMHeaderState header_{};
    std::array<std::uint8_t, 256 * 3> palette8_{};
    CBMPixelFormat runtimePixelFormat_{};
    std::vector<std::uintptr_t> textureInterfaces_;
    CBMDirectDraw* directDrawBackend_ = nullptr;
    CBMResources directDrawResources_{};
};

std::unique_ptr<CBMPicture> CreateCBMPicture(
    const CBMIndexedFile& file,
    std::uint8_t quality,
    const CBMPixelFormat& opaque,
    const CBMPixelFormat& alpha,
    CBMDirectDraw& backend,
    std::string* error = nullptr);

class CBMManager;

std::uintptr_t CBM_GetTextureInterface(
    const CBMManager& textures, std::uint8_t textureIndex, std::uint8_t oneBasedVariant);

class CBMLoader {
public:
    virtual ~CBMLoader() = default;

    virtual std::unique_ptr<CBMPicture> LoadCBM(std::string_view canonicalName) = 0;
};

class FileCBMLoader final : public CBMLoader {
public:
    FileCBMLoader(std::filesystem::path texturesDirectory,
                               CBMPixelFormat opaqueFormat,
                               CBMPixelFormat alphaFormat,
                               CBMDirectDraw& backend,
                               std::uint8_t textureQuality = 0);

    std::unique_ptr<CBMPicture> LoadCBM(std::string_view canonicalName) override;
    const std::string& LastError() const { return lastError_; }

private:
    std::filesystem::path texturesDirectory_;
    CBMPixelFormat opaqueFormat_{};
    CBMPixelFormat alphaFormat_{};
    CBMDirectDraw& backend_;
    std::uint8_t textureQuality_ = 0;
    std::string lastError_;
};

struct TEXTUREENTRY {
    bool used = false;
    std::string name;
    std::unique_ptr<CBMPicture> cbm;
};

class CBMManager {
public:

    using LoadObserver = void (*)(void* context, std::string_view sourceName,
                                  std::string_view aliasName,
                                  const CBMPicture* picture, bool success);
    using DeleteObserver = void (*)(void* context, std::string_view name,
                                    bool wasLoaded);
    using DeleteAllObserver = void (*)(void* context);

    static constexpr std::size_t MaxTextures = 0xFE;
    static constexpr std::uint8_t InvalidTexture = 0xFF;

    CBMManager();
    void Reset();
    void SetObserver(void* context, LoadObserver loadObserver,
                     DeleteObserver deleteObserver,
                     DeleteAllObserver deleteAllObserver) {
        observerContext_ = context;
        loadObserver_ = loadObserver;
        deleteObserver_ = deleteObserver;
        deleteAllObserver_ = deleteAllObserver;
    }

    bool Contains(std::string_view name) const;
    std::uint8_t IndexOf(std::string_view name) const;
    CBMPicture* Get(std::string_view name);
    const CBMPicture* Get(std::string_view name) const;
    CBMPicture* Get(std::uint8_t index);
    const CBMPicture* Get(std::uint8_t index) const;

    std::uint8_t Load(std::string_view name, CBMLoader& loader);

    std::uint8_t LoadAs(std::string_view sourceName, std::string_view aliasName,
                        CBMLoader& loader);

    bool Delete(std::string_view name);
    bool Delete(std::uint8_t index);
    void DeleteAll();

    std::uint32_t TotalCBMBytes() const;
    std::size_t UsedCount() const;
    const std::array<TEXTUREENTRY, MaxTextures>& Slots() const { return slots_; }

    static std::string CanonicalName(std::string_view name);

private:
    std::array<TEXTUREENTRY, MaxTextures> slots_{};
    void* observerContext_ = nullptr;
    LoadObserver loadObserver_ = nullptr;
    DeleteObserver deleteObserver_ = nullptr;
    DeleteAllObserver deleteAllObserver_ = nullptr;
};

}
