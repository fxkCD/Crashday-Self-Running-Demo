#include "path.hpp"
#include <system_error>

namespace flydemo {

CrashdayDirectory::CrashdayDirectory(std::filesystem::path gameDirectory)
    : gameDirectory_(std::move(gameDirectory)) {}

bool CrashdayDirectory::Exists() const {
    std::error_code ec;
    return std::filesystem::exists(gameDirectory_, ec) && !ec;
}

bool CrashdayDirectory::FromCrshPath(CrashdayDirectory& out,
                                     const std::filesystem::path& file) {
    CDFileOperations f;

    if (f.Open(file.string().c_str(), "rb+") != 0)
        return false;
    std::string line;
    const bool got = ReadConfigLine(f, line);
    (void)f.Close();
    if (!got || line.empty())
        return false;
    CrashdayDirectory candidate{std::filesystem::path(line)};
    if (!candidate.Exists())
        return false;
    out = std::move(candidate);
    return true;
}

int CrashdayDirectory::ChangeTo(const std::string& subdirectory) const {
    std::error_code ec;
    std::filesystem::current_path(gameDirectory_, ec);
    if (ec)
        return ec.value() ? ec.value() : -1;

    if (subdirectory == "ROOT")
        return 0;

    const std::filesystem::path child = gameDirectory_ / subdirectory;
    if (!std::filesystem::exists(child, ec) || ec)
        return ec.value() ? ec.value() : -1;
    std::filesystem::current_path(child, ec);
    return ec ? (ec.value() ? ec.value() : -1) : 0;
}

bool CrashdayDirectory::ReadConfigLine(CDFileOperations& file,
                                       std::string& out) {
    out.clear();
    if (!file.IsOpen())
        return false;

    constexpr std::size_t kMax = 0x7ff;
    bool readAny = false;
    while (out.size() < kMax) {
        const int raw = file.GetChar();
        if (file.Eof() != 0 && raw == 0) {
            break;
        }
        readAny = true;
        const char c = static_cast<char>(raw & 0xff);
        if (c == '#') {

            int d = c;
            while (d != '\n' && file.Eof() == 0)
                d = file.GetChar();
            break;
        }
        if (c == '\r')
            continue;
        if (c == '\n')
            break;
        out.push_back(c == '\t' ? ' ' : c);
    }

    while (!out.empty() && (out.back() == ' ' || out.back() == '\t'))
        out.pop_back();
    return readAny || !out.empty();
}

bool CrashdayDirectory::ReadConfigToken(CDFileOperations& file,
                                        std::string& out) {
    out.clear();
    if (!file.IsOpen())
        return false;

    constexpr std::size_t kMax = 0x7ff;
    bool readAny = false;
    while (out.size() < kMax) {
        const int raw = file.GetChar();
        if (file.Eof() != 0 && raw == 0)
            break;
        readAny = true;
        const char c = static_cast<char>(raw & 0xff);
        if (c == '\t' || c == ' ') {

            for (;;) {
                const int nextRaw = file.GetChar();
                if (file.Eof() != 0 && nextRaw == 0)
                    return readAny || !out.empty();
                const char next = static_cast<char>(nextRaw & 0xff);
                if (next == '#') {
                    int d = next;
                    while (d != '\n' && file.Eof() == 0)
                        d = file.GetChar();
                    return true;
                }
                if (next == '\n')
                    return true;
                if (next == '\r' || next == ' ' || next == '\t')
                    continue;
                (void)file.Seek(-1, SEEK_CUR);
                return true;
            }
        }
        if (c == '\r')
            continue;
        if (c == '\n')
            return true;
        if (c == '#') {
            int d = c;
            while (d != '\n' && file.Eof() == 0)
                d = file.GetChar();
            return true;
        }
        out.push_back(c);
    }
    return readAny || !out.empty();
}

}
