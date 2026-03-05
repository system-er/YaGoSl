#ifndef GodotSquirrel_H
#define GodotSquirrel_H

#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/classes/input_event.hpp>
#include <godot_cpp/variant/callable.hpp>
#include <godot_cpp/variant/callable_custom.hpp>
#include <godot_cpp/classes/object.hpp>
#include <godot_cpp/variant/variant.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/string_name.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/core/object_id.hpp>
#include "squirrel_draw_2d.h"

#include <squirrel-3.2/include/squirrel.h>
#include <squirrel-3.2/include/sqstdio.h>
#include <squirrel-3.2/include/sqstdblob.h>
#include <squirrel-3.2/include/sqstdstring.h>
#include <squirrel-3.2/include/sqstdsystem.h>
#include <squirrel-3.2/include/sqstdmath.h>

#include <cstdint>
#include <mutex>
#include <vector>
#include <string>


namespace godot {


class SquirrelSignalHandler : public CallableCustom {
public:
    HSQUIRRELVM vm;
    String squirrel_func;
    uint64_t object_id;
    StringName signal_name;

    SquirrelSignalHandler(HSQUIRRELVM p_vm, const String& p_func, uint64_t p_obj_id, const StringName& p_signal)
        : vm(p_vm), squirrel_func(p_func), object_id(p_obj_id), signal_name(p_signal) {}

    uint32_t hash() const override {
        return static_cast<uint32_t>((reinterpret_cast<uintptr_t>(this) >> 4) ^ 0x12345678);
    }

    String get_as_text() const override {
        return "[SquirrelSignalHandler: " + squirrel_func + "]";
    }

    CompareEqualFunc get_compare_equal_func() const override {
        return [](const CallableCustom *p_a, const CallableCustom *p_b) -> bool {
            return p_a == p_b;
        };
    }

    CompareLessFunc get_compare_less_func() const override {
        return [](const CallableCustom *p_a, const CallableCustom *p_b) -> bool {
            return p_a < p_b;
        };
    }

    bool is_valid() const override {
        if (!vm) return false;
        Object* obj = ObjectDB::get_instance(object_id);
        return obj != nullptr;
    }

    ObjectID get_object() const override {
        return ObjectID();
    }

    void call(const Variant **p_arguments, int p_argcount, Variant &r_return_value, GDExtensionCallError &r_call_error) const override {
        if (!vm) {
            r_return_value = Variant();
            return;
        }

        Object* obj = ObjectDB::get_instance(object_id);
        if (!obj) {
            r_return_value = Variant();
            return;
        }

        sq_pushroottable(vm);
        sq_pushstring(vm, squirrel_func.utf8().get_data(), -1);

        if (SQ_FAILED(sq_get(vm, -2))) {
            sq_pop(vm, 1);
            r_return_value = Variant();
            return;
        }

        sq_pushroottable(vm);

        for (int i = 0; i < p_argcount; i++) {
            Variant arg = *p_arguments[i];
            switch (arg.get_type()) {
                case Variant::NIL:
                    sq_pushnull(vm);
                    break;
                case Variant::BOOL:
                    sq_pushbool(vm, (bool)arg ? SQTrue : SQFalse);
                    break;
                case Variant::INT:
                    sq_pushinteger(vm, (SQInteger)(int64_t)arg);
                    break;
                case Variant::FLOAT:
                    sq_pushfloat(vm, (SQFloat)(double)arg);
                    break;
                case Variant::STRING: {
                    String s = arg;
                    sq_pushstring(vm, s.utf8().get_data(), -1);
                } break;
                case Variant::VECTOR2: {
                    Vector2 v2 = arg;
                    sq_newtable(vm);
                    sq_pushstring(vm, "x", -1); sq_pushfloat(vm, v2.x); sq_newslot(vm, -3, SQFalse);
                    sq_pushstring(vm, "y", -1); sq_pushfloat(vm, v2.y); sq_newslot(vm, -3, SQFalse);
                } break;
                case Variant::VECTOR3: {
                    Vector3 v3 = arg;
                    sq_newtable(vm);
                    sq_pushstring(vm, "x", -1); sq_pushfloat(vm, v3.x); sq_newslot(vm, -3, SQFalse);
                    sq_pushstring(vm, "y", -1); sq_pushfloat(vm, v3.y); sq_newslot(vm, -3, SQFalse);
                    sq_pushstring(vm, "z", -1); sq_pushfloat(vm, v3.z); sq_newslot(vm, -3, SQFalse);
                } break;
                case Variant::OBJECT: {
                    Object* o = arg;
                    if (o && ObjectDB::get_instance(o->get_instance_id())) {
                        sq_newtable(vm);
                        sq_pushstring(vm, "id", -1);
                        sq_pushinteger(vm, (SQInteger)o->get_instance_id());
                        sq_newslot(vm, -3, SQFalse);
                    } else {
                        sq_pushnull(vm);
                    }
                } break;
                default:
                    sq_pushnull(vm);
                    break;
            }
        }

        if (SQ_FAILED(sq_call(vm, 1 + p_argcount, SQFalse, SQTrue))) {
            UtilityFunctions::printerr("Squirrel signal handler call failed: ", squirrel_func);
        }

        sq_settop(vm, 0);
        r_return_value = Variant();
    }
};


struct SignalConnection {
    uint64_t object_id;
    StringName signal_name;
    String squirrel_func;
    HSQUIRRELVM vm;
    Callable callable;
    void* receiver_ptr;
};


class GodotSquirrel : public Node {
	GDCLASS(GodotSquirrel, Node)

private:
    HSQUIRRELVM vm = nullptr;
	String script_source;
	String script_path;
    bool ready_called = false;
    std::vector<SignalConnection> signal_connections;
    std::mutex signal_mutex;

protected:
	static void _bind_methods();


public:
	GodotSquirrel();
	~GodotSquirrel();
	void _ready() override;
	void _process(double delta) override;
	void _physics_process(double delta) override;
	void _input(const Ref<InputEvent> &event) override;
	void set_script(const String &p_script);
	void load_script(const String &code);
	void set_script_path(const String &p_path);
    String get_script_path() const;
	void update_debug_draw();
	SquirrelDraw2D *draw_2d = nullptr;
    bool connect_signal(uint64_t object_id, const StringName& signal, const String& squirrel_func);
    void disconnect_all_signals();

};

}

#endif