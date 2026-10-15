#pragma once
#include "reader_core.hpp"
namespace reader {
inline std::optional<std::string> search_address(std::string_view pattern,std::string_view key) {
    const auto escaped=percent(key);std::string out;
    while(!pattern.empty()) {
        const auto marker=pattern.find("{{key}}");
        const auto prefix=pattern.substr(0,marker);
        if(prefix.size()>512-out.size())return {};
        out+=prefix;
        if(marker==pattern.npos)break;
        if(escaped.size()>512-out.size())return {};
        out+=escaped;pattern.remove_prefix(marker+7);
    }
    return out;
}
struct Classic { std::string_view title, author, aliases; unsigned id; bool git; };
// Metadata only: no book bodies or allocated search index in the package.
inline constexpr std::array classics{
    Classic{"西游记", "吴承恩", "西遊記 Journey to the West", 23962, true},
    Classic{"红楼梦", "曹雪芹", "紅樓夢 Dream of the Red Chamber", 24264, true},
    Classic{"水浒传", "施耐庵", "水滸傳 Water Margin", 23863, true},
    Classic{"三国演义", "罗贯中", "三國志演義 Romance of the Three Kingdoms", 23950, false}
};
inline bool contains_folded(std::string_view value, std::string_view query) {
    auto fold = [](unsigned char c) { return c >= 'A' && c <= 'Z' ? c + ('a' - 'A') : c; };
    return std::search(value.begin(), value.end(), query.begin(), query.end(),
        [&](char a, char b) { return fold(a) == fold(b); }) != value.end() || query.empty();
}
inline std::vector<Book> search_classics(std::string_view query, SourceKind kind) {
    std::vector<Book> result;
    for (const auto& c : classics) {
        if (kind == SourceKind::gitenberg && !c.git) continue;
        if (!contains_folded(c.title, query) && !contains_folded(c.author, query) &&
            !contains_folded(c.aliases, query)) continue;
        const auto id = reader::number_string(c.id);
        auto url = kind == SourceKind::gitenberg
            ? "https://raw.githubusercontent.com/GITenberg/---_" + id + "/master/" + id + "-0.txt"
            : "https://www.gutenberg.org/cache/epub/" + id + "/pg" + id + ".txt";
        result.push_back({std::string(c.title), std::string(c.author), std::move(url), "", 0, 0, false, {}});
    }
    return result;
}
} // namespace reader
