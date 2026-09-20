#pragma once
#include <cstdint>

namespace flydemo {

struct CD3DVECTOR {
    float x, y, z;
};
static_assert(sizeof(CD3DVECTOR) == 0x0C);

enum CarCamera : std::uint8_t {
    COCKPIT = 0,
    REAR    = 1,
    CUSTOM1 = 2,
    CUSTOM2 = 3,
};

}
