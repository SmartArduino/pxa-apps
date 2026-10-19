#pragma once
#include "book_image.hpp"

namespace weread {
struct PageLine {
    std::uint32_t start=0,end=0;
    std::uint16_t x=0,y=0,width=0,height=0,image=65535;
    bool is_image()const{return image!=65535;}
};
struct ReadingPage {
    std::array<PageLine,64> lines{};
    std::size_t count=0,end=0;
};
// A page-sized list of text/image runs. No document-sized layout tree or
// bitmap cache. Text positions remain UTF-8 offsets; images own one U+FFFC.
inline ReadingPage paginate_document(std::string_view text,std::span<const BookImage> images,
    std::size_t start,int width,int height,int pixels,int leading,bool show_images=true){
    ReadingPage page;page.end=start;
    if(start>text.size()||width<pixels||pixels<8||height<leading||width>65535||height>65535||leading<pixels)return page;
    std::size_t at=start,text_begin=start,row_begin=0,row_start=start,commands=0,row_commands=0;
    int x=0,y=0,row_height=leading,text_x=0;
    auto image_it=std::ranges::lower_bound(images,start,{},&BookImage::offset);
    auto flush_text=[&]{
        if(at==text_begin)return true;
        const auto bytes=at-text_begin+28;
        if(page.count==page.lines.size()||commands+bytes>3000)return false;
        page.lines[page.count++]={std::uint32_t(text_begin),std::uint32_t(at),std::uint16_t(text_x),std::uint16_t(y),
            std::uint16_t(x-text_x),std::uint16_t(leading)};
        commands+=bytes;return true;
    };
    auto finish_row=[&]{
        for(auto i=row_begin;i<page.count;++i)page.lines[i].y+=std::uint16_t((row_height-page.lines[i].height)/2);
        page.end=at;y+=row_height;x=text_x=0;row_height=leading;
        row_begin=page.count;row_start=text_begin=at;row_commands=commands;
    };
    auto rollback_row=[&]{page.count=row_begin;page.end=row_start;commands=row_commands;};
    while(at<text.size()&&y+leading<=height){
        auto glyph=rune(text.substr(at));
        if(image_it!=images.end()&&image_it->offset==at&&glyph.value==0xfffc){
            if(!flush_text()){rollback_row();return page;}
            int image_width=width,image_height=leading;
            bool block=true;
            if(show_images){
                if(image_it->width_em_q8){
                    image_width=std::clamp(int(std::int64_t(pixels)*image_it->width_em_q8/256),1,width);
                    block=image_width>width*3/4;
                }
                image_height=std::max(1,int(std::int64_t(image_width)*65536/std::max(1u,image_it->aspect_q16)));
                if(image_height>height){image_height=height;image_width=std::max(1,int(std::int64_t(height)*image_it->aspect_q16/65536));image_width=std::min(image_width,width);}
            }
            if((block&&x)||(x+image_width>width))finish_row();
            if(y+std::max(row_height,image_height)>height){rollback_row();return page;}
            if(page.count==page.lines.size()||commands+48>3000){finish_row();return page;}
            const auto index=std::size_t(image_it-images.begin());
            if(index>=65535){rollback_row();return page;}
            page.lines[page.count++]={std::uint32_t(at),std::uint32_t(at+glyph.bytes),std::uint16_t(block?(width-image_width)/2:x),
                std::uint16_t(y),std::uint16_t(image_width),std::uint16_t(image_height),std::uint16_t(index)};
            commands+=48;row_height=std::max(row_height,image_height);x+=image_width;
            at+=glyph.bytes;++image_it;text_begin=at;text_x=x;
            if(block)finish_row();
            continue;
        }
        if(glyph.value=='\n'){
            if(!flush_text()){rollback_row();return page;}
            at+=glyph.bytes;finish_row();continue;
        }
        const int advance=glyph.value==' '?std::max(1,pixels/2):pixels;
        if(x+advance>width){
            if(!flush_text()){rollback_row();return page;}
            finish_row();continue;
        }
        at+=glyph.bytes;x+=advance;
    }
    if(at==text.size()){
        if(!flush_text()){rollback_row();return page;}
        finish_row();
    }
    return page;
}
// Kept for text-only protocol tests and callers. No image metadata allocation.
inline ReadingPage paginate(std::string_view text,std::size_t start,int width,int line_count,int pixels){
    const auto rows=std::clamp(line_count,0,64);
    return paginate_document(text,{},start,width,rows*pixels,pixels,pixels);
}
} // namespace weread
