#pragma once
#include "browser_core.hpp"
#include <chrono>
#include <limits>

namespace browser {
// Session-only and host-bound by design. Domain is validated but never widens
// scope to siblings: no bundled public-suffix database and no cross-site jar.
// Storage grows on demand, capped at 16 cookies / 4 KiB of strings.
class Cookies {
    struct Cookie {
        std::string host, path, name, value;
        std::uint64_t expires = 0;
        bool secure = false;
        std::size_t bytes() const { return host.size() + path.size() + name.size() + value.size(); }
    };
    std::vector<Cookie> items_;
    struct Address { std::string_view host, path; bool secure; };
    static Address address(std::string_view url) {
        const auto scheme = url.find("://");
        if (scheme == url.npos) return {};
        url.remove_prefix(scheme + 3);
        auto end = url.find_first_of("/?#");
        auto host = url.substr(0, end);
        if (host.starts_with('[')) host = host.substr(0, host.find(']') + 1);
        else host = host.substr(0, host.find(':'));
        auto path = end == url.npos ? "/"sv : url.substr(end);
        path = path.substr(0, path.find_first_of("?#"));
        return {host, path.empty() ? "/"sv : path, scheme == 5};
    }
    static bool domain_matches(std::string_view host, std::string_view domain) {
        return host == domain || (host.size() > domain.size() && host.ends_with(domain) &&
                                  host[host.size() - domain.size() - 1] == '.');
    }
    static bool path_matches(std::string_view path, std::string_view cookie) {
        return path == cookie || (path.starts_with(cookie) &&
               (cookie.ends_with('/') || path[cookie.size()] == '/'));
    }
    // RFC 6265 cookie-date tokenization, including obsolete date separators.
    static std::optional<std::int64_t> date(std::string_view value) {
        int day = -1, month = -1, year = -1, hour = -1, minute = -1, second = -1;
        auto number = [](std::string_view token, int &out) {
            auto [p, e] = std::from_chars(token.data(), token.data() + token.size(), out);
            return e == std::errc{} && p == token.data() + token.size();
        };
        for (std::size_t at = 0; at < value.size();) {
            auto delimiter = [](unsigned char c) {
                return c == 9 || (c >= 32 && c <= 47) || (c >= 59 && c <= 64) ||
                       (c >= 91 && c <= 96) || (c >= 123 && c <= 126);
            };
            if (delimiter(value[at])) { ++at; continue; }
            auto end = at;
            while (end < value.size() && !delimiter(value[end])) ++end;
            auto token = value.substr(at, end - at); at = end;
            if (hour < 0 && token.find(':') != token.npos) {
                auto a = token.find(':'), b = token.find(':', a + 1);
                if (b != token.npos && number(token.substr(0, a), hour) &&
                    number(token.substr(a + 1, b - a - 1), minute) && number(token.substr(b + 1), second)) continue;
                hour = minute = second = -1;
            }
            int n;
            if (day < 0 && token.size() <= 2 && number(token, n)) { day = n; continue; }
            if (month < 0 && token.size() >= 3) {
                auto m = std::string_view("janfebmaraprmayjunjulaugsepoctnovdec").find(lowercase(token.substr(0, 3)));
                if (m != std::string_view::npos && m % 3 == 0) { month = int(m / 3 + 1); continue; }
            }
            if (year < 0 && token.size() >= 2 && token.size() <= 4 && number(token, n)) year = n;
        }
        if (year >= 0 && year < 70) year += 2000;
        else if (year >= 70 && year < 100) year += 1900;
        using namespace std::chrono;
        const auto ymd = year_month_day{std::chrono::year(year), std::chrono::month(unsigned(month)), std::chrono::day(unsigned(day))};
        if (year < 1601 || year > 9999 || !ymd.ok() || hour < 0 || hour > 23 ||
            minute < 0 || minute > 59 || second < 0 || second > 59) return {};
        return duration_cast<seconds>(sys_days(ymd).time_since_epoch()).count() + hour * 3600 + minute * 60 + second;
    }
public:
    void clear() { std::vector<Cookie>{}.swap(items_); }
    std::size_t size() const { return items_.size(); }
    std::size_t bytes() const { std::size_t n = 0; for (auto &c : items_) n += c.bytes(); return n; }
    void receive(std::string_view url, std::string_view header, std::uint64_t now, std::int64_t wall) {
        if (header.empty() || header.size() > 1536 || std::ranges::any_of(header, [](unsigned char c) { return c < 32 || c == 127; })) return;
        const auto a = address(url);
        auto end = header.find(';');
        auto pair = trim(header.substr(0, end));
        auto eq = pair.find('=');
        if (a.host.empty() || eq == pair.npos) return;
        Cookie c; c.host = a.host; c.name = trim(pair.substr(0, eq)); c.value = trim(pair.substr(eq + 1));
        if (c.name.empty() || c.name.find_first_of("()<>@,;:\\\"/[]?={} \t") != c.name.npos || c.value.size() > 1024) return;
        auto last = a.path.rfind('/');
        c.path = last == 0 || last == a.path.npos ? "/" : std::string(a.path.substr(0, last));
        std::optional<std::int64_t> age, expires;
        bool domain_attribute = false, valid_domain = true;
        while (end != header.npos) {
            header.remove_prefix(end + 1); end = header.find(';');
            auto item = trim(header.substr(0, end)); auto equals = item.find('=');
            auto key = lowercase(trim(item.substr(0, equals)));
            auto value = equals == item.npos ? ""sv : trim(item.substr(equals + 1));
            if (key == "secure") c.secure = true;
            else if (key == "path" && value.starts_with('/')) c.path = value;
            else if (key == "domain") {
                domain_attribute = true;
                if (value.starts_with('.')) value.remove_prefix(1);
                auto domain = lowercase(value);
                valid_domain = !domain.empty() && !domain.ends_with('.') && domain_matches(a.host, domain);
            } else if (key == "max-age") {
                std::int64_t n; auto [p,e] = std::from_chars(value.data(), value.data()+value.size(), n);
                if (e == std::errc{} && p == value.data()+value.size()) age = n;
                else if (e == std::errc::result_out_of_range) age = value.starts_with('-') ? -1 : 31536000;
            } else if (key == "expires") expires = date(value);
        }
        if (!valid_domain || (c.secure && !a.secure) ||
            (c.name.starts_with("__Secure-") && (!c.secure || !a.secure)) ||
            (c.name.starts_with("__Host-") && (!c.secure || !a.secure || domain_attribute || c.path != "/"))) return;
        auto old = std::ranges::find_if(items_, [&](auto &v) { return v.host == c.host && v.path == c.path && v.name == c.name; });
        // An insecure response cannot replace a secure cookie with this key.
        if (old != items_.end() && old->secure && !a.secure) return;
        bool remove = age ? *age <= 0 : expires && wall > 0 && *expires <= wall;
        if (remove) { if (old != items_.end()) items_.erase(old); return; }
        if (age) c.expires = now + std::uint64_t(std::min<std::int64_t>(*age, 31536000));
        else if (expires && wall > 0) c.expires = now + std::uint64_t(std::min<std::int64_t>(*expires - wall, 31536000));
        const auto used = bytes() - (old == items_.end() ? 0 : old->bytes());
        if (used + c.bytes() > 4096 || (old == items_.end() && items_.size() >= 16)) return;
        if (old != items_.end()) *old = std::move(c);
        else items_.push_back(std::move(c));
    }
    std::string request(std::string_view url, std::uint64_t now) {
        std::erase_if(items_, [&](auto &c) { return c.expires && c.expires <= now; });
        auto a = address(url);
        std::array<const Cookie*,16> selected{}; std::size_t count = 0;
        for (auto &c : items_) if (c.host == a.host && (!c.secure || a.secure) && path_matches(a.path, c.path)) selected[count++] = &c;
        // Stable longest-path first, then creation order. Insertion sort is
        // bounded, allocation-free, and does not disturb replacement ordering.
        for (std::size_t i=1;i<count;++i) for (std::size_t j=i;j>0 && selected[j]->path.size()>selected[j-1]->path.size();--j) std::swap(selected[j],selected[j-1]);
        std::string result;
        for (std::size_t i=0;i<count;++i) {
            if (!result.empty()) result += "; ";
            result += selected[i]->name; result += '='; result += selected[i]->value;
        }
        return result;
    }
};
} // namespace browser
