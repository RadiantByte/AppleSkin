#pragma once

#include <map>
#include <string>
#include <string_view>

namespace appleskin::json {
std::string quote(std::string_view value);
bool object(std::string_view source, std::map<std::string, std::string>& values);
}
