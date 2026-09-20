#pragma once

#include <array>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

#include "types.hpp"

namespace flydemo {

class CDFileOperations;

struct CarSpecs {
    std::string carFile;
    std::array<std::string, 5> resourceStrings{};

    std::int32_t dword1024 = 0;
    std::int32_t dword1028 = 0;
    std::uint8_t firstRaceAvailable = 0;
    std::uint8_t latestRaceAvailable = 0;
    bool canChangeColors = false;
    std::vector<std::string> colorTextures;

    std::uint16_t word1034 = 0;
    float maxSpeed = 0.0f;
    float driveCoefficient = 0.0f;
    float reverseBrakeCoefficient = 0.0f;
    std::uint8_t numGears = 0;
    std::uint8_t missileCapacity = 0;
    float spec1048 = 0.0f;
    float steeringRate = 0.0f;
    float tyreGrip = 0.0f;
    float springStiffness = 0.0f;
    float wheelOffset = 0.0f;
    std::int16_t lightGroup0Count = 0;
    std::int16_t lightGroup1Count = 0;

    CD3DVECTOR spoilerMount{};
    CD3DVECTOR upperWheelMount{};
    CD3DVECTOR lowerWheelMount{};
    CD3DVECTOR minigunMount{};
    CD3DVECTOR missileMount{};
    std::array<CD3DVECTOR, 4> cameraMounts{};
};

enum CarSpecsError : std::uint32_t {
    CARSPECS_DIAG_NONE                  = 0,
    CARSPECS_NO_HEADER      = 1u << 0,
    CARSPECS_TRUNCATED       = 1u << 1,
    CARSPECS_BAD_RACE_RANGE   = 1u << 2,
    CARSPECS_BAD_FIRST_RACE  = 1u << 3,
    CARSPECS_BAD_LAST_RACE  = 1u << 4,
    CARSPECS_BAD_COLOR_FLAG = 1u << 5,
    CARSPECS_NO_COLOR_TEX = 1u << 6,
    CARSPECS_BAD_GEAR_COUNT= 1u << 7,
    CARSPECS_BAD_TYRE_GRIP       = 1u << 8,
    CARSPECS_BAD_SPRING= 1u << 9,
    CARSPECS_FILE_NOT_FOUND        = 1u << 10,
};

struct CarSpecsParseResult {
    bool parsed = false;
    std::uint32_t diagnostics = CARSPECS_DIAG_NONE;
};

CarSpecsParseResult ParseCarSpecs(CDFileOperations& file,
                                      const std::string& carFile,
                                      CarSpecs& out);

CarSpecsParseResult LoadCarSpecs(const std::filesystem::path& gameRoot,
                                     const std::string& carFile,
                                     CarSpecs& out);

void ApplyCarSpecsToRaw(void* rawCar, const CarSpecs& specs);

}
