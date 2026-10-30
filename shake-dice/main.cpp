// SPDX-License-Identifier: MIT
#include "model.hpp"
#include "cube.hpp"
#include "../common/motion-games/runtime.hpp"
#include "../common/motion-games/sound.hpp"
namespace dice {
using namespace motion;
enum class Screen {play,menu,history};
enum Action {roll_action=1,count_action,menu_action,resume_action,history_action,shake_action,exit_action,next_action,previous_action,sound_action};
struct App:Runtime<App> {
 Sounds sounds;
 Model model;Screen screen=Screen::play;unsigned history_page=0;
 bool saving=false,save_pending=false;float remaining=0,duration=1.2f,sparkle=0,sound_at=0;unsigned shake_sound=0;
 static const char* id(){return "pxa-shake-dice";}static const char* ready_message(){return "Dice ready";}
 pxa::Task<void> load(){std::array<std::byte,Model::profile_size> data{};auto n=co_await ctx->storage().get("dice-v1",data);if(n&&*n==data.size())model.decode(data);co_return pxa::Result<void>{};}
 bool initial_motion()const{return model.shake;}bool initial_gyro()const{return true;}
 void started(){controls.focus=roll_action;request_motion(model.shake,true);}
 void save(){save_pending=true;if(!saving){saving=true;if(!ctx->tasks().start(persist()))saving=false;}}
 pxa::Task<void> persist(){while(save_pending){save_pending=false;auto data=model.encode();auto result=co_await ctx->storage().set("dice-v1",data);if(!result){log("Dice save failed");break;}}saving=false;co_return pxa::Result<void>{};}
 void show(Screen s){request_motion(s==Screen::play&&model.shake,true);screen=s;controls.focus=s==Screen::play?roll_action:s==Screen::menu?resume_action:next_action;dirty=true;}
 void begin(){if(screen!=Screen::play||!model.begin(random))return;duration=1.15f+.045f*model.selected;remaining=duration;sparkle=0;sound_at=.13f;shake_sound=0;sounds.play(Cue::dice);dirty=true;
   char line[96];std::snprintf(line,sizeof line,"Dice begin n=%u count=%u",model.total_rolls+1,model.rolling.count);log(line);}
 void settle(){if(!model.settle())return;sounds.play(Cue::settle);remaining=0;sparkle=.55f;dirty=true;save();char line[100];std::snprintf(line,sizeof line,"Dice complete n=%u count=%u total=%u history=%u",model.total_rolls,model.result().count,model.result().total(),model.history_size);log(line);}
 void activate(int action){if(action!=sound_action)sounds.play(Cue::click);switch(action){
  case roll_action:begin();break;
  case count_action:if(!model.pending){model.count(model.selected%6+1);save();dirty=true;}break;
  case menu_action:if(!model.pending)show(Screen::menu);break;
  case resume_action:show(Screen::play);request_motion(model.shake,true);break;
  case history_action:history_page=0;show(Screen::history);break;
  case shake_action:model.shake=!model.shake;request_motion(screen==Screen::play&&model.shake,true);save();dirty=true;break;
  case sound_action:sounds.toggle();dirty=true;break;
    case exit_action:request_exit();break;
  case next_action:if((history_page+1)*history_capacity()<model.history_size){++history_page;dirty=true;}break;
  case previous_action:if(history_page){--history_page;dirty=true;}break;
 }}
 void key(uint32_t keys){using namespace pxa::input;if(keys&controller_b){(void)back();return;}if(keys&controller_up){controls.move(-1);dirty=true;}if(keys&controller_down){controls.move(1);dirty=true;}if(keys&controller_a)activate(controls.focus);}
 void pointer(pxa::input::PointerPhase phase,int x,int y){if(phase==pxa::input::PointerPhase::down)controls.down(x,y);
  else if(phase==pxa::input::PointerPhase::up||phase==pxa::input::PointerPhase::cancel){int action=controls.up(x,y,phase==pxa::input::PointerPhase::cancel);if(action>=0)activate(action);
    else if(phase==pxa::input::PointerPhase::up&&screen==Screen::play&&tray().contains(x,y)&&std::abs(x-controls.down_x)<12&&std::abs(y-controls.down_y)<12)begin();}}
 pxa::BackAction back(){if(screen==Screen::play){if(model.pending)settle();show(Screen::menu);}else if(screen==Screen::history)show(Screen::menu);else return pxa::BackAction::close;return pxa::BackAction::stay;}
 void motion_changed(bool shake,bool restored){if(shake&&model.shake)begin();if(restored)dirty=true;}
 void tick(float dt){if(remaining>0){sound_at-=dt;if(sound_at<=0){sounds.play(Cue::dice,++shake_sound);sound_at=.13f+shake_sound*.022f;}remaining=std::max(0.f,remaining-dt);dirty=true;if(!remaining)settle();}if(sparkle>0){sparkle=std::max(0.f,sparkle-dt);dirty=true;}}
 void background(){if(model.pending)settle();}
 void foreground(){request_motion(screen==Screen::play&&model.shake,true);}
 Rect tray()const{int top=h<220?49:65,bottom=h-(h<220?42:50);return {10,top,w-20,std::max(35,bottom-top)};}
 unsigned history_capacity()const{return h<200?1:h<260?2:3;}
 void draw(Paint& p){
  const bool small=h<220;int heading=small?23:31;char text[120];
  p.label({12,4,w-24,heading},screen==Screen::history?"好运记录":"摇摇骰子",ink,0,small?12:16);
  if(screen==Screen::play){
   p.label({w-79,4,67,heading},!model.shake?"摇动关闭":acceleration?"轻摇":connecting?"连接中":"按键触摸",teal,2,10);
   if(model.pending)p.label({12,heading+1,w-24,small?23:30},"好运正在翻滚…",coral);
   else if(model.valid){std::snprintf(text,sizeof text,"合计 %u 点",model.result().total());p.label({12,heading+1,w-24,small?23:30},text,teal,1,small?12:16);}
   else {std::snprintf(text,sizeof text,"%u颗骰子，准备好啦",model.selected);p.label({12,heading+1,w-24,small?23:30},text,muted);}
   auto area=tray();p.round(area,rgb(251,235,216),12);
   unsigned count=model.pending?model.rolling.count:model.valid?model.result().count:model.selected;
   unsigned cols=count==2&&h>w*3/2?1:count<=2?count:count<=4?2:h>w*3/2?2:3;if(w>h&&count>=3)cols=3;unsigned rows=(count+cols-1)/cols;
   int cw=(area.w-8)/int(cols),ch=(area.h-12)/int(rows);
   for(unsigned i=0;i<count;++i){unsigned row=i/cols,column=i%cols,n=std::min(cols,count-row*cols);int left=area.x+(area.w-int(n)*cw)/2;
     Rect box{left+int(column)*cw+3,area.y+4+int(row)*ch,cw-6,ch-3};unsigned value=model.pending?model.rolling.values[i]:model.valid?model.result().values[i]:(i%6+1);
     cube(p,box,value,model.pending?remaining/duration:0,i,gyro_energy);}
   if(sparkle>0){for(unsigned i=0;i<10;++i){float angle=i*.6283185f;int x=area.x+area.w/2+int(std::cos(angle)*(area.w*.38f)),y=area.y+area.h/2+int(std::sin(angle)*(area.h*.4f));p.star(x,y,2+int(sparkle*4),i&1?gold:coral);}}
   int y=h-(small?33:41),height=small?26:32,width=(w-32)/3;
   controls.add({10,y,width,height},roll_action,"摇一摇",pink,!model.pending);
   std::snprintf(text,sizeof text,"%u颗",model.selected);controls.add({16+width,y,width,height},count_action,text,lilac,!model.pending);
   controls.add({22+2*width,y,width,height},menu_action,"更多",mint,!model.pending);
  }else if(screen==Screen::menu){
   p.label({12,heading+4,w-24,24},"一摇一份小惊喜",muted);
   const char* labels[]={"继续摇一摇","查看历史记录",model.shake?"摇动：已开启":"摇动：已关闭",sounds.label(),"退出游戏"};int ids[]={resume_action,history_action,shake_action,sound_action,exit_action};
   int top=small?48:65,step=(h-top-12)/5;for(int i=0;i<5;++i)controls.add({12,top+i*step,w-24,step-6},ids[i],labels[i],i==0?pink:i==2?lilac:mint);
  }else {
   std::snprintf(text,sizeof text,"最近 %u 次 · 共 %u 次",model.history_size,model.total_rolls);p.label({12,heading,w-24,24},text,muted);
   unsigned capacity=history_capacity(),begin=history_page*capacity;int top=small?49:62,step=(h-top-48)/int(capacity);
   if(!model.history_size){p.label({14,top,w-28,h-top-55},"摇一次，留下第一份好运",teal);}
   for(unsigned i=0;i<capacity&&begin+i<model.history_size;++i){auto &r=model.history[begin+i];Rect card{12,top+int(i)*step,w-24,step-7};p.round(card,i?mint:pink);
    char details[96];int used=0;for(unsigned j=0;j<r.count;++j)used+=std::snprintf(details+used,sizeof(details)-used,j?" + %u":"%u",r.values[j]);std::snprintf(details+used,sizeof(details)-used," = %u",r.total());
    std::snprintf(text,sizeof text,"第 %u 次 · %u颗",model.total_rolls-begin-i,r.count);p.label({card.x+8,card.y,card.w-16,std::max(18,card.h/2)},text,muted,0,12);
    p.label({card.x+8,card.y+card.h/2-2,card.w-16,card.h/2+2},details,ink);}
   int y=h-37,width=(w-32)/3;controls.add({10,y,width,29},next_action,"下一页",mint,(history_page+1)*capacity<model.history_size);
   controls.add({16+width,y,width,29},previous_action,"上一页",lilac,history_page>0);controls.add({22+2*width,y,width,29},resume_action,"返回",pink);
  }
 }
};
}
PXA_APPLICATION(dice::App)
