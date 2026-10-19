// Compare the batched PXA edge walk against the original generic rasterizer.
#include "classic-raster.h"
#include "core.h"
#include <array>
#include <cassert>
#include <cstdio>
int osGetTimeMS(){return 0;}
bool osJoyReady(int){return false;}
void osJoyVibrate(int,float,float){}
const char *osFixFileName(const char *name){return name;}
int main(){
    Core::width=320;Core::height=240;
    static std::array<uint16_t,320*240+2> oldColor,newColor,oldDepth,newDepth;
    static Tile8 tile;static uint8 lightmap[8192];static uint16 palette[256];
    uint32_t seed=0x12345678;
    auto random=[&](){seed=seed*1664525u+1013904223u;return seed;};
    for(auto &p:tile.index)p=uint8(random()>>24);
    for(auto &p:lightmap)p=uint8(random()>>24);
    for(auto &p:palette)p=uint16(random()>>16);
    Index indices[]={0,1,2,3};int32_t offset=0;
    for(int i=0;i<4000;++i){
        oldColor.fill(0xa55a);newColor=oldColor;
        oldDepth.fill(uint16(random()>>16));newDepth=oldDepth;
        int x=int(random()%341)-10,y=int(random()%261)-10;
        int w=int(random()%180)+1,h=int(random()%140)+1;
        OlRasterVertex face[4]{};
        for(int j=0;j<4;++j){
            auto &v=face[j];v.x=(x+((j==1||j==2)?w:0))*65536;
            v.y=y+((j>=2)?h:0);v.z=int(random()%8000000u)+100000;
            v.u=((j==1||j==2)?254:1)*65536;v.v=(j>=2?254:1)*65536;
            v.l=int(random()%256)*65536;
        }
        if(i&1)std::swap(face[0],face[2]); // Both windings.
        GAPI::swVertices.reset();
        for(auto &v:face){GAPI::VertexSW copy;static_assert(sizeof(copy)==sizeof(v),"vertex layout");memcpy(&copy,&v,sizeof(v));GAPI::swVertices.push(copy);}
        GAPI::swColor=oldColor.data()+1;GAPI::swDepth=oldDepth.data()+1;
        GAPI::curTile=&tile;GAPI::swLightmap=lightmap;GAPI::swPalette=palette;
        GAPI::swClipRect=short4(7,9,313,231);
        GAPI::swDepthTest=bool(i&2);GAPI::swDepthWrite=bool(i&4);
        OlRasterState state={newColor.data()+1,newDepth.data()+1,tile.index,lightmap,palette,320,7,9,313,231,bool(i&2),bool(i&4)};
        if(i&8){GAPI::drawQuad(indices);ol_classic_draw(state,face,indices,&offset,1,nullptr,0);}
        else {GAPI::drawTriangle(indices);ol_classic_draw(state,face,indices,nullptr,0,&offset,1);}
        assert(oldColor==newColor && oldDepth==newDepth);
        assert(newColor.front()==0xa55a && newColor.back()==0xa55a);
    }
    GAPI::swDepth=nullptr;
    puts("batch raster: 4000 triangles/quads, winding, scissor, transparent texels, depth test/write modes; exact color/depth passed");
}
