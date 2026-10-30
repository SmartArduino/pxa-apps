// SPDX-License-Identifier: CC0-1.0
#include "renderer.hpp"
#include <stdint.h>
#include <algorithm>
#define S3L_MAX_PIXELS (marble::MAX_VIEW_PIXELS)
#define S3L_Z_BUFFER 1
#define S3L_SORT 0
#define S3L_PERSPECTIVE_CORRECTION 0
#define S3L_PIXEL_FUNCTION marble_fragment
#include "upstream/small3dlib/small3dlib.h"
namespace {
uint16_t *target;
int render_width=marble::VIEW_W;
struct Mesh {
    S3L_Unit vertices[1536*3];
    S3L_Index triangles[2304*3];
    uint16_t colors[2304];
    uint8_t material[2304];
    unsigned nv,nt;
    void clear() { nv=nt=0; }
    unsigned vertex(int x,int y,int z) { if(nv>=1536)__builtin_trap();unsigned v=nv++;vertices[v*3]=x;vertices[v*3+1]=y;vertices[v*3+2]=z;return v; }
    void triangle(unsigned a,unsigned b,unsigned c,uint16_t color,uint8_t texture=0) { if(nt>=2304)__builtin_trap();triangles[nt*3]=a;triangles[nt*3+1]=b;triangles[nt*3+2]=c;material[nt]=texture;colors[nt++]=color; }
};
Mesh board,objects,ball;
bool ball_ready;
S3L_Model3D models[3];S3L_Scene scene;
int cached_level=-1;
struct TrailPoint {int x,z;uint32_t time;};
TrailPoint trail[12];unsigned trail_next,trail_count;
uint32_t previous_time,previous_elapsed,burst_time;uint16_t previous_coins;
int previous_level=-1,burst_x,burst_z;
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
    unsigned b=m.nv;m.vertex(x,y+128,z);m.vertex(x,y-82,z);
    for(int i=0;i<4;++i)m.vertex(x+S3L_cos(angle+i*128)*87/512,y,z+S3L_sin(angle+i*128)*87/512);
    for(unsigned i=0;i<4;++i) { unsigned next=(i+1)%4;m.triangle(b,b+2+i,b+2+next,i&1?rgb(255,231,135):rgb(255,188,73));m.triangle(b+1,b+2+next,b+2+i,i&1?rgb(230,143,58):rgb(255,211,93)); }
}
void model(Mesh &mesh,S3L_Model3D &m) { S3L_model3DInit(mesh.vertices,mesh.nv,mesh.triangles,mesh.nt,&m);m.config.backfaceCulling=0; }
void build_board(unsigned level) {
    board.clear();uint16_t top=marble::levels[level].color;
    // Individual grass tiles retain checker shading; cliffs use stone tones.
    for(int y=0;y<7;++y) {
        for(int x=0;x<7;++x)if(marble::cell(level,x,y)!=' '){
            unsigned b=board.nv;board.vertex(wx(x),0,wz(y));board.vertex(wx(x+1),0,wz(y));board.vertex(wx(x+1),0,wz(y+1));board.vertex(wx(x),0,wz(y+1));
            uint16_t c=shade(top,(x+y)%2?245:256);board.triangle(b,b+1,b+2,c,1);board.triangle(b,b+2,b+3,c,1);
        }
        for(int x=0;x<7;++x) {
            char c=marble::cell(level,x,y);int cx=wx(x)+256,cz=wz(y)-256;
            if(c!=' ')for(int edge=0;edge<4;++edge) {
                int dx=edge==0?-1:edge==1?1:0,dy=edge==2?-1:edge==3?1:0;
                if(marble::cell(level,x+dx,y+dy)!=' ')continue;
                int x0=wx(x),x1=wx(x+1),z0=wz(y),z1=wz(y+1);
                if(edge==0)x1=x0;else if(edge==1)x0=x1;else if(edge==2)z1=z0;else z0=z1;
                const int depth[4]={0,-45,-220,-360};
                const uint16_t rock[3]={shade(top,210),rgb(143,155,161),rgb(73,115,133)};
                for(int band=0;band<3;++band){
                    unsigned b=board.nv;board.vertex(x0,depth[band],z0);board.vertex(x1,depth[band],z1);board.vertex(x1,depth[band+1],z1);board.vertex(x0,depth[band+1],z0);
                    uint16_t color=shade(rock[band],(edge==3?174:edge==1?238:205)-((x+y)%3)*7);
                    board.triangle(b,b+1,b+2,color,2);board.triangle(b,b+2,b+3,color,2);
                }
                // Shallow cyan water and a narrow foam ribbon trace the shoreline.
                for(int strip=0;strip<2;++strip){int near=strip?15:38,far=strip?38:110;unsigned b=board.nv;
                    board.vertex(x0+dx*near,-350,z0-dy*near);board.vertex(x1+dx*near,-350,z1-dy*near);
                    board.vertex(x1+dx*far,-350,z1-dy*far);board.vertex(x0+dx*far,-350,z0-dy*far);
                    uint16_t foam=strip?rgb(171,235,220):rgb(50,181,190);board.triangle(b,b+1,b+2,foam);board.triangle(b,b+2,b+3,foam);
                }
            }
            if(c=='.'&&(x*7+y*11+int(level))%4==0){
                // Sparse tufts frame the course without obscuring collision cues.
                for(int blade=0;blade<3;++blade){int xx=cx+98+blade*28,zz=cz+135;unsigned b=board.nv;
                    board.vertex(xx-18,2,zz);board.vertex(xx+12,95+blade*17,zz-15);board.vertex(xx+20,2,zz);
                    board.triangle(b,b+1,b+2,blade&1?rgb(89,151,73):rgb(130,184,84));}
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
    uint16_t color=p->modelIndex==0?board.colors[p->triangleIndex]:p->modelIndex==1?objects.colors[p->triangleIndex]:ball.colors[p->triangleIndex];
    if(p->modelIndex==0&&board.material[p->triangleIndex]){
        uint32_t hash=(uint32_t(p->x)*374761393u)^(uint32_t(p->y)*668265263u);hash=(hash^(hash>>13))*1274126177u;
        if((hash&31u)<5u)color=shade(color,board.material[p->triangleIndex]==1?242:226);
    }
    target[p->y*render_width+p->x]=color;
}
namespace marble {
void render(const Run &r,uint16_t *pixels,uint32_t time,int width,int height) {
    if (!pixels || width<1 || height<1 || width>MAX_VIEW || height>MAX_VIEW) return;
    target=pixels;render_width=width;S3L_resolutionX=uint16_t(width);S3L_resolutionY=uint16_t(height);
    if(previous_level!=r.level||time<previous_time||r.elapsed_ms<previous_elapsed){trail_count=trail_next=0;previous_coins=r.coins;burst_time=0;}
    if(previous_level==r.level&&(r.coins&~previous_coins)){
        unsigned coin=0;for(int y=0;y<7;++y)for(int x=0;x<7;++x)if(cell(r.level,x,y)=='C'){if((r.coins&~previous_coins)&(1u<<coin)){burst_x=wx(x)+256;burst_z=wz(y)-256;burst_time=time?time:1;}++coin;}
    }
    previous_level=r.level;previous_time=time;previous_elapsed=r.elapsed_ms;previous_coins=r.coins;
    const int phase=int(time/100)&511;
    for(int y=0;y<height;++y){
        int bend=S3L_sin(y*11+phase)/32;
        for(int x=0;x<width;++x){
            int caustic=(x*3+y*5+bend+int(time/130))&127;
            int light=caustic<3&&((x/18+y/11)&3)==0?13:caustic<8?2:0;
            int fleck=((x*17+y*29+int(time/350))%197)<2?8:0;
            pixels[y*width+x]=rgb(18+y/18+light,116+y/8+light+fleck,159+y/10+light+fleck);
        }
    }
    if(cached_level!=r.level)build_board(r.level);
    objects.clear();unsigned coin=0;
    int bx=int(r.x*512)-1792,bz=1792-int(r.y*512);
    if(r.phase==Phase::ROLLING){
        bool append=!trail_count;int dx=0,dz=0;
        if(trail_count){const auto &last=trail[(trail_next+11)%12];dx=bx-last.x;dz=bz-last.z;append=dx*dx+dz*dz>900;}
        if(append){trail[trail_next]={bx,bz,time};trail_next=(trail_next+1)%12;if(trail_count<12)++trail_count;}
    }
    for(unsigned i=0;i<trail_count;++i){const auto &p=trail[(trail_next+12-trail_count+i)%12];uint32_t age=time-p.time;if(age<320)disc(objects,p.x,4,p.z,18+int(320-age)/25,rgb(143,213,192));}
    if(r.phase!=Phase::FALLING)disc(objects,bx,5,bz,112,rgb(60,105,125));
    if(burst_time&&time>=burst_time&&time-burst_time<550){
        int age=int(time-burst_time);for(int i=0;i<8;++i){int a=i*64+age/8,distance=40+age/2;
            int x=burst_x+S3L_cos(a)*distance/512,z=burst_z+S3L_sin(a)*distance/512,y=180+age/2;
            unsigned b=objects.nv;objects.vertex(x-17,y,z);objects.vertex(x,y+45,z);objects.vertex(x+17,y,z);objects.triangle(b,b+1,b+2,i%2?rgb(255,228,143):rgb(245,250,224));}
    }
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
            unsigned a=objects.vertex(cx+180,460,cz+180),b=objects.vertex(cx+330,405+S3L_sin(int(time/7))/50,cz+180),c0=objects.vertex(cx+180,350,cz+180);objects.triangle(a,b,c0,color);
        }
    }
    model(objects,models[1]);
    if(!ball_ready){
        ball.clear();
        for(int row=0;row<=6;++row)for(int col=0;col<12;++col){
            int latitude=-128+row*256/6,longitude=col*512/12;
            int ring=S3L_cos(latitude)*96/512;
            ball.vertex(S3L_cos(longitude)*ring/512,S3L_sin(latitude)*96/512,S3L_sin(longitude)*ring/512);
        }
        for(int row=0;row<6;++row)for(int col=0;col<12;++col){
            unsigned a=row*12+col,b=row*12+(col+1)%12,c=(row+1)*12+col,d=(row+1)*12+(col+1)%12;
            uint16_t color=(col/2)%2?rgb(55,157,239):rgb(239,250,255);
            color=shade(color,165+row*14+(col<6?10:0));
            ball.triangle(a,b,c,color);ball.triangle(b,d,c,color);
        }
        ball_ready=true;
    }
    model(ball,models[2]);
    models[2].transform.translation.x=bx;models[2].transform.translation.z=bz;
    models[2].transform.translation.y=r.phase==Phase::FALLING?96-int(r.fall_ms)*2:96;
    models[2].transform.rotation.x=int(-r.y*430)&511;models[2].transform.rotation.z=int(-r.x*430)&511;
    models[2].config.visible=r.fall_ms<350;
    S3L_sceneInit(models,3,&scene);
    scene.camera.transform.translation.x=CAMERA_X;scene.camera.transform.translation.y=3300;scene.camera.transform.translation.z=CAMERA_Z;
    S3L_Vec4 aim={0,0,0,512};S3L_lookAt(aim,&scene.camera.transform);scene.camera.focalLength=1200 * std::min(width * VIEW_H, height * VIEW_W) / (width * VIEW_H);
    S3L_newFrame();S3L_drawScene(scene);
}
}
