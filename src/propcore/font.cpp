#include "font.hpp"
#include "cbm.hpp"
#include "render.hpp"
#include <array>

namespace flydemo {

bool ValidateFontType(FontType type) { return static_cast<std::uint8_t>(type) <= 1; }
const char* FontTexture(FontType type) {
    return type == FontType::Fat ? "fonts/big.cbm" : "fonts/medium.cbm";
}

FontLayout BuildFontLayout(FontType type, int x, int y, std::uint32_t color,
                           const std::string& text) {
    FontLayout out;
    if (!ValidateFontType(type)) return out;
    out.texture = FontTexture(type);
    const int firstX = x;
    constexpr float texel = 1.0f / 128.0f;
    for (unsigned char ch : text) {
        if (ch == static_cast<unsigned char>('|')) {
            x = firstX;
            y += 11;
            continue;
        }
        const int atlasX = (ch % 16) * 8;
        const int atlasY = (ch / 16) * 9;
        const float u0 = atlasX * texel;
        const float v0 = atlasY * texel;
        const float u1 = (atlasX + 8) * texel;
        const float v1 = (atlasY + 9) * texel;
        FontGlyphQuad q{};
        q.character = ch;

        q.vertices[0] = {float(x),   float(y),   u0, v0, color};
        q.vertices[1] = {float(x+8), float(y),   u1, v0, color};
        q.vertices[2] = {float(x),   float(y+9), u0, v1, color};
        q.vertices[3] = {float(x+8), float(y+9), u1, v1, color};
        out.glyphs.push_back(q);
        x += 9;
    }
    return out;
}

bool RenderFontText(FontType type, int x, int y, std::uint32_t color,
                    const std::string& text,
                    CBMManager& textures, CBMLoader& loader,
                    RendererState& renderer, RenderDevice& backend) {
    if (!ValidateFontType(type) || !renderer.SceneInProgress())
        return false;

    const char* textureName = FontTexture(type);
    if (!textures.Contains(textureName)) {
        if (textures.Load(textureName, loader) == CBMManager::InvalidTexture)
            return false;
    }
    const std::uint8_t textureIndex = textures.IndexOf(textureName);
    if (textureIndex == CBMManager::InvalidTexture)
        return false;

    renderer.SelectTexture(textureIndex, 1u, backend);
    renderer.SetGlobalAlpha(1.0f);
    renderer.SetBlendMode(BlendMode::Alpha,
                          backend.TextureHasAlpha(textureIndex), backend);

    std::array<ImmediateVertex,4> scratch{};
    for (auto& v : scratch) {
        v.z = 9.9999997473787516e-06f;
        v.rhw = 1.0f;
        v.specular = 0xFF000000u;
    }

    const int firstX = x;
    constexpr float texel = 1.0f / 128.0f;
    for (unsigned char ch : text) {
        if (ch == static_cast<unsigned char>('|')) {
            x = firstX;
            y += 11;
            continue;
        }

        const int atlasX = (ch % 16u) * 8;
        const int atlasY = (ch / 16u) * 9;
        const float u0 = static_cast<float>(atlasX) * texel;
        const float v0 = static_cast<float>(atlasY) * texel;
        const float u1 = static_cast<float>(atlasX + 8) * texel;
        const float v1 = static_cast<float>(atlasY + 9) * texel;

        scratch[0].x = static_cast<float>(x);
        scratch[0].y = static_cast<float>(y);
        scratch[0].diffuse = color;
        scratch[0].u0 = u0; scratch[0].v0 = v0;

        scratch[1].x = static_cast<float>(x + 8);
        scratch[1].y = static_cast<float>(y);
        scratch[1].diffuse = color;
        scratch[1].u0 = u1; scratch[1].v0 = v0;

        scratch[2].x = static_cast<float>(x);
        scratch[2].y = static_cast<float>(y + 9);
        scratch[2].diffuse = color;
        scratch[2].u0 = u0; scratch[2].v0 = v1;

        scratch[3].x = static_cast<float>(x + 8);
        scratch[3].y = static_cast<float>(y + 9);
        scratch[3].diffuse = color;
        scratch[3].u0 = u1; scratch[3].v0 = v1;

        backend.DrawPrimitive(6u, 0x000002C4u, scratch.data(), 4u, 8u);
        x += 9;
    }
    return true;
}

}
