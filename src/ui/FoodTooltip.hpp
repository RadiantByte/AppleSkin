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
    std::string_view text, int alignment, const TextStyle& style);
TooltipRows captureFontTooltipRows(void* font, std::string_view text, float x, float y);
void renderTooltipRows(void* context, const TooltipRows& rows, float alpha);
}
