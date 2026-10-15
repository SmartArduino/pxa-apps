#pragma once
#include "browser_core.hpp"
namespace browser {
enum class SearchEngine { bing, duckduckgo };
inline std::expected<std::string, Error> address_target(std::string_view input,
                                                        SearchEngine engine) {
    input = trim(input);
    if (input.empty())
        return std::unexpected(Error::invalid_url);
    bool search = input.starts_with('?');
    if (search)
        input = trim(input.substr(1));
    if (input.empty())
        return std::unexpected(Error::invalid_url);
    if (!search && input.find("://") != input.npos)
        return resolve({}, input);
    if (!search && input.find_first_of(" \t\r\n") == input.npos &&
        (input.find('.') != input.npos || input.starts_with("localhost") || input.starts_with('[')))
        return resolve({}, input, true);
    for (unsigned char c : input)
        if (c < 32 || c == 127)
            return std::unexpected(Error::invalid_url);
    std::string target = engine == SearchEngine::bing ? "https://www.bing.com/search?format=rss&q="
                                                      : "https://lite.duckduckgo.com/lite/?q=";
    target += encode(input, true);
    if (target.size() > max_url)
        return std::unexpected(Error::limit);
    return target;
}
// Minimal RSS 2.0 reader for search feeds. One bounded item, not a complete XML
// tree. No external entities, DTD expansion, network fetching or XML execution.
class RssParser {
    Document &doc_;
    std::string buffer_;
    bool item_ = false, overflow_ = false;
    static std::string value(std::string_view item, std::string_view tag) {
        std::string open = "<" + std::string(tag) + ">", close = "</" + std::string(tag) + ">";
        auto at = item.find(open);
        if (at == item.npos)
            return {};
        at += open.size();
        auto end = item.find(close, at);
        if (end == item.npos)
            return {};
        auto text = item.substr(at, end - at);
        if (text.starts_with("<![CDATA[") && text.ends_with("]]>"))
            return std::string(text.substr(9, text.size() - 12));
        return entities(text);
    }
    void publish() {
        auto title = value(buffer_, "title"), link = value(buffer_, "link"),
             description = value(buffer_, "description");
        auto target = resolve(doc_.base, trim(link));
        if (!target || doc_.links.size() >= max_links)
            return;
        // Plain titles never interpret embedded HTML; descriptions are sanitized
        // by the same bounded HTML tokenizer used for normal pages.
        Document safe;
        safe.clear(doc_.base);
        Parser parser(safe);
        parser.feed_plain(title);
        parser.finish();
        auto label = trim(safe.text);
        if (label.empty())
            label = *target;
        auto index = int(doc_.links.size());
        doc_.links.push_back({std::move(*target)});
        doc_.newline();
        doc_.append(label, Style::heading, index);
        doc_.newline();
        Parser text(doc_);
        text.feed(description);
        text.finish();
        doc_.newline();
    }

  public:
    explicit RssParser(Document &d) : doc_(d) {
        buffer_.reserve(8192);
        doc_.title = "搜索结果";
    }
    void feed(std::string_view bytes) {
        for (char c : bytes) {
            if (doc_.truncated)
                return;
            if (!item_) {
                buffer_ += c;
                if (buffer_.ends_with("<item>")) {
                    item_ = true;
                    buffer_.clear();
                    overflow_ = false;
                } else if (buffer_.size() > 6)
                    buffer_.erase(0, buffer_.size() - 6);
            } else {
                buffer_ += c;
                if (buffer_.ends_with("</item>")) {
                    if (!overflow_)
                        publish();
                    buffer_.clear();
                    item_ = false;
                } else if (buffer_.size() >= 8192) {
                    overflow_ = true;
                    buffer_.erase(0, buffer_.size() - 7);
                }
            }
        }
    }
    void finish() {
        if (doc_.links.empty())
            doc_.append("没有可显示的搜索结果，请更换关键词或搜索引擎。");
    }
};
} // namespace browser
