/* SPDX-License-Identifier: MIT
 * Independent keypad PXA platform boundary for the unchanged Simon Tatham puzzle backends.
 * No Rockbox frontend, shims, or game-rule changes are used here.
 */
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <wasi/api.h>
#include "puzzles.h"
#include "font.h"
#include "pxa_framebuffer.h"
#include "pxa_port_log.h"

enum { WIDTH=400, HEIGHT=416, BOARD_MAX_W=360, BOARD_MAX_H=272,
       UI_BG=240, UI_TEXT, UI_MUTED, UI_GREEN, UI_WHITE, UI_SHADE, UI_DARK };
static uint8_t pixels[WIDTH*HEIGHT], upload[PXA_RASTER_UPLOAD_HEADER_BYTES+WIDTH*HEIGHT];
struct frontend {
    pxfb_t fb; midend *me;
    int ox,oy,bw,bh,cx,cy,cw,ch;
    int menu,started,mode,captured,pointer_button,last_x,last_y,timer,dirty;
    uint32_t generation,sequence,timer_ms;
    key_label *keys; int nkeys,pad_key;
    char status[180],serial[65536];
    unsigned serial_used,serial_total; uint32_t serial_hash;
};
static frontend fe;
struct blitter { int w,h; uint8_t *data; };
static const drawing_api pxa_drawing;

static void pixel(int x,int y,int colour) {
    if(colour>=0 && x>=0 && x<WIDTH && y>=0 && y<HEIGHT &&
       x>=fe.cx && x<fe.cx+fe.cw && y>=fe.cy && y<fe.cy+fe.ch)
        pixels[y*WIDTH+x]=(uint8_t)colour;
}
static void rect(int x,int y,int w,int h,int colour) {
    int x0=max(max(x,fe.cx),0),y0=max(max(y,fe.cy),0);
    int x1=min(min(x+w,fe.cx+fe.cw),WIDTH),y1=min(min(y+h,fe.cy+fe.ch),HEIGHT);
    if(colour<0 || x1<=x0 || y1<=y0)return;
    for(int yy=y0;yy<y1;yy++)memset(pixels+yy*WIDTH+x0,colour,(size_t)(x1-x0));
}
static void line(int x0,int y0,int x1,int y1,int colour) {
    int dx=abs(x1-x0),sx=x0<x1?1:-1,dy=-abs(y1-y0),sy=y0<y1?1:-1,err=dx+dy;
    /* Backends use small board coordinates; bound corrupted/extreme input. */
    if(dx>WIDTH*8 || -dy>HEIGHT*8)return;
    for(;;){pixel(x0,y0,colour);if(x0==x1&&y0==y1)break;
        int twice=2*err;if(twice>=dy){err+=dy;x0+=sx;}if(twice<=dx){err+=dx;y0+=sy;}}
}
static const pz_font *font_at(int size) { return &pz_fonts[min(max(size,6),32)-6]; }
static void text(int x,int y,int size,int align,int colour,const char *s) {
    const pz_font *font=font_at(size);int width=(int)strlen(s)*(int)font->width;
    if((align&3)==ALIGN_HCENTRE)x-=width/2;else if((align&3)==ALIGN_HRIGHT)x-=width;
    int top=(align&ALIGN_VCENTRE)?y-(int)font->height/2:y-(int)font->ascent;
    for(;*s;s++,x+=(int)font->width){unsigned c=(unsigned char)*s;if(c<32||c>126)c='?';
        const uint32_t *rows=font->rows+(c-32)*font->height;
        for(unsigned yy=0;yy<font->height;yy++)for(unsigned xx=0;xx<font->width;xx++)
            if(rows[yy]&(UINT32_C(1)<<xx))pixel(x+(int)xx,top+(int)yy,colour);}
}
static void board_clip(void) { fe.cx=fe.ox;fe.cy=fe.oy;fe.cw=fe.bw;fe.ch=fe.bh; }
static void full_clip(void) { fe.cx=fe.cy=0;fe.cw=WIDTH;fe.ch=HEIGHT; }
static void dr_text(drawing *dr,int x,int y,int type,int size,int align,int colour,const char *s) {
    (void)dr;(void)type;text(x+fe.ox,y+fe.oy,size,align,colour,s);fe.dirty=1;
}
static void dr_rect(drawing *dr,int x,int y,int w,int h,int colour) {
    (void)dr;rect(x+fe.ox,y+fe.oy,w,h,colour);fe.dirty=1;
}
static void dr_line(drawing *dr,int x0,int y0,int x1,int y1,int colour) {
    (void)dr;line(x0+fe.ox,y0+fe.oy,x1+fe.ox,y1+fe.oy,colour);fe.dirty=1;
}
static void dr_polygon(drawing *dr,const int *points,int n,int fill,int outline) {
    (void)dr;if(n<2||n>256)return;
    if(fill>=0){int low=points[1],high=points[1];
        for(int i=1;i<n;i++){low=min(low,points[2*i+1]);high=max(high,points[2*i+1]);}
        low=max(low,fe.cy-fe.oy);high=min(high,fe.cy+fe.ch-1-fe.oy);
        for(int y=low;y<=high;y++){double intersections[256];int count=0;
            double scan=(double)y+0.5;
            for(int i=0,j=n-1;i<n;j=i++){double y0=points[2*j+1],y1=points[2*i+1];
                if((y0<=scan&&y1>scan)||(y1<=scan&&y0>scan))
                    intersections[count++]=points[2*j]+(scan-y0)*(points[2*i]-points[2*j])/(y1-y0);}
            for(int i=1;i<count;i++){double v=intersections[i];int j=i;while(j&&intersections[j-1]>v){intersections[j]=intersections[j-1];j--;}intersections[j]=v;}
            for(int i=0;i+1<count;i+=2){int a=(int)ceil(intersections[i]-0.5),b=(int)floor(intersections[i+1]-0.5);
                rect(a+fe.ox,y+fe.oy,b-a+1,1,fill);}}}
    if(outline>=0)for(int i=0,j=n-1;i<n;j=i++)line(points[2*j]+fe.ox,points[2*j+1]+fe.oy,points[2*i]+fe.ox,points[2*i+1]+fe.oy,outline);
    fe.dirty=1;
}
static void dr_circle(drawing *dr,int x,int y,int radius,int fill,int outline) {
    (void)dr;if(radius<0||radius>WIDTH*4)return;x+=fe.ox;y+=fe.oy;
    int low=max(-radius,fe.cy-y),high=min(radius,fe.cy+fe.ch-1-y);
    for(int dy=low;dy<=high;dy++){int edge=(int)floor(sqrt((double)radius*radius-(double)dy*dy));
        if(fill>=0)rect(x-edge,y+dy,2*edge+1,1,fill);}
    if(outline>=0){int dx=radius,dy=0,error=1-radius;
        while(dx>=dy){pixel(x+dx,y+dy,outline);pixel(x+dy,y+dx,outline);pixel(x-dy,y+dx,outline);pixel(x-dx,y+dy,outline);
            pixel(x-dx,y-dy,outline);pixel(x-dy,y-dx,outline);pixel(x+dy,y-dx,outline);pixel(x+dx,y-dy,outline);
            dy++;if(error<0)error+=2*dy+1;else{dx--;error+=2*(dy-dx)+1;}}}
    fe.dirty=1;
}
static void dr_thick(drawing *dr,float thickness,float x0,float y0,float x1,float y1,int colour) {
    (void)dr;x0+=fe.ox;x1+=fe.ox;y0+=fe.oy;y1+=fe.oy;
    float radius=max(thickness,1.0F)/2.0F,dx=x1-x0,dy=y1-y0,length=dx*dx+dy*dy;
    int lowx=max(fe.cx,(int)floor(min(x0,x1)-radius)),highx=min(fe.cx+fe.cw-1,(int)ceil(max(x0,x1)+radius));
    int lowy=max(fe.cy,(int)floor(min(y0,y1)-radius)),highy=min(fe.cy+fe.ch-1,(int)ceil(max(y0,y1)+radius));
    for(int y=lowy;y<=highy;y++)for(int x=lowx;x<=highx;x++){float t=length?((x-x0)*dx+(y-y0)*dy)/length:0;
        t=min(max(t,0),1);float a=x-(x0+t*dx),b=y-(y0+t*dy);if(a*a+b*b<=radius*radius)pixel(x,y,colour);}
    fe.dirty=1;
}
static void dr_clip(drawing *dr,int x,int y,int w,int h) {
    (void)dr;fe.cx=max(x+fe.ox,fe.ox);fe.cy=max(y+fe.oy,fe.oy);
    fe.cw=max(0,min(x+fe.ox+w,fe.ox+fe.bw)-fe.cx);fe.ch=max(0,min(y+fe.oy+h,fe.oy+fe.bh)-fe.cy);
}
static void dr_unclip(drawing *dr) { (void)dr;board_clip(); }
static void dr_start(drawing *dr) { (void)dr;board_clip(); }
static void dr_update(drawing *dr,int x,int y,int w,int h) { (void)dr;(void)x;(void)y;(void)w;(void)h;fe.dirty=1; }
static void dr_end(drawing *dr) { (void)dr;fe.dirty=1; }
static void dr_status(drawing *dr,const char *s) { (void)dr;snprintf(fe.status,sizeof(fe.status),"%s",s);fe.dirty=1; }
static blitter *dr_blitter_new(drawing *dr,int w,int h) {
    (void)dr;if(w<=0||h<=0||w>WIDTH*2||h>HEIGHT*2)fatal("PUZZLE invalid blitter");
    blitter *b=snew(blitter);b->w=w;b->h=h;b->data=snewn((size_t)w*h,uint8_t);return b;
}
static void dr_blitter_free(drawing *dr,blitter *b) { (void)dr;sfree(b->data);sfree(b); }
static void dr_blitter_save(drawing *dr,blitter *b,int x,int y) {
    (void)dr;x+=fe.ox;y+=fe.oy;
    for(int yy=0;yy<b->h;yy++)for(int xx=0;xx<b->w;xx++)
        b->data[yy*b->w+xx]=(x+xx>=0&&x+xx<WIDTH&&y+yy>=0&&y+yy<HEIGHT)?pixels[(y+yy)*WIDTH+x+xx]:0;
}
static void dr_blitter_load(drawing *dr,blitter *b,int x,int y) {
    (void)dr;for(int yy=0;yy<b->h;yy++)for(int xx=0;xx<b->w;xx++)pixel(x+fe.ox+xx,y+fe.oy+yy,b->data[yy*b->w+xx]);fe.dirty=1;
}
static char *dr_fallback(drawing *dr,const char *const *strings,int n) {
    (void)dr;for(int i=0;i<n;i++){int okay=1;for(const unsigned char *s=(const unsigned char*)strings[i];*s;s++)if(*s>=127)okay=0;
        if(okay)return dupstr(strings[i]);}return dupstr(n?strings[n-1]:"");
}
static const drawing_api pxa_drawing={.version=1,.draw_text=dr_text,.draw_rect=dr_rect,.draw_line=dr_line,
    .draw_polygon=dr_polygon,.draw_circle=dr_circle,.draw_update=dr_update,.clip=dr_clip,.unclip=dr_unclip,
    .start_draw=dr_start,.end_draw=dr_end,.status_bar=dr_status,.blitter_new=dr_blitter_new,.blitter_free=dr_blitter_free,
    .blitter_save=dr_blitter_save,.blitter_load=dr_blitter_load,.text_fallback=dr_fallback,.draw_thick_line=dr_thick};

void frontend_default_colour(frontend *f,float *output) { (void)f;output[0]=output[1]=output[2]=0.9F; }
void get_random_seed(void **seed,int *size) {
    uint8_t *bytes=snewn(32,uint8_t);if(__wasi_random_get(bytes,32))fatal("PUZZLE random unavailable");*seed=bytes;*size=32;
}
void activate_timer(frontend *f) { f->timer=1;f->timer_ms=f->fb.time_ms; }
void deactivate_timer(frontend *f) { f->timer=0; }
static uint16_t rgb(unsigned c) { return (uint16_t)((((c>>19)&31)<<11)|(((c>>10)&63)<<5)|((c>>3)&31)); }
static void ui_palette(void) {
    const unsigned colors[]={0xf6f5f0,0x243539,0x6b7a76,0x1e7c74,0xffffff,0xcbd5c5,0x124942};
    for(unsigned i=0;i<sizeof(colors)/sizeof(colors[0]);i++)fe.fb.palette[UI_BG+i]=rgb(colors[i]);
}
static void game_size_palette(void) {
    int w=BOARD_MAX_W,h=BOARD_MAX_H;midend_size(fe.me,&w,&h,false,1.0);
    if(w<=0||h<=0||w>BOARD_MAX_W||h>BOARD_MAX_H)fatal("PUZZLE board too large %dx%d",w,h);
    fe.bw=w;fe.bh=h;fe.ox=(WIDTH-w)/2;fe.oy=52+(BOARD_MAX_H-h)/2;
    int count;float *c=midend_colours(fe.me,&count);if(count>UI_BG)fatal("PUZZLE palette too large");
    for(int i=0;i<count;i++){unsigned r=(unsigned)(min(max(c[3*i],0),1)*255+0.5F),g=(unsigned)(min(max(c[3*i+1],0),1)*255+0.5F),b=(unsigned)(min(max(c[3*i+2],0),1)*255+0.5F);
        fe.fb.palette[i]=rgb((r<<16)|(g<<8)|b);}sfree(c);ui_palette();
    if(fe.fb.ready)pxa_raster_upload_palette_rgb565(fe.fb.context,fe.fb.palette,upload,sizeof(upload));
    board_clip();rect(fe.ox,fe.oy,fe.bw,fe.bh,0);
}
/* Request ownership follows the original midend API; refresh after each NEW. */
static void refresh_keys(void) {
    if(fe.keys)free_keys(fe.keys,fe.nkeys);
    fe.keys=NULL;fe.nkeys=0;
    fe.keys=midend_request_keys(fe.me,&fe.nkeys);
    if(!fe.keys||fe.nkeys<1||fe.nkeys>10)fatal("PUZZLE unsupported keypad count %d",fe.nkeys);
    pxlog_t log;pxlog_init(&log);pxlog_text(&log,"PZ KEYS");pxlog_field(&log,"count",fe.nkeys);pxlog_send(&log);
    for(int i=0;i<fe.nkeys;i++){
        if(fe.keys[i].button!='\b')for(const unsigned char *p=(const unsigned char*)fe.keys[i].label;*p;p++)
            if(*p<32||*p>126)fatal("PUZZLE unsupported keypad label");
        pxlog_init(&log);pxlog_text(&log,"PZ KEY");pxlog_field(&log,"index",i);pxlog_field(&log,"button",fe.keys[i].button);
        pxlog_text(&log," label=");pxlog_text(&log,fe.keys[i].button=='\b'?"CLR":fe.keys[i].label);pxlog_send(&log);
    }
}
static uint32_t hash_pixels(void) {
    uint32_t h=2166136261u;for(int y=fe.oy;y<fe.oy+fe.bh;y++)for(int x=fe.ox;x<fe.ox+fe.bw;x++)h=(h^pixels[y*WIDTH+x])*16777619u;return h;
}
static void serial_write(void *ctx,const void *buf,int len) {
    (void)ctx;const uint8_t *data=buf;for(int i=0;i<len;i++)fe.serial_hash=(fe.serial_hash^data[i])*16777619u;
    fe.serial_total+=(unsigned)len;unsigned copy=min((unsigned)len,(unsigned)sizeof(fe.serial)-1-fe.serial_used);
    memcpy(fe.serial+fe.serial_used,buf,copy);fe.serial_used+=copy;
}
static void trace(const char *action) {
#if PXA_RETRO_TRACE

    int pos=0,states=0,index=0;char move[96]={0};
    fe.serial_used=fe.serial_total=0;fe.serial_hash=2166136261u;
    if(fe.me&&fe.started){midend_serialise(fe.me,serial_write,NULL);fe.serial[fe.serial_used]=0;
        char *p=fe.serial,*end=p+fe.serial_used;
        while(p<end){char *colon=strchr(p,':');if(!colon||colon-p>20)break;char *second=strchr(colon+1,':');if(!second)break;
            int len=atoi(colon+1);char *value=second+1;if(len<0||value+len>end)break;
            if(!strncmp(p,"STATEPOS",8))pos=atoi(value);else if(!strncmp(p,"NSTATES",7))states=atoi(value);
            else if(!strncmp(p,"MOVE",4)||!strncmp(p,"SOLVE",5)||!strncmp(p,"RESTART",7)){
                index++;if(index<pos){unsigned n=min((unsigned)len,(unsigned)sizeof(move)-1);memcpy(move,value,n);move[n]=0;}}
            p=value+len;while(p<end&&(*p=='\n'||*p=='\r'))p++;}}
    pxlog_t log;pxlog_init(&log);pxlog_text(&log,"PZ ");pxlog_text(&log,action);
    pxlog_field(&log,"gen",fe.generation);pxlog_field(&log,"seq",fe.sequence);pxlog_field(&log,"menu",fe.menu);
    pxlog_field(&log,"pos",pos);pxlog_field(&log,"states",states);pxlog_field(&log,"timer",fe.timer);
    pxlog_field(&log,"undo",fe.me&&fe.started?midend_can_undo(fe.me):0);pxlog_field(&log,"redo",fe.me&&fe.started?midend_can_redo(fe.me):0);
    pxlog_field(&log,"ox",fe.ox);pxlog_field(&log,"oy",fe.oy);pxlog_field(&log,"bw",fe.bw);pxlog_field(&log,"bh",fe.bh);
    pxlog_field(&log,"tile",fe.me&&fe.started?midend_tilesize(fe.me):0);pxlog_field(&log,"mode",fe.mode);
    pxlog_field(&log,"board",(int32_t)hash_pixels());pxlog_field(&log,"serial",(int32_t)fe.serial_hash);pxlog_send(&log);
    if(fe.me&&fe.started){pxlog_init(&log);pxlog_text(&log,"PZ OUTCOME");pxlog_field(&log,"status",midend_status(fe.me));pxlog_send(&log);
        char *id=midend_get_game_id(fe.me);pxlog_init(&log);pxlog_text(&log,"PZ ID ");pxlog_text(&log,id);pxlog_send(&log);sfree(id);
        if(*move){pxlog_init(&log);pxlog_text(&log,"PZ MOVE ");pxlog_text(&log,move);pxlog_send(&log);}
        if(*fe.status){pxlog_init(&log);pxlog_text(&log,"PZ STATUS ");pxlog_text(&log,fe.status);pxlog_send(&log);}}

#else
    (void)action;
#endif
}
static void button(int x,int y,int w,int h,const char *label,int enabled) {
    rect(x,y,w,h,enabled?UI_GREEN:UI_SHADE);text(x+w/2,y+h/2,12,ALIGN_HCENTRE|ALIGN_VCENTRE,enabled?UI_WHITE:UI_MUTED,label);
}
static void draw_ui(void) {
    full_clip();
    if(fe.menu){memset(pixels,UI_BG,sizeof(pixels));text(20,36,24,0,UI_TEXT,thegame.name);
        text(20,67,12,0,UI_MUTED,"SIMON TATHAM'S PUZZLES");
        text(20,112,14,0,UI_TEXT,!strcmp(thegame.name,"Fifteen")?"Slide tiles into numeric order.":"Select a cell, then tap an original key.");
        if(!strcmp(thegame.name,"Fifteen"))text(20,141,12,0,UI_MUTED,"Tap a tile in the blank's row or column.");
        button(126,180,148,44,"NEW GAME",1);if(fe.started)button(126,244,148,44,"CONTINUE",1);
        text(20,338,12,0,UI_MUTED,"NEW / RESTART / UNDO / REDO");text(20,365,12,0,UI_MUTED,"HOME exits the game.");
    }else{
        rect(0,0,WIDTH,52,UI_BG);rect(0,332,WIDTH,HEIGHT-332,UI_BG);text(16,32,20,0,UI_TEXT,thegame.name);
        const char *modes[]={"LEFT","RIGHT","MIDDLE"};button(304,10,80,28,modes[fe.mode],1);
        text(12,49,10,0,UI_MUTED,fe.status);
        for(int i=0;i<fe.nkeys;i++){
            int left=i*WIDTH/fe.nkeys,right=(i+1)*WIDTH/fe.nkeys;
            const char *label=fe.keys[i].button=='\b'?"CLR":fe.keys[i].label;
            rect(left+2,332,right-left-4,31,i==fe.pad_key?UI_DARK:UI_GREEN);
            text((left+right)/2,347,strlen(label)>4?10:12,ALIGN_HCENTRE|ALIGN_VCENTRE,UI_WHITE,label);
        }
        const char *labels[]={"NEW","RESTART","UNDO","REDO","MENU"};
        for(int i=0;i<5;i++)button(i*80+3,364,74,32,labels[i],i==2?midend_can_undo(fe.me):i==3?midend_can_redo(fe.me):1);
    }
    board_clip();fe.dirty=0;
    if(fe.fb.ready&&pxfb_present(&fe.fb)<0)fatal("PUZZLE raster submission failed");
}
static void release_pointer(int cancel) {
    if(fe.captured&&fe.me){if(cancel)midend_process_key(fe.me,-100,-100,fe.pointer_button+3);
        midend_process_key(fe.me,cancel?-100:fe.last_x,cancel?-100:fe.last_y,fe.pointer_button+6);}
    fe.captured=0;
}
static void new_game(void) {
    release_pointer(1);full_clip();memset(pixels,UI_BG,sizeof(pixels));
    if(!fe.me)fe.me=midend_new(&fe,&thegame,&pxa_drawing,&fe);
    midend_new_game(fe.me);fe.started=1;fe.menu=0;fe.generation++;fe.sequence++;
    refresh_keys();fe.pad_key=0;game_size_palette();midend_redraw(fe.me);fe.dirty=1;trace("new");
}
static void command(int command) {
    release_pointer(1);fe.sequence++;
    if(command==0){new_game();return;}
    if(command==4){midend_stop_anim(fe.me);fe.menu=1;trace("menu");}
    else if(command==1){midend_restart_game(fe.me);trace("restart");}
    else{midend_process_key(fe.me,0,0,command==2?UI_UNDO:UI_REDO);trace(command==2?"undo":"redo");}
    fe.dirty=1;
}
static void pointer(void) {
    int x=fe.fb.pointer_x,y=fe.fb.pointer_y,phase=fe.fb.pointer_phase;
    if(phase==PXA_POINTER_CANCEL){release_pointer(1);fe.sequence++;trace("cancel");fe.dirty=1;return;}
    if(fe.menu){if(phase!=PXA_POINTER_DOWN)return;
        if(x>=126&&x<274&&y>=180&&y<224)new_game();
        else if(fe.started&&x>=126&&x<274&&y>=244&&y<288){fe.menu=0;full_clip();memset(pixels,UI_BG,sizeof(pixels));board_clip();rect(fe.ox,fe.oy,fe.bw,fe.bh,0);midend_force_redraw(fe.me);fe.sequence++;trace("continue");fe.dirty=1;}return;}
    if(phase==PXA_POINTER_DOWN){
        release_pointer(1);
        if(y>=332&&y<363&&x>=0&&x<WIDTH){
            int index=0;while(index+1<fe.nkeys&&x>=(index+1)*WIDTH/fe.nkeys)index++;
            /* A keypad touch is a key, never an outside-board mouse click. */
            midend_process_key(fe.me,0,0,fe.keys[index].button);fe.sequence++;
            pxlog_t log;pxlog_init(&log);pxlog_text(&log,"PZ KEYPAD");pxlog_field(&log,"index",index);pxlog_field(&log,"button",fe.keys[index].button);pxlog_send(&log);
            trace("key");fe.dirty=1;return;
        }
        if(y>=364&&y<396&&x>=0&&x<WIDTH){command(x/80);return;}
        if(x>=304&&x<384&&y>=10&&y<38){fe.mode=(fe.mode+1)%3;fe.sequence++;trace("mode");fe.dirty=1;return;}
        if(x<fe.ox||x>=fe.ox+fe.bw||y<fe.oy||y>=fe.oy+fe.bh)return;
        const int buttons[]={LEFT_BUTTON,RIGHT_BUTTON,MIDDLE_BUTTON};fe.pointer_button=buttons[fe.mode];fe.captured=1;
        fe.last_x=x-fe.ox;fe.last_y=y-fe.oy;midend_process_key(fe.me,fe.last_x,fe.last_y,fe.pointer_button);fe.sequence++;trace("down");
    }else if(fe.captured){fe.last_x=x-fe.ox;fe.last_y=y-fe.oy;
        midend_process_key(fe.me,fe.last_x,fe.last_y,fe.pointer_button+3);
        if(phase==PXA_POINTER_UP){release_pointer(0);fe.sequence++;trace("up");}}
    fe.dirty=1;
}
#include "../puzzles/controller.h"
int32_t pxa_app_start(const uint8_t *config,uint32_t bytes) {
    memset(&fe,0,sizeof(fe));fe.menu=1;full_clip();memset(pixels,UI_BG,sizeof(pixels));
    fe.fb.width=WIDTH;fe.fb.height=HEIGHT;fe.fb.pixels=pixels;fe.fb.upload=upload;fe.fb.upload_bytes=sizeof(upload);ui_palette();
    fe.dirty=1;trace("start");return pxfb_start(&fe.fb,config,bytes,25);
}
int32_t pxa_app_on_event(const uint8_t *bytes,uint32_t size) {
    uint32_t old_buttons=fe.fb.buttons;uint32_t kind=pxfb_event(&fe.fb,bytes,size);
    if(kind&PXFB_CONTROLLER)puzzle_controller(fe.fb.buttons&~old_buttons);
    if(kind&PXFB_ERROR)fatal("PUZZLE render context failed %d",fe.fb.error);
    if(kind&PXFB_POINTER)pointer();
    if(kind&PXFB_LIFECYCLE){release_pointer(1);fe.timer_ms=fe.fb.time_ms;trace("lifecycle");}
    if(kind&PXFB_BACK){release_pointer(1);if(fe.me)midend_stop_anim(fe.me);fe.menu=1;fe.dirty=1;trace("back");}
    if((kind&PXFB_TICK)&&fe.started&&!fe.menu&&fe.timer){int active=fe.timer;uint32_t elapsed=fe.fb.time_ms-fe.timer_ms;
        fe.timer_ms=fe.fb.time_ms;midend_timer(fe.me,(float)elapsed/1000.0F);if(active&&!fe.timer)trace("settled");}
    if((kind&PXFB_READY)||fe.dirty)draw_ui();
    return kind?PXA_EVENT_HANDLED:PXA_EVENT_UNHANDLED;
}
void pxa_app_stop(uint32_t reason) {
    (void)reason;release_pointer(1);trace("stop");if(fe.keys){free_keys(fe.keys,fe.nkeys);fe.keys=NULL;fe.nkeys=0;}if(fe.me){midend_free(fe.me);fe.me=NULL;}pxfb_stop(&fe.fb);
}
