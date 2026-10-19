#pragma once
#include "agent.hpp"

namespace weread::agent {
enum class NoteKind : std::uint8_t {highlights,thoughts};
inline constexpr std::size_t note_window=10,note_response_limit=131072,note_document_limit=32768;
struct NotePage {
    std::string body;
    std::int64_t next=0;
    std::size_t count=0;
    bool more=false;
};
// Only the currently open notes tab owns navigation. Intro/reading keep no
// notes buffer, index or cursor history. Responses are released after commit.
struct NoteNavigation {
    NoteKind kind;
    std::int64_t cursor=0,next=0;
    std::vector<std::int64_t> history;
    bool more=false;
};
inline std::optional<std::int64_t> note_cursor(Json value){
    const auto raw=value.raw();std::int64_t n=0;
    if(raw.empty())return {};
    auto [end,error]=std::from_chars(raw.data(),raw.data()+raw.size(),n);
    if(error!=std::errc{}||end!=raw.data()+raw.size()||n<0)return {};
    return n;
}
inline std::optional<std::string> note_text(Json value,std::size_t limit){
    auto text=value.string(limit);
    if(!text)return {};
    Utf8Validator utf8;std::size_t written=0;
    for(std::size_t i=0;i<text->size();++i){
        const auto c=static_cast<unsigned char>((*text)[i]);
        if(!utf8.append(c)||(c<32&&c!='\n'&&c!='\r'&&c!='\t')||c==127)return {};
        if(c=='\r'){
            (*text)[written++]='\n';
            if(i+1<text->size()&&(*text)[i+1]=='\n')++i;
        }else (*text)[written++]=char(c);
    }
    if(!utf8.complete())return {};
    text->resize(written);return text;
}
[[gnu::noinline]] inline bool note_append(std::string& out,std::string_view text){
    if(text.size()>note_document_limit-out.size())return false;
    out+=text;return true;
}
// All marks can arrive in one reply, so keep a local window and refetch when
// changing it. Thoughts use the server's opaque integer cursor, never an offset.
// Selected text is validated before commit; malformed JSON, identity/type or
// selected-row errors retain the previous document atomically.
inline std::optional<NotePage> note_page(std::string_view raw,std::string_view book,
    NoteKind kind,std::int64_t cursor=0){
    Json data(raw);
    if(book.empty()||book.size()>128||cursor<0||raw.size()>note_response_limit||status(data)!=Status::ok)return {};
    const bool thoughts=kind==NoteKind::thoughts;
    if(!thoughts&&kind!=NoteKind::highlights)return {};
    const std::string_view keys[]={thoughts?"reviews":"updated","chapters"};
    const auto [entries,chapters]=data.select(keys);
    NotePage page;std::size_t seen=0;bool valid=true;
    const auto append=[&](std::string_view text){return note_append(page.body,text);};
    if(!entries.each([&](Json entry){
        auto row=thoughts?entry.get("review"):entry;
        if(scalar(row.get("bookId"),128)!=book||(!thoughts&&row.get("type").number()!=1))return valid=false;
        const auto index=seen++;
        if(!thoughts&&(index<std::uint64_t(cursor)||page.count==note_window))return true;
        if(thoughts&&page.count==note_window)return valid=false;
        if(page.count&&!append("\n\n────────\n\n"))return valid=false;
        Json title;
        if(thoughts){
            title=row.get("chapterName");
            if(title.raw().empty())title=row.get("chapterTitle");
        }else if(!chapters.raw().empty()){
            const auto chapter_uid=row.get("chapterUid").raw();
            if(!chapters.each([&](Json chapter){
                if(chapter.get("chapterUid").raw()!=chapter_uid)return true;
                title=chapter.get("title");return false;
            },8192))return valid=false;
        }
        // Decode/normalize each displayed field through one bounded loop.
        // The chapter UID and root chapter array are borrowed once per row/page,
        // rather than looking them up again for every chapter candidate.
        const std::array<Json,3> fields{title,row.get(thoughts?"abstract":"markText"),thoughts?row.get("content"):Json{}};
        const unsigned count=thoughts?3:2;
        for(unsigned i=0;i<count;++i){
            const bool primary=i+1==count;
            const auto value=fields[i];
            if(!primary&&(value.raw().empty()||value.raw()=="null"))continue;
            auto text=note_text(value,i?8192:384);
            if(!text)return valid=false;
            const std::string_view rendered=primary&&text->empty()?"（无文字内容）":std::string_view(*text);
            if(rendered.empty())continue;
            const std::string_view prefix=i==0?"":primary?(thoughts?"\n想法：":"\n"):"\n原文：";
            if(!append(prefix)||!append(rendered))return valid=false;
        }
        ++page.count;return true;
    },thoughts?note_window:8192)||!valid)return {};
    if(thoughts){
        const auto more=data.get("hasMore").raw();
        if(more!="0"&&more!="1"&&more!="false"&&more!="true")return {};
        page.more=more=="1"||more=="true";
        if(page.more){
            auto next=note_cursor(data.get("synckey"));
            if(!next||*next==cursor)return {};
            page.next=*next;
        }
    }else{
        if(std::uint64_t(cursor)>seen)return {};
        page.next=cursor+std::int64_t(page.count);
        page.more=std::uint64_t(page.next)<seen;
    }
    if(!page.count)page.body=thoughts?"本页暂无个人想法":"暂无划线";
    return page;
}
} // namespace weread::agent
