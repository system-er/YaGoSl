#ifndef SQUIRREL_DRAW_2D_H
#define SQUIRREL_DRAW_2D_H

#include <godot_cpp/classes/node2d.hpp>
#include <godot_cpp/templates/vector.hpp>
#include <godot_cpp/variant/color.hpp>
#include <godot_cpp/variant/rect2.hpp>

using namespace godot;

class SquirrelDraw2D : public Node2D {
    GDCLASS(SquirrelDraw2D, Node2D);

protected:
    static void _bind_methods();

public:
    SquirrelDraw2D();
    void set_draw_enabled(bool p_enabled);
    bool is_draw_enabled() const;
    void _draw();
    void clear();
    void add_rect(const Rect2 &rect,
                  const Color &color,
                  bool filled = true,
                  float width = 1.0);
    void add_circle(const Vector2 &center,
                    float radius,
                    const Color &color,
                    bool filled = true,
                    float width = 1.0);

private:
    bool draw_enabled = true;
    
    struct DrawRect {
        Rect2 rect;
        Color color;
        bool filled;
        float width;
    };

    struct DrawCircle {
        Vector2 center;
        float radius;
        Color color;
        bool filled;
        float width;
    };

    Vector<DrawRect> rects;
    Vector<DrawCircle> circles;
};

#endif
