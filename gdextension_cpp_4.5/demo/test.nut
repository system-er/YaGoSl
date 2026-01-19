// Squirrel Code


sprite <- null // global variables
timepassed <- 0


function _ready() {
    print("hello world from squirrel ready")
    sprite <- get_node("Sprite2D")
    print("id: " + sprite.id)
    if (sprite == null) {
        print("node not found")
        return
    }
    print("node name: " + get_name(sprite.id))
    local pos = get_position(sprite.id)
    print("node position   pos.x:" + pos.x + "   pos.y:" + pos.y )
    timepassed <- 0
    print("timepassed: " + timepassed)

    local lbl = create_node("Label")
    set_property(lbl.id, "text", "this is YaGoSl")
    set_position(lbl.id, 20, 20)
}


function _process(delta) {
    timepassed <- timepassed + delta
    
    if (!sprite) return
    local pos = get_position(sprite.id)
    local new_x = pos.x + timepassed
    local new_y = pos.y
    if (timepassed > 6) {
        timepassed <- 0
        new_x = 0
    }
    set_position(sprite.id, new_x, new_y)
}