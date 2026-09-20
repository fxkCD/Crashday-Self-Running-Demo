#pragma once
#include <cstddef>
#include <cstdint>
#include <string_view>
#include "types.hpp"
#include "dynamic_object.hpp"

namespace flydemo {

struct CarSpecs;

struct CarCreateParams {
    std::uint32_t color = 0x00ffffffu;
    int spoiler = 0;
    int missileMode = 0;
    int minigun = 0;
    int afterburnerVariant = -1;
};

struct CarLightState {
    std::uint32_t baseColor = 0;
    float intensity = 0.0f;
};

inline std::uint32_t Car_ScaleLight(std::uint32_t rgb, float intensity) {
    const auto channel = [intensity](unsigned value) {
        return static_cast<std::uint32_t>(
            static_cast<double>(value) * static_cast<double>(intensity)) & 0xffu;
    };
    return (channel((rgb >> 16) & 0xffu) << 16u) |
           (channel((rgb >> 8) & 0xffu) << 8u) |
           channel(rgb & 0xffu);
}

inline void Car_UpdateLightStates(CarLightState* states,
                                                std::int16_t group0Count,
                                                std::int16_t group1Count,
                                                bool lightsActive,
                                                bool reverseActive,
                                                float speed,
                                                float dt) {
    const int group0 = static_cast<int>(group0Count);
    const int total = group0 + static_cast<int>(group1Count);
    if (lightsActive) {
        const float rise = dt * 10.0f;
        for (int i = 0; i < group0; ++i)
            if (states[i].intensity < 1.0f)
                states[i].intensity = states[i].intensity + rise > 1.0f
                    ? 1.0f : states[i].intensity + rise;
    } else {
        const float fall = dt * 4.0f;
        for (int i = 0; i < group0; ++i)
            if (states[i].intensity > 0.0f)
                states[i].intensity = states[i].intensity - fall < 0.0f
                    ? 0.0f : states[i].intensity - fall;
    }

    if (reverseActive && speed > 0.0f) {
        for (int i = group0; i < total; ++i)
            states[i].intensity = 1.0f;
    } else {
        const float fall = dt * 6.0f;
        for (int i = group0; i < total; ++i)
            if (states[i].intensity > 0.0f)
                states[i].intensity = states[i].intensity - fall < 0.0f
                    ? 0.0f : states[i].intensity - fall;
    }
}

struct CarObjectSizes {
    static constexpr std::size_t Car = 0x113Cu;
    static constexpr std::size_t Wheel = 0x1078u;
    static constexpr std::size_t Spoiler = 0x0FC4u;
    static constexpr std::size_t Minigun = 0x0FE0u;
    static constexpr std::size_t Afterburner = 0x0118u;
};

class CarCreateIO {
public:
    virtual ~CarCreateIO() = default;

    virtual void ConstructBaseCar(void* car, std::string_view objectName,
                                  std::string_view carFile) = 0;
    virtual void LoadSpecs(void* car, std::string_view carFile,
                           CarSpecs& specs) = 0;
    virtual void InvalidSpoilerSelector(int value) = 0;
    virtual void InvalidMinigunSelector(int value) = 0;

    virtual void* CreateWheel(std::size_t nativeBytes, std::string_view objectName,
                              std::string_view modelFile, void* ownerCar,
                              bool mirrored) = 0;
    virtual void* CreateSpoiler(std::size_t nativeBytes, std::string_view objectName,
                                std::string_view modelFile) = 0;
    virtual void* CreateMinigun(std::size_t nativeBytes, std::string_view objectName,
                                void* ownerCar, int side) = 0;
    virtual void* CreateAfterburner(std::size_t nativeBytes,
                                    std::string_view objectName, int variant) = 0;

    virtual void SetObjectPosition(void* object, const CD3DVECTOR& position) = 0;
    virtual void TranslateObject(void* object, const CD3DVECTOR& delta) = 0;

    virtual CarLightState* AllocateLightStates(std::size_t count) = 0;
    virtual std::uint32_t MaterialBaseColor(void* car, std::size_t index) = 0;
};

class CarDestroyIO {
public:
    virtual ~CarDestroyIO() = default;
    virtual void DestroyColorTextureArray(void* arrayPointer) = 0;
    virtual void DestroyLightStateTable(void* tablePointer) = 0;
    virtual void DestroyEmbeddedString(void* car, unsigned offset) = 0;
    virtual void DestroyBaseCar(void* car) = 0;
};

void Car_Construct(void* rawCar, std::string_view objectName,
                               std::string_view carFile,
                               const CarCreateParams& options,
                               CarCreateIO& backend);
void Car_Destruct(void* rawCar, CarDestroyIO& backend);

class CD3DCAROBJECT : public CD3DDYNAMICOBJECT {
    alignas(4) std::byte carData_[CarObjectSizes::Car - sizeof(CD3DDYNAMICOBJECT)]{};
public:
    void Forward(float rate);
    void Reverse(float rate);
    void Brake();
    void Left(float rate);
    void Right(float rate);
    bool FireMissile();
    bool FireMinigun();
    bool UseAfterburner();
    void ToggleLights();
    void SetLights(std::uint8_t state);
    void ApplyConditionDamage(float amount);
    void SetColor(std::uint32_t color);

    void Update(float dt);
    void UpdateWheelSpeeds(float dt);
    void UpdateGear();
    void UpdateWheels(float dt);
    void UpdateSpoilerMount();
    void UpdateMinigunMount();
    void UpdateLightMaterials(float dt);
    void* GetWheel(int wheel);
    CD3DVECTOR GetCameraPosition(CarCamera cam) const;
    std::uint32_t GetColor() const;
    std::uint8_t GetMissileMode() const;
    float GetCondition() const;
    std::uint8_t GetMissileCount() const;
    void SetMissileCount(std::uint8_t count);
    bool GetLightsState() const;
    void* GetSpoiler() const;
    void* GetMinigun() const;
    void* GetAfterburner() const;
};
static_assert(sizeof(CD3DCAROBJECT) == CarObjectSizes::Car);

void Car_ApplyTyrePhysics(void* car);
}
