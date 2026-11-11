// SPDX-License-Identifier: MIT
#include "living.hpp"
#include "settlement.hpp"
#include "wilderness.hpp"
#include "mines.hpp"
#include <algorithm>
#include <cstdlib>
namespace farm {
const char *region_name(Region r, bool en) {
  const char *cn[] = {"晴露农场", "晴露镇", "林间小径", "镜光湖畔", "温暖农舍", "蜜风果园", "晴露种子铺", "村民会馆", "回声矿洞", "小禾的暖房", "阿岚木工坊", "澄的湖畔小屋", "林婆婆的花屋"};
  const char *eng[] = {"Dewfield Farm", "Dewfield Town", "Whisper Woods",
                       "Mirror Lake", "Farm Cottage", "Honey Orchard", "Seed Store", "Village Lodge", "Echo Mine", "Xiaohe's quarters", "Alan's workshop", "Cheng's cabin", "Lin's cottage"};
  return (en ? eng : cn)[unsigned(r) < RegionCount ? unsigned(r) : 0];
}
const char *npc_name(int id, bool en) {
  const char *cn[] = {"小禾", "阿岚", "澄", "林婆婆"},
             *eng[] = {"Xiaohe", "Alan", "Cheng", "Granny Lin"};
  return (en ? eng : cn)[id % 4];
}
const char *npc_job(int id, bool en) {
  const char *cn[] = {"种子铺店主", "木匠", "湖畔钓手", "镇上的长者"},
             *eng[] = {"Seed keeper", "Carpenter", "Fisher", "Village elder"};
  return (en ? eng : cn)[id % 4];
}
const Portal *portal_at(const State &s, int x, int y) {
  for (const auto &p : Portals)
    if (unsigned(p.from) == s.region && p.x == x && p.y == y)
      return &p;
  return nullptr;
}
bool road_tile(uint8_t region,int x,int y) {
  switch(Region(region)) {
  case Region::Farm:
    return (y==7 && x>=6) || (x==6 && y>=7 && y<=18) ||
      (x==22 && y>=7 && y<=12) || (x==21 && y>=12 && y<=18) ||
      (y==12 && x>=21&&x<=22) || (y==18 && x>=6&&x<=21) || (x==15&&y>=18) ||
      (y==11 && x>=4&&x<=6) || (y==16 && x>=21&&x<=23) || (y==19 && x>=4 && x<=7) || (y==17 && x>=21 && x<=23);
  case Region::Town:
    return (y==19&&x>=11&&x<=25) || ((x==11||x==25)&&y>=12&&y<=19) || (y==12 && x>=7&&x<=24) || (x==15 && y>=7) ||
      (y==7 && x>=1 && x<=24) || (x==9&&y==8) || ((x==7||x==24)&&y>=7&&y<=12) ||
      (x>=12&&x<=18&&y>=9&&y<=15);
  case Region::Forest:
    return (y==4&&x>=15&&x<=22)||(x==22&&y>=3&&y<=4)||y==12 || (x==15&&y<=20) || (x==22&&y>=12&&y<=20) ||
      (y==20&&x>=15&&x<=22);
  case Region::Lake:
    return (y==12&&x>=21) || (x==26&&y>=9&&y<=12) || (x==24&&y>=9&&y<=17);
  case Region::Orchard:
    return (x==15&&y<=19) || (y==12&&x<=15) || ((y==9||y==19)&&x>=8&&x<=22) ||
      ((x==8||x==22)&&y>=9&&y<=19);
  default:return false;
  }
}
bool exit_clearance(uint8_t region,int x,int y) {
  for(const auto&p:Portals)
    if(unsigned(p.from)==region && !indoors(uint8_t(p.to)) &&
       std::abs(x-p.x)<=2 && std::abs(y-p.y)<=2)return true;
  return false;
}
Ground world_ground(const State &s, int x, int y) {
  Region r = Region(s.region);
  if(r==Region::Mine)return mine_ground(s,x,y);
  int wild=wild_tree_at(s,x,y);if(wild>=0&&s.wild_age[wild]!=7&&s.wild_age[wild]>=2)return Ground::Tree;
  if (indoors(s.region)) {
    if(furnished(s.region)) {
      if(x<7||x>24||y<5||y>20||home_solid(s.region,x,y))return Ground::Wall;
      return Ground::Path;
    }
    if(r==Region::SeedShop) {
      if(x<7||x>24||y<5||y>20 || (y==10&&x>=11&&x<=21) || (y>=6&&y<=7&&x>=8&&x<=23) || (y==16&&(x==8||x==23)))return Ground::Wall;
      return Ground::Path;
    }
    if (x < 7 || x > 24 || y < 5 || y > 20)
      return Ground::Wall;
    if ((x==15&&y==7)||(x==13&&y==5)||
        (x >= 9 && x <= 11 && y >= 7 && y <= 9) ||
        (x >= 20 && x <= 22 && y >= 5 && y <= 10) ||
        (x >= 18 && x <= 20 && y >= 14 && y <= 15) ||
        (x>=7&&x<=9&&y>=5&&y<=6) || ((x==17||x==21)&&y==16))
      return Ground::Wall;
    return Ground::Path;
  }
  if (x < 1 || y < 1 || x >= MapW - 1 || y >= MapH - 1)
    return Ground::Wall;
  if(building_solid(s.region,x,y))return Ground::Wall;
  if(y==1&&(x%2)&&!exit_clearance(s.region,x,y))return Ground::Wall;
  if (r == Region::Town) {
    if ((s.story >= 5 && x >= 14 && x <= 16 && y == 14) ||
        ((x == 16 || x == 18) && y == 10) || (y==10&&x>=10&&x<=12) || (y==8&&(x==5||x==10)))
      return Ground::Wall;
    if (y >= 17 && y <= 19 && x >= 3 && x <= 8)
      return Ground::Water;
    if (road_tile(s.region,x,y))
      return Ground::Path;
  } else if (r == Region::Forest) {
    if(x>=21&&x<=23&&y<=3&&!(x==22&&y==3))return Ground::Wall;
    int stream=y<6?4:y<11?5:y<15?4:y<19?3:4;
    if (x>=stream-1 && x<=stream+1) {
      if (y == 12)
        return s.story >= 3 ? Ground::Path : Ground::Water;
      return Ground::Water;
    }
    if (road_tile(s.region,x,y))
      return Ground::Path;
    if (x>=10&&x<=12&&y>=6&&y<=9 && !(x==10&&y==6) && !(x==12&&y==9))
      return Ground::Water;

  } else if (r == Region::Lake) {
    if(x>=21&&x<=23&&y>=11&&y<=12)return Ground::Path;
    if(s.friend_chapter[1]>=2&&y==14&&x>=25&&x<=27)return Ground::Wall;
    int left=y<8?7:y<12?4:y<17?3:6;
    int right=y<8?17:y<12?20:y<17?21:18;
    if (x>=left && x<=right && y>=4 && y<=20)
      return Ground::Water;
    if (road_tile(s.region,x,y))
      return Ground::Path;
  } else if (r == Region::Orchard) {

    if (x >= 23 && x <= 27 && y >= 10 && y <= 12) return Ground::Water;
    if (road_tile(s.region,x,y))
      return Ground::Path;
  }
  return Ground::Grass;
}
int portal_heading(const Portal &p) {
  if(p.from==Region::SeedShop&&p.to==Region::SeedHome)return 1;
  if(p.from==Region::SeedHome&&p.to==Region::SeedShop)return 3;
  if(p.from==Region::Mine)return 0;
  if (indoors(uint8_t(p.from))) return 2;
  if (indoors(uint8_t(p.to))) return 0;
  if (p.x <= 2) return 3;
  if (p.x >= 29) return 1;
  return p.y <= 2 ? 0 : 2;
}
int arrival_heading(const Portal &p) {
  for(const auto &back:Portals)if(back.from==p.to&&back.to==p.from)return (portal_heading(back)+2)%4;
  return portal_heading(p);
}
const Portal*portal_touch_at(const State&s,int x,int y) {
 if(const auto*p=portal_at(s,x/Tile,y/Tile))return p;
 // Side doors are painted above their floor tile. Tapping the visible frame
 // must choose the same threshold as tapping its bottom, not the tile behind it.
 for(const auto&p:Portals)if(unsigned(p.from)==s.region&&indoors(s.region)&&indoors(unsigned(p.to))){
  int d=portal_heading(p),cx=p.x*Tile+8,cy=p.y*Tile+8;
  if((d==1||d==3)&&x>=cx-7&&x<=cx+7&&y>=cy-24&&y<=cy+7)return &p;
 }
 if(indoors(s.region)||!road_tile(s.region,x/Tile,y/Tile))return nullptr;
 for(const auto&p:Portals)if(unsigned(p.from)==s.region&&!indoors(unsigned(p.to))){
  int d=portal_heading(p),dx=x-(p.x*Tile+8),dy=y-(p.y*Tile+8);
  int forward=d==1?dx:d==3?-dx:d==2?dy:-dy,lateral=d%2?dy:dx;
  if(forward>=-8&&forward<24&&std::abs(lateral)<=16)return &p;
 }
 return nullptr;
}
Point portal_approach(const Portal &p) {
  int d=portal_heading(p);
  return {p.x-(d==1)+(d==3),p.y-(d==2)+(d==0)};
}
bool crossed_portal(const Portal &p,int fx,int fy,int tx,int ty,int heading) {
  if(heading!=portal_heading(p))return false;
  int x=p.x*Tile+8,y=p.y*Tile+8;
  if(heading==0)return std::abs(tx-x)<=3 && fy>y && ty<=y;
  if(heading==2)return std::abs(tx-x)<=3 && fy<y && ty>=y;
  if(heading==1)return std::abs(ty-y)<=3 && fx<x && tx>=x;
  return std::abs(ty-y)<=3 && fx>x && tx<=x;
}
bool travel(State &s, const Portal &p) {
  if (unsigned(p.from) != s.region || !near(s, p.x, p.y) ||
      (p.bridge && s.story < 3) || !building_open(s,p.to))
    return false;
  if(p.to==Region::Mine)return enter_mine(s);
  if(p.from==Region::Mine){leave_mine(s);return true;}
  s.region = uint8_t(p.to);
  s.px = p.spawn_x * Tile + 8;
  s.py = p.spawn_y * Tile + 8;
  s.visited |= uint16_t(1u << s.region);
  return true;
}
int next_portal(const State &s, Region destination) {
  if (unsigned(destination) >= RegionCount || unsigned(destination) == s.region)
    return -1;
  int parent[RegionCount], edge[RegionCount], queue[RegionCount];
  std::fill(parent, parent + RegionCount, -1);
  int head = 0, tail = 0;
  queue[tail++] = s.region;
  parent[s.region] = s.region;
  while (head < tail) {
    int r = queue[head++];
    for (unsigned i = 0; i < sizeof(Portals) / sizeof(*Portals); ++i) {
      auto &p = Portals[i];
      int to = int(p.to);
      if (int(p.from) != r || parent[to] >= 0)
        continue;
      parent[to] = r;
      edge[to] = i;
      queue[tail++] = to;
    }
  }
  int at = int(destination);
  if (parent[at] < 0)
    return -1;
  while (parent[at] != s.region)
    at = parent[at];
  return edge[at];
}
#include "npc_routes.inc"
int npc_at(const State &s, int x, int y) {
  for (int id = 0; id < 4; ++id) {
    auto p = npc_pose(s, id);
    if (unsigned(p.region) == s.region && std::abs(p.x - (x * 16 + 8)) <= 16 &&
        std::abs(p.y - (y * 16 + 8)) <= 8)
      return id;
  }
  return -1;
}
bool chat(State &s, int id) {
  if (id < 0 || id >= 4)
    return false;
  s.met |= 1u << id;
  if (s.chat_day[id] == s.day)
    return false;
  s.chat_day[id] = s.day;
  s.bonds[id] = std::min(40, int(s.bonds[id]) + 2);
  return true;
}
bool gift(State &s, int id, int item) {
  if (id < 0 || id >= 4 || item < 8 || item > ItemCount || !item_count(s, item) ||
      s.gift_day[id] == s.day)
    return false;
  int value = (id == 0 && item >= 12 && item <= 15) ||
                      (id == 1 && (item == 17 || item == 18)) ||
                      (id == 2 && item == 16) ||
                      (id == 3 && (item >= 12 && item <= 15 || item >= 22)) ||
                      (id == 0 && (item == 19 || item == 23))
                  ? 6
                  : 3;
  change_item(s, item, -1);
  s.gift_day[id] = s.day;
  s.met |= 1u << id;
  constexpr int birthdays[]={7,12,19,25};
  if(season_of(s.day)==id&&season_day(s.day)==birthdays[id])value*=2;
  s.bonds[id] = std::min(40, int(s.bonds[id]) + value);
  sync_inventory(s);
  select_slot(s, s.active_slot == 255 ? -1 : s.active_slot);
  return true;
}
bool personal_event(State &s, int id) {
  if (id < 0 || id >= 4)
    return false;
  if (s.bonds[id] >= 10 && !(s.personal & (1u << id))) {
    s.personal |= 1u << id;
    return true;
  }
  return false;
}
#include "friend_stories.inc"
const char *npc_line(const State &s, int id, bool personal, bool en) {
  const char *stories[] = {"这袋老种子是母亲留下的。以前我怕种不好，总不敢打开"
                           "。看到你的农场，我想今年春天试试看。",
                           "父亲教我做的第一件东西，就是湖边那座桥。风雨毁了它"
                           "，我一直没敢再动手。谢谢你愿意一起修。",
                           "小时候爷爷带我在镜光湖看星星。后来桥断了，我只能在"
                           "林边练习。想和你再去看一次湖上的月亮。",
                           "以前每到收获季，大家都会带一道菜来到广场。人渐渐少"
                           "了，灯也收起来了。你让这里又有了盼头。"};
  const char *stories_en[] = {"Mother left me some old seeds. Your farm gave "
                              "me the courage to plant them.",
                              "Father taught me carpentry on that bridge. "
                              "Thank you for helping me rebuild it.",
                              "Grandpa and I watched stars by the lake. I'd "
                              "like to see its moonlight with you.",
                              "We used to share food in the square each "
                              "harvest. You brought hope back."};
  if (personal)
    return (en ? stories_en : stories)[id % 4];
  if(season_day(s.day)==14&&s.minute>=1020&&s.minute<1200) {
    const char *cn[]={"小禾：这是春天的种子交换会。把多余的种子留给邻居，也给自己带一份新的期待吧。", "阿岚：夏日的灯挂好了。澄说今晚有萤火，我特意留了湖边的座位。", "澄：秋日野餐的鱼汤好了！每个人带来的收获，都有不同的香气。", "林婆婆：冬天就围着炉火坐吧。我来讲旧山谷，你们来讲新一年的愿望。"};
    const char *eng[]={"Our spring seed exchange! Leave seeds for a neighbor, and take a little hope home.","The summer lanterns are hung. Cheng saved us seats to watch the fireflies.","The harvest picnic soup is ready. Every neighbor brought a different taste.","Come sit by the winter fire. I'll tell old valley stories; you tell next year's wishes."};
    return (en?eng:cn)[season_of(s.day)];
  }
  if(s.ranch_chapter>=5&&(s.day+id)%3==1) {
    const char *cn[]={"小鸡在新屋檐下躲雨，连叫声都安心了。记得添些草料呀。", "铁箍我又检查了一遍。旧梁还能陪你们过许多个雨季。", "傍晚从林边回来，远远就能看见你的牧场。那就是家的方向。", "灯亮起来的时候，就收好工具回家吧。小家伙们也在等你。"};
    const char *eng[]={"The chicks sound content under their new roof. Remember their hay.","I checked the iron braces again. Those old beams have many rainy seasons left.","Coming back from the woods, your shed shows me the way home.","When the lamp comes on, put your tools away. The little ones are waiting too."};
    if(id!=3||s.ranch_chapter>=6)return (en?eng:cn)[id];
  }
  if ((s.visited & (1u << unsigned(Region::Orchard))) && (s.day + id) % 3 == 0) {
    const char *cn[] = {"蜜风果园的苹果，配上浆果就能熬成果酱。记得给自己也留一瓶。",
        "林子东边那条路通往老果园。木牌重新立好了，别在树下迷路。",
        "果园的小池子也能钓鱼。带上热汤，忙完了就在水边歇歇吧。",
        "家里的灶台还好用吧？山菌和土豆煮一锅，干农活就有精神了。"};
    const char *eng[] = {"Apples and berries make lovely jam. Keep a jar for yourself.",
        "The east woodland path leads to the orchard. Follow the new sign.",
        "Try fishing in the orchard pond. Bring a warm stew for the walk.",
        "Mushrooms and a potato make a hearty stew at your cottage stove."};
    return (en ? eng : cn)[id];
  }
  if (s.story >= 5 && s.minute >= 1020) {
    const char *lines[] = {
        "我把那袋老种子种下去了。这盘小饼，第一份想留给你。谢谢你，让我又敢期待"
        "明年。",
        "桥修好了，长凳也做好了。往后镇上哪里需要搭把手，喊我一声就好。",
        "鱼汤再煮一会就好。今晚的星星倒映在碗里，好像镜光湖也来到了广场。",
        "人都到齐了，灯也亮了。你看，家不是一栋屋子，是愿意等你回来的人。"};
    return en ? "We saved a seat for you. Welcome home, friend."
              : lines[id % 4];
  }
  if (s.rain) {
    const char *a[] = {
        "雨天不用忙着浇水。来店里坐坐吧，我给你留了靠窗的位置。",
        "木料最怕受潮。我今天在镇上整理工具，等天晴了再进林子。",
        "雨水打在湖面时，鱼反而愿意靠近岸边。记得带好雨帽。",
        "这样的雨声，很像我年轻时。种子在土里慢慢醒来，人也别太着急。"};
    return en ? "Rain changes our plans. Take your time today." : a[id % 4];
  }
  if (s.minute >= 1080)
    return en ? "Evening already. Rest well, and see you tomorrow."
              : "天快黑了。今天也辛苦了，记得回农舍好好休息。";
  if (s.bonds[id] >= 20)
    return en ? "Seeing you makes the day brighter. You have a home here."
              : "见到你真好。晴露谷现在也有你的一份位置了。";
  const char *daily[] = {
      "我叫小禾，在镇上照料种子铺。别急着扩大农田，先把每一株小苗照顾好。",
      "我是阿岚。上午在工坊，午后去林间挑木料。那座旧桥，我想再试着修一修。",
      "叫我澄就好。桥没修好时，我划小船到林间这边。我的家就在湖岸，桥通了以后来坐坐。钓鱼可别急着收竿。",
      "大家叫我林婆婆。路走熟了，就多来广场坐坐吧。镇上的事，我们可以一起商量"
      "。"};
  const char *daily_en[] = {
      "I'm Xiaohe, the seed keeper. Care for each little seedling first.",
      "I'm Alan, the carpenter. Find me in the workshop or woods. Let's mend "
      "that bridge.",
      "Call me Cheng. I row to the woods while the bridge is broken. Visit my lake cabin once it is repaired.",
      "I'm Granny Lin. Visit the square often. We can help this village "
      "together."};
  return (en ? daily_en : daily)[id % 4];
}
const char *story_title(const State &s, bool en) {
  const char *cn[] = {"第一章 · 初到晴露",   "第二章 · 土地的回信",
                      "第三章 · 旧桥的约定", "第四章 · 湖上的星光",
                      "第五章 · 重聚的餐桌", "尾声 · 灯火长明"};
  const char *eng[] = {"A new beginning",  "The soil replies",
                       "The old bridge",   "Stars on the lake",
                       "A table together", "Lights of home"};
  return (en ? eng : cn)[std::min(5, int(s.story))];
}
const char *story_goal(const State &s, bool en) {
  const char *cn[] = {"结识四位村民，再找林婆婆聊聊。",
                      "累计收获 3 份作物，向小禾报喜。",
                      "带 12 木材和 9 石头找阿岚修桥。",
                      "到达镜光湖，带 3 条鱼交给澄。",
                      "与四人各达 1 心，带 4 份蔬菜找林婆婆。",
                      "傍晚到镇广场，与大家分享丰收。"};
  const char *eng[] = {"Meet all four villagers; talk to Granny Lin.",
                       "Harvest 3 crops; tell Xiaohe.",
                       "Bring Alan 12 wood and 9 stone.",
                       "Visit the lake; bring Cheng 3 fish.",
                       "One heart with each friend; bring Lin 4 crops.",
                       "Visit the town square after 17:00."};
  return (en ? eng : cn)[std::min(5, int(s.story))];
}
const char *story_line(const State &s, bool en) {
  const char *cn[] = {
      "欢迎来到晴露谷。镇上的四个人，你都见过了吗？每个人都在等一个新的开始。",
      "小禾把一袋种子放进你的手心：土地记住了你的照料。等桥修好，我们一起去湖边"
      "吧。",
      "阿岚展开一张旧图纸：木头和石料够了，就能把断桥连起来。那边曾是大家最喜欢"
      "的地方。",
      "澄望向重新开放的小径：原来这段路还记得我们。湖上的星光，终于又能亲眼看到"
      "了。",
      "林婆婆从箱子里取出旧灯串：我们再摆一次长桌吧。带上蔬菜，也把你交到的朋友"
      "叫来。",
      "傍晚，广场上的灯一盏盏亮起来。小禾带来种子饼，阿岚搬好长凳，澄煮起鱼汤。"
      "你终于也有了自己的归处。"};
  return en ? story_goal(s, true) : cn[std::min(5, int(s.story))];
}
const char *story_completion(int chapter, bool en) {
  const char *cn[] = {
      "",
      "大家都认识你了。先种下第一季的希望吧，晴露谷欢迎你。",
      "这些补种的种子送给你。你的第一份收获，让我也有了重新打开老种子袋的勇气"
      "。",
      "木头和石料都够了！我这就把旧桥修好。沿林间小径往西，就能到镜光湖了。",
      "鱼真新鲜，谢谢你！这几条我先留着做鱼汤。找林婆婆商量一下，我们再摆一次长"
      "桌吧。",
      "蔬菜收好了，大家也都答应来。傍晚五点到镇广场吧，我会把那串旧灯重新挂起来"
      "。"};
  const char *eng[] = {
      "",
      "Everyone knows you now. Welcome to Dewfield!",
      "These seeds are for you. Your first harvest gave me courage too.",
      "The materials are enough! The bridge is repaired. Follow the woods west "
      "to the lake.",
      "Fresh fish! I'll make soup. Ask Granny Lin about a table we can share.",
      "Everyone is coming. Meet us in the town square after 17:00. I'll hang "
      "the old lights."};
  return (en ? eng : cn)[std::clamp(chapter, 0, 5)];
}
bool advance_story(State &s, int id) {
  bool ok = false;
  switch (s.story) {
  case 0:
    ok = id == 3 && s.met == 15;
    break;
  case 1:
    ok = id == 0 && s.harvested >= 3;
    break;
  case 2:
    ok = id == 1 && s.wood >= 12 && s.stone >= 9;
    if (ok) {
      s.wood -= 12;
      s.stone -= 9;
    }
    break;
  case 3:
    ok = id == 2 && (s.visited & (1u << int(Region::Lake))) && s.fish >= 3;
    if (ok)
      s.fish -= 3;
    break;
  case 4: {
    unsigned crops = 0;
    for (auto n : s.produce)
      crops += n;
    ok = id == 3 && crops >= 4;
    for (auto n : s.bonds)
      ok = ok && n >= 10;
    if (ok) {
      int left = 4;
      for (auto &n : s.produce) {
        int take = std::min(left, int(n));
        n -= take;
        left -= take;
      }
    }
    break;
  }
  default:
    break;
  }
  if (!ok)
    return false;
  ++s.story;
  s.gold = std::min(999999u, s.gold + uint32_t(40 + s.story * 20));
  if (s.story == 2)
    s.seeds[0] = std::min(999, int(s.seeds[0]) + 6);
  sync_inventory(s);
  return true;
}
bool forage_available(const State &s, int id) {
  return id >= 0 && unsigned(id) < sizeof(Foraging) / sizeof(*Foraging) &&
      !(s.forage & (1u << id)) &&
      (Foraging[id].seasons & (1u << season_of(s.day))) &&
      (Foraging[id].region!=Region::Orchard||fruit_tree_ready(s,Foraging[id].x,Foraging[id].y));
}
int forage_at(const State &s, int x, int y) {
  for (unsigned i = 0; i < sizeof(Foraging) / sizeof(*Foraging); ++i) {
    auto &f = Foraging[i];
    if (unsigned(f.region) == s.region && f.x == x && f.y == y &&
        forage_available(s, i))
      return i;
  }
  return -1;
}
bool collect_forage(State &s, int id) {
  if (!forage_available(s, id))
    return false;
  auto &f = Foraging[id];
  if (unsigned(f.region) != s.region || !near(s, f.x, f.y))
    return false;
  unsigned count = item_count(s, f.item);
  if (count >= 999)
    return false;
  unsigned amount = f.item == 17 ? 3 : f.item == 18 || f.item >= 19 ? 2 : 1;
  if (!change_item(s, f.item, std::min(amount, 999 - count))) return false;
  s.forage |= 1u << id;
  sync_inventory(s);
  return true;
}
} // namespace farm
