#pragma once
#include "types.hpp"

namespace flydemo {

struct CD3DMATRIX {
    float m[16];
    float scratch[4];
};
static_assert(sizeof(CD3DMATRIX) == 0x50, "PE32 transform size");

constexpr double kRadiansPerAngleUnit = 0.02454369140625;
void SetIdentity(CD3DMATRIX& transform);
void CopyTransform(CD3DMATRIX& dst, const CD3DMATRIX& src);
void ClearTranslation(CD3DMATRIX& transform);
void ComposeTransform(CD3DMATRIX& dst, const CD3DMATRIX& rhs);
void TransformPoint(CD3DMATRIX& transform, CD3DVECTOR& point);
void MakeXRotation(CD3DMATRIX& transform, float angle);
void MakeYRotation(CD3DMATRIX& transform, float angle);
void MakeZRotation(CD3DMATRIX& transform, float angle);

void MakeProjection(CD3DMATRIX& transform,
                               float fovAngleUnits,
                               float nearPlane,
                               float farPlane);

CD3DVECTOR TransformVector(const void* matrix, const CD3DVECTOR& local);

}
