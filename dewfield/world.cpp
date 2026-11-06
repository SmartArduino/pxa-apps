// SPDX-License-Identifier: MIT
#include "world.hpp"
#include <algorithm>
#include <cstdlib>
namespace farm {
const char *region_name(Region r, bool en) {
  const char *cn[] = {"晴露农场", "晴露镇", "林间小径", "镜光湖畔", "温暖农舍"};
  const char *eng[] = {"Dewfield Farm", "Dewfield Town", "Whisper Woods",
                       "Mirror Lake", "Farm Cottage"};
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
Ground world_ground(const State &s, int x, int y) {
  Region r = Region(s.region);
  if (r == Region::Home) {
    if (x < 7 || x > 24 || y < 5 || y > 20)
      return Ground::Wall;
    if ((x >= 9 && x <= 11 && y >= 7 && y <= 9) ||
        (x >= 20 && x <= 22 && y >= 7 && y <= 10) ||
        (x >= 18 && x <= 20 && y >= 14 && y <= 15))
      return Ground::Wall;
    return Ground::Path;
  }
  if (x < 1 || y < 1 || x >= MapW - 1 || y >= MapH - 1)
    return Ground::Wall;
  if (r == Region::Town) {
    if ((s.story >= 5 && x >= 14 && x <= 16 && y == 14) || (x == 16 && y == 10))
      return Ground::Wall;
    if (((x >= 5 && x <= 9) || (x >= 22 && x <= 26)) && y >= 3 && y <= 6)
      return Ground::Wall;
    if (y >= 17 && y <= 19 && x >= 3 && x <= 8)
      return Ground::Water;
    if (y == 12 || x == 15 || y == 7 ||
        (x >= 12 && x <= 18 && y >= 9 && y <= 15))
      return Ground::Path;
  } else if (r == Region::Forest) {
    if (x >= 3 && x <= 4 && y >= 5 && y <= 20) {
      if (y == 12)
        return s.story >= 3 ? Ground::Path : Ground::Wall;
      return Ground::Water;
    }
    if (x == 15 || y == 12 || y == 3)
      return Ground::Path;
    if (x >= 11 && x <= 12 && y >= 5 && y <= 20)
      return Ground::Water;
    if ((x % 6 == 0 && x != 12 && y % 5 == 0) || (x == 22 && y == 6))
      return Ground::Tree;
  } else if (r == Region::Lake) {
    if (x >= 4 && x <= 20 && y >= 5 && y <= 19)
      return Ground::Water;
    if (y == 12 || x == 24)
      return Ground::Path;
  }
  return Ground::Grass;
}
bool travel(State &s, const Portal &p) {
  if (unsigned(p.from) != s.region || !near(s, p.x, p.y) ||
      (p.bridge && s.story < 3))
    return false;
  s.region = uint8_t(p.to);
  s.px = p.spawn_x * Tile + 8;
  s.py = p.spawn_y * Tile + 8;
  s.visited |= uint8_t(1u << s.region);
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
NpcPose npc_pose(const State &s, int id) {
  // Fixed daily stations and short, collision-checked walks. No per-frame BFS.
  Region r = Region::Town;
  int x = 13, y = 10;
  if (id == 0) {
    x = 10;
    y = 7;
    if (s.minute >= 1020) {
      x = 17;
      y = 12;
    }
  }
  if (id == 1) {
    x = 21;
    y = 7;
    if (s.minute >= 660 && s.minute < 1020 && !s.rain) {
      r = Region::Forest;
      x = 17;
      y = 12;
    }
  }
  if (id == 2) {
    x = 19;
    y = 12;
    if (!s.rain && s.minute < 1080) {
      r = s.story >= 3 ? Region::Lake : Region::Forest;
      x = r == Region::Lake ? 23 : 6;
      y = 12;
    }
  }
  if (id == 3) {
    x = 14;
    y = 12;
    if (s.minute >= 960) {
      x = 18;
      y = 7;
    }
  }
  if (s.story >= 5 && s.minute >= 1020) {
    r = Region::Town;
    x = 12 + id * 2;
    y = 13;
  }
  int phase = (s.minute + id * 5) % 32, offset = phase < 8    ? phase
                                                 : phase < 16 ? 8
                                                 : phase < 24 ? 24 - phase
                                                              : 0;
  // Adjacent straight steps stay on a safe walkway even when stations change.
  State probe = s;
  probe.region = uint8_t(r);
  bool moving = (phase < 8 || phase >= 16 && phase < 24) &&
                world_ground(probe, x + 1, y) != Ground::Wall &&
                world_ground(probe, x + 1, y) != Ground::Water;
  return {r,
          x * 16 + 8 +
              (moving                     ? offset
               : phase >= 8 && phase < 16 ? 8
                                          : 0),
          y * 16 + 12, moving ? (phase < 8 ? 1 : 3) : 2, moving};
}
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
  if (id < 0 || id >= 4 || item < 8 || item > 18 || !item_count(s, item) ||
      s.gift_day[id] == s.day)
    return false;
  int value = (id == 0 && item >= 12 && item <= 15) ||
                      (id == 1 && (item == 17 || item == 18)) ||
                      (id == 2 && item == 16) ||
                      (id == 3 && item >= 12 && item <= 15)
                  ? 6
                  : 3;
  if (item <= 11)
    --s.seeds[item - 8];
  else if (item <= 15)
    --s.produce[item - 12];
  else if (item == 16)
    --s.fish;
  else if (item == 17)
    --s.wood;
  else
    --s.stone;
  s.gift_day[id] = s.day;
  s.met |= 1u << id;
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
      "叫我澄就好。桥没修好以前，我在林子里等。湖畔的鱼很机灵，可别急着收竿。",
      "大家叫我林婆婆。路走熟了，就多来广场坐坐吧。镇上的事，我们可以一起商量"
      "。"};
  const char *daily_en[] = {
      "I'm Xiaohe, the seed keeper. Care for each little seedling first.",
      "I'm Alan, the carpenter. Find me in the workshop or woods. Let's mend "
      "that bridge.",
      "Call me Cheng. I wait in the woods until the lake bridge is repaired.",
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
int forage_at(const State &s, int x, int y) {
  for (unsigned i = 0; i < sizeof(Foraging) / sizeof(*Foraging); ++i) {
    auto &f = Foraging[i];
    if (unsigned(f.region) == s.region && f.x == x && f.y == y &&
        !(s.forage & (1u << i)))
      return i;
  }
  return -1;
}
bool collect_forage(State &s, int id) {
  if (id < 0 || unsigned(id) >= sizeof(Foraging) / sizeof(*Foraging) ||
      s.forage & (1u << id))
    return false;
  auto &f = Foraging[id];
  if (unsigned(f.region) != s.region || !near(s, f.x, f.y))
    return false;
  unsigned count = item_count(s, f.item);
  if (count >= 999)
    return false;
  if (f.item == 17)
    s.wood = std::min(999, int(s.wood) + 3);
  else if (f.item == 18)
    s.stone = std::min(999, int(s.stone) + 2);
  else if (f.item == 16)
    ++s.fish;
  else
    ++s.seeds[f.item - 8];
  s.forage |= 1u << id;
  sync_inventory(s);
  return true;
}
} // namespace farm
