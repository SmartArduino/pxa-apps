#include <cassert>
#include <cstdio>
#include "core.h"
int osGetTimeMS() {return 0;}
bool osJoyReady(int) {return false;}
void osJoyVibrate(int,float,float) {}
const char *osFixFileName(const char *name) {return name;}

int main() {
    Core::width=320;Core::height=240;
    Core::mModel.identity();Core::mViewProj.identity();
    Vertex vertices[8]{};
    for(int i=0;i<4;++i)vertices[i].coord=short4(30,30,30,0);
    vertices[4].coord=short4(-40,-40,0,0);vertices[5].coord=short4(-40,40,0,0);
    vertices[6].coord=short4(40,40,0,0);vertices[7].coord=short4(40,-40,0,0);
    for(auto &v:vertices) {v.light.x=255;v.texCoord.x=v.texCoord.y=1;}
    Index indices[]={0,1,2,0,2,3,4,5,6,4,6,7};
    GAPI::ambient=255;GAPI::lightsCount=0;
    GAPI::transform(indices,vertices,6,6,0);
    assert(GAPI::swTriangles.length>=2);
    for(int i=0;i<GAPI::swVertices.length;++i) {
        const auto &v=GAPI::swVertices[i];
        assert(v.x>=0 && v.x<=320*65536 && v.y>=0 && v.y<=240);
    }
    static uint16 pixels[320*240],depth[320*240],palette[256];
    static Tile8 tile;static uint8 lightmap[8192];
    memset(&tile,1,sizeof(tile));memset(depth,255,sizeof(depth));palette[1]=0xffff;
    for(unsigned i=0;i<sizeof(lightmap);++i)lightmap[i]=i&255;
    GAPI::swColor=pixels;GAPI::swDepth=depth;GAPI::swPalette=palette;
    GAPI::swLightmap=lightmap;GAPI::curTile=&tile;
    GAPI::swClipRect=short4(0,0,320,240);GAPI::swDepthTest=GAPI::swDepthWrite=true;
    for(int i=0;i<GAPI::swTriangles.length;++i)GAPI::drawTriangle(&GAPI::swIndices[GAPI::swTriangles[i]]);
    assert(pixels[120*320+160]==0xffff && pixels[20*320+20]==0xffff);
    // Normal offscreen quads use the cheaper integer scanline clipping path.
    vertices[4].coord=short4(-2,-1,0,0);vertices[5].coord=short4(-2,1,0,0);
    vertices[6].coord=short4(2,1,0,0);vertices[7].coord=short4(2,-1,0,0);
    GAPI::transform(indices,vertices,6,6,0);
    assert(GAPI::swQuads.length==1 && GAPI::swTriangles.length==0);
    memset(pixels,0,sizeof(pixels));memset(depth,255,sizeof(depth));
    GAPI::drawQuad(&GAPI::swIndices[GAPI::swQuads[0]]);
    assert(pixels[120*320+160]==0xffff && pixels[20*320+20]==0xffff);
    // A wall crossing the near plane must survive with bounded screen coords.
    Core::mViewProj=GAPI::perspective(65,4.0f/3.0f,32,46080,0);
    vertices[4].coord=short4(-100,-100,-16,0);vertices[5].coord=short4(-100,100,-16,0);
    vertices[6].coord=short4(100,100,-256,0);vertices[7].coord=short4(100,-100,-256,0);
    GAPI::transform(indices,vertices,6,6,0);
    assert(GAPI::swTriangles.length>0);
    for(int i=0;i<GAPI::swVertices.length;++i) {
        const auto &v=GAPI::swVertices[i];
        assert(v.x>=-65536 && v.x<=321*65536 && v.y>=-1 && v.y<=241 && v.z>=0);
    }
    // A fully hidden face contributes no geometry.
    for(int i=4;i<8;++i)vertices[i].coord.z=100;
    GAPI::transform(indices,vertices,6,6,0);
    assert(GAPI::swTriangles.length==0 && GAPI::swQuads.length==0);
    GAPI::swDepth=nullptr;
    puts("clip: nonzero mesh offset, integer screen edges, safety range, near plane, hidden face passed");
}
