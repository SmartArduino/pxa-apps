// SPDX-License-Identifier: MIT
// Images use Host pixels at display resolution, independent of the game surface.
// Only visible resources are requested; all handles are owned by this app.
#include "pxa_assets.h"
struct ImageResource {
    const char *path=nullptr;uint64_t handle=0;unsigned bytes=0;bool wanted=false,failed=false;
};
struct ImageDraw {milky::NativeImage source;milky::Rect target;int resource;bool visible=true;};
ImageResource image_resources[32];ImageDraw image_draws[64];
unsigned image_draw_count,image_load_slot;uint64_t image_load_token,image_next_token=9000;
constexpr unsigned IMAGE_BYTES_LIMIT=224*1024;
bool image_slots_full;
void native_images_begin(){image_draw_count=0;image_slots_full=false;for(auto &r:image_resources)r.wanted=false;}
int native_image_resource(const milky::NativeImage &image){
    if(!image.path)return -1;
    for(unsigned i=0;i<32;++i){auto &r=image_resources[i];if(r.path&&!strcmp(r.path,image.path)){r.wanted=true;return int(i);}}
    for(unsigned i=0;i<32;++i){auto &r=image_resources[i];if(!r.path){r={image.path,0,unsigned(image.width)*image.height*4,true,false};return int(i);}}
    image_slots_full=true;return -1;
}
bool native_image_add(const milky::Sprite &s,int x,int y,int w,int h){
    if(image_draw_count>=64||!s.large.path)return false;
    const int native_w=w*viewport.physical_width()/WIDTH,native_h=h*viewport.physical_height()/HEIGHT;
    const int side=std::max(native_w,native_h);
    const milky::NativeImage *tiers[]={&s.large,&s.small,&s.medium,&s.tiny};
    unsigned tier=side<=32?3:side<=64?2:side<=144?1:0;
    unsigned wanted=0;for(const auto &r:image_resources)if(r.wanted)wanted+=r.bytes;
    for(;tier<4;++tier){const auto &source=*tiers[tier];if(!source.path)continue;
        bool exists=false;for(const auto &r:image_resources)if(r.wanted&&r.path&&!strcmp(r.path,source.path))exists=true;
        if(!exists&&wanted+unsigned(source.width)*source.height*4>IMAGE_BYTES_LIMIT)continue;
        int slot=native_image_resource(source);if(slot<0)return false;
        image_draws[image_draw_count++]={source,{x,y,w,h},slot,true};
        return image_resources[slot].handle!=0;
    }
    return false;
}
void native_images_occlude(int x,int y,int w,int h){
    if(w<=2||h<=2)return;
    for(unsigned i=0;i<image_draw_count;++i){auto &d=image_draws[i];const auto &r=d.target;
        if(x<r.right()&&x+w>r.x&&y<r.bottom()&&y+h>r.y)d.visible=false;
    }
}
bool native_images_encode(pxa_canvas_frame_t *frame){
    for(unsigned i=0;i<image_draw_count;++i){const auto &d=image_draws[i];const auto &s=d.source;
        if(!d.visible||!image_resources[d.resource].handle)continue;
        const auto &r=d.target;
        const int left=system_text_logical(viewport.x+r.x*viewport.physical_width()/WIDTH);
        const int top=system_text_logical(viewport.y+r.y*viewport.physical_height()/HEIGHT);
        const int right=system_text_logical(viewport.x+r.right()*viewport.physical_width()/WIDTH);
        const int bottom=system_text_logical(viewport.y+r.bottom()*viewport.physical_height()/HEIGHT);
        const int width=std::max(1,right-left),height=std::max(1,bottom-top);
        if(!pxa_canvas_clip_push(frame,left,top,width,height)||
            !pxa_canvas_image_handle(frame,left-int(int64_t(s.x)*width/s.w),top-int(int64_t(s.y)*height/s.h),
                std::max(1,int(int64_t(s.width)*width/s.w)),std::max(1,int(int64_t(s.height)*height/s.h)),255,1,image_resources[d.resource].handle)||
            !pxa_canvas_clip_pop(frame))return false;
    }
    return true;
}
// Retire old Canvas pins BEFORE closing or loading resources for the next frame.
void native_images_service(){
    for(unsigned i=0;i<32;++i){auto &r=image_resources[i];if(!r.path||r.wanted||(image_load_token&&i==image_load_slot))continue;
        if(r.handle&&pxa_close_handle(r.handle))continue;r={};
    }
    if(image_slots_full)pocket::redraw();
    if(image_load_token)return;
    unsigned resident=0;for(const auto &r:image_resources)if(r.handle)resident+=r.bytes;
    for(unsigned i=0;i<32;++i){auto &r=image_resources[i];if(!r.path||!r.wanted||r.handle||r.failed)continue;
        if(r.bytes>IMAGE_BYTES_LIMIT-resident){r.failed=true;continue;}
        image_load_slot=i;image_load_token=++image_next_token;
        if(pxa_assets_load_image(image_load_token,r.path)){r.failed=true;image_load_token=0;}
        return;
    }
}
bool native_images_event(const pxa_event_t &event){
    if(!image_load_token||event.service!=PXA_ASSETS_SERVICE||event.opcode!=PXA_ASSETS_LOAD||event.token!=image_load_token)return false;
    auto &r=image_resources[image_load_slot];pxa_asset_result_t result{};
    if(pxa_assets_parse_result(&event,image_load_token,PXA_ASSETS_LOAD,&result)&&!result.status&&result.info.kind==PXA_ASSET_IMAGE){r.handle=result.handle;}
    else{r.failed=true;char message[180];snprintf(message,sizeof(message),"Milky: image unavailable status=%ld path=%s; using embedded fallback",(long)result.status,r.path);pxa_log_write(3,message);}
    image_load_token=0;system_text_hash=0;pocket::redraw();return true;
}
