// SPDX-License-Identifier: MIT
#pragma once
#include "pxa_window.h"
#include <cstring>

namespace dewfield_window {
inline void prefer_landscape() {
  // Window 0.5 orientation-preference=landscape. The optional record keeps
  // Window 0.4 Hosts usable; square screens retain their current angle.
  const uint8_t configuration[] = {9, 0x80, 1, 0, 2};
  (void)pxa_control_call(PXA_WINDOW_SERVICE, PXA_WINDOW_CONFIGURE, 0,
                       configuration, sizeof configuration);
}
inline bool snapshot(const pxa_event_t &event, uint64_t token,
                     pxa_window_snapshot_t &out) {
  if (pxa_window_parse_snapshot(&event, token, &out) ||
      pxa_window_parse_metrics_changed(&event, &out)) return true;
#ifdef PXA_WINDOW_DISPLAY_SNAPSHOT_RECORD_BYTES
  return false;
#else
  // Orientation opts newer Hosts into a display snapshot whose first 98
  // bytes retain the old layout. Older SDKs still need its navigation insets.
  bool reply=event.service==PXA_WINDOW_SERVICE && event.opcode==PXA_WINDOW_GET_SNAPSHOT && event.token==token;
  bool changed=event.service==PXA_WINDOW_SERVICE && event.opcode==PXA_WINDOW_METRICS_CHANGED;
  unsigned prefix=reply?4:0;
  if ((!reply&&!changed)||!event.payload||event.payload_size!=120+prefix ||
      (reply&&pxa_load_u32(event.payload))) return false;
  uint8_t legacy[102];std::memcpy(legacy,event.payload,98+prefix);
  if (legacy[prefix+92]==3) legacy[prefix+92]=0; // Square in Window 0.5.
  auto compatible=event;compatible.payload=legacy;compatible.payload_size=98+prefix;
  return pxa_window_parse_snapshot(&compatible,token,&out) ||
         pxa_window_parse_metrics_changed(&compatible,&out);
#endif
}
} // namespace dewfield_window
