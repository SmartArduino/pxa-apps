#pragma once
#include "browser_core.hpp"
namespace browser {
// An optional compatibility adapter for forms generated only by site scripts.
// No JS evaluation and no credential/account data. Static endpoints were
// checked against the site's published search_form() and normal login form.
inline void site_forms(Document &doc, std::string_view url) {
    const auto begin = url.find("://");
    if (begin == url.npos) return;
    const auto end = url.find('/', begin + 3);
    const auto host = url.substr(begin + 3, end - begin - 3);
    if (host == "ink.qq.com" || host == "weread.qq.com") {
        doc.append("已使用墨水屏页面。二维码和登录需要 JavaScript，当前暂不支持登录阅读。", Style::muted);
        doc.newline();
        return;
    }
    if (host == "www.gmanhua.com" || host == "gmanhua.com") {
        doc.append("此站章节图片依赖 JavaScript，暂不能显示；可浏览目录和简介。", Style::muted);
        doc.newline();
        return;
    }
    if (host != "www.bookshuku.org" && host != "wap.bookshuku.org") return;
    if (url.find("/User/Public/") != url.npos || url.find("/user/public/") != url.npos) return;
    const auto origin = std::string(url.substr(0, end));
    const int form = int(doc.forms.size());
    doc.forms.push_back({origin + "/search.html", true});
    const int field = int(doc.fields.size());
    doc.fields.push_back({"searchkey", "", "书名或作者", std::int16_t(form), FieldType::text});
    doc.fields.push_back({"", "", "搜索图书", std::int16_t(form), FieldType::submit});
    doc.append("书名或作者", Style::button, -1, field); doc.newline();
    doc.append("搜索图书", Style::button, -1, field + 1); doc.newline();
    const int link = int(doc.links.size());
    doc.links.push_back({origin + "/User/Public/login"});
    doc.append("登录账户", Style::body, link); doc.newline();
}
} // namespace browser
