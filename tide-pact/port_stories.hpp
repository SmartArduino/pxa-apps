#pragma once

namespace tide {

enum PortStoryId { TideSkiff, LastSugar, TornChart, TwoLetters, PortStoryCount };
enum PortStoryStage : std::uint8_t { PortStoryUnknown, PortStoryMet, PortStoryFirst, PortStorySecond };
struct PortStory {
    int port;
    Text title, person, invitation;
    Text opening;
    std::array<Text,2> choices, effects, endings;
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
};
static_assert(std::size(PortStories)==PortStoryCount && PortStoryCount==4);

inline int port_story_at(int port) {
    for (int i=0;i<PortStoryCount;++i)
        if (PortStories[i].port==port) return i;
    return -1;
}

} // namespace tide
