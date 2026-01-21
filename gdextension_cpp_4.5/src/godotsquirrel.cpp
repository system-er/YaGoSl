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
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/classes/texture2d.hpp>
//#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/variant/vector2.hpp>
#include <godot_cpp/variant/vector3.hpp>
#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/classes/input_event_key.hpp>
#include <godot_cpp/classes/input_event_mouse_button.hpp>


using namespace godot;



GodotSquirrel::GodotSquirrel() {
    UtilityFunctions::print("C++ constructor called");
    vm = sq_open(1024);
    if (!vm) {
        UtilityFunctions::printerr("VM not initialized!");
    }
    set_process(true);
    set_process_input(true);
}


GodotSquirrel::~GodotSquirrel() {
	// Add your cleanup here.
}


static bool table_get_float(HSQUIRRELVM v, SQInteger idx, const char *key, SQFloat &out) {
    sq_pushstring(v, _SC(key), -1);
    if (SQ_SUCCEEDED(sq_get(v, idx))) {
        if (sq_gettype(v, -1) == OT_FLOAT || sq_gettype(v, -1) == OT_INTEGER) {
            sq_getfloat(v, -1, &out);
            sq_pop(v, 1);
            return true;
        }
        sq_pop(v, 1);
    }
    return false;
}

    
static Variant squirrel_to_variant(HSQUIRRELVM v, SQInteger idx) {
    SQObjectType type = sq_gettype(v, idx);

    switch (type) {
        case OT_INTEGER: {
            SQInteger i;
            sq_getinteger(v, idx, &i);
            return Variant((int64_t)i);
        }
        case OT_FLOAT: {
            SQFloat f;
            sq_getfloat(v, idx, &f);
            return Variant((double)f);
        }
        case OT_BOOL: {
            SQBool b;
            sq_getbool(v, idx, &b);
            return Variant((bool)b);
        }
        case OT_STRING: {
            const SQChar *s;
            sq_getstring(v, idx, &s);
            return Variant(String(s));
        }
        case OT_ARRAY: {
            Array arr;
            SQInteger len = sq_getsize(v, idx);
            for (SQInteger i = 0; i < len; i++) {
                sq_pushinteger(v, i);
                if (SQ_SUCCEEDED(sq_get(v, idx > 0 ? idx : idx - 1))) {
                    arr.append(squirrel_to_variant(v, -1));
                    sq_pop(v, 1);
                }
            }
            return arr;
        }
        case OT_TABLE: {
            sq_pushstring(v, "ptr", -1);
            // Wir schauen, ob der Key "ptr" im Table existiert
            if (SQ_SUCCEEDED(sq_get(v, idx < 0 ? idx - 1 : idx))) {
                SQUserPointer p;
                if (SQ_SUCCEEDED(sq_getuserpointer(v, -1, &p))) {
                    sq_pop(v, 1); // "ptr" wert vom Stack nehmen
                    return Variant((Object*)p); // Als Godot-Objekt zurückgeben
                }
                sq_pop(v, 1); // Falls es kein Userpointer war
            }
            else {
                SQFloat x, y, z, r, g, b, a;
                if (table_get_float(v, idx, "x", x) && table_get_float(v, idx, "y", y)) {
                    if (table_get_float(v, idx, "z", z)) return Vector3(x, y, z);
                    return Vector2(x, y);
                }
                if (table_get_float(v, idx, "r", r) && table_get_float(v, idx, "g", g) && table_get_float(v, idx, "b", b)) {
                    float alpha = table_get_float(v, idx, "a", a) ? a : 1.0f;
                    return Color(r, g, b, alpha);
                }
            }

            Dictionary dict;
            sq_pushnull(v);

            SQInteger abs_idx = (idx < 0) ? sq_gettop(v) + idx : idx;
            
            while (SQ_SUCCEEDED(sq_next(v, abs_idx))) {

                Variant key = squirrel_to_variant(v, -2);
                Variant val = squirrel_to_variant(v, -1);
                dict[key] = val;
                sq_pop(v, 2);
            }
            sq_pop(v, 1);
            return dict;
        }
        case OT_NULL:
        default:
            return Variant();
    }
}

static Node *get_node_from_id(uint64_t id) {
    Object *obj = ObjectDB::get_instance(id);
    if (!obj) return nullptr;
    return Object::cast_to<Node>(obj);
}

static Object *get_object_from_id(uint64_t id) {
    return ObjectDB::get_instance(id);
}


static Array squirrel_array_to_array(HSQUIRRELVM v, SQInteger idx) {
    Array arr;

    if (sq_gettype(v, idx) != OT_ARRAY)
        return arr;

    SQInteger len = sq_getsize(v, idx);
    for (SQInteger i = 0; i < len; i++) {
        sq_pushinteger(v, i);
        if (SQ_SUCCEEDED(sq_get(v, idx))) {
            arr.append(squirrel_to_variant(v, -1));
            sq_pop(v, 1);
        }
    }
    return arr;
}

static bool squirrel_table_to_vector2(HSQUIRRELVM v, SQInteger idx, Vector2 &out_vec) {
    if (sq_gettype(v, idx) != OT_TABLE) return false;

    SQFloat x = 0;
    SQFloat y = 0;

    sq_pushstring(v, _SC("x"), -1);
    if (SQ_SUCCEEDED(sq_get(v, idx))) {
        sq_getfloat(v, -1, &x);
        sq_pop(v, 1);
    } else {
        sq_pop(v, 1);
        return false;
    }

    sq_pushstring(v, _SC("y"), -1);
    if (SQ_SUCCEEDED(sq_get(v, idx))) {
        sq_getfloat(v, -1, &y);
        sq_pop(v, 1);
    } else {
        sq_pop(v, 1);
        return false;
    }

    out_vec = Vector2(x, y);
    return true;
}

void push_godot_object_to_squirrel(HSQUIRRELVM v, Object* obj) {
    if (!obj) {
        sq_pushnull(v);
        return;
    }

    sq_newtable(v); 
    
    sq_pushstring(v, "ptr", -1);
    sq_pushuserpointer(v, obj);
    sq_newslot(v, -3, SQFalse);

    sq_pushstring(v, "_tostring", -1);
    sq_newclosure(v, [](HSQUIRRELVM v) -> SQInteger {
        sq_pushstring(v, "(GodotResource)", -1);
        return 1;
    }, 0);
    sq_newslot(v, -3, SQFalse);
}


SQInteger squirrel_load_resource(HSQUIRRELVM v) {
    const SQChar *path;
    if (SQ_SUCCEEDED(sq_getstring(v, 2, &path))) {
        Ref<Resource> res = ResourceLoader::get_singleton()->load(path);
        if (res.is_null()) {
            UtilityFunctions::print("Fehler: Ressource konnte nicht geladen werden: ", path);
        } else {
            UtilityFunctions::print("Erfolg: Ressource geladen: ", path);
        }
        
        if (res.is_valid()) {
            push_godot_object_to_squirrel(v, res.ptr());
            res->reference(); 
            return 1;
        }
    }
    sq_pushnull(v);
    return 1;
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

/*
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
*/

/*
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
*/

/*
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
*/

/*
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
*/

SQInteger squirrel_create_node(HSQUIRRELVM v) {
    SQInteger id;
    const SQChar* class_name_str;

    if (SQ_FAILED(sq_getinteger(v, 2, &id)) ||
        SQ_FAILED(sq_getstring(v, 3, &class_name_str))) {
        return sq_throwerror(v, _SC("error create_node(nodeid, class_name)"));
    }
    UtilityFunctions::print("squirrel_create_node id: ", id);
    Object *parent = ObjectDB::get_instance((uint64_t)id);
    if (!parent) {
        sq_pushnull(v);
        return 1;
    }

    StringName class_name = StringName(class_name_str);
    if (!ClassDB::class_exists(class_name)) {
        return sq_throwerror(v, _SC("Godot class does not exist"));
    }
    UtilityFunctions::print("squirrel_create_node classname: ", class_name_str);

    Object *obj = ClassDB::instantiate(class_name);
    Node *new_node = Object::cast_to<Node>(obj);

    if (!new_node) {
        memdelete(obj);
        return sq_throwerror(v, _SC("Object is not a Node type"));
    }

    //MainLoop* main_loop = Engine::get_singleton()->get_main_loop();
    //SceneTree* tree = Object::cast_to<SceneTree>(main_loop);
    //if (tree && tree->get_root()) {
    //    tree->get_root()->call_deferred("add_child", new_node);
    //}
    parent->call_deferred("add_child", new_node);

    sq_newtable(v);
    sq_pushstring(v, _SC("id"), -1);
    sq_pushinteger(v, (SQInteger)new_node->get_instance_id());
    sq_newslot(v, -3, SQFalse);

    return 1;
}

/*
SQInteger squirrel_set_property(HSQUIRRELVM v) {
    SQInteger id;
    const SQChar* property_name;

    if (SQ_FAILED(sq_getinteger(v, 2, &id)) || SQ_FAILED(sq_getstring(v, 3, &property_name))) {
        return sq_throwerror(v, _SC("Usage: set_property(id, property_name, value)"));
    }

    Object *obj = ObjectDB::get_instance((uint64_t)id);
    if (!obj) return 0;

    Variant godot_value;
    SQObjectType type = sq_gettype(v, 4);

    switch (type) {
        case OT_INTEGER: {
            SQInteger val;
            sq_getinteger(v, 4, &val);
            godot_value = (int64_t)val;
            break;
        }
        case OT_FLOAT: {
            SQFloat val;
            sq_getfloat(v, 4, &val);
            godot_value = (double)val;
            break;
        }
        case OT_STRING: {
            const SQChar* val;
            sq_getstring(v, 4, &val);
            godot_value = String(val);
            break;
        }
        case OT_BOOL: {
            SQBool val;
            sq_getbool(v, 4, &val);
            godot_value = (bool)val;
            break;
        }
        case OT_TABLE: {
            Vector2 vec2;
            if (squirrel_table_to_vector2(v, 4, vec2)) {
                godot_value = vec2;
                break;
            }
            return sq_throwerror(v, _SC("Unsupported table format for set_property"));
        }
        default:
            return sq_throwerror(v, _SC("Unsupported value type for set_property"));
    }

    obj->set(StringName(property_name), godot_value);

    return 0;
}
*/


SQInteger squirrel_get_property(HSQUIRRELVM v) {
    SQInteger id;
    const SQChar *property;

    if (SQ_FAILED(sq_getinteger(v, 2, &id)) ||
        SQ_FAILED(sq_getstring(v, 3, &property))) {
        return sq_throwerror(v, _SC("get_property(id, property_name)"));
    }

    Object *obj = ObjectDB::get_instance((uint64_t)id);
    if (!obj) {
        sq_pushnull(v);
        return 1;
    }

    Variant value = obj->get(StringName(property));
    
    if (value.get_type() == Variant::NIL) {
        sq_pushnull(v);
    } else if (value.get_type() == Variant::FLOAT) {
        sq_pushfloat(v, (SQFloat)value);
    } else if (value.get_type() == Variant::INT) {
        sq_pushinteger(v, (SQInteger)value);
    } else if (value.get_type() == Variant::STRING) {
        sq_pushstring(v, value.operator String().utf8().get_data(), -1);
    } else if (value.get_type() == Variant::BOOL) {
        sq_pushbool(v, value.operator bool());
    } else if (value.get_type() == Variant::VECTOR2) {
        Vector2 vec = value;
        sq_newtable(v);
        sq_pushstring(v, _SC("x"), -1);
        sq_pushfloat(v, vec.x);
        sq_newslot(v, -3, SQFalse);
        sq_pushstring(v, _SC("y"), -1);
        sq_pushfloat(v, vec.y);
        sq_newslot(v, -3, SQFalse);
    } else if (value.get_type() == Variant::VECTOR3) {
        Vector3 vec = value;
        sq_newtable(v);
        sq_pushstring(v, _SC("x"), -1);
        sq_pushfloat(v, vec.x);
        sq_newslot(v, -3, SQFalse);
        sq_pushstring(v, _SC("y"), -1);
        sq_pushfloat(v, vec.y);
        sq_newslot(v, -3, SQFalse);
        sq_pushstring(v, _SC("z"), -1);
        sq_pushfloat(v, vec.z);
        sq_newslot(v, -3, SQFalse);
    } else {

        sq_pushstring(v, _SC("[complex type]"), -1);
    }

    return 1;
}


SQInteger squirrel_set_property(HSQUIRRELVM v) {
    SQInteger id;
    const SQChar *property;

    if (SQ_FAILED(sq_getinteger(v, 2, &id)) ||
        SQ_FAILED(sq_getstring(v, 3, &property))) {
        return sq_throwerror(v, _SC("set_property(id, name, value)"));
    }

    Object *obj = ObjectDB::get_instance((uint64_t)id);
    if (!obj) return 0;

    Variant value = squirrel_to_variant(v, 4);
    obj->set(StringName(property), value);

    return 0;
}


void GodotSquirrel::set_script(const String &p_script) {
    script_source = p_script;
    load_script(script_source);
}

SQInteger squirrel_call_method(HSQUIRRELVM v) {
    SQInteger initial_stack = sq_gettop(v);
    
    if (initial_stack < 3) {
        return sq_throwerror(v, _SC("call_gd(obj_table, 'method', ...args) requires 2+ arguments"));
    }

    sq_pushstring(v, _SC("id"), -1);
    if (SQ_FAILED(sq_get(v, 2))) {
        return sq_throwerror(v, _SC("Argument 1 is not a valid Godot-Node table (missing 'id')"));
    }
    
    SQInteger id_raw;
    sq_getinteger(v, -1, &id_raw);
    sq_pop(v, 1);

    Object *obj = ObjectDB::get_instance((uint64_t)id_raw);
    if (!obj) {
        sq_pushnull(v);
        return 1;
    }

    const SQChar* method_name_str;
    sq_getstring(v, 3, &method_name_str);
    StringName method_name(method_name_str);

    Array godot_args;
    for (SQInteger i = 4; i <= initial_stack; i++) {
        godot_args.append(squirrel_to_variant(v, i));
    }

    Variant result = obj->callv(method_name, godot_args);

    if (result.get_type() == Variant::OBJECT) {
        Object* res_obj = result;
        if (res_obj) {
            sq_newtable(v);
            sq_pushstring(v, _SC("id"), -1);
            sq_pushinteger(v, (SQInteger)res_obj->get_instance_id());
            sq_newslot(v, -3, SQFalse);
        } else {
            sq_pushnull(v);
        }
    } else if (result.get_type() == Variant::NIL) {
        sq_pushnull(v);
    } else {
        sq_pushnull(v);
    }

    return 1;
}

void bind_squirrel_functions(HSQUIRRELVM vm) {
    sq_pushroottable(vm);

    auto bind = [&](const char* name, SQFUNCTION f) {
        sq_pushstring(vm, _SC(name), -1);
        sq_newclosure(vm, f, 0);
        sq_newslot(vm, -3, SQFalse);
    };

    bind("print", squirrel_godot_print);
    bind("call_method", squirrel_call_method);
    bind("get_node", squirrel_get_node);
    bind("create_node", squirrel_create_node);
    bind("get_property", squirrel_get_property);
    bind("set_property", squirrel_set_property);
    bind("load_resource", squirrel_load_resource);

    sq_pop(vm, 1);
}

void GodotSquirrel::load_script(const String &stringscript) {
    UtilityFunctions::print("load_script started");
    //if (vm) { sq_close(vm); vm = nullptr; }

    //vm = sq_open(1024);
    sq_pushroottable(vm);

    bind_squirrel_functions(vm);

    /*
    // Bindings
    auto bind = [&](const char* name, SQFUNCTION f) {
        sq_pushstring(vm, _SC(name), -1);
        sq_newclosure(vm, f, 0);
        sq_newslot(vm, -3, SQFalse);
    };
    
    bind("print", squirrel_godot_print);
    bind("get_node", squirrel_get_node);
    //bind("get_name", squirrel_get_name);
    //bind("set_name", squirrel_set_name);
    //bind("get_position", squirrel_get_position);
    //bind("set_position", squirrel_set_position);
    bind("create_node", squirrel_create_node);
    bind("set_property", squirrel_set_property);
    bind("get_property", squirrel_get_property);
    bind("call_command", squirrel_call_command);
    */

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

    if (SQ_SUCCEEDED(sq_get(vm, -2))) { // 3. Stack: [root, function]
        
        sq_pushroottable(vm);
        sq_pushfloat(vm, (SQFloat)delta);

        if (SQ_FAILED(sq_call(vm, 2, SQFalse, SQTrue))) {
            UtilityFunctions::printerr("Squirrel runtime error in _process (check if _process(delta) is defined)");
        }
        
        sq_pop(vm, 1);
    }

    sq_pop(vm, 1);
}


void GodotSquirrel::_input(const Ref<InputEvent> &event) {
    if (vm == nullptr || event.is_null()) return;

    sq_pushroottable(vm);
    sq_pushstring(vm, "_input", -1);
    
    if (SQ_SUCCEEDED(sq_get(vm, -2))) {
        sq_pushroottable(vm);
        sq_newtable(vm);

        bool supported_event = false;

        Ref<InputEventKey> key_event = Object::cast_to<InputEventKey>(event.ptr());
        if (key_event.is_valid()) {
            supported_event = true;
            sq_pushstring(vm, "type", -1);
            sq_pushstring(vm, "key", -1);
            sq_newslot(vm, -3, SQFalse);

            sq_pushstring(vm, "unicode", -1);
            sq_pushinteger(vm, (SQInteger)key_event->get_unicode());
            sq_newslot(vm, -3, SQFalse);
            
            sq_pushstring(vm, "pressed", -1);
            sq_pushbool(vm, key_event->is_pressed() ? SQTrue : SQFalse);
            sq_newslot(vm, -3, SQFalse);
        }

        if (!supported_event) {
            Ref<InputEventMouseButton> mouse_event = Object::cast_to<InputEventMouseButton>(event.ptr());
            if (mouse_event.is_valid()) {
                supported_event = true;
                sq_pushstring(vm, "type", -1);
                sq_pushstring(vm, "mouse", -1);
                sq_newslot(vm, -3, SQFalse);

                sq_pushstring(vm, "button", -1);
                sq_pushinteger(vm, (SQInteger)mouse_event->get_button_index());
                sq_newslot(vm, -3, SQFalse);

                sq_pushstring(vm, "pressed", -1);
                sq_pushbool(vm, mouse_event->is_pressed() ? SQTrue : SQFalse);
                sq_newslot(vm, -3, SQFalse);

                sq_pushstring(vm, "x", -1);
                sq_pushfloat(vm, (SQFloat)mouse_event->get_position().x);
                sq_newslot(vm, -3, SQFalse);

                sq_pushstring(vm, "y", -1);
                sq_pushfloat(vm, (SQFloat)mouse_event->get_position().y);
                sq_newslot(vm, -3, SQFalse);
            }
        }

        if (supported_event) {
            if (SQ_FAILED(sq_call(vm, 2, SQFalse, SQTrue))) {
            }
        }
    }
    sq_pop(vm, 1);
}


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
