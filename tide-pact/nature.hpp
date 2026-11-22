#pragma once

namespace tide {

enum NatureSite { NoNatureSite, LaurelWoods, SaltMarsh, MadeiraWaters, CaboVerdeWaters };
enum NatureEntry { IslandLaurel, Trocaz, Saltwort, Flamingo, CommonDolphin, Loggerhead, NatureCount };

struct NatureRecord {
    NatureSite site;
    Text name, scientific, trace, hint, sketch, note;
};

inline constexpr std::array<NatureRecord,NatureCount> NatureRecords{{
    {LaurelWoods,"马德拉月桂","Laurus novocanariensis","水渠旁的深绿叶片",
     "顺渠去林边，找枝头发亮的叶片。","描下叶片","叶面油亮，叶边平滑。熟果蓝黑，一粒一粒垂在细梗上。"},
    {LaurelWoods,"马德拉林鸽","Columba trocaz","林梢传来咕咕声",
     "到丰沙尔林地，清晨在树下静候。","守望树梢","一只灰鸽从树冠落下来。颈侧泛银光，展开的尾羽上有一道浅带。"},
    {SaltMarsh,"盐角草","Salicornia spp.","盐泥里的肉质茎",
     "去加的斯湾的盐田，沿外沟找。","画下枝节","盐泥里长出一丛肉质绿茎。枝条一节扣一节，秋来有些会泛红。"},
    {SaltMarsh,"大红鹳","Phoenicopterus roseus","浅水里有粉白长影",
     "到加的斯盐田，退潮后看外沟。","观察觅食","粉白的长腿鸟把黑尖弯喙埋进浅水，一边踱步，一边筛食。"},
    {MadeiraWaters,"短吻普通海豚","Delphinus delphis","船头浪里有背鳍",
     "掌舵到马德拉附近海域，风浪小些时留心船头。","记录跃浪","跃出水面的一刻，能看见海豚前腹侧泛黄、后腹侧浅灰，像一只沙漏。"},
    {CaboVerdeWaters,"红海龟","Caretta caretta","浅海有圆影浮起",
     "佛得角群岛近海。缩帆后留意浮出水面的影子。","看它换气","海龟探头换气，又悄悄沉下去。它的头粗，硬壳呈红褐色。"},
}};

inline constexpr Text nature_site_name(NatureSite site) {
    constexpr Text names[]{"","丰沙尔林地","加的斯盐田","马德拉外海","佛得角近海"};
    return names[site];
}

inline constexpr int NatureMarks=(1<<NatureCount)-1;
inline constexpr int NatureHikeBit=1<<NatureCount;
static_assert(NatureCount<8);

} // namespace tide
