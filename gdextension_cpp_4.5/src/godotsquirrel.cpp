#include "godotsquirrel.h"
#include <iostream>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/String.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/main_loop.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/classes/window.hpp>
#include <godot_cpp/classes/node2d.hpp>
#include <godot_cpp/variant/vector2.hpp>
#include <godot_cpp/classes/file_access.hpp>

using namespace godot;



static Node *get_node_from_id(uint64_t id) {
    Object *obj = ObjectDB::get_instance(id);
    if (!obj) return nullptr;
    return Object::cast_to<Node>(obj);
}

SQInteger squirrel_godot_print(HSQUIRRELVM v) {
    SQInteger nargs = sq_gettop(v);
    for (SQInteger n = 2; n <= nargs; n++) {
        const SQChar *s;
        if (SQ_SUCCEEDED(sq_getstring(v, n, &s))) {
            godot::UtilityFunctions::print(s);
        }
    }
    return 0;
}

SQInteger squirrel_get_node(HSQUIRRELVM v) {
    const SQChar* path;

    if (SQ_FAILED(sq_getstring(v, 2, &path))) {
        return sq_throwerror(v, _SC("arg1 is not path"));
    }

    Engine* engine = Engine::get_singleton();
    if (!engine) {
        sq_pushnull(v);
        return 1;
    }

    MainLoop* main_loop = engine->get_main_loop();
    SceneTree* tree = Object::cast_to<SceneTree>(main_loop);
    if (!tree) {
        sq_pushnull(v);
        return 1;
    }

    Node* found_node = tree->get_root()->find_child(path, true, false);
    if (!found_node) {
        UtilityFunctions::print("squirrel: node not found: ", path);
        sq_pushnull(v);
        return 1;
    }

    sq_newtable(v);
    sq_pushstring(v, _SC("id"), -1);
    sq_pushinteger(v, (SQInteger)found_node->get_instance_id());
    sq_newslot(v, -3, SQFalse);

    return 1;
}

SQInteger squirrel_get_name(HSQUIRRELVM v) {
    SQInteger id;
    if (SQ_FAILED(sq_getinteger(v, 2, &id))) {
        return sq_throwerror(v, _SC("no instance_id"));
    }

    Node *node = get_node_from_id((uint64_t)id);
    if (!node) {
        sq_pushnull(v);
        return 1;
    }

    String name = node->get_name();
    sq_pushstring(v, name.utf8().get_data(), -1);
    return 1;
}

SQInteger squirrel_set_name(HSQUIRRELVM v) {
    SQInteger id;
    const SQChar* new_name;

    if (SQ_FAILED(sq_getinteger(v, 2, &id)) ||
        SQ_FAILED(sq_getstring(v, 3, &new_name))) {
        return sq_throwerror(v, _SC("set_name(id, name)"));
    }

    Node *node = get_node_from_id((uint64_t)id);
    if (!node) {
        return 0;
    }

    node->set_name(String(new_name));
    return 0;
}

SQInteger squirrel_get_position(HSQUIRRELVM v) {
    SQInteger id;

    if (SQ_FAILED(sq_getinteger(v, 2, &id))) {
        return sq_throwerror(v, _SC("get_position(id)"));
    }

    Node *node = get_node_from_id((uint64_t)id);
    if (!node) {
        sq_pushnull(v);
        return 1;
    }

    Node2D *node2d = Object::cast_to<Node2D>(node);
    if (!node2d) {
        sq_pushnull(v);
        return 1;
    }

    Vector2 pos = node2d->get_position();

    sq_newtable(v);

    sq_pushstring(v, _SC("x"), -1);
    sq_pushfloat(v, pos.x);
    sq_newslot(v, -3, SQFalse);

    sq_pushstring(v, _SC("y"), -1);
    sq_pushfloat(v, pos.y);
    sq_newslot(v, -3, SQFalse);

    return 1;
}

SQInteger squirrel_set_position(HSQUIRRELVM v) {
    SQInteger id;
    SQFloat x, y;

    if (SQ_FAILED(sq_getinteger(v, 2, &id)) ||
        SQ_FAILED(sq_getfloat(v, 3, &x)) ||
        SQ_FAILED(sq_getfloat(v, 4, &y))) {
        return sq_throwerror(v, _SC("set_position(id, x, y)"));
    }

    Node *node = get_node_from_id((uint64_t)id);
    if (!node) {
        return 0;
    }

    Node2D *node2d = Object::cast_to<Node2D>(node);
    if (!node2d) {
        return 0;
    }

    node2d->set_position(Vector2(x, y));
    return 0;
}



GodotSquirrel::GodotSquirrel() {
    UtilityFunctions::print("C++ constructor called");
    vm = sq_open(1024);
    set_process(true);
}


GodotSquirrel::~GodotSquirrel() {
	// Add your cleanup here.
}

void GodotSquirrel::set_script(const String &p_script) {
    script_source = p_script;
    load_script(script_source);
}

void GodotSquirrel::load_script(const String &stringscript) {
    UtilityFunctions::print("load_script started");
    //if (vm) { sq_close(vm); vm = nullptr; }

    //vm = sq_open(1024);
    sq_pushroottable(vm);

    // Bindings
    auto bind = [&](const char* name, SQFUNCTION f) {
        sq_pushstring(vm, _SC(name), -1);
        sq_newclosure(vm, f, 0);
        sq_newslot(vm, -3, SQFalse);
    };

    bind("print", squirrel_godot_print);
    bind("get_node", squirrel_get_node);
    bind("get_name", squirrel_get_name);
    bind("set_name", squirrel_set_name);
    bind("get_position", squirrel_get_position);
    bind("set_position", squirrel_set_position);

    sq_pop(vm, 1); // root
    CharString utf8 = stringscript.utf8();
    const char *script = utf8.get_data();

    if (SQ_FAILED(sq_compilebuffer(vm, script, strlen(script), "script", SQTrue))) {
        UtilityFunctions::printerr("squirrel compile error");
        return;
    }
    
    // Initial call: Closure + root table as this
    sq_push(vm, -1);          // closure
    sq_pushroottable(vm);     // this
    if (SQ_FAILED(sq_call(vm, 1, SQFalse, SQTrue))) {
        UtilityFunctions::printerr("squirrel runtime error (initial call)");
    }
    sq_pop(vm, 1);            // pop closure
    sq_settop(vm, 0);         // Stack sauber
}

void GodotSquirrel::_ready() {
    UtilityFunctions::print("GodotSquirrel _ready called");

    if (!vm) {
        UtilityFunctions::printerr("_ready error: no vm");
        return;
    }
    sq_pushroottable(vm);
    sq_pushstring(vm, _SC("_ready"), -1);

    if (SQ_SUCCEEDED(sq_get(vm, -2))) {
        sq_pushroottable(vm);
        if (SQ_FAILED(sq_call(vm, 1, SQFalse, SQTrue))) {
            UtilityFunctions::printerr("squirrel runtime error in _ready");
        }
        sq_pop(vm, 1); // function
    }

    sq_pop(vm, 1); // root

}

void GodotSquirrel::_process(double delta) {
    if (!vm) return;

    sq_pushroottable(vm);              // 1. Stack: [root]
    sq_pushstring(vm, _SC("_process"), -1); // 2. Stack: [root, "_process"]

    // Sucht _process im root table
    if (SQ_SUCCEEDED(sq_get(vm, -2))) { // 3. Stack: [root, function]
        
        sq_pushroottable(vm);          // 4. Argument 1: 'this' (Stack: [root, function, root])
        sq_pushfloat(vm, (SQFloat)delta); // 5. Argument 2: 'delta' (Stack: [root, function, root, delta])

        // sq_call Argumente: 2 (this + delta)
        if (SQ_FAILED(sq_call(vm, 2, SQFalse, SQTrue))) {
            UtilityFunctions::printerr("Squirrel runtime error in _process (check if _process(delta) is defined)");
        }
        
        sq_pop(vm, 1); // Pop die Funktion
    }

    sq_pop(vm, 1); // Pop den root table
}
/*
void GodotSquirrel::_draw() {
    UtilityFunctions::print("GodotSquirrel _draw called");

    if (!vm) {
        UtilityFunctions::printerr("_draw error: no vm");
        return;
    }
    sq_pushroottable(vm);
    sq_pushstring(vm, _SC("_draw"), -1);

    if (SQ_SUCCEEDED(sq_get(vm, -2))) {
        sq_pushroottable(vm);
        if (SQ_FAILED(sq_call(vm, 1, SQFalse, SQTrue))) {
            UtilityFunctions::printerr("squirrel runtime error in _ready");
        }
        sq_pop(vm, 1); // function
    }

    sq_pop(vm, 1); // root

}
*/
void GodotSquirrel::set_script_path(const String &p_path) {
    script_path = p_path;

    if (p_path.is_empty()) return;

    Ref<FileAccess> file = FileAccess::open(p_path, FileAccess::READ);
    if (file.is_null()) {
        UtilityFunctions::printerr("squirrel: file not found: ", p_path);
        return;
    }

    String content = file->get_as_text();
    UtilityFunctions::print("squirrel: load script ", p_path);
    
    load_script(content); 
}

String GodotSquirrel::get_script_path() const {
    return script_path;
}


void GodotSquirrel::_bind_methods() {
    ClassDB::bind_method(D_METHOD("get_script_path"), &GodotSquirrel::get_script_path);
    ClassDB::bind_method(D_METHOD("set_script_path", "p_path"), &GodotSquirrel::set_script_path);

    ADD_PROPERTY(PropertyInfo(Variant::STRING, "script_path", PROPERTY_HINT_FILE, "*.nut"), "set_script_path", "get_script_path");
    ClassDB::bind_method(D_METHOD("load_script", "stringscript"),
        &GodotSquirrel::load_script
    );
    ClassDB::bind_method(D_METHOD("set_script", "stringscript"),
        &GodotSquirrel::set_script
    );
}
