// SPDX-License-Identifier: MIT
#pragma once
#include <pxa/app.hpp>
#include <pxa/game_upload.hpp>
#include <vector>
#include <cstdio>
#include <cstring>
#include "art.hpp"
#include "frame_decode.hpp"
namespace ash {
struct FramePixels {
    std::array<std::vector<std::byte>,art::pages> pages;
    std::array<std::byte,65556> upload;
    unsigned reads=0,uploads=0;
    std::array<bool,art::pages> inflight{};
    // The bank is embedded in memory: decoding performs no asynchronous I/O.
    // Immediate coroutine completions otherwise resume one another recursively
    // across the preload loop and exhaust the S31 native stack.
    pxa::Result<void> load(unsigned page){
        if(page>=art::pages)return std::unexpected(pxa::Error::invalid_argument);
        if(inflight[page])return std::unexpected(pxa::Error::would_block);
        if(!pages[page].empty())return pxa::Result<void>{};
        inflight[page]=true;
        auto& bytes=pages[page];bytes.resize(art::page_bytes[page]);
        if(!decodeFramePage(page,bytes)){
            bytes.clear();inflight[page]=false;return std::unexpected(pxa::Error::protocol_error);
        }
        inflight[page]=false;++reads;return pxa::Result<void>{};
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
