/* Task-local baseline/candidate comparison; no production hook or ABI. */
#include RASTER_SOURCE
#include <assert.h>
#include <stdio.h>
static uint32_t rng = 0x84173621;
static uint32_t random32(void) { rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5; return rng; }
int main(void) {
    uint16_t pixels[64*64], depth[64*64], palette[16*256];
    uint8_t texels[16*48];
    for(unsigned i=0;i<16*256;++i) palette[i]=(uint16_t)random32();
    for(unsigned i=0;i<sizeof(texels);++i) texels[i]=i%5 ? (uint8_t)random32() : 0;
    pxa_raster_target_t target={.pixels=pixels,.depth_pixels=depth,.width=64,.height=64,.stride_pixels=64,.depth_stride_pixels=64,.scratch_mode=PXA_RASTER_SCRATCH_DEPTH16};
    for(unsigned trial=0;trial<2400;++trial){
        for(unsigned i=0;i<64*64;++i){pixels[i]=(uint16_t)random32();depth[i]=(uint16_t)random32();}
        int16_t x0=(int16_t)(random32()%513)-256, y0=(int16_t)(random32()%513)-256;
        int16_t x1=x0+16+(int16_t)(random32()%1201), y1=y0+16+(int16_t)(random32()%1201);
        raster_vertex_t vertices[4]={{.x=x0,.y=y0},{.x=x1,.y=y0},{.x=x1,.y=y1},{.x=x0,.y=y1}};
        uint8_t flags=0;
        if(trial%3==0)flags|=PXA_RASTER_QUAD_SOLID_COLOR;
        if(trial%3==1)flags|=PXA_RASTER_QUAD_AFFINE_UV;
        if(trial%5==0)flags|=PXA_RASTER_QUAD_TRANSPARENT_INDEX0;
        if(trial%7==0)flags|=PXA_RASTER_QUAD_BLEND_75;
        for(unsigned i=0;i<4;++i){
            vertices[i].u=(int16_t)(random32()%8193)-4096;
            vertices[i].v=(int16_t)(random32()%8193)-4096;
            vertices[i].light=trial%2 ? (uint8_t)(random32()%16) : 7;
            vertices[i].depth=trial%11 ? 8+(uint16_t)(random32()%65528) : 0;
        }
        // Exercise quotient reuse explicitly, including both axes, negative
        // wrapped coordinates, and constant-depth faces with varying UVs.
        if(trial%9>=1 && trial%9<=4){
            for(unsigned i=0;i<4;++i){
                vertices[i].depth=1024;
                if(trial%9==2 || trial%9==4)vertices[i].u=96;
                if(trial%9==3 || trial%9==4)vertices[i].v=-32;
            }
        }
        pxa_raster_texture_t texture={.pixels=texels,.width=16,.height=trial%2 ? 16 : 48};
        unsigned row_begin=random32()%32, row_end=33+random32()%32;
        unsigned count=trial%4 ? 4 : 3;
        uint32_t covered=draw_painter_polygon(vertices,(uint8_t)count,flags,0x528a,&texture,palette,&target,(uint16_t)row_begin,(uint16_t)row_end,(uint8_t)(trial%4));
        assert(fwrite(pixels,sizeof(pixels),1,stdout)==1);
        assert(fwrite(depth,sizeof(depth),1,stdout)==1);
        assert(fwrite(&covered,sizeof(covered),1,stdout)==1);
    }
    return 0;
}
