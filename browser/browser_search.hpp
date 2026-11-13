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
// Minimal RSS 2.0 / Atom reader. One bounded item, not a complete XML
// tree. No external entities, DTD expansion, network fetching or XML execution.
class RssParser {
    Document &doc_;
    std::string buffer_;
    bool item_ = false, overflow_ = false, atom_ = false, cdata_ = false;
    static std::size_t tag_end(std::string_view text, std::size_t at) {
        char quote = 0;
        for (; at < text.size(); ++at) {
            const auto c = text[at];
            if (quote) { if (c == quote) quote = 0; }
            else if (c == '\'' || c == '"') quote = c;
            else if (c == '>') return at;
        }
        return text.npos;
    }
    static std::pair<std::size_t, std::size_t> opening(std::string_view text,
                                                      std::string_view name, std::size_t from = 0) {
        const std::string prefix = "<" + std::string(name);
        for (;;) {
            auto at = text.find(prefix, from);
            if (at == text.npos) return {at, at};
            const auto after = at + prefix.size();
            if (after < text.size() && (space(text[after]) || text[after] == '/' || text[after] == '>'))
                return {at, tag_end(text, after)};
            from = after;
        }
    }
    static std::string value(std::string_view item, std::string_view name) {
        const auto [start, at] = opening(item, name);
        if (start == item.npos || at == item.npos) return {};
        const auto end = item.find("</" + std::string(name) + ">", at + 1);
        if (end == item.npos) return {};
        auto text = trim(item.substr(at + 1, end - at - 1));
        if (text.starts_with("<![CDATA[") && text.ends_with("]]>"))
            return std::string(text.substr(9, text.size() - 12));
        return entities(text);
    }
    void publish() {
        auto title = value(buffer_, "title"), link = value(buffer_, "link"),
             description = value(buffer_, atom_ ? "summary" : "description");
        if (atom_) {
            if (description.empty()) description = value(buffer_, "content");
            for (std::size_t from = 0;;) {
                const auto [begin, end] = opening(buffer_, "link", from);
                if (begin == buffer_.npos || end == buffer_.npos) break;
                const auto element = tag(std::string_view(buffer_).substr(begin + 1, end - begin - 1));
                const auto rel = lowercase(element.get("rel"));
                if (rel.empty() || rel == "alternate") { link = element.get("href"); break; }
                from = end + 1;
            }
        }
        auto target = resolve(doc_.base, trim(link));
        if (!target || doc_.links.size() >= max_links)
            return;
        // Plain titles never interpret embedded HTML; descriptions are sanitized
        // by the same bounded HTML tokenizer used for normal pages.
        Document safe;
        safe.clear(doc_.base);
        Parser parser(safe);
        const auto [title_begin, title_end] = opening(buffer_, "title");
        const bool html_title = atom_ && title_begin != buffer_.npos && title_end != buffer_.npos &&
            tag(std::string_view(buffer_).substr(title_begin + 1, title_end - title_begin - 1)).get("type") == "html";
        if (html_title) parser.feed(title); else parser.feed_plain(title);
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
                bool begins = false;
                if (c == '>') {
                    const auto at = buffer_.rfind('<');
                    if (at != buffer_.npos) {
                        const auto text = std::string_view(buffer_).substr(at + 1, buffer_.size() - at - 2);
                        const auto t = tag(text);
                        begins = !t.closing && !trim(text).ends_with('/') &&
                                 (t.name == "item" || t.name == "entry");
                        if (begins) atom_ = t.name == "entry";
                    }
                }
                if (begins) {
                    item_ = true;
                    buffer_.clear();
                    overflow_ = cdata_ = false;
                } else if (buffer_.size() > 512)
                    buffer_.erase(0, buffer_.size() - 256);
            } else {
                buffer_ += c;
                if (!cdata_ && buffer_.ends_with("<![CDATA[")) cdata_ = true;
                else if (cdata_ && buffer_.ends_with("]]>")) cdata_ = false;
                else if (!cdata_ && buffer_.ends_with(atom_ ? "</entry>" : "</item>")) {
                    if (!overflow_)
                        publish();
                    buffer_.clear();
                    item_ = false;
                } else if (buffer_.size() >= 8192) {
                    overflow_ = true;
                    buffer_.erase(0, buffer_.size() - 16);
                }
            }
        }
    }
    void finish() {
        if (doc_.links.empty())
            doc_.append("没有可显示的内容，请更换关键词或网址。");
    }
};
} // namespace browser
