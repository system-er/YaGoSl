
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
	connect(button_node, "pressed", "on_button_pressed")
}


function on_button_pressed() {
    print("button clicked!")
}
