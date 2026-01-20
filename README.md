# YaGoSl
Yet another Godot Squirrel

the advantages of YaGoSl are that _ready and _process functions work in squirrel. see the flying sprite on screen!    
its a new script-language for godot. edit your nut-files with the editor of your choice    
and type the name of the .nut-file in the godotsquirrel-node in inspector.    

WIP - programmed with godot 4.5, squirrel 3.2. thx to Alberto Demichelis for squirrel.
tested on windows 11. 
    
  

firstpic:    
![Pic1](firstpic.jpg)



# commands:       
print(string)    
node = get_node(nodename) // stores the id     
create_node(nodename) // for example: local lbl = create_node("Label")    
set_property(node.id, property, value) // for example: set_property(lbl.id, "text", "this is YaGoSl")    
    // other example with vector2: set_property(id, "scale", { x = 1.5, y = 1.5 })    
    // or set_property with color (id, "modulate", { r=1, g=0.2, b=0.2 })    
get_property(node.id, property)    
call_method // examples:    
    // local sprite = call_method(my_node, "get_node", "Sprite2D")    
    // local parent = call_method(my_node, "get_parent")    
    // call_method(my_node, "set_visible", false)    
    // call_method(target_node, "queue_free")    


    

# example test.nut in project demo lets the sprite fly:    
```
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
    timepassed <- 0

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
```

    
# build:    
put squirrel version 3.2 to directory src/squirrel-3.2    
put godot-cpp version 4.5 to directory godot-cpp    


# last changes:
- new commands create_node and set_property (see list commands)
- now set_property works also with vector2 (example: set_property(id, "position", { x = 10, y = 20 }))
- set_property also with vector3, bool, color, dictionary, array
- new command get_proterty equal to set_property
- new command call_method    
  
  
