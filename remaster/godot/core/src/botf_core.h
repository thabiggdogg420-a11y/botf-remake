#pragma once

#include <godot_cpp/classes/object.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/string.hpp>

namespace botfgd {

// Engine singleton "BotfCore": a thin wrapper over botf_call(method, argsJson) from
// remaster/web/bridge/WebBridge.cpp. Every method of the browser client's API is available.
class BotfCore : public godot::Object {
    GDCLASS(BotfCore, godot::Object)

public:
    static BotfCore* singleton;

    // botf_call(method, JSON.stringify(args)) parsed into {"ok": bool, "result": ... | "error": String}.
    // Integral JSON numbers come back as int, not float.
    godot::Dictionary request(const godot::String& method, const godot::Dictionary& args = godot::Dictionary());
    // Raw JSON in, raw JSON out (the exact bridge string, for fixtures and byte comparisons).
    godot::String request_json(const godot::String& method, const godot::String& args_json = "{}");

protected:
    static void _bind_methods();
};

} // namespace botfgd
