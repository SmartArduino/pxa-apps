#pragma once
#include "voxel_inventory.hpp"

namespace voxel {
// One ingredient per cell, reserved from a real stack. Reservations never
// copy items: closing a menu or cancelling a recipe needs no return buffer.
struct CraftingGrid {
    std::array<std::int8_t,9> sources{-1,-1,-1,-1,-1,-1,-1,-1,-1};
    void clear() noexcept { sources.fill(-1); }
    unsigned reserved(int slot) const noexcept {
        unsigned n=0;for(auto source:sources)n+=source==slot;return n;
    }
    unsigned available(const Inventory& bag,int slot) const noexcept {
        if(slot<0||slot>=kInventorySlots)return 0;
        const unsigned count=bag.slots[slot].count,n=reserved(slot);
        return count>=n?count-n:0;
    }
    ItemStack ingredient(const Inventory& bag,int cell) const noexcept {
        if(cell<0||cell>=9)return {};
        const int slot=sources[cell];
        return slot>=0&&slot<kInventorySlots&&bag.slots[slot].count>=reserved(slot)
            ?ItemStack{bag.slots[slot].block,1,bag.slots[slot].wear}:ItemStack{};
    }
    bool put(const Inventory& bag,int cell,int slot) noexcept {
        if(cell<0||cell>=9||!available(bag,slot))return false;
        sources[cell]=std::int8_t(slot);return true;
    }
    // Normalize the occupied bounding rectangle so a 2x2 recipe may sit at
    // any corner of a 3x3 table. Empty interior cells still matter.
    ItemStack result(const Inventory& bag,int side) const noexcept {
        if(side!=2&&side!=3)return {};
        int x0=side,y0=side,x1=-1,y1=-1,occupied=0;
        for(int y=0;y<side;++y)for(int x=0;x<side;++x){
            const int at=y*3+x;if(sources[at]<0)continue;
            if(!ingredient(bag,at).count)return {};
            x0=std::min(x0,x);y0=std::min(y0,y);x1=std::max(x1,x);y1=std::max(y1,y);++occupied;
        }
        for(int y=0;y<3;++y)for(int x=0;x<3;++x)
            if((y>=side||x>=side)&&sources[y*3+x]>=0)return {};
        if(occupied==1&&ingredient(bag,y0*3+x0).block==kWood)return {kPlank,4};
        if(occupied==2&&x1==x0&&y1-y0==1&&
            ingredient(bag,y0*3+x0).block==kPlank&&ingredient(bag,y1*3+x0).block==kPlank)return {kStick,4};
        if(occupied==4&&x1-x0==1&&y1-y0==1){
            for(int y=y0;y<=y1;++y)for(int x=x0;x<=x1;++x)
                if(ingredient(bag,y*3+x).block!=kPlank)return {};
            return {kTable,1};
        }
        if(side==3&&y1-y0==2){
            const auto material=ingredient(bag,y0*3+x0).block;
            if(material!=kPlank&&material!=kStone&&material!=kCobble)return {};
            auto at=[&](int x,int y){return ingredient(bag,(y0+y)*3+x0+x).block;};
            auto output=[&](Tool kind){const auto id=std::uint8_t((material==kPlank?kWoodPickaxe:kStonePickaxe)+unsigned(kind)-1);return ItemStack{id,1,tool_life(id)};};
            if(occupied==5&&x1-x0==2&&at(0,0)==material&&at(1,0)==material&&at(2,0)==material&&
               at(0,1)==kAir&&at(1,1)==kStick&&at(2,1)==kAir&&
               at(0,2)==kAir&&at(1,2)==kStick&&at(2,2)==kAir)return output(Tool::pickaxe);
            if(occupied==3&&x1==x0&&at(0,1)==kStick&&at(0,2)==kStick)return output(Tool::shovel);
            if(occupied==5&&x1-x0==1&&at(0,0)==material&&at(1,0)==material&&
               ((at(0,1)==material&&at(1,1)==kStick&&at(0,2)==kAir&&at(1,2)==kStick)||
                (at(0,1)==kStick&&at(1,1)==material&&at(0,2)==kStick&&at(1,2)==kAir)))return output(Tool::axe);
        }
        return {};
    }
    bool take_result(Inventory& bag,int side) noexcept {
        const auto output=result(bag,side);if(!output.count)return false;
        auto updated=bag;
        for(auto source:sources)if(source>=0&&!updated.consume(source))return false;
        if(!updated.add(output.block,output.count))return false;
        for(auto& source:sources)
            if(source>=0&&updated.slots[source].block!=bag.slots[source].block)source=-1;
        bag=updated;
        // Leave the shape for another craft only while every reservation is
        // still backed by its stack. Output admission is transactional.
        for(int slot=0;slot<kInventorySlots;++slot)
            if(bag.slots[slot].count<reserved(slot))
                for(auto& source:sources)if(source==slot)source=-1;
        return true;
    }
};
static_assert(sizeof(CraftingGrid)==9);
} // namespace voxel
