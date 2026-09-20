#pragma once
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace flydemo {

class CarColorIO {
public:
    virtual ~CarColorIO() = default;

    virtual std::string_view CarName(void* car) const = 0;
    virtual std::string_view ColorTextureName(void* car,
                                                     std::size_t index) const = 0;

    virtual void LoadTextureAs(std::string_view sourceName,
                               std::string_view aliasName) = 0;

    virtual void ReplaceObjectTexture(void* object,
                                      std::string_view sourceName,
                                      std::string_view aliasName) = 0;

    virtual void RecolorTexture(std::string_view aliasName,
                                std::uint32_t rgb) = 0;
};

void Car_SetColorIO(CarColorIO* backend);
CarColorIO* Car_GetColorIO();

}
