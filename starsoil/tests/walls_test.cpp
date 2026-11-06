// SPDX-License-Identifier: MIT
#include "../core.hpp"
#include "../wall_art.hpp"
#include "../actor_renderer.hpp"
#include <algorithm>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <vector>
using namespace starsoil;
static_assert(Magnet==80 && WoodWalls==81); // Append-only saved item IDs.
static_assert(std::size(art::icons)==ItemCount);
static State s,loaded;
static void reset(int item=WoodWalls,int amount=50) {
  new_game(s,314159,WorldSize::Small);
  std::fill_n(s.tiles,Cells,uint8_t(Air));std::fill_n(s.walls,Cells,uint8_t(Air));
  std::fill_n(s.heights,WorldW,uint8_t(60));
  for(int x=1;x<s.width-1;++x)set_tile(s,x,60,Stone);
  for(auto &a:s.bag)a={};for(auto &n:s.npcs)n={};for(auto &m:s.mobs)m={};
  for(auto &d:s.drops)d={};s.selected=0;s.bag[0]={uint8_t(item),uint16_t(amount)};
  s.player={1608,960,0,0,100,0,1};s.player.grounded=true;s.invulnerable=60000;s.clock=100000;
}
static Input target(int x=102,int y=59) {Input i;i.use=true;i.target_x=x;i.target_y=y;return i;}
static void step(Input i,int ms=20) {s.spawn_timer=0;tick(s,i,ms);}
static void hold(Input i,int ms) {for(int n=0;n<ms;n+=20)step(i);}
static int recipe(int out,int ingredient) {
  for(int r=0;r<RecipeCount;++r)if(recipes[r].out==out && recipes[r].in[0].item==ingredient)return r;
  assert(false);return -1;
}
static void recipes_and_placement() {
  for(auto pair:{std::pair{WoodWalls,Boards},std::pair{BrickWalls,Bricks},std::pair{GlassWalls,Glass}}) {
    reset();s.bag[0]={uint8_t(pair.second),1};
    int r=recipe(pair.first,pair.second);assert(craft_status(s,r)==CraftStatus::Station);
    set_tile(s,98,59,Bench);assert(craft(s,r));assert(count(s,pair.first)==4 && !count(s,pair.second));
    assert(craft(s,recipe(pair.second,pair.first)));assert(!count(s,pair.first)&&count(s,pair.second)==1);
    reset(pair.first);auto in=target();step(in);
    assert(tile(s,102,59)==Air && s.walls[59*WorldW+102]==items[pair.first].tile && count(s,pair.first)==49);
    hold(in,800);assert(count(s,pair.first)==49);
    // A backing-only neighbor anchors the next panel, including behind actors/furniture.
    s.cooldown=0;step(target(102,58));assert(s.walls[58*WorldW+102]==items[pair.first].tile);
    s.cooldown=0;set_tile(s,101,59,StorageChest);step(target(101,59));
    assert(tile(s,101,59)==StorageChest && s.walls[59*WorldW+101]==items[pair.first].tile);
    s.cooldown=0;step(target(100,59));assert(s.walls[59*WorldW+100]==items[pair.first].tile);
    // Explicit replacement returns the old wall; repeated holding spends once.
    s.cooldown=0;int before=count(s,pair.first);s.walls[59*WorldW+103]=pair.first==WoodWalls?int(Glass):int(Plank);
    int old=s.walls[59*WorldW+103],refund=wall_drop(uint8_t(old)),old_count=count(s,refund);
    step(target(103,59));assert(count(s,pair.first)==before-1 && count(s,refund)==old_count+1);
    hold(target(103,59),600);assert(count(s,pair.first)==before-1);
    reset(pair.first);step(target(103,56));assert(count(s,pair.first)==50 && !s.walls[56*WorldW+103]);
    auto distant=target(110,59);step(distant);assert(count(s,pair.first)==50);
  }
  reset(Wood);set_tile(s,98,59,Bench);assert(craft(s,recipe(WoodHammer,Wood)) && count(s,WoodHammer)==1);
  reset(IronBar);add(s,Wood,3);set_tile(s,98,59,Anvil);
  assert(craft(s,recipe(IronHammer,IronBar)) && count(s,IronHammer)==1);
  puts("PASS three reversible wall recipes, both hammers, station checks, exact material consumption, actor/furniture backing and support/range guards");
}
static void exchanges_and_preview() {
  reset(GlassWalls,2);s.walls[59*WorldW+102]=Plank;
  for(auto &slot:s.bag)slot={Wood,999};s.bag[0]={GlassWalls,2};
  for(auto &drop:s.drops)drop={2000,960,0,Rock,999};
  loaded=s;auto plan=placement_preview(s,target(),102,59);
  assert(plan.failure==PlaceFailure::Space && std::memcmp(&s,&loaded,sizeof(s))==0);
  hold(target(),500);assert(s.bag[0].count==2 && s.walls[59*WorldW+102]==Plank);
  // Last held panel frees exactly the required refund slot, even with full loot.
  s.bag[0].count=1;s.cooldown=0;
  assert(placement_preview(s,target(),102,59).ok());
  hold(target(),900);assert(s.walls[59*WorldW+102]==Glass && count(s,WoodWalls)==1 && !count(s,GlassWalls));
  assert(s.placement_release);step({});assert(!s.placement_release);
  reset(GlassWalls,2);for(auto &slot:s.bag)slot={Wood,999};s.bag[0]={GlassWalls,2};
  s.walls[59*WorldW+102]=Plank;step(target());
  assert(s.walls[59*WorldW+102]==Glass && s.bag[0].count==1);
  assert(std::any_of(std::begin(s.drops),std::end(s.drops),[](const Drop &d){return d.item==WoodWalls && d.count==1;}));
  reset(WoodWalls);assert(placement_preview(s,target(),102,55).failure==PlaceFailure::Support);
  assert(placement_preview(s,target(),115,59).failure==PlaceFailure::Range);
  s.walls[59*WorldW+102]=Plank;
  assert(placement_preview(s,target(),102,59).failure==PlaceFailure::Duplicate);
  reset(WoodWalls);s.smart_cursor=true;s.target_ready=false;Input smart;smart.smart_cursor=true;
  assert(placement_preview(s,smart,102,59).failure==PlaceFailure::Blocked);
  reset(Doors);auto door=placement_preview(s,target(),102,59);assert(door.ok() && door.bottom==59);
  step(target());assert(tile(s,102,58)==DoorTop && tile(s,102,59)==Door);
  puts("PASS atomic wall replacement, exact held-stack use, refund spill/full-pool protection, last-panel hold latch and read-only placement/door previews");
}
static void demolition() {
  for(int hammer:{WoodHammer,IronHammer})for(int wall:{int(Plank),int(Brick),int(Glass)}) {
    reset(hammer,1);s.walls[59*WorldW+102]=uint8_t(wall);set_tile(s,102,59,StorageChest);
    auto in=target();step(in);assert(s.mining && s.walls[59*WorldW+102]==wall);
    hold(in,400);assert(!s.walls[59*WorldW+102] && tile(s,102,59)==StorageChest);
    assert(count(s,wall_drop(uint8_t(wall)))==1 && count(s,hammer)==1 && s.mined==1);
    hold(in,500);assert(count(s,wall_drop(uint8_t(wall)))==1);
  }
  reset(WoodPick,1);s.walls[59*WorldW+102]=Plank;set_tile(s,102,59,Dirt);
  hold(target(),400);assert(tile(s,102,59)==Air && s.walls[59*WorldW+102]==Plank);
  reset(IronHammer,1);s.walls[59*WorldW+102]=Plank;set_tile(s,102,59,Brick);
  hold(target(),500);assert(tile(s,102,59)==Brick && s.walls[59*WorldW+102]==Plank);
  reset(IronHammer,1);set_tile(s,101,59,Stone);set_tile(s,101,58,Stone);s.walls[59*WorldW+103]=Plank;
  hold(target(103,59),500);assert(s.walls[59*WorldW+103]==Plank); // no hammer through a foreground wall
  reset(WoodHammer,1);s.walls[59*WorldW+102]=Stone;hold(target(),400);
  assert(!s.walls[59*WorldW+102] && !count(s,BrickWalls)); // Natural cave backing never creates artificial materials.
  reset(WoodHammer,1);s.walls[59*WorldW+102]=Plank;step(target());assert(s.mining);
  step({});assert(!s.mining); // release cancels incomplete work
  reset(IronHammer,1);for(auto &slot:s.bag)slot={Wood,999};s.bag[0]={IronHammer,999};
  s.walls[59*WorldW+102]=Plank;hold(target(),140);assert(!s.walls[59*WorldW+102]);
  assert(std::any_of(std::begin(s.drops),std::end(s.drops),[](const Drop &d){return d.item==WoodWalls && d.count==1;}));
  reset(IronHammer,1);for(auto &slot:s.bag)slot={Wood,999};s.bag[0]={IronHammer,999};
  for(auto &drop:s.drops)drop={2000,960,0,Rock,999};s.walls[59*WorldW+102]=Plank;
  hold(target(),140);assert(s.walls[59*WorldW+102]==Plank && s.mined==0);
  for(int hammer:{WoodHammer,IronHammer}) {
    reset(hammer,1);s.mobs[0]={s.player.x+26,s.player.y,0,0,100,1,0};
    step(target());const int hp=100-(2+2*items[hammer].tier);assert(s.mobs[0].hp==hp);
    step(target());assert(s.mobs[0].hp==hp && s.tool_cooldown>0);
  }
  puts("PASS hammers recover exact backing only, picks preserve backing, blocked/hidden targets, natural walls, release and full-bag/full-loot safety");
}
static void smart_save_shelter_art() {
  reset(BrickWalls);Input in;in.use=true;in.smart_cursor=true;
  hold(in,700);int panels=0;for(int i=0;i<Cells;++i)panels+=s.walls[i]==Brick;
  assert(panels>1 && panels==50-count(s,BrickWalls));
  s.bag[1]=s.bag[0];s.bag[0]={IronHammer,1};s.cooldown=0;hold(in,1200);
  assert(count(s,BrickWalls)==50);for(int i=0;i<Cells;++i)assert(!s.walls[i]);
  reset(IronHammer,1);s.walls[59*WorldW+102]=Brick;s.walls[58*WorldW+102]=Glass;
  assert(!can_spawn_enemy(s,102,60,1));
  std::vector<uint8_t> data(SaveBytes);auto n=encode(s,data.data(),data.size());
  assert(n && decode(loaded,data.data(),n));assert(loaded.bag[0].item==IronHammer);
  assert(loaded.walls[59*WorldW+102]==Brick && loaded.walls[58*WorldW+102]==Glass);
  assert(std::memcmp(s.walls,loaded.walls,Cells)==0);
  for(int wall:{int(Plank),int(Brick),int(Glass)})for(int mask=0;mask<16;++mask) {
    uint16_t pixels[256]{};starsoil_wall::draw(wall,mask,3,5,0,0,255,[&](int x,int y,int w,int h,uint16_t c){
      assert(x>=0 && y>=0 && x+w<=16 && y+h<=16);for(int yy=y;yy<y+h;++yy)for(int xx=x;xx<x+w;++xx)pixels[yy*16+xx]=c;
    });for(auto c:pixels)assert(c);
  }
  for(int hammer:{WoodHammer,IronHammer}) {
    assert(starsoil_actor::pose(uint8_t(hammer),true)==starsoil_actor::ToolUse);
    const auto *grip=art::item_grips[hammer];assert(art::icons[hammer][grip[1]*16+grip[0]]);
  }
  puts("PASS smart wall chaining/removal, indoor spawn protection, save compatibility, hole-free joined wall art and opaque animated hammer grips");
}
int main(){recipes_and_placement();exchanges_and_preview();demolition();smart_save_shelter_art();}
