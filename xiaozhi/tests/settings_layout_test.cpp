// SPDX-License-Identifier: Apache-2.0
#include "../settings_layout.hpp"
#include <cassert>
#include <cstdio>

int main() {
  for (auto size : {std::pair{240,240}, {240,284}, {320,240}, {480,800}, {800,480}}) {
    xiaozhi::SettingsLayout l(size.first,size.second);
    auto inside=[&](xiaozhi::SettingsRect r) {
      assert(r.x>=0 && r.y>=0 && r.width>0 && r.height>0);
      assert(r.x+r.width<=l.width && r.y+r.height<=l.height);
    };
    inside(l.header); inside(l.tabs); inside(l.save);
    assert(l.header.y+l.header.height<=l.tabs.y);
    assert(l.tabs.y+l.tabs.height<l.body_top);
    for (unsigned count : {3u,4u}) {
      int bottom=l.body_top;
      for(unsigned i=0;i<count;++i) {
        auto r=l.row(i,count);inside(r);assert(r.y>=bottom);
        assert(r.y+r.height<=l.body_bottom);bottom=r.y+r.height;
      }
    }
    assert(l.body_bottom<l.save.y);
    // Portrait screens use their full height, rather than a letterboxed 4:3 panel.
    if(l.height>l.width) assert(l.save.y>l.height*3/4);
  }
  std::puts("PASS: full-height portrait/landscape settings, disjoint rows, fixed footer and on-screen targets");
}
