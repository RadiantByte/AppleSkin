#include "FoodTooltip.hpp"
#include "Painter.hpp"
#include "game/Food.hpp"
#include "config/Settings.hpp"
#include <algorithm>
#include <cstdio>

namespace appleskin {
namespace {
constexpr std::string_view marker = "\xc2\xa7r\xc2\xa7" "0\xc2\xa7" "0\xc2\xa7r";
constexpr std::string_view space = "                       ";
constexpr std::string_view foodLabel = "Food  ";
constexpr std::string_view saturationLabel = "Sat.  ";

std::string number(float value) {
    char buffer[32];
    std::snprintf(buffer, sizeof(buffer), "%.2f", static_cast<double>(value));
    std::string result(buffer);
    while (!result.empty() && result.back() == '0') result.pop_back();
    if (!result.empty() && result.back() == '.') result.pop_back();
    return result;
}

void appendRows(std::string& text, const Food& food, bool icons) {
    if (text.find(marker) != text.npos) return;
    const auto field = icons ? space : std::string_view{};
    text.append("\n").append(marker).append(foodLabel).append(field).append(" \xc2\xa7" "7");
    text.append(number(static_cast<float>(food.nutrition))).append("\xc2\xa7r");
    text.append("\n").append(marker).append(saturationLabel).append(field).append(" \xc2\xa7" "e");
    text.append(number(food.saturation)).append("\xc2\xa7r");
}

bool parseValue(std::string_view text, float& result) {
    result = 0;
    bool digit = false, fractional = false;
    float place = 0.1f;
    for (const auto value : text) {
        if (value == '.' && !fractional) { fractional = true; continue; }
        if (value < '0' || value > '9') break;
        digit = true;
        if (fractional) { result += (value - '0') * place; place *= 0.1f; }
        else result = result * 10 + (value - '0');
        if (!within(result, 0, 100000)) return false;
    }
    return digit;
}
}

void appendFoodTooltip(const void* stack, RedactableText* text) {
    const auto options = Settings::get().snapshot();
    if (!options->tooltips) return;
    if (!text) return;
    const auto food = foodFromStack(stack);
    if (!food.valid) return;
    appendRows(text->original, food, options->tooltipIcons);
    if (text->hasFiltered) appendRows(text->filtered, food, options->tooltipIcons);
}

namespace {
std::size_t skipFormatting(std::string_view text, std::size_t at, std::size_t end) {
    while (at + 2 < end && text.substr(at, 2) == "\xc2\xa7") at += 3;
    return at;
}

template<class Measure>
TooltipRows captureRows(void* font, const Rect& rectangle, std::string_view text,
    int alignment, const TextStyle& style, Measure measure) {
    TooltipRows result;
    const auto options = Settings::get().snapshot();
    if (!options->tooltips || !options->tooltipIcons || !font) return result;
    if (alignment != 0 || !within(style.scale, 0.25f, 4) || text.size() > 16384 ||
        (text.find(foodLabel) == text.npos && text.find(saturationLabel) == text.npos)) return result;
    result.outline = settingColor(options->saturationColor);
    result.outlineOpacity = options->saturationOpacity * 0.01f;
    result.borderWidth = options->borderWidth;
    const auto height = method<float (*)(void*)>(font, build::slot::FontLineHeight);
    if (!height) return result;
    const auto step = height(font) * style.scale + style.padding;
    if (!within(step, 4, 96)) return result;
    std::size_t lineStart = 0, line = 0;
    while (lineStart < text.size() && result.count < result.rows.size()) {
        const auto newline = text.find('\n', lineStart);
        const auto lineEnd = newline == text.npos ? text.size() : newline;
        const auto labelStart = skipFormatting(text, lineStart, lineEnd);
        const bool saturation = text.substr(labelStart, saturationLabel.size()) == saturationLabel;
        const auto label = saturation ? saturationLabel : foodLabel;
        if (text.substr(labelStart, label.size()) == label) {
            const auto fieldStart = labelStart + label.size();
            if (fieldStart + space.size() < lineEnd && text.substr(fieldStart, space.size()) == space) {
                auto valueStart = fieldStart + space.size();
                while (valueStart < lineEnd && text[valueStart] == ' ') ++valueStart;
                valueStart = skipFormatting(text, valueStart, lineEnd);
                float value = 0;
                if (parseValue(text.substr(valueStart, lineEnd - valueStart), value)) {
                    const auto x = rectangle.left + measure(text.substr(lineStart, fieldStart - lineStart));
                    const auto y = rectangle.top + static_cast<float>(line) * step;
                    const auto iconSize = std::min(9.0f * style.scale, measure(space) / 10.0f);
                    if (within(x, -32768, 32768) && within(y, -32768, 32768) && within(iconSize, 1, 36))
                        result.rows[result.count++] = {{x, y}, value, iconSize, saturation};
                }
            }
        }
        if (newline == text.npos) break;
        lineStart = newline + 1;
        ++line;
    }
    return result;
}
}

TooltipRows captureTooltipRows(void* context, void* font, const Rect& rectangle,
    std::string_view text, int alignment, const TextStyle& style) {
    const auto measure = method<float (*)(void*, void*, const std::string&, float, bool)>(context, build::slot::UiMeasureText);
    if (!measure) return {};
    return captureRows(font, rectangle, text, alignment, style, [&](std::string_view value) {
        const std::string owned(value);
        return measure(context, font, owned, style.scale, style.showFormatting);
    });
}

TooltipRows captureFontTooltipRows(void* font, std::string_view text, float x, float y) {
    const auto measure = method<float (*)(void*, std::string_view, float, bool)>(font, build::slot::FontMeasureText);
    if (!measure) return {};
    return captureRows(font, Rect{x, x, y, y}, text, 0, TextStyle{}, [&](std::string_view value) {
        return measure(font, value, 1.0f, false);
    });
}

void renderTooltipRows(void* context, const TooltipRows& rows, float alpha) {
    if (!rows.count || !within(alpha, 0, 1) || alpha == 0) return;
    Painter painter(context);
    if (!painter.available()) return;
    painter.start();
    for (std::size_t row = 0; row < rows.count; ++row) {
        const auto& data = rows.rows[row];
        const auto count = static_cast<int>(std::ceil(std::min(data.value, 20.0f) / 2.0f));
        for (int icon = 0; icon < count; ++icon) {
            const Vec2 position{data.position.x + icon * data.size, data.position.y};
            painter.hunger(position, data.size, false, true);
            painter.hunger(position, data.size, data.value - icon * 2 < 2);
        }
    }
    painter.finish(Color{}, alpha);
    for (std::size_t row = 0; row < rows.count; ++row) {
        const auto& data = rows.rows[row];
        if (!data.saturation) continue;
        for (int icon = 0; icon < 10; ++icon)
            painter.saturation({data.position.x + icon * data.size, data.position.y}, data.size,
                0, data.value * 0.5f - icon, rows.borderWidth);
    }
    painter.finish(rows.outline, alpha * rows.outlineOpacity);
}
}
