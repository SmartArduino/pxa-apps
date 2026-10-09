// Exercise the actual C Guest projection and independent world-axis UVs.
#include "../voxel_raster.c"
#include <assert.h>
#include <stdio.h>
int32_t pxa_submit(const uint8_t *data,uint32_t size) {(void)data;(void)size;return 0;}
int32_t pxa_io(uint64_t handle,uint32_t op,uint8_t *data,uint32_t size) {
    (void)handle;(void)op;(void)data;return (int32_t)size;
}
int main(void) {
    g_raster_capabilities=UINT32_MAX;
    for(int axis=0;axis<3;++axis) for(int sign=-1;sign<=1;sign+=2) {
        chunk_t chunk={0};
        mesh_quad_t q={.x=8,.y=8,.z=8,.u_length=2,.v_length=3,
            .axis=(uint8_t)axis,.sign=(int8_t)sign,.block=BLOCK_WOOD};
        raster_camera_t c={.x=8,.y=8,.z=0,.fz=1,.rx=1,.uy=1,
            .tan_x=2,.tan_y=2,.width=1024,.height=1024,
            .half_width=512,.half_height=512,.scale_x=256,.scale_y=256,
            .fog_start=40,.fog_end=64};
        if(axis==0)c.x+=sign*4;
        if(axis==1)c.y+=sign*4;
        if(axis==2 && sign==1) {c.z=20;c.fz=-1;c.rx=-1;}
        projected_quad_t out[8]; uint8_t clipped=0;
        assert(project_quad_raw(&c,&chunk,&q,out,8,&clipped)==1 && !clipped);
        int matched=0;
        for(int u=0;u<=1;++u) for(int v=0;v<=1;++v) {
            const float wx=q.x+(axis==0?0:u*q.u_length);
            const float wy=q.y+(axis==1?0:v*q.v_length);
            const float wz=q.z+(axis==0?u*q.u_length:axis==1?v*q.v_length:0);
            const float right=(wx-c.x)*c.rx+(wz-c.z)*c.rz;
            const float up=(wx-c.x)*c.ux+(wy-c.y)*c.uy+(wz-c.z)*c.uz;
            const float depth=(wx-c.x)*c.fx+(wy-c.y)*c.fy+(wz-c.z)*c.fz;
            const int sx=(int)((512+right/depth*256)*16+0.5f);
            const int sy=(int)((512-up/depth*256)*16+0.5f);
            int found=0;
            for(int i=0;i<4;++i) {
                const pxa_raster_vertex_t* p=&out[0].vertices[i];
                if(rc_clampi(p->x_q4-sx,-2,2)!=p->x_q4-sx ||
                   rc_clampi(p->y_q4-sy,-2,2)!=p->y_q4-sy)continue;
                assert(p->u_q4==u*q.u_length*256);
                assert(p->v_q4==(1-v)*q.v_length*256);
                assert(p->depth_q8==(uint16_t)(depth*256+0.5f));
                ++found;
            }
            assert(found==1);++matched;
        }
        assert(matched==4);
    }
    puts("C Guest UV: six normals, unequal merged extents, no axis swapping OK");
}
