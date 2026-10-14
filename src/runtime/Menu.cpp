#include "Menu.hpp"
#include "config/Settings.hpp"
#include "config/Json.hpp"
#include <dlfcn.h>
#include <sstream>
#include <locale>

namespace appleskin {
namespace {
constexpr std::string_view moduleId = "appleskin";

std::string decimal(float value) {
    std::ostringstream result;
    result.imbue(std::locale::classic());
    result << value;
    return result.str();
}

std::string schema() {
    std::string result = R"({"version":2,"default_category":"hud","categories":[{"id":"hud","title":"HUD","description":"Food previews and status displays"},{"id":"tooltips","title":"Tooltips","description":"Food information in item descriptions"},{"id":"appearance","title":"Appearance","description":"Colors, animation, and bar size"}],"nodes":[)";
    struct Section { const char* id; const char* title; const char* category; };
    constexpr Section sections[]{
        {"food_previews", "Food previews", "hud"},
        {"saturation_display", "Saturation", "hud"},
        {"exhaustion_display", "Exhaustion", "hud"},
        {"tooltip_display", "Food information", "tooltips"},
        {"preview_style", "Preview animation", "appearance"},
        {"outline_style", "Saturation outline", "appearance"},
        {"bar_style", "Exhaustion bar", "appearance"}
    };
    bool first = true;
    const auto begin = [&] { if (!first) result += ','; first = false; };
    for (const auto& section : sections) {
        begin();
        result += "{\"id\":" + json::quote(section.id) + ",\"type\":\"section\",\"title\":" +
            json::quote(section.title) + ",\"category\":" + json::quote(section.category) + '}';
    }
    const Options defaults;
    for (const auto& spec : settingSpecs()) {
        begin();
        const auto type = spec.kind == SettingKind::Toggle ? "toggle" :
            spec.kind == SettingKind::Color ? "color" : "slider_float";
        result += "{\"id\":" + json::quote(spec.key) + ",\"key\":" + json::quote(spec.key) +
            ",\"title\":" + json::quote(spec.title) + ",\"description\":" + json::quote(spec.description) +
            ",\"category\":" + json::quote(spec.category) + ",\"section\":" + json::quote(spec.section) +
            ",\"type\":" + json::quote(type) + ",\"default_value\":" + json::quote(settingValue(defaults, spec));
        if (spec.kind == SettingKind::Slider)
            result += ",\"min_value\":" + json::quote(decimal(spec.minimum)) + ",\"max_value\":" +
                json::quote(decimal(spec.maximum)) + ",\"step\":" + json::quote(decimal(spec.step)) +
                ",\"unit\":" + json::quote(spec.unit);
        if (spec.kind == SettingKind::Color) result += ",\"color_alpha\":true";
        if (!spec.dependsOn.empty())
            result += ",\"visible_when\":[{\"key\":" + json::quote(spec.dependsOn) + ",\"op\":\"truthy\",\"value\":\"\"}]";
        result += '}';
    }
    result += "]}";
    return result;
}
}

bool Menu::connect(std::string_view owner) {
    if (registered) return true;
    disconnect();
    handle = dlopen("libpreloader.so", RTLD_NOW | RTLD_NOLOAD);
    if (!handle) return false;
    const auto registration = reinterpret_cast<Register>(dlsym(handle,
        "_ZN2pl7modmenu14registerModuleERKNS0_10ModuleInfoE"));
    unregister = reinterpret_cast<Unregister>(dlsym(handle,
        "_ZN2pl7modmenu16unregisterModuleENSt6__ndk117basic_string_viewIcNS1_11char_traitsIcEEEE"));
    const auto setSchema = reinterpret_cast<SetSchema>(dlsym(handle,
        "_ZN2pl7modmenu19setConfigSchemaJsonENSt6__ndk117basic_string_viewIcNS1_11char_traitsIcEEEES5_"));
    if (!registration || !unregister) { disconnect(); return false; }
    pl::modmenu::ModuleInfo module;
    module.moduleId = moduleId;
    module.displayName = "AppleSkin";
    module.description = "Food and healing previews, saturation outlines, and food tooltips.";
    module.modId = owner;
    module.defaultEnabled = Settings::get().enabled();
    module.hideInHudEditor = true;
    module.onToggle = [](std::string_view, bool value) { Settings::get().enable(value); };
    module.onConfigChanged = [](std::string_view, std::string_view key, std::string_view value) {
        Settings::get().change(key, value);
    };
    const auto options = Settings::get().snapshot();
    for (const auto& spec : settingSpecs()) {
        const auto type = spec.kind == SettingKind::Toggle ? pl::modmenu::ConfigType::Toggle :
            spec.kind == SettingKind::Color ? pl::modmenu::ConfigType::Color : pl::modmenu::ConfigType::SliderFloat;
        module.configs.push_back({std::string(spec.key), std::string(spec.title), type,
            settingValue(*options, spec), spec.kind == SettingKind::Slider ? decimal(spec.minimum) : "",
            spec.kind == SettingKind::Slider ? decimal(spec.maximum) : "", std::string(spec.dependsOn)});
    }
    registered = registration(module);
    if (!registered) { disconnect(); return false; }
    if (setSchema) setSchema(moduleId, schema());
    return true;
}

void Menu::disconnect() {
    if (registered && unregister) unregister(moduleId);
    registered = false;
    unregister = nullptr;
    if (handle) dlclose(handle);
    handle = nullptr;
}
}
