#include "Settings.hpp"
#include "Json.hpp"
#include <algorithm>
#include <charconv>
#include <fstream>
#include <iomanip>
#include <locale>
#include <sstream>
#include <type_traits>

namespace appleskin {
namespace {
const SettingSpec specs[]{
    {"hunger_preview", "Hunger preview", "Pulse the hunger icons restored by the food in your hand.", "hud", "food_previews", SettingKind::Toggle, &Options::hungerPreview},
    {"health_preview", "Healing preview", "Pulse the hearts that food could restore through natural regeneration.", "hud", "food_previews", SettingKind::Toggle, &Options::healthPreview},
    {"preview_full_hunger", "Preview at full hunger", "Show potential saturation and healing while planning a meal at full hunger.", "hud", "food_previews", SettingKind::Toggle, &Options::previewFullHunger},
    {"saturation", "Saturation outline", "Outline the drumsticks that represent your current saturation.", "hud", "saturation_display", SettingKind::Toggle, &Options::saturation},
    {"saturation_preview", "Saturation preview", "Pulse the additional outline supplied by the food in your hand.", "hud", "saturation_display", SettingKind::Toggle, &Options::saturationPreview, 0, 0, 0, {}, "saturation"},
    {"exhaustion", "Exhaustion bar", "Show progress toward the next saturation or hunger drain.", "hud", "exhaustion_display", SettingKind::Toggle, &Options::exhaustion},
    {"hide_empty_exhaustion", "Hide when empty", "Hide the exhaustion bar when there is no exhaustion.", "hud", "exhaustion_display", SettingKind::Toggle, &Options::hideEmptyExhaustion, 0, 0, 0, {}, "exhaustion"},
    {"tooltips", "Food tooltips", "Add nutrition and saturation values to food descriptions.", "tooltips", "tooltip_display", SettingKind::Toggle, &Options::tooltips},
    {"tooltip_icons", "Tooltip icons", "Show drumstick rows beside the food values.", "tooltips", "tooltip_display", SettingKind::Toggle, &Options::tooltipIcons, 0, 0, 0, {}, "tooltips"},
    {"animate_previews", "Pulse previews", "Animate food restoration previews instead of showing them steadily.", "appearance", "preview_style", SettingKind::Toggle, &Options::animatePreviews},
    {"preview_opacity", "Preview opacity", "Set the brightest opacity of hunger, healing, and saturation previews.", "appearance", "preview_style", SettingKind::Slider, &Options::previewOpacity, 10, 100, 1, "%"},
    {"pulse_speed", "Pulse speed", "Set the number of pulses per second.", "appearance", "preview_style", SettingKind::Slider, &Options::pulseSpeed, 0.25f, 3, 0.05f, " Hz", "animate_previews"},
    {"saturation_color", "Outline color", "Choose the saturation outline color.", "appearance", "outline_style", SettingKind::Color, &Options::saturationColor},
    {"saturation_opacity", "Outline opacity", "Set the opacity of current saturation and tooltip outlines.", "appearance", "outline_style", SettingKind::Slider, &Options::saturationOpacity, 10, 100, 1, "%"},
    {"border_width", "Outline width", "Set the width of the outline without covering the icon's center.", "appearance", "outline_style", SettingKind::Slider, &Options::borderWidth, 0.5f, 1, 0.1f, " px"},
    {"exhaustion_color", "Bar color", "Choose the exhaustion fill color.", "appearance", "bar_style", SettingKind::Color, &Options::exhaustionColor, 0, 0, 0, {}, "exhaustion"},
    {"exhaustion_opacity", "Bar opacity", "Set the opacity of the exhaustion bar.", "appearance", "bar_style", SettingKind::Slider, &Options::exhaustionOpacity, 10, 100, 1, "%", "exhaustion"},
    {"exhaustion_height", "Bar height", "Set the exhaustion bar height in GUI pixels.", "appearance", "bar_style", SettingKind::Slider, &Options::exhaustionHeight, 3, 8, 1, " px", "exhaustion"},
    {"exhaustion_gap", "Bar spacing", "Set the space between the hunger icons and exhaustion bar.", "appearance", "bar_style", SettingKind::Slider, &Options::exhaustionGap, 0, 12, 1, " px", "exhaustion"}
};

bool boolean(std::string_view value, bool& output) {
    if (value == "true" || value == "1") { output = true; return true; }
    if (value == "false" || value == "0") { output = false; return true; }
    return false;
}

bool assign(Options& options, const SettingSpec& spec, std::string_view value) {
    return std::visit([&](auto member) {
        using Value = std::remove_reference_t<decltype(options.*member)>;
        if constexpr (std::is_same_v<Value, bool>) return boolean(value, options.*member);
        else if constexpr (std::is_same_v<Value, float>) {
            if (value.empty() || value.size() > 64) return false;
            std::istringstream input{std::string(value)};
            input.imbue(std::locale::classic());
            float parsed;
            if (!(input >> parsed) || !std::isfinite(parsed)) return false;
            input >> std::ws;
            if (!input.eof()) return false;
            options.*member = std::clamp(parsed, spec.minimum, spec.maximum);
            return true;
        } else {
            if (value.starts_with('#')) value.remove_prefix(1);
            else if (value.starts_with("0x") || value.starts_with("0X")) value.remove_prefix(2);
            if (value.size() != 6 && value.size() != 8) return false;
            std::uint32_t parsed;
            const auto result = std::from_chars(value.data(), value.data() + value.size(), parsed, 16);
            if (result.ec != std::errc{} || result.ptr != value.data() + value.size()) return false;
            options.*member = value.size() == 6 ? parsed | 0xff000000 : parsed;
            return true;
        }
    }, spec.member);
}
}

std::span<const SettingSpec> settingSpecs() { return specs; }

std::string settingValue(const Options& options, const SettingSpec& spec) {
    return std::visit([&](auto member) -> std::string {
        using Value = std::remove_cvref_t<decltype(options.*member)>;
        if constexpr (std::is_same_v<Value, bool>) return options.*member ? "true" : "false";
        else {
            std::ostringstream output;
            output.imbue(std::locale::classic());
            if constexpr (std::is_same_v<Value, std::uint32_t>)
                output << '#' << std::hex << std::uppercase << std::setw(8) << std::setfill('0') << options.*member;
            else output << std::setprecision(6) << options.*member;
            return output.str();
        }
    }, spec.member);
}

Color settingColor(std::uint32_t argb) {
    return {static_cast<float>((argb >> 16) & 255) / 255,
        static_cast<float>((argb >> 8) & 255) / 255, static_cast<float>(argb & 255) / 255,
        static_cast<float>((argb >> 24) & 255) / 255};
}

Settings& Settings::get() { static Settings result; return result; }
bool Settings::enabled() const { return master.load(std::memory_order_acquire); }
std::shared_ptr<const Options> Settings::snapshot() const { return std::atomic_load_explicit(&current, std::memory_order_acquire); }
void Settings::publish(const Options& options) {
    std::atomic_store_explicit(&current, std::make_shared<const Options>(options), std::memory_order_release);
}

void Settings::load(const std::filesystem::path& root) {
    std::lock_guard lock(mutex);
    path = root.empty() ? std::filesystem::path{} : root / "config" / "config.json";
    Options options;
    bool enabled = true;
    std::error_code error;
    const auto size = path.empty() ? 0 : std::filesystem::file_size(path, error);
    if (!error && size > 0 && size <= 65536) {
        std::ifstream input(path, std::ios::binary);
        std::string source(static_cast<std::size_t>(size), '\0');
        std::map<std::string, std::string> values;
        if (input.read(source.data(), static_cast<std::streamsize>(source.size())) && json::object(source, values)) {
            if (const auto entry = values.find("enabled"); entry != values.end()) boolean(entry->second, enabled);
            for (const auto& spec : specs)
                if (const auto entry = values.find(std::string(spec.key)); entry != values.end()) assign(options, spec, entry->second);
        }
    }
    publish(options);
    master.store(enabled, std::memory_order_release);
}

void Settings::enable(bool value) {
    std::lock_guard lock(mutex);
    master.store(value, std::memory_order_release);
    save();
}

void Settings::change(std::string_view key, std::string_view value) {
    std::lock_guard lock(mutex);
    auto options = *snapshot();
    for (const auto& spec : specs) {
        if (spec.key != key) continue;
        if (assign(options, spec, value)) { publish(options); save(); }
        return;
    }
}

void Settings::save() const {
    if (path.empty()) return;
    std::error_code error;
    std::filesystem::create_directories(path.parent_path(), error);
    if (error) return;
    auto temporary = path;
    temporary += ".tmp";
    std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
    if (!output) return;
    output << "{\n  \"version\": 1,\n  \"enabled\": " << (enabled() ? "true" : "false");
    const auto options = snapshot();
    for (const auto& spec : specs) {
        const auto value = settingValue(*options, spec);
        output << ",\n  " << json::quote(spec.key) << ": " << (spec.kind == SettingKind::Color ? json::quote(value) : value);
    }
    output << "\n}\n";
    output.close();
    if (!output) { std::filesystem::remove(temporary, error); return; }
    std::filesystem::rename(temporary, path, error);
    if (error) std::filesystem::remove(temporary, error);
}
}
