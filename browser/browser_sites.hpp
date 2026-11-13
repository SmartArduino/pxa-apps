#pragma once
#include "browser_core.hpp"
namespace browser {
struct Site { std::string_view name, url; };
// Keep navigation independent of the user's editable bookmarks. Entries are
// checked in the real reader. Prefer static text or feeds for small screens;
// public availability and linked pages can change independently of this app.
inline constexpr std::array sites{
    Site{"小辣椒与萝卜头 · 中文漫画", "https://www.peppercarrot.com/cn/webcomic/ep01_Potion-of-Flight.html"},
    Site{"xkcd · 英文漫画", "https://xkcd.com/#comic"},
    Site{"古诗文 · 诗词阅读", "https://m.gushiwen.cn/"},
    Site{"IT之家 · 资讯订阅", "https://www.ithome.com/rss/"},
    Site{"博客园 · 技术文章", "https://feed.cnblogs.com/blog/sitehome/rss"},
    Site{"维基文库 · 经典原文", "https://zh.wikisource.org/zh-hans/%E9%A6%96%E9%A1%B5"},
    Site{"NPR · 英文文字新闻", "https://text.npr.org/"},
    Site{"TXT 图书", "http://wap.bookshuku.org/"},
    Site{"万本 TXT 小说网", "https://10000txt.com/"},
    Site{"努努书坊", "https://www.nunubook.com/"},
    Site{"手机新浪网", "https://3g.sina.com.cn/"},
    Site{"手机漫画网（目录）", "https://www.gmanhua.com/"},
    Site{"hao123", "https://wap.hao123.com/"},
    Site{"2345", "https://www.2345.com/wap.htm"},
    Site{"微信读书（实验）", "https://ink.qq.com/"}
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
