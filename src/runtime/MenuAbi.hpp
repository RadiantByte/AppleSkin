#pragma once

#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace pl::modmenu {
enum class ConfigType { Toggle, SliderInt, SliderFloat, Radio, Color, Keybind, Text, Button };
struct ConfigEntry {
    std::string key, displayName;
    ConfigType type{};
    std::string defaultValue, minValue, maxValue, dependsOn;
};
struct ModuleInfo {
    std::string moduleId, displayName, description, modId;
    bool defaultEnabled{}, hideInHudEditor{};
    std::vector<ConfigEntry> configs;
    std::function<void(std::string_view, bool)> onToggle;
    std::function<void(std::string_view, std::string_view, std::string_view)> onConfigChanged;
    std::function<void(std::string_view, std::string_view, bool)> onKeybind;
};
}
