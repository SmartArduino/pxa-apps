#pragma once
#include "voxel_items.hpp"

namespace voxel {
inline constexpr int kQuickCount=9, kBagCount=27, kInventorySlots=kQuickCount+kBagCount;
inline constexpr unsigned kStackLimit=64;
struct ItemStack {
    std::uint8_t block=0,count=0,wear=0;
    friend bool operator==(ItemStack,ItemStack)=default;
};
static_assert(sizeof(ItemStack)==3);
struct Inventory {
    std::array<ItemStack,kInventorySlots> slots{};
    friend bool operator==(const Inventory&,const Inventory&)=default;
    static bool valid(ItemStack s)noexcept {
        return s.count ? valid_item(s.block)&&s.count<=stack_limit(s.block)&&
            (tool_life(s.block)?s.wear>0&&s.wear<=tool_life(s.block):s.wear==0)
            : s.block==kAir&&s.wear==0;
    }
    unsigned count(std::uint8_t block)const noexcept {
        unsigned n=0;for(auto s:slots)if(s.block==block)n+=s.count;return n;
    }
    bool add(std::uint8_t block,unsigned count=1)noexcept {
        if(!valid_item(block)||!count)return false;
        unsigned available=0;
        for(auto s:slots)if(!s.count||(s.block==block&&!tool_life(block)))available+=stack_limit(block)-s.count;
        if(count>available)return false;
        for(int pass=0;pass<2;++pass)for(auto& s:slots){
            if((pass==0?s.count&&s.block==block&&!tool_life(block):!s.count)&&count){
                auto n=std::min(count,stack_limit(block)-s.count);s.block=block;s.count+=n;s.wear=tool_life(block);count-=n;
            }
        }return true;
    }
    bool take(std::uint8_t block,unsigned n)noexcept {
        if(!n||count(block)<n)return false;
        for(auto& s:slots)if(s.block==block&&n){unsigned removed=std::min(n,unsigned(s.count));s.count-=removed;n-=removed;if(!s.count)s={};}
        return true;
    }
    bool consume(int index)noexcept {
        if(index<0||index>=kInventorySlots||!slots[index].count)return false;
        if(!--slots[index].count)slots[index]={};return true;
    }
    void damage(int index)noexcept {
        if(index<0||index>=kInventorySlots)return;
        auto& s=slots[index];if(tool_life(s.block)&&s.wear&&!--s.wear)s={};
    }
    bool transfer(int from,int to)noexcept {
        if(from<0||from>=kInventorySlots||to<0||to>=kInventorySlots||from==to||!slots[from].count)return false;
        auto& a=slots[from];auto& b=slots[to];
        if(a.block==b.block&&!tool_life(a.block)){unsigned moved=std::min(unsigned(a.count),stack_limit(a.block)-b.count);if(!moved)return false;b.count+=moved;a.count-=moved;if(!a.count)a={};}
        else std::swap(a,b);return true;
    }
};
static_assert(sizeof(Inventory)==108);
} // namespace voxel
