#include "camera.hpp"

#include <cmath>

namespace flydemo {
namespace {

constexpr double kInvTwoPi = 0.15915495087284554;
constexpr double kTurnUnits = 256.0;

float DirectionAngle(float y, float x) {
    const double radians = std::atan2(static_cast<double>(y), static_cast<double>(x));
    return static_cast<float>((radians * kInvTwoPi) * kTurnUnits);
}

}

CD3DCAMERA::CD3DCAMERA() {

    fov_ = 64.0f;
    position_ = {0.0f, 0.0f, 0.0f};
    rotation_ = {0.0f, 0.0f, 0.0f};
}

float CD3DCAMERA::NormalizeAngle256(float angle) {

    if (std::isnan(angle))
        return angle;
    while (angle < 0.0f)
        angle += 256.0f;
    while (angle >= 256.0f)
        angle -= 256.0f;
    return angle;
}

void CD3DCAMERA::SetPosition(const CD3DVECTOR& p) {

    position_ = p;
}

void CD3DCAMERA::SetPosition(float x, float y, float z) {

    position_ = {x, y, z};
}

void CD3DCAMERA::SetRotation(float x, float y, float z) {
    rotation_.x = NormalizeAngle256(x);
    rotation_.y = NormalizeAngle256(y);
    rotation_.z = NormalizeAngle256(z);
}

void CD3DCAMERA::SetRotationX(float x) {

    SetRotation(x, rotation_.y, rotation_.z);
}

void CD3DCAMERA::SetRotationY(float y) {

    SetRotation(rotation_.x, y, rotation_.z);
}

void CD3DCAMERA::SetRotationZ(float z) {

    SetRotation(rotation_.x, rotation_.y, z);
}

void CD3DCAMERA::LookAt(const CD3DVECTOR& target) {

    long double dx = static_cast<long double>(target.x) - position_.x;
    long double dy = static_cast<long double>(target.y) - position_.y;
    long double dz = static_cast<long double>(target.z) - position_.z;
    const long double invLength = 1.0L / std::sqrt(dx * dx + dy * dy + dz * dz);
    const float x = static_cast<float>(dx * invLength);
    const float y = static_cast<float>(dy * invLength);
    const float z = static_cast<float>(dz * invLength);

    float pitch = 0.0f;
    float yaw = 0.0f;

    if (x == 0.0f && z == 0.0f) {

        pitch = y > 0.0f ? 64.0f : 192.0f;
    } else {
        yaw = DirectionAngle(z, x);
        const float horizontal = static_cast<float>(
            std::sqrt(static_cast<long double>(x) * x +
                      static_cast<long double>(z) * z));
        pitch = DirectionAngle(y, horizontal);
        if (yaw < 0.0f)
            yaw += 256.0f;
        if (pitch < 0.0f)
            pitch += 256.0f;
    }

    rotation_.x = NormalizeAngle256(pitch);
    rotation_.y = NormalizeAngle256(yaw + 192.0f);
    rotation_.z = 0.0f;
}

void CD3DCAMERA::SetFov(float fov) {

    fov_ = fov;
}

}
