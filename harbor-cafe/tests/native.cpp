// SPDX-License-Identifier: CC0-1.0
#include "../core.hpp"
#include "../../common/pocket-play/runtime.hpp"
#include <assert.h>
#include <stdio.h>
#include <string.h>
namespace test_runtime { extern bool drawing;extern uint32_t clock_ms;extern unsigned saves,tones;void image(const char *); }
using cafe::State;using cafe::Event;using pocket::Phase;using pocket::Key;
static unsigned checks=0;
#define CHECK(v) do{++checks;if(!(v)){fprintf(stderr,"CHECK FAILED %d %s\n",__LINE__,#v);assert(v);}}while(0)
static void advance(State &s,unsigned ms) { while(ms){unsigned n=ms>100?100:ms;cafe::step(s,n);ms-=n;} }
static void live(unsigned ms) { while(ms){unsigned n=ms>100?100:ms;test_runtime::clock_ms+=n;game::tick(n);ms-=n;} }
static State current() { uint8_t b[160];State s;CHECK(game::serialize(b,sizeof(b))==160);CHECK(cafe::decode(s,b,sizeof(b)));return s; }
static void same(const State &a,const State &b) { uint8_t aa[160],bb[160];CHECK(cafe::encode(a,aa,sizeof(aa))==160);CHECK(cafe::encode(b,bb,sizeof(bb))==160);CHECK(!memcmp(aa,bb,160)); }
static void roundtrip(const State &a) { uint8_t b[160];State t;CHECK(cafe::encode(a,b,sizeof(b))==160);CHECK(cafe::decode(t,b,sizeof(b)));same(a,t); }
static void restore(const State &a) { uint8_t b[160];CHECK(cafe::encode(a,b,sizeof(b))==160);game::load(b,sizeof(b)); }
static void draw(const char *root,const char *tag,const State &a) { restore(a);test_runtime::drawing=true;game::draw();test_runtime::drawing=false;char path[1024];snprintf(path,sizeof(path),"%s/%s.ppm",root,tag);test_runtime::image(path); }
static void rehash(uint8_t *p) { uint32_t h=2166136261u;for(unsigned i=0;i<156;++i)h=(h^p[i])*16777619u;for(unsigned i=0;i<4;++i)p[156+i]=uint8_t(h>>(8*i)); }
static void tap(int x,int y) { game::pointer(Phase::DOWN,x,y);game::pointer(Phase::UP,x,y); }
int main(int argc,char **argv) {
 CHECK(argc==2);State s;CHECK(cafe::valid(s));roundtrip(s);
 // Five complete 150-second days, using the same public economy operations as UI.
 unsigned packs[3]={},consumed[3]={},made[3]={},upgrade_spend=0,pack_spend=0,sales=0,bonuses=0,rent=0;
 unsigned completed=0;
 for(unsigned day=1;day<=5;++day) {
  CHECK(s.day==day);while(!s.summary) {
   for(unsigned i=0;i<3;++i) if(s.level[i]<2&&s.coins>=cafe::upgrade_cost(s,i)+16) { upgrade_spend+=cafe::upgrade_cost(s,i);CHECK(cafe::upgrade(s,i)==Event::UPGRADED); }
   if(s.cook_recipe==cafe::NONE) {
    int best=-1;for(unsigned i=0;i<3;++i) if(s.customers[i].stage&&(s.ready_recipe==cafe::NONE||s.customers[i].recipe==s.ready_recipe)){best=int(i);break;}
    if(best>=0) {
     s.table=uint8_t(best);auto &c=s.customers[best];
     if(c.stage==1)CHECK(cafe::act(s)==Event::ACCEPTED);
     if(s.ready_recipe!=cafe::NONE) { unsigned before=s.revenue;CHECK(cafe::act(s)==Event::SOLD);sales+=s.revenue-before;++completed; }
     else {
      s.recipe=c.recipe;for(unsigned i=0;i<3;++i)if(s.stock[i]<cafe::INGREDIENTS[s.recipe][i]){CHECK(cafe::buy(s,i)==Event::BOUGHT);++packs[i];pack_spend+=cafe::PACK_COST[i];}
      ++made[s.recipe];for(unsigned i=0;i<3;++i)consumed[i]+=cafe::INGREDIENTS[s.recipe][i];CHECK(cafe::act(s)==Event::COOKING);
     }
    }
   }
   cafe::step(s,100);CHECK(cafe::valid(s));if(s.day_ms%10000==0)roundtrip(s);
  }
  CHECK(s.served>=cafe::target(s));CHECK(s.bonus==8);CHECK(s.fee==10);CHECK(s.day_ms==150000);CHECK(s.coins>0);bonuses+=s.bonus;rent+=s.fee;roundtrip(s);
  auto frozen=s;advance(s,50000);same(s,frozen);if(day<5)cafe::next_day(s);
 }
 CHECK(completed>=45);CHECK(made[0]&&made[1]&&made[2]);CHECK(s.stars==5&&s.total_served==completed);
 CHECK(s.level[0]==2&&s.level[1]==2&&s.level[2]==2);
 CHECK(s.coins==30+sales+bonuses-rent-upgrade_spend-pack_spend);
 for(unsigned i=0;i<3;++i)CHECK(s.stock[i]==unsigned(i==0?8:6)+packs[i]*4-consumed[i]);
 draw(argv[1],"day5-settlement",s);cafe::next_day(s);draw(argv[1],"upgraded-cafe",s);
 auto keep=s;cafe::restart(s);CHECK(s.coins==keep.coins&&s.stars==keep.stars&&s.day==keep.day&&s.level[0]==2&&s.stock[0]==keep.stock[0]);
 // Settling a replay of an already settled day never duplicates its fee/bonus.
 State replay=keep;replay.paid_day=replay.day;cafe::restart(replay);unsigned cash=replay.coins,stars=replay.stars;advance(replay,150000);CHECK(replay.coins==cash&&replay.stars==stars&&replay.bonus==0&&replay.fee==0);
 // Honest shortage recovery consumes a credit crate then repays it from sales.
 State poor;poor.coins=0;poor.stock[0]=poor.stock[1]=poor.stock[2]=0;advance(poor,600);CHECK(poor.customers[0].stage==1);CHECK(cafe::act(poor)==Event::ACCEPTED);CHECK(cafe::act(poor)==Event::STOCK);CHECK(cafe::buy(poor,0)==Event::POOR);CHECK(cafe::buy(poor,3)==Event::CREDIT);CHECK(poor.debt==8&&poor.rescued);CHECK(cafe::buy(poor,3)==Event::MAXED);CHECK(cafe::act(poor)==Event::COOKING);CHECK(poor.stock[0]==1&&poor.cogs==2);advance(poor,2400);CHECK(poor.ready_recipe==0);CHECK(cafe::act(poor)==Event::SOLD);CHECK(poor.debt==0&&poor.coins==2&&poor.revenue==10);roundtrip(poor);
 // The credit ceiling must never permanently deny recovery. Honest sales
 // reduce maximum debt; target rewards can fund a normal cash purchase.
 State deep;deep.coins=0;deep.stock[0]=deep.stock[1]=deep.stock[2]=0;deep.debt=1000;
 unsigned rescue_days=0;while(deep.coins<8&&rescue_days<100) {
  CHECK(cafe::buy(deep,3)==Event::CREDIT);CHECK(deep.debt<=1000);++rescue_days;
  for(unsigned drink=0;drink<3;++drink) { unsigned r=drink==2?2:0;deep.table=0;deep.recipe=uint8_t(r);deep.customers[0]={2,uint8_t(r),0,cafe::patience_limit(deep)};CHECK(cafe::act(deep)==Event::COOKING);advance(deep,2400);deep.table=0;CHECK(cafe::act(deep)==Event::SOLD); }
  roundtrip(deep);if(deep.coins>=8)break;advance(deep,150000);cafe::next_day(deep);
 }
 CHECK(rescue_days>=1&&rescue_days<100&&deep.debt<1000&&deep.coins>=8);CHECK(cafe::buy(deep,0)==Event::BOUGHT);roundtrip(deep);
 // Wrong drink stays on the tray; binning keeps ingredient cost. Failures recover.
 State wrong;advance(wrong,600);CHECK(cafe::act(wrong)==Event::ACCEPTED);wrong.recipe=1;CHECK(cafe::act(wrong)==Event::COOKING);CHECK(cafe::act(wrong)==Event::BUSY);advance(wrong,2400);CHECK(cafe::act(wrong)==Event::WRONG);CHECK(wrong.ready_recipe==1);CHECK(cafe::discard(wrong)==Event::DISCARDED);CHECK(wrong.cogs==3&&wrong.stock[0]==7&&wrong.stock[1]==5);advance(wrong,40000);CHECK(wrong.failed>0&&wrong.coins==30);roundtrip(wrong);
 // Byte-exact format, unchanged target on every failed decoder invocation.
 uint8_t good[160],bad[256];State base=poor;CHECK(cafe::encode(base,good,160)==160);CHECK(!cafe::encode(base,bad,159));CHECK(!cafe::decode(base,nullptr,160));
 for(unsigned n=0;n<=256;++n)if(n!=160){memset(bad,0,sizeof(bad));memcpy(bad,good,160);State out=poor;CHECK(!cafe::decode(out,bad,n));same(out,poor);}
 for(unsigned i=0;i<160;++i)for(unsigned b=0;b<8;++b){memcpy(bad,good,160);bad[i]^=uint8_t(1u<<b);State out=poor;CHECK(!cafe::decode(out,bad,160));same(out,poor);}
 const unsigned invalid[]={5,30,33,34,35,36,37,38,39,43,47,52,56,57,77,98,155};
 for(unsigned at:invalid){memcpy(bad,good,160);bad[at]=255;rehash(bad);State out=poor;CHECK(!cafe::decode(out,bad,160));same(out,poor);}
 uint32_t rng=1;for(unsigned k=0;k<3000;++k){for(auto &b:bad){rng=rng*1664525u+1013904223u;b=uint8_t(rng>>24);}if(k%2==0){memcpy(bad,"HCFE\1\0\240\0",8);rehash(bad);}State out=poor;CHECK(!cafe::decode(out,bad,k%257));same(out,poor);}
 // Actual game hooks: touch/controller economy, CANCEL and outside release.
 game::init();live(700);auto before=current();game::pointer(Phase::DOWN,143,179);game::pointer(Phase::CANCEL,143,179);game::pointer(Phase::UP,143,179);same(current(),before);
 game::pointer(Phase::DOWN,143,179);game::pointer(Phase::UP,-30,260);same(current(),before);tap(143,179);CHECK(current().customers[0].stage==2);game::key(Key::A);CHECK(current().cook_recipe==0);CHECK(current().stock[0]==7);roundtrip(current());live(2400);game::key(Key::A);CHECK(current().total_served==1&&current().coins==40);CHECK(test_runtime::tones>=3);
 game::key(Key::X);CHECK(current().view==cafe::View::SUPPLY);tap(140,68);CHECK(current().stock[0]==11&&current().coins==32);game::key(Key::B);game::key(Key::Y);CHECK(current().view==cafe::View::UPGRADES);game::key(Key::A);CHECK(current().level[0]==1&&current().coins==12);game::key(Key::B);
 before=current();game::reset();auto after=current();CHECK(after.coins==before.coins&&after.level[0]==1&&after.stock[0]==before.stock[0]&&after.day_ms==0);
 wrong.ready_recipe=1;restore(wrong);tap(63,135);CHECK(current().ready_recipe==cafe::NONE&&current().cogs==3);
 // Old v1 sound-off saves remain silent through idle ticks and failed actions.
 State muted;cafe::fresh(muted);muted.sound=0;game::init();restore(muted);
 unsigned quiet=test_runtime::tones;live(100);game::key(Key::A);CHECK(current().sound==0&&test_runtime::tones==quiet);
 game::key(Key::L);game::key(Key::A);CHECK(current().sound==1&&test_runtime::tones>quiet);
 quiet=test_runtime::tones;live(100);CHECK(test_runtime::tones==quiet);
 game::key(Key::A);quiet=test_runtime::tones;live(100);CHECK(current().sound==0&&test_runtime::tones==quiet);
 // Render both languages, every screen, recipe, upgrade and maximum HUD.
 for(unsigned lang=0;lang<2;++lang) {
  pocket::set_english(lang!=0);
  State visual;advance(visual,25000);for(unsigned i=0;i<3;++i){visual.customers[i]={uint8_t(i%2+1),uint8_t(i),uint8_t(i),uint32_t(5000+i*10000)};}
  char tag[64];snprintf(tag,sizeof(tag),"%s-fresh-cafe-orders",lang?"en":"zh");draw(argv[1],tag,visual);
  for(unsigned level=0;level<4;++level){for(auto &l:visual.level)l=uint8_t(level);visual.recipe=uint8_t(level%3);visual.table=uint8_t(level%3);visual.cook_recipe=visual.recipe;visual.cook_ms=300;visual.ready_recipe=cafe::NONE;snprintf(tag,sizeof(tag),"%s-cafe-level-%u",lang?"en":"zh",level);draw(argv[1],tag,visual);}
  visual.cook_recipe=cafe::NONE;visual.cook_ms=0;visual.ready_recipe=2;visual.coins=999999;visual.stars=999;visual.day=999;snprintf(tag,sizeof(tag),"%s-max-hud",lang?"en":"zh");draw(argv[1],tag,visual);
  for(unsigned v=1;v<4;++v){visual.view=cafe::View(v);snprintf(tag,sizeof(tag),"%s-view-%u",lang?"en":"zh",v);draw(argv[1],tag,visual);}
 }
 pocket::set_english(false);
 restore(poor);unsigned saves=test_runtime::saves;live(11900);CHECK(test_runtime::saves>saves);
 printf("NATIVE PASS checks=%u days=5 served=%u made=%u/%u/%u cash=%u stars=%u levels=%u/%u/%u\n",checks,completed,made[0],made[1],made[2],unsigned(s.coins),s.stars,s.level[0],s.level[1],s.level[2]);
}
