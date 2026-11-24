#pragma once

namespace tide {

enum PortStoryId { TideSkiff, LastSugar, TornChart, TwoLetters, WetPorcelain, TideCopy, SampleWeights, SpareMast, PortStoryCount };
enum PortStoryStage : std::uint8_t { PortStoryUnknown, PortStoryMet, PortStoryFirst, PortStorySecond };
struct PortStory {
    int port;
    Text title, person, invitation;
    Text opening;
    std::array<Text,2> choices, effects, endings;
    int dispatch=-1;
};

inline constexpr PortStory PortStories[]{
    {lisbon,"赶潮的小艇","船工托梅","找船工",
     "托梅的小艇漏水，潮又快退了。他缺一件木料，也缺人手把艇拖上岸。",
     {"给一件木料","借船员帮忙"},
     {"54金 · 声望+2","28金 · 士气-8"},
     {"托梅钉好新船板，小艇赶在退潮前出了塔霍河口。他把这趟的工钱分给你，也把你的名字告诉了渔船伙计。",
      "船员帮托梅把艇拖上岸，累得肩膀发酸。托梅付了工钱，让他们在船棚歇脚，等下一轮潮。"}},
    {madeira,"最后两袋糖","糖商伊莎贝尔","找糖商",
     "买家没来，伊莎贝尔还剩两袋糖。你想低价买走，还是帮她叫来另一条船？",
     {"买下两袋糖","叫那条商船"},
     {"付20金 · 蔗糖+2","18金 · 声望+1"},
     {"两袋糖过了秤，装进了你的船舱。伊莎贝尔收拾起空麻袋，说明天一早还有新糖。",
      "商船的管事回头把糖买走。伊莎贝尔给你十八金作谢，她也记住了你这条船。"}},
    {canaries,"缺了角的海图","领航员马丽玛","找领航员",
     "马丽玛用葡语说，圣地亚哥岛南岸有个泊地。可付钱抄图，也可沿河谷自己找。",
     {"抄下靠泊图","记下大致海域"},
     {"付28金 · 定位泊地","免费 · 自己找"},
     {"里贝拉格兰德的泊地已画上海图。可以从目的地清单选择这座港口。",
      "你记下了大致航向。马丽玛提醒你，退潮时能看清河口两边的礁。"}},
    {cadiz,"两封引荐信","商会管事伊内斯","找管事",
     "伊内斯递来两封信。大商号愿出钱求引荐，三位小船主想借你的名字共写一封。",
     {"引荐大商号","替小船署名"},
     {"84金","声望+4 · 信任+2"},
     {"大商号封好信，当面付清八十四金。小船的船主收起空信封，打算直接去外港试试。",
      "三位小船主把名字写在你旁边。他们约好结伴出湾，到外港时也好替彼此说句话。"}},
    {malacca,"潮湿的瓷箱","瓷商伊斯迈尔","找瓷商",
     "伊斯迈尔撩起一角潮湿的草垫。他想托你经泉州把瓷送去那霸，也缺一块干布给留港的货换垫。",
     {"接下瓷箱运单","留一件织锦"},
     {"押45金 · 经泉州","织锦-1 · 信任+2"},
     {"他把箱号写进运单，托你在泉州看一眼草垫。","你留下干布。伊斯迈尔换好垫子，把箱子移到仓屋的高处。"},FirstStoryDispatch},
    {naha,"迟来的潮表","船商阿真","找船商",
     "一页潮表被海水溅湿了。阿真想寄给宁波的接货人，或留在港里，请你帮她另抄一份。",
     {"送潮表去宁波","留下帮忙抄"},
     {"押18金 · 经泉州","耗一日 · 声望+3"},
     {"阿真把抄件装进布套，嘱你在泉州看看还有没有字洇开。","你们对着旧表抄了一天。日落前，阿真把清楚的一份借给了候潮的船工。"},FirstStoryDispatch+1},
    {ningbo,"两边一样重","管货的徐禾","找管货人",
     "徐禾在两本账上看见了不同的斤数。他想请你经博多送一套样秤去堺，或先给码头留一笔校秤钱。",
     {"接下样秤运单","付钱校秤"},
     {"押45金 · 经博多","付18金 · 信任+2"},
     {"徐禾把秤装进箱，让你在博多对过秤再往东走。","你付了工钱，徐禾当着几条船的管事重新校秤。原先争吵的人也来帮着举秤杆。"},FirstStoryDispatch+2},
    {sakai,"渡海的桅木","船匠弥兵卫","找船匠",
     "弥兵卫挑出几根直木，要送给釜山浦的船工。他还缺一件木料，可以先修好本港的小艇。",
     {"接下桅木运单","给一件木料"},
     {"押45金 · 经博多","木料-1 · 声望+3"},
     {"弥兵卫在绳结上系了布条。他说，到了博多，看看桅木有没有被绳勒出印。","你把木料留在棚里。弥兵卫锯下合用的一段，剩下的也给邻船修了条护舷。"},FirstStoryDispatch+3},
};
static_assert(std::size(PortStories)==PortStoryCount && PortStoryCount*2<=16);
static_assert(DispatchCount-FirstStoryDispatch==PortStoryCount-WetPorcelain);

inline constexpr int story_for_dispatch(int id) {
    return id>=FirstStoryDispatch && id<DispatchCount?WetPorcelain+id-FirstStoryDispatch:-1;
}

inline int port_story_at(int port) {
    for (int i=0;i<PortStoryCount;++i)
        if (PortStories[i].port==port) return i;
    return -1;
}

} // namespace tide
