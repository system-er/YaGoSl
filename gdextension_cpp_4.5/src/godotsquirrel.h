#ifndef GodotSquirrel_H
#define GodotSquirrel_H


#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/classes/input_event.hpp>
#include <squirrel-3.2/include/squirrel.h>
#include <squirrel-3.2/include/sqstdio.h>
#include <squirrel-3.2/include/sqstdblob.h>
#include <squirrel-3.2/include/sqstdstring.h>
#include <squirrel-3.2/include/sqstdsystem.h>
#include <squirrel-3.2/include/sqstdmath.h>
#include "squirrel_draw_2d.h"


namespace godot {


class GodotSquirrel : public Node {
	GDCLASS(GodotSquirrel, Node)

private:
    HSQUIRRELVM vm = nullptr;
	String script_source;
	String script_path;
    bool ready_called = false;
	

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

};

}

#endif