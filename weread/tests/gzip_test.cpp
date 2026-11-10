#include "../reader_gzip.hpp"
#include <cassert>
#include <fstream>
#include <iostream>
#include <string>

int main(int argc,char** argv){
    assert(argc==4);
    const auto read=[](const char* path){std::ifstream f(path,std::ios::binary);assert(f);return std::string(std::istreambuf_iterator<char>(f),{});};
    const auto encoded=read(argv[1]);const bool valid=std::string_view(argv[2])!="-";
    const auto expected=valid?read(argv[2]):std::string{};
    const auto limit=std::stoul(argv[3]);
    { // Counting validates CRC/limits without retaining a growing output.
        weread::ReaderGzip decoder;auto part=std::string_view(encoded);
        auto error=weread::ReaderGzip::Error::none;
        while(error==weread::ReaderGzip::Error::none&&(!part.empty()||decoder.pending()))error=decoder.step(part,nullptr,limit);
        assert((error==weread::ReaderGzip::Error::none&&decoder.complete())==valid);
        if(valid)assert(decoder.size()==expected.size());
    }
    for(std::size_t chunk:{1u,7u,113u,4064u,8192u}){
        weread::ReaderGzip decoder;weread::ReaderInput output;
        auto error=weread::ReaderGzip::Error::none;
        for(std::size_t at=0;at<encoded.size()&&error==weread::ReaderGzip::Error::none;){
            auto part=std::string_view(encoded).substr(at,std::min(chunk,encoded.size()-at));at+=part.size();
            do{error=decoder.step(part,output,limit);}while(error==weread::ReaderGzip::Error::none&&(!part.empty()||decoder.pending()));
        }
        const bool complete=error==weread::ReaderGzip::Error::none&&decoder.complete();
        assert(output.size()<=limit&&complete==valid);
        if(valid)assert(output.view()==expected);
    }
    std::cout<<"gzip slicing/validation passed\n";
}
