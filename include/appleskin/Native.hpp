#pragma once

#include "Build.hpp"
#include <cmath>
#include <cstring>
#include <memory>
#include <string>

namespace appleskin {
struct Vec2 { float x{}, y{}; };
struct Vec3 { float x{}, y{}, z{}; };
struct Rect { float left{}, right{}, top{}, bottom{}; };
struct Color { float r{1}, g{1}, b{1}, a{1}; };
struct TextStyle {
    float scale{1}, padding{};
    bool shadow{}, showFormatting{}, hideHyphen{};
};
struct Caret { int position{}; bool visible{}; };
struct RedactableText {
    std::string original;
    std::string filtered;
    bool hasFiltered{};
};
struct TextureData;
struct ResourceKey {
    int source{};
    std::string path;
    std::uint64_t pathHash{}, hash{};
    explicit ResourceKey(const char* name);
};
struct Texture {
    std::shared_ptr<const TextureData> data;
    std::shared_ptr<ResourceKey> location;
};
struct MaterialKey {
    std::uint64_t hash{};
    std::string name;
    const MaterialKey* cached{};
    explicit MaterialKey(const char* value);
};
using HungerFn = void (*)(void*, void*, void*, void*, int, void*);
using HeartFn = HungerFn;
using HoverFn = void (*)(const void*, const void*, void*, RedactableText*, bool);
using TextFn = void (*)(void*, void*, const Rect&, std::string&, const Color&, int, float, const TextStyle&, const Caret&);
using TextViewFn = void (*)(void*, void*, const Rect&, std::string_view, const Color&, int, float, const TextStyle&, const Caret&);
using FontDrawFn = void (*)(void*, void*, std::string_view, float, float, const Color&, int, float, const void*, int, void*);
using FinishFn = void (*)(void*);
using LookupFn = const void* (*)(const void*, const void*);
using AttributeValueFn = float (*)(const void*);
using MeshValidFn = bool (*)(const void*);
using ControlPositionFn = const Vec2* (*)(void*);
using BatchFn = void (*)(void*, const void*, const void*, std::size_t, const Vec3*, float, const void*);
using HasEffectFn = bool (*)(const void*, int);
using ActorLevelFn = const void* (*)(const void*);
using GameRuleFn = bool (*)(const void*, int, bool);
using ExhaustionComponentFn = const void* (*)(const void*);
struct Targets {
    void* hunger{};
    void* heart{};
    void* hover{};
    void* text{};
    void* textView{};
    void* fontDraw{};
    void* finish{};
    LookupFn lookup{};
    AttributeValueFn value{};
    AttributeValueFn maximum{};
    MeshValidFn meshValid{};
    ControlPositionFn position{};
    BatchFn batch{};
    HasEffectFn hasEffect{};
    ActorLevelFn level{};
    GameRuleFn gameRule{};
    ExhaustionComponentFn exhaustionComponent{};
    const void* hungerKey{};
    const void* saturationKey{};
    const void* exhaustionKey{};
    const void* healthKey{};
};
template<class T> T read(const void* object, std::size_t offset) {
    T result;
    std::memcpy(&result, static_cast<const std::byte*>(object) + offset, sizeof(result));
    return result;
}
template<class F> F method(const void* object, std::size_t slot) {
    if (!object) return nullptr;
    const auto table = read<const void* const*>(object, 0);
    return table ? reinterpret_cast<F>(const_cast<void*>(table[slot])) : nullptr;
}
inline bool within(float value, float low, float high) {
    return std::isfinite(value) && value >= low && value <= high;
}
inline bool textureReady(const void* data) {
    if (!data) return false;
    const auto texture = read<const std::byte*>(data, build::field::TextureGpuPointer);
    return texture && (__atomic_load_n(reinterpret_cast<const std::uint8_t*>(texture + build::field::TextureGpuReady), __ATOMIC_ACQUIRE) & 1);
}
static_assert(sizeof(void*) == 8);
static_assert(sizeof(std::string) == 24);
static_assert(sizeof(std::string_view) == 16);
static_assert(sizeof(Texture) == 32);
static_assert(offsetof(ResourceKey, path) == 8);
static_assert(offsetof(ResourceKey, pathHash) == 32);
static_assert(sizeof(ResourceKey) == 48);
static_assert(sizeof(MaterialKey) == 40);
static_assert(offsetof(RedactableText, filtered) == 24);
static_assert(offsetof(RedactableText, hasFiltered) == 48);
}
