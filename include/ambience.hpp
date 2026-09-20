#pragma once

#include "engine.hpp"
#include "path.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace flydemo {

struct AMBIENCE_ENTRY {
    std::string name;
    std::string fileName;
    std::string resource;
    std::uint32_t ambientColor = 0;
    CD3DLIGHT light{};
    EnvironmentState environmentTail{};
};

struct AmbienceParseResult {
    bool ok = false;
    std::string error;
};

AmbienceParseResult ReadAmbienceFile(CDFileOperations& file,
                                     const std::string& fileName,
                                     AMBIENCE_ENTRY& out);

enum class AmbienceChoiceKind : std::uint8_t {
    NoChange,
    DefaultEnvironment,
    Entry,
    Unavailable,
};

struct AmbienceSelection {
    AmbienceChoiceKind kind = AmbienceChoiceKind::Unavailable;
    std::size_t index = 0;
    AMBIENCE_ENTRY* entry = nullptr;
};

class AmbienceSystem {
public:
    static constexpr const char* DefaultToken = "DEFAULT";
    static constexpr const char* RandomToken = "RANDOM";
    static constexpr const char* SourceDirectory = "TEXTURES/AMBIENCE";
    static constexpr const char* FilePattern = "*.amb";

    explicit AmbienceSystem(std::vector<AMBIENCE_ENTRY> entries = {});
    std::size_t Count() const { return entries_.size(); }
    const std::string& CurrentRequest() const { return currentRequest_; }
    const std::vector<AMBIENCE_ENTRY>& Entries() const { return entries_; }
    std::vector<AMBIENCE_ENTRY>& Entries() { return entries_; }

    bool IsAvailable(const std::string& name) const;
    const std::string& NameAt(std::size_t index) const;
    const std::string& FileAt(std::size_t index) const;

    AmbienceSelection Choose(const std::string& request,
                             std::uint32_t randomValue);

    AmbienceSelection ChooseAndApply(const std::string& request,
                                     std::uint32_t randomValue,
                                     bool sceneInProgress,
                                     EngineEnvironment& engine,
                                     EngineEnvIO& backend);

    bool LoadEnumerated(CrashdayDirectory& directory,
                        CDFileOperations& file,
                        const std::vector<std::string>& fileNames,
                        std::string* error = nullptr);

private:
    std::vector<AMBIENCE_ENTRY> entries_;
    std::string currentRequest_ = DefaultToken;
};

}
