// SPDX-License-Identifier: MIT
#include "../window_preferences.hpp"
#include <cassert>
#include <cstdio>
static int host_status;
extern "C" int32_t pxa_call(uint32_t route,uint64_t token,const uint8_t *data,uint32_t size) {
  assert(route==(PXA_WINDOW_SERVICE|(PXA_WINDOW_CONFIGURE<<16))&&token==0);
  assert(size==5&&pxa_load_u16(data)==0x8009&&pxa_load_u16(data+2)==1&&data[4]==2);
  return host_status;
}
int main() {
  dewfield_window::prefer_landscape();host_status=-3;
  dewfield_window::prefer_landscape(); // Older Hosts ignore the optional preference.
  uint8_t bytes[124]{};size_t size;
  pxa_window_snapshot_t sample{};sample.revision=1;
  sample.logical_width=854;sample.logical_height=480;
  sample.pixel_width=854;sample.pixel_height=480;
  sample.density_numerator=sample.density_denominator=1;
  sample.system_bar_insets.bottom=24;sample.orientation=2;sample.focused=1;
  assert(pxa_window_snapshot_records_encode(bytes+4,120,&sample,&size)&&size==98);
  pxa_event_t event{};event.service=PXA_WINDOW_SERVICE;event.opcode=PXA_WINDOW_GET_SNAPSHOT;
  event.token=77;event.payload=bytes;event.payload_size=102;
  pxa_window_snapshot_t result{};
  assert(dewfield_window::snapshot(event,77,result)&&result.logical_width==854&&result.system_bar_insets.bottom==24);
  auto *extra=bytes+4+98;
  pxa_store_u16(extra,9);pxa_store_u16(extra+2,1);extra[4]=1;
  pxa_store_u16(extra+5,10);pxa_store_u16(extra+7,8);
  pxa_store_u32(extra+9,480);pxa_store_u32(extra+13,854);
  pxa_store_u16(extra+17,11);pxa_store_u16(extra+19,1);extra[21]=15;
  event.payload_size=124;
  assert(dewfield_window::snapshot(event,77,result)&&result.logical_height==480&&result.system_bar_insets.bottom==24);
  assert(!dewfield_window::snapshot(event,78,result));
  // Square screens keep their navigation margins with both SDK generations.
  pxa_store_u32(bytes+4+16,480);pxa_store_u32(bytes+4+28,480);bytes[4+92]=3;
  event.opcode=PXA_WINDOW_METRICS_CHANGED;event.token=0;event.payload=bytes+4;event.payload_size=120;
  assert(dewfield_window::snapshot(event,77,result)&&result.logical_width==480&&result.system_bar_insets.bottom==24);
  event.service=9;assert(!dewfield_window::snapshot(event,77,result));
  puts("PASS: optional landscape, rotated/square metrics and navigation insets with old and new SDKs");
}
