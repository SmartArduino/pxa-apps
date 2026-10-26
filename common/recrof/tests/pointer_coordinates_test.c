#include "../../pxa_canvas_pixels.h"
#include "framebuffer.h"
#include <assert.h>
#include <stdio.h>
static int x,y;
static int map(int px,int py,unsigned density,unsigned dw,unsigned dh,unsigned scale) {
    return pxa_game_pointer_coordinates(px,py,density,dw,dh,320,240,scale,&x,&y);
}
int main(void) {
    assert(map(292,212,65536,320,240,1) && port_control_key_at(x,y,0)==200);
    assert(map(374,272,51200,320,240,1) && port_control_key_at(x,y,0)==200);
    assert(map(195,142,98304,320,240,1) && port_control_key_at(x,y,0)==200);
    assert(map(0,0,65536,320,240,1) && x==0 && y==0);
    assert(!map(-1,20,65536,320,240,1));
    assert(!map(320,20,65536,320,240,1));
    assert(!map(20,240,65536,320,240,1));
    assert(map(160,30,65536,960,540,2) && x==0 && y==0);
    assert(map(799,509,65536,960,540,2) && x==319 && y==239);
    assert(map(480,270,65536,960,540,2) && x==160 && y==120);
    assert(!map(159,30,65536,960,540,2));
    assert(!map(800,509,65536,960,540,2));
    assert(!map(160,29,65536,960,540,2));
    assert(!map(799,510,65536,960,540,2));
    assert(map(0,0,65536,240,160,1) && x==40 && y==40);
    assert(map(239,159,65536,240,160,1) && x==279 && y==199);
    assert(pxa_game_pointer_coordinates(300,212,65536,320,240,296,222,1,&x,&y));
    assert(x==311 && y==219 && port_control_key_at(x,y,0)==200);
    puts("PASS: density-correct button hits, scaled/centered surfaces, letterbox rejection, cropped view and 296x222 geometry");
}
