// SPDX-License-Identifier: Apache-2.0
#include "../response_wait.hpp"
#include <cassert>
#include <cstdio>
int main() {
  xiaozhi::ResponseWait state;
  using Phase = xiaozhi::ResponseWait::Phase;
  assert(state.phase() == Phase::none && !state.expired(100000));
  state.think(12000); // A real STT result starts a reply deadline.
  assert(state.phase() == Phase::thinking && !state.expired(41999));
  assert(state.expired(42000));
  state.reset(); // TTS, cancellation, disconnect and background clear waiting.
  assert(state.phase() == Phase::none && !state.expired(999999));
  state.think(2000);
  state.reset();
  state.think(5000);
  assert(!state.expired(34999) && state.expired(35000));
  puts("Response wait: reply deadline, cancellation and fresh turns passed");
}
