// SPDX-License-Identifier: MIT
#include "../model.hpp"
#include <cassert>
#include <cstdio>
int main(){motion::Random random{0x8144};fortune::Profile p;std::array<unsigned,24> seen{};
 for(unsigned i=0;i<100000;++i){p.theme=i%7;unsigned id=p.choose(random);assert(id<24&&(!p.theme||fortune::signs[id].theme==p.theme));if(p.history_size)assert(id!=p.latest());++seen[id];unsigned before=p.total;p.record(id);assert(p.total==before+1&&p.latest()==id&&p.history_size==std::min(i+1,8u));
  p.toggle(id);assert(p.favorite(id));auto data=p.encode();fortune::Profile other;assert(other.decode(data)&&other.latest()==id&&other.favorite(id)&&other.theme==p.theme);data[20]^=std::byte{1};assert(!other.decode(data));p.toggle(id);assert(!p.favorite(id));}
 for(auto count:seen)assert(count>2000);auto data=p.encode();data[4]=std::byte{7};motion::put32(data.data()+60,motion::checksum(std::span(data).first(60)));assert(!p.decode(data));
 std::puts("PASS: 100000 theme-filtered draws, no immediate repeats, every sign reachable, bounded history, favorites and strict persistence");}
