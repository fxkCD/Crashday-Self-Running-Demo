#pragma once

#include "world.hpp"
#include "render.hpp"

namespace flydemo {

class WorldRendererBridge final : public WorldRender {
public:
    WorldRendererBridge(RendererState& renderer,
                              RenderDevice& device)
        : renderer_(renderer), device_(device) {}

    WorldCullClass ClassifySphere(const CD3DVECTOR& center, float radius,
                                  bool objectTest) override {
        const SphereVisibility value =
            renderer_.ClassifySphere(center, radius, objectTest, device_);
        return static_cast<WorldCullClass>(static_cast<std::uint8_t>(value));
    }

    void FlushDeferredGeometry() override {
        lastFlushSucceeded_ = renderer_.FlushDeferredTriangles(device_);
    }

    bool LastFlushSucceeded() const { return lastFlushSucceeded_; }

private:
    RendererState& renderer_;
    RenderDevice& device_;
    bool lastFlushSucceeded_ = true;
};

}
