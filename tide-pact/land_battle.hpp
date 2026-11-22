#pragma once

namespace tide {

inline constexpr int LandColumns=9, LandRows=6, LandCells=LandColumns*LandRows;
inline constexpr int LandFriends=3, LandUnits=7;
enum LandBattleKind { LandPeace, RoadEscort, WharfAssault, WharfDefense };
enum LandGround { LandRoad, LandGrass, LandCover, LandBarrier, LandGoal };
enum LandUnitAction { LandMoved=1, LandStruck=2, LandBraced=4 };

struct LandUnit {
    std::uint8_t cell=0,hp=0,orders=0;
};

struct LandFight {
    std::uint8_t kind=0,site=0,round=0,holding=0;
    std::array<LandUnit,LandUnits> units{};
};

constexpr int land_cell(Point point) { return point.y*LandColumns+point.x; }
constexpr Point land_point(int cell) { return {cell%LandColumns,cell/LandColumns}; }
constexpr int land_distance(int a,int b) {
    const auto p=land_point(a),q=land_point(b);
    return std::max(p.x,q.x)-std::min(p.x,q.x)+std::max(p.y,q.y)-std::min(p.y,q.y);
}

struct LandField {
    std::array<Text,LandRows> rows;
    Point objective;
    std::array<Point,LandUnits> starts;
};

inline constexpr LandField LandFields[]{
    {{{"ggggggggg","g#ggcgg#g","ggcg..cgg","........o","g#ggcgg#g","ggggggggg"}},
     {8,3}, {{{0,3},{1,2},{2,4},{6,1},{6,4},{7,3},{8,5}}}},
    {{{"ggggggggg","g#g#gcggg","...c.g#go","g........","gggcg#ggg","ggggggggg"}},
     {8,2}, {{{0,3},{1,2},{1,4},{6,1},{6,3},{7,4},{8,4}}}},
    {{{"ggggggggg","ggcgg#ggg","g..#....g","...c..c.o","gg#ggggcg","ggggggggg"}},
     {8,3}, {{{0,3},{1,2},{1,4},{6,1},{7,2},{7,4},{8,1}}}},
};

constexpr const LandField& land_field(int kind,int site) {
    return LandFields[kind==RoadEscort?0:site==0?1:2];
}
constexpr LandGround land_ground(const LandFight& fight,int cell) {
    const auto at=land_point(cell);
    switch (land_field(fight.kind,fight.site).rows[at.y][at.x]) {
    case '#': return LandBarrier;
    case 'c': return LandCover;
    case 'o': return LandGoal;
    case '.': return LandRoad;
    default: return LandGrass;
    }
}
constexpr int land_max_hp(int unit) { return unit==0?5:unit<LandFriends?4:3; }

constexpr bool valid_land_fields() {
    for (int i=0;i<int(std::size(LandFields));++i) {
        const LandFight fight{.kind=static_cast<std::uint8_t>(i?WharfAssault:RoadEscort),
                              .site=static_cast<std::uint8_t>(i==2)};
        const auto& field=LandFields[i];
        for (const auto row : field.rows) if (row.size()!=LandColumns) return false;
        if (land_ground(fight,land_cell(field.objective))!=LandGoal) return false;
        for (const auto start : field.starts)
            if (land_ground(fight,land_cell(start))==LandBarrier) return false;
    }
    return true;
}
static_assert(valid_land_fields());

int land_occupant(const LandFight& fight,int cell);
std::array<std::uint8_t,LandCells> land_steps(const LandFight& fight,int unit);
bool land_can_strike(const LandFight& fight,int unit,int target);
bool valid_land_fight(const LandFight& fight,int origin,bool underway);

} // namespace tide
