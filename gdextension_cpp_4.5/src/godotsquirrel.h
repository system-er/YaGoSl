#ifndef GodotSquirrel_H
#define GodotSquirrel_H


#include <godot_cpp/classes/node.hpp>
#include <squirrel-3.2/include/squirrel.h>


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
	void set_script(const String &p_script);
	void load_script(const String &code);
	void set_script_path(const String &p_path);
    String get_script_path() const;


};

}

#endif