buttonx <- 100
buttony <- 100
buttonwidth <- 150
buttonheight <- 50

button_node <- null

function _ready() {
    local root = get_node("GodotSquirrel")
    button_node <- create_node(root.id, "Button")
    set_property(button_node.id, "text", "click me!")
    set_property(button_node.id, "position", {x=buttonx, y=buttony})
    set_property(button_node.id, "size", {x=buttonwidth, y=buttonheight})

}

function _input(event) {
    if (event.type == "mouse" && event.pressed && event.button == 1) {
        local mx = event.x
        local my = event.y

        local btn_pos = get_property(button_node.id, "position")
        local btn_size = get_property(button_node.id, "size")

        if (mx >= btn_pos.x && mx <= btn_pos.x + btn_size.x &&
            my >= btn_pos.y && my <= btn_pos.y + btn_size.y) {
            print("Button clicked!")
        }
    }
}