#include "Food.hpp"
#include <algorithm>

namespace appleskin {
Food foodFromStack(const void* stack) {
    if (!stack || !read<std::uint8_t>(stack, build::field::StackValid) ||
        !read<std::uint8_t>(stack, build::field::StackCount)) return {};
    const auto handle = read<const void*>(stack, build::field::StackItemHandle);
    const auto item = handle ? read<const void*>(handle, 0) : nullptr;
    const auto getFood = method<const void* (*)(const void*)>(item, build::slot::ItemFood);
    const auto component = getFood ? getFood(item) : nullptr;
    if (!component) return {};
    const auto nutrition = method<int (*)(const void*)>(component, build::slot::FoodNutrition);
    const auto modifier = method<float (*)(const void*)>(component, build::slot::FoodSaturationModifier);
    const auto alwaysEat = method<bool (*)(const void*)>(component, build::slot::FoodAlwaysEat);
    if (!nutrition || !modifier || !alwaysEat) return {};
    const auto points = nutrition(component);
    const auto ratio = modifier(component);
    const auto saturation = static_cast<float>(points) * ratio * 2.0f;
    if (points < 0 || points > 1000 || !within(ratio, 0, 100) || !within(saturation, 0, 100000)) return {};
    return {points, saturation, alwaysEat(component), true};
}

HungerState hungerState(void* context, const Targets& targets) {
    if (!context) return {};
    const auto client = read<const void*>(context, build::field::UiClient);
    const auto getPlayer = method<const void* (*)(const void*)>(client, build::slot::LocalPlayer);
    const auto player = getPlayer ? getPlayer(client) : nullptr;
    if (!player) return {};
    const auto hungerAttribute = targets.lookup(player, targets.hungerKey);
    const auto saturationAttribute = targets.lookup(player, targets.saturationKey);
    if (!hungerAttribute || !saturationAttribute) return {};
    HungerState result;
    result.hunger = targets.value(hungerAttribute);
    result.saturation = targets.value(saturationAttribute);
    if (!within(result.hunger, 0, 20) || !within(result.saturation, 0, 20)) return {};
    if (const auto exhaustion = targets.lookup(player, targets.exhaustionKey)) {
        result.exhaustion = targets.value(exhaustion);
        result.exhaustionMaximum = targets.maximum(exhaustion);
        result.hasExhaustion = within(result.exhaustion, 0, 100000) &&
            within(result.exhaustionMaximum, 0, 100000);
    }
    result.player = player;
    const auto carried = method<const void* (*)(const void*)>(player, build::slot::CarriedItem);
    result.food = foodFromStack(carried ? carried(player) : nullptr);
    result.valid = true;
    return result;
}

HealthPreview healthPreview(const HungerState& state, const Targets& targets, bool previewFullHunger) {
    if (!state.valid || !state.food.valid ||
        (!previewFullHunger && !state.food.alwaysEat && state.hunger >= 20)) return {};
    auto hunger = std::min(20.0f, state.hunger + state.food.nutrition);
    if (hunger < 18) return {};
    const auto attribute = targets.lookup(state.player, targets.healthKey);
    if (!attribute) return {};
    const auto current = targets.value(attribute);
    const auto maximum = targets.maximum(attribute);
    if (!within(current, 0.01f, 512) || !within(maximum, current, 512) || current >= maximum) return {};
    const auto level = targets.level(state.player);
    const auto difficulty = method<int (*)(const void*)>(level, build::slot::LevelDifficulty);
    const auto getRules = method<const void* (*)(const void*)>(level, build::slot::LevelGameRules);
    if (difficulty && difficulty(level) == 0) return {};
    const auto rules = getRules ? getRules(level) : nullptr;
    if (rules && !targets.gameRule(rules, build::rule::NaturalRegeneration, true)) return {};
    for (const auto effect : {19, 20, 25})
        if (targets.hasEffect(state.player, effect)) return {};
    const auto component = targets.exhaustionComponent(state.player);
    const auto nativeCost = component ? read<float>(component, build::field::ExhaustionHeal) : -1.0f;
    const auto cost = within(nativeCost, 0, 100000) ? nativeCost : 6.0f;
    const auto exhaustionMaximum = state.hasExhaustion && state.exhaustionMaximum > 0 ? state.exhaustionMaximum : 20.0f;
    auto saturation = std::min(hunger, state.saturation + state.food.saturation);
    auto exhaustion = state.hasExhaustion ? std::min(state.exhaustion, exhaustionMaximum) : 0.0f;
    auto predicted = current;
    for (int step = 0; step < 512 && predicted < maximum; ++step) {
        auto drains = std::floor(exhaustion / 4.0f);
        exhaustion -= drains * 4.0f;
        const auto saturatedDrains = std::min(drains, std::ceil(saturation));
        saturation = std::max(0.0f, saturation - saturatedDrains);
        hunger = std::max(0.0f, hunger - (drains - saturatedDrains));
        if (hunger < 18) break;
        predicted = std::min(maximum, predicted + 1.0f);
        exhaustion = std::min(exhaustionMaximum, exhaustion + cost);
    }
    return {current, maximum, predicted, predicted > current};
}

int uiProfile(void* context) {
    if (!context) return -1;
    const auto client = read<const void*>(context, build::field::UiClient);
    const auto getOptions = method<const void* (*)(const void*)>(client, build::slot::ClientOptions);
    const auto options = getOptions ? getOptions(client) : nullptr;
    const auto profile = method<int (*)(const void*)>(options, build::slot::OptionsUiProfile);
    return profile ? profile(options) : -1;
}
}
