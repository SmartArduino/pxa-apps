/* GPL-2.0-or-later */
#include "controls.h"
#include "font8x8.h"
#include <string.h>

/* Original 5x7 capitals for the cached game buttons. Menus use Host fonts;
 * these fixed ASCII labels need neither Unicode decoding nor font services. */
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

static uint16_t blend(uint16_t back,uint16_t front,unsigned alpha) {
    unsigned r=((((back>>11)&31)*(8-alpha)+((front>>11)&31)*alpha+4)>>3);
    unsigned g=((((back>>5)&63)*(8-alpha)+((front>>5)&63)*alpha+4)>>3);
    unsigned b=(((back&31)*(8-alpha)+(front&31)*alpha+4)>>3);
    return (r<<11)|(g<<5)|b;
}
static int inside(int x,int y,int w,int h,int radius) {
    if(x<0 || y<0 || x>=w || y>=h)return 0;
    int dx=x<radius?radius-x-1:(x>=w-radius?x-(w-radius):0);
    int dy=y<radius?radius-y-1:(y>=h-radius?y-(h-radius):0);
    return dx*dx+dy*dy<radius*radius;
}
static void pixel(nes_controls_t *c,int x,int y,unsigned style) {
    if(x<0 || y<0 || x>=(int)c->width || y>=(int)c->height)return;
    c->mask[y*c->width+x]=style;c->row_used[y]=1;
    nes_control_bounds_t *b=&c->paint_bounds[(style-1)/3];
    if((unsigned)x<b->left)b->left=x;
    if((unsigned)y<b->top)b->top=y;
    if((unsigned)x+1>b->right)b->right=x+1;
    if((unsigned)y+1>b->bottom)b->bottom=y+1;
}
void nes_controls_set_labels(nes_controls_t *c,const char *select_label,const char *start_label,const char *menu_label) {
    unsigned width=c->width,height=c->height;
    unsigned scale=nes_display_ui_scale(width,height);
    memset(c->mask,0,sizeof(c->mask));memset(c->row_used,0,sizeof(c->row_used));
    for(unsigned i=0;i<NES_CONTROLS_COUNT;++i)
        c->paint_bounds[i]=(nes_control_bounds_t){width,height,0,0};
    unsigned safe_width=c->safe.width?c->safe.width:width;
    unsigned safe_height=c->safe.height?c->safe.height:height;
    int y=safe_height/scale-72;
    const nes_control_button_t buttons[NES_CONTROLS_COUNT]={
        {14,y,30,23,64,0,"<"},{80,y,30,23,128,0,">"},
        {47,y-24,30,23,16,0,"^"},{47,y+24,30,23,32,0,"v"},
        {122,y+19,48,23,4,0,select_label},{176,y+19,48,23,8,0,start_label},
        {128,y-17,72,23,0,0,menu_label},
        {230,y+16,30,27,2,0x6808,"B"},{260,y-10,30,27,1,0x980b,"A"},
    };
    for(unsigned i=0;i<NES_CONTROLS_COUNT;++i) {
        c->buttons[i]=buttons[i];nes_control_button_t *b=&c->buttons[i];
        b->x=c->safe.x+b->x*(int)safe_width/296;b->w=b->w*(int)safe_width/296;
        b->y=c->safe.y+b->y*scale;b->h*=scale;
        int radius=(i>=6?11:6)*scale;
        for(int yy=0;yy<b->h;++yy)for(int xx=0;xx<b->w;++xx) {
            if(!inside(xx,yy,b->w,b->h,radius))continue;
            unsigned style=inside(xx-1,yy-1,b->w-2,b->h-2,radius-1)?1:2;
            pixel(c,b->x+xx,b->y+yy,i*3+style);
        }
        unsigned count=strlen(b->label),small=count>1;
        unsigned logical_width=small?count*6-1:8,logical_height=small?7:8;
        unsigned label_width=logical_width*scale,label_height=logical_height*scale;
        if(label_width>(unsigned)b->w-4) {
            label_width=b->w>4?b->w-4:1;
            label_height=(logical_height*label_width+logical_width-1)/logical_width;
        }
        int left=b->x+(b->w-(int)label_width)/2,top=b->y+(b->h-(int)label_height)/2;
        for(unsigned yy=0;yy<label_height;++yy)for(unsigned xx=0;xx<label_width;++xx) {
            unsigned sx=xx*logical_width/label_width,sy=yy*logical_height/label_height;
            unsigned ink=small?(sx%6<5 && (small_letter(b->label[sx/6],sy)&(1u<<(4-sx%6)))):
                (font8x8_basic[(unsigned char)b->label[0]&127][sy]&(1u<<sx));
            if(ink)pixel(c,left+xx,top+yy,i*3+3);
        }
    }
}
void nes_controls_init(nes_controls_t *c,unsigned width,unsigned height,const uint16_t *palette) {
    memset(c,0,sizeof(*c));c->width=width;c->height=height;c->alpha=3;c->cached_alpha=255;
    c->safe=(nes_display_rect_t){0,0,width,height};
    memcpy(c->palette,palette,sizeof(c->palette));memcpy(c->lut[0],palette,sizeof(c->palette));
    nes_controls_set_labels(c,"SELECT","START","MENU");nes_controls_update(c,0);
}
void nes_controls_update(nes_controls_t *c,unsigned pressed) {
    if(c->cached_alpha==c->alpha && c->cached_pressed==pressed)return;
    for(unsigned i=0;i<NES_CONTROLS_COUNT;++i) {
        unsigned down=(c->buttons[i].bit&pressed)!=0;
        if(c->cached_alpha==c->alpha && ((c->cached_pressed&c->buttons[i].bit)!=0)==down)continue;
        unsigned a=c->alpha+(down?2:0);if(a>8)a=8;
        unsigned edge=a+1;if(edge>8)edge=8;
        unsigned label=a+3;if(label>8)label=8;
        c->black_lut[i*3+1]=blend(0,c->buttons[i].tint,a);
        c->black_lut[i*3+2]=blend(0,0xffff,edge);
        c->black_lut[i*3+3]=blend(0,0xffff,label);
        for(unsigned color=0;color<64;++color) {
            c->lut[i*3+1][color]=blend(c->palette[color],c->buttons[i].tint,a);
            c->lut[i*3+2][color]=blend(c->palette[color],0xffff,edge);
            c->lut[i*3+3][color]=blend(c->palette[color],0xffff,label);
        }
    }
    c->cached_alpha=c->alpha;c->cached_pressed=pressed;
}
void nes_controls_cycle(nes_controls_t *c) {
    c->alpha=c->alpha==3?2:(c->alpha==2?5:(c->alpha==5?0:3));
}
int nes_controls_visible(const nes_controls_t *c) {return c->alpha && !c->controller_hidden;}
int nes_controls_use_controller(nes_controls_t *c) {
    int changed=!c->controller_hidden;c->controller_hidden=1;return changed;
}
int nes_controls_use_touch(nes_controls_t *c) {
    int changed=c->controller_hidden;c->controller_hidden=0;return changed;
}
int nes_controls_hit(const nes_controls_t *c,int x,int y) {
    unsigned scale=nes_display_ui_scale(c->width,c->height);
    if(!nes_controls_visible(c) || x<(int)c->safe.x || x>=(int)(c->safe.x+c->safe.width) ||
       y<(int)c->safe.y || y>=(int)(c->safe.y+c->safe.height)-24*(int)scale)return 0;
    /* One thumb can press a diagonal. The centre is a neutral dead zone. */
    int left=c->buttons[0].x,right=c->buttons[1].x+c->buttons[1].w;
    int top=c->buttons[2].y,bottom=c->buttons[3].y+c->buttons[3].h;
    if(x>=left && x<right && y>=top && y<bottom) {
        int cx=c->buttons[2].x+c->buttons[2].w/2,cy=c->buttons[0].y+c->buttons[0].h/2;
        int dx=c->safe.width*15/296,dy=12*scale,bit=0;
        if(x<cx-dx)bit|=64;else if(x>cx+dx)bit|=128;
        if(y<cy-dy)bit|=16;else if(y>cy+dy)bit|=32;
        return bit;
    }
    for(unsigned i=0;i<NES_CONTROLS_COUNT;++i) {
        const nes_control_button_t *b=&c->buttons[i];
        if(x>=b->x && x<b->x+b->w && y>=b->y && y<b->y+b->h)return b->bit?b->bit:-1;
    }
    return 0;
}
