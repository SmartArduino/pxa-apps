// SPDX-License-Identifier: MIT
#pragma once
#include "../common/motion-games/motion.hpp"
#include "../common/motion-games/codec.hpp"
#include "content.hpp"
namespace fortune {
struct Profile {
 static constexpr unsigned size=64,capacity=8;
 unsigned theme=0,total=0,history_size=0;bool shake=true;
 std::array<uint8_t,capacity> history{};uint32_t favorites=0;
 unsigned latest()const{return history_size?history[0]:0;}
 unsigned choose(motion::Random& random)const{
   std::array<unsigned,24> pool{};unsigned count=0;
   for(unsigned i=0;i<signs.size();++i)if((!theme||signs[i].theme==theme)&&(!history_size||i!=latest()))pool[count++]=i;
   return pool[random.below(count)];
 }
 void record(unsigned sign){if(sign>=signs.size())return;for(unsigned i=capacity-1;i;--i)history[i]=history[i-1];history[0]=uint8_t(sign);history_size=std::min(capacity,history_size+1);if(total!=UINT32_MAX)++total;}
 bool favorite(unsigned sign)const{return sign<24&&(favorites&(1u<<sign));}
 void toggle(unsigned sign){if(sign<24)favorites^=1u<<sign;}
 std::array<std::byte,size> encode()const{
   std::array<std::byte,size> out{};motion::put32(out.data(),0x46525431);out[4]=std::byte(theme);out[5]=std::byte(shake);out[6]=std::byte(history_size);motion::put32(out.data()+8,total);motion::put32(out.data()+12,favorites);
   for(unsigned i=0;i<history_size;++i)out[16+i]=std::byte(history[i]);motion::put32(out.data()+60,motion::checksum(std::span(out).first(60)));return out;
 }
 bool decode(std::span<const std::byte> in){
   if(in.size()!=size||motion::get32(in.data())!=0x46525431||motion::get32(in.data()+60)!=motion::checksum(in.first(60)))return false;
   Profile p;p.theme=unsigned(in[4]);p.shake=bool(in[5]);p.history_size=unsigned(in[6]);p.total=motion::get32(in.data()+8);p.favorites=motion::get32(in.data()+12);
   if(p.theme>6||unsigned(in[5])>1||p.history_size>capacity||p.total<p.history_size||p.favorites>>24||in[7]!=std::byte{})return false;
   for(unsigned i=0;i<p.history_size;++i){p.history[i]=uint8_t(in[16+i]);if(p.history[i]>=24)return false;}
   for(unsigned i=16+p.history_size;i<60;++i)if(in[i]!=std::byte{})return false;*this=p;return true;
 }
};
}
