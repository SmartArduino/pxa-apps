// SPDX-License-Identifier: MIT
#include "model.hpp"
#include "../common/motion-games/runtime.hpp"
#include "../common/motion-games/sound.hpp"
#include <bit>
#include "artwork.hpp"
namespace fortune {
using namespace motion;
enum class Screen {draw,result,menu,history,favorites};
enum Action {draw_action=1,theme_action,menu_action,resume_action,history_action,favorites_action,shake_action,exit_action,page_action,favorite_action,back_action,next_action,previous_action,sound_action,entry_base=100};
constexpr uint16_t vermilion=rgb(180,65,58),ochre=rgb(171,121,57),blush=rgb(251,230,215);
struct App:Runtime<App> {
 Sounds sounds;
 Profile profile;Screen screen=Screen::draw,origin=Screen::draw;unsigned shown=0,pending_sign=0,page=0,browse_page=0;
 bool drawing=false,saving=false,save_pending=false;float remaining=0,sparkle=0,sound_at=0;unsigned shake_sound=0;
 static const char* id(){return "pxa-fortune-sticks";}static const char* ready_message(){return "Fortune ready";}
 pxa::Task<void> load(){std::array<std::byte,Profile::size> data{};auto n=co_await ctx->storage().get("fortune-v1",data);if(n&&*n==data.size())profile.decode(data);co_return pxa::Result<void>{};}
 bool initial_motion()const{return profile.shake;}bool initial_gyro()const{return true;}
 void started(){shown=profile.latest();screen=profile.history_size?Screen::result:Screen::draw;controls.focus=draw_action;request_motion(profile.shake,true);}
 void save(){save_pending=true;if(!saving){saving=true;if(!ctx->tasks().start(persist()))saving=false;}}
 pxa::Task<void> persist(){while(save_pending){save_pending=false;auto data=profile.encode();auto status=co_await ctx->storage().set("fortune-v1",data);if(!status){log("Fortune save failed");break;}}saving=false;co_return pxa::Result<void>{};}
 bool can_shake()const{return screen==Screen::draw||screen==Screen::result;}
 void show(Screen next){screen=next;controls.focus=next==Screen::menu?resume_action:next==Screen::history||next==Screen::favorites?entry_base:draw_action;
   request_motion(can_shake()&&profile.shake,true);dirty=true;}
 void begin(){if(drawing||!can_shake())return;pending_sign=profile.choose(random);drawing=true;remaining=1.8f;sound_at=.19f;shake_sound=0;sounds.play(Cue::bamboo);screen=Screen::draw;controls.focus=draw_action;sparkle=0;dirty=true;log("Fortune draw begin");}
 void settle(){if(!drawing)return;sounds.play(Cue::settle);drawing=false;remaining=0;shown=pending_sign;profile.record(shown);page=0;origin=Screen::draw;screen=Screen::result;controls.focus=draw_action;sparkle=.7f;save();dirty=true;
   char text[100];std::snprintf(text,sizeof text,"Fortune complete n=%u sign=%u theme=%u",profile.total,shown+1,signs[shown].theme);log(text);}
 unsigned capacity()const{return h<220?2:3;}
 unsigned entries(std::array<unsigned,24>& list)const{unsigned count=0;if(screen==Screen::history){for(unsigned i=0;i<profile.history_size;++i)list[count++]=profile.history[i];}
   else for(unsigned i=0;i<24;++i)if(profile.favorite(i))list[count++]=i;return count;}
 void browse(Screen next){browse_page=0;show(next);}
 void activate(int action){if(action!=sound_action)sounds.play(Cue::click);if(drawing)return;
   if(action>=entry_base&&action<entry_base+24){std::array<unsigned,24> list{};unsigned count=entries(list),index=browse_page*capacity()+unsigned(action-entry_base);if(index<count){shown=list[index];origin=screen;page=0;show(Screen::result);}return;}
   switch(action){
    case draw_action:origin=Screen::draw;show(Screen::draw);begin();break;
    case theme_action:profile.theme=(profile.theme+1)%7;save();dirty=true;break;
    case menu_action:show(Screen::menu);break;
    case resume_action:show(Screen::draw);break;
    case history_action:browse(Screen::history);break;
    case favorites_action:browse(Screen::favorites);break;
    case shake_action:profile.shake=!profile.shake;save();dirty=true;break;
    case page_action:sounds.play(Cue::paper);page=(page+1)%3;dirty=true;break;
    case favorite_action:profile.toggle(shown);sounds.play(Cue::favorite);save();dirty=true;break;
    case back_action:if(back()==pxa::BackAction::close)request_exit();break;
    case next_action:{std::array<unsigned,24> list{};if((browse_page+1)*capacity()<entries(list))++browse_page;dirty=true;break;}
    case previous_action:if(browse_page)--browse_page;dirty=true;break;
    case sound_action:sounds.toggle();dirty=true;break;
    case exit_action:request_exit();break;
   }
 }
 void key(uint32_t keys){using namespace pxa::input;if(keys&controller_b){if(back()==pxa::BackAction::close)request_exit();return;}if(drawing)return;
   if(keys&controller_up){controls.move(-1);dirty=true;}if(keys&controller_down){controls.move(1);dirty=true;}if(keys&controller_a)activate(controls.focus);}
 Rect illustration()const{return {12,h<220?42:59,w-24,h-(h<220?84:111)};}
 void pointer(pxa::input::PointerPhase phase,int x,int y){if(phase==pxa::input::PointerPhase::down)controls.down(x,y);
   else if(phase==pxa::input::PointerPhase::up||phase==pxa::input::PointerPhase::cancel){int action=controls.up(x,y,phase==pxa::input::PointerPhase::cancel);if(action>=0)activate(action);
    else if(phase==pxa::input::PointerPhase::up&&screen==Screen::draw&&illustration().contains(x,y)&&std::abs(x-controls.down_x)<12&&std::abs(y-controls.down_y)<12)begin();}}
 pxa::BackAction back(){if(drawing)settle();
   if(screen==Screen::result){Screen next=origin;if(next!=Screen::history&&next!=Screen::favorites)next=Screen::draw;show(next);if(next==Screen::history||next==Screen::favorites){std::array<unsigned,24> list{};auto n=entries(list);browse_page=std::min(browse_page,n?(n-1)/capacity():0u);}return pxa::BackAction::stay;}
   if(screen==Screen::draw){show(Screen::menu);return pxa::BackAction::stay;}if(screen==Screen::menu)return pxa::BackAction::close;show(Screen::menu);return pxa::BackAction::stay;}
 void motion_changed(bool shaken,bool status){if(shaken&&profile.shake)begin();dirty|=status;}
 void tick(float dt){if(drawing){sound_at-=dt;if(sound_at<=0){sounds.play(Cue::bamboo,++shake_sound);sound_at=.18f+shake_sound*.025f;}remaining=std::max(0.f,remaining-dt);dirty=true;if(!remaining)settle();}if(sparkle>0){sparkle=std::max(0.f,sparkle-dt);dirty=true;}}
 void background(){if(drawing)settle();}
 void foreground(){request_motion(can_shake()&&profile.shake,true);}
 void toolbar(std::initializer_list<std::pair<int,std::string_view>> actions,bool enabled=true){int height=h<220?27:33,y=h-height-8,width=(w-20-6*(int(actions.size())-1))/int(actions.size()),x=10;
   for(auto a:actions){controls.add({x,y,width,height},a.first,a.second,a.first==draw_action?pink:a.first==page_action?lilac:mint,enabled);x+=width+6;}}
 // Fixed-width glyphs are wrapped on UTF-8 boundaries. Every line gets a page;
 // the interpretation/action page never relies on touch-only scrolling.
 void paragraph(Paint& p,Rect r,std::string_view text,uint16_t color,int px){int line_h=Paint::text_height(px)+3,y=r.y;while(!text.empty()&&y+line_h<=r.y+r.h+3){std::string_view rest=text;size_t bytes=0;int width=0;
    while(!rest.empty()){auto old=rest.size();auto code=Paint::next(rest);int advance=(font_advance[Paint::glyph(code)]*px+8)/16;if(bytes&&width+advance>r.w)break;width+=advance;bytes+=old-rest.size();}
    if(!bytes)break;p.text(r.x,y,text.substr(0,bytes),color,0,px);text.remove_prefix(bytes);y+=line_h;}if(!text.empty())log("Fortune text overflow");}
 int fitted_text(std::string_view text,int width,int height,int preferred)const{for(int px=preferred;px>=8;--px){int lines=1,used=0;auto rest=text;while(!rest.empty()){auto code=Paint::next(rest);int advance=(font_advance[Paint::glyph(code)]*px+8)/16;if(used&&used+advance>width){++lines;used=0;}used+=advance;}if(lines*(Paint::text_height(px)+3)<=height+3)return px;}return 8;}
 void cup(Paint& p,Rect a){draw_sign_cup(p,a,drawing,drawing?1-remaining/1.8f:0,gyro_energy);}
 void draw(Paint& p){bool small=h<220;int heading=small?25:32;char text[128];
   if(screen==Screen::draw){p.label({12,3,w-24,heading},"好运签",vermilion,0,small?12:16);p.label({w-70,4,58,heading},!profile.shake?"点击求签":acceleration?"轻摇签筒":"按键求签",ochre,2,10);
     p.label({12,heading-1,w-24,small?20:25},drawing?"好运正在路上…":"摇出一份今天的小惊喜",muted,1,small?10:13);cup(p,illustration());
     toolbar({{draw_action,"求一签"},{theme_action,theme_names[profile.theme]},{menu_action,"更多"}},!drawing);
   }else if(screen==Screen::result){auto &sign=signs[shown];std::snprintf(text,sizeof text,"第 %02u 签 · %s",shown+1,page==1?"签意":page==2?"小行动":theme_names[sign.theme]);p.label({12,3,w-69,heading},text,ochre,0,small?10:13);
     Rect card{10,heading+3,w-20,h-heading-(small?46:53)};p.round({card.x,card.y+2,card.w,card.h},rgb(229,208,179),10);p.round(card,blush,10);p.line({float(card.x+12),float(card.y+3)},{float(card.x+32),float(card.y+3)},1,rgb(223,177,108));p.line({float(card.x+card.w-32),float(card.y+3)},{float(card.x+card.w-12),float(card.y+3)},1,rgb(223,177,108));int title_y=card.y+(small?2:9);p.label({card.x+8,title_y,card.w-16,small?25:34},sign.title,vermilion,1,small?16:20);
     int top=title_y+(small?26:41),bottom=card.y+card.h-6;if(!page&&h>=360){
       int available=bottom-top,verse_h=50,meaning_h=(available-verse_h-24)*3/5,action_top=top+verse_h+meaning_h+24;
       p.label({card.x+8,top,card.w-16,25},sign.verse_a,ink,1,16);p.label({card.x+8,top+25,card.w-16,25},sign.verse_b,ink,1,16);
       paragraph(p,{card.x+10,top+verse_h,card.w-20,meaning_h},sign.meaning,ink,fitted_text(sign.meaning,card.w-20,meaning_h,16));
       p.label({card.x+10,action_top-23,card.w-20,20},"今日小行动",vermilion,0,13);paragraph(p,{card.x+10,action_top,card.w-20,bottom-action_top},sign.action,teal,fitted_text(sign.action,card.w-20,bottom-action_top,16));
     }else if(!page){int gap=std::max(22,(bottom-top)/3);p.label({card.x+8,top,card.w-16,gap},sign.verse_a,ink,1,small?12:16);p.label({card.x+8,top+gap,card.w-16,gap},sign.verse_b,ink,1,small?12:16);
       p.label({card.x+8,bottom-(small?17:23),card.w-16,small?17:23},"解签里有一件小行动",ochre,1,small?9:12);
     }else{auto content=page==1?sign.meaning:sign.action;int px=fitted_text(content,card.w-20,bottom-top,small?11:h>=360?32:16);paragraph(p,{card.x+10,top,card.w-20,bottom-top},content,page==1?ink:teal,px);}
     toolbar({{draw_action,"再求一签"},{page_action,page==0?"解签":page==1?"小行动":"看签文"},{favorite_action,profile.favorite(shown)?"已收藏":"收藏"}});controls.add({w-53,5,43,small?21:26},back_action,"返回",mint);
     if(sparkle>0)for(int i=0;i<5;++i)p.star(15+(w-30)*i/4,heading+3,2+int(sparkle*3),gold);
   }else if(screen==Screen::menu){p.label({12,3,w-24,heading},"签筒小天地",vermilion,0,small?12:16);int top=small?31:46,step=(h-top-9)/6;
     const char* labels[]={"回到签筒","最近求过的签","我的收藏",profile.shake?"摇动：已开启":"摇动：已关闭",sounds.label(),"退出游戏"};int ids[]={resume_action,history_action,favorites_action,shake_action,sound_action,exit_action};
     for(int i=0;i<6;++i)controls.add({12,top+i*step,w-24,step-5},ids[i],labels[i],i==0?pink:i%2?mint:lilac);
   }else{bool history=screen==Screen::history;p.label({12,3,w-24,heading},history?"最近求过的签":"我的收藏",vermilion,0,small?12:16);std::array<unsigned,24> list{};unsigned count=entries(list),cap=capacity();
     browse_page=std::min(browse_page,count?(count-1)/cap:0u);std::snprintf(text,sizeof text,"共 %u 支 · %u / %u 页",count,browse_page+1,std::max(1u,(count+cap-1)/cap));p.label({12,heading-2,w-24,23},text,muted,0,small?10:12);
     int top=small?49:65,step=(h-top-46)/int(cap);if(!count)paragraph(p,{18,top+8,w-36,h-top-60},"喜欢的签，可以收藏在这里。",ochre,small?11:14);
     for(unsigned i=0;i<cap&&browse_page*cap+i<count;++i){unsigned sign=list[browse_page*cap+i];std::snprintf(text,sizeof text,"%02u · %s",sign+1,signs[sign].title);controls.add({12,top+int(i)*step,w-24,step-7},entry_base+int(i),text,i&1?mint:pink);}
     toolbar({{next_action,"下一页"},{previous_action,"上一页"},{back_action,"返回"}});for(unsigned i=0;i<controls.count;++i){if(controls.items[i].id==next_action)controls.items[i].enabled=(browse_page+1)*cap<count;if(controls.items[i].id==previous_action)controls.items[i].enabled=browse_page>0;}
   }
 }
};
}
PXA_APPLICATION(fortune::App)
