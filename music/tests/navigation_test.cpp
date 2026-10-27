// SPDX-License-Identifier: MIT
#include "../main.cpp"
#include <cassert>
static unsigned back_requests = 0, storage_writes = 0, control_calls = 0;
extern "C" int32_t pxa_call(uint32_t op, uint64_t, const uint8_t *, uint32_t) {
  ++control_calls;
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
  view = 0;
  click(SearchOpen);
  assert(view == 3 && search_return_view == 0);
  std::snprintf(draft, sizeof(draft), "晴天");
  search();
  assert(view == 1 && !std::strcmp(query, "晴天"));
  count = 2;
  tracks[0].id = 1;
  tracks[1].id = 2;
  focus = 1;
  scroll.layout(77, 193, 48, 12);
  scroll.offset = scroll.target = 88;
  choose(tracks[1]);
  assert(view == 4 && player_return_view == 1);
  click(ShowLyrics);
  assert(view == 5);
  click(Back);
  assert(view == 4);
  click(Back);
  assert(view == 1 && focus == 1 && scroll.offset == 88 && count == 2);
  click(SearchOpen);
  assert(view == 3 && search_return_view == 0);
  search();
  assert(view == 1);
  click(Back);
  assert(view == 3);
  click(Back);
  assert(view == 0);
  view = 2;
  click(SearchOpen);
  assert(search_return_view == 2);
  click(Back);
  assert(view == 2);
  click(Back);
  assert(view == 0);
  uint8_t event[32]{};
  uint32_t n = 0;
  assert(pxa_build_message(event, sizeof(event), PXA_WINDOW_SERVICE,
                           PXA_WINDOW_BACK_REQUESTED, 0, nullptr, 0, &n));
  view = 4;
  player_return_view = 1;
  dirty = false;
  assert(pxa_app_on_event(event, n) == PXA_EVENT_HANDLED && view == 1);
  view = 0;
  assert(pxa_app_on_event(event, n) == PXA_EVENT_UNHANDLED);
  click(Back);
  assert(back_requests == 1);
  view = 1;
  player_return_view = 2;
  choose(tracks[0], true);
  assert(view == 1 && player_return_view == 2);
  view = 5;
  choose(tracks[1], true);
  assert(view == 5 && player_return_view == 2);
  // A list tap on the active song reopens the player without touching the
  // current session, queued samples, pending metadata or playback queue.
  view = 1;
  count = 2;
  tracks[1] = current;
  have_current = want_play = true;
  paused = false;
  audio = UINT64_C(0x10000000b);
  stream = UINT64_C(0x100000007);
  cover = UINT64_C(0x10000000d);
  job_token = 999;
  position = 42321;
  played_frames = 123456;
  sent_frames = 125000;
  queue_count = 2;
  queue_index = 1;
  queue[0].id = 777;
  scene.initialized = 1;
  auto before_calls = control_calls;
  click(TrackBase + 1);
  assert(view == 4 && player_return_view == 1 && position == 42321 &&
         played_frames == 123456 && sent_frames == 125000 && !paused &&
         audio == UINT64_C(0x10000000b) && stream == UINT64_C(0x100000007) &&
         cover == UINT64_C(0x10000000d) && job_token == 999 &&
         queue_count == 2 && queue_index == 1 && queue[0].id == 777 &&
         control_calls == before_calls && scene.initialized);
  assert(navigate_back() && view == 1 && scene.initialized);
  paused = true;
  click(TrackBase + 1);
  assert(view == 4 && paused && position == 42321 &&
         control_calls == before_calls);
  assert(!storage_writes);
  std::puts("music: system/controller/toolbar back, lyrics -> player -> search "
            "results -> search -> origin, scroll/focus retention and "
            "current-song reopen preserves session and pause state passed");
}
