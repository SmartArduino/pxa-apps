#include "../browser_content_encoding.hpp"
#include <cassert>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <vector>
using namespace browser;
using Error=ContentDecoder::Error;
static std::string read(const std::string& path) {
    std::ifstream file(path,std::ios::binary);assert(file);
    return {std::istreambuf_iterator<char>(file),{}};
}
struct Result {std::string text;Error error;std::size_t peak;};
static Result decode(std::string_view wire,ContentEncoding coding,std::size_t chunk,
                     std::size_t output_bytes,std::size_t limit=524288) {
    ContentDecoder decoder(coding,limit);std::vector<std::byte> output(output_bytes);
    std::string text;std::size_t at=0,end=0;
    while(at<wire.size()||decoder.pending()) {
        if(at==end)end=std::min(wire.size(),at+chunk);
        auto input=std::as_bytes(std::span{wire.data()+at,end-at});
        auto step=decoder.step(input,output);
        if(step.error!=Error::none)return {std::move(text),step.error,decoder.peak_heap()};
        at+=step.consumed;text.append(reinterpret_cast<char*>(output.data()),step.produced);
        assert(step.consumed||step.produced||at==wire.size());
    }
    return {std::move(text),decoder.finish(),decoder.peak_heap()};
}
int main(int argc,char** argv) {
    assert(argc==2);const std::string root=argv[1];const auto plain=read(root+"/page.txt");
    const auto gzip=read(root+"/page.gz"),deflate=read(root+"/page.deflate");
    assert(content_encoding(" GZip\t")==ContentEncoding::gzip);
    assert(content_encoding("X-GZIP")==ContentEncoding::gzip);
    assert(content_encoding("")==ContentEncoding::identity);
    assert(content_encoding("gzip, br")==ContentEncoding::unsupported);
    assert(content_encoding("br")==ContentEncoding::unsupported);
    std::size_t peak=0;
    for(auto chunk:{1u,2u,7u,97u,1024u})for(auto out:{1u,31u,1536u}) {
        for(auto coding:{ContentEncoding::gzip,ContentEncoding::deflate}) {
            auto r=decode(coding==ContentEncoding::gzip?gzip:deflate,coding,chunk,out);
            assert(r.error==Error::none&&r.text==plain);
            peak=std::max(peak,r.peak);assert(r.peak<=ContentDecoder::heap_limit);
        }
    }
    auto identity=decode(plain,ContentEncoding::identity,17,13);
    assert(identity.error==Error::none&&identity.text==plain&&identity.peak==0);
    assert(decode(read(root+"/empty.gz"),ContentEncoding::gzip,1,1).error==Error::none);
    auto joined=decode(gzip+gzip,ContentEncoding::gzip,97,17);
    assert(joined.error==Error::none&&joined.text==plain+plain);
    std::string members;for(int i=0;i<17;++i)members+=read(root+"/empty.gz");
    assert(decode(members,ContentEncoding::gzip,1024,31).error==Error::format);
    for(std::size_t trim=1;trim<=8;++trim)
        assert(decode(std::string_view(gzip).substr(0,gzip.size()-trim),ContentEncoding::gzip,7,31).error==Error::truncated);
    auto damaged=gzip;damaged[damaged.size()-8]^=1;
    assert(decode(damaged,ContentEncoding::gzip,7,31).error==Error::format);
    assert(decode(gzip+"garbage",ContentEncoding::gzip,7,31).error==Error::format);
    assert(decode(deflate+"garbage",ContentEncoding::deflate,7,31).error==Error::format);
    assert(decode(gzip,ContentEncoding::gzip,7,31,plain.size()).error==Error::none);
    assert(decode(gzip,ContentEncoding::gzip,7,31,plain.size()-1).error==Error::limit);
    assert(decode(read(root+"/bomb.gz"),ContentEncoding::gzip,1024,1536).error==Error::limit);
    std::cout<<"content encoding PASS: arbitrary chunks, CRC, EOF, concatenated members, quotas; inflate peak="<<peak<<" identity heap=0\n";
}
