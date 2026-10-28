// SPDX-License-Identifier: MIT
#pragma once
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <string>
#include <string_view>
#include <vector>
#include "../media/json.hpp"

namespace study {
enum class Kind : uint8_t { math, poetry, words, hanzi, cards };
inline constexpr const char* levels[] = {"启蒙", "小学", "中学", "成人"};
struct Item {
    std::string question, answer, explanation, lookup;
    uint8_t level = 0;
    uint32_t id = 0;
};
inline uint32_t hash(std::string_view value) {
    uint32_t h = 2166136261u;
    for (unsigned char c : value) h = (h ^ c) * 16777619u;
    return h ? h : 1;
}
inline uint32_t identity(const Item& item) {
    return hash(item.question + "\n" + item.answer);
}
inline uint32_t random(uint32_t& seed) {
    if (!seed) seed = 0x12345678;
    seed ^= seed << 13; seed ^= seed >> 17; seed ^= seed << 5;
    return seed;
}
struct Review { uint32_t id = 0; uint16_t due = 0; uint8_t streak = 0, wrong = 0; };
struct Stats { uint32_t answered = 0, correct = 0; };
inline constexpr size_t review_capacity = 88;
struct Progress {
    uint8_t level = 0;
    uint32_t seed = 0x5a731953;
    std::array<Stats, 4> stats{};
    std::array<uint16_t, 4> bookmark{};
    std::array<Review, review_capacity> cards{};
    std::array<std::array<uint32_t,8>,4> math_mistakes{};
    Review* find(uint32_t id) {
        for (auto& r : cards) if (r.id == id) return &r;
        return nullptr;
    }
    const Review* find(uint32_t id) const {
        for (const auto& r : cards) if (r.id == id) return &r;
        return nullptr;
    }
    void record(uint32_t id, bool correct, uint16_t day) {
        auto& s = stats[level];
        if (s.answered < UINT32_MAX) ++s.answered;
        if (correct && s.correct < UINT32_MAX) ++s.correct;
        auto* r = find(id);
        if (!r) for (auto& free : cards) if (!free.id) { r = &free; break; }
        // Keep unresolved mistakes when the bounded review table fills.
        if (!r) for (auto& old : cards) if (!old.wrong && (!r || old.due < r->due)) r = &old;
        if (!r) return;
        if (r->id != id) *r = Review{id};
        r->wrong = correct ? 0 : 1;
        r->streak = correct ? std::min<unsigned>(r->streak + 1, 6) : 0;
        constexpr uint16_t days[] = {0, 1, 3, 7, 14, 30, 60};
        r->due = day ? uint16_t(std::min<unsigned>(65535, day + days[r->streak])) : 0;
    }
};
inline constexpr size_t mistakes_offset = 72 + review_capacity * 8;
inline constexpr size_t save_size = mistakes_offset + 4 * 32 + 8;
static_assert(save_size <= 960, "Keep snapshots within the embedded Storage value limit");
inline void put32(uint8_t* out, uint32_t n) { for (int i=0;i<4;++i) out[i]=uint8_t(n>>(8*i)); }
inline uint32_t get32(const uint8_t* in) { return uint32_t(in[0]) | uint32_t(in[1])<<8 | uint32_t(in[2])<<16 | uint32_t(in[3])<<24; }
inline std::array<uint8_t, save_size> encode(const Progress& p, Kind kind) {
    std::array<uint8_t, save_size> out{};
    std::memcpy(out.data(), "STDY", 4); out[4]=2; out[5]=uint8_t(kind); out[6]=p.level;
    put32(out.data()+8,p.seed);
    for (unsigned i=0;i<4;++i) {
        put32(out.data()+12+i*12,p.stats[i].answered);
        put32(out.data()+16+i*12,p.stats[i].correct);
        put32(out.data()+20+i*12,p.bookmark[i]);
    }
    for (unsigned i=0;i<p.cards.size();++i) {
        auto* o=out.data()+72+i*8; const auto& r=p.cards[i];
        put32(o,r.id); o[4]=uint8_t(r.due); o[5]=uint8_t(r.due>>8); o[6]=r.streak; o[7]=r.wrong;
    }
    for(unsigned level=0;level<4;++level)for(unsigned i=0;i<8;++i)
        put32(out.data()+mistakes_offset+level*32+i*4,p.math_mistakes[level][i]);
    put32(out.data()+save_size-4,hash({reinterpret_cast<const char*>(out.data()),save_size-4}));
    return out;
}
inline bool decode(Progress& out, Kind kind, const uint8_t* p, size_t n) {
    if (n!=save_size || std::memcmp(p,"STDY",4) || p[4]!=2 || p[5]!=uint8_t(kind) || p[6]>3 ||
        get32(p+n-4)!=hash({reinterpret_cast<const char*>(p),n-4})) return false;
    Progress next; next.level=p[6]; next.seed=get32(p+8);
    for(unsigned i=0;i<4;++i) {
        next.stats[i]={get32(p+12+i*12),get32(p+16+i*12)};
        if(next.stats[i].correct>next.stats[i].answered || get32(p+20+i*12)>65535) return false;
        next.bookmark[i]=uint16_t(get32(p+20+i*12));
    }
    for(unsigned i=0;i<next.cards.size();++i) {
        const auto* r=p+72+i*8;
        if(r[6]>6 || r[7]>1) return false;
        next.cards[i]={get32(r),uint16_t(r[4]|unsigned(r[5])<<8),r[6],r[7]};
    }
    for(unsigned level=0;level<4;++level)for(unsigned i=0;i<8;++i)
        next.math_mistakes[level][i]=get32(p+mistakes_offset+level*32+i*4);
    out=next; return true;
}
struct Question {
    Item item;
    std::array<std::string,4> options;
    unsigned correct=0;
};
inline void numeric_options(Question& q, int answer, uint32_t& seed, int maximum=INT_MAX) {
    std::array<int,4> options{answer,0,0,0};
    for(unsigned i=1;i<4;++i) {
        int v;
        bool duplicate;
        do {
            v=std::clamp(answer+int(random(seed)%21)-10,0,maximum);
            duplicate=std::find(options.begin(),options.begin()+i,v)!=options.begin()+i;
        } while(duplicate);
        options[i]=v;
    }
    for(unsigned i=3;i;--i) std::swap(options[i],options[random(seed)%(i+1)]);
    for(unsigned i=0;i<4;++i) {q.options[i]=std::to_string(options[i]);if(options[i]==answer)q.correct=i;}
}
[[gnu::always_inline]] inline Question math_question(unsigned level, uint32_t& seed) {
    Question q; char question[160], note[240]; int a,b,answer;
    const unsigned mode=random(seed)%4;
    if(level==0) {
        a=random(seed)%11; b=random(seed)%(11-a); answer=a+b;
        std::snprintf(question,sizeof(question),"%d + %d = ?",a,b);
        std::snprintf(note,sizeof(note),"从 %d 开始，再往后数 %d 个，得到 %d。",a,b,answer);
    } else if(level==1) {
        a=1+random(seed)%9; b=1+random(seed)%9;
        if(mode<2) {answer=a*b;std::snprintf(question,sizeof(question),"%d × %d = ?",a,b);
            std::snprintf(note,sizeof(note),"%d 个 %d 相加是 %d。反过来：%d ÷ %d = %d。",a,b,answer,answer,a,b);}
        else {a=10+random(seed)%90;b=random(seed)%(a+1);answer=a-b;
            std::snprintf(question,sizeof(question),"%d - %d = ?",a,b);
            std::snprintf(note,sizeof(note),"验算：%d + %d = %d。",answer,b,a);}
    } else if(level==2) {
        a=2+random(seed)%8; b=1+random(seed)%20; answer=1+random(seed)%20;
        const int total=a*answer+b;
        std::snprintf(question,sizeof(question),"%dx + %d = %d，x = ?",a,b,total);
        std::snprintf(note,sizeof(note),"先两边减去 %d，得到 %dx = %d；再除以 %d，x = %d。",b,a,total-b,a,answer);
    } else {
        a=(1+random(seed)%20)*100; b=(1+random(seed)%9)*10;answer=a*b/100;
        if(mode<2) {
            std::snprintf(question,sizeof(question),"%d 元的 %d%% 是多少元？",a,b);
            std::snprintf(note,sizeof(note),"%d × %d ÷ 100 = %d 元。",a,b,answer);
        } else {
            answer=a-a*b/100;
            std::snprintf(question,sizeof(question),"原价 %d 元，减价 %d%%，现价？",a,b);
            std::snprintf(note,sizeof(note),"减去 %d 元：%d - %d = %d 元。",a*b/100,a,a*b/100,answer);
        }
    }
    q.item={question,std::to_string(answer),note,"",uint8_t(level),0};q.item.id=identity(q.item);
    numeric_options(q,answer,seed,level==0?10:level==1?100:INT_MAX); return q;
}
inline std::vector<unsigned> selection(const std::vector<Item>& bank, Progress& p, bool review, uint16_t day) {
    std::vector<unsigned> result;
    for(unsigned i=0;i<bank.size();++i) if(bank[i].level==p.level) {
        const auto* r=p.find(bank[i].id);
        if(!review || (r && (r->wrong || (day && r->due<=day)))) result.push_back(i);
    }
    for(size_t i=result.size();i>1;--i) std::swap(result[i-1],result[random(p.seed)%i]);
    if(result.size()>10) result.resize(10);
    return result;
}
inline Question choice(const std::vector<Item>& bank, unsigned index, uint32_t& seed) {
    Question q; q.item=bank.at(index); q.options[0]=q.item.answer;
    std::vector<std::string> other;
    for(const auto& item:bank) if(item.answer!=q.item.answer &&
        std::find(other.begin(),other.end(),item.answer)==other.end()) other.push_back(item.answer);
    for(size_t i=other.size();i>1;--i) std::swap(other[i-1],other[random(seed)%i]);
    for(unsigned i=1;i<4;++i) q.options[i]=i<=other.size()?other[i-1]:"以上都不是";
    q.correct=random(seed)%4; std::swap(q.options[0],q.options[q.correct]);return q;
}
inline media::json::Value json(std::string_view s) {
    const auto* p=reinterpret_cast<const uint8_t*>(s.data());return {p,s.size()};
}
inline std::string string(media::json::Value value, size_t limit=1200) {
    std::vector<char> buffer(limit+1); bool truncated=false;
    return value.string(buffer.data(),buffer.size(),&truncated)&&!truncated?std::string(buffer.data()):std::string{};
}
inline bool complete_json(std::string_view data) {
    if(data.empty()) return false;
    const auto* p=reinterpret_cast<const uint8_t*>(data.data());
    media::json::Reader r{p,p+data.size()};if(!r.skip())return false;r.ws();return r.p==r.end;
}
// A small, explicit course format: validate completely before replacing content.
inline bool parse_course(std::string_view data, std::vector<Item>& out) {
    if(data.size()>8192 || !complete_json(data))return false;
    auto root=json(data); if(root.get("version").number()!=1)return false;
    media::json::Array rows(root.get("cards"));media::json::Value row;
    std::vector<Item> next;
    while(rows.next(row)) {
        if(next.size()>=24)return false;
        auto level=row.get("level").number(99);
        Item item{string(row.get("question"),180),string(row.get("answer"),120),
                  string(row.get("explanation"),480),"",uint8_t(level),0};
        if(level>3 || item.question.empty() || item.answer.empty())return false;
        item.id=identity(item);
        for(const auto& old:next)if(old.id==item.id)return false;
        next.push_back(std::move(item));
    }
    if(next.size()<4)return false;
    std::vector<std::string_view> answers;
    for(const auto& item:next)if(std::find(answers.begin(),answers.end(),item.answer)==answers.end())answers.push_back(item.answer);
    if(answers.size()<4)return false;
    out=std::move(next);return true;
}
struct Point { int x=0,y=0; };
using Stroke = std::vector<Point>;
inline bool parse_strokes(std::string_view data, std::vector<Stroke>& out) {
    if(data.size()>8192 || !complete_json(data))return false;
    media::json::Array strokes(json(data).get("medians")); media::json::Value stroke;
    std::vector<Stroke> next;
    while(strokes.next(stroke)) {
        if(next.size()>=32)return false;
        media::json::Array points(stroke);media::json::Value point;Stroke line;
        while(points.next(point)) {
            if(line.size()>=40)return false;
            media::json::Array axes(point);media::json::Value x,y,extra;
            if(!axes.next(x)||!axes.next(y)||axes.next(extra))return false;
            auto number=[](media::json::Value v,int& out) {
                std::string s(reinterpret_cast<const char*>(v.p),v.n); char* end=nullptr;
                long n=std::strtol(s.c_str(),&end,10);
                if(end!=s.c_str()+s.size() || n< -256 || n>1280)return false;
                out=int(n);return true;
            };
            Point p;if(!number(x,p.x)||!number(y,p.y))return false;line.push_back(p);
        }
        if(line.size()<2)return false;
        next.push_back(std::move(line));
    }
    if(next.empty())return false;
    out=std::move(next);return true;
}
inline unsigned cache_slot(std::string_view url) { return hash(url)%8; }
inline bool valid_feed_url(std::string_view url) {
    if(url.size()>384 || !url.starts_with("https://"))return false;
    const auto end=url.find('/',8); auto authority=url.substr(8,end==url.npos?url.size()-8:end-8);
    if(authority.empty() || authority.find('@')!=authority.npos)return false;
    for(unsigned char c:url)if(c<=32 || c==127)return false;
    return true;
}
} // namespace study
