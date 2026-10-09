#include "../voxel_inventory.hpp"
#include "../voxel_crafting.hpp"
#include "../voxel_mining.hpp"
#include <cassert>
#include <cstdio>
int main(){
    using namespace voxel;
    Inventory bag;assert(bag.count(kDirt)==0&&!bag.consume(0));
    assert(bag.add(kDirt,65)&&bag.slots[0]==ItemStack(kDirt,64)&&bag.slots[1]==ItemStack(kDirt,1));
    assert(bag.consume(1)&&bag.slots[1]==ItemStack{});
    assert(bag.add(kDirt)&&bag.slots[1].count==1);
    assert(bag.transfer(0,8)&&bag.slots[8].count==64&&!bag.slots[0].count);
    assert(bag.transfer(8,1)&&bag.slots[1].count==64&&bag.slots[8].count==1);
    assert(!bag.transfer(8,1));
    bag.slots[0]={kStone,3};assert(bag.transfer(0,8)&&bag.slots[8].block==kStone&&bag.slots[0].block==kDirt);
    assert(!bag.add(kAir)&&!bag.add(kWater)&&!bag.add(kBedrock)&&!bag.transfer(-1,0));
    assert(!bag.take(kWood,1));
    CraftingGrid grid;
    bag={};assert(bag.add(kWood,1)&&grid.put(bag,4,0));
    assert(grid.available(bag,0)==0&&!grid.put(bag,0,0));
    assert(grid.result(bag,2)==ItemStack(kPlank,4)&&grid.take_result(bag,2));
    assert(bag.count(kWood)==0&&bag.count(kPlank)==4&&grid.sources[4]==-1);
    for(int i:{0,1,3,4})assert(grid.put(bag,i,0));
    assert(grid.result(bag,2)==ItemStack(kTable,1)&&grid.take_result(bag,2));
    assert(bag.count(kTable)==1&&bag.count(kPlank)==0&&!grid.take_result(bag,2));
    // Wrong shape, extra material and insufficient shared stack never craft.
    bag={};assert(bag.add(kPlank,4));grid.clear();
    for(int i:{0,1,2,3})assert(grid.put(bag,i,0));
    assert(!grid.result(bag,3).count&&!grid.result(bag,2).count);
    grid.clear();for(int i:{4,5,7,8})assert(grid.put(bag,i,0));
    assert(grid.result(bag,3)==ItemStack(kTable,1));
    assert(!grid.result(bag,2).count);
    grid.clear();assert(bag.count(kPlank)==4); // cancellation returns every reservation
    // Repeated output preserves the shape only while enough inputs remain.
    bag={};assert(bag.add(kPlank,8));for(int i:{0,1,3,4})assert(grid.put(bag,i,0));
    assert(grid.take_result(bag,2)&&grid.result(bag,2).count);
    assert(grid.take_result(bag,2)&&bag.count(kTable)==2&&!grid.result(bag,2).count);
    for(auto& s:bag.slots)s={kStone,64};
    const auto full=bag;assert(!bag.add(kDirt)&&bag==full);
    bag.slots[0]={kWood,2};grid.clear();assert(grid.put(bag,0,0));
    const auto no_output=bag;const auto reservations=grid.sources;
    assert(!grid.take_result(bag,2)&&bag==no_output&&grid.sources==reservations);
    bag.slots[0]={kWood,1};assert(grid.take_result(bag,2)&&bag.slots[0]==ItemStack(kPlank,4));
    assert(grid.sources[0]==-1); // recycled output slot cannot become a phantom ingredient
    assert(!grid.put(bag,-1,0)&&!grid.put(bag,9,0)&&!grid.put(bag,0,-1));
    assert(!Inventory::valid({kWood,0})&&!Inventory::valid({kWood,65}));
    MiningProgress dig;RayHit leaf;leaf.hit=true;leaf.x=4;leaf.y=1;leaf.z=8;
    assert(!dig.advance(leaf,kLeaves,.20f)&&dig.progress()>.4f);
    auto other=leaf;other.x++;
    assert(!dig.advance(other,kStone,.20f)&&dig.progress()<.1f);
    assert(!dig.advance(other,kDirt,.20f)&&dig.progress()<.4f); // Material replacement resets.
    dig.reset();assert(!dig.advance(leaf,kLeaves,.44f)&&dig.advance(leaf,kLeaves,.02f));
    dig.reset();assert(!dig.advance(leaf,kStone,1.6f)&&dig.progress()==.5f);
    assert(!dig.advance({},kStone,1)&&dig.key==0);
    assert(!dig.advance(leaf,kBedrock,10)&&dig.key==0);
    // Matching wooden/stone tools are 2x/4x; the wrong tool stays at hand speed.
    assert(MiningProgress::duration_seconds(kStone,kWoodPickaxe)==1.6f);
    assert(MiningProgress::duration_seconds(kStone,kStonePickaxe)==.8f);
    assert(MiningProgress::duration_seconds(kStone,kStoneAxe)==3.2f);
    assert(MiningProgress::duration_seconds(kWood,kWoodAxe)==.8f);
    assert(MiningProgress::duration_seconds(kWood,kStoneAxe)==.4f);
    assert(MiningProgress::duration_seconds(kDirt,kStoneShovel)==.65f/4);
    dig.reset();assert(!dig.advance(leaf,kStone,.4f,kWoodPickaxe));
    assert(!dig.advance(leaf,kStone,.2f,kStonePickaxe)&&dig.progress()==.25f); // Tool switch resets.
    bag={};assert(bag.add(kWoodPickaxe,2));
    assert(bag.slots[0]==ItemStack(kWoodPickaxe,1,60)&&bag.slots[1]==ItemStack(kWoodPickaxe,1,60));
    bag.damage(0);assert(bag.slots[0].wear==59);
    assert(bag.transfer(0,1)&&bag.slots[1].wear==59&&bag.slots[0].wear==60);
    for(int i=0;i<59;++i)bag.damage(1);assert(bag.slots[1]==ItemStack{});
    assert(!Inventory::valid({kWoodPickaxe,2,60})&&!Inventory::valid({kWoodPickaxe,1,0}));
    assert(!Inventory::valid({kStonePickaxe,1,133})&&!Inventory::valid({kDirt,1,1}));
    bag={};assert(bag.add(kPlank,2));grid.clear();assert(grid.put(bag,0,0)&&grid.put(bag,3,0));
    assert(grid.result(bag,2)==ItemStack(kStick,4)&&grid.take_result(bag,2));
    // All six tools use real materials, two sticks, and a 3x3 workbench.
    for(auto material:{kPlank,kStone,kCobble})for(auto kind:{Tool::pickaxe,Tool::axe,Tool::shovel}){
        bag={};assert(bag.add(material,3)&&bag.add(kStick,2));grid.clear();
        if(kind==Tool::pickaxe){for(int at:{0,1,2})assert(grid.put(bag,at,0));assert(grid.put(bag,4,1)&&grid.put(bag,7,1));}
        if(kind==Tool::axe){for(int at:{0,1,3})assert(grid.put(bag,at,0));assert(grid.put(bag,4,1)&&grid.put(bag,7,1));}
        if(kind==Tool::shovel){assert(grid.put(bag,0,0)&&grid.put(bag,3,1)&&grid.put(bag,6,1));}
        assert(!grid.result(bag,2).count);auto result=grid.result(bag,3);
        assert(tool_kind(result.block)==kind&&result.wear==(material==kPlank?60:132));
        assert(grid.take_result(bag,3)&&bag.count(result.block)==1&&bag.count(kStick)==0);
    }
    std::printf("Finite inventory %zu B: stacking, consumption, transfer, full rollback, recipes and per-target mining OK\n",sizeof(Inventory));
}
