// SPDX-License-Identifier: MIT
#pragma once
#include <chrono>
#include <cstdio>
namespace iron {
// Opt-in developer instrumentation; no overlay, absent in normal packages.
struct RenderProfile {
 uint64_t epoch=0,frames=0,draw=0,copy=0;
 static uint64_t now(){return std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();}
 template<class Context> void present(Context& context,uint64_t begin,uint64_t drawn,uint64_t end){
  if(!epoch)epoch=begin;++frames;draw+=drawn-begin;copy+=end-drawn;
  if(end-epoch<5000000)return;
  char line[180];std::snprintf(line,sizeof line,"Iron Dawn perf frames=%llu window_us=%llu fps_x100=%llu draw_us=%llu copy_us=%llu",(unsigned long long)frames,(unsigned long long)(end-epoch),(unsigned long long)(frames*100000000/(end-epoch)),(unsigned long long)(draw/frames),(unsigned long long)(copy/frames));
  (void)context.log().write(pxa::LogLevel::info,line);epoch=end;frames=draw=copy=0;
 }
};
}
