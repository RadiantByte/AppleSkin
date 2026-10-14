#include "Image.hpp"
#include <algorithm>
#include <elf.h>
#include <link.h>

namespace appleskin {
bool Image::locate() {
    segments.clear();
    buildId.clear();
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
            if (header.p_type != PT_NOTE) continue;
            auto cursor = begin;
            const auto end = begin + header.p_filesz;
            while (cursor + sizeof(Elf64_Nhdr) <= end) {
                const auto note = read<Elf64_Nhdr>(reinterpret_cast<void*>(cursor), 0);
                cursor += sizeof(note);
                const auto nameBytes = (static_cast<std::size_t>(note.n_namesz) + 3) & ~std::size_t(3);
                const auto dataBytes = (static_cast<std::size_t>(note.n_descsz) + 3) & ~std::size_t(3);
                if (nameBytes > end - cursor || dataBytes > end - cursor - nameBytes) break;
                if (note.n_type == NT_GNU_BUILD_ID && note.n_namesz == 4 &&
                    std::memcmp(reinterpret_cast<void*>(cursor), "GNU", 4) == 0) {
                    const auto* bytes = reinterpret_cast<const std::uint8_t*>(cursor + nameBytes);
                    image.buildId.assign(bytes, bytes + note.n_descsz);
                }
                cursor += nameBytes + dataBytes;
            }
        }
        return 1;
    }, this);
    return !segments.empty();
}

bool Image::supported() const {
    return buildId.size() == build::MinecraftBuildId.size() &&
        std::equal(buildId.begin(), buildId.end(), build::MinecraftBuildId.begin());
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

bool Image::resolve(Targets& output, VtableLookupFn hostResolver) const {
    if (!supported()) return false;
    std::uintptr_t hungerTable{}, heartTable{}, delegateTable{}, itemTable{}, uiTable{};
    if (!hostResolver) {
        hungerTable = vtable("17HudHungerRenderer", 18);
        heartTable = vtable("16HudHeartRenderer", 18);
        delegateTable = vtable("23HungerAttributeDelegate", 3);
        itemTable = vtable("4Item", 56);
        uiTable = vtable("24MinecraftUIRenderContext", 33);
        if (!hungerTable || !heartTable || !delegateTable || !itemTable || !uiTable) return false;
    }
    const auto entry = [&](std::string_view type, std::uintptr_t table, std::size_t slot) {
        if (hostResolver) return hostResolver(type, slot, "libminecraftpe.so");
        return read<std::uintptr_t>(reinterpret_cast<void*>(table), slot * 8);
    };
    const auto tick = entry("17HudHungerRenderer", hungerTable, build::slot::HungerTick);
    const auto render = entry("17HudHungerRenderer", hungerTable, build::slot::HungerRender);
    const auto heartTick = entry("16HudHeartRenderer", heartTable, build::slot::HeartTick);
    const auto heartRender = entry("16HudHeartRenderer", heartTable, build::slot::HeartRender);
    const auto delegate = entry("23HungerAttributeDelegate", delegateTable, build::slot::HungerDelegateTick);
    const auto hover = entry("4Item", itemTable, build::slot::ItemHoverText);
    const auto text = entry("24MinecraftUIRenderContext", uiTable, build::slot::UiDrawText);
    for (const auto address : {tick, render, heartTick, heartRender, delegate, hover, text})
        if (!contains(address, 4, PF_R | PF_X)) return false;
    const auto lookup = branch(tick + build::call::TickAttributeLookup);
    const auto current = branch(tick + build::call::TickAttributeCurrent);
    const auto batch = branch(render + build::call::RenderBatch);
    const auto meshValid = branch(render + build::call::RenderMeshValid);
    const auto position = branch(render + build::call::RenderPosition);
    const auto maximum = branch(heartTick + build::call::HeartTickMaximum);
    const auto hasEffect = branch(heartTick + build::call::HeartTickHasEffect);
    const auto level = branch(delegate + build::call::DelegateLevel);
    const auto gameRule = branch(delegate + build::call::DelegateGameRule);
    const auto component = branch(delegate + build::call::DelegateExhaustionComponent);
    if (!lookup || !current || !batch || !meshValid || !position || !maximum ||
        !hasEffect || !level || !gameRule || !component) return false;
    const auto hunger = addressPair(tick + build::call::TickHungerKey, 1);
    const auto saturation = addressPair(tick + build::call::TickSaturationKey, 1);
    const auto health = addressPair(heartTick + build::call::HeartTickHealthKey, 1);
    if (!hunger || !saturation || !health || hunger == saturation || hunger == health || saturation == health) return false;
    const auto exhaustion = saturation + build::field::SaturationToExhaustion;
    if (!contains(exhaustion, 8, PF_R) || exhaustion == hunger || exhaustion == health) return false;
    if (lookup != branch(tick + build::call::TickSecondLookup) ||
        lookup != branch(heartTick + build::call::HeartTickLookup) ||
        current != branch(tick + build::call::TickSecondCurrent) ||
        current != branch(heartTick + build::call::HeartTickCurrent) ||
        batch != branch(render + build::call::RenderBatchSecond) ||
        batch != branch(heartRender + build::call::HeartRenderBatch) ||
        batch != branch(heartRender + build::call::HeartRenderBatchSecond) ||
        meshValid != branch(heartRender + build::call::HeartRenderMeshValid) ||
        position != branch(heartRender + build::call::HeartRenderPosition) ||
        hasEffect != branch(heartTick + build::call::HeartTickHasEffectSecond) ||
        level != branch(delegate + build::call::DelegateLevelSecond)) return false;
    Targets result;
    result.hunger = reinterpret_cast<void*>(render);
    result.heart = reinterpret_cast<void*>(heartRender);
    result.hover = reinterpret_cast<void*>(hover);
    result.text = reinterpret_cast<void*>(text);
    result.lookup = reinterpret_cast<LookupFn>(lookup);
    result.value = reinterpret_cast<AttributeValueFn>(current);
    result.maximum = reinterpret_cast<AttributeValueFn>(maximum);
    result.meshValid = reinterpret_cast<MeshValidFn>(meshValid);
    result.position = reinterpret_cast<ControlPositionFn>(position);
    result.batch = reinterpret_cast<BatchFn>(batch);
    result.hasEffect = reinterpret_cast<HasEffectFn>(hasEffect);
    result.level = reinterpret_cast<ActorLevelFn>(level);
    result.gameRule = reinterpret_cast<GameRuleFn>(gameRule);
    result.exhaustionComponent = reinterpret_cast<ExhaustionComponentFn>(component);
    result.hungerKey = reinterpret_cast<void*>(hunger);
    result.saturationKey = reinterpret_cast<void*>(saturation);
    result.exhaustionKey = reinterpret_cast<void*>(exhaustion);
    result.healthKey = reinterpret_cast<void*>(health);
    output = result;
    return true;
}
}
