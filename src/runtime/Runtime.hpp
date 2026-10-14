#pragma once

#include "Host.hpp"
#include "Image.hpp"
#include "Menu.hpp"
#include "Metadata.hpp"
#include <atomic>
#include <mutex>

namespace appleskin {
class Runtime {
public:
    static Runtime& get();
    bool load(const ModLocation& location);
    bool enable();
    bool disable();
    bool unload();
    void libraryLoaded(const char* path);
    bool active() const;
    const Targets& native() const;
private:
    bool install();
    std::mutex mutex;
    Host host;
    Menu menu;
    Image image;
    Targets targets;
    std::atomic_bool wanted{}, running{};
    bool installed{};
};
}
