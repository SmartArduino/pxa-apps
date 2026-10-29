// SPDX-License-Identifier: MIT
// Test-only adapter: real product navigation, real simulator UI controls.
#include "mz79_navigation.h"
#include <SDL.h>
#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <string>

namespace {
Mz79Navigation navigation;
SDL_SpinLock lock;
std::array<SDL_Keycode,64> keys;
unsigned head,tail;
bool initialized;
lv_obj_t* previous;
std::string previous_label;
int watch(void*,SDL_Event* event) {
    if(event->type!=SDL_KEYDOWN||event->key.repeat)return 1;
    auto key=event->key.keysym.sym;
    if(key!=SDLK_UP&&key!=SDLK_DOWN&&key!=SDLK_RETURN)return 1;
    SDL_AtomicLock(&lock);
    if(tail-head<keys.size())keys[tail++%keys.size()]=key;
    SDL_AtomicUnlock(&lock);
    return 1;
}
std::string label(lv_obj_t* object) {
    if(!object)return {};
    if(lv_obj_check_type(object,&lv_label_class))return lv_label_get_text(object);
    for(unsigned i=0;i<lv_obj_get_child_count(object);++i) {
        auto value=label(lv_obj_get_child(object,i));
        if(!value.empty())return value;
    }
    return {};
}
// Optional geometry evidence for layout regressions. Use native LVGL bounds
// and font measurements so tests can catch clipped text beyond focus checks.
void layout_trace(lv_obj_t* object, std::string& output) {
    if(!lv_obj_is_visible(object))return;
    if(lv_obj_check_type(object,&lv_label_class)) {
        const char* text=lv_label_get_text(object);
        if(*text) {
            lv_area_t bounds,clip;lv_obj_get_coords(object,&bounds);clip=bounds;
            for(auto* parent=lv_obj_get_parent(object);parent;parent=lv_obj_get_parent(parent)) {
                if(lv_obj_has_flag(parent,LV_OBJ_FLAG_OVERFLOW_VISIBLE))continue;
                lv_area_t area;lv_obj_get_content_coords(parent,&area);
                clip.x1=std::max(clip.x1,area.x1);clip.y1=std::max(clip.y1,area.y1);
                clip.x2=std::min(clip.x2,area.x2);clip.y2=std::min(clip.y2,area.y2);
            }
            lv_point_t size;
            lv_text_get_size(&size,text,lv_obj_get_style_text_font(object,LV_PART_MAIN),
                lv_obj_get_style_text_letter_space(object,LV_PART_MAIN),
                lv_obj_get_style_text_line_space(object,LV_PART_MAIN),
                lv_obj_get_content_width(object),LV_TEXT_FLAG_NONE);
            char prefix[192];std::snprintf(prefix,sizeof(prefix),
                "STUDY_LABEL %d %d %d %d %d %d %d %d %d %d ",
                bounds.x1,bounds.y1,bounds.x2,bounds.y2,
                clip.x1,clip.y1,clip.x2,clip.y2,size.x,size.y);
            output+=prefix;
            for(const unsigned char* p=(const unsigned char*)text;*p;++p) {
                constexpr char hex[]="0123456789abcdef";
                output+=hex[*p>>4];output+=hex[*p&15];
            }
            output+='\n';
        }
    }
    for(unsigned i=0;i<lv_obj_get_child_count(object);++i)
        layout_trace(lv_obj_get_child(object,i),output);
}
}
extern "C" void study_navigation_poll() {
    if(!initialized) {
        navigation.Init(lv_display_get_default());
        SDL_AddEventWatch(watch,nullptr);
        initialized=true;
    }
    auto* root=lv_screen_active();
    lv_obj_update_layout(root);
    navigation.Refresh(root);
    for(;;) {
        SDL_AtomicLock(&lock);
        if(head==tail){SDL_AtomicUnlock(&lock);break;}
        auto key=keys[head++%keys.size()];
        SDL_AtomicUnlock(&lock);
        if(key==SDLK_RETURN)navigation.Confirm();
        else navigation.Step(key==SDLK_UP?-1:1);
    }
    auto* focused=navigation.Focused();
    auto value=label(focused);
    if(focused!=previous||value!=previous_label) {
        std::fprintf(stderr,"STUDY_FOCUS %s\n",value.c_str());
        std::fflush(stderr);previous=focused;previous_label=std::move(value);
    }
    static const bool trace=std::getenv("STUDY_TRACE_LAYOUT")!=nullptr;
    static std::string previous_layout;
    if(trace) {
        std::string current;layout_trace(root,current);
        if(current!=previous_layout) {
            std::fprintf(stderr,"STUDY_LAYOUT_BEGIN\n%sSTUDY_LAYOUT_END\n",current.c_str());
            std::fflush(stderr);previous_layout=std::move(current);
        }
    }
}
