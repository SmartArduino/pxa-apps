#pragma once

namespace tide {

struct Rumor {
    int port, site;
    Point area;
    Text speaker, clue;
};

inline constexpr Rumor Rumors[] {
    {lisbon,azores,{147,273},"返航舵手","往西过大洋，和塔霍河口差不多的纬度，有些黑色火山岛。我们回里斯本前在那儿躲过风。"},
    {bordeaux,bristol,{282,157},"羊毛商","越过比斯开湾向北，在英格兰西面的长海湾里找一条河。沿河往里就是卖羊毛的布里斯托尔。"},
    {bristol,bruges,{346,172},"织工","向东穿过海峡，佛兰德斯低岸上有条水道通向布鲁日。见到大片沙洲就知道离得不远了。"},
    {cadiz,gibraltar,{285,287},"渡船家","出加的斯湾往东南，见到两岸靠得很近，就是海峡。北边有块高岩，南边是非洲。再往东就是地中海。"},
    {cadiz,tangier,{274,302},"鱼贩","往加的斯湾东南，到海峡对岸找丹吉尔。那边的船喜欢在风小的时候靠岸。"},
    {tangier,valencia,{309,271},"染料商","过窄口后沿伊比利亚东岸向北。能看见岛屿的那段海岸上，有收陶器和丝绸的瓦伦西亚。"},
    {tangier,tunis,{380,291},"商船水手","沿北非海岸向东走，过几个大海湾。在西西里西南方找个能通潟湖的海湾，那里就是突尼斯。"},
    {valencia,genoa,{381,230},"布商","向东北走，别绕到意大利东岸。山脚下有片开向南边的海湾，热那亚就夹在山和海之间。"},
    {tunis,venice,{402,224},"盐船舵手","绕到意大利东边。向北走到长海湾的尽头，浅水沙洲里面藏着威尼斯。"},
    {tunis,alexandria,{516,339},"香料商","从突尼斯向东走，到埃及北岸。尼罗河口那片三角洲的西边，就是买香料的亚历山大港。"},
    {canaries,arguin,{189,404},"渔夫","驶向东南，看见撒哈拉西缘时，留心海面忽然变浅。沙洲上常有鸟群，附近是阿尔金。"},
    {canaries,ribeira,{143,447},"糖商","从大加那利向西南，海上有一串岛。圣地亚哥岛南面有河谷和补给泊地，叫里贝拉格兰德。"},
    {ribeira,elmina,{293,546},"小船船主","到几内亚湾北岸留意那些穿过浪头的小船。顺着它们往岸边看，就能找到埃尔米纳。"},
    {elmina,mpinda,{394,617},"河口领航员","穿过几内亚湾向东南。看到大片浑水伸进大海，就快到刚果河口的姆平达了。"},
    {kilwa,malindi,{607,608},"季风舵手","沿珊瑚海岸往北。沙滩外的礁盘有缝，小船能从那里进去马林迪。"},
};
inline constexpr int RumorCount=std::size(Rumors);
static_assert(RumorCount<=32);

inline constexpr Text RumorFragments[]{
    "只听懂几个词：往西、黑色石头。",
    "只听懂几个词：向北、羊毛、河。",
    "只听懂几个词：向东、沙洲。",
    "只听懂几个词：向东南、两岸、高岩。",
    "只听懂几个词：向东南、对岸、候风。",
    "只听懂几个词：沿东岸向北、陶器、丝绸。",
    "只听懂几个词：沿北非向东、有潟湖。",
    "只听懂几个词：东北、山脚、朝南的海湾。",
    "只听懂几个词：绕到东面、长海湾。",
    "只听懂几个词：向东、三角洲、香料。",
    "只听懂几个词：向东南、浅滩、水鸟。",
    "只听懂几个词：西南方的岛、南岸河谷。",
    "只听懂几个词：向东、小舟、浪后。",
    "只听懂几个词：东南、大河、浑水。",
    "只听懂几个词：沿岸向北、礁盘开口。"
};
static_assert(std::size(RumorFragments)==RumorCount);

inline constexpr std::uint32_t StartingChart=(1U<<lisbon)|(1U<<cadiz)|(1U<<madeira)|
                                             (1U<<canaries)|(1U<<bordeaux);
inline constexpr int ChartCell=12;
inline constexpr int ChartColumns=(ChartWidth+ChartCell-1)/ChartCell;
inline constexpr int ChartRows=(ChartHeight+ChartCell-1)/ChartCell;
inline constexpr int ChartBytes=(ChartColumns*ChartRows+7)/8;

} // namespace tide
