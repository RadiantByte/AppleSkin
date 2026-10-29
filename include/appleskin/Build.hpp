#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

namespace appleskin::build {
inline constexpr std::string_view Version = "1.1.1";
inline constexpr std::string_view Minecraft = "1.26.60";
namespace slot {
inline constexpr std::size_t HungerTick = 4;
inline constexpr std::size_t HungerRender = 17;
inline constexpr std::size_t HeartTick = 4;
inline constexpr std::size_t HeartRender = 17;
inline constexpr std::size_t HungerDelegateTick = 2;
inline constexpr std::size_t ItemHoverText = 55;
inline constexpr std::size_t UiDrawText = 6;
inline constexpr std::size_t UiDrawTextView = 7;
inline constexpr std::size_t UiFinish = 31;
inline constexpr std::size_t UiMeasureText = 2;
inline constexpr std::size_t UiDrawImage = 9;
inline constexpr std::size_t UiFlushImages = 11;
inline constexpr std::size_t UiGetTexture = 33;
inline constexpr std::size_t ClientOptions = 173;
inline constexpr std::size_t OptionsUiProfile = 15;
inline constexpr std::size_t LevelDifficulty = 37;
inline constexpr std::size_t LevelGameRules = 344;
inline constexpr std::size_t LocalPlayer = 31;
inline constexpr std::size_t CarriedItem = 78;
inline constexpr std::size_t ItemFood = 26;
inline constexpr std::size_t FoodNutrition = 2;
inline constexpr std::size_t FoodSaturationModifier = 3;
inline constexpr std::size_t FoodAlwaysEat = 4;
inline constexpr std::size_t FontLineHeight = 8;
inline constexpr std::size_t FontMeasureText = 7;
}
namespace field {
inline constexpr std::size_t RendererAlpha = 0x8;
inline constexpr std::size_t RendererVisible = 0x11;
inline constexpr std::size_t RendererHungerEffect = 0x12;
inline constexpr std::size_t RendererTextures = 0x28;
inline constexpr std::size_t RendererMeshes = 0xe8;
inline constexpr std::size_t RendererPositions = 0xf28;
inline constexpr std::size_t TextureStride = 0x20;
inline constexpr std::size_t TextureGpuPointer = 0x0;
inline constexpr std::size_t TextureGpuReady = 0x12;
inline constexpr std::size_t MeshStride = 0x260;
inline constexpr std::size_t UiClient = 0x8;
inline constexpr std::size_t UiScreen = 0x10;
inline constexpr std::size_t ControlPosition = 0x10;
inline constexpr std::size_t StackItemHandle = 0x8;
inline constexpr std::size_t StackCount = 0x22;
inline constexpr std::size_t StackValid = 0x23;
inline constexpr std::size_t AttributeId = 0x4;
inline constexpr std::size_t ActorRegistry = 0x10;
inline constexpr std::size_t ActorIdentifier = 0x18;
inline constexpr std::size_t ExhaustionHeal = 0;
inline constexpr std::size_t HeartVisible = 0xd;
inline constexpr std::size_t HeartFullType = 0x28;
inline constexpr std::size_t HeartHalfType = 0x40;
inline constexpr std::size_t HeartTextures = 0xb8;
inline constexpr std::size_t HeartMeshes = 0x338;
inline constexpr std::size_t HeartPositions = 0x32b8;
inline constexpr std::size_t HeartPositionsEnd = 0x32c0;
}
namespace rule {
inline constexpr int NaturalRegeneration = 20;
}
namespace preloader {
inline constexpr std::size_t ContextId = 0x8;
inline constexpr std::size_t ContextRoot = 0xe0;
}
}
