/* SPDX-License-Identifier: MIT. Software rasterizer and pointer frontend for
 * Open Golf. Physics, BVH, math, entity transforms and game states are upstream. */
#include "pxa_framebuffer.h"
#include "pxa_port_log.h"
#include "common/graphics.h"
#include "common/level.h"
#include "golf/game.h"
#include "golf/golf.h"
#include "generated/font.h"
#include <float.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#define WIDTH 320
#define VIEW_HEIGHT 240
#define HEIGHT 280
static uint8_t pixels[WIDTH*HEIGHT];
static uint8_t upload[PXA_RASTER_UPLOAD_HEADER_BYTES+WIDTH*HEIGHT];
static float depth[WIDTH*VIEW_HEIGHT];
static pxfb_t fb;
static unsigned selected_level, frame, last_ms;
static bool dragging;
static vec2 down;
static int previous_state=-1;

static uint8_t color(float r,float g,float b) {
    r=golf_clampf(r,0,1);g=golf_clampf(g,0,1);b=golf_clampf(b,0,1);
    return (uint8_t)(((int)(r*7)<<5)|((int)(g*7)<<2)|(int)(b*3));
}
static void rect(int x,int y,int w,int h,uint8_t c) {
    for(int row=y;row<y+h;row++) for(int col=x;col<x+w;col++)
      if(col>=0&&col<WIDTH&&row>=0&&row<HEIGHT) pixels[row*WIDTH+col]=c;
}
static void text(int x,int y,const char *s,uint8_t c,int scale) {
    while(*s) {
        unsigned ch=(unsigned char)*s++;
        if(ch>=32&&ch<127) for(int row=0;row<12;row++) for(int col=0;col<6;col++)
          if(font6x12[(ch-32)*12+row]&(1u<<col)) rect(x+col*scale,y+row*scale,scale,scale,c);
        x+=6*scale;
    }
}
typedef struct {float x,y,z;} projected_t;
static projected_t project(vec3 p) {
    golf_graphics_t *graphics=golf_graphics_get();
    vec4 clip=vec4_apply_mat(V4(p.x,p.y,p.z,1),graphics->proj_view_mat);
    if(clip.w<=0.08f) return (projected_t){0,0,-1};
    return (projected_t){(clip.x/clip.w*0.5f+0.5f)*WIDTH,
      (0.5f-clip.y/clip.w*0.5f)*VIEW_HEIGHT,1.0f/clip.w};
}
static float edge(projected_t a,projected_t b,float x,float y) {return (x-a.x)*(b.y-a.y)-(y-a.y)*(b.x-a.x);}
static void triangle(vec3 a,vec3 b,vec3 c,uint8_t ink) {
    projected_t p=project(a),q=project(b),r=project(c);
    /* Camera near-plane clipping: omit triangles crossing the near plane.
       This affects drawing only; collision geometry remains complete. */
    if(p.z<=0||q.z<=0||r.z<=0) return;
    float area=edge(p,q,r.x,r.y);
    if(fabsf(area)<0.001f) return;
    int x0=golf_clampi((int)floorf(fminf(p.x,fminf(q.x,r.x))),0,WIDTH-1);
    int x1=golf_clampi((int)ceilf(fmaxf(p.x,fmaxf(q.x,r.x))),0,WIDTH-1);
    int y0=golf_clampi((int)floorf(fminf(p.y,fminf(q.y,r.y))),0,VIEW_HEIGHT-1);
    int y1=golf_clampi((int)ceilf(fmaxf(p.y,fmaxf(q.y,r.y))),0,VIEW_HEIGHT-1);
    for(int y=y0;y<=y1;y++) for(int x=x0;x<=x1;x++) {
        float u=edge(q,r,x+0.5f,y+0.5f)/area,v=edge(r,p,x+0.5f,y+0.5f)/area,w=1-u-v;
        if(u>=0&&v>=0&&w>=0) {
            float z=u*p.z+v*q.z+w*r.z;
            int at=y*WIDTH+x;
            if(z>depth[at]) {depth[at]=z;pixels[at]=ink;}
        }
    }
}
static vec3 material_color(const char *name,golf_entity_t *entity) {
    if(entity->type==WATER_ENTITY || strstr(name,"water")) return V3(0.08f,0.6f,0.85f);
    if(strstr(name,"wall")) return V3(0.72f,0.53f,0.32f);
    if(strstr(name,"ground")||strstr(name,"grass")) return V3(0.32f,0.7f,0.22f);
    if(strstr(name,"wood")||strstr(name,"bark")) return V3(0.48f,0.27f,0.1f);
    if(strstr(name,"leaf")||strstr(name,"green")) return V3(0.14f,0.48f,0.12f);
    if(strstr(name,"red")) return V3(0.95f,0.2f,0.18f);
    if(entity->type==HOLE_ENTITY) return V3(0.08f,0.08f,0.08f);
    return V3(0.64f,0.72f,0.64f);
}
static void render_model(golf_entity_t *entity,golf_model_t *model,mat4 transform) {
    if(!model) return;
    for(int i=0;i<model->groups.length;i++) {
        golf_model_group_t *group=&model->groups.data[i];
        vec3 base=material_color(group->material_name,entity);
        golf_material_t material;
        if(golf_level_get_material(golf_get()->level,group->material_name,&material) &&
           (material.type==GOLF_MATERIAL_COLOR||material.type==GOLF_MATERIAL_DIFFUSE_COLOR)) base=V3(material.color.x,material.color.y,material.color.z);
        for(int n=group->start_vertex;n<group->start_vertex+group->vertex_count;n+=3) {
            vec3 a=vec3_apply_mat4(model->positions.data[n],1,transform);
            vec3 b=vec3_apply_mat4(model->positions.data[n+1],1,transform);
            vec3 c=vec3_apply_mat4(model->positions.data[n+2],1,transform);
            vec3 normal=vec3_normalize(vec3_cross(vec3_sub(b,a),vec3_sub(c,a)));
            float shade=0.48f+0.52f*fabsf(vec3_dot(normal,vec3_normalize(V3(-0.3f,0.9f,0.5f))));
            triangle(a,b,c,color(base.x*shade,base.y*shade,base.z*shade));
        }
    }
}
static void line(vec3 a,vec3 b,uint8_t c) {
    projected_t p=project(a),q=project(b);
    if(p.z<=0||q.z<=0) return;
    float dx=q.x-p.x,dy=q.y-p.y;
    int steps=(int)fmaxf(fabsf(dx),fabsf(dy));
    if(steps<1||steps>1500) return;
    for(int n=0;n<=steps;n++) rect((int)(p.x+dx*n/steps),(int)(p.y+dy*n/steps),2,2,c);
}
static void render(void) {
    golf_t *golf=golf_get();golf_game_t *game=golf_game_get();
    memset(pixels,color(0.53f,0.77f,0.88f),sizeof(pixels));
    memset(depth,0,sizeof(depth));
    golf_graphics_update_proj_view_mat();
    if(golf->level) for(int i=0;i<golf->level->entities.length;i++) {
        golf_entity_t *entity=&golf->level->entities.data[i];
        if(entity->type==MODEL_ENTITY||entity->type==GEO_ENTITY||entity->type==WATER_ENTITY||entity->type==HOLE_ENTITY) {
            golf_transform_t transform=golf_entity_get_world_transform(golf->level,entity);
            golf_movement_t *movement=golf_entity_get_movement(entity);
            if(movement) transform=golf_transform_apply_movement(transform,*movement,game->t);
            render_model(entity,golf_entity_get_model(entity),golf_transform_get_model_mat(transform));
        }
    }
    if(golf->state==GOLF_STATE_IN_GAME) {
        golf_entity_t dummy={.type=MODEL_ENTITY};
        golf_transform_t t=golf_transform(game->ball.draw_pos,V3(0.12f,0.12f,0.12f),game->ball.orientation);
        render_model(&dummy,golf_data_get_model("data/models/golf_ball.obj"),golf_transform_get_model_mat(t));
        /* A clear ball outline keeps low-resolution aiming readable. */
        projected_t ball=project(game->ball.draw_pos);
        if(ball.z>0) rect((int)ball.x-2,(int)ball.y-2,5,5,255);
        if(game->state==GOLF_GAME_STATE_AIMING) for(int i=1;i<game->aim_line.num_points;i++) line(game->aim_line.points[i-1],game->aim_line.points[i],color(1,1,0));
        char status[64];snprintf(status,sizeof(status),"HOLE %d / 20     STROKES %d",golf->level_num+1,game->stroke_count);
        rect(0,0,WIDTH,18,color(0.05f,0.15f,0.13f));text(8,3,status,255,1);
        if(game->state==GOLF_GAME_STATE_FINISHED) {rect(35,83,250,67,color(0.05f,0.16f,0.13f));text(76,89,"HOLE COMPLETE",255,2);text(65,127,"NEXT advances to the next hole",255,1);}
        if(game->state==GOLF_GAME_STATE_PAUSED) {rect(68,90,184,47,color(0.05f,0.16f,0.13f));text(120,105,"PAUSED",255,2);}
        if(game->state==GOLF_GAME_STATE_WAITING_FOR_AIM) {rect(0,223,WIDTH,17,color(0.05f,0.15f,0.13f));text(20,226,"DRAG DOWN TO AIM, RELEASE TO HIT",255,1);}
    } else {
        rect(25,55,270,138,color(0.04f,0.14f,0.1f));
        text(60,65,"OPEN GOLF",255,3);
        text(62,111,"ORIGINAL PHYSICS + 20 COURSES",color(0.4f,1,0.4f),1);
        char label[32];snprintf(label,sizeof(label),"PLAY HOLE %u",selected_level+1);
        rect(62,140,196,35,color(0.15f,0.48f,0.24f));text(94,149,label,255,2);
    }
    rect(0,240,WIDTH,40,color(0.05f,0.12f,0.13f));
    const char *names[]={"LEFT","RIGHT","RETRY","MENU","NEXT"};
    for(int i=0;i<5;i++) {rect(i*64+2,243,60,32,color(0.13f,0.27f,0.24f));text(i*64+17,253,names[i],255,1);}
}
static void trace(const char *reason) {
#if PXA_RETRO_TRACE

    golf_game_t *g=golf_game_get();pxlog_t log;pxlog_init(&log);pxlog_text(&log,"GOLF ");pxlog_text(&log,reason);
    pxlog_field(&log,"state",g->state);pxlog_field(&log,"golf",golf_get()->state);
    pxlog_field(&log,"level",golf_get()->level_num);pxlog_field(&log,"strokes",g->stroke_count);
    pxlog_field(&log,"x",(int)(g->ball.pos.x*1000));pxlog_field(&log,"y",(int)(g->ball.pos.y*1000));pxlog_field(&log,"z",(int)(g->ball.pos.z*1000));
    pxlog_field(&log,"speed",(int)(vec3_length(g->ball.vel)*1000));pxlog_field(&log,"hole",g->ball.is_in_hole);
    pxlog_field(&log,"out",g->ball.is_out_of_bounds);pxlog_field(&log,"power",(int)(g->aim_line.power*1000));
    pxlog_field(&log,"collisions",g->physics.collision_history.length);
    pxlog_field(&log,"hole_x",(int)(g->hole_pos.x*1000));pxlog_field(&log,"hole_z",(int)(g->hole_pos.z*1000));
    pxlog_field(&log,"distance",(int)(vec3_distance(g->hole_pos,g->ball.pos)*1000));
    pxlog_field(&log,"angle",(int)(g->cam.angle*1000));pxlog_field(&log,"frame",frame);pxlog_send(&log);

#else
    (void)reason;
#endif
}
static void cancel_drag(void) {if(dragging&&golf_game_get()->state==GOLF_GAME_STATE_AIMING) golf_game_get()->state=GOLF_GAME_STATE_WAITING_FOR_AIM;dragging=false;}
static void pointer(void) {
    golf_t *golf=golf_get();golf_game_t *game=golf_game_get();
    if(fb.pointer_phase==PXA_POINTER_CANCEL) {cancel_drag();return;}
    if(fb.pointer_phase==PXA_POINTER_DOWN) {
        if(fb.pointer_y>=240) {
            int button=golf_clampi(fb.pointer_x/64,0,4);cancel_drag();
            if(golf->state!=GOLF_STATE_IN_GAME) {
                if(button==0) selected_level=(selected_level+19)%20;
                if(button==1||button==4) selected_level=(selected_level+1)%20;
                if(button==2) golf_start_level((int)selected_level);
            } else {
                if(button<2) {game->cam.angle+=(button==0?-0.25f:0.25f);game->cam.auto_rotate=false;}
                if(button==2) golf_start_level(golf->level_num);
                if(button==3) {selected_level=golf->level_num;golf_goto_main_menu();}
                if(button==4&&game->state==GOLF_GAME_STATE_FINISHED) golf_start_level((golf->level_num+1)%20);
            }
        } else if(golf->state!=GOLF_STATE_IN_GAME) {
            if(fb.pointer_y>=140&&fb.pointer_y<=175&&fb.pointer_x>=62&&fb.pointer_x<=258) golf_start_level((int)selected_level);
        } else if(game->state==GOLF_GAME_STATE_WAITING_FOR_AIM) {
            down=V2(fb.pointer_x,fb.pointer_y);dragging=true;golf_game_start_aiming();
        }
    }
    if(dragging&&(fb.pointer_phase==PXA_POINTER_MOVE||fb.pointer_phase==PXA_POINTER_UP)) {
        vec2 delta=V2(fb.pointer_x-down.x,fb.pointer_y-down.y);
        float length=vec2_length(delta);
        game->aim_line.aim_delta=delta;
        game->aim_line.power=golf_clampf((length-10.0f)/100.0f,0,1);
        if(fb.pointer_phase==PXA_POINTER_UP) {
            if(game->aim_line.power>0) golf_game_hit_ball(delta);
            else game->state=GOLF_GAME_STATE_WAITING_FOR_AIM;
            dragging=false;
        }
    }
    trace("input");
}
int32_t pxa_app_start(const uint8_t *config,uint32_t size) {
    memset(&fb,0,sizeof(fb));fb.width=WIDTH;fb.height=HEIGHT;fb.pixels=pixels;fb.upload=upload;fb.upload_bytes=sizeof(upload);
    for(int i=0;i<256;i++) fb.palette[i]=(uint16_t)((((i>>5)&7)*31/7<<11)|(((i>>2)&7)*63/7<<5)|((i&3)*31/3));
    golf_graphics_t *graphics=golf_graphics_get();graphics->window_size=V2(WIDTH,HEIGHT);graphics->viewport_size=V2(WIDTH,VIEW_HEIGHT);graphics->render_size=V2(WIDTH,VIEW_HEIGHT);
    golf_init();frame=0;last_ms=0;selected_level=0;dragging=false;
    pxa_log_write(2,"GOLF source=b9029f1e80c3110ae3d4fc9300854bdaeef96508 courses=20 original_physics=1 render=software audio=off save=session");
    fb.pointer_controller=1;return pxfb_start(&fb,config,size,33);
}
int32_t pxa_app_on_event(const uint8_t *event,uint32_t size) {
    uint32_t kind=pxfb_event(&fb,event,size);
    if(kind&PXFB_ERROR) {pxa_log_write(4,"GOLF framebuffer failure");return PXA_EVENT_HANDLED;}
    if(kind&PXFB_POINTER) pointer();
    if(kind&PXFB_LIFECYCLE) {cancel_drag();if(fb.background) golf_game_pause();else if(golf_game_get()->state==GOLF_GAME_STATE_PAUSED) golf_game_resume();}
    if(kind&PXFB_BACK) {cancel_drag();golf_goto_main_menu();}
    if(kind&(PXFB_READY|PXFB_TICK)) {
        float dt=golf_clampf((fb.time_ms-last_ms)*0.001f,0,0.042f);last_ms=fb.time_ms;
        golf_update(dt);frame++;render();
        if(previous_state!=golf_game_get()->state||frame%15==0) {trace("tick");previous_state=golf_game_get()->state;}
        if(pxfb_present(&fb)<0) pxa_log_write(4,"GOLF frame submission failure");
    }
    return kind?PXA_EVENT_HANDLED:PXA_EVENT_UNHANDLED;
}
void pxa_app_stop(uint32_t reason) {(void)reason;cancel_drag();trace("stop");pxfb_stop(&fb);}
