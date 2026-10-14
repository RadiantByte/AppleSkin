#pragma once

#include "MenuAbi.hpp"

namespace appleskin {
class Menu {
public:
    bool connect(std::string_view owner);
    void disconnect();
private:
    using Register = bool (*)(const pl::modmenu::ModuleInfo&);
    using Unregister = void (*)(std::string_view);
    using SetSchema = bool (*)(std::string_view, std::string_view);
    void* handle{};
    Unregister unregister{};
    bool registered{};
};
}
