// SPDX-License-Identifier: MIT
// Included in core.cpp after the actor/door collision helpers.
enum class PlaceFailure { None, Blocked, Support, DoorSpace };
struct Placement { PlaceFailure failure; int bottom; bool ok() const { return failure==PlaceFailure::None; } };
bool replaceable(int t) { return t==Air || t==Flower || t==Herb || t==Mushroom || t==Vine; }
Placement placement(const State &s,const Input &in,int x,int y) {
  const auto &def=items[s.bag[s.selected].item];
  auto fail=[&](PlaceFailure reason) { return Placement{reason,y}; };
  float dx=x*16+8-s.player.x,dy=y*16+8-(s.player.y-12);
  if(!inside(s,x,y) || dx*dx+dy*dy>96*96 || (def.kind!=Block && def.kind!=Furniture))
    return fail(PlaceFailure::Blocked);
  if(def.tile==Door) {
    int bottom=y;
    if(!solid(tile(s,x,bottom+1)) && tile(s,x,bottom+1)!=Platform)++bottom;
    for(int yy=bottom-1;yy<=bottom;++yy)
      if(!inside(s,x,yy) || !replaceable(tile(s,x,yy)) || occupied(s,x,yy))
        return fail(PlaceFailure::DoorSpace);
    if(!solid(tile(s,x,bottom+1)) && tile(s,x,bottom+1)!=Platform)
      return fail(PlaceFailure::DoorSpace);
    return {PlaceFailure::None,bottom};
  }
  if(!replaceable(tile(s,x,y)))return fail(PlaceFailure::Blocked);
  bool wall=def.tile==Plank && in.down;
  if(wall && s.walls[index(x,y)]==Plank)return fail(PlaceFailure::Blocked);
  if(solid(def.tile) && !wall && occupied(s,x,y))return fail(PlaceFailure::Blocked);
  if(!(tile(s,x-1,y) || tile(s,x+1,y) || tile(s,x,y-1) || tile(s,x,y+1) || s.walls[index(x,y)]))
    return fail(PlaceFailure::Support);
  if(def.tile==Sapling && tile(s,x,y+1)!=Grass && tile(s,x,y+1)!=Dirt)
    return fail(PlaceFailure::Blocked);
  return {PlaceFailure::None,y};
}
int mining_tier(int t) {
  return t==GoldOre?2:(t==CrystalOre || t==RubyOre || t==Obsidian || t==Deepstone)?3:1;
}
bool chop_target(int t) {
  return t==Log || t==Leaves || t==Cactus || t==Vine || t==Sapling;
}
bool target_ray(const State &s,int x,int y,float end_x,float end_y) {
  float start_x=s.player.x,start_y=s.player.y-12,dx=end_x-start_x,dy=end_y-start_y;
  int cx=ix(start_x),cy=ix(start_y),step_x=dx<0?-1:1,step_y=dy<0?-1:1;
  float next_x=std::fabs(dx)<.001f?1e9f:((cx+(step_x>0))*16-start_x)/dx;
  float next_y=std::fabs(dy)<.001f?1e9f:((cy+(step_y>0))*16-start_y)/dy;
  float delta_x=std::fabs(dx)<.001f?1e9f:16/std::fabs(dx);
  float delta_y=std::fabs(dy)<.001f?1e9f:16/std::fabs(dy);
  auto blocked=[&](int tx,int ty) { return (tx!=x || ty!=y) && solid(tile(s,tx,ty)); };
  for(int n=0;n<20;++n) {
    if(cx==x && cy==y)return true;
    if(n && blocked(cx,cy))return false;
    if(std::fabs(next_x-next_y)<.00001f) {
      if(blocked(cx+step_x,cy) || blocked(cx,cy+step_y))return false;
      cx+=step_x;cy+=step_y;next_x+=delta_x;next_y+=delta_y;
    } else if(next_x<next_y) { cx+=step_x;next_x+=delta_x; }
    else { cy+=step_y;next_y+=delta_y; }
  }
  return false;
}
bool target_visible(const State &s,int x,int y) {
  // Faces keep a new bridge cell reachable above an existing bridge edge.
  for(auto offset : {std::pair{8,8},std::pair{8,1},std::pair{1,8},std::pair{15,8},std::pair{8,15}})
    if(target_ray(s,x,y,float(x*16+offset.first),float(y*16+offset.second)))return true;
  return false;
}
bool smart_kind(int item) {
  auto k=items[item].kind;
  return k==Pick || k==Axe || k==Block || k==Furniture || item==Glass;
}
bool choose_smart_target(State &s,const Input &in) {
  const int item=s.bag[s.selected].item;
  const auto &def=items[item];
  const bool building=def.kind==Block || def.kind==Furniture;
  const bool wall=building && def.tile==Plank && in.down;
  int ay=wall?0:(in.aim_y>0?1:in.aim_y<0?-1:0);
  int ax=in.aim_x?(in.aim_x>0?1:-1):ay?(in.move>0?1:in.move<0?-1:0):(s.player.face?1:-1);
  if(!ax && !ay)ax=s.player.face?1:-1;
  const int fallback_x=s.target_x,fallback_y=s.target_y;
  const bool row_chain=building && def.kind==Block && !wall && !ay &&
      in.use && s.last_place_item==item && std::abs(s.last_place_y-ix(s.player.y))<=1;
  const int foot_y=ix(s.player.y),player_x=ix(s.player.x);
  const bool bridge=building && !wall && !ay && s.player.grounded &&
      (solid(def.tile) || def.tile==Platform) &&
      (!tile(s,player_x+ax,foot_y) || !tile(s,player_x+2*ax,foot_y));
  auto score=[&](int x,int y) {
    if(!inside(s,x,y))return 1000000;
    float rx=x*16+8-s.player.x,ry=y*16+8-(s.player.y-12);
    if(rx*rx+ry*ry>96*96)return 1000000;
    float along=rx*ax+ry*ay,across=std::fabs(rx*ay-ry*ax);
    if(along< -2 || across>along*1.3f+14 || (ay<0 && ry>=-4) || (ay>0 && ry<=4))return 1000000;
    if(!building && item!=Glass && !ay && y>=ix(s.player.y))return 1000000;
    int t=tile(s,x,y);
    if(building) {
      if((row_chain || bridge) && y!=(row_chain?s.last_place_y:foot_y))return 1000000;
      if(!placement(s,in,x,y).ok() || (!wall && occupied(s,x,y)))return 1000000;
      // DOWN on planks selects backing-wall mode rather than the floor.
      int anchor_y=row_chain?s.last_place_y:bridge?foot_y:wall?foot_y-1:fallback_y;
      int anchor_x=row_chain?s.last_place_x+ax:fallback_x;
      return (x-anchor_x)*(x-anchor_x)*40+(y-anchor_y)*(y-anchor_y)*40+int(along*.5f+across*2);
    }
    if(item==Glass) { if(t!=Water && t!=Lava)return 1000000; }
    else {
      if(!t || t==Water || t==Lava || def.tier<mining_tier(t) || (def.kind==Axe && !chop_target(t)))return 1000000;
    }
    int result=int(along*2+across*5);
    if(def.kind==Axe)result+=t==Log || t==Cactus?0:72;
    else if(def.kind==Pick) {
      if(is_ore(t))result-=12;
      if(t==Leaves || t==Vine || t==Sapling || t==Flower || t==Herb || t==Mushroom)result+=40;
      if(door(t) || t==Chest || t==StorageChest || t==Bed || t==Bench || t==Furnace || t==Anvil || t==Portal)result+=48;
    }
    if(!ax && rx*(s.player.face?1:-1)<-4)result+=8;
    return result;
  };
  // Keep work on one reachable block until it breaks or the direction changes.
  if(in.use && s.mining && score(s.mine_x,s.mine_y)<1000000 && target_visible(s,s.mine_x,s.mine_y)) {
    s.target_x=s.mine_x;s.target_y=s.mine_y;return true;
  }
  int best=1000000,best_x=fallback_x,best_y=fallback_y;
  int px=ix(s.player.x),py=ix(s.player.y-12);
  for(int y=py-6;y<=py+6;++y)for(int x=px-6;x<=px+6;++x) {
    int value=score(x,y);
    if(value<best && target_visible(s,x,y)) { best=value;best_x=x;best_y=y; }
  }
  s.target_x=best_x;s.target_y=best_y;
  return best<1000000;
}
