#pragma once
// Diagnostic build only. No production instrumentation or reserved buffer.
#include <pxa/app.hpp>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
namespace stack_probe {
extern "C" unsigned char __stack_low,__stack_high;
inline std::uintptr_t base=0,low=0,high=0,paint_end=0;
inline std::size_t maximum=0;
inline unsigned task_peak=0,task_failures=0;
inline unsigned char pattern(std::uintptr_t address){return 0xa5^(address*37>>3);}
[[gnu::noinline]] inline void exercise(){
 volatile unsigned char bytes[4096];
 for(std::size_t i=0;i<sizeof bytes;++i)bytes[i]=static_cast<unsigned char>(i);
}
[[gnu::noinline]] inline void initialize(){
 volatile unsigned char marker=0;
 base=reinterpret_cast<std::uintptr_t>(&__stack_low);low=base+16;
 high=reinterpret_cast<std::uintptr_t>(&__stack_high);
 const auto live=reinterpret_cast<std::uintptr_t>(&marker);
 if(live<=low+1024||live>=high)std::abort();
 paint_end=live-1024;
 // Leave address zero/the bottom 16 B untouched. Never paint active frames.
 for(auto at=low;at<paint_end;++at)*reinterpret_cast<volatile unsigned char*>(at)=pattern(at);
 exercise();
}
[[gnu::noinline]] inline void sample(pxa::Context& context){
 auto at=low;
 while(at<paint_end&&*reinterpret_cast<volatile unsigned char*>(at)==pattern(at))++at;
 const auto used=high-at;
 const auto tasks=pxa::task_pool_stats();
 if(used>maximum||tasks.peak_slots>task_peak||tasks.allocation_failures!=task_failures){
  maximum=std::max(maximum,std::size_t(used));task_peak=tasks.peak_slots;task_failures=tasks.allocation_failures;char text[192];
  const auto n=std::snprintf(text,sizeof text,"WEREAD STACK PROBE reserved=%zu high_water_upper=%zu painted=%zu task_peak=%u task_failures=%u slot_bytes=%zu",std::size_t(high-base),maximum,std::size_t(paint_end-low),task_peak,task_failures,tasks.slot_bytes);
  if(n>0)(void)context.log().write(pxa::LogLevel::info,std::string_view(text,std::size_t(n)));
 }
}
}
