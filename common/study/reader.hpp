// SPDX-License-Identifier: MIT
#pragma once
#include <algorithm>
#include <string>
#include <string_view>
#include <vector>

namespace study {
// Slice at UTF-8 boundaries. Estimate wide glyphs as two cells and preserve
// every original byte; native Text handles word wrapping inside each page.
inline std::vector<std::string> reading_pages(std::string_view value,unsigned columns=26,unsigned rows=6) {
    columns=std::clamp(columns,12u,60u);rows=std::clamp(rows,3u,8u);
    std::vector<std::string> out;
    size_t begin=0;
    while(begin<value.size()) {
        size_t cursor=begin;unsigned line=0,used=0;
        while(cursor<value.size()) {
            const unsigned char lead=value[cursor];
            size_t bytes=lead<0x80?1:lead<0xe0?2:lead<0xf0?3:4;
            bytes=std::min(bytes,value.size()-cursor);
            const unsigned cells=lead<0x80?1:2;
            if(value[cursor]=='\n') {if(line+1>=rows)break;++line;used=0;}
            else {if(used+cells>columns){if(line+1>=rows)break;++line;used=0;}used+=cells;}
            cursor+=bytes;
        }
        if(cursor==begin)++cursor;
        // Keep a Latin word together when it crosses a page boundary.
        if(cursor<value.size()&&cursor>begin&&static_cast<unsigned char>(value[cursor])<0x80&&
           value[cursor]!=' '&&value[cursor]!='\n') {
            const auto space=value.rfind(' ',cursor-1);
            if(space!=value.npos&&space>begin&&cursor-space<20)cursor=space+1;
        }
        out.emplace_back(value.substr(begin,cursor-begin));begin=cursor;
    }
    if(out.empty())out.emplace_back("");
    return out;
}
}
