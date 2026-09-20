#include "lighting.hpp"
#include <cmath>

namespace flydemo {
namespace {
bool Fail(std::string* e, const char* msg) { if (e) *e = msg; return false; }
}

bool CD3DLIGHT::Configure(const CD3DVECTOR& p, float newRange, std::uint32_t color,
                            int useCoronas, int useLensFlares, int useLightUp,
                            std::string* error) {
    if (newRange < 0.0f) return Fail(error, "range must be >= 0");
    if ((useCoronas != 0 && useCoronas != 1) ||
        (useLensFlares != 0 && useLensFlares != 1) ||
        (useLightUp != 0 && useLightUp != 1))
        return Fail(error, "light flags must be 0 or 1");
    position = p;
    range = newRange;
    packedColor = color;
    coronas = useCoronas != 0;
    lensFlares = useLensFlares != 0;
    lightUp = useLightUp != 0;
    if (error) error->clear();
    return true;
}

float CD3DLIGHT::RecomputeVectorLength() {
    cachedVectorLength = std::sqrt(position.x*position.x + position.y*position.y +
                                   position.z*position.z);
    return cachedVectorLength;
}

}
