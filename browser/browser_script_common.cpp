#include "browser_script.hpp"
#include "browser_script_markup.hpp"

namespace browser::script {
using namespace detail;
void Capture::append(std::string_view part, std::size_t limit) {
    if (limited_) return;
    auto keep=[&](char c) {
        if(bytes_.size()>=limit) { limited_=true;return false; }
        if(bytes_.size()==bytes_.capacity() && bytes_.capacity()>limit/2) bytes_.reserve(limit);
        bytes_+=c;return true;
    };
    // Preserve only a bounded CSS prefix for the subsequent DOM repaint. Delay
    // candidate closing bytes so a quota boundary cannot split the closing tag.
    // Scripts/comments keep their existing capture and policy behavior.
    for(char c:part) {
        if(mode_==Mode::plaintext) {if(!keep(c))return;continue;}
        if(mode_==Mode::style || mode_==Mode::script || mode_==Mode::raw) {
            const bool style=mode_==Mode::style;
            const auto end=style ? "</style"sv : mode_==Mode::script ? "</script"sv : raw_end_;
            if(style) {
                auto css_byte=[&](char item) {
                    if(!style_enabled_ || style_bytes_>=browser::css::max_bytes)return true;
                    ++style_bytes_; return keep(item);
                };
                if(match_==end.size() && tag_boundary(c)) {
                    tag_start_=bytes_.size();
                    for(char item:end)if(!keep(item))return;
                    if(!keep(c))return;
                    mode_=c=='>' ? Mode::text : Mode::tag;quote_=0;match_=0;continue;
                }
                if(match_<end.size() && lower(c)==end[match_]) {
                    style_candidate_[match_++]=c;continue;
                }
                for(unsigned i=0;i<match_;++i)if(!css_byte(style_candidate_[i]))return;
                match_=0;
                if(c=='<')style_candidate_[match_++]=c;
                else if(!css_byte(c))return;
                continue;
            }
            if(!style && !keep(c)) return;
            if(match_==end.size()) {
                if(tag_boundary(c)) {
                    if(style) {
                        tag_start_=bytes_.size();
                        for(char item:end) if(!keep(item)) return;
                        if(!keep(c))return;
                    } else tag_start_=bytes_.size()-end.size()-1;
                    mode_=c=='>' ? Mode::text : Mode::tag;quote_=0;match_=0;
                    continue;
                }
                match_=0;
            }
            match_=lower(c)==end[match_] ? match_+1 : (c=='<' ? 1 : 0);
            continue;
        }
        if(!keep(c)) return;
        if(mode_==Mode::comment) {
            if(bytes_.ends_with("-->"))mode_=Mode::text;
        } else if(mode_==Mode::text) {
            if(c=='<') {mode_=Mode::tag;tag_start_=bytes_.size()-1;quote_=0;}
        } else {
            auto text=std::string_view(bytes_).substr(tag_start_+1);
            if(text=="!--") {mode_=Mode::comment;continue;}
            if(quote_) {if(c==quote_)quote_=0;continue;}
            if(c=='\'' || c=='"') {quote_=c;continue;}
            if(c!='>')continue;
            text=trim(text.substr(0,text.size()-1));
            const auto name=text.substr(0,text.find_first_of(" \t\r\n\f/"));
            mode_=equal_ascii(name,"style") ? Mode::style : equal_ascii(name,"script") ? Mode::script : Mode::text;
            if(mode_==Mode::text) {
                const auto text_name=browser::lowercase(name);
                const auto kind=browser::html_text(text_name);
                if(kind==browser::HtmlText::plaintext)mode_=Mode::plaintext;
                else if(kind!=browser::HtmlText::data) {mode_=Mode::raw;raw_end_=browser::text_end_tag(text_name);}
            }
            if(mode_==Mode::style) {
                const auto parsed=browser::tag(text);
                style_enabled_=browser::css::active_style(parsed.get("media"),parsed.get("type"));
            }
            match_=0;
        }
    }
}
bool Capture::has_script() const noexcept {
    std::size_t at = 0; bool script = false;
    while (at < bytes_.size()) {
        const auto item = token(bytes_, at);
        if (!item.tag) continue;
        const auto t = browser::tag(item.text);
        if (!t.closing && t.name == "meta" && lowercase(t.get("http-equiv")) == "content-security-policy") return false;
        if (!t.closing && t.name == "script" && executable(t)) script = true;
        if (!t.closing) {
            // A disabled input/button cannot activate its click handler by
            // itself. Other events or scripts may still enable that control.
            const bool click = t.has("onclick") &&
                !((t.name == "input" || t.name == "button") && t.has("disabled"));
            if (click || t.has("onload") || t.has("onsubmit")) script = true;
        }
        if (!t.closing && (t.name == "style" || t.name == "script")) {
            auto end = raw_end(bytes_, t.name == "style" ? "</style" : "</script", at);
            if (end != bytes_.npos) at = end;
        } else if(!t.closing) {
            const auto kind=browser::html_text(t.name);
            if(kind==browser::HtmlText::plaintext)break;
            if(kind!=browser::HtmlText::data) {
                const auto end=raw_end(bytes_,browser::text_end_tag(t.name),at);
                at=end==bytes_.npos?bytes_.size():end;
            }
        }
    }
    return script;
}
bool same_origin(std::string_view a, std::string_view b) {
    const auto first = origin(a);
    return !first.empty() && first == origin(b);
}
bool script_url_allowed(std::string_view page, std::string_view target, bool external_script) {
    const auto source = origin(page), destination = origin(target);
    if (source.empty() || destination.empty()) return false;
    if (source.starts_with("https:") && !destination.starts_with("https:")) return false;
    return external_script || same_origin(page, target);
}

std::optional<std::string> Storage::get(std::string_view url, std::string_view key, bool session) const {
    const auto scope = origin(url);
    for (const auto& e : entries_) if (e.origin == scope && e.key == key && e.session == session) return e.value;
    return {};
}
bool Storage::set(std::string_view url, std::string_view key, const std::string* value, bool session, bool clear_all) {
    const auto scope = origin(url);
    if (scope.empty() || key.size() > 128 || (value && value->size() > 4096)) return false;
    if (!value) { std::erase_if(entries_, [&](const auto& e) { return e.origin == scope && e.session == session && (clear_all || e.key == key); }); return true; }
    auto old = std::ranges::find_if(entries_, [&](const auto& e) { return e.origin == scope && e.key == key && e.session == session; });
    std::size_t bytes = scope.size() + key.size() + value->size();
    for (const auto& e : entries_) if (&e != (old == entries_.end() ? nullptr : &*old)) bytes += e.origin.size() + e.key.size() + e.value.size();
    if (bytes > 8192 || (old == entries_.end() && entries_.size() >= 32)) return false;
    if (old == entries_.end()) entries_.push_back({scope, std::string(key), *value, session});
    else old->value = *value;
    return true;
}

} // namespace browser::script
