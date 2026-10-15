#pragma once
#include "browser_core.hpp"
namespace browser {
struct Site { std::string_view name, url; };
// Keep navigation independent of the user's editable bookmarks. Entries are
// verified with the reader; the audit document records unsupported old links.
inline constexpr std::array sites{
    Site{"TXT 图书", "http://wap.bookshuku.org/"},
    Site{"万本 TXT 小说网", "https://10000txt.com/"},
    Site{"努努书坊", "https://www.nunubook.com/"},
    Site{"手机新浪网", "https://3g.sina.com.cn/"},
    Site{"手机漫画网", "https://www.gmanhua.com/"},
    Site{"hao123", "https://wap.hao123.com/"},
    Site{"2345", "https://www.2345.com/wap.htm"}
};
// Per-origin compatibility, recomputed after every redirect. Do not claim to
// be a modern JS engine globally or send the reader UA to unrelated sites.
inline std::string_view site_user_agent(std::string_view url, std::string_view fallback) {
    const auto scheme = url.find("://");
    if (scheme == url.npos) return fallback;
    const auto begin = scheme + 3;
    auto host = url.substr(begin, url.find_first_of("/?#", begin) - begin);
    if (host.ends_with(":443")) host.remove_suffix(4);
    if (host == "ink.qq.com" || host == "weread.qq.com")
        return "Mozilla/5.0 (X11; U; Linux armv7l like Android; en-us) "
               "AppleWebKit/531.2+ (KHTML, like Gecko) Version/5.0 Safari/533.2+ Kindle/3.0+";
    return fallback;
}
inline bool gif_url(std::string_view url) {
    return lowercase(url.substr(0, url.find_first_of("?#"))).ends_with(".gif");
}
} // namespace browser
