#pragma once
#include "protocol.hpp"

namespace weread {
struct ChapterLink {std::string title,param;std::uint32_t uid=0;};
struct Catalog {
    static constexpr std::size_t capacity=300;
    std::vector<ChapterLink> items;
    std::uint32_t begin=0,end=0,total=0;
    std::uint32_t wire_begin=0,wire_end=0,source_rows=0,last_chapter=0;
    bool reached_start=false,reached_end=false,wire_known=true;
    // Retain one bounded outline window. Re-fetch another window on demand,
    // rather than losing subheadings or caching the complete HTML indefinitely.
    bool initial(Json reader){
        auto [chapters,fallback,params,chapter,window,book,count]=reader.select({
            "chapterIndexes","chapterInfos","readerUrlParams","curChapter","catalogWindow","bookInfo","chapterInfoCount"});
        if(chapters.raw().empty())chapters=fallback;
        if(params.raw().empty())params=Json("[]");
        if(chapters.raw().empty())chapters=Json("[]");
        const auto current=chapter.get("chapterUid").number();
        std::size_t length=0,selected=0;
        if(!chapters.each([&](Json row){if(row.get("chapterUid").number()==current)selected=length;++length;return true;},8192))return false;
        const auto skip=length<=capacity?0:std::min(selected>capacity/2?selected-capacity/2:0,length-capacity);
        Catalog next;
        next.source_rows=window.get("length").number().value_or(length);
        const auto offset=window.get("offset").number().value_or(0);
        if(skip>UINT32_MAX-offset)return false;
        next.begin=offset+skip;
        next.last_chapter=book.get("lastChapterIdx").number().value_or(0);
        next.total=std::max(count.number().value_or(0),next.source_rows);
        if(!next.read(chapters,params,skip,true))return false;
        if(next.items.size()>UINT32_MAX-next.begin)return false;
        next.end=next.begin+next.items.size();
        if(next.end>next.source_rows)return false;
        *this=std::move(next);return true;
    }
    std::optional<std::uint32_t> local_window(int direction)const{
        if(direction<0&&begin)return begin>capacity?begin-capacity:0;
        if(direction>0&&end<source_rows)return end;
        return {};
    }
    bool can_load(int direction)const{
        if(local_window(direction))return true;
        if(items.empty()||!wire_known)return false;
        return direction<0?!reached_start&&wire_begin>0:
            !reached_end&&(last_chapter?wire_end<last_chapter:source_rows<total);
    }
    bool page(Json response,int direction){
        if(!response.valid()||response.get("succ").number()!=1 || (direction!=1&&direction!=-1))return false;
        Catalog next;next.total=total;next.last_chapter=last_chapter;
        // The official Kindle client uses this historical misspelling on wire.
        if(!next.read(response.get("chapterInfos"),response.get("readerUrlParmas")))return false;
        if(next.items.empty()){
            // A successful empty boundary is not an error or an endless retry.
            (direction<0?reached_start:reached_end)=true;return true;
        }
        if(!next.wire_known || (direction>0?next.wire_begin<=wire_end:next.wire_end>=wire_begin))return false;
        next.end=next.source_rows=next.items.size();
        *this=std::move(next);return true;
    }
private:
    bool read(Json chapters,Json params,std::size_t skip=0,bool windowed=false){
        std::size_t at=0;bool valid=true;
        if(!chapters.each([&](Json row){
            if(at++<skip)return true;
            if(items.size()==capacity){if(!windowed)valid=false;return windowed;}
            auto uid=row.get("chapterUid").number();
            if(!uid){valid=false;return false;}
            const auto wire_index=row.get("chapterIdx").number();
            if(!wire_index)wire_known=false;
            const auto index=wire_index.value_or(at-1);
            if(!items.empty()&&index<wire_end){valid=false;return false;}
            if(items.empty())wire_begin=index;wire_end=index;
            auto title=scalar(row.get("title"),384);
            if(title.empty())title="第 "+number(index)+" 章";
            items.push_back({std::move(title),{},*uid});return true;
        },8192)||!valid)return false;
        std::optional<std::uint32_t> previous_uid;std::string_view previous_param;
        if(!params.each([&](Json row){
            auto uid=row.get("cUid").number();if(!uid)uid=row.get("chapterUid").number();
            if(!uid){valid=false;return false;}
            // Nested headings share a chapter UID and its authorized URL.
            // Repeated subheading links are common. Borrow the previous raw
            // parameter to skip identical adjacent assignments without a map.
            const auto raw_param=row.get("param").raw();
            if(uid==previous_uid&&raw_param==previous_param)return true;
            previous_uid=uid;previous_param=raw_param;
            const auto param=scalar(Json(raw_param),1024);
            for(auto& chapter:items)if(chapter.uid==*uid)chapter.param=param;
            return true;
        },8192)||!valid)return false;
        return true;
    }
};
} // namespace weread
