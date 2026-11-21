// SPDX-License-Identifier: MIT
#pragma once
#include <algorithm>
#include <cstddef>
#include <cstring>
#include <span>
#include "art.hpp"
#include "frame_dimensions.hpp"

namespace ash {
// Decode directly from the embedded bank with constant stack use and no I/O.
inline bool decodeFramePage(unsigned page,std::span<std::byte> bytes){
    if(page>=art::pages||bytes.size()!=art::page_bytes[page])return false;
    unsigned offset=0,position=art::page_offset[page],end=position+art::page_rle_bytes[page];
    const auto* packed=reinterpret_cast<const unsigned char*>(art::frame_rle);
    if(end>sizeof(art::frame_rle)-1)return false;
    while(position<end){
        unsigned control=packed[position++],count=(control&127u)+1;
        if(count>bytes.size()-offset)return false;
        if(control&128u){
            if(position==end)return false;
            std::fill_n(bytes.data()+offset,count,std::byte(packed[position++]));
        }else{
            if(count>end-position)return false;
            std::memcpy(bytes.data()+offset,packed+position,count);position+=count;
        }
        offset+=count;
    }
    return offset==bytes.size();
}
}
