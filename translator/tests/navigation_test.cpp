// SPDX-License-Identifier: MIT
#include "../main.cpp"
#include <cassert>
static unsigned back_calls = 0;
static uint16_t clock_ms = 0;
extern "C" int32_t pxa_call(uint32_t operation, uint64_t, const uint8_t *p,
                            uint32_t n) {
  if (operation == (PXA_WINDOW_REQUEST_BACK << 16 | PXA_WINDOW_SERVICE))
    ++back_calls;
  if (operation == (PXA_CLOCK_SET_PERIOD << 16 | PXA_CLOCK_SERVICE) && n == 2)
    clock_ms = pxa_load_u16(p);
  return 0;
}
extern "C" int32_t pxa_io(uint64_t, uint32_t, uint8_t *, uint32_t) {
  return PXA_STATUS_WOULD_BLOCK;
}
int main() {
  pxa_app_start(nullptr, 0);
  assert(clock_ms ==
         20); // The legacy helper takes milliseconds, not microseconds.
  for (auto size : {std::pair{160, 128},
                    {240, 200},
                    {240, 240},
                    {240, 284},
                    {296, 240},
                    {320, 160},
                    {284, 284},
                    {390, 844},
                    {800, 480},
                    {1280, 720}}) {
    scene.viewport.content = {0, 0, size.first, size.second};
    for (View page : {View::Home, View::Editor, View::Languages, View::History,
                      View::Settings, View::Network, View::VoiceSettings,
                      View::Voice, View::Output}) {
      for (unsigned mode = 0; mode < 3; ++mode) {
        editing.provider = mode != 0;
        std::snprintf(record.output, sizeof(record.output), "%s",
                      mode ? "测试译文" : "");
        form_page = mode;
        history_count = 4;
        for (auto &r : history) {
          std::strcpy(r.input, "Test input");
          std::strcpy(r.output, "测试译文");
        }
        change_view(page);
        render();
        assert(!scene.frame.failed && scene.count);
        for (unsigned i = 0; i < scene.count; ++i) {
          auto a = scene.hits[i];
          assert(a.x >= 0 && a.y >= 0 && a.w > 0 && a.h > 0 &&
                 a.x + a.w <= size.first && a.y + a.h <= size.second);
          for (unsigned j = 0; j < i; ++j) {
            auto b = scene.hits[j];
            assert(a.id != b.id);
            if (!(a.x >= b.x + b.w || b.x >= a.x + a.w || a.y >= b.y + b.h ||
                  b.y >= a.y + a.h)) {
              std::fprintf(stderr,
                           "Overlap %dx%d page=%d ids=%u,%u "
                           "rectangles=%d,%d,%d,%d / %d,%d,%d,%d\n",
                           size.first, size.second, int(page), a.id, b.id, a.x,
                           a.y, a.w, a.h, b.x, b.y, b.w, b.h);
              std::abort();
            }
          }
        }
      }
    }
  }
  scene.viewport.content = {0, 0, 1080, 1920};
  std::memset(record.output, 'a', sizeof(record.output) - 1);
  record.output[sizeof(record.output) - 1] = 0;
  change_view(View::Output);
  render();
  assert(!scene.frame.failed && output_total > 0);
  scene.begin(1080, 1920, Paper);
  auto bytes = scene.frame.length;
  assert(wrap(record.output, {12, 40, 1056, 1840}, Ink, 0, 20, false) > 0);
  assert(scene.frame.length ==
         bytes); // Measurement does not duplicate draw commands.
  view = View::Editor;
  std::strcpy(draft, "A draft");
  assert(back() && view == View::Home && !std::strcmp(record.input, "A draft"));
  view = View::Settings;
  click(Quit);
  assert(back_calls == 1 && view == View::Home);
  std::puts(
      "PASS: 10 viewport sizes, native control bounds and disjoint targets on "
      "every page, draft return, explicit exit and autonomous 20 ms clock.");
}
