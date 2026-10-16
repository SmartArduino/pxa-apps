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
    // CSS is not evaluated by this DOM. Drop only raw style contents while
    // streaming, retaining tags/attributes and never interpreting script text
    // or comments as HTML. No second buffer or larger capture quota is needed.
    for(char c:part) {
        if(mode_==Mode::style || mode_==Mode::script) {
            const bool style=mode_==Mode::style;
            const auto end=style ? "</style"sv : "</script"sv;
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
        if (!t.closing && (t.has("onclick") || t.has("onload") || t.has("onsubmit"))) script = true;
        if (!t.closing && (t.name == "style" || t.name == "script")) {
            auto end = raw_end(bytes_, t.name == "style" ? "</style" : "</script", at);
            if (end != bytes_.npos) at = end;
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
