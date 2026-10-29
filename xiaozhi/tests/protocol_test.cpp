// SPDX-License-Identifier: Apache-2.0
#include "../protocol.hpp"
#include <cassert>
#include <cstdio>
int main() {
  auto hello = xiaozhi::parse(xiaozhi::hello);
  assert(hello && xiaozhi::text(hello.get(), "type") == "hello");
  for (unsigned duration : {20u, 40u, 60u}) {
    auto negotiated = xiaozhi::parse(xiaozhi::hello_for(duration));
    assert(negotiated);
    auto params = cJSON_GetObjectItemCaseSensitive(negotiated.get(), "audio_params");
    assert(cJSON_GetObjectItemCaseSensitive(params, "frame_duration")->valueint == int(duration));
  }
  assert(xiaozhi::hello_for(0).empty() && xiaozhi::hello_for(10).empty());
  auto message =
      xiaozhi::command("session-1", "listen", "detect", "你好\"\n\\小智");
  auto parsed = xiaozhi::parse(message);
  assert(parsed && xiaozhi::text(parsed.get(), "text") == "你好\"\n\\小智");
  assert(xiaozhi::command("bad\"id", "abort").empty());
  auto wake_abort = xiaozhi::parse(xiaozhi::abort_command("session-1", true));
  assert(wake_abort && xiaozhi::text(wake_abort.get(), "type") == "abort" &&
         xiaozhi::text(wake_abort.get(), "reason") == "wake_word_detected");
  auto touch_abort = xiaozhi::parse(xiaozhi::abort_command("session-1"));
  assert(touch_abort && xiaozhi::text(touch_abort.get(), "reason").empty());
  assert(xiaozhi::abort_command("bad\"id", true).empty());
  assert(!xiaozhi::parse("{}garbage") && !xiaozhi::parse("{\"text\":") &&
         !xiaozhi::parse("[]"));
  assert(!xiaozhi::safe_header("token\r\nInjected: value", 100));
  assert(!xiaozhi::session_valid(std::string(128, 'x')));
  assert(xiaozhi::session_valid("aB_12-3"));
  puts("Xiaozhi protocol: hello, escaped Chinese text, malformed JSON and "
       "identifier/header bounds passed");
}
