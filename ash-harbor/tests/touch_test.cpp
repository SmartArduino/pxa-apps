// SPDX-License-Identifier: MIT
#include "../touch.hpp"
#include <cassert>
#include <cstdio>
int main(){
    using namespace ash;
    const int positions[][2]={{0,-24},{18,-18},{24,0},{18,18},{0,24},{-18,18},{-24,0},{-18,-18}};
    for(unsigned i=0;i<8;++i){
        assert(stickDirection(positions[i][0],positions[i][1])==stickDirections[i]);
        for(int n=8;n<=32;++n)assert(stickDirection(positions[i][0]*n/24,positions[i][1]*n/24)==stickDirections[i]);
    }
    assert(stickDirection(0,0)==0&&stickDirection(6,0)==0&&stickDirection(3,3)==0);
    assert(stickDirection(7,0)==Right);
    assert(stickDirection(24,5)==Right&&stickDirection(5,-24)==Up);
    assert(stickDirection(0,24)==Down&&stickDirection(18,18)==(Down|Right)&&stickDirection(24,0)==Right);
    std::puts("Eight sectors, neutral zone, cardinal tolerance and quarter-circle path passed.");
}
