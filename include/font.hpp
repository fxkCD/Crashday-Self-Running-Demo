#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace flydemo {

class CBMManager;
class CBMLoader;
class RendererState;
class RenderDevice;

enum class FontType : std::uint8_t { Thin = 0, Fat = 1 };

struct FontVertex2D {
    float x = 0.0f, y = 0.0f;
    float u = 0.0f, v = 0.0f;
    std::uint32_t color = 0xFFFFFFFFu;
};
struct FontGlyphQuad {
    unsigned char character = 0;
    FontVertex2D vertices[4]{};
};
struct FontLayout {
    std::string texture;
    std::vector<FontGlyphQuad> glyphs;
};

bool ValidateFontType(FontType type);
const char* FontTexture(FontType type);

FontLayout BuildFontLayout(FontType type, int x, int y, std::uint32_t color,
                           const std::string& text);

bool RenderFontText(FontType type, int x, int y, std::uint32_t color,
                    const std::string& text,
                    CBMManager& textures, CBMLoader& loader,
                    RendererState& renderer, RenderDevice& backend);

}
