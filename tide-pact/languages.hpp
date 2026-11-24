#pragma once

namespace tide {

enum Language : std::uint8_t {
    Portuguese, English, Flemish, French, Castilian, Arabic, Valencian,
    Ligurian, Venetian, Akan, Kikongo, Swahili, Norwegian, Persian, Malayalam, Hokkien, Japanese, Korean, Malay, Ryukyuan, ZhejiangSpeech, LanguageCount
};
enum Fluency : std::uint8_t { Unfamiliar, Phrases, Fluent };

inline constexpr Text LanguageNames[]{
    "葡萄牙语","英语","佛兰德语","法语","卡斯蒂利亚语","阿拉伯语","瓦伦西亚语",
    "利古里亚语","威尼斯语","阿坎语","刚果语","斯瓦希里语","挪威语","波斯语","马拉雅拉姆语","闽南话","日语","朝鲜语","马来语","琉球语","浙东话"
};
inline constexpr Language PortSpeech[]{
    Portuguese,English,Flemish,French,Castilian,Arabic,Valencian,Ligurian,Arabic,Venetian,
    Arabic,Portuguese,Castilian,Portuguese,Akan,Kikongo,Swahili,Swahili,Swahili,Arabic,
    Norwegian,Norwegian,Arabic,Persian,Malayalam,Castilian,
    // Merchant contacts in the new ports can conduct business in these languages.
    Venetian,Venetian,Persian,Arabic,
    Hokkien,Japanese,Malayalam,Korean,
    Malay,Ryukyuan,ZhejiangSpeech,Japanese
};
inline constexpr Language BookLanguage[]{Portuguese,Portuguese,Arabic,Portuguese};
inline constexpr Language WitnessLanguage[]{Portuguese,Portuguese,Castilian,Akan};
static_assert(std::size(LanguageNames)==LanguageCount && LanguageCount*2<64);
static_assert(std::size(PortSpeech)==PortCount);
inline constexpr Language port_language(int place) { return PortSpeech[port_index(place)]; }
static_assert(std::size(BookLanguage)==ExpeditionCount && std::size(WitnessLanguage)==ExpeditionCount);

inline constexpr Language port_second_language(int port) {
    switch (port) {
    case cadiz: case kilwa: case hormuz: case calicut: case diu: case kochi: return Arabic;
    case tangier: case valencia: return Castilian;
    case elmina: return Portuguese;
    case busanpo: return Japanese;
    case malacca: return Arabic;
    case naha: return Hokkien;
    default: return LanguageCount;
    }
}

enum TideLedgerStage : std::uint8_t { LedgerUnseen, LedgerCopied, LedgerPublished, LedgerReturned };

} // namespace tide
