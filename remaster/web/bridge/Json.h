#pragma once
// Minimal, dependency-free JSON writer and reader for the web bridge.
// The writer streams directly into a string; the reader builds a small value tree.
// Only what the bridge needs: objects, arrays, strings, 64-bit integers, doubles, booleans, null.

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

namespace botfweb::json {

// ----------------------------------------------------------------------------------------------
// Writer
// ----------------------------------------------------------------------------------------------
class Writer {
public:
    std::string out;

    Writer& beginObject() { comma(); out += '{'; stack.push_back(true); return *this; }
    Writer& endObject() { out += '}'; stack.pop_back(); markValue(); return *this; }
    Writer& beginArray() { comma(); out += '['; stack.push_back(true); return *this; }
    Writer& endArray() { out += ']'; stack.pop_back(); markValue(); return *this; }

    Writer& key(std::string_view name) {
        comma();
        writeString(name);
        out += ':';
        afterKey = true;
        return *this;
    }

    Writer& value(std::string_view text) { comma(); writeString(text); markValue(); return *this; }
    Writer& value(const char* text) { return text ? value(std::string_view(text)) : null(); }
    Writer& value(const std::string& text) { return value(std::string_view(text)); }
    Writer& value(bool flag) { comma(); out += flag ? "true" : "false"; markValue(); return *this; }
    template <typename T, std::enable_if_t<std::is_integral_v<T> && !std::is_same_v<T, bool>, int> = 0>
    Writer& value(T number) {
        comma();
        // JavaScript numbers are doubles: integers beyond 2^53 are written as strings to stay exact.
        if constexpr (std::is_signed_v<T>) {
            const long long v = static_cast<long long>(number);
            if (v > 9007199254740991LL || v < -9007199254740991LL) writeString(std::to_string(v));
            else out += std::to_string(v);
        } else {
            const unsigned long long v = static_cast<unsigned long long>(number);
            if (v > 9007199254740991ULL) writeString(std::to_string(v));
            else out += std::to_string(v);
        }
        markValue();
        return *this;
    }
    Writer& value(double number) {
        comma();
        if (!std::isfinite(number)) out += "null";
        else {
            char buffer[40];
            std::snprintf(buffer, sizeof buffer, "%.10g", number);
            out += buffer;
        }
        markValue();
        return *this;
    }
    Writer& null() { comma(); out += "null"; markValue(); return *this; }
    // Inserts pre-serialised JSON verbatim.
    Writer& raw(std::string_view json) { comma(); out += json; markValue(); return *this; }

    template <typename T> Writer& field(std::string_view name, const T& v) { key(name); return value(v); }

private:
    std::vector<bool> stack;   // true while the container has no element yet
    bool afterKey = false;

    void comma() {
        if (afterKey) { afterKey = false; return; }
        if (!stack.empty()) {
            if (!stack.back()) out += ',';
        }
    }
    void markValue() {
        if (!stack.empty()) stack.back() = false;
    }
    void writeString(std::string_view text) {
        out += '"';
        for (unsigned char c : text) {
            switch (c) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            case '\b': out += "\\b"; break;
            case '\f': out += "\\f"; break;
            default:
                if (c < 0x20) {
                    char buffer[8];
                    std::snprintf(buffer, sizeof buffer, "\\u%04x", c);
                    out += buffer;
                } else out += static_cast<char>(c);
            }
        }
        out += '"';
    }
};

// ----------------------------------------------------------------------------------------------
// Reader
// ----------------------------------------------------------------------------------------------
struct Value {
    enum class Kind { Null, Bool, Number, String, Array, Object };
    Kind kind = Kind::Null;
    bool boolean = false;
    double number = 0;
    std::int64_t integer = 0;
    bool isInteger = false;
    std::string text;
    std::vector<Value> items;
    std::map<std::string, Value> members;

    bool isNull() const { return kind == Kind::Null; }
    bool isObject() const { return kind == Kind::Object; }
    bool isArray() const { return kind == Kind::Array; }
    bool isString() const { return kind == Kind::String; }
    bool isNumber() const { return kind == Kind::Number; }
    bool isBool() const { return kind == Kind::Bool; }

    bool has(const std::string& name) const {
        if (kind != Kind::Object) return false;
        const auto it = members.find(name);
        return it != members.end() && !it->second.isNull();
    }
    const Value& operator[](const std::string& name) const {
        static const Value missing;
        if (kind != Kind::Object) return missing;
        const auto it = members.find(name);
        return it == members.end() ? missing : it->second;
    }
    std::int64_t asInt(std::int64_t fallback = 0) const {
        if (kind == Kind::Number) return isInteger ? integer : static_cast<std::int64_t>(number);
        if (kind == Kind::Bool) return boolean ? 1 : 0;
        if (kind == Kind::String && !text.empty()) {
            try { std::size_t used = 0; const auto v = std::stoll(text, &used); if (used == text.size()) return v; } catch (...) {}
        }
        return fallback;
    }
    std::uint64_t asUInt(std::uint64_t fallback = 0) const {
        if (kind == Kind::String && !text.empty()) {
            try { std::size_t used = 0; const auto v = std::stoull(text, &used); if (used == text.size()) return v; } catch (...) {}
            return fallback;
        }
        const auto v = asInt(static_cast<std::int64_t>(fallback));
        return v < 0 ? fallback : static_cast<std::uint64_t>(v);
    }
    double asDouble(double fallback = 0) const {
        if (kind == Kind::Number) return isInteger ? static_cast<double>(integer) : number;
        return fallback;
    }
    bool asBool(bool fallback = false) const {
        if (kind == Kind::Bool) return boolean;
        if (kind == Kind::Number) return asInt() != 0;
        return fallback;
    }
    std::string asString(const std::string& fallback = {}) const {
        if (kind == Kind::String) return text;
        if (kind == Kind::Number) return isInteger ? std::to_string(integer) : std::to_string(number);
        return fallback;
    }
    int getInt(const std::string& name, int fallback = 0) const {
        return has(name) ? static_cast<int>((*this)[name].asInt(fallback)) : fallback;
    }
    bool getBool(const std::string& name, bool fallback = false) const {
        return has(name) ? (*this)[name].asBool(fallback) : fallback;
    }
    std::string getString(const std::string& name, const std::string& fallback = {}) const {
        return has(name) ? (*this)[name].asString(fallback) : fallback;
    }
};

class Parser {
public:
    explicit Parser(std::string_view source) : s(source) {}
    Value parse() {
        skip();
        Value v = parseValue(0);
        skip();
        if (i != s.size()) fail("Unexpected trailing characters");
        return v;
    }

private:
    std::string_view s;
    std::size_t i = 0;

    [[noreturn]] void fail(const char* why) const {
        throw std::runtime_error(std::string("Invalid JSON: ") + why + " at offset " + std::to_string(i));
    }
    void skip() {
        while (i < s.size() && (s[i] == ' ' || s[i] == '\n' || s[i] == '\r' || s[i] == '\t')) ++i;
    }
    bool consume(char c) {
        if (i < s.size() && s[i] == c) { ++i; return true; }
        return false;
    }
    Value parseValue(int depth) {
        if (depth > 64) fail("Nesting too deep");
        if (i >= s.size()) fail("Unexpected end");
        const char c = s[i];
        if (c == '{') return parseObject(depth);
        if (c == '[') return parseArray(depth);
        if (c == '"') { Value v; v.kind = Value::Kind::String; v.text = parseString(); return v; }
        if (c == 't' && s.substr(i, 4) == "true") { i += 4; Value v; v.kind = Value::Kind::Bool; v.boolean = true; return v; }
        if (c == 'f' && s.substr(i, 5) == "false") { i += 5; Value v; v.kind = Value::Kind::Bool; v.boolean = false; return v; }
        if (c == 'n' && s.substr(i, 4) == "null") { i += 4; return Value{}; }
        if (c == '-' || (c >= '0' && c <= '9')) return parseNumber();
        fail("Unexpected character");
    }
    Value parseObject(int depth) {
        Value v; v.kind = Value::Kind::Object;
        ++i; skip();
        if (consume('}')) return v;
        while (true) {
            skip();
            if (i >= s.size() || s[i] != '"') fail("Expected key");
            std::string name = parseString();
            skip();
            if (!consume(':')) fail("Expected ':'");
            skip();
            v.members[name] = parseValue(depth + 1);
            skip();
            if (consume(',')) continue;
            if (consume('}')) return v;
            fail("Expected ',' or '}'");
        }
    }
    Value parseArray(int depth) {
        Value v; v.kind = Value::Kind::Array;
        ++i; skip();
        if (consume(']')) return v;
        while (true) {
            skip();
            v.items.push_back(parseValue(depth + 1));
            skip();
            if (consume(',')) continue;
            if (consume(']')) return v;
            fail("Expected ',' or ']'");
        }
    }
    static void appendUtf8(std::string& out, std::uint32_t cp) {
        if (cp < 0x80) out += static_cast<char>(cp);
        else if (cp < 0x800) { out += static_cast<char>(0xC0 | (cp >> 6)); out += static_cast<char>(0x80 | (cp & 0x3F)); }
        else if (cp < 0x10000) {
            out += static_cast<char>(0xE0 | (cp >> 12)); out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
            out += static_cast<char>(0x80 | (cp & 0x3F));
        } else {
            out += static_cast<char>(0xF0 | (cp >> 18)); out += static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
            out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F)); out += static_cast<char>(0x80 | (cp & 0x3F));
        }
    }
    std::uint32_t hex4() {
        if (i + 4 > s.size()) fail("Truncated escape");
        std::uint32_t v = 0;
        for (int k = 0; k < 4; ++k) {
            const char c = s[i++];
            v <<= 4;
            if (c >= '0' && c <= '9') v |= static_cast<std::uint32_t>(c - '0');
            else if (c >= 'a' && c <= 'f') v |= static_cast<std::uint32_t>(c - 'a' + 10);
            else if (c >= 'A' && c <= 'F') v |= static_cast<std::uint32_t>(c - 'A' + 10);
            else fail("Bad hex escape");
        }
        return v;
    }
    std::string parseString() {
        ++i;   // opening quote
        std::string out;
        while (true) {
            if (i >= s.size()) fail("Unterminated string");
            const char c = s[i++];
            if (c == '"') return out;
            if (c != '\\') { out += c; continue; }
            if (i >= s.size()) fail("Unterminated escape");
            const char e = s[i++];
            switch (e) {
            case '"': out += '"'; break;
            case '\\': out += '\\'; break;
            case '/': out += '/'; break;
            case 'b': out += '\b'; break;
            case 'f': out += '\f'; break;
            case 'n': out += '\n'; break;
            case 'r': out += '\r'; break;
            case 't': out += '\t'; break;
            case 'u': {
                std::uint32_t cp = hex4();
                if (cp >= 0xD800 && cp <= 0xDBFF && i + 1 < s.size() && s[i] == '\\' && s[i + 1] == 'u') {
                    i += 2;
                    const std::uint32_t low = hex4();
                    if (low >= 0xDC00 && low <= 0xDFFF) cp = 0x10000 + ((cp - 0xD800) << 10) + (low - 0xDC00);
                }
                appendUtf8(out, cp);
                break;
            }
            default: fail("Unknown escape");
            }
        }
    }
    Value parseNumber() {
        const std::size_t start = i;
        bool integral = true;
        if (s[i] == '-') ++i;
        while (i < s.size() && s[i] >= '0' && s[i] <= '9') ++i;
        if (i < s.size() && s[i] == '.') { integral = false; ++i; while (i < s.size() && s[i] >= '0' && s[i] <= '9') ++i; }
        if (i < s.size() && (s[i] == 'e' || s[i] == 'E')) {
            integral = false; ++i;
            if (i < s.size() && (s[i] == '+' || s[i] == '-')) ++i;
            while (i < s.size() && s[i] >= '0' && s[i] <= '9') ++i;
        }
        const std::string token(s.substr(start, i - start));
        if (token.empty() || token == "-") fail("Bad number");
        Value v; v.kind = Value::Kind::Number;
        if (integral) {
            try { v.integer = std::stoll(token); v.isInteger = true; v.number = static_cast<double>(v.integer); return v; }
            catch (...) { /* fall through to double */ }
        }
        v.number = std::stod(token);
        v.isInteger = false;
        return v;
    }
};

inline Value parse(std::string_view text) { return Parser(text).parse(); }

} // namespace botfweb::json
