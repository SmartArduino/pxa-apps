// SPDX-License-Identifier: MIT
#pragma once
#include "../common/motion-games/motion.hpp"
#include "../common/motion-games/codec.hpp"
namespace dice {
struct Roll {
  uint8_t count=0;std::array<uint8_t,6> values{};
  unsigned total() const {unsigned n=0;for(unsigned i=0;i<count;++i)n+=values[i];return n;}
};
struct Model {
  static constexpr unsigned history_capacity=8,profile_size=96;
  unsigned selected=2,history_size=0,total_rolls=0;
  bool shake=true,valid=false,pending=false;
  Roll rolling;std::array<Roll,history_capacity> history{};
  bool begin(motion::Random& rng){if(pending)return false;rolling={};rolling.count=uint8_t(selected);
    for(unsigned i=0;i<selected;++i)rolling.values[i]=uint8_t(rng.below(6)+1);pending=true;return true;}
  bool settle(){if(!pending)return false;for(unsigned i=history_capacity-1;i;--i)history[i]=history[i-1];history[0]=rolling;
    history_size=std::min(history_capacity,history_size+1);++total_rolls;pending=false;valid=true;return true;}
  void count(unsigned n){if(pending)return;selected=std::clamp(n,1u,6u);valid=false;}
  const Roll& result() const {return pending?rolling:history[0];}
  std::array<std::byte,profile_size> encode() const {
    std::array<std::byte,profile_size> out{};motion::put32(out.data(),0x44494331);out[4]=std::byte(selected);out[5]=std::byte(shake);out[6]=std::byte(valid);out[7]=std::byte(history_size);motion::put32(out.data()+8,total_rolls);
    for(unsigned i=0;i<history_size;++i){out[12+i*7]=std::byte(history[i].count);for(unsigned j=0;j<6;++j)out[13+i*7+j]=std::byte(history[i].values[j]);}
    motion::put32(out.data()+profile_size-4,motion::checksum(std::span(out).first(profile_size-4)));return out;
  }
  bool decode(std::span<const std::byte> in){
    if(in.size()!=profile_size||motion::get32(in.data())!=0x44494331||motion::get32(in.data()+profile_size-4)!=motion::checksum(in.first(profile_size-4)))return false;
    Model candidate;candidate.selected=unsigned(in[4]);if(!candidate.selected||candidate.selected>6||unsigned(in[5])>1||unsigned(in[6])>1||unsigned(in[7])>history_capacity)return false;
    candidate.shake=bool(in[5]);candidate.valid=bool(in[6]);candidate.history_size=unsigned(in[7]);candidate.total_rolls=motion::get32(in.data()+8);
    if((candidate.valid&&!candidate.history_size)||candidate.total_rolls<candidate.history_size)return false;
    for(unsigned i=0;i<candidate.history_size;++i){auto &r=candidate.history[i];r.count=uint8_t(in[12+i*7]);if(!r.count||r.count>6)return false;
      for(unsigned j=0;j<6;++j){r.values[j]=uint8_t(in[13+i*7+j]);if(j<r.count?(r.values[j]<1||r.values[j]>6):r.values[j]!=0)return false;}}
    for(unsigned i=12+candidate.history_size*7;i<profile_size-4;++i)if(in[i]!=std::byte{})return false;
    if(candidate.valid&&candidate.history[0].count!=candidate.selected)return false;
    *this=candidate;return true;
  }
};
}
