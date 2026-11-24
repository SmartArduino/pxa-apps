#pragma once

namespace tide {

struct Rumor {
    int port, site;
    Point area;
    Text speaker, clue;
};

inline constexpr Rumor Rumors[] {
    {lisbon,azores,{147+LegacyMapX,273+LegacyMapY},"返航舵手","往西过大洋，和塔霍河口差不多的纬度，有些黑色火山岛。我们回里斯本前在那儿躲过风。"},
    {bordeaux,bristol,{282+LegacyMapX,157+LegacyMapY},"羊毛商","越过比斯开湾向北，在英格兰西面的长海湾里找一条河。沿河往里就是卖羊毛的布里斯托尔。"},
    {bristol,bruges,{346+LegacyMapX,172+LegacyMapY},"织工","向东穿过海峡，佛兰德斯低岸上有条水道通向布鲁日。见到大片沙洲就知道离得不远了。"},
    {cadiz,gibraltar,{285+LegacyMapX,287+LegacyMapY},"渡船家","出加的斯湾往东南，见到两岸靠得很近，就是海峡。北边有块高岩，南边是非洲。再往东就是地中海。"},
    {cadiz,tangier,{274+LegacyMapX,302+LegacyMapY},"鱼贩","往加的斯湾东南，到海峡对岸找丹吉尔。那边的船喜欢在风小的时候靠岸。"},
    {tangier,valencia,{309+LegacyMapX,271+LegacyMapY},"染料商","过窄口后沿伊比利亚东岸向北。能看见岛屿的那段海岸上，有收陶器和丝绸的瓦伦西亚。"},
    {tangier,tunis,{380+LegacyMapX,291+LegacyMapY},"商船水手","沿北非海岸向东走，过几个大海湾。在西西里西南方找个能通潟湖的海湾，那里就是突尼斯。"},
    {valencia,genoa,{381+LegacyMapX,230+LegacyMapY},"布商","向东北走，别绕到意大利东岸。山脚下有片开向南边的海湾，热那亚就夹在山和海之间。"},
    {tunis,venice,{402+LegacyMapX,224+LegacyMapY},"盐船舵手","绕到意大利东边。向北走到长海湾的尽头，浅水沙洲里面藏着威尼斯。"},
    {tunis,alexandria,{516+LegacyMapX,339+LegacyMapY},"香料商","从突尼斯向东走，到埃及北岸。尼罗河口那片三角洲的西边，就是买香料的亚历山大港。"},
    {canaries,arguin,{189+LegacyMapX,404+LegacyMapY},"渔夫","驶向东南，看见撒哈拉西缘时，留心海面忽然变浅。沙洲上常有鸟群，附近是阿尔金。"},
    {canaries,ribeira,{143+LegacyMapX,447+LegacyMapY},"糖商","从大加那利向西南，海上有一串岛。圣地亚哥岛南面有河谷和补给泊地，叫里贝拉格兰德。"},
    {ribeira,elmina,{293+LegacyMapX,546+LegacyMapY},"小船船主","到几内亚湾北岸留意那些穿过浪头的小船。顺着它们往岸边看，就能找到埃尔米纳。"},
    {elmina,mpinda,{394+LegacyMapX,617+LegacyMapY},"河口领航员","穿过几内亚湾向东南。看到大片浑水伸进大海，就快到刚果河口的姆平达了。"},
    {kilwa,malindi,{607+LegacyMapX,608+LegacyMapY},"季风舵手","沿珊瑚海岸往北。沙滩外的礁盘有缝，小船能从那里进去马林迪。"},
    {bristol,bergen,chart_point(4.8f,60.5f),"干鱼商","越过北海，挪威西岸的群岛后有一座深湾。水边成排的木仓就是卑尔根。"},
    {bergen,vardo,chart_point(30.5f,70.5f),"渔船船主","沿挪威海岸绕过北角，再往东找一座离岸小岛。瓦尔德面朝巴伦支海。"},
    {aden,muscat,chart_point(58.7f,23.7f),"阿曼船商","循阿拉伯半岛海岸向东北，越过哈德角。岩山之间的深水港便是马斯喀特。"},
    {muscat,hormuz,chart_point(56.6f,27.f),"盐船舵手","沿阿曼湾向西北驶进海峡。北岸附近有个红褐色的岛，岛上是霍尔木兹。"},
    {malindi,calicut,chart_point(75.5f,11.3f),"胡椒商","等西南季风，横渡阿拉伯海。对岸的马拉巴尔海岸有一座卖棉布和胡椒的大港。"},
    {canaries,santo_domingo,chart_point(-69.9f,18.3f),"远洋船工","借信风一路向西。伊斯帕尼奥拉岛南岸有条叫奥萨马的河，河口附近新建了聚落。"},
};
inline constexpr int RumorCount=std::size(Rumors);
static_assert(RumorCount<=32);
inline constexpr int ArguinRumor=10, RibeiraRumor=11;
static_assert(Rumors[ArguinRumor].port==canaries && Rumors[ArguinRumor].site==arguin);
static_assert(Rumors[RibeiraRumor].port==canaries && Rumors[RibeiraRumor].site==ribeira);

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
    "只听懂几个词：沿岸向北、礁盘开口。",
    "只听懂几个词：北海、木仓。","只听懂几个词：北角、岛。","只听懂几个词：岩山、向东北。",
    "只听懂几个词：海峡、红岛。","只听懂几个词：季风、胡椒。","只听懂几个词：向西、河口。"
};
static_assert(std::size(RumorFragments)==RumorCount);

inline constexpr auto StartingChart=place_bit(lisbon)|place_bit(cadiz)|place_bit(madeira)|
                                    place_bit(canaries)|place_bit(bordeaux);
inline constexpr int ChartCell=16;
inline constexpr int ChartColumns=(ChartWidth+ChartCell-1)/ChartCell;
inline constexpr int ChartRows=(ChartHeight+ChartCell-1)/ChartCell;
inline constexpr int PreviousChartWidth=1540, PreviousChartHeight=1136, PreviousPlaceCount=35;
inline constexpr int PreviousChartColumns=(PreviousChartWidth+ChartCell-1)/ChartCell;
inline constexpr int PreviousChartRows=(PreviousChartHeight+ChartCell-1)/ChartCell;
inline constexpr int ChartBytes=(PreviousChartColumns*PreviousChartRows+7)/8;
inline constexpr int OuterChartColumns=(ChartColumns+1)/2, OuterChartRows=(ChartRows+1)/2;
inline constexpr int OuterChartBytes=(OuterChartColumns*OuterChartRows+7)/8;

} // namespace tide
