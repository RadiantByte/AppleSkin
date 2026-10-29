#pragma once

#include <appleskin/Native.hpp>
#include <vector>

namespace appleskin {
class Image {
public:
    bool locate();
    bool contains(std::uintptr_t address, std::size_t bytes, unsigned flags) const;
    std::uintptr_t vtable(std::string_view type, std::size_t slots) const;
    std::uintptr_t branch(std::uintptr_t address) const;
    std::uintptr_t addressPair(std::uintptr_t address, unsigned reg) const;
    bool resolve(Targets& targets) const;
private:
    struct Segment { std::uintptr_t begin{}, end{}; unsigned flags{}; };
    bool signatures(std::vector<std::uintptr_t>& matches) const;
    std::vector<Segment> segments;
};
}
