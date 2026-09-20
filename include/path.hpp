#pragma once
#include <filesystem>
#include <string>
#include "cdfileop.hpp"

namespace flydemo {

class CrashdayDirectory {
public:
    explicit CrashdayDirectory(std::filesystem::path gameDirectory);

    static bool FromCrshPath(CrashdayDirectory& out,
                             const std::filesystem::path& file = "CrshPath.txt");

    int ChangeTo(const std::string& subdirectory) const;

    const std::filesystem::path& GameDirectory() const { return gameDirectory_; }
    bool Exists() const;

    static bool ReadConfigLine(CDFileOperations& file, std::string& out);

    static bool ReadConfigToken(CDFileOperations& file, std::string& out);

private:
    std::filesystem::path gameDirectory_;
};

}
