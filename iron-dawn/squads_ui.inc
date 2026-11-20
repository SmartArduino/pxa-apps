// SPDX-License-Identifier: MIT
// The two controls for each squad are deliberately separate touch targets.

 void squad_cards(Raster& r,int index,Rect recall,Rect assign){
  unsigned number=index+1;
  int count=world.squad_size(number);
  char label[40];
  std::snprintf(label,sizeof label,"%u队 · %d",number,count);
  button(r,recall,squad_recall0+index,label,world.squad_selected(number),count>0);
  std::snprintf(label,sizeof label,"编入 %u队",number);
  button(r,assign,squad_assign0+index,label,false,world.has_squad_selection());
 }

 void squad_action(unsigned number,bool assign){
  if(assign){
   if(world.assign_squad(number))save();
   return;
  }
  bool jump=world.squad_selected(number);
  if(world.recall_squad(number)){
   if(jump){
    field.focus(world.squad_center(number));
    world.say("已定位当前编队");
    log("Iron Dawn squad focused");
   }
   sounds.respond(world,UnitCue::select);
   report_selection();
  }
 }
