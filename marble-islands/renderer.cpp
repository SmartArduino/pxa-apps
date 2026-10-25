// SPDX-License-Identifier: CC0-1.0
#include "renderer.hpp"
#include <stdint.h>
#define S3L_RESOLUTION_X 144
#define S3L_RESOLUTION_Y 100
#define S3L_Z_BUFFER 1
#define S3L_SORT 0
#define S3L_PERSPECTIVE_CORRECTION 0
#define S3L_PIXEL_FUNCTION marble_fragment
#include "upstream/small3dlib/small3dlib.h"
namespace {
uint16_t *target;
struct Mesh {
    S3L_Unit vertices[640*3];
    S3L_Index triangles[960*3];
    uint16_t colors[960];
    unsigned nv,nt;
    void clear() { nv=nt=0; }
    unsigned vertex(int x,int y,int z) { unsigned v=nv++;vertices[v*3]=x;vertices[v*3+1]=y;vertices[v*3+2]=z;return v; }
    void triangle(unsigned a,unsigned b,unsigned c,uint16_t color) { triangles[nt*3]=a;triangles[nt*3+1]=b;triangles[nt*3+2]=c;colors[nt++]=color; }
};
Mesh board,objects;
S3L_Model3D models[3];S3L_Scene scene;
int cached_level=-1;
constexpr uint16_t rgb(unsigned r,unsigned g,unsigned b) { return uint16_t(((r&248)<<8)|((g&252)<<3)|(b>>3)); }
uint16_t shade(uint16_t c,unsigned n) { return uint16_t(((((c>>11)&31)*n/256)<<11)|(((((c>>5)&63)*n/256)<<5))|((c&31)*n/256)); }
int wx(int tile) { return tile*512-1792; }
int wz(int tile) { return 1792-tile*512; }
void box(Mesh &m,int x0,int y0,int z0,int x1,int y1,int z1,uint16_t color) {
    unsigned b=m.nv;
    m.vertex(x0,y0,z0);m.vertex(x1,y0,z0);m.vertex(x1,y0,z1);m.vertex(x0,y0,z1);
    m.vertex(x0,y1,z0);m.vertex(x1,y1,z0);m.vertex(x1,y1,z1);m.vertex(x0,y1,z1);
    // Top, front, right, rear, left. The underside can never face this camera.
    static const unsigned faces[10][3]={{4,6,5},{4,7,6},{0,5,1},{0,4,5},{1,6,2},{1,5,6},{2,7,3},{2,6,7},{3,4,0},{3,7,4}};
    for(unsigned i=0;i<10;++i)m.triangle(b+faces[i][0],b+faces[i][1],b+faces[i][2],shade(color,i<2?256:i<4?170:i<6?202:i<8?220:188));
}
void disc(Mesh &m,int x,int y,int z,int radius,uint16_t color) {
    unsigned c=m.vertex(x,y,z),first=m.nv;
    for(int i=0;i<8;++i)m.vertex(x+S3L_cos(i*64)*radius/512,y,z+S3L_sin(i*64)*radius/512);
    for(int i=0;i<8;++i)m.triangle(c,first+i,first+(i+1)%8,color);
}
void column(Mesh &m,int x,int z,int radius,int height,uint16_t color) {
    unsigned b=m.nv;
    for(int j=0;j<2;++j)for(int i=0;i<8;++i)m.vertex(x+S3L_cos(i*64)*radius/512,j?height:3,z+S3L_sin(i*64)*radius/512);
    unsigned top=m.vertex(x,height,z);
    for(unsigned i=0;i<8;++i) { unsigned next=(i+1)%8;m.triangle(b+i,b+next,b+8+i,shade(color,170+i*9));m.triangle(b+next,b+8+next,b+8+i,shade(color,170+i*9));m.triangle(top,b+8+i,b+8+next,shade(color,235+(i&1)*21)); }
}
void gem(Mesh &m,int x,int y,int z,int angle) {
    unsigned b=m.nv;m.vertex(x,y+97,z);m.vertex(x,y-65,z);
    for(int i=0;i<4;++i)m.vertex(x+S3L_cos(angle+i*128)*72/512,y,z+S3L_sin(angle+i*128)*72/512);
    for(unsigned i=0;i<4;++i) { unsigned next=(i+1)%4;m.triangle(b,b+2+i,b+2+next,i&1?rgb(255,231,135):rgb(255,188,73));m.triangle(b+1,b+2+next,b+2+i,i&1?rgb(230,143,58):rgb(255,211,93)); }
}
const S3L_Unit ball_vertices[]={-48,78,0,48,78,0,-48,-78,0,48,-78,0,0,-48,78,0,48,78,0,-48,-78,0,48,-78,78,0,-48,78,0,48,-78,0,-48,-78,0,48};
const S3L_Index ball_triangles[]={0,11,5,0,5,1,0,1,7,0,7,10,0,10,11,1,5,9,5,11,4,11,10,2,10,7,6,7,1,8,3,9,4,3,4,2,3,2,6,3,6,8,3,8,9,4,9,5,2,4,11,6,2,10,8,6,7,9,8,1};
const uint16_t ball_colors[]={rgb(246,250,255),rgb(255,255,255),rgb(190,231,255),rgb(112,187,246),rgb(224,243,255),rgb(255,206,225),rgb(142,191,244),rgb(69,125,195),rgb(102,172,230),rgb(220,240,255),rgb(111,178,233),rgb(69,121,192),rgb(137,189,235),rgb(89,146,212),rgb(164,212,248),rgb(236,244,255),rgb(79,135,200),rgb(64,112,188),rgb(165,216,255),rgb(239,251,255)};
void model(Mesh &mesh,S3L_Model3D &m) { S3L_model3DInit(mesh.vertices,mesh.nv,mesh.triangles,mesh.nt,&m);m.config.backfaceCulling=0; }
void build_board(unsigned level) {
    board.clear();uint16_t top=marble::levels[level].color;
    // Merge floor tops into row spans; emit sides only at exposed water edges.
    // Internal vertical faces would fight the coplanar top at rounded screen edges.
    for(int y=0;y<7;++y) {
        int x=0;while(x<7) { while(x<7&&marble::cell(level,x,y)==' ')++x;int first=x;while(x<7&&marble::cell(level,x,y)!=' ')++x;
            if(x>first) {
                unsigned b=board.nv;board.vertex(wx(first),0,wz(y));board.vertex(wx(x),0,wz(y));board.vertex(wx(x),0,wz(y+1));board.vertex(wx(first),0,wz(y+1));
                uint16_t color=shade(top,y&1?245:256);board.triangle(b,b+1,b+2,color);board.triangle(b,b+2,b+3,color);
            }
        }
        for(int x=0;x<7;++x) {
            char c=marble::cell(level,x,y);int cx=wx(x)+256,cz=wz(y)-256;
            if(c!=' ')for(int edge=0;edge<4;++edge) {
                int dx=edge==0?-1:edge==1?1:0,dy=edge==2?-1:edge==3?1:0;
                if(marble::cell(level,x+dx,y+dy)!=' ')continue;
                int x0=wx(x),x1=wx(x+1),z0=wz(y),z1=wz(y+1);
                if(edge==0)x1=x0;else if(edge==1)x0=x1;else if(edge==2)z1=z0;else z0=z1;
                unsigned b=board.nv;board.vertex(x0,0,z0);board.vertex(x1,0,z1);board.vertex(x1,-145,z1);board.vertex(x0,-145,z0);
                uint16_t color=shade(top,edge==3?155:edge==1?190:175);board.triangle(b,b+1,b+2,color);board.triangle(b,b+2,b+3,color);
            }
            if(c=='#')box(board,wx(x)+12,0,wz(y+1)+12,wx(x+1)-12,245,wz(y)-12,rgb(119,143,172));
            if(c=='O')column(board,cx,cz,138,185,rgb(242,127,164));
            if(c=='S')disc(board,cx,3,cz,155,rgb(156,217,232));
        }
    }
    model(board,models[0]);cached_level=int(level);
}
}
static inline void marble_fragment(S3L_PixelInfo *p) {
    uint16_t color=p->modelIndex==0?board.colors[p->triangleIndex]:p->modelIndex==1?objects.colors[p->triangleIndex]:ball_colors[p->triangleIndex];
    target[p->y*144+p->x]=color;
}
namespace marble {
void render(const Run &r,uint16_t *pixels,uint32_t time) {
    target=pixels;
    for(int y=0;y<VIEW_H;++y)for(int x=0;x<VIEW_W;++x) {
        unsigned wave=((x*3+y*5+int(time/90))&31)==0?5:0;
        pixels[y*VIEW_W+x]=rgb(39+y/6+wave,113+y/3+wave,158+y/4+wave);
    }
    if(cached_level!=r.level)build_board(r.level);
    objects.clear();unsigned coin=0;
    int bx=int(r.x*512)-1792,bz=1792-int(r.y*512);
    if(r.phase!=Phase::FALLING)disc(objects,bx,5,bz,112,rgb(60,105,125));
    for(int y=0;y<7;++y)for(int x=0;x<7;++x) {
        char c=cell(r.level,x,y);int cx=wx(x)+256,cz=wz(y)-256;
        if(c=='C') { if(!(r.coins&(1u<<coin))) {disc(objects,cx,4,cz,85,shade(levels[r.level].color,190));gem(objects,cx,160+S3L_sin(int(time/7+x*70))/32,cz,int(time/9)&511);}++coin; }
        if(c=='T') {
            bool open=gate_open(r,x,y);uint16_t color=open?rgb(111,232,188):rgb(250,126,137);
            box(objects,wx(x)+8,0,cz-65,wx(x)+45,320,cz+65,color);
            box(objects,wx(x+1)-45,0,cz-65,wx(x+1)-8,320,cz+65,color);
            // The raised arch remains visible while its collision is open.
            box(objects,wx(x)+45,open?310:5,cz-50,wx(x+1)-45,open?355:255,cz+50,color);
        }
        if(c=='G') {
            bool ready=collected(r)==coin_count(r.level);uint16_t color=ready?rgb(133,247,190):rgb(143,180,176);
            disc(objects,cx,7,cz,203,color);disc(objects,cx,9,cz,135,ready?rgb(224,255,207):rgb(186,215,193));
            box(objects,cx+170,0,cz+170,cx+190,470,cz+190,rgb(242,245,219));
            unsigned a=objects.vertex(cx+180,460,cz+180),b=objects.vertex(cx+330,405,cz+180),c0=objects.vertex(cx+180,350,cz+180);objects.triangle(a,b,c0,color);
        }
    }
    model(objects,models[1]);
    S3L_model3DInit(ball_vertices,12,ball_triangles,20,&models[2]);models[2].config.backfaceCulling=0;
    models[2].transform.translation.x=bx;models[2].transform.translation.z=bz;
    models[2].transform.translation.y=r.phase==Phase::FALLING?96-int(r.fall_ms)*2:96;
    models[2].transform.rotation.x=int(-r.y*430)&511;models[2].transform.rotation.z=int(-r.x*430)&511;
    models[2].config.visible=r.fall_ms<350;
    S3L_sceneInit(models,3,&scene);
    scene.camera.transform.translation.x=650;scene.camera.transform.translation.y=3300;scene.camera.transform.translation.z=-4300;
    S3L_Vec4 aim={0,0,0,512};S3L_lookAt(aim,&scene.camera.transform);scene.camera.focalLength=1200;
    S3L_newFrame();S3L_drawScene(scene);
}
}
