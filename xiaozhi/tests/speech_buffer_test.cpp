// SPDX-License-Identifier: Apache-2.0
#include "../speech_buffer.hpp"
#include <cassert>
#include <vector>
#include <cstdio>

int main() {
  using xiaozhi::SpeechBuffer;
  SpeechBuffer b;
  std::array<std::int16_t, 320> frame{};
  frame.fill(7);
  assert(!b.ready(1000, false));
  assert(b.push(frame, 1000));
  assert(!b.ready(1199, false) && b.ready(1200, false));
  b.reset();
  for (unsigned i = 0; i < 6; ++i) {
    assert(b.push(frame, 1000 + 20 * i));
    assert(b.ready(1000 + 20 * i, false) == (i == 5));
  }
  // Partial writes and repeated backpressure never consume or reorder data.
  b.reset();
  std::vector<std::int16_t> expected, actual;
  for (unsigned round = 0; round < 50; ++round) {
    for (unsigned i = 0; i < frame.size(); ++i) frame[i] = std::int16_t(round * 320 + i);
    assert(b.push(frame, round * 20));
    expected.insert(expected.end(), frame.begin(), frame.end());
    auto before = b.size(); assert(b.front().front() == expected[actual.size()]);
    assert(b.size() == before); // would-block: caller does not consume.
    auto chunk = b.front(); unsigned n = std::min(123u, unsigned(chunk.size()));
    actual.insert(actual.end(), chunk.begin(), chunk.begin() + n); assert(b.consume(n));
    while (b.available() < 1920) {
      chunk = b.front(); actual.insert(actual.end(), chunk.begin(), chunk.end());
      assert(b.consume(unsigned(chunk.size())));
    }
  }
  while (!b.empty()) {
    auto chunk = b.front(); actual.insert(actual.end(), chunk.begin(), chunk.end());
    assert(b.consume(unsigned(chunk.size())));
  }
  assert(actual == expected && b.available() == SpeechBuffer::Capacity);
  assert(!b.consume(1));
  b.reset(); assert(b.push(frame, 100)); assert(b.ready(100, true)); // short reply tail.
  b.reset(); assert(b.empty() && !b.ready(10000, true)); // abort/cancel clears all state.
  std::vector<std::int16_t> full(SpeechBuffer::Capacity, 3);
  assert(b.push(full, 0)); assert(!b.push(frame, 0) && b.size() == full.size());
  puts("PASS: bounded prebuffer, startup deadline, partial writes, backpressure, ring wrap, exact sample order, short tails and cancellation");
}
