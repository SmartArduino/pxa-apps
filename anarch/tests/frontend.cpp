// Native frontend contract tests. SPDX-License-Identifier: CC0-1.0
#include <assert.h>
#include <string>
#include <vector>
#include <fstream>
#include "../main.cpp"

struct Call { uint16_t service,opcode; uint64_t token; std::vector<uint8_t> data; };
static std::vector<Call> calls;
static int fail_storage, acquire_index;
extern "C" int32_t pxa_call(uint32_t route,uint64_t token,const uint8_t *data,uint32_t n) {
    calls.push_back({uint16_t(route),uint16_t(route>>16),token,std::vector<uint8_t>(data,data+n)});
    if (uint16_t(route)==PXA_STORAGE_SERVICE && uint16_t(route>>16)==PXA_STORAGE_SET && fail_storage) {
        --fail_storage; return PXA_STATUS_WOULD_BLOCK;
    }
    return 0;
}
extern "C" int32_t pxa_io(uint64_t,uint32_t operation,uint8_t *data,uint32_t n) {
    if (operation==PXA_SURFACE_IO_ACQUIRE) data[0]=uint8_t(acquire_index);
    if (operation==PXA_EVENT_PORT_IO_WAIT) return PXA_STATUS_WOULD_BLOCK;
    return int32_t(n);
}
static int event(uint16_t service,uint16_t opcode,uint64_t token,const uint8_t *payload,unsigned n) {
    uint8_t bytes[256]; uint32_t written;
    assert(pxa_build_message(bytes,sizeof(bytes),service,opcode,token,payload,n,&written));
    return handle_event(bytes,written);
}
static void status(uint16_t service,uint16_t opcode,uint64_t token,int32_t value=0) {
    uint8_t p[4]; pxa_store_u32(p,uint32_t(value)); assert(event(service,opcode,token,p,4)>=0);
}
static void created() {
    uint8_t p[24]{}; pxa_store_u64(p+4,78); pxa_store_u32(p+12,surface_w*2);
    pxa_store_u32(p+16,surface_bytes); p[20]=2;
    assert(event(PXA_SURFACE_SERVICE,PXA_SURFACE_CREATE,CREATE,p,24)>=0);
    const Call &configure=calls.back();
    assert(configure.service==PXA_SURFACE_SERVICE && configure.opcode==PXA_SURFACE_CONFIGURE_LAYER);
    assert(pxa_load_u16(configure.data.data()+16)==surface_w);
    assert(pxa_load_u16(configure.data.data()+18)==surface_h);
    status(PXA_SURFACE_SERVICE,PXA_SURFACE_CONFIGURE_LAYER,CONFIGURE);
}
static void prefs_load(const uint8_t *value,unsigned n,int32_t result=0) {
    uint8_t payload[64]{}; pxa_store_u32(payload,uint32_t(result));
    unsigned size=4;
    if (!result) { size_t written; assert(pxa_wire_record_encode(payload+4,60,2,value,n,&written)); size+=written; }
    assert(event(PXA_STORAGE_SERVICE,PXA_STORAGE_GET,PREFS_LOAD,payload,size)>=0);
}
static void touch(int x,int y,uint8_t phase=PXA_POINTER_DOWN,uint8_t id=7) {
    pxa_ui_pointer_data_t p{}; p.x=viewport.host_x+(viewport.x+x*viewport.w/160)*viewport.host_scale;
    p.y=viewport.host_y+(viewport.y+y*viewport.h/120)*viewport.host_scale;
    p.phase=phase; p.pointer_id=id; pointer(p);
}
static unsigned hash_image() {
    unsigned h=2166136261u; for (uint8_t p:image) h=(h^p)*16777619u; return h;
}
static void dump(const char *root,const char *name) {
    std::ofstream out(std::string(root)+"/"+name+".ppm",std::ios::binary);
    out<<"P6\n160 120\n255\n";
    for (uint8_t p:image) { uint16_t c=paletteRGB565[p]; char b[3]={char((c>>11)*255/31),char(((c>>5)&63)*255/63),char((c&31)*255/31)}; out.write(b,3); }
}
int main(int argc,char **argv) {
    assert(argc==2); const char *root=argv[1];
    background=false; running=true; epoch=now_us(); anarch_audio_reset(); SFG_init();
    uint32_t zero[4]{};
    struct Case { unsigned w,h; int x,y,vw,vh; };
    const Case cases[]={{296,240,0,9,296,222},{320,240,0,0,320,240},{480,320,27,0,426,320},
        {128,96,0,0,128,96},{80,80,0,10,80,60},{1,1,0,0,1,1}};
    for (const auto &c:cases) {
        anarch_view::Viewport v; v.fit(c.w,c.h,zero);
        assert(v.x==c.x && v.y==c.y && v.w==c.vw && v.h==c.vh);
        int x=-1,y=-1;
        assert(v.map(v.x,v.y,x,y) && x==0 && y==0);
        assert(!v.map(v.x-1,v.y,x,y));
        assert(!v.map(v.x+v.w,v.y,x,y));
        assert(v.map(v.x+v.w-1,v.y+v.h-1,x,y) && x<160 && y<120);
    }
    uint32_t safe[4]={10,8,10,8}; anarch_view::Viewport v; v.fit(320,240,safe);
    assert(v.x==13 && v.y==10 && v.w==293 && v.h==220);
    v.fit(960,640,zero); assert(v.host_scale==2 && v.w==426 && v.h==320);
    int x,y; assert(v.map(480,320,x,y) && x==80 && y==60);
    puts("PASS geometry: aspect fit, tiny displays, safe area, host 2x, inverse mapping");

    display_w=296; display_h=240; assert(!request_surface()); created(); assert(surface_ready);
    assert(surface_bytes==296*240*2 && viewport.y==9);
    SFG_game.state=SFG_GAME_STATE_PLAYING; touch(144,56); assert(touch_buttons==PXA_CONTROLLER_A);
    touch(24,42,PXA_POINTER_MOVE,8); assert(touch_buttons==PXA_CONTROLLER_A);
    touch(144,56,PXA_POINTER_UP,8); assert(touch_active);
    touch(144,56,PXA_POINTER_CANCEL); assert(!touch_active && !touch_buttons);
    touch(24,42); assert(touch_buttons==PXA_CONTROLLER_UP);
    pxa_ui_pointer_data_t outside{}; outside.pointer_id=7; outside.phase=PXA_POINTER_MOVE;
    outside.x=2; outside.y=0; pointer(outside); assert(!touch_buttons);
    release_input(); display_w=480; display_h=320; assert(!resize_surface()); assert(!surface_ready && surface_pending);
    assert(calls[calls.size()-2].service==PXA_CORE_SERVICE && calls[calls.size()-2].opcode==PXA_CORE_CLOSE_HANDLE);
    touch(144,56); assert(!touch_active);
    // Another environment arrives while CREATE is in flight: finish then recreate.
    display_w=128; display_h=96; created(); assert(surface_pending && !surface_ready);
    assert(surface_w==128 && surface_h==96); created(); assert(surface_ready && !surface_pending);
    puts("PASS surface: matching configure size, packed registration, resize and in-flight coalescing, touch capture/cancel/outside");

    SFG_game.state=SFG_GAME_STATE_MENU; SFG_game.selectedMenuItem=0; release_input();
    pocket::set_english(false); assert(!pocket::english());
    uint8_t bad[4]={'A','L',1,2}; prefs_load(bad,4); assert(prefs_ready && !pocket::english());
    uint8_t en[4]={'A','L',1,1}; prefs_load(en,4); assert(pocket::english());
    pocket::set_english(false); fail_storage=1; change_language(); assert(prefs_dirty && !prefs_pending);
    prefs_retry_us=0; send_preferences(); assert(prefs_pending && sent_prefs[3]==1);
    change_language(); assert(!pocket::english() && prefs_dirty && sent_prefs[3]==1);
    status(PXA_STORAGE_SERVICE,PXA_STORAGE_SET,PREFS_SAVE); assert(prefs_pending && sent_prefs[3]==0);
    status(PXA_STORAGE_SERVICE,PXA_STORAGE_SET,PREFS_SAVE,PXA_STATUS_WOULD_BLOCK);
    assert(prefs_dirty && !prefs_pending); prefs_retry_us=0; send_preferences();
    status(PXA_STORAGE_SERVICE,PXA_STORAGE_SET,PREFS_SAVE); assert(!prefs_pending && !prefs_dirty);
    assert(SFG_SAVE_SIZE==12 && std::string(SAVE_KEY)=="anarch.save.v1");
    for (unsigned i=0;i<SFG_SAVE_SIZE;++i) saved[i]=uint8_t(i+11);
    save_valid=true; uint8_t loaded[12]{}; assert(SFG_load(loaded)); assert(!memcmp(loaded,saved,12));
    puts("PASS preferences: Chinese default, schema rejection, English load, submit/ACK retry, immutable in-flight snapshot, original 12-byte save");

    display_w=320; display_h=240; assert(!resize_surface()); created();
    SFG_currentLevel.levelPointer=nullptr; SFG_currentLevel.levelNumber=0;
    anarch_view::pixels=image; pocket::set_english(false); anarch_view::menu();
    unsigned zh=hash_image(); dump(root,"menu-zh");
    pocket::set_english(true); anarch_view::menu(); assert(hash_image()!=zh); dump(root,"menu-en");
    pocket::set_english(false); touch(132,6); assert(pocket::english()); touch(132,6,PXA_POINTER_UP);
    touch(80,6); assert(help_open && !touch_buttons); touch(80,6,PXA_POINTER_UP);
    anarch_view::help(); dump(root,"help-en"); pocket::set_english(false); anarch_view::help(); dump(root,"help-zh");
    touch(80,60); assert(!help_open); touch(80,60,PXA_POINTER_UP);
    frontend_controls(PXA_CONTROLLER_Y); assert(help_open); frontend_controls(PXA_CONTROLLER_B); assert(!help_open);
    SFG_game.state=SFG_GAME_STATE_INTRO; anarch_view::story(); dump(root,"story-zh");
    SFG_game.state=SFG_GAME_STATE_OUTRO; anarch_view::story(); dump(root,"ending-zh");
    SFG_setAndInitLevel(0); SFG_game.state=SFG_GAME_STATE_PLAYING; SFG_draw();
    anarch_view::overlays(); dump(root,"hud-zh");
    SFG_game.state=SFG_GAME_STATE_WIN; SFG_draw(); anarch_view::overlays(); dump(root,"win-zh");
    SFG_game.state=SFG_GAME_STATE_MENU; touch_visible=false; dirty=true;
    acquire_index=1; present(); assert(!dirty);
    bool rendered=false;
    for (unsigned i=surface_bytes/2;i<surface_bytes;++i) if (frames[i]) rendered=true;
    assert(rendered);
    assert(surface_bytes/2*2<=2*MAX_PIXELS);
    puts("PASS localized rendering: menu, help, story, actual level/HUD, win, touch language/help, controller close, second mapped buffer");
    // The capped canvas can stay the same while the Host integer scale changes.
    display_w=960; display_h=640; assert(!resize_surface()); created();
    assert(viewport.host_scale==2 && surface_display_w==960);
    display_w=1000; display_h=700; assert(!resize_surface());
    assert(!surface_ready && surface_pending); created();
    assert(surface_display_w==1000);
    // Backends without mapped buffers still use the same software-fit frame.
    display_w=296; display_h=240; assert(!resize_surface());
    status(PXA_SURFACE_SERVICE,PXA_SURFACE_CREATE,CREATE,PXA_STATUS_UNSUPPORTED);
    assert(!mapped && surface_pending); created(); dirty=true; present(); assert(!dirty);
    assert(calls.back().service==PXA_SURFACE_SERVICE && calls.back().opcode==PXA_SURFACE_QUEUE_FRAME);
    puts("PASS resize with unchanged capped canvas, Host scale change and copied-frame fallback");
    // Test the original menu action, then the Host-facing event result. A BACK
    // marked HANDLED after run_game returns leaves the Host application alive.
    release_input(); help_open=false; SFG_game.state=SFG_GAME_STATE_MENU;
    SFG_game.save[0]=0; SFG_game.continues=1;
    memset(SFG_game.keyStates,0,sizeof(SFG_game.keyStates));
    touch(80,82);
    assert(SFG_getMenuItem(SFG_game.selectedMenuItem)==SFG_MENU_ITEM_EXIT);
    SFG_gameStep(); assert(!SFG_game.continues);
    touch(80,82,PXA_POINTER_UP);
    assert(!SFG_mainLoopBody());
    uint8_t back[PXA_HEADER_BYTES]; uint32_t back_size;
    assert(pxa_build_message(back,sizeof(back),PXA_WINDOW_SERVICE,PXA_WINDOW_BACK_REQUESTED,0,nullptr,0,&back_size));
    running=false; finished=false; foreground_tick=false; help_open=true;
    assert(pxa_app_on_event(back,back_size)==PXA_EVENT_UNHANDLED);
    assert(help_open); // No game-state access before initialization.
    running=true; SFG_game.continues=1;
    assert(pxa_app_on_event(back,back_size)==PXA_EVENT_HANDLED);
    assert(!help_open && SFG_game.continues);
    SFG_game.state=SFG_GAME_STATE_PLAYING;
    assert(pxa_app_on_event(back,back_size)==PXA_EVENT_HANDLED);
    assert(SFG_game.state==SFG_GAME_STATE_MENU && SFG_game.continues);
    size_t before_back=calls.size();
    assert(pxa_app_on_event(back,back_size)==PXA_EVENT_HANDLED);
    assert(!SFG_game.continues && calls.size()==before_back);
    finished=true;
    assert(pxa_app_on_event(back,back_size)==PXA_EVENT_UNHANDLED);
    assert(calls.size()==before_back); // Exit BACK reaches Host without requeue.
    puts("PASS Exit: original menu touch stops loop; BACK before init/after exit reaches Host; help/play/menu handling and status passthrough");
    printf("PASS frontend native checks; frame allocation=%zu bytes\n",sizeof(frames));
}
