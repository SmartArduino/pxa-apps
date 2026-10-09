#pragma once
#include <string_view>
#include <cstdint>
namespace voxel {
inline constexpr std::u32string_view kFontCharacters=U"VOXEL CRAFT0123456789失败保存成功档损坏或读取正在已放入快捷栏体素世界新游戏加载设置菜单继续返回标题视角灵敏度s低中高飞行模式开关反转上下自动距跳跃恢复默认d有空槽位未更改覆盖弃确定此消工作台背包选择方块";
inline int select_font_size(float scale)noexcept {return scale>=1.65f?18:scale>=1.25f?14:10;}
inline int font_index(char32_t c)noexcept {auto p=kFontCharacters.find(c);return p==kFontCharacters.npos?-1:int(p);}
inline char32_t font_next(std::string_view s,std::size_t& at)noexcept {
    auto first=static_cast<unsigned char>(s[at++]);if(first<128)return first;
    unsigned n=first<224?1:first<240?2:3;char32_t c=first&((1u<<(6-n))-1);
    while(n--&&at<s.size())c=(c<<6)|(static_cast<unsigned char>(s[at++])&63);return c;
}
} // namespace voxel
