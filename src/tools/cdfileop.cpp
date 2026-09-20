#include "cdfileop.hpp"

namespace flydemo {

int CDFileOperations::Open(const char* filename, const char* mode,
                           std::uint8_t flag) {
    if (file_ != nullptr)
        return -1;
    if (!filename || !mode)
        return -1;

    file_ = std::fopen(filename, mode);
    if (!file_)
        return -1;

    filename_ = filename;
    flag_ = flag;
    return 0;
}

int CDFileOperations::Close() {
    if (!file_)
        return -1;
    const int result = std::fclose(file_);
    file_ = nullptr;
    filename_.clear();
    flag_ = 0;
    return result == 0 ? 0 : -1;
}

int CDFileOperations::Seek(long offset, int origin) {
    if (!file_)
        return -1;

    (void)std::fseek(file_, offset, origin);
    return 0;
}

long CDFileOperations::Tell() const {
    if (!file_)
        return -1;
    return std::ftell(file_);
}

int CDFileOperations::Read(void* dst, std::size_t size, std::size_t count) {
    if (!file_)
        return -1;
    (void)std::fread(dst, size, count, file_);
    return 0;
}

int CDFileOperations::Write(const void* src, std::size_t size,
                            std::size_t count) {
    if (!file_)
        return -1;
    (void)std::fwrite(src, size, count, file_);
    return 0;
}

int CDFileOperations::GetChar() {
    std::uint8_t value = 0;
    if (Read(&value, 1, 1) != 0)
        return 0;
    return static_cast<int>(value);
}

int CDFileOperations::PutChar(std::uint8_t value) {
    return Write(&value, 1, 1) == 0 ? 0 : -1;
}

int CDFileOperations::Eof() const {
    if (!file_)
        return -1;

    return std::feof(file_) ? 0x10 : 0;
}

int CDFileOperations::Error() const {
    if (!file_)
        return -1;

    return std::ferror(file_) ? 0x20 : 0;
}

}
