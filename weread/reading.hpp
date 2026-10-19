#pragma once
#include "hydration.hpp"
#include "html.hpp"
#include "catalog.hpp"
#include "book_image.hpp"

namespace weread {
enum class ReadingGrant {unknown,purchased,chapter_purchased,free,limited_free,preview};
struct Reading {
    std::string title,book_id,chapter_title,text,current,previous,next,token;
    Catalog catalog;
    std::vector<BookImage> images;
    std::uint32_t section=0,sections=0,chapter_uid=0,chapter_index=0,total_chapters=0;
    bool requires_purchase=false,unavailable=false;
    ReadingGrant grant=ReadingGrant::unknown;
    bool sold_out=false,content_error=false;
    std::string_view access_label()const{
        switch(grant){
        case ReadingGrant::purchased:return "已购";
        case ReadingGrant::chapter_purchased:return "部分章节已购";
        case ReadingGrant::free:return "免费可读";
        case ReadingGrant::limited_free:return "限时免费";
        case ReadingGrant::preview:return "当前可试读";
        default:return {};
        }
    }
    std::string_view denied_message()const{
        if(content_error)return "服务端未返回本章正文；已购图书请核对官方网页版的阅读权限";
        if(requires_purchase)return "本章暂无阅读授权，可在官方客户端查看购买或限免资格";
        return "本章暂不可用，请稍后重试";
    }
    bool parse(Json data) {
        if(data.raw().empty() || data.raw().front()!='{')return false;
        Reading candidate;
        const auto [book,chapter,current,previous,next,token,section,sections,total,error,purchase,body_json]=data.select({
            "bookInfo","curChapter","curChapterUrlParam","prevChapterUrlParam","nextChapterUrlParam",
            "token","curChapterSection","chapterSections","chapterInfoCount","errorInfo","isCurChapterNeedToPay","contentText"});
        candidate.title=scalar(book.get("title"),384);candidate.book_id=scalar(book.get("bookId"),128);
        candidate.chapter_title=scalar(chapter.get("title"),384);
        candidate.chapter_uid=chapter.get("chapterUid").number().value_or(0);
        candidate.chapter_index=chapter.get("chapterIdx").number().value_or(0);
        candidate.current=scalar(current.get("param"),1024);
        candidate.previous=scalar(previous.get("param"),1024);
        candidate.next=scalar(next.get("param"),1024);
        candidate.token=scalar(token,1024);
        candidate.section=section.number().value_or(0);
        sections.each([&](Json){++candidate.sections;return true;},8192);
        candidate.total_chapters=total.number().value_or(0);
        const auto raw=error.raw();
        candidate.unavailable=!raw.empty() && raw!="null" && raw!="false" && raw!="0" && raw!="\"\"" && raw!="{}";
        const auto code=error.get("code").raw();
        if(code=="0")candidate.unavailable=false;
        candidate.content_error=code=="-505";
        candidate.requires_purchase=purchase.raw()=="true" || purchase.number()==1;
        candidate.sold_out=book.get("soldout").number().value_or(0)>0;
        const auto pay_type=book.get("payType").number().value_or(0);
        if(book.get("free").number()==1 || (pay_type&32))candidate.grant=ReadingGrant::free;
        else if(pay_type&64)candidate.grant=ReadingGrant::limited_free;
        else if(book.get("paid").number()==1)candidate.grant=ReadingGrant::purchased;
        else if(book.get("isChapterPaid").number()==1)candidate.grant=ReadingGrant::chapter_purchased;
        else if(book.get("paid").number()==0 && book.get("maxFreeChapter").number().value_or(0)>0)
            candidate.grant=ReadingGrant::preview;
        // Ownership hints never gate a chapter: memberships, limited free
        // events and paid chapter grants are decided by the official response.
        auto body=body_json.string(196608);
        if(body)candidate.text=std::move(*body);
        if(!candidate.catalog.initial(data))return false;
        if(candidate.title.empty() && candidate.book_id.empty() && candidate.current.empty() && !candidate.unavailable)return false;
        *this=std::move(candidate);return true;
    }
    std::string adjacent(int direction) const {
        if(direction>0 && section+1<sections && !current.empty())return "?bc="+percent(current)+"&sect="+number(section+1);
        if(direction<0 && section>0 && !current.empty())return "?bc="+percent(current)+"&sect="+number(section-1)+"&prev=1";
        const auto& p=direction>0?next:previous;if(p.empty())return {};
        return "?bc="+percent(p)+(direction<0?"&prev=1":"");
    }
};
inline std::string reader_text(std::string_view html,std::span<const std::byte> mapping={},std::vector<BookImage>* images=nullptr,bool* image_limit=nullptr) {
    std::string out;out.reserve(html.size());
    std::size_t image_bytes=0;
    if(images)images->clear();if(image_limit)*image_limit=false;
    bool spacing=false;auto newline=[&]{if(!out.empty()&&out.back()!='\n')out+='\n';spacing=false;};
    for(std::size_t at=0;at<html.size();){
        if(html[at]=='<'){
            html::Tag tag;auto next=at;
            if(!html::next(html,next,tag)||tag.begin!=at){
                // Comments and declarations contain no book prose. A literal
                // less-than remains visible instead of dropping the tail.
                if(html.substr(at).starts_with("<!--")){
                    auto end=html.find("-->",at+4);at=end==html.npos?html.size():end+3;continue;
                }
                if(html.substr(at).starts_with("<!")){
                    auto end=html.find('>',at+2);at=end==html.npos?html.size():end+1;continue;
                }
            }else{
                if(images&&!tag.closing&&html::equal(tag.name,"img")){
                    if(spacing&&!out.empty()&&out.back()!='\n')out+=' ';spacing=false;
                    auto image=book_image(tag,std::uint32_t(out.size()));
                    image_bytes+=2*sizeof(BookImage)+image.url.size()+image.alt.size()+2;
                    if(image_bytes>document_image_budget){if(image_limit)*image_limit=true;images->clear();return {};}
                    images->push_back(std::move(image));append_rune(out,0xfffc);
                }
                if((tag.closing&&(html::equal(tag.name,"p")||html::equal(tag.name,"div")||
                    html::equal(tag.name,"li")||html::equal(tag.name,"blockquote")||
                    (tag.name.size()==2&&html::lower(tag.name[0])=='h'&&tag.name[1]>='1'&&tag.name[1]<='6')))||
                    html::equal(tag.name,"br"))newline();
                if(!tag.closing&&(html::equal(tag.name,"script")||html::equal(tag.name,"style")))
                    next=html::raw_end(html,next,tag.name);
                at=next;continue;
            }
        }
        if(html[at]=='&'){
            auto end=html.find(';',at+1);std::uint32_t cp=0;
            if(end!=html.npos&&end-at<16)if(auto value=html::entity(html.substr(at+1,end-at-1))){
                cp=*value;
                if(cp==' ')spacing=true;
                else {if(spacing&&!out.empty()&&out.back()!='\n')out+=' ';spacing=false;append_rune(out,cp);}
                at=end+1;continue;
            }
        }
        auto code=rune(html.substr(at));at+=code.bytes;
        if(code.value=='\n'||code.value=='\r'||code.value=='\t'||code.value==' '){spacing=true;continue;}
        if(spacing&&!out.empty()&&out.back()!='\n')out+=' ';
        spacing=false;
        if(mapping.size()==41984 && code.value>=0x4e00 && code.value<=0x9fff){
            const auto i=(code.value-0x4e00)*2;
            code.value=std::to_integer<unsigned>(mapping[i])|(std::to_integer<unsigned>(mapping[i+1])<<8);
        }
        append_rune(out,code.value);
    }
    while(!out.empty()&&(out.back()=='\n'||out.back()==' '))out.pop_back();
    return out;
}
} // namespace weread
#include "document_layout.hpp"
