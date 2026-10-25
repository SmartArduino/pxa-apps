// SPDX-License-Identifier: CC0-1.0
// Native physics oracle for independently retrieved simulator/device saves.
#include "../core.hpp"
#include <stdio.h>
#include <stdlib.h>
int main(int argc,char **argv){
    if(argc!=4)return 2;
    uint8_t bytes[elements::SAVE_SIZE];FILE *file=fopen(argv[1],"rb");if(!file)return 3;
    size_t size=fread(bytes,1,sizeof(bytes),file);const int tail=fgetc(file);fclose(file);
    elements::State state;if(tail!=EOF||!elements::decode(state,bytes,size))return 4;
    char *end=nullptr;unsigned long steps=strtoul(argv[2],&end,10);if(!end||*end||steps>100000)return 5;
    while(steps--)elements::step(state);
    if(elements::encode(state,bytes,sizeof(bytes))!=sizeof(bytes))return 6;
    file=fopen(argv[3],"wb");if(!file)return 7;
    const bool ok=fwrite(bytes,1,sizeof(bytes),file)==sizeof(bytes);fclose(file);return ok?0:8;
}
