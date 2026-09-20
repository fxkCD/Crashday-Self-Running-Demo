#pragma once
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <string>

namespace flydemo {

class CDFileOperations {
public:
    int Open(const char* filename, const char* mode, std::uint8_t flag = 0);
    int Close();
    int Seek(long offset, int origin);
    long Tell() const;
    std::FILE* Handle() const { return file_; }
    int Read(void* dst, std::size_t size, std::size_t count);
    int Write(const void* src, std::size_t size, std::size_t count);
    int GetChar();
    int PutChar(std::uint8_t value);
    int Eof() const;
    int Error() const;

    bool IsOpen() const { return file_ != nullptr; }
    const std::string& Filename() const { return filename_; }
    std::uint8_t Flag() const { return flag_; }

private:
    std::FILE* file_ = nullptr;
    std::string filename_;
    std::uint8_t flag_ = 0;
};

}
