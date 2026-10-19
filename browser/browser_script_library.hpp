#pragma once
#include "browser_script_libraries.hpp"
#include <array>
#include <bit>
#include <cstdint>
#include <string>
#include <string_view>

namespace browser::script {
// Exact-source identity for app-signed compilation results. Bytecode is never
// accepted from HTTP, storage writable by a page, or a server-provided hash.
// Input remains owned by the pending response. Only digest state survives
// between idle polls; no source pointer, copy or additional I/O task is retained.
class SourceDigest {
    static constexpr std::array<std::uint32_t,64> k={
        0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
        0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
        0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
        0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
        0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
        0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
        0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
        0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2};
    std::array<std::uint32_t,8> h_={0x6a09e667,0xbb67ae85,0x3c6ef372,0xa54ff53a,
                                  0x510e527f,0x9b05688c,0x1f83d9ab,0x5be0cd19};
    std::size_t next_=0;
public:
    bool advance(std::string_view source) {
        const auto blocks=(source.size()+9+63)/64;
        if(next_==blocks)return true;
        const auto bits=std::uint64_t(source.size())*8;
        const auto block=next_++;
        std::array<std::uint32_t,16> w{};
        if(block*64+64<=source.size()) {
            // All but the final block are raw input. Keep padding branches out
            // of that path, and assemble big-endian words without alignment UB.
            const auto* raw=reinterpret_cast<const unsigned char*>(source.data()+block*64);
            for(unsigned i=0;i<16;++i) {
                const auto at=i*4;
                w[i]=(std::uint32_t(raw[at])<<24)|(std::uint32_t(raw[at+1])<<16)|
                     (std::uint32_t(raw[at+2])<<8)|std::uint32_t(raw[at+3]);
            }
        } else {
            for(unsigned i=0;i<64;++i) {
                const auto at=block*64+i;
                unsigned byte=0;
                if(at<source.size())byte=static_cast<unsigned char>(source[at]);
                else if(at==source.size())byte=128;
                else if(at>=blocks*64-8)byte=unsigned(bits>>((blocks*64-1-at)*8))&255;
                w[i/4]=(w[i/4]<<8)|byte;
            }
        }
        auto a=h_[0],b=h_[1],c=h_[2],d=h_[3],e=h_[4],f=h_[5],g=h_[6],h=h_[7];
        for(unsigned i=0;i<64;++i) {
            if(i>=16) {
                const auto x=w[(i+1)%16],y=w[(i+14)%16];
                w[i%16]+=(std::rotr(x,7)^std::rotr(x,18)^(x>>3))+w[(i+9)%16]+
                          (std::rotr(y,17)^std::rotr(y,19)^(y>>10));
            }
            const auto t1=h+(std::rotr(e,6)^std::rotr(e,11)^std::rotr(e,25))+
                          ((e&f)^(~e&g))+k[i]+w[i%16];
            const auto t2=(std::rotr(a,2)^std::rotr(a,13)^std::rotr(a,22))+
                          ((a&b)^(a&c)^(b&c));
            h=g;g=f;f=e;e=d+t1;d=c;c=b;b=a;a=t1+t2;
        }
        h_[0]+=a;h_[1]+=b;h_[2]+=c;h_[3]+=d;
        h_[4]+=e;h_[5]+=f;h_[6]+=g;h_[7]+=h;
        return next_==blocks;
    }
    bool matches(std::string_view digest) const noexcept {
        if(digest.size()!=64)return false;
        constexpr char digits[]="0123456789abcdef";
        unsigned at=0;
        for(auto word:h_)for(int shift=28;shift>=0;shift-=4)
            if(digest[at++]!=digits[(word>>shift)&15])return false;
        return true;
    }
    std::array<char,64> hex() const noexcept {
        constexpr char digits[]="0123456789abcdef";
        std::array<char,64> result{};
        unsigned at=0;
        for(auto word:h_)for(int shift=28;shift>=0;shift-=4)result[at++]=digits[(word>>shift)&15];
        return result;
    }
};
inline std::string digest_path(std::string_view digest) {
    std::string path;path.reserve(23+64+4);
    path="assets/script-libraries/";path.append(digest);path+=".qjb";
    return path;
}
inline std::string library_path(std::string_view source) {
    SourceDigest digest;while(!digest.advance(source)){}
    const auto hex=digest.hex();return digest_path({hex.data(),hex.size()});
}
inline bool library_candidate(std::size_t size) noexcept {
    for(const auto& item:compiled_libraries)if(item.source_bytes==size)return true;
    return false;
}
inline std::string matching_library_path(std::size_t size,const std::array<char,64>& digest) {
    const std::string_view hex{digest.data(),digest.size()};
    for(const auto& item:compiled_libraries)
        if(item.source_bytes==size && item.sha256==hex)return digest_path(hex);
    return {};
}
inline std::string matching_library_path(std::size_t size,const SourceDigest& digest) {
    for(const auto& item:compiled_libraries)
        if(item.source_bytes==size && digest.matches(item.sha256))return digest_path(item.sha256);
    return {};
}
inline std::string known_library_path(std::string_view source) {
    if(!library_candidate(source.size()))return {};
    SourceDigest digest;while(!digest.advance(source)){}
    return matching_library_path(source.size(),digest);
}
}
