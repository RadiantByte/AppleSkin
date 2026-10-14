#pragma once

#include <appleskin/Native.hpp>
#include <array>

namespace appleskin {
struct TooltipRow {
    Vec2 position;
    float value{};
    float size{};
    bool saturation{};
};
struct TooltipRows {
    std::array<TooltipRow, 2> rows;
    std::size_t count{};
    Color outline;
    float outlineOpacity{1}, borderWidth{1};
};
void appendFoodTooltip(const void* stack, RedactableText* text);
TooltipRows captureTooltipRows(void* context, void* font, const Rect& rectangle,
    const std::string& text, int alignment, const TextStyle& style);
void renderTooltipRows(void* context, const TooltipRows& rows, float alpha);
}
