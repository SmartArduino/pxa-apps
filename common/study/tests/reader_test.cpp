// SPDX-License-Identifier: MIT
#include "../reader.hpp"
#include <cassert>
#include <string>
#include <iostream>
int main() {
    for(const auto& input : {std::string(""),std::string("A short explanation."),
        std::string("第一行\n第二行\n第三行\n第四行\n第五行\n第六行\n第七行"),
        std::string("朗读汉字 é ī ū ǚ 😊。"),std::string(8192,'X')}) {
        auto pages=study::reading_pages(input,12,3);std::string joined;
        for(const auto& page:pages){assert(page.empty()||(static_cast<unsigned char>(page.front())&0xc0)!=0x80);joined+=page;}
        assert(joined==input);
    }
    auto pages=study::reading_pages("one two three four five six seven eight nine ten",12,3);
    assert(pages.size()>1);
    for(const auto& page:pages)assert(page.size()<40);
    std::cout<<"Study reader: lossless UTF-8 pages, line limits and long text passed\n";
}
