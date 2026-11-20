#pragma once

namespace tide {

enum Language : std::uint8_t {
    Portuguese, English, Flemish, French, Castilian, Arabic, Valencian,
    Ligurian, Venetian, Akan, Kikongo, Swahili, LanguageCount
};
enum Fluency : std::uint8_t { Unfamiliar, Phrases, Fluent };

inline constexpr Text LanguageNames[]{
    "葡萄牙语","英语","佛兰德语","法语","卡斯蒂利亚语","阿拉伯语","瓦伦西亚语",
    "利古里亚语","威尼斯语","阿坎语","刚果语","斯瓦希里语"
};
inline constexpr Language PortSpeech[]{
    Portuguese,English,Flemish,French,Castilian,Arabic,Valencian,Ligurian,Arabic,Venetian,
    Arabic,Portuguese,Castilian,Portuguese,Akan,Kikongo,Swahili,Swahili,Swahili,Arabic
};
inline constexpr Language BookLanguage[]{Portuguese,Portuguese,Arabic,Portuguese};
inline constexpr Language WitnessLanguage[]{Portuguese,Portuguese,Castilian,Akan};
static_assert(std::size(LanguageNames)==LanguageCount && LanguageCount*2<=32);
static_assert(std::size(PortSpeech)==PortCount);
static_assert(std::size(BookLanguage)==ExpeditionCount && std::size(WitnessLanguage)==ExpeditionCount);

inline constexpr Language port_second_language(int port) {
    switch (port) {
    case cadiz: case kilwa: return Arabic;
    case tangier: case valencia: return Castilian;
    case elmina: return Portuguese;
    default: return LanguageCount;
    }
}

enum TideLedgerStage : std::uint8_t { LedgerUnseen, LedgerCopied, LedgerPublished, LedgerReturned };

} // namespace tide
