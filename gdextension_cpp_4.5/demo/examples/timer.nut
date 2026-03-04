local time = 0.0;

function _process(delta) {
    time += delta;
    local x = 400 + math.sin(time * 2.0) * 200;
    local y = 300 + math.cos(time * 1.5) * 100;

    draw_clear();
    //draw_rect({x=x, y=y, width=50, height=50}, {r=0, g=0.5, b=1, a=1}, true);
	draw_circle({x=x, y=y}, {r=1, g=0.5, b=1, a=1}, 50);
}