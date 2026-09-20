#include "afterburner.hpp"
#include "memory.hpp"
#include <cstdint>
#include <cstring>
#include <limits>
#include <string>
#include <string_view>

namespace flydemo {
using flydemo::mem::field;

namespace {
std::int32_t signedFloatBits(float value) {
    std::int32_t bits = 0;
    static_assert(sizeof(bits) == sizeof(value), "binary32 required");
    std::memcpy(&bits, &value, sizeof(bits));
    return bits;
}

float x87MulAdd(float a, float b, float c) {

    return static_cast<float>(static_cast<long double>(a) *
                              static_cast<long double>(b) +
                              static_cast<long double>(c));
}

class DbCursor {
public:
    explicit DbCursor(std::string_view bytes) : bytes_(bytes) {}

    bool ReadLine(std::string& out) {
        out.clear();
        std::size_t written = 0;
        bool sawAny = false;
        while (written < 0x7ff) {
            char ch = 0;
            if (!ReadChar(ch))
                return sawAny;
            sawAny = true;
            if (ch == '\t')
                ch = ' ';
            if (ch == '#') {
                while (ReadChar(ch) && ch != '\n') {}
                TrimSpaceTab(out);
                return true;
            }
            if (ch == '\n') {

                if (!out.empty())
                    out.pop_back();
                TrimSpaceTab(out);
                return true;
            }
            out.push_back(ch);
            ++written;
        }

        return true;
    }

    bool ReadToken(std::string& out) {
        out.clear();
        std::size_t written = 0;
        while (written < 0x7ff) {
            char ch = 0;
            if (!ReadChar(ch))
                return written != 0;
            if (ch == '\t')
                ch = ' ';

            if (ch == ' ' || ch == '\t') {

                if (!SkipTokenDelimiter())
                    return true;
                return true;
            }
            if (ch == '\n')
                return true;
            if (ch == '#') {
                SkipComment();
                return true;
            }
            out.push_back(ch);
            ++written;
        }
        return true;
    }

private:
    bool ReadChar(char& out) {
        if (pos_ >= bytes_.size())
            return false;
        out = bytes_[pos_++];
        return true;
    }

    void SeekBackOne() {
        if (pos_ != 0)
            --pos_;
    }

    void SkipComment() {
        char ch = 0;
        while (ReadChar(ch) && ch != '\n') {}
    }

    bool SkipTokenDelimiter() {
        char ch = 0;
        while (ReadChar(ch)) {
            if (ch == '#') {
                SkipComment();
                return true;
            }
            if (ch == ' ' || ch == '\t')
                continue;
            if (ch == '\n')
                return true;
            SeekBackOne();
            return true;
        }
        return false;
    }

    static void TrimSpaceTab(std::string& s) {
        while (!s.empty() && (s.back() == ' ' || s.back() == '\t'))
            s.pop_back();
    }

    std::string_view bytes_;
    std::size_t pos_ = 0;
};

std::int32_t ParseInt(std::string_view s) {
    std::size_t i = 0;

    while (i < s.size()) {
        const unsigned char c = static_cast<unsigned char>(s[i]);
        if (!(c == ' ' || c == '\t' || c == '\n' || c == '\r' ||
              c == '\f' || c == '\v'))
            break;
        ++i;
    }
    bool negative = false;
    if (i < s.size() && (s[i] == '+' || s[i] == '-')) {
        negative = s[i] == '-';
        ++i;
    }
    std::uint32_t acc = 0;
    while (i < s.size() && s[i] >= '0' && s[i] <= '9') {

        acc = acc * 10u + static_cast<std::uint32_t>(s[i] - '0');
        ++i;
    }
    if (negative)
        acc = 0u - acc;
    std::int32_t result = 0;
    std::memcpy(&result, &acc, sizeof(result));
    return result;
}
}

bool Afterburner_ParseDB(std::string_view bytes,
                                   AfterburnerType type,
                                   AfterburnerDbRecord& out) {
    out = {};
    DbCursor cursor(bytes);
    std::string text;

    if (!cursor.ReadLine(text))
        return false;
    if (!cursor.ReadLine(text))
        return false;
    if (ParseInt(text) != kAfterburnerCount)
        out.diagnostics |= Afterburner_BadCount;

    const auto wanted = static_cast<std::int32_t>(static_cast<std::int8_t>(type)) + 1;
    for (;;) {
        if (!cursor.ReadToken(text))
            return false;
        if (text != "Next:")
            continue;
        if (!cursor.ReadToken(text))
            return false;
        if (ParseInt(text) != wanted)
            continue;
        break;
    }

    if (!cursor.ReadLine(out.label))
        return false;
    if (!cursor.ReadLine(text))
        return false;
    const std::int32_t useful = ParseInt(text);
    if (useful != 0 && useful != 1)
        out.diagnostics |= Afterburner_BadAirFlag;
    out.info.usefulInAir = static_cast<std::uint8_t>(useful);

    auto readIntegerFloat = [&](float& fieldOut) {
        if (!cursor.ReadLine(text))
            return false;

        fieldOut = static_cast<float>(static_cast<long double>(ParseInt(text)));
        return true;
    };
    return readIntegerFloat(out.info.force) &&
           readIntegerFloat(out.info.heatUpRate) &&
           readIntegerFloat(out.info.coolDownRate);
}

void Afterburner_Init(void* object, AfterburnerType type,
                                   const AfterburnerInfo& info) {
    const auto rawType = static_cast<std::uint8_t>(type);

    field<std::uint8_t>(object, afterburner_off::Type) = rawType;
    field<std::uint8_t>(object, afterburner_off::UsefulInAir) = info.usefulInAir;
    field<float>(object, afterburner_off::Force) = info.force;
    field<float>(object, afterburner_off::HeatUpRate) = info.heatUpRate;
    field<float>(object, afterburner_off::CoolDownRate) = info.coolDownRate;
    field<std::uint8_t>(object, afterburner_off::Requested) = 0;
    field<float>(object, afterburner_off::Heat) = 0.0f;
    field<std::uint8_t>(object, afterburner_off::Locked) = 0;
}

bool Afterburner_RequestUse(void* object) {

    if (field<std::uint8_t>(object, afterburner_off::Locked) == 1)
        return false;
    field<std::uint8_t>(object, afterburner_off::Requested) = 1;
    return true;
}

void Afterburner_Update(void* object, float dt) {
    auto& heat = field<float>(object, afterburner_off::Heat);
    auto& locked = field<std::uint8_t>(object, afterburner_off::Locked);
    const bool requested = field<std::uint8_t>(object, afterburner_off::Requested) != 0;

    if (requested) {
        heat = x87MulAdd(field<float>(object, afterburner_off::HeatUpRate), dt, heat);

        if (signedFloatBits(heat) > signedFloatBits(100.0f)) {
            heat = 100.0f;
            locked = 1;
        }
    } else {
        heat = x87MulAdd(-field<float>(object, afterburner_off::CoolDownRate), dt, heat);

        if (heat < 0.0f) {
            heat = 0.0f;
            locked = 0;
        }

        if (signedFloatBits(heat) < signedFloatBits(50.0f))
            locked = 0;
    }

    field<std::uint8_t>(object, afterburner_off::Requested) = 0;
}

AfterburnerInfo Afterburner_GetInfo(const void* object) {
    AfterburnerInfo info;
    info.usefulInAir = field<std::uint8_t>(object, afterburner_off::UsefulInAir);
    info.force = field<float>(object, afterburner_off::Force);
    info.heatUpRate = field<float>(object, afterburner_off::HeatUpRate);
    info.coolDownRate = field<float>(object, afterburner_off::CoolDownRate);
    return info;
}

AfterburnerType Afterburner_GetType(const void* object) {
    return static_cast<AfterburnerType>(field<std::uint8_t>(object, afterburner_off::Type));
}

float Afterburner_GetHeat(const void* object) {
    return field<float>(object, afterburner_off::Heat);
}

bool Afterburner_IsLocked(const void* object) {
    return field<std::uint8_t>(object, afterburner_off::Locked) == 1;
}

}
