#include "flyctrl.hpp"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <limits>

namespace flydemo {
namespace {

template <typename T>
bool readExact(std::istream& in, T& value) {
    return static_cast<bool>(in.read(reinterpret_cast<char*>(&value), sizeof(value)));
}

template <typename T>
bool writeExact(std::ostream& out, const T& value) {
    return static_cast<bool>(out.write(reinterpret_cast<const char*>(&value), sizeof(value)));
}

bool readVec3(std::istream& in, CD3DVECTOR& v) {
    return readExact(in, v.x) && readExact(in, v.y) && readExact(in, v.z);
}

bool writeVec3(std::ostream& out, const CD3DVECTOR& v) {
    return writeExact(out, v.x) && writeExact(out, v.y) && writeExact(out, v.z);
}

}

bool CamFlightController::SetFrames(std::vector<CameraFrame> frames) {
    if (frames.size() > std::numeric_limits<std::uint16_t>::max())
        return false;
    frames_ = std::move(frames);
    time_ = 0.0f;
    currentFrame_ = 0;
    return !frames_.empty();
}

bool CamFlightController::LoadPath(const std::string& filename) {

    std::ifstream in(filename, std::ios::binary);
    if (!in)
        return false;

    std::uint16_t count = 0;
    if (!readExact(in, count))
        return false;

    std::vector<CameraFrame> loaded(count);
    for (CameraFrame& frame : loaded) {
        if (!readExact(in, frame.At) ||
            !readVec3(in, frame.position) ||
            !readVec3(in, frame.rotation) ||
            !readExact(in, frame.interpolationMode)) {
            return false;
        }
        frame.reserved = 0;
    }

    frames_ = std::move(loaded);
    time_ = 0.0f;
    currentFrame_ = 0;
    return true;
}

bool CamFlightController::SavePath(const std::string& filename) const {
    std::ofstream out(filename, std::ios::binary | std::ios::trunc);
    if (!out)
        return false;

    const std::uint16_t count = static_cast<std::uint16_t>(frames_.size());
    if (!writeExact(out, count))
        return false;
    for (const CameraFrame& frame : frames_) {
        if (!writeExact(out, frame.At) ||
            !writeVec3(out, frame.position) ||
            !writeVec3(out, frame.rotation) ||
            !writeExact(out, frame.interpolationMode)) {
            return false;
        }
    }
    return static_cast<bool>(out);
}

bool CamFlightController::SetStart(float start) {

    if (frames_.empty() || start < 0.0f || !(start < frames_.back().At))
        return false;
    time_ = start;
    return true;
}

float CamFlightController::InterpolateWrappedYaw(float current, float next,
                                                  float currentWeight,
                                                  float nextWeight) {

    if (std::fabs((current + 256.0f) - next) > std::fabs(current - next))
        current -= 256.0f;
    if (std::fabs((current - 256.0f) - next) > std::fabs(current - next))
        current += 256.0f;
    if (std::fabs((next + 256.0f) - current) > std::fabs(next - current))
        next -= 256.0f;
    if (std::fabs((next - 256.0f) - current) > std::fabs(next - current))
        next += 256.0f;
    return current * currentWeight + next * nextWeight;
}

bool CamFlightController::Update(float dt, CD3DCAMERA* camera) {
    if (frames_.size() < 2)
        return false;

    CameraFrame& loop = frames_.back();
    const CameraFrame& first = frames_.front();
    loop.position = first.position;
    loop.rotation = first.rotation;
    loop.interpolationMode = first.interpolationMode;

    time_ += dt;

    int current = static_cast<int>(frames_.size()) - 1;
    while (current >= 0 && frames_[static_cast<std::size_t>(current)].At > time_)
        --current;
    if (current < 0)
        current = static_cast<int>(frames_.size()) - 1;

    currentFrame_ = static_cast<std::uint16_t>(current);
    int next = current + 1;
    if (current == static_cast<int>(frames_.size()) - 1)
        next = 1;
    if (next >= static_cast<int>(frames_.size()))
        next = 1;

    const CameraFrame& a = frames_[static_cast<std::size_t>(current)];
    const CameraFrame& b = frames_[static_cast<std::size_t>(next)];
    const float denom = b.At - a.At;
    float currentWeight = (b.At - time_) / denom;
    float nextWeight = 1.0f - currentWeight;

    if (a.interpolationMode == 1) {
        currentWeight = 1.0f;
        nextWeight = 0.0f;
    }

    lastPosition_.x = a.position.x * currentWeight + b.position.x * nextWeight;
    lastPosition_.y = a.position.y * currentWeight + b.position.y * nextWeight;
    lastPosition_.z = a.position.z * currentWeight + b.position.z * nextWeight;

    lastRotation_.x = a.rotation.x * currentWeight + b.rotation.x * nextWeight;
    lastRotation_.y = InterpolateWrappedYaw(a.rotation.y, b.rotation.y,
                                            currentWeight, nextWeight);
    lastRotation_.z = a.rotation.z * currentWeight + b.rotation.z * nextWeight;

    if (camera) {
        camera->SetPosition(lastPosition_);
        camera->SetRotation(lastRotation_.x, lastRotation_.y, lastRotation_.z);
    }

    if (time_ > frames_.back().At)
        time_ = 0.0f;
    return true;
}

}
