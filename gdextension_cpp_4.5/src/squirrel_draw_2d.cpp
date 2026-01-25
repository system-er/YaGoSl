#include "squirrel_draw_2d.h"

void SquirrelDraw2D::_bind_methods() {
    ClassDB::bind_method(D_METHOD("set_draw_enabled", "enabled"),
                         &SquirrelDraw2D::set_draw_enabled);
    ClassDB::bind_method(D_METHOD("is_draw_enabled"),
                         &SquirrelDraw2D::is_draw_enabled);
    ClassDB::bind_method(D_METHOD("clear"), &SquirrelDraw2D::clear);
    ClassDB::bind_method(
        D_METHOD("add_rect", "rect", "color", "filled", "width"),
        &SquirrelDraw2D::add_rect
    );
    //ClassDB::bind_method(D_METHOD("_draw"), &SquirrelDraw2D::_draw);
}

SquirrelDraw2D::SquirrelDraw2D() {
    draw_enabled = true;
}

void SquirrelDraw2D::set_draw_enabled(bool p_enabled) {
    if (draw_enabled == p_enabled) {
        return;
    }
    draw_enabled = p_enabled;

    if (!draw_enabled) {
        // optional: Draw-Queue leeren
        // rects.clear();
    }

    queue_redraw();
}

bool SquirrelDraw2D::is_draw_enabled() const {
    return draw_enabled;
}

void SquirrelDraw2D::clear() {
    rects.clear();
    queue_redraw();
}

void SquirrelDraw2D::add_rect(const Rect2 &rect,
                              const Color &color,
                              bool filled,
                              float width) {
    DrawRect r;
    r.rect = rect;
    r.color = color;
    r.filled = filled;
    r.width = width;

    rects.push_back(r);
    queue_redraw();
}

void SquirrelDraw2D::_draw() {
    for (int i = 0; i < rects.size(); i++) {
        const DrawRect &r = rects[i];
        draw_rect(r.rect, r.color, r.filled, r.width);
    }
}

