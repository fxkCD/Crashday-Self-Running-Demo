#include "cbm.hpp"
#include <algorithm>
#include <cctype>
#include <fstream>
#include <iterator>
#include <limits>

namespace flydemo {

FileCBMLoader::FileCBMLoader(
    std::filesystem::path texturesDirectory, CBMPixelFormat opaqueFormat,
    CBMPixelFormat alphaFormat, CBMDirectDraw& backend,
    std::uint8_t textureQuality)
    : texturesDirectory_(std::move(texturesDirectory)),
      opaqueFormat_(opaqueFormat), alphaFormat_(alphaFormat), backend_(backend),
      textureQuality_(textureQuality) {}

std::unique_ptr<CBMPicture> FileCBMLoader::LoadCBM(
    std::string_view canonicalName) {
    lastError_.clear();
    const std::filesystem::path file = texturesDirectory_ /
        std::filesystem::path(std::string(canonicalName));
    std::ifstream input(file, std::ios::binary);
    if (!input) {
        lastError_ = "Cannot open CBM: " + file.string();
        return nullptr;
    }
    std::vector<std::uint8_t> bytes((std::istreambuf_iterator<char>(input)),
                                    std::istreambuf_iterator<char>());
    CBMIndexedFile parsed{};
    if (!ParseCBMIndexedFile(bytes, parsed, &lastError_))
        return nullptr;
    return CreateCBMPicture(parsed, textureQuality_, opaqueFormat_,
                                      alphaFormat_, backend_, &lastError_);
}

CBMManager::CBMManager() { Reset(); }

void CBMManager::Reset() {

    for (auto& slot : slots_) {
        slot.used = false;
        slot.name.clear();
        slot.cbm.reset();
    }
}

std::string CBMManager::CanonicalName(std::string_view name) {
    std::string out(name);
    for (char& ch : out) {
        const unsigned char c = static_cast<unsigned char>(ch);
        if (c >= 'A' && c <= 'Z')
            ch = static_cast<char>(c + ('a' - 'A'));
    }
    return out;
}

bool CBMManager::Contains(std::string_view name) const {
    return IndexOf(name) != InvalidTexture;
}

std::uint8_t CBMManager::IndexOf(std::string_view name) const {
    const std::string key = CanonicalName(name);
    for (std::size_t i = 0; i < slots_.size(); ++i) {
        if (slots_[i].used && slots_[i].name == key)
            return static_cast<std::uint8_t>(i);
    }
    return InvalidTexture;
}

CBMPicture* CBMManager::Get(std::string_view name) {
    const std::uint8_t i = IndexOf(name);
    return i == InvalidTexture ? nullptr : slots_[i].cbm.get();
}

const CBMPicture* CBMManager::Get(std::string_view name) const {
    const std::uint8_t i = IndexOf(name);
    return i == InvalidTexture ? nullptr : slots_[i].cbm.get();
}

CBMPicture* CBMManager::Get(std::uint8_t index) {
    if (index >= MaxTextures || !slots_[index].used)
        return nullptr;
    return slots_[index].cbm.get();
}

const CBMPicture* CBMManager::Get(std::uint8_t index) const {
    if (index >= MaxTextures || !slots_[index].used)
        return nullptr;
    return slots_[index].cbm.get();
}

std::uint8_t CBMManager::Load(std::string_view name, CBMLoader& loader) {

    const std::string key = CanonicalName(name);
    const std::uint8_t old = IndexOf(key);
    if (old != InvalidTexture)
        return old;

    std::size_t freeIndex = slots_.size();
    for (std::size_t i = 0; i < slots_.size(); ++i) {
        if (!slots_[i].used) {
            freeIndex = i;
            break;
        }
    }
    if (freeIndex == slots_.size())
        return InvalidTexture;

    auto& slot = slots_[freeIndex];
    slot.used = true;
    slot.name = key;
    slot.cbm = loader.LoadCBM(key);
    if (!slot.cbm) {
        if (loadObserver_)
            loadObserver_(observerContext_, key, {}, nullptr, false);
        slot.used = false;
        slot.name.clear();
        return InvalidTexture;
    }
    if (loadObserver_)
        loadObserver_(observerContext_, key, {}, slot.cbm.get(), true);
    return static_cast<std::uint8_t>(freeIndex);
}

std::uint8_t CBMManager::LoadAs(std::string_view sourceName,
                                std::string_view aliasName,
                                CBMLoader& loader) {

    const std::string alias = CanonicalName(aliasName);
    const std::uint8_t old = IndexOf(alias);
    if (old != InvalidTexture)
        return old;

    std::size_t freeIndex = slots_.size();
    for (std::size_t i = 0; i < slots_.size(); ++i) {
        if (!slots_[i].used) {
            freeIndex = i;
            break;
        }
    }
    if (freeIndex == slots_.size())
        return InvalidTexture;

    auto& slot = slots_[freeIndex];
    slot.used = true;
    slot.name = alias;

    const std::string source = CanonicalName(sourceName);
    slot.cbm = loader.LoadCBM(source);
    if (!slot.cbm) {
        if (loadObserver_)
            loadObserver_(observerContext_, source, alias, nullptr, false);
        slot.used = false;
        slot.name.clear();
        return InvalidTexture;
    }
    if (loadObserver_)
        loadObserver_(observerContext_, source, alias, slot.cbm.get(), true);
    return static_cast<std::uint8_t>(freeIndex);
}

bool CBMManager::Delete(std::string_view name) {
    const std::uint8_t i = IndexOf(name);
    const bool loaded = i != InvalidTexture;
    if (deleteObserver_)
        deleteObserver_(observerContext_, name, loaded);
    if (!loaded)
        return false;

    auto& slot = slots_[i];
    slot.used = false;
    slot.cbm.reset();
    return true;
}

bool CBMManager::Delete(std::uint8_t index) {
    if (index >= MaxTextures || !slots_[index].used || !slots_[index].cbm)
        return false;
    auto& slot = slots_[index];
    if (deleteObserver_)
        deleteObserver_(observerContext_, slot.name, true);
    slot.used = false;
    slot.cbm.reset();
    return true;
}

void CBMManager::DeleteAll() {
    if (deleteAllObserver_)
        deleteAllObserver_(observerContext_);

    for (auto& slot : slots_) {
        if (!slot.used)
            continue;
        slot.used = false;
        slot.cbm.reset();
    }
}

std::uint32_t CBMManager::TotalCBMBytes() const {

    std::uint64_t total = 0;
    for (const auto& slot : slots_) {
        if (slot.used && slot.cbm)
            total += slot.cbm->GetCBMSize();
    }
    return total > std::numeric_limits<std::uint32_t>::max()
               ? std::numeric_limits<std::uint32_t>::max()
               : static_cast<std::uint32_t>(total);
}

std::uintptr_t CBM_GetTextureInterface(
    const CBMManager& textures, std::uint8_t textureIndex, std::uint8_t oneBasedVariant) {

    const CBMPicture* picture = textures.Get(textureIndex);
    return picture ? picture->TextureInterface(oneBasedVariant) : 0;
}

std::size_t CBMManager::UsedCount() const {
    return static_cast<std::size_t>(std::count_if(
        slots_.begin(), slots_.end(), [](const TEXTUREENTRY& s) { return s.used; }));
}

}
