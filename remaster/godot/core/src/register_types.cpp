#include "botf_core.h"

#include <gdextension_interface.h>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/core/defs.hpp>
#include <godot_cpp/godot.hpp>

using namespace godot;

static void initialize_botf(ModuleInitializationLevel level) {
    if (level != MODULE_INITIALIZATION_LEVEL_SCENE) return;
    GDREGISTER_CLASS(botfgd::BotfCore);
    botfgd::BotfCore::singleton = memnew(botfgd::BotfCore);
    Engine::get_singleton()->register_singleton("BotfCore", botfgd::BotfCore::singleton);
}

static void uninitialize_botf(ModuleInitializationLevel level) {
    if (level != MODULE_INITIALIZATION_LEVEL_SCENE) return;
    Engine::get_singleton()->unregister_singleton("BotfCore");
    memdelete(botfgd::BotfCore::singleton);
    botfgd::BotfCore::singleton = nullptr;
}

extern "C" {
GDExtensionBool GDE_EXPORT botf_core_init(GDExtensionInterfaceGetProcAddress get_proc_address,
    const GDExtensionClassLibraryPtr library, GDExtensionInitialization* initialization) {
    GDExtensionBinding::InitObject init(get_proc_address, library, initialization);
    init.register_initializer(initialize_botf);
    init.register_terminator(uninitialize_botf);
    init.set_minimum_library_initialization_level(MODULE_INITIALIZATION_LEVEL_SCENE);
    return init.init();
}
}
