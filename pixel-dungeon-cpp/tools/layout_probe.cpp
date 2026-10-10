#include "layout.hpp"
#include <cstdio>
#include <cstdlib>
using namespace dungeon;
int main(int argc,char** argv){
    if(argc!=9)return 2;
    const int w=std::atoi(argv[1]),h=std::atoi(argv[2]),shape=std::atoi(argv[3]);
    int radii[4];for(int& radius:radii)radius=std::atoi(argv[8]);
    pd_layout_t l{};pd_layout_build(&l,w,h,std::atoi(argv[4]),std::atoi(argv[5]),std::atoi(argv[6]),std::atoi(argv[7]));
    pd_layout_fit_display_shape(&l,shape,radii);
    bool first=true;std::printf("{");
    auto point=[&](const char* name,pd_rect_t r){std::printf("%s\"%s\":[%d,%d]",first?"":",",name,r.x+r.w/2,r.y+r.h/2);first=false;};
    point("title",l.menu_primary);point("slot",pd_layout_save_row(&l,1,0));point("class",l.class_button[0]);
    point("slot_saved",pd_layout_save_row(&l,2,0));
    point("search",l.button[PD_BUTTON_SEARCH]);point("wait",l.button[PD_BUTTON_WAIT]);point("bag",l.button[PD_BUTTON_BAG]);point("bag_close",l.bag_close);
    point("pause",l.settings);point("pause_settings",l.pause_settings);point("settings_back",l.settings_back);
    point("pause_menu",l.pause_menu);std::printf("}\n");
}
