#include "Painter.hpp"
#include <algorithm>

namespace appleskin {
namespace {
std::uint64_t hashBytes(std::string_view bytes, std::uint64_t seed) {
    for (const unsigned char byte : bytes) {
        seed *= 0x100000001b3ULL;
        seed ^= byte;
    }
    return seed;
}
MaterialKey& material() {
    static MaterialKey result("ui_flush");
    return result;
}
}
ResourceKey::ResourceKey(const char* name) : path(name) {
    pathHash = hashBytes(path, 1469598103934665603ULL);
    hash = pathHash;
}
MaterialKey::MaterialKey(const char* value) : name(value) {
    hash = name.empty() ? 0 : hashBytes(name, 0xcbf29ce484222325ULL);
}

Painter::Painter(void* value) : context(value) {
    const auto get = method<Texture (*)(void*, const ResourceKey&, bool)>(context, build::slot::UiGetTexture);
    if (get) {
        texture = get(context, ResourceKey("textures/gui/icons"), false);
        white = get(context, ResourceKey("textures/ui/White"), false);
    }
}

bool Painter::available() const { return textureReady(texture.data.get()) || textureReady(white.data.get()); }

void Painter::start() {
    const auto flush = method<void (*)(void*, const Color&, float, const MaterialKey&)>(context, build::slot::UiFlushImages);
    if (flush) flush(context, Color{}, 1.0f, material());
}

void Painter::sprite(const Texture& source, Vec2 position, Vec2 size, Vec2 uv, Vec2 uvSize) {
    if (!textureReady(source.data.get()) || size.x <= 0 || size.y <= 0) return;
    const auto draw = method<void (*)(void*, const TextureData&, const Vec2&, const Vec2&, const Vec2&, const Vec2&, bool)>(context, build::slot::UiDrawImage);
    if (!draw) return;
    draw(context, *source.data, position, size, uv, uvSize, false);
    pending = true;
}

void Painter::hunger(Vec2 position, float size, bool half, bool background) {
    const float x = background ? 16.0f : half ? 61.0f : 52.0f;
    sprite(texture, position, {size, size}, {x / 256.0f, 27.0f / 256.0f}, {9.0f / 256.0f, 9.0f / 256.0f});
}

void Painter::saturation(Vec2 position, float size, float first, float last, float width) {
    const auto low = std::clamp(first, 0.0f, 1.0f);
    const auto high = std::clamp(last, 0.0f, 1.0f);
    if (high <= low) return;
    const auto unit = size / 9.0f;
    const auto stroke = std::clamp(width, 0.5f, 1.0f);
    const auto clipLeft = (1.0f - high) * 9.0f;
    const auto clipRight = (1.0f - low) * 9.0f;
    constexpr std::array<Vec2, 26> contour{{
        {2, 0}, {4, 0}, {4, 1}, {5, 1}, {5, 2}, {6, 2}, {6, 3}, {7, 3},
        {7, 6}, {9, 6}, {9, 8}, {8, 8}, {8, 9}, {6, 9}, {6, 7}, {3, 7},
        {3, 6}, {2, 6}, {2, 5}, {1, 5}, {1, 4}, {0, 4}, {0, 2}, {1, 2},
        {1, 1}, {2, 1}
    }};
    const auto direction = [](Vec2 a, Vec2 b) {
        const auto length = std::abs(b.x - a.x) + std::abs(b.y - a.y);
        return Vec2{(b.x - a.x) / length, (b.y - a.y) / length};
    };
    const auto submit = [&](Vec2 a, Vec2 b) {
        const auto left = std::max(clipLeft, std::min(a.x, b.x));
        const auto right = std::min(clipRight, std::max(a.x, b.x));
        if (right <= left) return;
        rectangle({position.x + left * unit, position.x + right * unit,
            position.y + std::min(a.y, b.y) * unit, position.y + std::max(a.y, b.y) * unit});
    };
    for (std::size_t index = 0; index < contour.size(); ++index) {
        const auto a = contour[index], b = contour[(index + 1) % contour.size()];
        const auto next = contour[(index + 2) % contour.size()];
        const auto d = direction(a, b), n = direction(b, next);
        const auto turn = d.x * n.y - d.y * n.x;
        const Vec2 end{b.x - (turn > 0 ? d.x * stroke : 0), b.y - (turn > 0 ? d.y * stroke : 0)};
        const Vec2 inward{-d.y * stroke, d.x * stroke};
        submit({std::min(a.x, end.x) + std::min(0.0f, inward.x),
                    std::min(a.y, end.y) + std::min(0.0f, inward.y)},
            {std::max(a.x, end.x) + std::max(0.0f, inward.x),
                    std::max(a.y, end.y) + std::max(0.0f, inward.y)});
    }
}

void Painter::rectangle(const Rect& rectangle) {
    sprite(white, {rectangle.left, rectangle.top},
        {rectangle.right - rectangle.left, rectangle.bottom - rectangle.top}, {0, 0}, {1, 1});
}

void Painter::finish(Color color, float alpha) {
    if (!pending) return;
    const auto flush = method<void (*)(void*, const Color&, float, const MaterialKey&)>(context, build::slot::UiFlushImages);
    alpha *= color.a;
    color.a = 1;
    if (flush) flush(context, color, std::clamp(alpha, 0.0f, 1.0f), material());
    pending = false;
}

}
