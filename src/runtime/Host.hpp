#pragma once

#include <appleskin/Native.hpp>
#include <vector>

namespace appleskin {
class Host {
public:
    bool connect();
    bool install(void* target, void* replacement, void** continuation);
    bool removeAll();
private:
    using Hook = int (*)(void*, void*, void**, int);
    using Unhook = bool (*)(void*, void*);
    struct Entry { void* target; void* replacement; };
    void* handle{};
    Hook hook{};
    Unhook unhook{};
    std::vector<Entry> entries;
};
}
