#pragma once
#include "types.hpp"

namespace flydemo {

class CD3DCAMERA {
public:
    CD3DCAMERA();

    void SetPosition(const CD3DVECTOR& p);
    void SetPosition(float x, float y, float z);
    void SetRotation(float x, float y, float z);
    void SetRotationX(float x);
    void SetRotationY(float y);
    void SetRotationZ(float z);
    void LookAt(const CD3DVECTOR& target);
    void SetFov(float fov);

    const CD3DVECTOR& GetPosition() const { return position_; }
    float GetFov() const { return fov_; }
    const CD3DVECTOR& GetRotation() const { return rotation_; }
    float GetRotationX() const { return rotation_.x; }
    float GetRotationY() const { return rotation_.y; }
    float GetRotationZ() const { return rotation_.z; }

    static float NormalizeAngle256(float angle);

private:
    float fov_ = 64.0f;
    CD3DVECTOR position_{0.0f, 0.0f, 0.0f};
    CD3DVECTOR rotation_{0.0f, 0.0f, 0.0f};
};

}
