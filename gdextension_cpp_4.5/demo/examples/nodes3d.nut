// Squirrel Code


parent <- null // global variables
node3d <- null
timer <- 0



function _ready() {
    print("hello world from squirrel ready")
    parent <- get_node("GodotSquirrel")
    print("parentid: " + parent.id)
    node3d <- create_node(parent.id, "Node3D")
    print("id: " + node3d.id)
    if (node3d == null) {
        print("node3d not found")
        return
    }
    local box = create_node(node3d.id, "CSGBox3D")
    set_property(box.id, "use_collision", true)
    set_property(box.id, "scale", { x = 10, y = 0.5, z = 10 })
    local camera = create_node(node3d.id,"Camera3D")
    local pos = get_property(camera.id, "position")
    local new_x = 0
    local new_y = 1
    local new_z = 5
    set_property(camera.id, "position", { x = new_x, y = new_y, z = new_z })
    local light = create_node(node3d.id, "DirectionalLight3D")
    local pos = get_property(camera.id, "position")
    local new_x = -9
    local new_y = 2
    local new_z = 0
    set_property(light.id, "position", { x = new_x, y = new_y, z = new_z })

    timer <- 0
}


function SpawnBox(){
    local rigidbody = create_node(node3d.id, "RigidBody3D")
    local pos = get_property(rigidbody.id, "position")
    local new_x = 0
    local new_y = 1
    local new_z = 5
    set_property(rigidbody.id, "position", { x = 0, y = 5, z = 0 })

    call_method(rigidbody, "rotate", { x=1, y=0, z=0 }, randint(360).randomnumber)
    call_method(rigidbody, "rotate", { x=0, y=1, z=0 }, randint(360).randomnumber)
    local collisionshape = create_node(rigidbody.id, "CollisionShape3D")
    local csgbox = create_node(collisionshape.id, "CSGBox3D")
    local boxshape = instantiate("BoxShape3D")
    set_property(collisionshape.id, "shape", boxshape.ptr)

    boxmat <- instantiate("StandardMaterial3D")
    local red = randfloat(100).randomnumber * 0.01
    local green = randfloat(100).randomnumber * 0.01
    local blue = randfloat(100).randomnumber * 0.01
    set_property_object(boxmat, "albedo_color", {r=red, g=green, b=blue, a=1.0})
    set_property(csgbox.id, "material_override", boxmat.ptr)
}


function _process(delta) {
    timer <- timer + 1
    if (timer > 60) {
        SpawnBox()
        timer <- 0
    }
}
