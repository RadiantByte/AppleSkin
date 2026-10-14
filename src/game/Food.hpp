#pragma once

#include <appleskin/Native.hpp>

namespace appleskin {
struct Food {
    int nutrition{};
    float saturation{};
    bool alwaysEat{};
    bool valid{};
};
struct HungerState {
    float hunger{}, saturation{};
    float exhaustion{}, exhaustionMaximum{};
    const void* player{};
    Food food;
    bool valid{}, hasExhaustion{};
};
struct HealthPreview {
    float current{}, maximum{}, predicted{};
    bool valid{};
};
Food foodFromStack(const void* stack);
HungerState hungerState(void* context, const Targets& targets);
HealthPreview healthPreview(const HungerState& state, const Targets& targets, bool previewFullHunger);
int uiProfile(void* context);
}
