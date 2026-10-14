#include "Json.hpp"
#include <utility>

namespace appleskin::json {
std::string quote(std::string_view value) {
    constexpr char digits[] = "0123456789abcdef";
    std::string result = "\"";
    for (const unsigned char byte : value) {
        if (byte == '"' || byte == '\\') { result += '\\'; result += static_cast<char>(byte); }
        else if (byte < 32) {
            result += "\\u00";
            result += digits[byte >> 4];
            result += digits[byte & 15];
        } else result += static_cast<char>(byte);
    }
    result += '"';
    return result;
}

namespace {
class Reader {
public:
    explicit Reader(std::string_view value) : source(value) {}
    bool parse(std::map<std::string, std::string>& values) {
        if (!take('{')) return false;
        if (take('}')) return finished();
        do {
            std::string key, value;
            if (!string(key) || !take(':')) return false;
            skip();
            if (cursor < source.size() && source[cursor] == '"') {
                if (!string(value)) return false;
            } else {
                const auto begin = cursor;
                while (cursor < source.size() && source[cursor] != ',' && source[cursor] != '}' &&
                    !whitespace(source[cursor])) {
                    if (source[cursor] == '{' || source[cursor] == '[' || source[cursor] == '"') return false;
                    ++cursor;
                }
                if (cursor == begin) return false;
                value = source.substr(begin, cursor - begin);
                if (value != "true" && value != "false" && value != "null" && !number(value)) return false;
            }
            values.insert_or_assign(std::move(key), std::move(value));
            if (take('}')) return finished();
        } while (take(','));
        return false;
    }
private:
    static bool whitespace(char c) { return c == ' ' || c == '\t' || c == '\r' || c == '\n'; }
    void skip() { while (cursor < source.size() && whitespace(source[cursor])) ++cursor; }
    bool take(char c) { skip(); if (cursor >= source.size() || source[cursor] != c) return false; ++cursor; return true; }
    bool finished() { skip(); return cursor == source.size(); }
    static bool number(std::string_view value) {
        std::size_t index = 0;
        if (index < value.size() && value[index] == '-') ++index;
        if (index >= value.size()) return false;
        if (value[index] == '0') ++index;
        else {
            if (value[index] < '1' || value[index] > '9') return false;
            while (index < value.size() && value[index] >= '0' && value[index] <= '9') ++index;
        }
        if (index < value.size() && value[index] == '.') {
            const auto begin = ++index;
            while (index < value.size() && value[index] >= '0' && value[index] <= '9') ++index;
            if (index == begin) return false;
        }
        if (index < value.size() && (value[index] == 'e' || value[index] == 'E')) {
            ++index;
            if (index < value.size() && (value[index] == '-' || value[index] == '+')) ++index;
            const auto begin = index;
            while (index < value.size() && value[index] >= '0' && value[index] <= '9') ++index;
            if (index == begin) return false;
        }
        return index == value.size();
    }
    bool string(std::string& result) {
        if (!take('"')) return false;
        while (cursor < source.size()) {
            const auto c = source[cursor++];
            if (c == '"') return true;
            if (static_cast<unsigned char>(c) < 32) return false;
            if (c != '\\') { result += c; continue; }
            if (cursor >= source.size()) return false;
            const auto escape = source[cursor++];
            switch (escape) {
                case '"': case '\\': case '/': result += escape; break;
                case 'b': result += '\b'; break;
                case 'f': result += '\f'; break;
                case 'n': result += '\n'; break;
                case 'r': result += '\r'; break;
                case 't': result += '\t'; break;
                case 'u': {
                    if (cursor + 4 > source.size()) return false;
                    unsigned code = 0;
                    for (int i = 0; i < 4; ++i) {
                        const auto digit = source[cursor++];
                        code <<= 4;
                        if (digit >= '0' && digit <= '9') code += digit - '0';
                        else if (digit >= 'a' && digit <= 'f') code += digit - 'a' + 10;
                        else if (digit >= 'A' && digit <= 'F') code += digit - 'A' + 10;
                        else return false;
                    }
                    if (code > 127) return false;
                    result += static_cast<char>(code);
                    break;
                }
                default: return false;
            }
        }
        return false;
    }
    std::string_view source;
    std::size_t cursor{};
};
}
bool object(std::string_view source, std::map<std::string, std::string>& values) {
    std::map<std::string, std::string> parsed;
    if (source.size() > 65536 || !Reader(source).parse(parsed)) return false;
    values = std::move(parsed);
    return true;
}
}
