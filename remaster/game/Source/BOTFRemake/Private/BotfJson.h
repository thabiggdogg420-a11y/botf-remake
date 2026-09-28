#pragma once
// Minimal engine-independent JSON reader for the client's local data files (LocalAssets/resolved-manifest.json).
//
// Deliberately small: objects, arrays, strings (with \uXXXX escapes to UTF-8, surrogate pairs included), numbers,
// true/false/null. A UTF-8 BOM (PowerShell's Set-Content -Encoding UTF8 writes one) is skipped. Parse errors return
// false and leave a message; they never throw. Compiled and tested outside Unreal (Tools/client-logic-tests.cpp).

#include <cstddef>
#include <cstdlib>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace botf::json {

struct Value {
    enum class Kind { Null, Bool, Number, String, Array, Object };
    Kind kind = Kind::Null;
    bool boolean = false;
    double number = 0.0;
    std::string text;
    std::vector<Value> items;
    std::vector<std::string> keys;  // Object member names in source order; values[i] belongs to keys[i].
    std::vector<Value> values;

    bool isNull() const { return kind == Kind::Null; }
    bool isString() const { return kind == Kind::String; }
    bool isNumber() const { return kind == Kind::Number; }
    bool isArray() const { return kind == Kind::Array; }
    bool isObject() const { return kind == Kind::Object; }
    const Value* find(const std::string& key) const {
        if (kind != Kind::Object) return nullptr;
        for (std::size_t i = 0; i < keys.size() && i < values.size(); ++i)
            if (keys[i] == key) return &values[i];
        return nullptr;
    }
    std::string stringOr(const std::string& key, const std::string& fallback = {}) const {
        const Value* v = find(key);
        return v && v->isString() ? v->text : fallback;
    }
    double numberOr(const std::string& key, double fallback) const {
        const Value* v = find(key);
        return v && v->isNumber() ? v->number : fallback;
    }
    std::vector<std::string> stringsAt(const std::string& key) const {
        std::vector<std::string> out;
        const Value* v = find(key);
        if (v && v->isArray())
            for (const auto& item : v->items)
                if (item.isString()) out.push_back(item.text);
        // PowerShell's ConvertTo-Json writes a one-element array as a bare string in some versions.
        if (v && v->isString() && !v->text.empty()) out.push_back(v->text);
        return out;
    }
};

class Reader {
public:
    explicit Reader(const std::string& source) : s(source) {}

    bool parse(Value& out) {
        if (s.size() >= 3 && static_cast<unsigned char>(s[0]) == 0xEF && static_cast<unsigned char>(s[1]) == 0xBB &&
            static_cast<unsigned char>(s[2]) == 0xBF)
            pos = 3;
        if (!value(out, 0)) return false;
        skipSpace();
        if (pos != s.size()) return fail("trailing characters");
        return true;
    }
    const std::string& error() const { return message; }

private:
    const std::string& s;
    std::size_t pos = 0;
    std::string message;

    bool fail(const char* what) {
        if (message.empty()) message = std::string(what) + " at offset " + std::to_string(pos);
        return false;
    }
    void skipSpace() {
        while (pos < s.size() && (s[pos] == ' ' || s[pos] == '\t' || s[pos] == '\n' || s[pos] == '\r')) ++pos;
    }
    bool literal(const char* word) {
        std::size_t length = 0;
        while (word[length]) ++length;
        if (s.compare(pos, length, word) != 0) return fail("bad literal");
        pos += length;
        return true;
    }
    bool value(Value& out, int depth) {
        if (depth > 64) return fail("nesting too deep");
        skipSpace();
        if (pos >= s.size()) return fail("unexpected end");
        const char c = s[pos];
        if (c == '{') return object(out, depth);
        if (c == '[') return array(out, depth);
        if (c == '"') { out.kind = Value::Kind::String; return string(out.text); }
        if (c == 't') { out.kind = Value::Kind::Bool; out.boolean = true; return literal("true"); }
        if (c == 'f') { out.kind = Value::Kind::Bool; out.boolean = false; return literal("false"); }
        if (c == 'n') { out.kind = Value::Kind::Null; return literal("null"); }
        return numberValue(out);
    }
    bool numberValue(Value& out) {
        const std::size_t start = pos;
        if (pos < s.size() && (s[pos] == '-' || s[pos] == '+')) ++pos;
        while (pos < s.size() && ((s[pos] >= '0' && s[pos] <= '9') || s[pos] == '.' || s[pos] == 'e' || s[pos] == 'E' ||
                                  s[pos] == '-' || s[pos] == '+'))
            ++pos;
        if (pos == start) return fail("unexpected character");
        const std::string digits = s.substr(start, pos - start);
        char* end = nullptr;
        out.kind = Value::Kind::Number;
        out.number = std::strtod(digits.c_str(), &end);
        if (!end || *end != '\0') return fail("bad number");
        return true;
    }
    static void appendUtf8(std::string& out, unsigned long code) {
        if (code < 0x80) out.push_back(static_cast<char>(code));
        else if (code < 0x800) {
            out.push_back(static_cast<char>(0xC0 | (code >> 6)));
            out.push_back(static_cast<char>(0x80 | (code & 0x3F)));
        } else if (code < 0x10000) {
            out.push_back(static_cast<char>(0xE0 | (code >> 12)));
            out.push_back(static_cast<char>(0x80 | ((code >> 6) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | (code & 0x3F)));
        } else {
            out.push_back(static_cast<char>(0xF0 | (code >> 18)));
            out.push_back(static_cast<char>(0x80 | ((code >> 12) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | ((code >> 6) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | (code & 0x3F)));
        }
    }
    bool hex4(unsigned long& code) {
        if (pos + 4 > s.size()) return fail("short \\u escape");
        code = 0;
        for (int i = 0; i < 4; ++i) {
            const char h = s[pos++];
            code <<= 4;
            if (h >= '0' && h <= '9') code |= static_cast<unsigned long>(h - '0');
            else if (h >= 'a' && h <= 'f') code |= static_cast<unsigned long>(h - 'a' + 10);
            else if (h >= 'A' && h <= 'F') code |= static_cast<unsigned long>(h - 'A' + 10);
            else return fail("bad \\u escape");
        }
        return true;
    }
    bool string(std::string& out) {
        ++pos; // opening quote
        out.clear();
        while (pos < s.size()) {
            const char c = s[pos++];
            if (c == '"') return true;
            if (c != '\\') { out.push_back(c); continue; }
            if (pos >= s.size()) break;
            const char e = s[pos++];
            switch (e) {
            case '"': out.push_back('"'); break;
            case '\\': out.push_back('\\'); break;
            case '/': out.push_back('/'); break;
            case 'b': out.push_back('\b'); break;
            case 'f': out.push_back('\f'); break;
            case 'n': out.push_back('\n'); break;
            case 'r': out.push_back('\r'); break;
            case 't': out.push_back('\t'); break;
            case 'u': {
                unsigned long code = 0;
                if (!hex4(code)) return false;
                if (code >= 0xD800 && code <= 0xDBFF && pos + 6 <= s.size() && s[pos] == '\\' && s[pos + 1] == 'u') {
                    pos += 2;
                    unsigned long low = 0;
                    if (!hex4(low)) return false;
                    code = 0x10000 + ((code - 0xD800) << 10) + (low - 0xDC00);
                }
                appendUtf8(out, code);
                break;
            }
            default: return fail("bad escape");
            }
        }
        return fail("unterminated string");
    }
    bool array(Value& out, int depth) {
        out.kind = Value::Kind::Array;
        ++pos;
        skipSpace();
        if (pos < s.size() && s[pos] == ']') { ++pos; return true; }
        while (true) {
            out.items.emplace_back();
            if (!value(out.items.back(), depth + 1)) return false;
            skipSpace();
            if (pos < s.size() && s[pos] == ',') { ++pos; continue; }
            if (pos < s.size() && s[pos] == ']') { ++pos; return true; }
            return fail("expected , or ]");
        }
    }
    bool object(Value& out, int depth) {
        out.kind = Value::Kind::Object;
        ++pos;
        skipSpace();
        if (pos < s.size() && s[pos] == '}') { ++pos; return true; }
        while (true) {
            skipSpace();
            if (pos >= s.size() || s[pos] != '"') return fail("expected key");
            std::string key;
            if (!string(key)) return false;
            skipSpace();
            if (pos >= s.size() || s[pos] != ':') return fail("expected :");
            ++pos;
            out.keys.push_back(std::move(key));
            out.values.emplace_back();
            if (!value(out.values.back(), depth + 1)) return false;
            skipSpace();
            if (pos < s.size() && s[pos] == ',') { ++pos; continue; }
            if (pos < s.size() && s[pos] == '}') { ++pos; return true; }
            return fail("expected , or }");
        }
    }
};

inline bool parse(const std::string& source, Value& out, std::string* error = nullptr) {
    Reader reader(source);
    const bool ok = reader.parse(out);
    if (!ok && error) *error = reader.error();
    return ok;
}

} // namespace botf::json
