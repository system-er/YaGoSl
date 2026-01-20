// Squirrel Code

parent <- null // global variables 
sprite <- null
timepassed <- 0


function _ready() {
    print("hello world from squirrel ready")
    sprite <- get_node("Sprite2D")
    print("id: " + sprite.id)
    if (sprite == null) {
        print("node not found")
        return
    }
    timepassed <- 0

    parent <- call_method(sprite, "get_parent");
    print("parentid: " + parent.id)
    local tree = call_method(parent, "get_tree")
    print("treeid: " + tree.id)

    local lbl = create_node("Label")
    set_property(lbl.id, "text", "this is YaGoSl")
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