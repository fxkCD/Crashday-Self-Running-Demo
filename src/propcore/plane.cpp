#include "plane.hpp"

#include <cmath>
#include <limits>
#include <stdexcept>

namespace flydemo {
namespace {
constexpr double kPlaneEpsilon = 0.001;

double EvalWide(const CD3DPLANE& p, const CD3DVECTOR& v) {

    return static_cast<double>(p.a) * static_cast<double>(v.x) +
           static_cast<double>(p.b) * static_cast<double>(v.y) +
           static_cast<double>(p.c) * static_cast<double>(v.z) -
           static_cast<double>(p.d);
}

PlaneSide ClassifyWide(double distance) {

    if (std::isnan(distance)) return PlaneSide::Negative;
    if (distance < -kPlaneEpsilon) return PlaneSide::Negative;
    if (distance <= kPlaneEpsilon) return PlaneSide::Coplanar;
    return PlaneSide::Positive;
}
}

CD3DPLANE CD3DPLANE::FromThreePoints(const CD3DVECTOR& p0, const CD3DVECTOR& p1, const CD3DVECTOR& p2) {
    const double uX = static_cast<double>(p1.x) - p0.x;
    const double uY = static_cast<double>(p1.y) - p0.y;
    const double uZ = static_cast<double>(p1.z) - p0.z;
    const double vX = static_cast<double>(p2.x) - p0.x;
    const double vY = static_cast<double>(p2.y) - p0.y;
    const double vZ = static_cast<double>(p2.z) - p0.z;

    double nX = uY * vZ - uZ * vY;
    double nY = uZ * vX - uX * vZ;
    double nZ = uX * vY - uY * vX;
    const double length = std::sqrt(nX*nX + nY*nY + nZ*nZ);
    if (!(length > 0.0) || !std::isfinite(length))
        throw std::invalid_argument("CD3DPLANE requires three non-collinear finite points");

    nX /= length; nY /= length; nZ /= length;
    CD3DPLANE out;
    out.a = static_cast<float>(nX);
    out.b = static_cast<float>(nY);
    out.c = static_cast<float>(nZ);
    out.d = static_cast<float>(static_cast<double>(out.a)*p0.x +
                               static_cast<double>(out.b)*p0.y +
                               static_cast<double>(out.c)*p0.z);
    return out;
}

float CD3DPLANE::Evaluate(const CD3DVECTOR& point) const {
    return static_cast<float>(EvalWide(*this, point));
}

PlaneSide CD3DPLANE::ClassifyPoint(const CD3DVECTOR& point) const {
    return ClassifyWide(EvalWide(*this, point));
}

PlaneSide CD3DPLANE::ClassifyTriangle(const CD3DVECTOR& p0, const CD3DVECTOR& p1, const CD3DVECTOR& p2) const {
    bool anyNegative = false;
    bool anyNonNegative = false;
    const CD3DVECTOR* points[] = {&p0, &p1, &p2};
    for (const CD3DVECTOR* point : points) {
        if (ClassifyPoint(*point) == PlaneSide::Negative) anyNegative = true;
        else anyNonNegative = true;
    }
    if (anyNegative && anyNonNegative) return PlaneSide::Spanning;
    if (anyNegative) return PlaneSide::Negative;

    return PlaneSide::Positive;
}

void CD3DPLANE::OrientPlane(const CD3DVECTOR& referencePoint) {
    if (ClassifyPoint(referencePoint) == PlaneSide::Positive) {
        a = -a; b = -b; c = -c; d = -d;
    }
}

bool CD3DPLANE::IntersectSegment(const CD3DVECTOR& p0, const CD3DVECTOR& p1, CD3DVECTOR& intersection) const {
    const double d0 = EvalWide(*this, p0);
    const double d1 = EvalWide(*this, p1);
    if (!std::isfinite(d0) || !std::isfinite(d1)) return false;

    if ((d0 < 0.0 && d1 < 0.0) || (d0 > 0.0 && d1 > 0.0)) return false;
    const double denom = d0 - d1;
    if (denom == 0.0) return false;
    const double t = d0 / denom;
    intersection.x = static_cast<float>(p0.x + (static_cast<double>(p1.x)-p0.x)*t);
    intersection.y = static_cast<float>(p0.y + (static_cast<double>(p1.y)-p0.y)*t);
    intersection.z = static_cast<float>(p0.z + (static_cast<double>(p1.z)-p0.z)*t);
    return true;
}

}
