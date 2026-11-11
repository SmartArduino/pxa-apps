// SPDX-License-Identifier: MIT
#pragma once
#include "pxa_assets.h"
#include <algorithm>
#include <cstring>
namespace dewfield_assets {
// A single bounded blob, loaded on scene changes. No resident Host image or
// decoder, retained Host pointers, dynamic allocation, or per-frame reads.
class Stream {
 uint8_t *destination=nullptr;
 unsigned expected=0,received=0;
 uint64_t sequence=0x4f504e00,token=0;
 char path[64]{};
 bool failed=false;
 void next() {
  token=++sequence;
  if(pxa_assets_read(token,path,received,std::min(expected-received,unsigned(PXA_ASSETS_READ_MAX_BYTES)))<0){token=0;failed=true;}
 }
public:
 void cancel(){if(token)pxa_cancel(token);token=0;destination=nullptr;expected=received=0;failed=false;path[0]=0;}
 bool load(const char*name,void*buffer,unsigned bytes) {
  if(destination!=buffer||expected!=bytes||std::strcmp(path,name)) {
   cancel();auto n=std::strlen(name);if(n>=sizeof path||!buffer||!bytes)return false;
   std::memcpy(path,name,n+1);destination=static_cast<uint8_t*>(buffer);expected=bytes;next();
  }
  return !failed&&received==expected;
 }
 bool event(const pxa_event_t&e) {
  if(!token||e.token!=token||e.service!=PXA_ASSETS_SERVICE)return false;
  pxa_asset_read_result_t r{};token=0;
  if(!pxa_assets_parse_read(&e,e.token,&r)||r.status||r.offset!=received||r.total_bytes!=expected||!r.bytes){failed=true;return true;}
  std::memcpy(destination+received,r.data,r.bytes);received+=r.bytes;
  if(received<expected)next();return true;
 }
};
}
