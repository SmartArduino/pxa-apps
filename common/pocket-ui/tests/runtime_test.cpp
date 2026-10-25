// SPDX-License-Identifier: CC0-1.0
// Exercise the actual runtimes against the SDK wire codec, without a Host.
#include <assert.h>
#include <string.h>
#include <vector>
#if defined(POCKET_TEST_CHUNKED)
#include "../../pocket-play/chunked-runtime.cpp"
#elif defined(POCKET_TEST_PLAY)
#include "../../pocket-play/runtime.cpp"
#else
#include "../../pocket-game/runtime.cpp"
#endif

namespace {
struct Request { uint16_t service, opcode; uint64_t token; std::vector<uint8_t> data; };
std::vector<Request> requests;
int storage_submit_status, present_blocks, queue_blocks, draw_count, acquire_count, writes, registrations, closes;
uint8_t acquired_index = 1;
uint16_t painted = 0x1234;
int pointer_count, pointer_x, pointer_y;
pocket::Phase pointer_phase;
void reset_preferences() {
    pocket::set_english(false);
    strcpy(preferences_key, "test.prefs.v1");
    preferences_loaded=preferences_loading=preferences_dirty=preferences_pending=language_changed=false;
    preferences_load_retry=preferences_write_retry=0;
    storage_submit_status=0; requests.clear();
}
int reply_pref_get(int32_t status, const uint8_t *value=nullptr, size_t size=0) {
    uint8_t payload[32]{};
    pxa_store_u32(payload,uint32_t(status));
    size_t record=0;
    if (!status) assert(pxa_wire_record_encode(payload+4,sizeof(payload)-4,2,value,size,&record));
    pxa_event_t e{PXA_STORAGE_SERVICE,PXA_STORAGE_GET,PREFERENCES_LOAD,payload,uint32_t(4+record)};
    return handle_preferences(e);
}
int reply_pref_set(int32_t status) {
    uint8_t payload[4]; pxa_store_u32(payload,uint32_t(status));
    pxa_event_t e{PXA_STORAGE_SERVICE,PXA_STORAGE_SET,PREFERENCES_SAVE,payload,4};
    return handle_preferences(e);
}
int reply_create(int32_t status=0) {
    uint8_t payload[24]{}; pxa_store_u32(payload,uint32_t(status));
    if (!status) {
        pxa_store_u64(payload+4,42); pxa_store_u32(payload+12,uint32_t(viewport.width*2));
        pxa_store_u32(payload+16,frame_bytes); payload[20]=2;
    }
    pxa_event_t e{PXA_SURFACE_SERVICE,PXA_SURFACE_CREATE,CREATE,payload,status?4u:24u};
    return handle_surface(e);
}
int reply_configure() {
    uint8_t payload[4]{};
    pxa_event_t e{PXA_SURFACE_SERVICE,PXA_SURFACE_CONFIGURE_LAYER,CONFIGURE,payload,4};
    return handle_surface(e);
}
void test_preferences() {
    reset_preferences();
    storage_submit_status=PXA_STATUS_WOULD_BLOCK; service_preferences();
    assert(!preferences_loading && preferences_load_retry>0);
    storage_submit_status=0; preferences_load_retry=0; service_preferences();
    assert(preferences_loading);
    assert(reply_pref_get(PXA_STATUS_INTERNAL)==PXA_EVENT_HANDLED);
    assert(!preferences_loaded && !preferences_loading && preferences_load_retry>0);
    preferences_load_retry=0; service_preferences();
    assert(reply_pref_get(PXA_STATUS_NOT_FOUND)==PXA_EVENT_HANDLED);
    assert(preferences_loaded && !pocket::english());
    reset_preferences(); service_preferences();
    const uint8_t english_pref[]={'P','L',1,1};
    assert(reply_pref_get(0,english_pref,4)==PXA_EVENT_HANDLED && pocket::english());
    reset_preferences(); service_preferences();
    // A delayed load cannot overwrite a language the user just selected.
    toggle_language(); assert(pocket::english() && preferences_pending);
    const uint8_t chinese_pref[]={'P','L',1,0};
    assert(reply_pref_get(0,chinese_pref,4)==PXA_EVENT_HANDLED && pocket::english());
    toggle_language(); assert(!pocket::english() && preferences_dirty);
    assert(reply_pref_set(0)==PXA_EVENT_HANDLED && preferences_dirty && !preferences_pending);
    service_preferences(); assert(preferences_pending && !preferences_dirty && preferences_payload[3]==0);
    assert(reply_pref_set(PXA_STATUS_INTERNAL)==PXA_EVENT_HANDLED);
    assert(preferences_dirty && !preferences_pending && preferences_write_retry>0);
    preferences_write_retry=0; service_preferences();
    assert(preferences_pending && preferences_payload[3]==0);
    assert(reply_pref_set(0)==PXA_EVENT_HANDLED && !preferences_pending && !preferences_dirty);
    // Malformed preferences fall back to Chinese, without touching game state.
    reset_preferences(); service_preferences();
    const uint8_t invalid[]={'P','L',1,8};
    assert(reply_pref_get(0,invalid,4)==PXA_EVENT_HANDLED && preferences_loaded && !pocket::english());
    for (const auto &r : requests) if (r.service==PXA_STORAGE_SERVICE) {
        assert(r.token==PREFERENCES_LOAD || r.token==PREFERENCES_SAVE);
        pxa_wire_record_view_t key; size_t consumed;
        assert(pxa_wire_record_decode(r.data.data(),r.data.size(),&key,&consumed));
        assert(key.raw_tag==1 && key.payload_size==strlen(preferences_key));
        assert(!memcmp(key.payload,preferences_key,key.payload_size));
    }
}
void test_surface_and_backpressure() {
    mapped=true; surface=0; frame_id=0; display_w=296; display_h=240;
    surface_inflight=false; background=false; dirty=true; prepared=staged=false; paused=false;
    assert(request_surface()==0 && surface_inflight && !surface_ready);
    assert(viewport.width==296 && viewport.height==230);
    // Environment changes while CREATE is outstanding. Its completion must
    // finish CONFIGURE, then close and recreate before any old frame is drawn.
    display_w=480; display_h=320;
    assert(reply_create()==PXA_EVENT_HANDLED && registrations==1);
    assert(reply_configure()==PXA_EVENT_HANDLED && surface_inflight && !surface_ready && closes==1);
    assert(viewport.width==411 && viewport.height==320);
    assert(reply_create()==PXA_EVENT_HANDLED);
    assert(reply_configure()==PXA_EVENT_HANDLED && surface_ready && !surface_inflight);
    assert(frame_bytes==411u*320u*2u);
    draw_count=acquire_count=0; present_blocks=1;
    pocket::redraw(); present();
    assert(prepared && draw_count==1 && acquire_count==1 && frame_id==0);
    assert(frames[viewport.pixels()]==painted);
    painted=0x5678; pocket::redraw(); present();
    assert(!prepared && dirty && draw_count==1 && acquire_count==1 && frame_id==1);
    present(); assert(!prepared && !dirty && draw_count==2 && frame_id==2);
    assert(frames[viewport.pixels()]==painted);
    // A surface resize also discards a retained buffer from a blocked present.
    pocket::redraw(); present_blocks=1; present(); assert(prepared);
    display_w=240; display_h=240;
    assert(request_surface()==0 && !prepared && !staged && !surface_ready);
    assert(reply_create()==PXA_EVENT_HANDLED && reply_configure()==PXA_EVENT_HANDLED);
    // Hosts without guest mapping use the full-frame copy path. Queue pressure
    // must retain that staged copy, even after another redraw arrives.
    mapped=false; assert(request_surface()==0);
    assert(reply_create()==PXA_EVENT_HANDLED && reply_configure()==PXA_EVENT_HANDLED);
    draw_count=writes=0; queue_blocks=1; pocket::redraw(); present();
    assert(prepared && staged && writes==1 && draw_count==1);
    pocket::redraw(); present(); assert(!prepared && !staged && dirty && writes==1 && draw_count==1);
    present(); assert(!dirty && writes==2 && draw_count==2);
    // Capture permits out-of-bounds release coordinates, but rejects starts
    // in letterboxing. Move and cancel preserve the active pointer identity.
    running=true; exit_requested=false; paused=false; pointer_active=false;
    pxa_ui_pointer_data_t p{}; p.pointer_id=7; p.phase=PXA_POINTER_DOWN;
    p.x=viewport.x-1; p.y=viewport.y; pointer(p); assert(!pointer_active);
    p.x=viewport.x+viewport.physical_width()/2; p.y=viewport.y+viewport.physical_height()/2;
    pointer_count=0; pointer(p); assert(pointer_active && pointer_count==1);
    p.phase=PXA_POINTER_UP; p.x=viewport.x-1; pointer(p);
    assert(!pointer_active && pointer_phase==pocket::Phase::UP && pointer_x<0);
    p.phase=PXA_POINTER_DOWN; p.x=viewport.x+viewport.physical_width()/2; pointer(p);
    cancel_input(); assert(!pointer_active && pointer_phase==pocket::Phase::CANCEL);
    // Fourth menu row changes language and keeps pause; the first three rows
    // retain their original y72, y102 and y132 coordinates.
    paused=true; menu_row=3; preferences_loaded=true; preferences_dirty=preferences_pending=false;
    bool was_english=pocket::english(); menu_activate();
    assert(paused && pocket::english()!=was_english);
}
}
extern "C" int32_t pxa_call(uint32_t route,uint64_t token,const uint8_t *data,uint32_t length) {
    uint16_t service=uint16_t(route), opcode=uint16_t(route>>16);
    requests.push_back({service,opcode,token,std::vector<uint8_t>(data,data+length)});
    if (service==PXA_STORAGE_SERVICE) return storage_submit_status;
    if (service==PXA_CORE_SERVICE && opcode==PXA_CORE_CLOSE_HANDLE) ++closes;
    if (service==PXA_SURFACE_SERVICE && opcode==PXA_SURFACE_QUEUE_FRAME && queue_blocks>0) {
        --queue_blocks; return PXA_STATUS_WOULD_BLOCK;
    }
    return 0;
}
extern "C" int32_t pxa_io(uint64_t,uint32_t operation,uint8_t *data,uint32_t length) {
    if (operation==PXA_SURFACE_IO_REGISTER_BUFFERS) {++registrations;return int32_t(length);}
    if (operation==PXA_SURFACE_IO_ACQUIRE) {++acquire_count;data[0]=acquired_index;return 4;}
    if (operation==PXA_SURFACE_IO_PRESENT) {
        if (present_blocks>0) {--present_blocks;return PXA_STATUS_WOULD_BLOCK;}
        return 16;
    }
    if (operation==2) {++writes;return int32_t(length);}
    return PXA_STATUS_WOULD_BLOCK;
}
namespace game {
const char *title(){return "Test";} const char *app_id(){return "test";}
void init(){} void reset(){} void key(pocket::Key){} void tick(uint32_t){}
void pointer(pocket::Phase phase,int x,int y){++pointer_count;pointer_x=x;pointer_y=y;pointer_phase=phase;}
void draw(){++draw_count;pocket::clear(painted);}
size_t serialize(uint8_t*,size_t){return 0;} void load(const uint8_t*,size_t){}
}
int main(){test_preferences();test_surface_and_backpressure();puts("Pocket runtime: async preferences / retry / resize / mapped+copied backpressure / pointer capture / fourth menu row passed");}
