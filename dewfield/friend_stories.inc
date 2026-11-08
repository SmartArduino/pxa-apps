// SPDX-License-Identifier: MIT
// Included inside farm. These original scenes connect the four villagers.
bool friend_story_ready(const State&s,int npc) {
  if(npc<0||npc>=NpcCount||s.friend_chapter[npc]>=2)return false;
  constexpr int companions[]={3,2,1,0};
  return s.bonds[npc]>=(s.friend_chapter[npc]?30:20)&&
    (!s.friend_chapter[npc]||(s.story>=3&&s.bonds[companions[npc]]>=10));
}
bool friend_story(State&s,int npc) {
  if(!friend_story_ready(s,npc))return false;
  ++s.friend_chapter[npc];s.bonds[npc]=std::min(40,int(s.bonds[npc])+2);
  s.gold=std::min(999999u,s.gold+50u*s.friend_chapter[npc]);return true;
}
const char *friend_story_line(int npc,int chapter,bool en) {
  const char *cn[][2]={
    {"小禾：母亲留下的种子终于发芽了。婆婆教我记下播种的日子，阿岚替我做花箱。我想把第一株放在店门口，告诉路过的人：春天会回来。", "小禾：你和婆婆都说花开得好。我在花箱上写下大家的名字。种子铺门前从今天起，就有属于我们的颜色了。下个春天，也一起播种吧。"},
    {"阿岚：澄小时候总把钓线挂在我的木料上。桥断后，他不再来找我。我不是怕修不好，是怕再让朋友失望。谢谢你陪我把那块旧木拿起来。", "阿岚：澄来帮我磨平最后一块木板了。我把长凳放在湖边，留两个座位。一个给老朋友，一个给总能把大家重新叫到一起的你。"},
    {"澄：这张没寄出的明信片，是写给爷爷的。以前觉得离开湖岸，记忆就会散。小禾说，种子换块土地也会发芽。我想明天去镇上看看。", "澄：阿岚说长凳做好了，我答应带鱼汤。他仍记得我不喜欢太咸。原来有些朋友，只是等一句好久不见。你也一起来坐坐吧。"},
    {"林婆婆：会馆那张空椅子，我一直没舍得收。小禾来添了花，阿岚修了桌脚，澄端来一碗汤。今天我才懂，想念可以和新的日子坐在一起。", "林婆婆：我把你的名字写进谷里的记事本，也把下一页留白。那不是一个人的农场了。等下次季节聚会，我们一起商量明年种什么吧。"}
  };
  const char *eng[][2]={
    {"Xiaohe: Mother's seeds sprouted! Lin kept the dates; Alan made a flower box. Our first seedling will greet everyone outside the shop.","Xiaohe: I wrote our names on the flower boxes. The shop wears our colors now. Let's plant together again next spring."},
    {"Alan: Cheng's fishing line always tangled in my timber. After the bridge broke, we stopped talking. You helped me pick up that old plank again.","Alan: Cheng helped sand the last board. I put a bench by the lake: a seat for an old friend, and one for you. Come sit with us."},
    {"Cheng: This unsent postcard is for Grandpa. Xiaohe says seeds can grow in new soil too. Tomorrow, I think I'll visit town.","Cheng: Alan's bench is ready. I'll bring fish soup. He still remembers how I like it. Some friendships only need a hello. Join us by the lake."},
    {"Lin: I kept an empty chair. Xiaohe brought flowers, Alan fixed the table, Cheng brought soup. Memories can sit beside new days.","Lin: Your name is in our valley book; the next page is blank. At our seasonal gathering, let's decide what to plant next year."}
  };
  return (en?eng:cn)[std::clamp(npc,0,3)][std::clamp(chapter-1,0,1)];
}
