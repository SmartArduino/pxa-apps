// SPDX-License-Identifier: MIT
#include "../main.cpp"
#include <cassert>
static unsigned back_requests = 0, storage_writes = 0;
extern "C" int32_t pxa_call(uint32_t op, uint64_t, const uint8_t *, uint32_t) {
  if (op == (PXA_WINDOW_REQUEST_BACK << 16 | PXA_WINDOW_SERVICE))
    ++back_requests;
  if (op == (PXA_STORAGE_SET << 16 | PXA_STORAGE_SERVICE))
    ++storage_writes;
  return 0;
}
extern "C" int32_t pxa_io(uint64_t, uint32_t, uint8_t *, uint32_t) {
  return PXA_STATUS_WOULD_BLOCK;
}
int main() {
  view = 1;
  std::snprintf(query, sizeof(query), "jazz");
  count = 2;
  focus_index = 1;
  scroll.offset = scroll.target = 43;
  click(PlayerView);
  assert(player_view);
  click(BackView);
  assert(!player_view && view == 1 && count == 2 && focus_index == 1 &&
         scroll.offset == 43 && !std::strcmp(query, "jazz"));
  click(Custom);
  assert(view == 3 && custom_return_view == 1);
  click(PlayerView);
  click(BackView);
  assert(view == 3 && !player_view);
  click(BackView);
  assert(view == 1);
  click(BackView);
  assert(view == 0 && !query[0]);
  uint8_t event[32]{};
  uint32_t n = 0;
  assert(pxa_build_message(event, sizeof(event), PXA_WINDOW_SERVICE,
                           PXA_WINDOW_BACK_REQUESTED, 0, nullptr, 0, &n));
  search_open = true;
  assert(pxa_app_on_event(event, n) == PXA_EVENT_HANDLED && !search_open);
  assert(pxa_app_on_event(event, n) == PXA_EVENT_UNHANDLED);
  click(BackView);
  assert(back_requests == 1);
  click(VolumeUp);
  assert(!storage_writes && volume_hint_until > now_ms());
  std::puts("radio: nested back, search results/query/scroll retention, custom "
            "form return, root close and system-only volume passed");
}
