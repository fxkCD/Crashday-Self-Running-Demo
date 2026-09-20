#include "ambience.hpp"

#include <cstdlib>
#include <stdexcept>
#include <utility>

namespace flydemo {
namespace {

int ParseInt(const std::string& text) {

    return static_cast<int>(std::strtol(text.c_str(), nullptr, 10));
}

float ParseFloat(const std::string& text) {

    return static_cast<float>(std::strtod(text.c_str(), nullptr));
}

std::uint32_t PackAmbienceRGB(int r, int g, int b) {

    return (static_cast<std::uint32_t>(r) & 0xffu) << 16 |
           (static_cast<std::uint32_t>(g) & 0xffu) << 8  |
           (static_cast<std::uint32_t>(b) & 0xffu);
}

bool ReadLine(CDFileOperations& file, std::string& out,
              const char* what, std::string& error) {
    if (CrashdayDirectory::ReadConfigLine(file, out))
        return true;
    error = std::string("ambience file ended before ") + what;
    return false;
}

bool ReadToken(CDFileOperations& file, std::string& out,
               const char* what, std::string& error) {
    if (CrashdayDirectory::ReadConfigToken(file, out))
        return true;
    error = std::string("ambience file ended before ") + what;
    return false;
}

bool ReadRGB(CDFileOperations& file, std::uint32_t& out,
             const char* what, std::string& error) {
    std::string a, b, c;

    if (!ReadToken(file, a, what, error) ||
        !ReadToken(file, b, what, error) ||
        !ReadLine(file, c, what, error))
        return false;
    out = PackAmbienceRGB(ParseInt(a), ParseInt(b), ParseInt(c));
    return true;
}

bool ReadXYZ(CDFileOperations& file, CD3DVECTOR& out,
             const char* what, std::string& error) {
    std::string a, b, c;
    if (!ReadToken(file, a, what, error) ||
        !ReadToken(file, b, what, error) ||
        !ReadLine(file, c, what, error))
        return false;
    out.x = ParseFloat(a);
    out.y = ParseFloat(b);
    out.z = ParseFloat(c);
    return true;
}

}

AmbienceParseResult ReadAmbienceFile(CDFileOperations& file,
                                     const std::string& fileName,
                                     AMBIENCE_ENTRY& out) {
    AmbienceParseResult result;
    if (!file.IsOpen()) {
        result.error = "ambience file is not open";
        return result;
    }

    AMBIENCE_ENTRY parsed;
    parsed.fileName = fileName;

    std::string ignoredHeader;
    if (!ReadLine(file, ignoredHeader, "header", result.error) ||
        !ReadLine(file, parsed.name, "display name", result.error) ||
        !ReadLine(file, parsed.resource, "resource base", result.error))
        return result;

    if (!ReadRGB(file, parsed.ambientColor, "ambient RGB", result.error) ||
        !ReadRGB(file, parsed.light.packedColor, "light RGB", result.error) ||
        !ReadXYZ(file, parsed.light.position, "light direction", result.error) ||
        !ReadRGB(file, parsed.environmentTail.packedColor8, "tail RGB", result.error))
        return result;

    std::string value0, value4;
    if (!ReadLine(file, value0, "tail value +0x6C", result.error) ||
        !ReadLine(file, value4, "tail value +0x70", result.error))
        return result;
    parsed.environmentTail.value0 = ParseFloat(value0);
    parsed.environmentTail.value4 = ParseFloat(value4);

    out = std::move(parsed);
    result.ok = true;
    return result;
}

AmbienceSystem::AmbienceSystem(std::vector<AMBIENCE_ENTRY> entries)
    : entries_(std::move(entries)) {}

bool AmbienceSystem::IsAvailable(const std::string& name) const {
    if (name == DefaultToken || name == RandomToken)
        return true;

    for (const auto& e : entries_)
        if (e.name == name)
            return true;
    return false;
}

const std::string& AmbienceSystem::NameAt(std::size_t index) const {

    if (index >= entries_.size())
        throw std::out_of_range("ambience index");
    return entries_[index].name;
}

const std::string& AmbienceSystem::FileAt(std::size_t index) const {
    if (index >= entries_.size())
        throw std::out_of_range("ambience index");
    return entries_[index].fileName;
}

AmbienceSelection AmbienceSystem::Choose(const std::string& request,
                                               std::uint32_t randomValue) {

    if (request == currentRequest_)
        return {AmbienceChoiceKind::NoChange, 0, nullptr};

    currentRequest_ = request;

    if (request == DefaultToken)
        return {AmbienceChoiceKind::DefaultEnvironment, 0, nullptr};

    std::string wanted = request;
    if (request == RandomToken) {

        if (entries_.empty())
            throw std::domain_error("native RANDOM ambience would divide by zero");
        wanted = entries_[randomValue % entries_.size()].fileName;
    }

    for (std::size_t i = 0; i < entries_.size(); ++i) {
        if (entries_[i].fileName == wanted)
            return {AmbienceChoiceKind::Entry, i, &entries_[i]};
    }
    return {AmbienceChoiceKind::Unavailable, 0, nullptr};
}

AmbienceSelection AmbienceSystem::ChooseAndApply(
    const std::string& request,
    std::uint32_t randomValue,
    bool sceneInProgress,
    EngineEnvironment& engine,
    EngineEnvIO& backend) {
    AmbienceSelection selected = Choose(request, randomValue);
    if (selected.kind == AmbienceChoiceKind::DefaultEnvironment) {
        engine.ApplyDefault(sceneInProgress, backend);
    } else if (selected.kind == AmbienceChoiceKind::Entry) {
        AMBIENCE_ENTRY& e = *selected.entry;
        engine.ApplyEnvironment(e.ambientColor, e.light, e.environmentTail,
                                e.resource, sceneInProgress, backend);
    }
    return selected;
}

bool AmbienceSystem::LoadEnumerated(CrashdayDirectory& directory,
                                         CDFileOperations& file,
                                         const std::vector<std::string>& fileNames,
                                         std::string* error) {

    if (directory.ChangeTo(SourceDirectory) != 0) {
        if (error) *error = "cannot change to TEXTURES/AMBIENCE";
        return false;
    }
    if (fileNames.empty()) {
        if (error) *error = "no *.amb files";
        return false;
    }
    if (fileNames.size() > 0x7fffu) {
        if (error) *error = "ambience count exceeds signed WORD storage";
        return false;
    }

    std::vector<AMBIENCE_ENTRY> parsed;
    parsed.reserve(fileNames.size());
    for (const std::string& name : fileNames) {

        if (file.Open(name.c_str(), "r+", 1) != 0) {
            if (error) *error = "cannot open ambience file: " + name;
            return false;
        }
        AMBIENCE_ENTRY entry;
        const AmbienceParseResult r = ReadAmbienceFile(file, name, entry);
        (void)file.Close();
        if (!r.ok) {
            if (error) *error = name + ": " + r.error;
            return false;
        }
        parsed.push_back(std::move(entry));
    }

    entries_ = std::move(parsed);

    currentRequest_ = DefaultToken;
    if (error) error->clear();
    return true;
}

}
