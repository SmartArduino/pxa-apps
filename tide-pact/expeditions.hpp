#pragma once

namespace tide {

enum ExpeditionId { WesternReturn, BirdsAtLowWater, TwoShores, ColdCurrent, ExpeditionCount };
enum ExpeditionStage { UnknownClue, ReadClue, AtSeaSearch, HasEvidence, FiledReport };
enum ExpeditionFlag { HeardWitness=1, Sponsored=2, FirstObserver=4 };

struct Expedition {
    int archive, site, race_days, advance, purse, method;
    Text title, archivist, book, witness, scene, options[2], retry, evidence;
};

inline constexpr Expedition Expeditions[] {
    {lisbon,azores,20,110,430,0,"西风归途","抄图师贝娅",
     "《返航手记》里画了一串岛。从马德拉向西北走，到了和葡萄牙差不多的纬度，就能去找这串岛和书里说的西风。",
     "老舵手指着亚速尔群岛说，到外海看帆往哪边鼓。穿过岩口的风变得快，别在那里记。",
     "浪撞上两旁的黑岩。岩口的风一会儿往这边吹，一会儿往那边吹。外海的浮沫却一直朝同一边漂。",
     {"在外海记风","到岩口看风"},"岩口的风又转了。在这里看不出外海的风向。",
     "外海的西风有时能送船东返。回程可以走更北的海路。贝娅把这条路线抄给等船的家人。"},
    {madeira,arguin,27,130,520,1,"潮退时的白翅","船医特蕾莎",
     "《南下鸟记》里夹着一根白羽。画鸟的那页写着，浅滩在撒哈拉西岸、佛得角东北，要从大加那利岛往南找。",
     "水手说，那是阿尔金。等退潮露出沙洲，在船上就能看见鸟。划艇靠近会惊飞它们。",
     "潮一退，鸟群就落在沙洲上找吃的。只要画下长喙和脚印，就能补全旧手稿。",
     {"划艇去追","退潮后远望"},"船桨刚碰到水，鸟就全飞了。",
     "水鸟迁徙途中会来阿尔金觅食。特蕾莎在航图上分开标了沙洲和观测船停的位置。"},
    {cadiz,gibraltar,14,75,310,1,"两岸的水痕","领航员露西亚",
     "《两岸渡船记》写道，加的斯东南有道海峡，两岸的峭壁隔水相望。放下木片，它总先漂向地中海。",
     "渡船家说，那是直布罗陀海峡。到中段放木片，记下它往哪边漂。港湾里的回流容易看错。",
     "北边是伊比利亚，南边是北非。岸边的水在打转，海峡中间的浮沫却慢慢向东漂。",
     {"在港湾放浮标","在海峡放浮标"},"浮标只在岸边打转。得去海峡中间再试。",
     "直布罗陀海峡的表层水总体向东，较深处有水流向大西洋。露西亚把表层的流向标在渡船图上。"},
    {elmina,benguela,42,190,760,0,"雾外的寒流","制图师科菲",
     "《南岸雾记》记着，从刚果河口向南走，还没到好望角，舀上来的海水就凉了许多。清早，雾贴着水面。",
     "贩盐的舵手说，那是本格拉寒流。去舀一桶水比比冷热，再放个浮标，看它往哪儿漂。",
     "雾遮住了岸。船尾的浮沫朝北漂，这里舀上来的海水也比来时凉。",
     {"放浮标、舀海水","满帆追雾"},"船跑得太快，你已经看不清浮标往哪儿漂了。",
     "本格拉寒流大致沿非洲西南岸北上，近岸冷水容易带来海雾。科菲把这次的记录收进了档案室。"},
};
static_assert(std::size(Expeditions)==ExpeditionCount);

struct ExpeditionProgress {
    std::uint8_t stage=UnknownClue, flags=0;
    int rival_day=0;
};

} // namespace tide
