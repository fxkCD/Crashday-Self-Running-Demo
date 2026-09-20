#include <cassert>
#include <cstring>
#include <cerrno>
#include <cmath>
#include <cstdlib>
#include <cstdint>
#include <limits>
#include <string>
#include "types.hpp"
#include "car_specs.hpp"
#include "cdfileop.hpp"
#include "path.hpp"
#include "offsets.hpp"
#include "memory.hpp"

namespace flydemo {
using namespace flydemo;
using flydemo::mem::field;

struct CarSpecsLayout {
    static constexpr unsigned kCarFile              = off::CAR_FILE_STRING;
    static constexpr unsigned kFirstRaceAvailable   = off::FIRST_RACE_AVAILABLE;
    static constexpr unsigned kLatestRaceAvailable  = off::LATEST_RACE_AVAILABLE;
    static constexpr unsigned kCanChangeColors      = off::CAN_CHANGE_COLORS;
    static constexpr unsigned kColorTextureCount    = off::COLOR_TEXTURE_COUNT;
    static constexpr unsigned kColorTextureArray    = off::COLOR_TEXTURE_ARRAY;
    static constexpr unsigned kMaxSpeed             = off::MAX_SPEED;
    static constexpr unsigned kDriveCoefficient     = off::DRIVE_COEFFICIENT;
    static constexpr unsigned kReverseBrakeCoeff    = off::REVERSE_BRAKE_COEFF;
    static constexpr unsigned kNumGears             = off::NUM_GEARS;
    static constexpr unsigned kMissileCapacity      = off::MISSILE_CAPACITY;
    static constexpr unsigned kSteeringRate         = off::STEERING_RATE;
    static constexpr unsigned kTyreGrip             = off::TYRE_GRIP;
    static constexpr unsigned kSpringStiffness      = off::SPRING_STIFFNESS;
    static constexpr unsigned kWheelParam           = off::WHEEL_OFFSET;
    static constexpr unsigned kLightGroup0Count     = off::LIGHT_GROUP0_COUNT;
    static constexpr unsigned kLightGroup1Count     = off::LIGHT_GROUP1_COUNT;
    static constexpr unsigned kSpoilerMount         = off::SPOILER_MOUNT;
    static constexpr unsigned kUpperWheelMount      = off::WHEEL_MOUNT_UPPER;
    static constexpr unsigned kLowerWheelMount      = off::WHEEL_MOUNT_LOWER;
    static constexpr unsigned kMinigunMount         = off::MINIGUN_MOUNT;
    static constexpr unsigned kMissileMount     = off::MISSILE_MOUNT;
    static constexpr unsigned kCameraBase           = off::CAR_CAMERA_BASE;
};

namespace {

constexpr const char* kCarHeader = "Crashday-CarObject-File";
constexpr float kMinigunZAdjustment = -0.35f;

static int ParseInt(const std::string& text) {
    return std::atoi(text.c_str());
}

static float ParseFloat(const std::string& text) {

    return static_cast<float>(std::strtod(text.c_str(), nullptr));
}

static bool ReadLine(CDFileOperations& file, std::string& text,
                     std::uint32_t& diagnostics) {
    if (CrashdayDirectory::ReadConfigLine(file, text))
        return true;
    diagnostics |= CARSPECS_TRUNCATED;
    return false;
}

static bool ReadToken(CDFileOperations& file, std::string& text,
                      std::uint32_t& diagnostics) {
    if (CrashdayDirectory::ReadConfigToken(file, text))
        return true;
    diagnostics |= CARSPECS_TRUNCATED;
    return false;
}

static bool ReadIntLine(CDFileOperations& file, int& value,
                        std::uint32_t& diagnostics) {
    std::string text;
    if (!ReadLine(file, text, diagnostics))
        return false;
    value = ParseInt(text);
    return true;
}

static bool ReadFloatLine(CDFileOperations& file, float& value,
                          std::uint32_t& diagnostics) {
    std::string text;
    if (!ReadLine(file, text, diagnostics))
        return false;
    value = ParseFloat(text);
    return true;
}

static bool ReadVec3Tokens(CDFileOperations& file, CD3DVECTOR& value,
                           std::uint32_t& diagnostics) {
    std::string token;
    if (!ReadToken(file, token, diagnostics)) return false;
    value.x = ParseFloat(token);
    if (!ReadToken(file, token, diagnostics)) return false;
    value.y = ParseFloat(token);
    if (!ReadToken(file, token, diagnostics)) return false;
    value.z = ParseFloat(token);
    return true;
}

static bool InvalidUnitRange(float value) {
    if (!std::isnan(value) && value < 0.0f)
        return true;
    std::int32_t signedBits = 0;
    static_assert(sizeof(signedBits) == sizeof(value));
    std::memcpy(&signedBits, &value, sizeof(signedBits));
    return signedBits > static_cast<std::int32_t>(0x3f800000u);
}

}

CarSpecsParseResult ParseCarSpecs(CDFileOperations& file,
                                      const std::string& carFile,
                                      CarSpecs& out) {
    CarSpecsParseResult result{};
    out = CarSpecs{};
    out.carFile = carFile;

    if (!file.IsOpen()) {
        result.diagnostics |= CARSPECS_FILE_NOT_FOUND;
        return result;
    }

    std::string text;
    bool foundHeader = false;
    while (CrashdayDirectory::ReadConfigLine(file, text)) {
        if (text == kCarHeader) {
            foundHeader = true;
            break;
        }
        if (file.Eof() != 0)
            break;
    }
    if (!foundHeader) {
        result.diagnostics |= CARSPECS_NO_HEADER;
        return result;
    }

    for (auto& resource : out.resourceStrings) {
        if (!ReadLine(file, resource, result.diagnostics))
            return result;
    }

    int ivalue = 0;
    if (!ReadIntLine(file, ivalue, result.diagnostics)) return result;
    out.dword1024 = static_cast<std::int32_t>(ivalue);
    if (!ReadIntLine(file, ivalue, result.diagnostics)) return result;
    out.dword1028 = static_cast<std::int32_t>(ivalue);

    if (!ReadIntLine(file, ivalue, result.diagnostics)) return result;
    out.firstRaceAvailable = static_cast<std::uint8_t>(ivalue);
    if (!ReadIntLine(file, ivalue, result.diagnostics)) return result;
    out.latestRaceAvailable = static_cast<std::uint8_t>(ivalue);

    if (out.latestRaceAvailable < out.firstRaceAvailable)
        result.diagnostics |= CARSPECS_BAD_RACE_RANGE;
    if (out.firstRaceAvailable < 1u)
        result.diagnostics |= CARSPECS_BAD_FIRST_RACE;
    if (out.latestRaceAvailable > 24u)
        result.diagnostics |= CARSPECS_BAD_LAST_RACE;

    if (!ReadLine(file, text, result.diagnostics)) return result;
    if (text == "0")
        out.canChangeColors = false;
    else if (text == "1")
        out.canChangeColors = true;
    else {

        out.canChangeColors = false;
        result.diagnostics |= CARSPECS_BAD_COLOR_FLAG;
    }

    if (!ReadIntLine(file, ivalue, result.diagnostics)) return result;
    const auto colorCount = static_cast<std::uint8_t>(ivalue);
    out.colorTextures.reserve(colorCount);
    if (colorCount != 0u) {
        for (unsigned i = 0; i < colorCount; ++i) {
            if (!ReadToken(file, text, result.diagnostics)) return result;
            out.colorTextures.push_back(text);
        }
    } else {
        if (out.canChangeColors)
            result.diagnostics |= CARSPECS_NO_COLOR_TEX;

        if (!ReadLine(file, text, result.diagnostics)) return result;
    }

    if (!ReadIntLine(file, ivalue, result.diagnostics)) return result;
    out.word1034 = static_cast<std::uint16_t>(ivalue);
    if (!ReadFloatLine(file, out.maxSpeed, result.diagnostics)) return result;
    if (!ReadFloatLine(file, out.driveCoefficient, result.diagnostics)) return result;
    if (!ReadFloatLine(file, out.reverseBrakeCoefficient, result.diagnostics)) return result;

    if (!ReadIntLine(file, ivalue, result.diagnostics)) return result;
    out.numGears = static_cast<std::uint8_t>(ivalue);
    if (static_cast<std::int8_t>(out.numGears) <= 0)
        result.diagnostics |= CARSPECS_BAD_GEAR_COUNT;

    if (!ReadIntLine(file, ivalue, result.diagnostics)) return result;
    out.missileCapacity = static_cast<std::uint8_t>(ivalue);

    if (!ReadIntLine(file, ivalue, result.diagnostics)) return result;
    out.spec1048 = static_cast<float>(static_cast<std::int32_t>(ivalue));
    if (!ReadFloatLine(file, out.steeringRate, result.diagnostics)) return result;
    if (!ReadFloatLine(file, out.tyreGrip, result.diagnostics)) return result;
    if (InvalidUnitRange(out.tyreGrip))
        result.diagnostics |= CARSPECS_BAD_TYRE_GRIP;
    if (!ReadFloatLine(file, out.springStiffness, result.diagnostics)) return result;
    if (InvalidUnitRange(out.springStiffness))
        result.diagnostics |= CARSPECS_BAD_SPRING;
    if (!ReadFloatLine(file, out.wheelOffset, result.diagnostics)) return result;

    if (!ReadIntLine(file, ivalue, result.diagnostics)) return result;
    out.lightGroup0Count = static_cast<std::int16_t>(ivalue);
    if (!ReadIntLine(file, ivalue, result.diagnostics)) return result;
    out.lightGroup1Count = static_cast<std::int16_t>(ivalue);

    if (!ReadVec3Tokens(file, out.spoilerMount, result.diagnostics)) return result;
    if (!ReadVec3Tokens(file, out.upperWheelMount, result.diagnostics)) return result;
    if (!ReadVec3Tokens(file, out.lowerWheelMount, result.diagnostics)) return result;
    if (!ReadVec3Tokens(file, out.minigunMount, result.diagnostics)) return result;
    out.minigunMount.z += kMinigunZAdjustment;
    if (!ReadVec3Tokens(file, out.missileMount, result.diagnostics)) return result;
    for (auto& camera : out.cameraMounts) {
        if (!ReadVec3Tokens(file, camera, result.diagnostics)) return result;
    }

    result.parsed = true;
    return result;
}

CarSpecsParseResult LoadCarSpecs(const std::filesystem::path& gameRoot,
                                     const std::string& carFile,
                                     CarSpecs& out) {
    const auto path = gameRoot / "TRKDATA" / "CARS" / carFile;
    CDFileOperations file;
    CarSpecsParseResult result{};
    if (file.Open(path.string().c_str(), "rb") != 0) {
        result.diagnostics = CARSPECS_FILE_NOT_FOUND;
        out = CarSpecs{};
        out.carFile = carFile;
        return result;
    }
    result = ParseCarSpecs(file, carFile, out);
    (void)file.Close();
    return result;
}

void ApplyCarSpecsToRaw(void* rawCar, const CarSpecs& specs) {
    field<std::int32_t>(rawCar, off::SPECS_DWORD_1024) = specs.dword1024;
    field<std::int32_t>(rawCar, off::SPECS_DWORD_1028) = specs.dword1028;
    field<std::uint8_t>(rawCar, off::FIRST_RACE_AVAILABLE) = specs.firstRaceAvailable;
    field<std::uint8_t>(rawCar, off::LATEST_RACE_AVAILABLE) = specs.latestRaceAvailable;
    field<std::uint8_t>(rawCar, off::CAN_CHANGE_COLORS) = specs.canChangeColors ? 1u : 0u;
    field<std::uint8_t>(rawCar, off::COLOR_TEXTURE_COUNT) =
        static_cast<std::uint8_t>(specs.colorTextures.size());
    field<std::uint16_t>(rawCar, off::SPECS_WORD_1034) = specs.word1034;
    field<float>(rawCar, off::MAX_SPEED) = specs.maxSpeed;
    field<float>(rawCar, off::DRIVE_COEFFICIENT) = specs.driveCoefficient;
    field<float>(rawCar, off::REVERSE_BRAKE_COEFF) = specs.reverseBrakeCoefficient;
    field<std::uint8_t>(rawCar, off::NUM_GEARS) = specs.numGears;
    field<std::uint8_t>(rawCar, off::MISSILE_CAPACITY) = specs.missileCapacity;
    field<float>(rawCar, off::SPECS_FLOAT_1048) = specs.spec1048;
    field<float>(rawCar, off::STEERING_RATE) = specs.steeringRate;
    field<float>(rawCar, off::TYRE_GRIP) = specs.tyreGrip;
    field<float>(rawCar, off::SPRING_STIFFNESS) = specs.springStiffness;
    field<float>(rawCar, off::WHEEL_OFFSET) = specs.wheelOffset;
    field<std::int16_t>(rawCar, off::LIGHT_GROUP0_COUNT) = specs.lightGroup0Count;
    field<std::int16_t>(rawCar, off::LIGHT_GROUP1_COUNT) = specs.lightGroup1Count;
    field<CD3DVECTOR>(rawCar, off::SPOILER_MOUNT) = specs.spoilerMount;
    field<CD3DVECTOR>(rawCar, off::WHEEL_MOUNT_UPPER) = specs.upperWheelMount;
    field<CD3DVECTOR>(rawCar, off::WHEEL_MOUNT_LOWER) = specs.lowerWheelMount;
    field<CD3DVECTOR>(rawCar, off::MINIGUN_MOUNT) = specs.minigunMount;
    field<CD3DVECTOR>(rawCar, off::MISSILE_MOUNT) = specs.missileMount;
    for (unsigned i = 0; i < specs.cameraMounts.size(); ++i)
        field<CD3DVECTOR>(rawCar, off::CAR_CAMERA_BASE + i * 12u) = specs.cameraMounts[i];
}

static inline int NumGears(const void* car) {
    return static_cast<int>(field<std::int8_t>(car, off::NUM_GEARS));
}

float Car_GetStageSpeed(void* car, int gear) {
    const int numGears = NumGears(car);
    assert(gear >= -1 && gear <= numGears);
    assert(numGears > 0);

    if (gear == 0)
        return 0.0f;

    const float maxSpeed = field<float>(car, off::MAX_SPEED);
    if (gear == -1)
        return -maxSpeed / static_cast<float>(numGears);

    return (maxSpeed / static_cast<float>(numGears)) * static_cast<float>(gear);
}

float Car_GetDriveCoefficient(void* car, int gear) {
    const int numGears = NumGears(car);
    assert(gear >= -1 && gear <= numGears);
    assert(numGears > 0);

    const float base = field<float>(car, off::DRIVE_COEFFICIENT);
    if (gear == -1 || gear == 0)
        return base;

    const float scale = static_cast<float>(numGears - gear + 1)
                      / static_cast<float>(numGears);
    return scale * base;
}

float Car_ReverseBrake(void* car) {
    return field<float>(car, off::REVERSE_BRAKE_COEFF);
}

bool Car_CanChangeColors(const void* car) {
    return field<std::uint8_t>(car, off::CAN_CHANGE_COLORS) != 0;
}

std::uint8_t Car_GetNumGears(const void* car) {
    return field<std::uint8_t>(car, off::NUM_GEARS);
}

std::uint8_t Car_GetMissileCapacity(const void* car) {
    return field<std::uint8_t>(car, off::MISSILE_CAPACITY);
}

float Car_GetSpec1048(const void* car) { return field<float>(car, off::SPECS_FLOAT_1048); }
float Car_GetSteeringRate(const void* car) { return field<float>(car, off::STEERING_RATE); }
float Car_GetTyreGrip(const void* car) { return field<float>(car, off::TYRE_GRIP); }
float Car_GetSpringStiffness(const void* car) { return field<float>(car, off::SPRING_STIFFNESS); }
float Car_GetWheelParam1058(const void* car) { return field<float>(car, off::WHEEL_OFFSET); }
std::int16_t Car_GetLightGroup0Count(const void* car) { return field<std::int16_t>(car, off::LIGHT_GROUP0_COUNT); }
std::int16_t Car_GetLightGroup1Count(const void* car) { return field<std::int16_t>(car, off::LIGHT_GROUP1_COUNT); }

CD3DVECTOR Car_GetSpoilerMount(const void* car) {
    return field<CD3DVECTOR>(car, off::SPOILER_MOUNT);
}

enum WheelPosition : int {
    UPPERLEFT  = 0,
    UPPERRIGHT = 1,
    LOWERLEFT  = 2,
    LOWERRIGHT = 3,
};

static CD3DVECTOR MirrorX(CD3DVECTOR v) {
    v.x = -v.x;
    return v;
}

CD3DVECTOR Car_GetWheelMount(const void* car, int wpos) {
    assert(wpos >= UPPERLEFT && wpos <= LOWERRIGHT);

    switch (wpos) {
    case UPPERLEFT:
        return field<CD3DVECTOR>(car, off::WHEEL_MOUNT_UPPER);
    case UPPERRIGHT:
        return MirrorX(field<CD3DVECTOR>(car, off::WHEEL_MOUNT_UPPER));
    case LOWERLEFT:
        return MirrorX(field<CD3DVECTOR>(car, off::WHEEL_MOUNT_LOWER));
    case LOWERRIGHT:
        return field<CD3DVECTOR>(car, off::WHEEL_MOUNT_LOWER);
    default:
        return CD3DVECTOR{0.0f, 0.0f, 0.0f};
    }
}

CD3DVECTOR Car_GetMinigunMount(const void* car) {
    return field<CD3DVECTOR>(car, off::MINIGUN_MOUNT);
}

CD3DVECTOR Car_GetMissileMount(const void* car) {
    return field<CD3DVECTOR>(car, off::MISSILE_MOUNT);
}

CD3DVECTOR Car_GetCameraMount(const void* car, CarCamera cam) {
    assert(cam >= COCKPIT && cam <= CUSTOM2);
    return field<CD3DVECTOR>(car, off::CAR_CAMERA_BASE + static_cast<unsigned>(cam) * 12u);
}

}
