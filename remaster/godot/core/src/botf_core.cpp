#include "botf_core.h"

#include <godot_cpp/classes/json.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/array.hpp>

#include <cmath>

extern "C" const char* botf_call(const char* method, const char* args);

namespace botfgd {
using namespace godot;

BotfCore* BotfCore::singleton = nullptr;

namespace {
// JSON numbers parse as float in Godot; turn integral ones back into int (the bridge writes
// integers beyond 2^53 as strings, so every integral number here is exact).
Variant normalize(const Variant& v) {
    switch (v.get_type()) {
        case Variant::FLOAT: {
            const double d = v;
            if (std::isfinite(d) && std::floor(d) == d && std::fabs(d) <= 9007199254740992.0) return static_cast<int64_t>(d);
            return v;
        }
        case Variant::ARRAY: {
            Array in = v;
            for (int64_t i = 0; i < in.size(); ++i) in[i] = normalize(in[i]);
            return in;
        }
        case Variant::DICTIONARY: {
            Dictionary in = v;
            Array keys = in.keys();
            for (int64_t i = 0; i < keys.size(); ++i) in[keys[i]] = normalize(in[keys[i]]);
            return in;
        }
        default: return v;
    }
}
} // namespace

String BotfCore::request_json(const String& method, const String& args_json) {
    const CharString m = method.utf8();
    const CharString a = args_json.utf8();
    return String::utf8(botf_call(m.get_data(), a.get_data()));
}

Dictionary BotfCore::request(const String& method, const Dictionary& args) {
    const String text = request_json(method, JSON::stringify(args));
    Ref<JSON> json;
    json.instantiate();
    Dictionary reply;
    if (json->parse(text) != OK) {
        reply["ok"] = false;
        reply["error"] = String("BotfCore: bridge returned invalid JSON: ") + json->get_error_message();
        return reply;
    }
    return normalize(json->get_data());
}

void BotfCore::_bind_methods() {
    ClassDB::bind_method(D_METHOD("request", "method", "args"), &BotfCore::request, DEFVAL(Dictionary()));
    ClassDB::bind_method(D_METHOD("request_json", "method", "args_json"), &BotfCore::request_json, DEFVAL("{}"));
}

} // namespace botfgd
