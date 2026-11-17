// SPDX-License-Identifier: MIT
// Shared layout also supplies pointer hit targets in main.cpp.
drawing_ui=overlay&&display;
if(!drawing_ui)return; // Menus/HUD are always native, including mount transitions.
const float ui=ui_scale(),cx=width*.5f;
const UiLayout layout(width,height,ui,safe_x,safe_y);
auto card=[&](UiRect b,uint32_t fill=INK){panel(f,b.x,b.y,b.w,b.h,fill,0x9f885b);};
auto button=[&](UiRect b,const char* label,bool primary=false){card(b,primary?0xdab66e:INK);text_box(f,label,{b.x+10*ui,b.y,b.w-20*ui,b.h},primary?18:14,primary?INK:IVORY,1,primary);};
auto veil=[&]{if(drawing_ui){auto b=pxa::ui::surface_to_canvas_region({0,0,int(display->width),int(display->height)},*display);overlay->rect(b.x,b.y,b.width,b.height,0x061d1870);}};
char num[96];
if(s.mode==Mode::Title){
 if(title_background&&drawing_ui){auto b=pxa::ui::surface_to_canvas_region({0,0,int(display->width),int(display->height)},*display);overlay->image(b,*title_background,pxa::ui::ImageFit::cover);}
 if(title_logo&&drawing_ui){
  const float rx=float(display->width)/width,ry=float(display->height)/height;
  auto b=pxa::ui::surface_to_canvas_region({int((cx-132*ui)*rx),int(height*.09f*ry),int(264*ui*rx),int(88*ui*ry)},*display);overlay->image(b,*title_logo,pxa::ui::ImageFit::contain);
 }else{text_box(f,"遗迹疾奔",{cx-132*ui,height*.08f,264*ui,54*ui},28,GOLD,1,true);text_box(f,"R E L I C   R U N",{cx-100*ui,height*.08f+49*ui,200*ui,22*ui},12,IVORY);}
 card(layout.record);
 text_box(f,"最佳纪录",{layout.record.x+14*ui,layout.record.y,80*ui,layout.record.h},12,JADE,0);
 snprintf(num,sizeof(num),"%06u",save.best);text_box(f,num,{layout.record.cx(),layout.record.y,layout.record.w*.5f-12*ui,layout.record.h},18,GOLD,2,true);
 button(layout.start,"开始探险",true);button(layout.help,"操作指南");button(layout.quality,quality?"画质 流畅":"画质 精美");
}else if(s.mode==Mode::Running){
 const float hx=std::max(8*ui,safe_x),hy=8*ui+safe_y;
 card({hx,hy,106*ui,44*ui});
 snprintf(num,sizeof(num),"%06u",s.score);text_box(f,num,{hx+10*ui,hy+3*ui,86*ui,23*ui},18,s.combo_t>0?GOLD:IVORY,0,true);
 snprintf(num,sizeof(num),"%um",unsigned(s.distance));text_box(f,num,{hx+10*ui,hy+27*ui,44*ui,14*ui},12,GOLD,0);
 // A small coin token makes this metadata row readable without a long label.
 rect(f,hx+59*ui,hy+31*ui,5*ui,6*ui,GOLD);rect(f,hx+61*ui,hy+32*ui,ui,4*ui,0x966e37);
 snprintf(num,sizeof(num),"%u",s.coins);text_box(f,num,{hx+67*ui,hy+27*ui,29*ui,14*ui},12,GOLD,2);
 card(layout.pause);rect(f,layout.pause.x+11*ui,layout.pause.y+10*ui,4*ui,14*ui,GOLD);rect(f,layout.pause.x+20*ui,layout.pause.y+10*ui,4*ui,14*ui,GOLD);
 rect(f,hx+10*ui,hy+44*ui,86*ui,2*ui,0x304c40);rect(f,hx+10*ui,hy+44*ui,86*ui*(s.rush>0?s.rush/7:s.energy/60),2*ui,s.rush>0?GOLD:JADE);
 const UiRect status={hx+115*ui,hy+ui,layout.pause.x-hx-122*ui,19*ui};
 const char* region=theme==0?"丛林石桥":theme==1?"落日神庙":"幽蓝古墓";uint32_t col=IVORY;
 if(s.rush>0){snprintf(num,sizeof(num),"觉醒 x4 %ds",int(s.rush)+1);region=num;col=GOLD;}
 else if(s.shield>0||s.magnet>0){snprintf(num,sizeof(num),"%s %ds",s.shield>0?"护盾":"磁铁",int(s.shield>0?s.shield:s.magnet));region=num;col=JADE;}
 const bool compact=status.w<40*ui;const char* notice=nullptr;uint32_t notice_color=GOLD;
 if(!compact)text_box(f,region,status,12,col,1,true);
 UiRect message={status.x,hy+23*ui,status.w,19*ui};
 if(s.next_turn-s.distance<30){bool armed=s.queued_turn==s.turn_direction;notice=armed?(s.turn_direction==1?"右转 已准备":"左转 已准备"):(s.turn_direction==1?"向右转 >>>":"<<< 向左转");notice_color=armed?JADE:GOLD;}
 else if(s.feedback_t>0&&s.feedback!=Feedback::Rush){
  notice=s.feedback==Feedback::Jump?"漂亮跳跃":s.feedback==Feedback::Slide?"完美滑铲":s.feedback==Feedback::Dodge?"惊险闪避":s.feedback==Feedback::Turn?"漂亮转弯":s.feedback==Feedback::Quest?"挑战达成 +500":s.feedback==Feedback::Rescue?"救援护盾":s.feedback==Feedback::Shield?"护盾破碎":"";
 }
 if(!compact&&notice)text_box(f,notice,message,12,notice_color,1,true);
 if(s.distance<28&&save.runs<=1)snprintf(num,sizeof(num),"左右变道   上跳跃   下滑铲");
 else snprintf(num,sizeof(num),"%s  %u/%u",s.quests%3==0?"收集金币":s.quests%3==1?"躲避障碍":"通过转弯",quest_progress(s),quest_goal(s));
 const UiRect quest={cx-112*ui,hy+50*ui,224*ui,18*ui};
 if(drawing_ui){const float rx=float(display->width)/width,ry=float(display->height)/height;auto b=pxa::ui::surface_to_canvas_region({int(quest.x*rx),int(quest.y*ry),int(quest.w*rx),int(quest.h*ry)},*display);overlay->rect(b.x,b.y,b.width,b.height,0x092a238e);}
 text_box(f,compact&&notice?notice:num,quest,12,compact&&notice?notice_color:IVORY);
}else{
 veil();const UiRect frame=s.mode==Mode::Help?layout.guide:s.mode==Mode::Paused?layout.paused:layout.result;card(frame);
 if(s.mode==Mode::Help){
  heading(f,"操作指南",layout.guide_heading);rule(f,{frame.x+30*ui,frame.y+48*ui,frame.w-60*ui,ui},0xa18e61);
  const char* lines[]={"左右滑动 / 方向键：变道","向上滑动 / A：跳跃","向下滑动 / B：滑铲","弯口：按提示向左或右","START 暂停 / 失败 B 救援","金币连击：觉醒吸金护体"};
  for(unsigned i=0;i<6;i++)text_box(f,lines[i],{frame.x+18*ui,frame.y+(58+i*21)*ui,frame.w-36*ui,20*ui},12,i<4?IVORY:JADE);
  text_box(f,"轻触返回",{frame.x+22*ui,frame.y+190*ui,frame.w-44*ui,20*ui},12,GOLD);
 }else if(s.mode==Mode::Paused){
  heading(f,"暂停旅程",layout.paused_heading);rule(f,{frame.x+30*ui,frame.y+51*ui,frame.w-60*ui,ui},0xa18e61);
  button(layout.resume,"继续探险",true);button(layout.sound,save.sound?"音效 开":"音效 关");button(layout.home,"返回主菜单");
 }else{
  heading(f,s.record?"新纪录":"旅程结束",layout.result_heading);rule(f,{frame.x+34*ui,frame.y+48*ui,frame.w-68*ui,ui},0xa18e61);
  snprintf(num,sizeof(num),"%06u",s.score);text_box(f,num,layout.score,28,GOLD,1,true);
  snprintf(num,sizeof(num),"距离 %um    金币 %u",unsigned(s.distance),s.coins);text_box(f,num,layout.stats,14,IVORY);
  text_box(f,!strcmp(s.reason,"gap")?"坠入深渊":!strcmp(s.reason,"turn")?"错过弯口":"碰到障碍",layout.reason,12,0xd6bba0);
  snprintf(num,sizeof(num),"连击 %u  挑战 %u",s.best_combo,s.quests);text_box(f,num,layout.achievements,12,JADE);
  button(layout.rescue,s.rescues?"返回":"救援继续");button(layout.retry,"重来",true);
 }
}
