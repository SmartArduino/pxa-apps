// SPDX-License-Identifier: MIT
// Test-only adapter: real product navigation, real simulator UI controls.
#include "mz79_navigation.h"
#include <SDL.h>
#include <array>
#include <cstdio>
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
}
