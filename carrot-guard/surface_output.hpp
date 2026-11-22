// SPDX-License-Identifier: MIT
#pragma once
#include "pxa_surface.h"
#include "pixel_renderer.hpp"
#include <cstdlib>

namespace carrot {
class SurfaceOutput {
  static constexpr uint64_t Create=7301,Configure=7302,Query=7303;
  uint64_t handle=0,frameId=0;
  uint8_t* buffers=nullptr;
  uint32_t frameBytes=0,stride=0;
  bool requested=false,ready=false;
  int width=480,height=480;
  uint32_t density=65536;
public:
  PixelRenderer painter;
  void query(){uint8_t packet[64];uint32_t n=0;if(handle&&pxa_surface_build_query_state(packet,sizeof(packet),Query,handle,&n))pxa_control_packet(packet,n);}
  void stop() {
    if(handle)pxa_close_handle(handle);
    if(requested&&!handle)pxa_cancel(Create);
    handle=0;requested=ready=false;
    std::free(buffers);buffers=nullptr;
  }
  void resize(int w,int h,uint32_t d) {
    if(w==width&&h==height&&d==density&&requested)return;
    stop();width=w;height=h;density=d;painter.resize(w,h,d);
    uint8_t packet[64];uint32_t n=0;
    pxa_surface_desc_t desc{uint16_t(w),uint16_t(h),PXA_SURFACE_RGB565,2,
      PXA_SURFACE_PREFER_DIRECT_SCANOUT|PXA_SURFACE_GUEST_MAPPED};
    requested=pxa_surface_build_create(packet,sizeof(packet),Create,desc,&n)&&pxa_control_packet(packet,n)==PXA_STATUS_OK;
  }
  bool event(const pxa_event_t& e) {
    if(painter.event(e))return true;
    pxa_surface_state_t state{};
    if(pxa_surface_parse_state(&e,Query,&state)){
      char message[240];std::snprintf(message,sizeof(message),"carrot presentation submitted=%llu presented=%llu dropped=%llu replaced=%llu flags=%u",
        (unsigned long long)state.submitted_frames,(unsigned long long)state.presented_frames,
        (unsigned long long)state.dropped_frames,(unsigned long long)state.replaced_frames,state.flags);pxa_log_write(2,message);return true;
    }
    pxa_surface_create_result_t result{};
    if(pxa_surface_parse_create(&e,Create,&result)) {
      if(result.status!=PXA_STATUS_OK){pxa_log_write(3,"carrot Surface create failed");return true;}
      handle=result.handle;frameBytes=result.frame_bytes;stride=result.stride_bytes;
      if(result.buffer_count!=2||stride<unsigned(width)*2||frameBytes<stride*height){pxa_log_write(3,"carrot Surface geometry rejected");stop();return true;}
      buffers=static_cast<uint8_t*>(std::aligned_alloc(64,(size_t(frameBytes)*2+63)&~size_t(63)));
      if(!buffers||pxa_surface_register_buffers(handle,buffers,frameBytes,2)<0){pxa_log_write(3,"carrot Surface buffers unavailable");stop();return true;}
      uint8_t packet[64];uint32_t n=0;
      if(pxa_surface_build_configure_layer(packet,sizeof(packet),Configure,handle,0,0,width,height,0,1,&n))pxa_control_packet(packet,n);
      return true;
    }
    int32_t status=0;
    if(pxa_surface_parse_status(&e,Configure,PXA_SURFACE_CONFIGURE_LAYER,&status)) {
      ready=status==PXA_STATUS_OK;
      pxa_log_write(ready?1:3,ready?"carrot mapped Surface ready":"carrot Surface configure failed");return true;
    }
    return false;
  }
  bool paint(const uint8_t* data,size_t bytes,size_t prefix,size_t hudBegin=0,size_t hudEnd=0,uint64_t revision=0,int top=0,int bottom=0,size_t hudSplit=0) {
    painter.pump();
    if(!ready||painter.loadingBackground()||!painter.stageReady(data,prefix))return false;
    uint8_t index=0;if(pxa_surface_acquire_buffer(handle,&index)!=4||index>=2)return false;
    auto dest=reinterpret_cast<uint16_t*>(buffers+size_t(index)*frameBytes);
    bool ok=painter.paint(dest,stride/2,data,bytes,prefix,hudBegin,hudEnd,revision,top,bottom,hudSplit,index);
    if(!ok&&!painter.pending())pxa_log_write(3,"carrot software display list rejected");
    return pxa_surface_present_buffer(handle,index,++frameId)>=0&&ok;
  }
  bool available()const{return ready;}
};
} // namespace carrot
