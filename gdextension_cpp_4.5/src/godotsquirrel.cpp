#include "godotsquirrel.h"
#include <iostream>
#include <cstdlib>
#include <vector>
#include <mutex>
#include <algorithm>
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
#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/variant/vector2.hpp>
#include <godot_cpp/variant/vector3.hpp>
#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/classes/input_event_key.hpp>
#include <godot_cpp/classes/input_event_mouse_button.hpp>
#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/classes/ref.hpp> 
#include <godot_cpp/classes/resource.hpp>
#include <godot_cpp/classes/primitive_mesh.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/classes/box_mesh.hpp>
#include <godot_cpp/classes/standard_material3d.hpp>
#include <godot_cpp/classes/box_shape3d.hpp>



using namespace godot;

struct SquirrelGodotRef {
    Ref<RefCounted> ref;
    ~SquirrelGodotRef() {}
};

static std::vector<SquirrelGodotRef*> g_wrapper_refs;
static std::mutex g_wrapper_mutex;

struct ConnectionInfo {
    uint64_t object_id;
    StringName signal_name;
    String squirrel_func;
};
static std::vector<ConnectionInfo> g_connections;

static SQInteger release_wrapper(SQUserPointer p, SQInteger size) {
    SquirrelGodotRef* wrapper = static_cast<SquirrelGodotRef*>(p);
    if (wrapper) {
        std::lock_guard<std::mutex> lock(g_wrapper_mutex);
        auto it = std::find(g_wrapper_refs.begin(), g_wrapper_refs.end(), wrapper);
        if (it != g_wrapper_refs.end()) {
            g_wrapper_refs.erase(it);
        }
        memdelete(wrapper);
    }
    return 0;
}



GodotSquirrel::GodotSquirrel() {
    UtilityFunctions::print("C++ constructor called");
    vm = sq_open(1024);
    if (!vm) {
        UtilityFunctions::printerr("VM not initialized!");
    }
    //sq_pushroottable(vm);
    //sq_pushstring(vm, _SC("math"), -1);
    //sq_newtable(vm);                 
    //sqstd_register_mathlib(vm);
    //sq_newslot(vm, -3, SQFalse);
    //sq_pop(vm, 1);   

    auto register_stdlib = [&](const char* lib_name, SQRESULT (*reg_func)(HSQUIRRELVM)) {
        sq_pushroottable(vm);
        sq_pushstring(vm, _SC(lib_name), -1);
        sq_newtable(vm);
        reg_func(vm);
        sq_newslot(vm, -3, SQFalse);
        sq_pop(vm, 1);
    };

    register_stdlib("blob",    sqstd_register_bloblib);
    register_stdlib("io",      sqstd_register_iolib);
    register_stdlib("math",    sqstd_register_mathlib);
    register_stdlib("string",  sqstd_register_stringlib);
    register_stdlib("system",  sqstd_register_systemlib);

    sq_pushroottable(vm);
    sq_pushstring(vm, _SC("math"), -1);
    if (SQ_SUCCEEDED(sq_get(vm, -2))) {
        UtilityFunctions::print("stdlib math registered successfully");
    }
    sq_pop(vm, 2);


    set_process(true);
    set_process_input(true);

}


GodotSquirrel::~GodotSquirrel() {
    if (draw_2d) {
        memdelete(draw_2d);
    }
    
    {
        std::lock_guard<std::mutex> lock(g_wrapper_mutex);
        for (SquirrelGodotRef* wrapper : g_wrapper_refs) {
            memdelete(wrapper);
        }
        g_wrapper_refs.clear();
    }
    
    if (vm) {
        sq_close(vm);
    }
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


static bool table_to_rect2(HSQUIRRELVM v, SQInteger idx, Rect2 &out_rect) {
    if (sq_gettype(v, idx) != OT_TABLE) return false;

    SQFloat x = 0, y = 0, w = 0, h = 0;

    if (!table_get_float(v, idx, "x", x))      return false;
    if (!table_get_float(v, idx, "y", y))      return false;
    if (!table_get_float(v, idx, "width", w))  return false;
    if (!table_get_float(v, idx, "height", h)) return false;

    out_rect = Rect2(x, y, w, h);
    return true;
}


static bool table_to_color(HSQUIRRELVM v, SQInteger idx, Color &out_color) {
    if (sq_gettype(v, idx) != OT_TABLE) return false;

    SQFloat r = 0, g = 0, b = 0, a = 1.0f;

    if (!table_get_float(v, idx, "r", r)) return false;
    if (!table_get_float(v, idx, "g", g)) return false;
    if (!table_get_float(v, idx, "b", b)) return false;
    table_get_float(v, idx, "a", a);

    out_color = Color(r, g, b, a);
    return true;
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
            sq_pushstring(v, _SC("ptr"), -1);
            SQInteger table_idx = (idx < 0) ? (idx - 1) : idx;
            
            if (SQ_SUCCEEDED(sq_get(v, table_idx))) {
                SQUserPointer p;
                if (SQ_SUCCEEDED(sq_getuserpointer(v, -1, &p))) {
                    sq_pop(v, 1);
                    Object* obj = static_cast<Object*>(p);

                    if (!obj) return Variant();

                    if (obj->is_class("RefCounted")) {
                        return Variant(Ref<RefCounted>(Object::cast_to<RefCounted>(obj)));
                    }
                    return Variant(obj);
                }
                sq_pop(v, 1);
            }

            SQFloat x, y, z, r, g, b, a;
            if (table_get_float(v, idx, "x", x) && table_get_float(v, idx, "y", y)) {
                if (table_get_float(v, idx, "z", z)) return Vector3(x, y, z);
                return Vector2(x, y);
            }
            if (table_get_float(v, idx, "r", r) && table_get_float(v, idx, "g", g) && table_get_float(v, idx, "b", b)) {
                float alpha = table_get_float(v, idx, "a", a) ? a : 1.0f;
                return Color(r, g, b, alpha);
            }

            Dictionary dict;
            sq_pushnull(v);
            SQInteger abs_idx = (idx < 0) ? sq_gettop(v) + idx : idx;
            while (SQ_SUCCEEDED(sq_next(v, abs_idx))) {
                dict[squirrel_to_variant(v, -2)] = squirrel_to_variant(v, -1);
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
            UtilityFunctions::print("error - load resource: ", path);
        } else {
            UtilityFunctions::print("resource loaded: ", path);
        }
        
        if (res.is_valid()) {
            SquirrelGodotRef* wrapper = memnew(SquirrelGodotRef);
            wrapper->ref = res;
            
            {
                std::lock_guard<std::mutex> lock(g_wrapper_mutex);
                g_wrapper_refs.push_back(wrapper);
            }

            push_godot_object_to_squirrel(v, res.ptr());
            sq_pushstring(v, _SC("wrapper"), -1);
            sq_pushuserpointer(v, wrapper);
            sq_newslot(v, -3, SQFalse);
            
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




SQInteger squirrel_instantiate(HSQUIRRELVM v) {
    const SQChar* classname = nullptr;
    if (SQ_FAILED(sq_getstring(v, 2, &classname)) || !classname || classname[0] == '\0') {
        return sq_throwerror(v, _SC("Usage: instantiate(classname: string)"));
    }

    godot::StringName cname(classname);

    sq_newtable(v);

    bool handled_as_resource = false;

    if (ClassDB::class_exists(cname)) {
        if (ClassDB::is_parent_class(cname, "Resource") ||
            ClassDB::is_parent_class(cname, "RefCounted")) {

            handled_as_resource = true;

            if (cname == godot::StringName("BoxMesh")) {
                godot::Ref<godot::BoxMesh> mesh;
                mesh.instantiate();

                if (mesh.is_valid()) {
                    SquirrelGodotRef* wrapper = memnew(SquirrelGodotRef);
                    wrapper->ref = mesh;

                    {
                        std::lock_guard<std::mutex> lock(g_wrapper_mutex);
                        g_wrapper_refs.push_back(wrapper);
                    }

                    sq_pushstring(v, _SC("ptr"), -1);
                    sq_pushuserpointer(v, wrapper);
                    sq_newslot(v, -3, SQFalse);

                    sq_pushstring(v, _SC("type"), -1);
                    sq_pushstring(v, _SC("refcounted"), -1);
                    sq_newslot(v, -3, SQFalse);

                    sq_pushstring(v, _SC("raw"), -1);
                    sq_pushuserpointer(v, mesh.ptr());
                    sq_newslot(v, -3, SQFalse);

                    return 1;
                } else {
                    sq_poptop(v);
                    return sq_throwerror(v, _SC("Failed to instantiate BoxMesh"));
                }
            }
            else if (cname == godot::StringName("StandardMaterial3D")) {
                godot::Ref<godot::StandardMaterial3D> mat;
                mat.instantiate();

                if (mat.is_valid()) {
                    SquirrelGodotRef* wrapper = memnew(SquirrelGodotRef);
                    wrapper->ref = mat;

                    {
                        std::lock_guard<std::mutex> lock(g_wrapper_mutex);
                        g_wrapper_refs.push_back(wrapper);
                    }

                    sq_pushstring(v, _SC("ptr"), -1);
                    sq_pushuserpointer(v, wrapper);
                    sq_newslot(v, -3, SQFalse);

                    sq_pushstring(v, _SC("type"), -1);
                    sq_pushstring(v, _SC("refcounted"), -1);
                    sq_newslot(v, -3, SQFalse);

                    sq_pushstring(v, _SC("raw"), -1);
                    sq_pushuserpointer(v, mat.ptr());
                    sq_newslot(v, -3, SQFalse);

                    return 1;
                } else {
                    sq_poptop(v);
                    return sq_throwerror(v, _SC("Failed to instantiate StandardMaterial3D"));
                }
            }
            else if (cname == godot::StringName("BoxShape3D")) {
                godot::Ref<godot::BoxShape3D> box;
                box.instantiate();

                if (box.is_valid()) {
                    SquirrelGodotRef* wrapper = memnew(SquirrelGodotRef);
                    wrapper->ref = box;

                    {
                        std::lock_guard<std::mutex> lock(g_wrapper_mutex);
                        g_wrapper_refs.push_back(wrapper);
                    }

                    sq_pushstring(v, _SC("ptr"), -1);
                    sq_pushuserpointer(v, wrapper);
                    sq_newslot(v, -3, SQFalse);

                    sq_pushstring(v, _SC("type"), -1);
                    sq_pushstring(v, _SC("refcounted"), -1);
                    sq_newslot(v, -3, SQFalse);

                    sq_pushstring(v, _SC("raw"), -1);
                    sq_pushuserpointer(v, box.ptr());
                    sq_newslot(v, -3, SQFalse);

                    return 1;
                } else {
                    sq_poptop(v);
                    return sq_throwerror(v, _SC("Failed to instantiate BoxShape3D"));
                }
            }
            else {
                sq_poptop(v);
                return sq_throwerror(v, _SC("Unsupported Resource/RefCounted type"));
            }
        }
    }


    if (!handled_as_resource) {
        UtilityFunctions::print("[DEBUG] Normale Klasse (nicht Resource) → ClassDB::instantiate für ", cname);

        godot::Object* obj = godot::ClassDB::instantiate(cname);
        if (!obj) {
            sq_poptop(v);
            return sq_throwerror(v, _SC("Failed to instantiate class (ClassDB returned null)"));
        }

        sq_pushstring(v, _SC("id"), -1);
        sq_pushinteger(v, (SQInteger)obj->get_instance_id());
        sq_newslot(v, -3, SQFalse);

        sq_pushstring(v, _SC("ptr"), -1);
        sq_pushuserpointer(v, obj);
        sq_newslot(v, -3, SQFalse);

        sq_pushstring(v, _SC("type"), -1);
        sq_pushstring(v, _SC("object"), -1);
        sq_newslot(v, -3, SQFalse);

        return 1;
    }

    sq_poptop(v);
    return sq_throwerror(v, _SC("Unexpected path in instantiate"));
}



SQInteger squirrel_create_node(HSQUIRRELVM v) {
    SQInteger id;
    const SQChar* class_name_str;

    if (SQ_FAILED(sq_getinteger(v, 2, &id)) ||
        SQ_FAILED(sq_getstring(v, 3, &class_name_str))) {
        return sq_throwerror(v, _SC("error create_node(nodeid, class_name)"));
    }

    Object *parent = ObjectDB::get_instance((uint64_t)id);
    if (!parent) {
        sq_pushnull(v);
        return 1;
    }

    StringName class_name = StringName(class_name_str);
    if (!ClassDB::class_exists(class_name)) {
        return sq_throwerror(v, _SC("Godot class does not exist"));
    }

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
    //parent->call_deferred("add_child", new_node);
    Node *parent_node = Object::cast_to<Node>(parent);
    if (parent_node) {
        parent_node->add_child(new_node);
    } else {
        memdelete(new_node);
        return sq_throwerror(v, _SC("Parent is not a Node type"));
    }

    sq_newtable(v);
    sq_pushstring(v, _SC("id"), -1);
    sq_pushinteger(v, (SQInteger)new_node->get_instance_id());
    sq_newslot(v, -3, SQFalse);

    return 1;
}



SQInteger squirrel_randint(HSQUIRRELVM v) {
    SQInteger range;

    if (SQ_FAILED(sq_getinteger(v, 2, &range)))  {
        return sq_throwerror(v, _SC("error randint(range)"));
    }
    
    int randomNum = rand() % range;

    sq_newtable(v);
    sq_pushstring(v, _SC("randomnumber"), -1);
    sq_pushinteger(v, (SQInteger)randomNum);
    sq_newslot(v, -3, SQFalse);

    return 1;
}

SQInteger squirrel_randfloat(HSQUIRRELVM v) {
    SQInteger range;

    if (SQ_FAILED(sq_getinteger(v, 2, &range)))  {
        return sq_throwerror(v, _SC("error randfloat(range)"));
    }
    
    float randomNum = ((float)rand() / (float)RAND_MAX) * (float)range;

    sq_newtable(v);
    sq_pushstring(v, _SC("randomnumber"), -1);
    sq_pushfloat(v, (SQFloat)randomNum);
    sq_newslot(v, -3, SQFalse);

    return 1;
}


SQInteger squirrel_srand(HSQUIRRELVM v) {
    SQInteger seed;

    if (SQ_FAILED(sq_getinteger(v, 2, &seed)))  {
        return sq_throwerror(v, _SC("error randint(range)"));
    }
    
    srand(seed);

    sq_newtable(v);
    sq_pushstring(v, _SC("seed"), -1);
    sq_pushinteger(v, (SQInteger)seed);
    sq_newslot(v, -3, SQFalse);

    return 1;
}


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
    } else if (value.get_type() == Variant::COLOR) {
        Color col = value;
        sq_newtable(v);
        sq_pushstring(v, _SC("r"), -1);
        sq_pushfloat(v, col.r);
        sq_newslot(v, -3, SQFalse);
        sq_pushstring(v, _SC("g"), -1);
        sq_pushfloat(v, col.g);
        sq_newslot(v, -3, SQFalse);
        sq_pushstring(v, _SC("b"), -1);
        sq_pushfloat(v, col.b);
        sq_newslot(v, -3, SQFalse);
        sq_pushstring(v, _SC("a"), -1);
        sq_pushfloat(v, col.a);
        sq_newslot(v, -3, SQFalse);
    } else {

        sq_pushstring(v, _SC("[complex type]"), -1);
    }

    return 1;
}


Variant s_get_property_object(Object *obj, const StringName &prop) {
    if (!obj) return Variant();
    return obj->get(prop);
}



void add_tostring_color(HSQUIRRELVM v) {
    sq_pushstring(v, _SC("_tostring"), -1);
    sq_newclosure(v, [](HSQUIRRELVM v) -> SQInteger {
        sq_pushstring(v, _SC("[Color]"), -1);
        return 1;
    }, 0);
    sq_newslot(v, -3, SQFalse);
}


SQInteger squirrel_get_property_object(HSQUIRRELVM v) {
    if (sq_gettop(v) < 3) {
        return sq_throwerror(v, _SC("Expected (object_ptr, property_name)"));
    }

    SQUserPointer p = nullptr;
    if (SQ_FAILED(sq_getuserpointer(v, 2, &p))) {
        sq_pushnull(v);
        return 1;
    }

    Object *obj = static_cast<Object *>(p);
    if (!obj) {
        sq_pushnull(v);
        return 1;
    }

    if (!ObjectDB::get_instance(obj->get_instance_id())) {
        sq_pushnull(v);
        return 1;
    }

    const SQChar *prop_name = nullptr;
    if (SQ_FAILED(sq_getstring(v, 3, &prop_name))) {
        return sq_throwerror(v, _SC("Argument 2 must be a string"));
    }

    Variant value = obj->get(StringName(prop_name));

    switch (value.get_type()) {

        case Variant::NIL:
            sq_pushnull(v);
            break;

        case Variant::BOOL:
            sq_pushbool(v, (bool)value);
            break;

        case Variant::INT:
            sq_pushinteger(v, (SQInteger)((int64_t)value));
            break;

        case Variant::FLOAT:
            sq_pushfloat(v, (SQFloat)((double)value));
            break;

        case Variant::STRING: {
            String s = value;
            sq_pushstring(v, s.utf8().get_data(), -1);
        } break;

        case Variant::VECTOR2: {
            Vector2 v2 = value;
            sq_newtable(v);
            sq_pushstring(v, "x", -1); sq_pushfloat(v, v2.x); sq_newslot(v, -3, SQFalse);
            sq_pushstring(v, "y", -1); sq_pushfloat(v, v2.y); sq_newslot(v, -3, SQFalse);
        } break;

        case Variant::VECTOR3: {
            Vector3 v3 = value;
            sq_newtable(v);
            sq_pushstring(v, "x", -1); sq_pushfloat(v, v3.x); sq_newslot(v, -3, SQFalse);
            sq_pushstring(v, "y", -1); sq_pushfloat(v, v3.y); sq_newslot(v, -3, SQFalse);
            sq_pushstring(v, "z", -1); sq_pushfloat(v, v3.z); sq_newslot(v, -3, SQFalse);
        } break;

        case Variant::COLOR: {
            Color c = value;
            sq_newtable(v);
            sq_pushstring(v, "r", -1); sq_pushfloat(v, c.r); sq_newslot(v, -3, SQFalse);
            sq_pushstring(v, "g", -1); sq_pushfloat(v, c.g); sq_newslot(v, -3, SQFalse);
            sq_pushstring(v, "b", -1); sq_pushfloat(v, c.b); sq_newslot(v, -3, SQFalse);
            sq_pushstring(v, "a", -1); sq_pushfloat(v, c.a); sq_newslot(v, -3, SQFalse);
            add_tostring_color(v);
        } break;

        case Variant::OBJECT: {
            Object *o = value;

            if (!o || !ObjectDB::get_instance(o->get_instance_id())) {
                sq_pushnull(v);
                break;
            }

            if (o->is_class("RefCounted")) {
                sq_newtable(v);
                sq_pushstring(v, "raw", -1);
                sq_pushuserpointer(v, o);
                sq_newslot(v, -3, SQFalse);
                break;
            }

            if (o->is_class("Node")) {
                sq_newtable(v);
                sq_pushstring(v, "id", -1);
                sq_pushinteger(v, (SQInteger)o->get_instance_id());
                sq_newslot(v, -3, SQFalse);
                break;
            }

            sq_pushuserpointer(v, o);
        } break;

        default:
            sq_pushstring(v, _SC("[unsupported Variant type]"), -1);
            break;
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
    if (!obj)
        return sq_throwerror(v, _SC("wrong object-id"));

    SQObjectType t = sq_gettype(v, 4);

    if (t == OT_USERPOINTER) {
        SQUserPointer ptr;
        sq_getuserpointer(v, 4, &ptr);
        SquirrelGodotRef* wrapper = static_cast<SquirrelGodotRef*>(ptr);
        if (wrapper && wrapper->ref.is_valid()) {
            obj->set(StringName(property), wrapper->ref);
            return 0;
        }
    }

    Variant value = squirrel_to_variant(v, 4);
    obj->set(StringName(property), value);

    return 0;
}


SQInteger squirrel_set_property_object(HSQUIRRELVM v) {
    sq_pushstring(v, _SC("raw"), -1);
    if (SQ_FAILED(sq_get(v, 2))) {
        sq_pushstring(v, _SC("ptr"), -1);
        if (SQ_FAILED(sq_get(v, 2))) {
            return sq_throwerror(v, _SC("No valid object pointer found (neither raw nor ptr)"));
        }
    }
    
    SQUserPointer p;
    sq_getuserpointer(v, -1, &p);
    Object* obj = static_cast<Object*>(p);
    sq_pop(v, 1);

    if (!obj) return sq_throwerror(v, _SC("null ref"));

    const SQChar* prop_name;
    sq_getstring(v, 3, &prop_name);
    
    Variant value = squirrel_to_variant(v, 4);

    obj->set(String(prop_name), value);

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


SQInteger squirrel_draw_rect(HSQUIRRELVM v) {
    SQInteger top = sq_gettop(v);
    GodotSquirrel* self = static_cast<GodotSquirrel*>(sq_getforeignptr(v));
    
    if (top < 3) {
        return sq_throwerror(v, _SC("add_rect(rect_table, color_table, [filled=true], [width=1.0])"));
    }

    Rect2 rect;
    if (!table_to_rect2(v, 2, rect)) {
        return sq_throwerror(v, _SC("Argument 1 must be table {x,y,width,height}"));
    }

    Color color;
    if (!table_to_color(v, 3, color)) {
        return sq_throwerror(v, _SC("Argument 2 must be table {r,g,b,[a]}"));
    }

    SQBool filled = SQTrue;
    if (top >= 4) {
        if (SQ_FAILED(sq_getbool(v, 4, &filled))) {
            return sq_throwerror(v, _SC("Argument 3 must be bool (filled)"));
        }
    }

    SQFloat width = 1.0f;
    if (top >= 5) {
        if (SQ_FAILED(sq_getfloat(v, 5, &width))) {
            return sq_throwerror(v, _SC("Argument 4 must be number (width)"));
        }
    }

    if (self && self->draw_2d) {
        self->draw_2d->add_rect(rect, color, filled == SQTrue, width);
    }
    //if (draw_2d) {
    //    draw_2d->add_rect(rect, color, filled == SQTrue, width);
    //} else {
    //    UtilityFunctions::printerr("Squirrel add_rect: draw_2d is null!");
    //}

    return 0;
}


SQInteger squirrel_draw_clear(HSQUIRRELVM v) {
    GodotSquirrel* self = static_cast<GodotSquirrel*>(sq_getforeignptr(v));
    if (self && self->draw_2d) {
        self->draw_2d->clear();
    }
    return 0;
}

SQInteger squirrel_draw_circle(HSQUIRRELVM v) {
    SQInteger top = sq_gettop(v);
    GodotSquirrel* self = static_cast<GodotSquirrel*>(sq_getforeignptr(v));
    
    if (top < 3) {
        return sq_throwerror(v, _SC("draw_circle(center_table, color_table, radius, [filled=true], [width=1.0])"));
    }

    Vector2 center;
    if (!squirrel_table_to_vector2(v, 2, center)) {
        return sq_throwerror(v, _SC("Argument 1 must be table {x, y}"));
    }

    Color color;
    if (!table_to_color(v, 3, color)) {
        return sq_throwerror(v, _SC("Argument 2 must be table {r,g,b,[a]}"));
    }

    SQFloat radius = 10.0f;
    if (top >= 4) {
        if (SQ_FAILED(sq_getfloat(v, 4, &radius))) {
            return sq_throwerror(v, _SC("Argument 3 must be number (radius)"));
        }
    }

    SQBool filled = SQTrue;
    if (top >= 5) {
        if (SQ_FAILED(sq_getbool(v, 5, &filled))) {
            return sq_throwerror(v, _SC("Argument 4 must be bool (filled)"));
        }
    }

    SQFloat width = 1.0f;
    if (top >= 6) {
        if (SQ_FAILED(sq_getfloat(v, 6, &width))) {
            return sq_throwerror(v, _SC("Argument 5 must be number (width)"));
        }
    }

    if (self && self->draw_2d) {
        self->draw_2d->add_circle(center, radius, color, filled == SQTrue, width);
    }

    return 0;
}

SQInteger squirrel_connect(HSQUIRRELVM v) {
    SQInteger top = sq_gettop(v);
    
    if (top < 4) {
        return sq_throwerror(v, _SC("connect(object, signal_name, callback_func)"));
    }

    SQUserPointer ptr = nullptr;
    sq_getuserpointer(v, 2, &ptr);
    Object* obj = static_cast<Object*>(ptr);
    
    if (!obj) {
        sq_pushbool(v, SQFalse);
        return 1;
    }

    const SQChar* signal_name = nullptr;
    sq_getstring(v, 3, &signal_name);
    
    const SQChar* callback_name = nullptr;
    sq_getstring(v, 4, &callback_name);
    
    if (!signal_name || !callback_name) {
        return sq_throwerror(v, _SC("signal_name and callback_func must be strings"));
    }

    ConnectionInfo conn;
    conn.object_id = obj->get_instance_id();
    conn.signal_name = StringName(signal_name);
    conn.squirrel_func = String(callback_name);
    g_connections.push_back(conn);
    
    sq_pushbool(v, SQTrue);
    return 1;
}

SQInteger squirrel_set_draw_enabled(HSQUIRRELVM v) {
    SQBool enabled;
    GodotSquirrel* self = static_cast<GodotSquirrel*>(sq_getforeignptr(v));
    if (SQ_FAILED(sq_getbool(v, 2, &enabled))) {
        return sq_throwerror(v, _SC("set_draw_enabled(bool) erwartet"));
    }
    if (self->draw_2d) {
        self->draw_2d->set_draw_enabled(enabled == SQTrue);
    }
    return 0;
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
    bind("instantiate", squirrel_instantiate);
    bind("get_property", squirrel_get_property);
    bind("set_property", squirrel_set_property);
    bind("load_resource", squirrel_load_resource);
    bind("set_property_object", squirrel_set_property_object);
    bind("randint", squirrel_randint);
    bind("randfloat", squirrel_randfloat);
    bind("srand", squirrel_srand);
    bind("get_property_object", squirrel_get_property_object);
    bind("set_draw_enabled", squirrel_set_draw_enabled);
    bind("draw_rect", squirrel_draw_rect);
    bind("draw_circle", squirrel_draw_circle);
    bind("draw_clear", squirrel_draw_clear);
    bind("connect", squirrel_connect);


    sq_pop(vm, 1);
}


void GodotSquirrel::load_script(const String &stringscript) {
    UtilityFunctions::print("load_script started");
    //if (vm) { sq_close(vm); vm = nullptr; }

    //vm = sq_open(1024);
    sq_pushroottable(vm);

    bind_squirrel_functions(vm);


    sq_pop(vm, 1); // root
    CharString utf8 = stringscript.utf8();
    const char *script = utf8.get_data();

    if (SQ_FAILED(sq_compilebuffer(vm, script, strlen(script), "script", SQTrue))) {
        sq_getlasterror(vm);
        
        String err_msg = "unknown error";
        SQInteger err_line = -1;
        
        if (sq_gettype(vm, -1) == OT_STRING) {
            const SQChar* msg = nullptr;
            sq_getstring(vm, -1, &msg);
            err_msg = String(msg);
        } else if (sq_gettype(vm, -1) == OT_TABLE) {
            sq_pushstring(vm, _SC("_message"), -1);
            if (SQ_SUCCEEDED(sq_get(vm, -2))) {
                const SQChar* msg = nullptr;
                sq_getstring(vm, -1, &msg);
                err_msg = String(msg);
                sq_pop(vm, 1);
            }
            sq_pushstring(vm, _SC("_line"), -1);
            if (SQ_SUCCEEDED(sq_get(vm, -2))) {
                sq_getinteger(vm, -1, &err_line);
                sq_pop(vm, 1);
            }
            sq_pushstring(vm, _SC("_column"), -1);
            if (SQ_SUCCEEDED(sq_get(vm, -2))) {
                SQInteger col = -1;
                sq_getinteger(vm, -1, &col);
                if (err_line > 0) err_msg = err_msg + " (col " + String::num(col) + ")";
                sq_pop(vm, 1);
            }
        }
        
        UtilityFunctions::printerr("Squirrel compile error",
            err_line > 0 ? String(" (line ") + String::num(err_line) + ")" : "", 
            ": ", err_msg);
        sq_pop(vm, 1);
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
    sq_setforeignptr(vm, this);
    if (draw_2d == nullptr) {
        draw_2d = memnew(SquirrelDraw2D);
        add_child(draw_2d);
        draw_2d->set_name("SquirrelDraw2D");
        draw_2d->set_z_index(1000);
        draw_2d->set_draw_enabled(true);
        UtilityFunctions::print("SquirrelDraw2D added as child in _ready()");
    }


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

void GodotSquirrel::_physics_process(double delta) {
    if (!vm) return;

    sq_pushroottable(vm);              // 1. Stack: [root]
    sq_pushstring(vm, _SC("_physics_process"), -1); // 2. Stack: [root, "_process"]

    if (SQ_SUCCEEDED(sq_get(vm, -2))) { // 3. Stack: [root, function]
        
        sq_pushroottable(vm);
        sq_pushfloat(vm, (SQFloat)delta);

        if (SQ_FAILED(sq_call(vm, 2, SQFalse, SQTrue))) {
            UtilityFunctions::printerr("Squirrel runtime error in _physics_process (check if _physics_process(delta) is defined)");
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
