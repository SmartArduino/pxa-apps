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
     "《返航手记》记着：从马德拉向西北，有一串与葡萄牙纬度相近的岛。船长把归航时的风都记在那一页。",
     "老舵手指着亚速尔群岛：守在外海看帆往哪边鼓。峡口的乱风容易骗人。",
     "浪打在两片黑色火山岩之间。岩缝里风声很急，外海的浮沫却稳稳移向一侧。",
     {"在外海记风","进岩缝测风"},"峡口阵风转了方向，读数没法用。",
     "外海的西风有时能帮船东返。归船不必沿着来时的低纬航线折返。贝娅把风向抄给等船的家人。"},
    {madeira,arguin,27,130,520,1,"潮退时的白翅","船医特蕾莎",
     "《南下鸟记》里夹着一根白羽。作者从大加那利岛向南航行，在佛得角东北、撒哈拉西岸的浅滩画过鸟群。",
     "水手说那是阿尔金浅滩。露出沙洲时远望便能看见鸟喙，跟小艇进去会把鸟惊飞。",
     "潮水涨落间，成群的鸟在沙洲上穿梭。记录它们的长喙和脚印就足以补全旧手稿。",
     {"划艇追踪","退潮远望"},"船桨一响，鸟群全飞离了沙洲。",
     "阿尔金的潮间带为大群迁徙水鸟提供食物。特蕾莎把沙洲和观测地点分开画在航图上。"},
    {cadiz,gibraltar,14,75,310,1,"两岸的水痕","领航员露西亚",
     "《两岸渡船记》说：从加的斯往东南，北岸峭壁与南岸遥遥相对。同一叶木片总会先漂向地中海。",
     "两岸船夫指的是直布罗陀海峡。先记木片从哪边起漂，再看海峡中段的流向，别在港湾里读数。",
     "窄口两侧分别是伊比利亚与北非。码头边有小涡，海峡中部的漂沫却缓缓向东。",
     {"在港湾放标","在海峡放标"},"漂标在岸边打转，量到的只有回流。",
     "直布罗陀海峡的表层净流向东，较深处有流向大西洋的水。露西亚在渡船图上添了表流箭头。"},
    {elmina,benguela,42,190,760,0,"雾外的寒流","制图师科菲",
     "《南岸雾记》从刚果河口一路向南，到好望角以前，舀起的海水已比赤道附近冷，晨雾贴在水面。",
     "贩盐的舵手说这是本格拉寒流。松帆前先系牢浮标，垂直量温，再记漂向。",
     "雾带把海岸遮去。船尾的浮沫朝北漂去，北上的水明显比来时舀过的海水凉。",
     {"系标测温","满帆追雾"},"风帆拉偏了船身，雾在前面，浮标却已看不清。",
     "本格拉寒流沿非洲西南岸大致北上，近岸冷水有利于形成海雾。科菲把雾记和测温结果一起收进馆藏。"},
};
static_assert(std::size(Expeditions)==ExpeditionCount);

struct ExpeditionProgress {
    std::uint8_t stage=UnknownClue, flags=0;
    int rival_day=0;
};

} // namespace tide
