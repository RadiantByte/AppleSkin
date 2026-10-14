#include "HealthOverlay.hpp"
#include "Animation.hpp"
#include "game/Food.hpp"
#include <algorithm>
#include <array>

namespace appleskin {
void renderHealthOverlay(void* renderer, void* context, void* control, const Targets& targets) {
    const auto options = Settings::get().snapshot();
    if (!options->healthPreview) return;
    if (!renderer || !context || !control || !read<std::uint8_t>(renderer, build::field::HeartVisible)) return;
    const auto opacity = read<float>(renderer, build::field::RendererAlpha);
    if (!within(opacity, 0, 1) || opacity == 0) return;
    const auto preview = healthPreview(hungerState(context, targets), targets, options->previewFullHunger);
    if (!preview.valid) return;
    const auto origin = targets.position(control);
    const auto screen = read<void*>(context, build::field::UiScreen);
    if (!origin || !screen || !within(origin->x, -32768, 32768) || !within(origin->y, -32768, 32768)) return;
    const auto begin = read<std::uintptr_t>(renderer, build::field::HeartPositions);
    const auto end = read<std::uintptr_t>(renderer, build::field::HeartPositionsEnd);
    if (!begin || end < begin || (end - begin) % sizeof(Vec3) || end - begin > 512 * sizeof(Vec3)) return;
    const auto count = std::min<std::size_t>((end - begin) / sizeof(Vec3),
        static_cast<std::size_t>(std::ceil(preview.maximum * 0.5f)));
    const auto before = std::ceil(preview.current);
    const auto after = std::ceil(preview.predicted);
    std::array<Vec3, 256> full;
    std::array<Vec3, 1> half;
    std::size_t fullCount = 0, halfCount = 0;
    for (std::size_t index = 0; index < count; ++index) {
        if (index * 2 + 2 <= before || index * 2 >= after) continue;
        const auto local = read<Vec3>(reinterpret_cast<const void*>(begin), index * sizeof(Vec3));
        if (!within(local.x, -4096, 4096) || !within(local.y, -4096, 4096) || !within(local.z, -16, 16)) return;
        const Vec3 position{origin->x + local.x, origin->y + local.y, local.z};
        if (index * 2 + 2 <= after) full[fullCount++] = position;
        else half[halfCount++] = position;
    }
    auto* base = static_cast<std::byte*>(renderer);
    const auto pulse = previewOpacity(opacity, *options);
    const auto submit = [&](int type, const Vec3* positions, std::size_t size) {
        if (!size || type < 0 || type >= 20) return;
        const auto mesh = base + build::field::HeartMeshes + type * build::field::MeshStride;
        const auto texture = base + build::field::HeartTextures + type * build::field::TextureStride;
        if (textureReady(read<const void*>(texture, 0)) && targets.meshValid(mesh))
            targets.batch(screen, mesh, texture, size, positions, pulse);
    };
    submit(read<int>(renderer, build::field::HeartFullType), full.data(), fullCount);
    submit(read<int>(renderer, build::field::HeartHalfType), half.data(), halfCount);
}
}
