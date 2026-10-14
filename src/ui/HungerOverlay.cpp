#include "HungerOverlay.hpp"
#include "Painter.hpp"
#include "Animation.hpp"
#include "game/Food.hpp"
#include <algorithm>
#include <array>

namespace appleskin {
void renderHungerOverlay(void* renderer, void* context, void* control, const Targets& targets) {
    const auto options = Settings::get().snapshot();
    if (!options->hungerPreview && !options->saturation && !options->exhaustion) return;
    if (!renderer || !context || !control || !read<std::uint8_t>(renderer, build::field::RendererVisible)) return;
    const auto opacity = read<float>(renderer, build::field::RendererAlpha);
    if (!within(opacity, 0, 1) || opacity == 0) return;
    const auto state = hungerState(context, targets);
    if (!state.valid) return;
    const auto origin = targets.position(control);
    if (!origin || !within(origin->x, -32768, 32768) || !within(origin->y, -32768, 32768)) return;
    std::array<Vec3, 10> positions;
    for (std::size_t index = 0; index < positions.size(); ++index) {
        auto local = read<Vec3>(renderer, build::field::RendererPositions + index * sizeof(Vec3));
        if (!within(local.x, -256, 256) || !within(local.y, -64, 64) || !within(local.z, -16, 16)) return;
        positions[index] = {origin->x + local.x, origin->y + local.y, local.z};
    }
    const bool preview = state.food.valid && (options->previewFullHunger || state.food.alwaysEat || state.hunger < 20);
    const auto afterHunger = preview ? std::min(20.0f, state.hunger + state.food.nutrition) : state.hunger;
    const auto afterSaturation = preview ? std::min(afterHunger, state.saturation + state.food.saturation) : state.saturation;
    const auto pulse = previewOpacity(opacity, *options);
    auto* base = static_cast<std::byte*>(renderer);
    const auto screen = read<void*>(context, build::field::UiScreen);
    const auto effect = read<std::uint8_t>(renderer, build::field::RendererHungerEffect) ? 1u : 0u;
    if (options->hungerPreview && preview && afterHunger > state.hunger && screen) {
        std::array<Vec3, 10> full;
        std::array<Vec3, 1> half;
        std::size_t fullCount = 0, halfCount = 0;
        for (int index = 0; index < 10; ++index) {
            if (index * 2 + 2 <= state.hunger || index * 2 >= afterHunger) continue;
            if (index * 2 + 2 <= afterHunger) full[fullCount++] = positions[index];
            else half[halfCount++] = positions[index];
        }
        const auto submit = [&](unsigned type, const Vec3* points, std::size_t count) {
            const auto mesh = base + build::field::RendererMeshes + type * build::field::MeshStride;
            const auto texture = base + build::field::RendererTextures + type * build::field::TextureStride;
            const auto data = read<const void*>(texture, 0);
            if (count && textureReady(data) && targets.meshValid(mesh)) targets.batch(screen, mesh, texture, count, points, pulse);
        };
        submit(2 + effect, full.data(), fullCount);
        submit(4 + effect, half.data(), halfCount);
    }
    const bool drawSaturation = options->saturation && state.saturation > 0;
    const bool drawSaturationPreview = options->saturation && options->saturationPreview && preview && afterSaturation > state.saturation;
    const bool drawExhaustion = options->exhaustion && state.hasExhaustion &&
        (!options->hideEmptyExhaustion || state.exhaustion > 0);
    if (!drawSaturation && !drawSaturationPreview && !drawExhaustion) return;
    Painter painter(context);
    if (!painter.available()) return;
    painter.start();
    const auto gold = settingColor(options->saturationColor);
    if (drawSaturation) {
        for (int index = 0; index < 10; ++index)
            painter.saturation({positions[index].x, positions[index].y}, 9.0f, 0,
                state.saturation * 0.5f - index, options->borderWidth);
        painter.finish(gold, opacity * options->saturationOpacity * 0.01f);
    }
    if (drawSaturationPreview) {
        for (int index = 0; index < 10; ++index)
            painter.saturation({positions[index].x, positions[index].y}, 9.0f,
                state.saturation * 0.5f - index, afterSaturation * 0.5f - index, options->borderWidth);
        painter.finish(gold, pulse);
    }
    if (drawExhaustion) {
        const auto profile = uiProfile(context);
        if (profile != 0 && profile != 1) return;
        auto left = positions[0].x, right = left + 9.0f;
        auto top = positions[0].y, bottom = top + 9.0f;
        for (const auto& point : positions) {
            left = std::min(left, point.x);
            right = std::max(right, point.x + 9.0f);
            top = std::min(top, point.y);
            bottom = std::max(bottom, point.y + 9.0f);
        }
        const auto height = options->exhaustionHeight;
        const auto y = profile == 0 ? top - options->exhaustionGap - height : bottom + options->exhaustionGap;
        const auto barAlpha = opacity * options->exhaustionOpacity * 0.01f;
        const auto fraction = std::clamp(state.exhaustion / 4.0f, 0.0f, 1.0f);
        painter.rectangle({left, right, y, y + height});
        painter.finish({0.03f, 0.03f, 0.03f, 1}, barAlpha * 0.85f);
        painter.rectangle({left + 1.0f, right - 1.0f, y + 1.0f, y + height - 1.0f});
        painter.finish({0.2f, 0.2f, 0.2f, 1}, barAlpha);
        painter.rectangle({right - 1.0f - (right - left - 2.0f) * fraction, right - 1.0f, y + 1.0f, y + height - 1.0f});
        painter.finish(settingColor(options->exhaustionColor), barAlpha);
        for (int mark = 1; mark < 4; ++mark) {
            const auto x = left + (right - left) * static_cast<float>(mark) * 0.25f;
            painter.rectangle({x, x + 0.75f, y + 1.0f, y + height - 1.0f});
        }
        painter.finish({0.03f, 0.03f, 0.03f, 1}, barAlpha * 0.65f);
    }
}
}
