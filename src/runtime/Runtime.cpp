#include "Runtime.hpp"
#include "ui/FoodTooltip.hpp"
#include "ui/HungerOverlay.hpp"
#include "ui/HealthOverlay.hpp"
#include "config/Settings.hpp"
#include <android/dlext.h>
#include <array>
#include <dlfcn.h>

namespace appleskin {
namespace {
HungerFn hungerNext{};
HeartFn heartNext{};
HoverFn hoverNext{};
TextFn textNext{};
TextViewFn textViewNext{};
FontDrawFn fontDrawNext{};
FinishFn finishNext{};
struct PendingTooltip { void* screen{}; TooltipRows rows; float alpha{}; };
thread_local std::array<PendingTooltip, 16> pendingTooltips{};
thread_local std::size_t pendingCount{};
thread_local bool finishing{};
void* (*openNext)(const char*, int){};
void* (*openExtNext)(const char*, int, const android_dlextinfo*){};
thread_local bool insideLoader{};

void hungerHook(void* self, void* context, void* client, void* control, int pass, void* area) {
    hungerNext(self, context, client, control, pass, area);
    auto& runtime = Runtime::get();
    if (runtime.active()) renderHungerOverlay(self, context, control, runtime.native());
}

void heartHook(void* self, void* context, void* client, void* control, int pass, void* area) {
    heartNext(self, context, client, control, pass, area);
    auto& runtime = Runtime::get();
    if (runtime.active()) renderHealthOverlay(self, context, control, runtime.native());
}

void hoverHook(const void* self, const void* stack, void* level, RedactableText* text, bool category) {
    hoverNext(self, stack, level, text, category);
    if (Runtime::get().active()) appendFoodTooltip(stack, text);
}

void queueTooltip(void* screen, const TooltipRows& rows, float alpha) {
    if (finishing || !screen || !rows.count || !within(alpha, 0, 1) || alpha == 0) return;
    for (std::size_t index = 0; index < pendingCount; ++index) {
        auto& item = pendingTooltips[index];
        if (item.screen != screen || item.rows.count != rows.count) continue;
        bool same = true;
        for (std::size_t row = 0; row < rows.count; ++row) {
            const auto& a = item.rows.rows[row];
            const auto& b = rows.rows[row];
            same &= std::abs(a.position.x - b.position.x) < 0.01f &&
                std::abs(a.position.y - b.position.y) < 0.01f && a.value == b.value &&
                a.saturation == b.saturation && std::abs(a.size - b.size) < 0.01f;
        }
        if (same) { item = {screen, rows, alpha}; return; }
    }
    if (pendingCount < pendingTooltips.size()) pendingTooltips[pendingCount++] = {screen, rows, alpha};
}

void textHook(void* context, void* font, const Rect& rectangle, std::string& text,
    const Color& color, int alignment, float alpha, const TextStyle& style, const Caret& caret) {
    const auto rows = Runtime::get().active() ? captureTooltipRows(context, font, rectangle, text, alignment, style) : TooltipRows{};
    textNext(context, font, rectangle, text, color, alignment, alpha, style, caret);
    if (Runtime::get().active()) queueTooltip(read<void*>(context, build::field::UiScreen), rows, alpha);
}

void textViewHook(void* context, void* font, const Rect& rectangle, std::string_view text,
    const Color& color, int alignment, float alpha, const TextStyle& style, const Caret& caret) {
    const auto rows = Runtime::get().active() ? captureTooltipRows(context, font, rectangle, text, alignment, style) : TooltipRows{};
    textViewNext(context, font, rectangle, text, color, alignment, alpha, style, caret);
    if (Runtime::get().active()) queueTooltip(read<void*>(context, build::field::UiScreen), rows, alpha);
}

void fontDrawHook(void* font, void* screen, std::string_view text, float x, float y,
    const Color& color, int flags, float depth, const void* data, int index, void* output) {
    const auto rows = Runtime::get().active() && !finishing ? captureFontTooltipRows(font, text, x, y) : TooltipRows{};
    fontDrawNext(font, screen, text, x, y, color, flags, depth, data, index, output);
    if (Runtime::get().active()) queueTooltip(screen, rows, color.a);
}

void finishHook(void* context) {
    const auto previous = finishing;
    finishing = true;
    const auto screen = context ? read<void*>(context, build::field::UiScreen) : nullptr;
    if (!previous) {
        if (Runtime::get().active()) {
            for (std::size_t index = 0; index < pendingCount;) {
                const auto item = pendingTooltips[index];
                if (item.screen != screen) { ++index; continue; }
                for (auto next = index + 1; next < pendingCount; ++next) pendingTooltips[next - 1] = pendingTooltips[next];
                --pendingCount;
                renderTooltipRows(context, item.rows, item.alpha);
            }
        } else pendingCount = 0;
    }
    finishNext(context);
    finishing = previous;
}

void notifyLoad(const char* name) {
    if (insideLoader || !name) return;
    insideLoader = true;
    Runtime::get().libraryLoaded(name);
    insideLoader = false;
}

void* openHook(const char* name, int flags) {
    auto* handle = openNext(name, flags);
    if (handle) notifyLoad(name);
    return handle;
}

void* openExtHook(const char* name, int flags, const android_dlextinfo* info) {
    auto* handle = openExtNext(name, flags, info);
    if (handle) notifyLoad(name);
    return handle;
}
}

Runtime& Runtime::get() { static Runtime instance; return instance; }
bool Runtime::active() const { return running.load(std::memory_order_acquire) && Settings::get().enabled(); }
const Targets& Runtime::native() const { return targets; }

bool Runtime::load(const ModLocation& location) {
    std::lock_guard lock(mutex);
    insideLoader = true;
    const bool connected = host.connect();
    insideLoader = false;
    if (!connected) return false;
    Settings::get().load(location.root);
    menu.connect(location.owner);
    if (image.locate()) return true;
    const auto ordinary = dlsym(RTLD_DEFAULT, "dlopen");
    const auto extended = dlsym(RTLD_DEFAULT, "android_dlopen_ext");
    bool observed = false;
    if (ordinary) observed |= host.install(ordinary, reinterpret_cast<void*>(openHook), reinterpret_cast<void**>(&openNext));
    if (extended) observed |= host.install(extended, reinterpret_cast<void*>(openExtHook), reinterpret_cast<void**>(&openExtNext));
    if (!observed) { menu.disconnect(); host.removeAll(); }
    return observed;
}

bool Runtime::install() {
    if (installed) { running.store(true, std::memory_order_release); return true; }
    if (!image.locate()) return true;
    if (!host.connect() || !image.resolve(targets)) return false;
    if (!host.install(targets.hunger, reinterpret_cast<void*>(hungerHook), reinterpret_cast<void**>(&hungerNext)) ||
        !host.install(targets.heart, reinterpret_cast<void*>(heartHook), reinterpret_cast<void**>(&heartNext)) ||
        !host.install(targets.hover, reinterpret_cast<void*>(hoverHook), reinterpret_cast<void**>(&hoverNext)) ||
        !host.install(targets.text, reinterpret_cast<void*>(textHook), reinterpret_cast<void**>(&textNext)) ||
        !host.install(targets.textView, reinterpret_cast<void*>(textViewHook), reinterpret_cast<void**>(&textViewNext)) ||
        !host.install(targets.fontDraw, reinterpret_cast<void*>(fontDrawHook), reinterpret_cast<void**>(&fontDrawNext)) ||
        !host.install(targets.finish, reinterpret_cast<void*>(finishHook), reinterpret_cast<void**>(&finishNext))) {
        host.removeAll();
        return false;
    }
    installed = true;
    running.store(true, std::memory_order_release);
    return true;
}

bool Runtime::enable() {
    std::lock_guard lock(mutex);
    wanted.store(true, std::memory_order_release);
    return install();
}

bool Runtime::disable() {
    wanted.store(false, std::memory_order_release);
    running.store(false, std::memory_order_release);
    return true;
}

bool Runtime::unload() {
    std::lock_guard lock(mutex);
    disable();
    menu.disconnect();
    installed = false;
    return host.removeAll();
}

void Runtime::libraryLoaded(const char* path) {
    std::string_view name(path);
    const auto slash = name.find_last_of('/');
    if (slash != name.npos) name.remove_prefix(slash + 1);
    if (name != "libminecraftpe.so" || !wanted.load(std::memory_order_acquire)) return;
    std::lock_guard lock(mutex);
    if (wanted.load(std::memory_order_acquire)) install();
}
}
