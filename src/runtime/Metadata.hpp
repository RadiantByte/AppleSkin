#pragma once

#include <cstdint>
#include <filesystem>
#include <string>

struct PLModInfo {
    std::uint32_t size;
    const char* mod_id;
    const char* display_name;
    const char* author;
    const char* version;
    const char* entry_path;
    const char* entry_file_name;
    const char* library_path;
    const char* icon_path;
    const char* manifest_path;
    const char* mod_root_path;
};

namespace appleskin {
struct ModLocation {
    std::string owner;
    std::filesystem::path root;
};
ModLocation contextLocation(const void* context);
ModLocation legacyLocation(const PLModInfo* info);
}
