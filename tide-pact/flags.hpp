#pragma once

namespace tide {

enum ShipColors : std::uint8_t { MerchantColors, PortugueseColors, CastilianColors, FurledColors, ColorsCount };
enum RoyalCourt { NoCourt=-1, Portugal, Castile, CourtCount };

struct ColorsDescription { Text name, note; };
inline constexpr ColorsDescription FlagDetails[]{
    {"本船商旗","潮汐商号的蓝布金星。以自家名义通商。"},
    {"葡萄牙王家旗","白底王室盾徽。受托差船须备里斯本签发的委状。"},
    {"卡斯蒂利亚王家旗","城堡与莱昂狮交错。受托差船须备加的斯签发的委状。"},
    {"收旗","海图上的旗舰收起旗帜。原有的文书和留案仍在港簿上。"},
};
static_assert(std::size(FlagDetails)==ColorsCount);

inline constexpr RoyalCourt colors_court(int colors) {
    return colors==PortugueseColors?Portugal:colors==CastilianColors?Castile:NoCourt;
}

inline constexpr RoyalCourt port_court(int port) {
    switch (port) {
    case lisbon: case madeira: case ribeira: return Portugal;
    case cadiz: case canaries: case santo_domingo: return Castile;
    default: return NoCourt;
    }
}

inline constexpr int issuing_port(RoyalCourt court) {
    return court==Portugal?lisbon:court==Castile?cadiz:-1;
}

} // namespace tide
