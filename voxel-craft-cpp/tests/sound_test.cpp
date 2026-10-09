#include "../voxel_sound.hpp"
#include <cassert>
#include <cstdio>
#include <vector>
#include <fstream>
#include <string>
int main(int argc,char** argv){
    std::vector<std::int16_t> preview;
    const std::uint8_t materials[]={voxel::kLeaves,voxel::kDirt,voxel::kSand,voxel::kSnow,voxel::kWood,voxel::kStone,voxel::kGlass,voxel::kWool};
    std::vector<std::int16_t> previous;
    for(auto block:materials){
        std::vector<std::int16_t> material;
        for(unsigned action:{0,1,2}){
            voxel::ContactSound synth(block,action,713);std::array<std::int16_t,320> packet{};
            std::vector<std::int16_t> samples;unsigned peak=0;
            while(synth.remaining()){
                auto n=synth.render(packet);assert(n>0&&n<=320);
                for(unsigned i=0;i<n;++i){samples.push_back(packet[i]);peak=std::max(peak,unsigned(std::abs(int(packet[i]))));}
            }
            assert(samples.front()==0&&samples.back()==0&&peak>20&&peak<6000);
            material.insert(material.end(),samples.begin(),samples.end());
            preview.insert(preview.end(),samples.begin(),samples.end());preview.insert(preview.end(),4000,0);
        }
        assert(material!=previous);previous=material;
    }
    if(argc==2){
        std::ofstream file(argv[1],std::ios::binary);
        auto u16=[&](unsigned n){file.put(n&255);file.put((n>>8)&255);};
        auto u32=[&](unsigned n){u16(n&65535);u16(n>>16);};
        file.write("RIFF",4);u32(36+preview.size()*2);file.write("WAVEfmt ",8);u32(16);u16(1);u16(1);u32(16000);u32(32000);u16(2);u16(16);file.write("data",4);u32(preview.size()*2);
        for(auto value:preview)u16(std::uint16_t(value));assert(file.good());
    }
    std::puts("Sound: eight distinct material profiles, hit/break/place, bounded PCM packets, click-free ends and no clipping OK");
}
