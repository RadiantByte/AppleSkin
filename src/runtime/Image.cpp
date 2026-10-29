#include "Image.hpp"
#include "Patterns.hpp"
#include <android/log.h>
#include <algorithm>
#include <elf.h>
#include <link.h>

namespace appleskin {
bool Image::locate() {
    segments.clear();
    dl_iterate_phdr([](dl_phdr_info* info, std::size_t, void* argument) {
        std::string_view name(info->dlpi_name ? info->dlpi_name : "");
        name.remove_prefix(name.find_last_of('/') == name.npos ? 0 : name.find_last_of('/') + 1);
        if (name != "libminecraftpe.so") return 0;
        auto& image = *static_cast<Image*>(argument);
        for (std::size_t index = 0; index < info->dlpi_phnum; ++index) {
            const auto& header = info->dlpi_phdr[index];
            const auto begin = info->dlpi_addr + header.p_vaddr;
            if (header.p_type == PT_LOAD && (header.p_flags & PF_R))
                image.segments.push_back({begin, begin + header.p_memsz, header.p_flags});
        }
        return 1;
    }, this);
    return !segments.empty();
}

bool Image::contains(std::uintptr_t address, std::size_t bytes, unsigned flags) const {
    for (const auto& segment : segments)
        if ((segment.flags & flags) == flags && address >= segment.begin &&
            address <= segment.end && bytes <= segment.end - address) return true;
    return false;
}

std::uintptr_t Image::vtable(std::string_view type, std::size_t slots) const {
    const std::string needle = std::string(type) + '\0';
    std::vector<std::uintptr_t> names;
    for (const auto& segment : segments) {
        const auto* begin = reinterpret_cast<const char*>(segment.begin);
        const auto* end = reinterpret_cast<const char*>(segment.end);
        while (begin < end) {
            const auto* found = std::search(begin, end, needle.begin(), needle.end());
            if (found == end) break;
            names.push_back(reinterpret_cast<std::uintptr_t>(found));
            begin = found + needle.size();
        }
    }
    if (names.empty()) return 0;
    std::vector<std::uintptr_t> typeInfos;
    for (const auto& segment : segments) {
        if (segment.flags & PF_X) continue;
        for (auto address = (segment.begin + 7) & ~std::uintptr_t(7); address + 8 <= segment.end; address += 8) {
            const auto pointer = read<std::uintptr_t>(reinterpret_cast<void*>(address), 0);
            if (std::find(names.begin(), names.end(), pointer) != names.end() && address >= segment.begin + 8)
                typeInfos.push_back(address - 8);
        }
    }
    if (typeInfos.empty()) return 0;
    std::uintptr_t answer = 0;
    for (const auto& segment : segments) {
        if (segment.flags & PF_X) continue;
        for (auto address = (segment.begin + 15) & ~std::uintptr_t(7); address + (slots + 1) * 8 <= segment.end; address += 8) {
            const auto pointer = read<std::uintptr_t>(reinterpret_cast<void*>(address), 0);
            if (std::find(typeInfos.begin(), typeInfos.end(), pointer) == typeInfos.end()) continue;
            if (read<std::intptr_t>(reinterpret_cast<void*>(address - 8), 0) != 0) continue;
            bool valid = true;
            for (std::size_t slot = 0; slot < slots; ++slot) {
                const auto target = read<std::uintptr_t>(reinterpret_cast<void*>(address + 8), slot * 8);
                valid &= contains(target, 4, PF_R | PF_X);
            }
            if (!valid) continue;
            if (answer && answer != address + 8) return 0;
            answer = address + 8;
        }
    }
    return answer;
}

std::uintptr_t Image::branch(std::uintptr_t address) const {
    if (!contains(address, 4, PF_R | PF_X)) return 0;
    const auto instruction = read<std::uint32_t>(reinterpret_cast<void*>(address), 0);
    if ((instruction & 0xfc000000u) != 0x94000000u) return 0;
    const auto displacement = static_cast<std::int64_t>(static_cast<std::int32_t>(instruction << 6) >> 4);
    const auto target = address + displacement;
    return contains(target, 4, PF_R | PF_X) ? target : 0;
}

std::uintptr_t Image::addressPair(std::uintptr_t address, unsigned reg) const {
    if (!contains(address, 8, PF_R | PF_X)) return 0;
    const auto adrp = read<std::uint32_t>(reinterpret_cast<void*>(address), 0);
    const auto add = read<std::uint32_t>(reinterpret_cast<void*>(address), 4);
    if ((adrp & 0x9f00001fu) != (0x90000000u | reg) ||
        (add & 0xffc003ffu) != (0x91000000u | (reg << 5) | reg)) return 0;
    const auto immediate = ((adrp >> 5) & 0x7ffffu) * 4u + ((adrp >> 29) & 3u);
    const auto signedPage = static_cast<std::int32_t>(immediate << 11) >> 11;
    const auto target = (address & ~std::uintptr_t(0xfff)) + static_cast<std::int64_t>(signedPage) * 4096 + ((add >> 10) & 0xfffu);
    return contains(target, 8, PF_R) ? target : 0;
}

namespace {
struct CheckByte { std::size_t offset; std::uint8_t value; };
struct Compiled {
    std::vector<CheckByte> bytes;
    std::size_t length{};
    std::uint32_t word{};
    std::uintptr_t match{};
    unsigned count{};
};
int digit(char value) {
    return value >= '0' && value <= '9' ? value - '0' : value - 'A' + 10;
}
Compiled compile(const pattern::Signature& signature) {
    Compiled result;
    auto text = signature.bytes;
    while (!text.empty()) {
        while (!text.empty() && text.front() == ' ') text.remove_prefix(1);
        if (text.empty()) break;
        const auto end = text.find(' ');
        const auto token = text.substr(0, end);
        if (token.front() != '?')
            result.bytes.push_back({result.length,
                static_cast<std::uint8_t>((digit(token[0]) << 4) | digit(token[1]))});
        ++result.length;
        if (end == text.npos) break;
        text.remove_prefix(end + 1);
    }
    for (const auto& byte : result.bytes)
        if (byte.offset >= signature.anchor && byte.offset < signature.anchor + 4)
            result.word |= static_cast<std::uint32_t>(byte.value) << ((byte.offset - signature.anchor) * 8);
    return result;
}
}

bool Image::signatures(std::vector<std::uintptr_t>& matches) const {
    std::array<Compiled, pattern::Count> compiled;
    std::vector<std::vector<std::size_t>> buckets(65536);
    for (std::size_t index = 0; index < compiled.size(); ++index) {
        compiled[index] = compile(pattern::Signatures[index]);
        buckets[compiled[index].word & 0xffffu].push_back(index);
    }
    for (const auto& segment : segments) {
        if (!(segment.flags & PF_X)) continue;
        for (auto address = (segment.begin + 3) & ~std::uintptr_t(3); address + 4 <= segment.end; address += 4) {
            const auto word = read<std::uint32_t>(reinterpret_cast<void*>(address), 0);
            for (const auto index : buckets[word & 0xffffu]) {
                auto& value = compiled[index];
                const auto anchor = pattern::Signatures[index].anchor;
                if (value.count > 1 || value.word != word || address - segment.begin < anchor) continue;
                const auto start = address - anchor;
                if (value.length > segment.end - start) continue;
                bool valid = true;
                for (const auto& byte : value.bytes)
                    if (read<std::uint8_t>(reinterpret_cast<void*>(start), byte.offset) != byte.value) {
                        valid = false;
                        break;
                    }
                if (valid) {
                    value.match = start;
                    ++value.count;
                }
            }
        }
    }
    matches.clear();
    for (std::size_t index = 0; index < compiled.size(); ++index) {
        if (compiled[index].count != 1) {
            __android_log_print(ANDROID_LOG_ERROR, "AppleSkin", "Missing or ambiguous signature: %s", pattern::Signatures[index].name);
            return false;
        }
        matches.push_back(compiled[index].match);
    }
    return true;
}

bool Image::resolve(Targets& output) const {
    output = {};
    using namespace pattern;
    std::vector<std::uintptr_t> at;
    if (!signatures(at)) return false;
    const auto hungerTable = vtable("17HudHungerRenderer", 18);
    const auto heartTable = vtable("16HudHeartRenderer", 18);
    const auto itemTable = vtable("4Item", 56);
    const auto uiTable = vtable("24MinecraftUIRenderContext", 34);
    if (!hungerTable || !heartTable || !itemTable || !uiTable) {
        __android_log_print(ANDROID_LOG_ERROR, "AppleSkin", "Missing or ambiguous native vtable");
        return false;
    }
    const auto entry = [](std::uintptr_t table, std::size_t slot) {
        return read<std::uintptr_t>(reinterpret_cast<void*>(table), slot * 8);
    };
    const auto belongs = [](std::uintptr_t address, std::uintptr_t method, std::size_t limit) {
        return address >= method && address - method < limit;
    };
    if (!belongs(at[HungerHeader], entry(hungerTable, build::slot::HungerRender), 0x80) ||
        !belongs(at[HeartHeader], entry(heartTable, build::slot::HeartRender), 0x80) ||
        !belongs(at[HungerRead], entry(hungerTable, build::slot::HungerTick), 0x1000) ||
        !belongs(at[SaturationRead], entry(hungerTable, build::slot::HungerTick), 0x1000) ||
        !belongs(at[HealthRead], entry(heartTable, build::slot::HeartTick), 0x4000)) {
        __android_log_print(ANDROID_LOG_ERROR, "AppleSkin", "Native render vtable does not match signatures");
        return false;
    }
    const auto instruction = [&](std::uintptr_t address) {
        return contains(address, 4, PF_R | PF_X) ? read<std::uint32_t>(reinterpret_cast<void*>(address), 0) : 0u;
    };
    const auto immediate = [&](std::uintptr_t address, std::uint32_t opcode, std::size_t scale, std::size_t value) {
        const auto word = instruction(address);
        return (word & 0xffc00000u) == opcode && ((word >> 10) & 0xfffu) * scale == value;
    };
    const auto lookup = branch(at[HungerRead] + 16);
    const auto current = branch(at[HungerRead] + 20);
    const auto maximum = branch(at[HealthRead] + 60);
    const auto meshValid = branch(at[MeshRead] + 20);
    const auto position = branch(at[PositionRead] + 4);
    const auto batch = branch(at[BatchRead] + 36);
    const auto effect = branch(at[EffectRead] + 8);
    const auto level = branch(at[LevelRead] + 4);
    const auto rule = branch(at[RuleRead] + 32);
    const auto component = branch(at[ComponentRead] + 4);
    const auto fontDraw = branch(at[NativeText] + 40);
    for (const auto address : {lookup, current, maximum, meshValid, position, batch, effect, level, rule, component, fontDraw})
        if (!address) return false;
    if (lookup != branch(at[SaturationRead] + 16) || current != branch(at[SaturationRead] + 20) ||
        lookup != branch(at[HealthRead] + 12) || current != branch(at[HealthRead] + 48) ||
        level != branch(at[RuleRead] + 8)) return false;
    const auto hunger = addressPair(at[HungerRead], 1);
    const auto saturation = addressPair(at[SaturationRead], 1);
    const auto health = addressPair(at[HealthRead], 1);
    const auto exhaustion = addressPair(at[ExhaustionRead], 1);
    std::array keys{hunger, saturation, health, exhaustion};
    std::sort(keys.begin(), keys.end());
    if (!keys.front() || std::adjacent_find(keys.begin(), keys.end()) != keys.end()) return false;
    const auto screen = branch(at[MeshRead] + 4);
    const bool layout =
        immediate(at[HungerHeader], 0x39400000u, 1, build::field::RendererVisible) &&
        immediate(at[HungerHeader] + 8, 0x91000000u, 1, build::field::RendererTextures) &&
        immediate(at[HungerHeader] + 32, 0xf9400000u, 8, build::field::TextureGpuPointer) &&
        immediate(at[HungerHeader] + 52, 0x91000000u, 1, build::field::TextureGpuReady) &&
        immediate(at[MeshRead] + 8, 0x91000000u, 1, build::field::RendererMeshes) &&
        immediate(at[HungerLayout] + 20, 0x91000000u, 1, build::field::RendererPositions) &&
        ((instruction(at[HungerLayout] + 12) >> 5) & 0xffffu) == build::field::MeshStride &&
        immediate(at[BatchRead], 0x39400000u, 1, build::field::RendererHungerEffect) &&
        immediate(at[BatchRead] + 4, 0xbd400000u, 4, build::field::RendererAlpha) &&
        instruction(at[BatchRead] + 32) == 0xaa1f03e5u &&
        immediate(at[HeartHeader], 0x39400000u, 1, build::field::HeartVisible) &&
        immediate(at[HeartHeader] + 8, 0x91000000u, 1, build::field::HeartTextures) &&
        immediate(at[HeartMesh], 0x91000000u, 1, build::field::HeartMeshes) &&
        immediate(at[HeartMesh] + 28, 0x91000000u, 1, build::field::MeshStride) &&
        immediate(at[HeartLayout] + 20, 0xf9400000u, 8, build::field::HeartPositions) &&
        immediate(at[HeartLayout] + 24, 0xf9400000u, 8, build::field::HeartPositionsEnd) &&
        immediate(at[PlayerRead] + 8, 0xf9400000u, 8, build::slot::LocalPlayer * 8) &&
        immediate(at[OptionsRead] + 4, 0xf9400000u, 8, build::slot::ClientOptions * 8) &&
        immediate(at[OptionsRead] + 20, 0xf9400000u, 8, build::slot::OptionsUiProfile * 8) &&
        immediate(at[LevelRead] + 12, 0xf9400000u, 8, build::slot::LevelDifficulty * 8) &&
        immediate(at[RuleRead] + 16, 0xf9400000u, 8, build::slot::LevelGameRules * 8) &&
        ((instruction(at[RuleRead] + 24) >> 5) & 0xffffu) == build::rule::NaturalRegeneration &&
        immediate(current, 0xbd400000u, 4, 0x7c) &&
        immediate(maximum, 0xbd400000u, 4, 0x78) &&
        immediate(screen, 0xf9400000u, 8, build::field::UiScreen) &&
        immediate(entry(itemTable, build::slot::ItemFood), 0xf9400000u, 8, 0x180);
    if (!layout) {
        __android_log_print(ANDROID_LOG_ERROR, "AppleSkin", "Native layout does not match Minecraft 26.60");
        return false;
    }
    Targets result;
    result.hunger = reinterpret_cast<void*>(entry(hungerTable, build::slot::HungerRender));
    result.heart = reinterpret_cast<void*>(entry(heartTable, build::slot::HeartRender));
    result.hover = reinterpret_cast<void*>(entry(itemTable, build::slot::ItemHoverText));
    result.text = reinterpret_cast<void*>(entry(uiTable, build::slot::UiDrawText));
    result.textView = reinterpret_cast<void*>(entry(uiTable, build::slot::UiDrawTextView));
    result.fontDraw = reinterpret_cast<void*>(fontDraw);
    result.finish = reinterpret_cast<void*>(entry(uiTable, build::slot::UiFinish));
    result.lookup = reinterpret_cast<LookupFn>(lookup);
    result.value = reinterpret_cast<AttributeValueFn>(current);
    result.maximum = reinterpret_cast<AttributeValueFn>(maximum);
    result.meshValid = reinterpret_cast<MeshValidFn>(meshValid);
    result.position = reinterpret_cast<ControlPositionFn>(position);
    result.batch = reinterpret_cast<BatchFn>(batch);
    result.hasEffect = reinterpret_cast<HasEffectFn>(effect);
    result.level = reinterpret_cast<ActorLevelFn>(level);
    result.gameRule = reinterpret_cast<GameRuleFn>(rule);
    result.exhaustionComponent = reinterpret_cast<ExhaustionComponentFn>(component);
    result.hungerKey = reinterpret_cast<void*>(hunger);
    result.saturationKey = reinterpret_cast<void*>(saturation);
    result.exhaustionKey = reinterpret_cast<void*>(exhaustion);
    result.healthKey = reinterpret_cast<void*>(health);
    output = result;
    return true;
}
}
