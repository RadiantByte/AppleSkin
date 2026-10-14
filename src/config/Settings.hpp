#pragma once

#include <appleskin/Native.hpp>
#include <atomic>
#include <filesystem>
#include <mutex>
#include <span>
#include <variant>

namespace appleskin {
struct Options {
    bool hungerPreview{true}, healthPreview{true}, saturation{true}, saturationPreview{true};
    bool exhaustion{}, hideEmptyExhaustion{true}, tooltips{true}, tooltipIcons{true};
    bool previewFullHunger{true}, animatePreviews{true};
    float previewOpacity{75}, pulseSpeed{0.75f}, saturationOpacity{100}, borderWidth{1};
    float exhaustionOpacity{100}, exhaustionHeight{4}, exhaustionGap{2};
    std::uint32_t saturationColor{0xffffcc20}, exhaustionColor{0xff8ce53a};
};
enum class SettingKind { Toggle, Slider, Color };
using SettingMember = std::variant<bool Options::*, float Options::*, std::uint32_t Options::*>;
struct SettingSpec {
    std::string_view key, title, description, category, section;
    SettingKind kind;
    SettingMember member;
    float minimum{}, maximum{}, step{};
    std::string_view unit, dependsOn;
};
std::span<const SettingSpec> settingSpecs();
std::string settingValue(const Options& options, const SettingSpec& spec);
Color settingColor(std::uint32_t argb);

class Settings {
public:
    static Settings& get();
    void load(const std::filesystem::path& root);
    std::shared_ptr<const Options> snapshot() const;
    bool enabled() const;
    void enable(bool value);
    void change(std::string_view key, std::string_view value);
private:
    void publish(const Options& options);
    void save() const;
    mutable std::mutex mutex;
    std::filesystem::path path;
    std::shared_ptr<const Options> current{std::make_shared<const Options>()};
    std::atomic_bool master{true};
};
}
