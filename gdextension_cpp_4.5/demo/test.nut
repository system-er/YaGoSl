// Squirrel Code

parent <- null // global variables
sprite <- null
timepassed <- 0


function _ready() {
    print("hello world from squirrel ready")
    parent <- get_node("GodotSquirrel")
    print("parentid: " + parent.id)
    sprite <- create_node(parent.id, "Sprite2D")
    print("id: " + sprite.id)
    if (sprite == null) {
        print("node not found")
        return
    }
    local tex = load_resource("res://icon.svg")
    set_property(sprite.id, "texture", tex)
    local pos = get_property(sprite.id, "position")
    local new_x = 80
    local new_y = 150
    set_property(sprite.id, "position", { x = new_x, y = new_y })

    local lbl = create_node(parent.id, "Label")
    set_property(lbl.id, "text", "this is YaGoSl - press key or mouse to test input")
    set_property(lbl.id, "position", { x = 20, y = 20 })
    set_property(lbl.id, "scale", { x = 2.5, y = 2.5 })
}


function _process(delta) {
    timepassed <- timepassed + delta

    if (!sprite) return
    local pos = get_property(sprite.id, "position")
    local new_x = pos.x + timepassed
    local new_y = pos.y
    if (timepassed > 6) {
        timepassed <- 0
        new_x = 0
    }
    set_property(sprite.id, "position", { x = new_x, y = new_y })
}


function _input(event) {
    //print("event! type: " + event.type);

    if (event.type == "key" && event.pressed) {
        print("key: " + event.unicode.tochar());
    }

    if (event.type == "mouse" && event.pressed) {
        print("mouseclick: " + event.button + " at " + event.x);
    }
}