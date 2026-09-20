#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include "camera.hpp"
#include "types.hpp"

namespace flydemo {

struct CameraFrame {
    float At = 0.0f;
    CD3DVECTOR position{0.0f, 0.0f, 0.0f};
    CD3DVECTOR rotation{0.0f, 0.0f, 0.0f};
    std::uint16_t interpolationMode = 0;
    std::uint16_t reserved = 0;
};
static_assert(sizeof(CameraFrame) == 0x20, "CameraFrame stride must match PE32");

class CamFlightController {
public:
    float GetTime() const { return time_; }
    std::uint16_t GetCurrentFrame() const { return currentFrame_; }
    std::uint16_t GetNumFrames() const { return static_cast<std::uint16_t>(frames_.size()); }
    const std::vector<CameraFrame>& GetFrames() const { return frames_; }

    bool LoadPath(const std::string& filename);
    bool SavePath(const std::string& filename) const;
    bool SetFrames(std::vector<CameraFrame> frames);
    bool SetStart(float start);

    bool Update(float dt, CD3DCAMERA* camera = nullptr);

    const CD3DVECTOR& LastPosition() const { return lastPosition_; }
    const CD3DVECTOR& LastRotation() const { return lastRotation_; }

private:
    static float InterpolateWrappedYaw(float current, float next,
                                       float currentWeight, float nextWeight);

    float time_ = 0.0f;
    std::uint16_t currentFrame_ = 0;
    std::vector<CameraFrame> frames_;
    CD3DVECTOR lastPosition_{0.0f, 0.0f, 0.0f};
    CD3DVECTOR lastRotation_{0.0f, 0.0f, 0.0f};
};

}
