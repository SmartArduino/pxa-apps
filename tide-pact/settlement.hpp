#pragma once

namespace tide {

enum EnclaveStatus : std::uint8_t { EnclaveClosed, EnclaveLease, EnclaveOccupied };
enum EnclaveBuilding : std::uint8_t {
    EnclaveWharf, EnclaveWarehouse, EnclaveHouses, EnclaveGardens, EnclaveWorkshop, EnclaveWatch, EnclaveBuildingCount
};
constexpr std::uint8_t building_flag(int id) { return static_cast<std::uint8_t>(1U<<id); }

struct EnclaveSite {
    int home;
    Point position;
    Text name, ground, people;
    int fee, resistance, crop_yield, timber_yield;
};

inline constexpr EnclaveSite EnclaveSites[]{
    {madeira,chart_point(-16.766f,32.719f),"马希库",
     "小湾背风，有溪水和灌渠。狭谷里可种菜，木料要从山坡运下。",
     "卡塔琳娜说：暴雨冲走了过溪的木桥。商船靠在湾里，村里就得派人划小艇去卸粮。你肯修码头，渔户就留下帮工。",
     145,37,3,2},
    {azores,chart_point(-25.437f,37.716f),"维拉弗兰卡多坎普",
     "圣米格尔岛南岸。田地肥沃，外海涌浪会穿过锚地。",
     "米格尔说：移居到岛上的人都要找屋住。先把粮库和水井看好，开荒的人吃得饱才会把家人带来。",
     115,41,4,1},
};
inline constexpr int EnclaveCount=std::size(EnclaveSites);

struct BuildingOrder {
    Text name;
    std::uint8_t timber, coins, days;
};
inline constexpr BuildingOrder BuildingOrders[]{
    {"码头",4,34,2}, {"仓房",4,42,2}, {"住房",5,48,3}, {"菜地",2,27,2},
    {"木工棚",6,55,3}, {"望楼",5,63,3},
};

struct EnclaveHolding {
    int building_day=0, peace_day=0;
    std::uint8_t status=EnclaveClosed, buildings=0, under_construction=0;
    std::uint8_t residents=0, food=0, timber=0, exports=0;
    std::uint8_t term=0, prisoners=0;
    bool carpenter=false;
};

enum EnclaveTransfer { DeliverEnclaveFood, DeliverEnclaveTimber, CollectEnclaveFood, CollectEnclaveTimber };
enum EnclavePrisonerChoice { ReleaseEnclavePrisoners, RansomEnclavePrisoners };

} // namespace tide
