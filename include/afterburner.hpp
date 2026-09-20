#pragma once
#include <cstdint>
#include <string>
#include <string_view>

namespace flydemo {

enum class AfterburnerType : std::uint8_t {
    Nitro = 0,
    F14Engine = 1,
    Particler = 2,
};

struct AfterburnerInfo {
    std::uint8_t usefulInAir = 0;
    float force = 0.0f;
    float heatUpRate = 0.0f;
    float coolDownRate = 0.0f;
};

inline constexpr std::string_view kAfterburnerDbDirectory = "TRKDATA/CARS/";
inline constexpr std::string_view kAfterburnerDbFilename = "aftbur.dbs";
inline constexpr std::string_view kAfterburnerDbOpenMode = "r+";
inline constexpr int kAfterburnerCount = 3;

enum AfterburnerDiag : std::uint32_t {
    AfterburnerDiag_None = 0,

    Afterburner_BadCount = 1u << 0,

    Afterburner_BadAirFlag = 1u << 1,
};

struct AfterburnerDbRecord {
    std::string label;
    AfterburnerInfo info{};
    std::uint32_t diagnostics = AfterburnerDiag_None;
};

bool Afterburner_ParseDB(std::string_view bytes,
                                   AfterburnerType type,
                                   AfterburnerDbRecord& out);

namespace afterburner_off {
constexpr unsigned Type = 0x00E8;
constexpr unsigned Info = 0x00EC;
constexpr unsigned UsefulInAir = 0x00FC;
constexpr unsigned Force = 0x0100;
constexpr unsigned HeatUpRate = 0x0104;
constexpr unsigned CoolDownRate = 0x0108;
constexpr unsigned Requested = 0x010C;
constexpr unsigned Heat = 0x0110;
constexpr unsigned Locked = 0x0114;
constexpr unsigned ObjectSize = 0x0118;
}

void Afterburner_Init(void* object, AfterburnerType type,
                                   const AfterburnerInfo& info);

bool Afterburner_RequestUse(void* object);

void Afterburner_Update(void* object, float dt);

AfterburnerInfo Afterburner_GetInfo(const void* object);
AfterburnerType Afterburner_GetType(const void* object);
float Afterburner_GetHeat(const void* object);
bool Afterburner_IsLocked(const void* object);

}
