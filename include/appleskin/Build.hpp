#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace appleskin::build {
inline constexpr std::string_view Version = "1.0.0";
inline constexpr std::string_view Minecraft = "1.26.52.03_RC2";
inline constexpr std::array<std::uint8_t, 20> MinecraftBuildId{
    0x56, 0xde, 0x9e, 0xed, 0x07, 0x76, 0x31, 0xe0, 0x3a, 0x31,
    0xf4, 0xf5, 0x8e, 0xb2, 0xf0, 0xe0, 0x00, 0x71, 0xd3, 0x35
};
namespace slot {
inline constexpr std::size_t HungerTick = 4;
inline constexpr std::size_t HungerRender = 17;
inline constexpr std::size_t HeartTick = 4;
inline constexpr std::size_t HeartRender = 17;
inline constexpr std::size_t HungerDelegateTick = 2;
inline constexpr std::size_t ItemHoverText = 55;
inline constexpr std::size_t UiDrawText = 6;
inline constexpr std::size_t UiMeasureText = 2;
inline constexpr std::size_t UiDrawImage = 8;
inline constexpr std::size_t UiFlushImages = 10;
inline constexpr std::size_t UiGetTexture = 32;
inline constexpr std::size_t ClientOptions = 175;
inline constexpr std::size_t OptionsUiProfile = 15;
inline constexpr std::size_t LevelDifficulty = 37;
inline constexpr std::size_t LevelGameRules = 344;
inline constexpr std::size_t LocalPlayer = 32;
inline constexpr std::size_t CarriedItem = 78;
inline constexpr std::size_t ItemFood = 26;
inline constexpr std::size_t FoodNutrition = 2;
inline constexpr std::size_t FoodSaturationModifier = 3;
inline constexpr std::size_t FoodAlwaysEat = 4;
inline constexpr std::size_t FontLineHeight = 8;
}
namespace field {
inline constexpr std::size_t RendererAlpha = 0x8;
inline constexpr std::size_t RendererVisible = 0x11;
inline constexpr std::size_t RendererHungerEffect = 0x12;
inline constexpr std::size_t RendererTextures = 0x28;
inline constexpr std::size_t RendererMeshes = 0xe8;
inline constexpr std::size_t RendererPositions = 0xfb8;
inline constexpr std::size_t TextureStride = 0x20;
inline constexpr std::size_t TextureGpuPointer = 0x8;
inline constexpr std::size_t TextureGpuReady = 0x12;
inline constexpr std::size_t MeshStride = 0x278;
inline constexpr std::size_t UiClient = 0x8;
inline constexpr std::size_t UiScreen = 0x10;
inline constexpr std::size_t ControlPosition = 0x10;
inline constexpr std::size_t StackItemHandle = 0x8;
inline constexpr std::size_t StackCount = 0x22;
inline constexpr std::size_t StackValid = 0x23;
inline constexpr std::size_t AttributeId = 0x4;
inline constexpr std::ptrdiff_t SaturationToExhaustion = 0x30;
inline constexpr std::size_t ActorRegistry = 0x10;
inline constexpr std::size_t ActorIdentifier = 0x18;
inline constexpr std::size_t ExhaustionHeal = 0;
inline constexpr std::size_t HeartVisible = 0xd;
inline constexpr std::size_t HeartFullType = 0x28;
inline constexpr std::size_t HeartHalfType = 0x40;
inline constexpr std::size_t HeartTextures = 0xb8;
inline constexpr std::size_t HeartMeshes = 0x338;
inline constexpr std::size_t HeartPositions = 0x3498;
inline constexpr std::size_t HeartPositionsEnd = 0x34a0;
}
namespace call {
inline constexpr std::size_t TickAttributeLookup = 0x550;
inline constexpr std::size_t TickAttributeCurrent = 0x554;
inline constexpr std::size_t TickHungerKey = 0x540;
inline constexpr std::size_t TickSaturationKey = 0x588;
inline constexpr std::size_t TickSecondLookup = 0x598;
inline constexpr std::size_t TickSecondCurrent = 0x59c;
inline constexpr std::size_t RenderMeshValid = 0x17c;
inline constexpr std::size_t RenderPosition = 0x2c8;
inline constexpr std::size_t RenderBatch = 0x3a8;
inline constexpr std::size_t RenderBatchSecond = 0x3e4;
inline constexpr std::size_t HeartTickHealthKey = 0x1e4;
inline constexpr std::size_t HeartTickLookup = 0x1f4;
inline constexpr std::size_t HeartTickCurrent = 0x218;
inline constexpr std::size_t HeartTickMaximum = 0x224;
inline constexpr std::size_t HeartTickHasEffect = 0x1a94;
inline constexpr std::size_t HeartTickHasEffectSecond = 0x1d44;
inline constexpr std::size_t HeartRenderPosition = 0x154;
inline constexpr std::size_t HeartRenderMeshValid = 0x120;
inline constexpr std::size_t HeartRenderBatch = 0x28c;
inline constexpr std::size_t HeartRenderBatchSecond = 0x2e4;
inline constexpr std::size_t DelegateLevel = 0x68;
inline constexpr std::size_t DelegateLevelSecond = 0x98;
inline constexpr std::size_t DelegateGameRule = 0xb0;
inline constexpr std::size_t DelegateExhaustionComponent = 0x1b0;
}
namespace rule {
inline constexpr int NaturalRegeneration = 20;
}
namespace preloader {
inline constexpr std::size_t ContextId = 0x8;
inline constexpr std::size_t ContextRoot = 0xe0;
}
}
