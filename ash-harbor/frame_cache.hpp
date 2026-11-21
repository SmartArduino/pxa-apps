// SPDX-License-Identifier: MIT
#pragma once
#include <pxa/app.hpp>
#include <pxa/game_upload.hpp>
#include <vector>
#include <cstdio>
#include <cstring>
#include "art.hpp"
#include "frame_dimensions.hpp"
namespace ash {
struct FramePixels {
    std::array<std::vector<std::byte>,art::pages> pages;
    std::array<std::byte,65556> upload;
    unsigned reads=0,uploads=0;
    std::array<bool,art::pages> inflight{};
    pxa::Task<void> load(pxa::Context& ctx,unsigned page){
        if(inflight[page])co_return std::unexpected(pxa::Error::would_block);
        if(!pages[page].empty())co_return pxa::Result<void>{};
        inflight[page]=true;
        auto& bytes=pages[page];bytes.resize(art::page_bytes[page]);
        unsigned offset=0,position=art::page_offset[page],end=position+art::page_rle_bytes[page];
        const auto* packed=reinterpret_cast<const unsigned char*>(art::frame_rle);
        bool valid=end<=sizeof(art::frame_rle)-1;
        while(valid&&position<end){
            unsigned control=packed[position++],count=(control&127u)+1;
            if(count>bytes.size()-offset){valid=false;break;}
            if(control&128u){
                if(position==end){valid=false;break;}
                std::fill_n(bytes.data()+offset,count,std::byte(packed[position++]));
            }else{
                if(count>end-position){valid=false;break;}
                std::memcpy(bytes.data()+offset,packed+position,count);position+=count;
            }
            offset+=count;
        }
        if(!valid||offset!=bytes.size()){
            bytes.clear();inflight[page]=false;co_return std::unexpected(pxa::Error::protocol_error);
        }
        inflight[page]=false;++reads;co_return pxa::Result<void>{};
    }
    pxa::Result<void> bind(pxa::game::Renderer& renderer,unsigned page,unsigned slot){
        auto result=pxa::game::Upload(renderer,upload).texture({uint8_t(slot)},
            art::page_width[page],art::page_height[page],pages[page]);
        if(result)++uploads;return result;
    }
    static std::array<bool,art::pages> fighterPages(int a,int b){
        std::array<bool,art::pages> wanted{};
        for(int c:{a,b}){
            for(auto tile:art::fighters[c][0])wanted[tile.page]=true;
            for(auto tile:art::extras[c])wanted[tile.page]=true;
            for(auto tile:art::walk[c])wanted[tile.page]=true;
        }
        return wanted;
    }
    void retain(const std::array<bool,art::pages>& wanted){
        for(unsigned i=art::static_pages;i<art::pages;++i)if(!wanted[i])std::vector<std::byte>().swap(pages[i]);
    }
    unsigned bytes()const{unsigned n=0;for(const auto& page:pages)n+=page.capacity();return n;}
};
}
