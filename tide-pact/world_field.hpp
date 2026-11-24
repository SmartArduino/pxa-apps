#pragma once

#include <cstdlib>

namespace tide {

enum FieldGround { BorealGround, MangroveGround, IceGround, TropicalGround, ReefGround, SandstoneGround, LavaGround,
                   BeyporeGround, HouzhuGround, TsushimaGround, YeompoGround };

struct FieldStudy {
    int place;
    FieldGround ground;
    int hunt;
    Text region, habitat, animal, plant, animal_note, plant_note;
    std::array<Text,3> pictures;
    bool culture=false;
};

#define FIELD_PLATES(name) {"assets/world/" name "-shore.pxi","assets/world/" name "-animal.pxi","assets/world/" name "-plant.pxi"}
inline constexpr FieldStudy FieldStudies[]{
    {newfoundland,BorealGround,0,"北美","沿着林间小径到岩岸。风大时鸟叫听不远，往开阔处看。",
     "大海雀","黑云杉","这只黑白海鸟的翅膀短小。在水里，它划得比一条小艇还利落。",
     "低矮的云杉沿岸背风处成片生长。枝端留着几个还没张开的球果。",FIELD_PLATES("newfoundland")},
    {orinoco,MangroveGround,1,"南美","瓦劳舟人指给你一条沿水道的高脊。涨水时，低处的树根都藏在水下。",
     "红鹮","莫里切棕榈","一群朱红色水鸟在泥滩探食。它们飞起来时，翼尖露出一抹黑。",
     "扇形的叶子在水面上摇。结着赤褐色鳞皮果实的那棵离水道最近。",FIELD_PLATES("orinoco")},
    {greenland,IceGround,-1,"北极","湾口有浮冰。沿着营地附近的岩脊走，看到海豹也别踩上浮冰。",
     "环斑海豹","北极柳","伏在浮冰上的海豹背上有浅色环纹。它抬头看了片刻，便滑回水里。",
     "这株柳树贴着碎石铺开。叶片小，枝条都伏在避风的一侧。",FIELD_PLATES("greenland")},
    {shetlands,IceGround,-1,"南极","这片岸上找不到补给。趁天气平稳上岸，绕开崖下的碎冰，随时准备回船。",
     "帽带企鹅","南极发草","几只企鹅沿岩石排成一列。它们白色下颌边有一道细黑纹。",
     "石缝里有稀疏的细叶草丛。冰雪退开后，这点绿色才显出来。",FIELD_PLATES("shetlands")},
    {malacca,TropicalGround,-1,"东南亚","码头的货船挨着河岸卸货。往远处走几步，仓屋便让位给咸潮林。",
     "长尾猕猴","水椰","细长的尾巴在树枝间一扫。猕猴抱着果实钻进了林子深处。",
     "水椰的叶子从泥里向上展开。球形的果序很低，几乎贴着潮水。",FIELD_PLATES("malacca")},
    {quanzhou,MangroveGround,-1,"东亚","沿晋江口的浅滩走。老石桥旁有行船的人，滩边常聚着白色水鸟。",
     "黄嘴白鹭","秋茄","白羽在泥滩上格外显眼。那只鹭的黄嘴朝浅水一探，叼起了小鱼。",
     "潮沟边的常绿灌木伸出细长的绿胚轴。落下后有的会随潮水漂走。",FIELD_PLATES("quanzhou")},
    {naha,ReefGround,-1,"东亚","那霸的货船泊在礁岸内侧。当地人带你沿沙地小路去看外湾。",
     "绿海龟","草海桐","浅水里的龟慢慢拨动前鳍，钻进了一片水草。",
     "厚叶子能挡住带盐的海风。白花只展开半边，像一把小扇。",FIELD_PLATES("naha")},
    {kimberley,SandstoneGround,-1,"澳洲","潮水能淹到砂岩脚下。沿高处走得远些，也能离危险的浅溪远些。",
     "湾鳄","白骨壤","潮沟里有一双眼睛从水面露出来。远远画下头形便已足够。",
     "浅泥里伸着许多笔杆一样的根。退潮时，整片林子就显出这一层。",FIELD_PLATES("kimberley")},
    {galapagos,LavaGround,-1,"南美外海","熔岩凝成黑色岩阶。岸上几乎找不到水，背阴处却长着成片的仙人掌。",
     "海鬣蜥","加岛仙人掌","黑色蜥蜴在熔岩上晒热身体。有一只游进浪里去吃藻。",
     "老仙人掌生出木质主干。高处伸着扁平的茎片，边缘开了一朵黄花。",FIELD_PLATES("galapagos")},
    {beypore,BeyporeGround,-1,"印度·马拉巴尔","木匠拉维让你走河边小路。他缺一根记号绳，正拿旧绳在新船架上比量。",
     "比船架的拉维","船肋与木楔","拉维用绳在两边各比一次。左边多出一指，他叫徒弟把船肋卸下再刨。他说，船入水前，歪一点也藏不住。",
     "徒弟用木片试探接缝，一面做记号一面请你画下顺序。他说，下次换坏船板，照这个顺序拆，隔壁的板就不会松动。",
     FIELD_PLATES("beypore"),true},
    {houzhu,HouzhuGround,-1,"中国·明朝福建","船到岸外时潮水正退。卸货的阿泉带你绕开泥滩，去看哪些瓷碗还能随下一船走。",
     "拣瓷的阿泉","稻草垫与瓷碗","阿泉轻敲两只碗，一只清脆，一只闷。他捡出那只有暗裂的，又在筐边系了条绳。回程要把坏碗送给窑工验看。",
     "大碗装在筐底，小碗之间塞稻草。草还能吸去潮气。阿泉要你画出筐盖的绑法，说到港时一眼就能查出有没有动过。",
     FIELD_PLATES("houzhu"),true},
    {tsushima,TsushimaGround,-1,"日本·室町对马","海湾外起雾。宗氏的船还没走，码头有一卷文书等人送过来。",
     "守文书的弥七","等船的米袋","弥七三次看向湾口，唯独没敢先拆纸封。凭宗氏出具的文书才能办理通商，名字写错一个字，这趟就白跑。",
     "两个袋口各拴着不同长短的绳。弥七凭绳结分出船上口粮和待交的米。雨到前先装粮，你顺手记下这套记号。",
     FIELD_PLATES("tsushima"),true},
    {yeompo,YeompoGround,-1,"朝鲜·蔚山湾","潮水已落。晾鱼架边有条小路，沿它走可以到查验货件的小棚。",
     "查验的书吏","退潮后的鱼架","书吏收起纸簿，等另一条船送来对照的文书。他让你把货留在棚内，等查清再叫人来取。",
     "岸边的渔户已把鱼翻过一面。他说，潮气一重，鱼得隔半掌晾。傍晚的接驳船一来，这批鱼就能换些布。",
     FIELD_PLATES("yeompo"),true},
};
#undef FIELD_PLATES

inline constexpr int FieldStudyCount=std::size(FieldStudies);
static_assert(FieldStudyCount*2<=32);
constexpr std::uint32_t field_note_bit(int study,int type) {
    return std::uint32_t{1}<<(study*2+type);
}
constexpr int field_study_at(int place) {
    for (int i=0;i<FieldStudyCount;++i)
        if (FieldStudies[i].place==place) return i;
    return -1;
}

inline constexpr Text QuarryNames[]{"柳雷鸟","低地斑豚鼠"};
inline constexpr int FieldColumns=22, FieldRows=11;
struct FieldSceneLandmarks { Point camp, animal, plant, quarry; };

#include "field_layout.hpp"

constexpr bool field_walkable(FieldGround ground,Point p) {
    return p.x>0 && p.x<FieldColumns-1 && p.y>0 && p.y<FieldRows-1 && FieldLayout[ground][p.y][p.x]=='.';
}

constexpr int field_range(Point a,Point b) { return std::abs(a.x-b.x)+std::abs(a.y-b.y); }

class FieldWalk {
public:
    FieldWalk() : FieldWalk(BorealGround) {}
    explicit FieldWalk(FieldGround ground)
        : player(FieldLandmarks[ground].camp), from(player),
          quarry(FieldLandmarks[ground].quarry), previous_quarry(quarry), target(player) {}

    Point player, from, quarry, previous_quarry;
    Point target, facing{1,0};
    Point shot_from{}, shot_to{};
    unsigned moved_at=0, animal_at=0, shot_at=0;
    bool stalking=false, hunted=false;
    int step=0;

    bool move(FieldGround ground,Point direction,unsigned now) {
        const Point next{player.x+direction.x,player.y+direction.y};
        if ((!direction.x && !direction.y) || !field_walkable(ground,next)) return false;
        from=player;
        player=next;
        facing=direction;
        moved_at=now;
        return true;
    }

    void advance(FieldGround ground,unsigned now,Point heading,int prey,int wind,bool raining) {
        ++step;
        if (heading.x || heading.y) target=player;
        if (!stalking || step%2==0) {
            if (heading.x || heading.y) move(ground,heading,now);
            else if (field_range(target,player)>0) move(ground,route(ground),now);
        }
        if (prey<0 || hunted || step%3) return;
        constexpr Point steps[]{{1,0},{0,1},{-1,0},{0,-1}};
        const auto breeze=Directions[wind];
        const int downwind=(player.x-quarry.x)*breeze.x+(player.y-quarry.y)*breeze.y;
        const int range=stalking?2:raining?4:5;
        const bool alerted=field_range(player,quarry)<=range-(downwind>0);
        auto winner=quarry;
        int furthest=alerted?field_range(player,quarry):-1;
        const Point habitat=FieldLandmarks[ground].quarry;
        for (int i=0;i<4;++i) {
            const auto d=steps[(i+step/3)%4];
            const Point candidate{quarry.x+d.x,quarry.y+d.y};
            if (std::abs(candidate.x-habitat.x)>4 || std::abs(candidate.y-habitat.y)>2 ||
                !field_walkable(ground,candidate)) continue;
            const int distance=field_range(player,candidate);
            if (alerted && distance<=furthest) continue;
            winner=candidate;
            furthest=distance;
            if (!alerted) break;
        }
        if (field_range(winner,quarry)) {
            previous_quarry=quarry;
            quarry=winner;
            animal_at=now;
        }
    }

    bool can_shoot(FieldGround ground) const {
        const int distance=field_range(player,quarry);
        if (hunted || !distance || distance>(stalking?4:2)) return false;
        for (int i=1;i<distance;++i) {
            const Point p{player.x+(quarry.x-player.x)*i/distance,
                          player.y+(quarry.y-player.y)*i/distance};
            if (!field_walkable(ground,p)) return false;
        }
        return true;
    }

private:
    Point route(FieldGround ground) const {
        if (!field_walkable(ground,target) || !field_walkable(ground,player)) return {};
        struct Search { Point at, first; };
        std::array<Search, FieldColumns*FieldRows> queue{};
        std::array<bool, FieldColumns*FieldRows> visited{};
        const auto index=[](Point p) { return p.y*FieldColumns+p.x; };
        visited[index(player)]=true;
        queue[0]={player,{}};
        int count=1;
        constexpr Point steps[]{{1,0},{0,1},{-1,0},{0,-1}};
        for (int i=0;i<count;++i) {
            const auto node=queue[i];
            for (auto offset : steps) {
                const Point next{node.at.x+offset.x,node.at.y+offset.y};
                if (!field_walkable(ground,next) || visited[index(next)]) continue;
                const Point first=i?node.first:offset;
                if (field_range(next,target)==0) return first;
                visited[index(next)]=true;
                queue[count++]={next,first};
            }
        }
        return {};
    }
};

} // namespace tide
