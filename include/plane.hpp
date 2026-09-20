#pragma once

#include "types.hpp"
#include <cstdint>

namespace flydemo {

enum class PlaneSide : std::uint8_t {
    Coplanar = 0,
    Negative = 1,
    Positive = 2,
    Spanning = 3
};

struct CD3DPLANE {
    float a = 0.0f;
    float b = 0.0f;
    float c = 0.0f;
    float d = 0.0f;

    static CD3DPLANE FromThreePoints(const CD3DVECTOR& p0, const CD3DVECTOR& p1, const CD3DVECTOR& p2);
    float Evaluate(const CD3DVECTOR& point) const;
    PlaneSide ClassifyPoint(const CD3DVECTOR& point) const;
    PlaneSide ClassifyTriangle(const CD3DVECTOR& p0, const CD3DVECTOR& p1, const CD3DVECTOR& p2) const;
    void OrientPlane(const CD3DVECTOR& referencePoint);
    bool IntersectSegment(const CD3DVECTOR& p0, const CD3DVECTOR& p1, CD3DVECTOR& intersection) const;
};
static_assert(sizeof(CD3DPLANE) == 0x10);

}
