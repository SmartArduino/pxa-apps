// SPDX-License-Identifier: MIT
// PXA rule adaptation of https://game.0z.hk/game/2048/js/game.js.
#pragma once
#include <stdint.h>
#include <string.h>
namespace frog2048 {
struct Move { bool changed; uint32_t gained; uint8_t largest; };
// Directions: up, right, down, left. Values store exponents; 12 is 4096.
inline Move move(uint8_t *board,unsigned direction){
    uint8_t previous[16];memcpy(previous,board,16);Move result{};
    for(int line=0;line<4;++line){
        int positions[4];uint8_t values[4]{};int count=0;
        for(int i=0;i<4;++i){
            positions[i]=direction==0?i*4+line:direction==1?line*4+3-i:direction==2?(3-i)*4+line:line*4+i;
            if(board[positions[i]])values[count++]=board[positions[i]];
        }
        int out=0;
        for(int i=0;i<count;++i){
            uint8_t value=values[i];
            if(i+1<count&&values[i+1]==value&&value<12){++value;++i;result.gained+=1u<<value;if(value>result.largest)result.largest=value;}
            board[positions[out++]]=value;
        }
        while(out<4)board[positions[out++]]=0;
    }
    result.changed=memcmp(previous,board,16)!=0;return result;
}
inline bool over(const uint8_t *board){
    for(int i=0;i<16;++i){if(!board[i])return false;
        if(board[i]<12&&((i%4<3&&board[i]==board[i+1])||(i<12&&board[i]==board[i+4])))return false;}
    return true;
}
}
