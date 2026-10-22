/* GPL-2.0-or-later. Layout and glyphs adapted from ../nes/controls.c. */
#include "controls.h"
#include "../controls/touch_layout.h"
#include "font8x8.h"
#include <string.h>

static uint8_t small_letter(char c,unsigned y) {
    static const char letters[]="ACELMNRSTU";
    static const uint8_t rows[][7]={
        {14,17,17,31,17,17,17},{15,16,16,16,16,16,15},
        {31,16,16,30,16,16,31},{16,16,16,16,16,16,31},
        {17,27,21,21,17,17,17},{17,25,25,21,19,19,17},
        {30,17,17,30,20,18,17},{15,16,16,14,1,1,30},
        {31,4,4,4,4,4,4},{17,17,17,17,17,17,14}};
    const char *p=strchr(letters,c);return p?rows[p-letters][y]:0;
}
static int inside(int x,int y,int w,int h,int radius) {
    if(x<0 || y<0 || x>=w || y>=h)return 0;
    int dx=x<radius?radius-x-1:(x>=w-radius?x-(w-radius):0);
    int dy=y<radius?radius-y-1:(y>=h-radius?y-(h-radius):0);
    return dx*dx+dy*dy<radius*radius;
}
void emu_controls_set_labels(emu_controls_t *c,const char *select_label,const char *start_label,const char *menu_label) {
    if(!c->safe.width || !c->safe.height)c->safe=(emu_display_rect_t){0,0,c->width,c->height};
    emu_touch_rect_t rects[EMU_TOUCH_COUNT];
    unsigned q=emu_touch_layout(rects,c->safe.width,c->safe.height,EMU_CONTROLLER_STYLE,c->safe.x,c->safe.y);
    unsigned scale=(q+32768)>>16;if(!scale)scale=1;
    emu_control_button_t layout[EMU_CONTROLS_COUNT]={
        {0,0,0,0,EMU_PAD_LEFT,0,"<",0},
        {0,0,0,0,EMU_PAD_RIGHT,0,">",0},
        {0,0,0,0,EMU_PAD_UP,0,"^",0},
        {0,0,0,0,EMU_PAD_DOWN,0,"v",0},
        {0,0,0,0,EMU_PAD_SELECT,0,select_label,0},
        {0,0,0,0,EMU_PAD_START,0,start_label,0},
        {0,0,0,0,0,0,menu_label,0},
        {0,0,0,0,EMU_PAD_B,0x6808,"B",0},
        {0,0,0,0,EMU_PAD_A,0x980b,"A",0},
        {0,0,0,0,EMU_PAD_X,0x180b,"X",0},
        {0,0,0,0,EMU_PAD_Y,0x1108,"Y",0},
        {0,0,0,0,EMU_PAD_L,0,"L",0},
        {0,0,0,0,EMU_PAD_R,0,"R",0},
    };
#if EMU_CONTROLLER_STYLE == 6
    const unsigned slots[]={EMU_CONTROL_A,EMU_CONTROL_B,EMU_CONTROL_X,EMU_CONTROL_Y,EMU_CONTROL_L,EMU_CONTROL_R};
    const uint16_t bits[]={EMU_PAD_A,EMU_PAD_B,EMU_PAD_Y,EMU_PAD_X,EMU_PAD_L,EMU_PAD_R};
    const char *labels[]={"A","B","C","X","Y","Z"};
    for(unsigned n=0;n<6;++n){layout[slots[n]].bit=bits[n];layout[slots[n]].label=labels[n];layout[slots[n]].tint=0x4008;}
#endif
    memset(c->mask,0,sizeof(c->mask));c->mask_used=0;c->cached_alpha=255;
    for(unsigned i=0;i<EMU_CONTROLS_COUNT;++i) {
        emu_control_button_t *b=&c->buttons[i];*b=layout[i];
        b->x=rects[i].x;b->y=rects[i].y;b->w=rects[i].w;b->h=rects[i].h;
        if(b->w<=0 || b->h<=0){b->w=b->h=0;continue;}
        b->mask_offset=c->mask_used;
        unsigned bytes=(unsigned)b->w*b->h;
        if(bytes>EMU_CONTROLS_MASK_BYTES-c->mask_used){b->w=b->h=0;continue;}
        c->mask_used+=bytes;
        int radius=emu_touch_px(i>=6?11:6,q);
        if(radius>b->w/2)radius=b->w/2;if(radius>b->h/2)radius=b->h/2;
        if(radius<1)radius=1;
        uint8_t *mask=c->mask+b->mask_offset;
        for(int yy=0;yy<b->h;++yy)for(int xx=0;xx<b->w;++xx)
            if(inside(xx,yy,b->w,b->h,radius))
                mask[yy*b->w+xx]=inside(xx-1,yy-1,b->w-2,b->h-2,radius-1)?1:2;
        unsigned count=strlen(b->label),small=count>1;
        unsigned logical_width=small?count*6-1:8,logical_height=small?7:8;
        unsigned lw=logical_width*scale,lh=logical_height*scale;
        if(lw>(unsigned)b->w-4){lw=b->w>4?b->w-4:1;lh=(logical_height*lw+logical_width-1)/logical_width;}
        int left=(b->w-(int)lw)/2,top=(b->h-(int)lh)/2;
        for(unsigned yy=0;yy<lh;++yy)for(unsigned xx=0;xx<lw;++xx) {
            unsigned sx=xx*logical_width/lw,sy=yy*logical_height/lh;
            unsigned ink=small?(sx%6<5 && (small_letter(b->label[sx/6],sy)&(1u<<(4-sx%6)))):
                (font8x8_basic[(unsigned char)b->label[0]&127][sy]&(1u<<sx));
            if(ink && left+(int)xx>=0 && left+(int)xx<b->w && top+(int)yy>=0 && top+(int)yy<b->h)
                mask[(top+yy)*b->w+left+xx]=3;
        }
    }
}
void emu_controls_init(emu_controls_t *c,unsigned width,unsigned height) {
    memset(c,0,sizeof(*c));c->width=width;c->height=height;c->stride=width;c->alpha=3;
    c->safe=(emu_display_rect_t){0,0,width,height};
    emu_controls_set_labels(c,"SELECT","START","MENU");
}
unsigned emu_controls_cycle(emu_controls_t *c) {
    c->alpha=c->alpha==3?2:(c->alpha==2?5:(c->alpha==5?0:3));return c->alpha;
}
int emu_controls_visible(const emu_controls_t *c){return c->alpha && !c->controller_hidden;}
int emu_controls_use_controller(const emu_controls_t *c){return c->controller_hidden;}
int emu_controls_use_touch(const emu_controls_t *c){return !c->controller_hidden;}
int emu_controls_hit(const emu_controls_t *c,int x,int y) {
    unsigned q=emu_touch_scale(c->safe.width,c->safe.height);
    if(!emu_controls_visible(c) || x<(int)c->safe.x || x>=(int)(c->safe.x+c->safe.width) ||
       y<(int)c->safe.y || y>=(int)(c->safe.y+c->safe.height)-emu_touch_gesture_margin(q))return 0;
    emu_touch_rect_t b[EMU_CONTROLS_COUNT];
    for(unsigned i=0;i<EMU_CONTROLS_COUNT;++i)b[i]=(emu_touch_rect_t){c->buttons[i].x,c->buttons[i].y,c->buttons[i].w,c->buttons[i].h};
    int target=emu_touch_target(b,EMU_CONTROLS_COUNT,x,y,q);
    if(target>=16){unsigned bits=target-16,result=0;for(unsigned i=0;i<4;++i)if(bits&(1u<<i))result|=c->buttons[i].bit;return result;}
    return target<0?0:(c->buttons[target].bit?c->buttons[target].bit:-1);
}
static void update(emu_controls_t *c,unsigned pressed) {
    if(c->cached_alpha==c->alpha && c->cached_pressed==pressed)return;
    for(unsigned i=0;i<EMU_CONTROLS_COUNT;++i) {
        unsigned down=(c->buttons[i].bit&pressed)!=0;
        if(c->cached_alpha==c->alpha && ((c->cached_pressed&c->buttons[i].bit)!=0)==down)continue;
        unsigned alpha=c->alpha+(down?2:0);if(alpha>8)alpha=8;
        for(unsigned part=1;part<=3;++part) {
            unsigned a=alpha+(part==2?1:part==3?3:0);if(a>8)a=8;
            uint16_t color=part==1?c->buttons[i].tint:0xffff;
            unsigned style=i*3+part;
            for(unsigned v=0;v<32;++v) {
                c->red[style][v]=((v*(8-a)+(color>>11)*a+4)>>3)<<11;
                c->blue[style][v]=(v*(8-a)+(color&31)*a+4)>>3;
            }
            for(unsigned v=0;v<64;++v)c->green[style][v]=((v*(8-a)+((color>>5)&63)*a+4)>>3)<<5;
        }
    }
    c->cached_alpha=c->alpha;c->cached_pressed=pressed;
}
void emu_controls_clear_bars_rows(const emu_controls_t *c,uint16_t *out,const emu_display_rect_t *view,const uint8_t *dirty) {
    if(!emu_controls_visible(c))return;
    unsigned stride=c->stride?c->stride:c->width;
    for(unsigned i=0;i<EMU_CONTROLS_COUNT;++i) {
        const emu_control_button_t *b=&c->buttons[i];
        for(int yy=0;yy<b->h;++yy) {
            int y=b->y+yy;if(y<0 || y>=(int)c->height)continue;
            if(dirty && !dirty[y])continue;
            int left=b->x<0?0:b->x,right=b->x+b->w;
            if(right>(int)c->width)right=c->width;if(right<=left)continue;
            uint16_t *row=out+(unsigned)y*stride;
            if(y<(int)view->y || y>=(int)(view->y+view->height))memset(row+left,0,(right-left)*2u);
            else {
                int end=right<(int)view->x?right:(int)view->x;
                if(end>left)memset(row+left,0,(end-left)*2u);
                int begin=left>(int)(view->x+view->width)?left:(int)(view->x+view->width);
                if(right>begin)memset(row+begin,0,(right-begin)*2u);
            }
        }
    }
}
void emu_controls_draw_rows(emu_controls_t *c,uint16_t *out,unsigned pressed,const uint8_t *dirty) {
    if(!emu_controls_visible(c))return;
    update(c,pressed);
    unsigned stride=c->stride?c->stride:c->width;
    for(unsigned i=0;i<EMU_CONTROLS_COUNT;++i) {
        const emu_control_button_t *b=&c->buttons[i];
        const uint8_t *mask=c->mask+b->mask_offset;
        for(int yy=0;yy<b->h;++yy) {
            int y=b->y+yy;if(y<0 || y>=(int)c->height)continue;
            if(dirty && !dirty[y])continue;
            uint16_t *row=out+(unsigned)y*stride;
            for(int xx=0;xx<b->w;++xx) {
                int x=b->x+xx;if(x<0 || x>=(int)c->width)continue;
                unsigned part=mask[yy*b->w+xx];if(!part)continue;
                unsigned style=i*3+part,v=row[x];
                row[x]=c->red[style][v>>11]|c->green[style][(v>>5)&63]|c->blue[style][v&31];
            }
        }
    }
}
void emu_controls_clear_bars(const emu_controls_t *c,uint16_t *out,const emu_display_rect_t *view) {
    emu_controls_clear_bars_rows(c,out,view,NULL);
}
void emu_controls_draw(emu_controls_t *c,uint16_t *out,unsigned pressed) {
    emu_controls_draw_rows(c,out,pressed,NULL);
}
