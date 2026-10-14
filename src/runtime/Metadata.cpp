#include "Metadata.hpp"
#include <appleskin/Build.hpp>
#include <dlfcn.h>

namespace appleskin {
namespace {
ModLocation fallbackLocation() {
    ModLocation result;
    if (auto* preloader = dlopen("libpreloader.so", RTLD_NOW | RTLD_NOLOAD)) {
        const auto current = reinterpret_cast<const void* (*)()>(dlsym(preloader,
            "_ZN2pl3mod9NativeMod7currentEv"));
        if (current) result = contextLocation(current());
        dlclose(preloader);
    }
    if (result.root.empty()) {
        Dl_info library{};
        if (dladdr(reinterpret_cast<const void*>(fallbackLocation), &library) && library.dli_fname)
            result.root = std::filesystem::path(library.dli_fname).parent_path();
    }
    return result;
}
}

ModLocation contextLocation(const void* context) {
    if (!context) return {};
    const auto bytes = static_cast<const std::byte*>(context);
    return {*reinterpret_cast<const std::string*>(bytes + build::preloader::ContextId),
        *reinterpret_cast<const std::filesystem::path*>(bytes + build::preloader::ContextRoot)};
}

ModLocation legacyLocation(const PLModInfo* info) {
    auto result = fallbackLocation();
    if (!info || info->size < sizeof(PLModInfo)) return result;
    if (info->mod_id) result.owner = info->mod_id;
    if (info->mod_root_path && *info->mod_root_path) result.root = info->mod_root_path;
    return result;
}
static_assert(sizeof(std::filesystem::path) == 24);
static_assert(offsetof(PLModInfo, mod_root_path) == 80);
static_assert(sizeof(PLModInfo) == 88);
}
