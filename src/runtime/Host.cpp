#include "Host.hpp"
#include <dlfcn.h>

namespace appleskin {
bool Host::connect() {
    if (hook && unhook) return true;
    handle = dlopen("libpreloader.so", RTLD_NOW | RTLD_NOLOAD);
    if (!handle) return false;
    hook = reinterpret_cast<Hook>(dlsym(handle, "_ZN2pl6memory4hookEPvS1_PS1_NS0_12HookPriorityE"));
    unhook = reinterpret_cast<Unhook>(dlsym(handle, "_ZN2pl6memory6unhookEPvS1_"));
    resolveVtable = reinterpret_cast<VtableLookupFn>(dlsym(handle,
        "_ZN2pl6memory21resolveVtableFunctionENSt6__ndk117basic_string_viewIcNS1_11char_traitsIcEEEEmS5_"));
    if (hook && unhook) return true;
    dlclose(handle);
    handle = nullptr;
    hook = nullptr;
    unhook = nullptr;
    resolveVtable = nullptr;
    return false;
}

VtableLookupFn Host::vtableResolver() const { return resolveVtable; }

bool Host::install(void* target, void* replacement, void** continuation) {
    if (!target || !replacement || !continuation || !hook || !unhook) return false;
    for (const auto& entry : entries)
        if (entry.target == target && entry.replacement == replacement) return true;
    if (hook(target, replacement, continuation, 200) != 0) return false;
    entries.push_back({target, replacement});
    return true;
}

bool Host::removeAll() {
    bool complete = true;
    for (auto index = entries.size(); index > 0; --index) {
        const auto entry = entries[index - 1];
        if (unhook(entry.target, entry.replacement)) entries.erase(entries.begin() + index - 1);
        else complete = false;
    }
    if (complete && handle) {
        dlclose(handle);
        handle = nullptr;
        hook = nullptr;
        unhook = nullptr;
        resolveVtable = nullptr;
    }
    return complete;
}
}
