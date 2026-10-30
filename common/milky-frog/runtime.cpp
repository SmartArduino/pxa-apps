// SPDX-License-Identifier: MIT
#ifndef PXA_POCKET_SYSTEM_TEXT
#define PXA_POCKET_SYSTEM_TEXT 1
#endif
#include "media.hpp"
#define PXA_POCKET_MEDIA 1
#include "../pocket-game/runtime.cpp"
namespace milky {
void icon(Icon kind,Rect box,uint16_t color){system_icon_add(kind,box,color);}
void image(const Sprite &s,int x,int y,int width,int height){
    if(!canvas||!s.pixels||!s.width||!s.height)return;
    if(!width)width=s.width;if(!height)height=s.height;
    if(width<=0||height<=0)return;
    if(native_image_add(s,x,y,width,height))return;
    const int left=x<0?0:x,top=y<0?0:y,right=x+width>pocket::WIDTH?pocket::WIDTH:x+width,bottom=y+height>pocket::HEIGHT?pocket::HEIGHT:y+height;
    for(int py=top;py<bottom;++py)for(int px=left;px<right;++px){
        const uint32_t p=s.pixels[(py-y)*s.height/height*s.width+(px-x)*s.width/width];
        const unsigned a=p>>16;
        if(!a)continue;
        uint16_t &out=canvas[py*pocket::WIDTH+px];
        if(a==15){out=uint16_t(p);continue;}
        const unsigned inv=15-a;
        out=uint16_t((((((p>>11)&31)*a+((out>>11)&31)*inv)/15)<<11)|
            (((((p>>5)&63)*a+((out>>5)&63)*inv)/15)<<5)|(((p&31)*a+(out&31)*inv)/15));
    }
}
void rounded(int x,int y,int w,int h,uint16_t color,int radius){
    if(radius<0)radius=0;if(radius>w/2)radius=w/2;if(radius>h/2)radius=h/2;
    pocket::rect(x+radius,y,w-2*radius,h,color);pocket::rect(x,y+radius,w,h-2*radius,color);
    for(int dy=0;dy<radius;++dy)for(int dx=0;dx<radius;++dx){unsigned coverage=0;
        for(int sy=1;sy<=3;sy+=2)for(int sx=1;sx<=3;sx+=2){int xx=radius*4-dx*4-sx,yy=radius*4-dy*4-sy;coverage+=xx*xx+yy*yy<=radius*radius*16;}
        if(!coverage)continue;
        const int xs[]={x+dx,x+w-1-dx},ys[]={y+dy,y+h-1-dy};
        for(int py:ys)for(int px:xs){if(!canvas||px<0||py<0||px>=pocket::WIDTH||py>=pocket::HEIGHT)continue;uint16_t &out=canvas[py*pocket::WIDTH+px];unsigned inv=4-coverage;
            out=uint16_t((((((color>>11)&31)*coverage+((out>>11)&31)*inv)/4)<<11)|(((((color>>5)&63)*coverage+((out>>5)&63)*inv)/4)<<5)|(((color&31)*coverage+(out&31)*inv)/4));}
    }
}
void center(int x,int y,int w,const char *value,uint16_t color,int scale){system_text_add(x,y,w,value,color,scale,1);}
void button(int x,int y,int w,int h,const char *value,bool selected){rounded(x,y,w,h,selected?HONEY:SOFT);center(x,y+(h-10)/2,w,value,BROWN);}
bool inside(int x,int y,int l,int t,int w,int h){return x>=l&&x<l+w&&y>=t&&y<t+h;}
bool held(pocket::Key key){return !paused&&!background&&!exit_requested&&(buttons&key_masks[unsigned(key)]);}
void sounds(const char *const *paths,unsigned count){recording_paths=paths;recording_count=count;prewarm_index=0;}
void play(unsigned id,unsigned track,bool loop){
    if(track>=3||id>=recording_count||!effects_gain||paused||background)return;
    if(recording_duration[track]&&recording_track_id[track]!=id){
        if(sound_handle)pxa_audio_control_sound(sound_handle,uint8_t(track),3,0);
        recording_duration[track]=0;
    }
    recording_stop_at[track]=0;
    requested_sounds[track]={id,true,loop,sound_now()+SOUND_RATE/2};
}
void stop(unsigned track){if(track<3){requested_sounds[track]={};recording_stop_at[track]=0;recording_duration[track]=0;if(sound_handle)pxa_audio_control_sound(sound_handle,uint8_t(track),3,0);}}
void finish(unsigned track){
    if(track>=3)return;
    requested_sounds[track].pending=false;
    if(!recording_duration[track])return;
    const uint64_t elapsed=sound_now()-recording_started[track],length=recording_duration[track];
    recording_stop_at[track]=recording_started[track]+(elapsed/length+1)*length;
}
void volume(unsigned effects,unsigned music){
    effects_gain=effects>100?100:effects;music_gain=music>100?100:music;
    if(!effects_gain){for(unsigned i=0;i<3;++i)stop(i);}
    else if(sound_handle)for(unsigned i=0;i<3;++i)pxa_audio_control_sound(sound_handle,uint8_t(i),4,recording_gain(effects_gain));
    if(sound_handle)pxa_audio_control_music(sound_handle,music_gain?4:3,music_gain?recording_gain(music_gain):0);
    if(!music_gain)music_started=false;
}
unsigned effects_volume(){return effects_gain;}
unsigned music_volume(){return music_gain;}
void audio_test(){play(0);}
void configure_screen(int width,int height,uint32_t density,const uint32_t *insets,unsigned shape,const uint32_t *corners){
    int native_insets[4]{},radius=0;
    for(int i=0;i<4;++i){native_insets[i]=insets?int(uint64_t(insets[i])*(density?density:65536)/65536):0;
        if(corners)radius=std::max(radius,int(uint64_t(corners[i])*(density?density:65536)/65536));}
    screen=fit_screen(width,height,native_insets,radius,shape==2);
    pocket::WIDTH=screen.width;pocket::HEIGHT=screen.height;
}
void panel(Rect r,uint16_t color,bool shadow){
    if(shadow)rounded(r.x,r.y+2,r.w,r.h,BORDER,std::min(10,r.h/3));
    rounded(r.x,r.y,r.w,r.h,color?color:PAPER,std::min(10,r.h/3));
}
void label(Rect r,const char *value,uint16_t color,int scale,unsigned align){
    if(r.w<=0||r.h<=0||!value)return;
    const char *p=value;int units=0;while(*p){auto cp=pocket_ui::next_utf8(p);units+=cp<128?6:10;}
    int font=text_height(scale)-2;if(units)font=std::max(8,std::min(font,(r.w-2)*10/units));
    const int y=r.y+(r.h-font-2)/2;
    const auto before=system_text_count;
    system_text_add(r.x,y,r.w,value,color,scale,align);
    if(system_text_count>before){auto &item=system_texts[system_text_count-1];item.font_size=font;item.box_y=r.y;item.box_h=r.h;}
}
void portrait(const Sprite &s,Rect r){
    int w=r.w,h=w*s.height/s.width;
    if(h>r.h){h=r.h;w=h*s.width/s.height;}
    image(s,r.x+(r.w-w)/2,r.y+(r.h-h)/2,w,h);
}
void thumbnail(const Sprite &s,Rect r){
    // A whole unlocked gallery must fit beside the original voice recording.
    // Use one consistent tier rather than giving early cells all the memory.
    Sprite thumb=s;thumb.large=thumb.small=thumb.medium;portrait(thumb,r);
}
void header(const char *title,bool back){
    auto b=bounds();const int g=gap(),h=touch_height();int x=b.x+g;
    if(back){auto r=back_button();
        if(pointer_active&&!pointer_consumed&&!paused&&r.contains(last_x,last_y))panel(r,SOFT,false);
        icon(Icon::Back,r,BROWN);x=r.right()+g;}
    label({x,b.y+g/2,menu_button().x-g-x,h},title,BROWN,2);
}
void metric(Rect r,const char *caption,uint32_t value,bool accent){
    panel(r,accent?SOFT:PAPER,false);char number[16];snprintf(number,sizeof(number),"%lu",(unsigned long)value);
    if(r.h<40){label({r.x+6,r.y,r.w/2-6,r.h},caption,SAGE);label({r.x+r.w/2,r.y,r.w/2-6,r.h},number,BROWN,1,2);}
    else{label({r.x+8,r.y+2,r.w-16,text_height()},caption,SAGE,1,1);
        int scale=int(strlen(number))*12>r.w-10?1:2;label({r.x+5,r.y+text_height(),r.w-10,r.h-text_height()-2},number,BROWN,scale,1);}
}
void action(Rect r,const char *caption,bool primary,Icon symbol){
    const bool pressing=pointer_active&&!pointer_consumed&&!paused&&r.contains(last_x,last_y);
    panel(r,pressing?SOFT:primary?HONEY:PAPER,!pressing);
    if(symbol==Icon::None){label({r.x+3,r.y,r.w-6,r.h},caption,BROWN,1,1);return;}
    const int side=std::clamp(r.h/2,14,20),spacing=5;
    const int text=std::min(r.w-side-spacing-12,(pocket_ui::text_width(caption)*(text_height()-2)+9)/10+2);
    const int left=r.x+(r.w-side-spacing-text)/2;
    const bool trailing=symbol==Icon::Next;
    icon(symbol,{trailing?left+text+spacing:left,r.y,side,r.h},BROWN);
    label({trailing?left:left+side+spacing,r.y,text,r.h},caption,BROWN,1,1);
}
void settings_draw(){
    pocket::clear(CREAM);header(pocket::tr("Sound","声音设置"),true);auto l=settings_layout();
    const auto toggle=[](Rect r,const char *caption,bool on){panel(r);int pill=std::clamp(r.w/5,34,52);
        label({r.x+gap(),r.y,r.w-pill-3*gap(),r.h},caption,BROWN);
        Rect p{r.right()-gap()-pill,r.y+(r.h-24)/2,pill,24};rounded(p.x,p.y,p.w,p.h,on?SAGE:SOFT,12);
        label(p,on?pocket::tr("ON","开"):pocket::tr("OFF","关"),on?PAPER:BROWN,1,1);};
    toggle(l.effects,pocket::tr("Laughs & effects","笑声与音效"),effects_volume());
    toggle(l.music,pocket::tr("Background music","背景音乐"),music_volume());
    action(l.preview,pocket::tr("Play original laugh","试听原版笑声"),effects_volume());action(l.back,pocket::tr("Back","返回"));
}
int settings_touch(int x,int y){auto l=settings_layout();
    if(l.effects.contains(x,y)){volume(effects_volume()?0:100,music_volume());if(effects_volume())audio_test();return 1;}
    if(l.music.contains(x,y)){volume(effects_volume(),music_volume()?0:20);return 1;}
    if(l.preview.contains(x,y)){audio_test();return 1;}
    if(l.back.contains(x,y)||back_button().contains(x,y))return 2;
    return 0;
}
void menu_draw(bool is_paused,int selected,bool saving){
    auto button=menu_button();panel(button,PAPER,false);
    const auto pause_icon=[&](){icon(Icon::Pause,button,SAGE);};
    if(!is_paused){pause_icon();return;}
    auto p=pause_panel();panel(p,CREAM);label({p.x+gap(),p.y+gap(),p.w-2*gap(),text_height(2)},saving?pocket::tr("Saving…","保存中…"):pocket::tr("Take a break","歇一会儿"),BROWN,2,1);
    const char *labels[]={pocket::tr("Continue","继续游戏"),pocket::tr("Restart","重新开始"),pocket::tr("Exit","退出游戏"),pocket::tr("Language: English","语言：中文")};
    for(unsigned i=0;i<4;++i)action(pause_row(i),labels[i],selected==int(i));
    // The pause toggle remains available above the modal on compact screens.
    pause_icon();
}
}
