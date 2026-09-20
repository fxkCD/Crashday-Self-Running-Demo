#include <cmath>
#include <cstring>
#include "transform.hpp"

namespace flydemo {

using Wide = long double;

void SetIdentity(CD3DMATRIX& t) {
    std::memset(t.m, 0, sizeof(t.m));
    t.m[0] = t.m[5] = t.m[10] = t.m[15] = 1.0f;

}

void CopyTransform(CD3DMATRIX& dst, const CD3DMATRIX& src) {
    if (&dst != &src)
        std::memcpy(&dst, &src, sizeof(dst));
}

void ClearTranslation(CD3DMATRIX& t) {

    t.m[3] = t.m[7] = t.m[11] = 0.0f;
    t.m[12] = t.m[13] = t.m[14] = 0.0f;
    t.m[15] = 1.0f;
}

void ComposeTransform(CD3DMATRIX& dst, const CD3DMATRIX& rhs) {
    float result[16];
    for (unsigned row = 0; row < 4; ++row) {
        for (unsigned col = 0; col < 4; ++col) {

            const Wide p0 = Wide(dst.m[row * 4]) * rhs.m[col];
            const Wide p1 = Wide(dst.m[row * 4 + 1]) * rhs.m[4 + col];
            const Wide p2 = Wide(dst.m[row * 4 + 2]) * rhs.m[8 + col];
            const Wide p3 = Wide(dst.m[row * 4 + 3]) * rhs.m[12 + col];
            result[row * 4 + col] = static_cast<float>(((p0 + p1) + p2) + p3);
        }
    }

    std::memcpy(dst.m, result, sizeof(result));
}

void TransformPoint(CD3DMATRIX& t, CD3DVECTOR& point) {
    t.scratch[0] = point.x;
    t.scratch[1] = point.y;
    t.scratch[2] = point.z;
    const Wide x = t.scratch[0], y = t.scratch[1], z = t.scratch[2];
    t.scratch[3] = static_cast<float>(((x * t.m[3] + y * t.m[7])
                                      + z * t.m[11]) + t.m[15]);
    if (t.scratch[3] == 0.0f)
        return;
    t.scratch[3] = static_cast<float>(Wide(1) / t.scratch[3]);
    const Wide invW = t.scratch[3];
    point.x = static_cast<float>((((y * t.m[4] + x * t.m[0])
                                    + z * t.m[8]) + t.m[12]) * invW);
    point.y = static_cast<float>((((x * t.m[1] + y * t.m[5])
                                    + z * t.m[9]) + t.m[13]) * invW);
    point.z = static_cast<float>((((x * t.m[2] + y * t.m[6])
                                    + z * t.m[10]) + t.m[14]) * invW);
}

static void RotationTerms(float angle, float& sine, float& cosine) {
    const Wide radians = Wide(angle) * Wide(kRadiansPerAngleUnit);

    const double sine64 = static_cast<double>(std::sin(radians));
    sine = static_cast<float>(sine64);
    cosine = static_cast<float>(std::cos(radians));
}

void MakeXRotation(CD3DMATRIX& t, float angle) {
    SetIdentity(t);
    float s, c; RotationTerms(angle, s, c);
    t.m[5] = c; t.m[6] = s; t.m[9] = -s; t.m[10] = c;
}
void MakeYRotation(CD3DMATRIX& t, float angle) {
    SetIdentity(t);
    float s, c; RotationTerms(angle, s, c);
    t.m[0] = c; t.m[2] = -s; t.m[8] = s; t.m[10] = c;
}
void MakeZRotation(CD3DMATRIX& t, float angle) {
    SetIdentity(t);
    float s, c; RotationTerms(angle, s, c);
    t.m[0] = c; t.m[1] = s; t.m[4] = -s; t.m[5] = c;
}

void MakeProjection(CD3DMATRIX& t,
                               float fovAngleUnits,
                               float nearPlane,
                               float farPlane) {

    std::memset(t.m, 0, sizeof(t.m));

    const Wide halfRadians =
        Wide(fovAngleUnits) * Wide(0.5f) * Wide(kRadiansPerAngleUnit);
    const float cotangent = static_cast<float>(
        std::cos(halfRadians) / std::sin(halfRadians));

    const Wide far80 = Wide(farPlane);
    const Wide near80 = Wide(nearPlane);
    const Wide depthRatio = far80 / (far80 - near80);

    t.m[0] = cotangent;
    t.m[5] = cotangent;
    t.m[10] = static_cast<float>(depthRatio);
    t.m[11] = 1.0f;
    t.m[14] = static_cast<float>(-near80 * depthRatio);
}

CD3DVECTOR TransformVector(const void* matrix, const CD3DVECTOR& local) {
    CD3DMATRIX copy;
    std::memcpy(&copy, matrix, sizeof(copy));
    CD3DVECTOR result = local;
    TransformPoint(copy, result);
    return result;
}

}
