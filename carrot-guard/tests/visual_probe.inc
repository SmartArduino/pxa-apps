// SPDX-License-Identifier: MIT
// Isolated simulator fixture; never modifies progress or shipped builds.
void updateVisualProbe(uint64_t now){
  static int phase=-1;
  int next=visualProbePhase;
  if(next==phase)return;
  phase=next;game.unlock(Levels);game.reset(0);view=Play;selected=-1;
  game.coins=100000;
  for(int i=0;i<Slots;++i)if(game.land(i)&&!game.towers[i].obstacle){game.build(i,1);selected=i;break;}
  game.coins=999999;game.hearts=phase==1?7:phase==2?4:phase==3?1:10;
  carrotFeedback.reset(game.hearts);
  if(phase==4){
    selected=-1;game.wave=3;game.active=true;for(auto& t:game.towers)t={};
    for(int i=0;i<EnemyTypes;++i){game.spawn(i);game.enemies[i].p={50.f+(i%5)*85.f,95.f+(i/5)*88.f};game.enemies[i].facesRight=(i%2)==0;}
  }else if(phase==5){view=Complete;game.result=Result::Win;}
  else if(phase==6){view=Complete;game.hearts=0;game.result=Result::Loss;}
  else if(phase==7){view=Help;helpPage=1;}
  else if(phase==8){view=LevelSelect;levelPage=2;}
  displayValid=false;
  char message[96];std::snprintf(message,sizeof(message),"carrot visual probe phase=%d hearts=%d",phase,game.hearts);pxa_log_write(2,message);
}
