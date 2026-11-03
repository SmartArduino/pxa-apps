// SPDX-License-Identifier: MIT
#include "../save_codec.hpp"
#include "../core.hpp"
#include <cassert>
#include <cstdio>
#include <random>
#include <vector>

static void roundtrip(std::vector<uint8_t> source, bool expect_packed = false) {
  using namespace starsoil_save;
  std::memcpy(source.data(), "SSR1", 4);
  store32(source.data() + source.size() - 4, checksum(InitialHash, source.data(), source.size() - 4));
  size_t capacity = buffer_capacity(source.size());
  std::vector<uint8_t> buffer(capacity + 32, 0xa5);
  std::copy(source.begin(), source.end(), buffer.begin());
  size_t size = pack(buffer.data(), source.size(), capacity);
  if (expect_packed) assert(packed(buffer.data(), size));
  std::vector<uint8_t> encoded(buffer.begin(), buffer.begin() + size);
  assert(unpack(buffer.data(), size, capacity, source.size()));
  assert(size == source.size() && std::equal(source.begin(), source.end(), buffer.begin()));
  for (size_t i = capacity; i < buffer.size(); ++i) assert(buffer[i] == 0xa5);
  if (packed(encoded.data(), encoded.size())) {
    for (size_t cut = 4; cut < encoded.size(); cut += std::max(size_t(1), encoded.size() / 64)) {
      auto broken = encoded; broken.resize(capacity); size_t n = cut;
      assert(!unpack(broken.data(), n, capacity, source.size()));
    }
    auto bad = encoded; bad.resize(capacity); size_t n = encoded.size();
    store32(bad.data() + 4, uint32_t(source.size() + 1));
    assert(!unpack(bad.data(), n, capacity, source.size()));
  }
}
int main() {
  std::mt19937 rng(314159);
  for (size_t n : {size_t(128),size_t(129),size_t(130),size_t(131),size_t(256),size_t(4096),starsoil::SaveBytes}) {
    roundtrip(std::vector<uint8_t>(n, 0), n >= 256);
    std::vector<uint8_t> random(n);
    for (auto &v : random) v = uint8_t(rng());
    roundtrip(random);
    // Literal-heavy prefix/tail around a long run stresses both overlap directions.
    for (size_t edge : {size_t(1), n / 4, n / 2}) {
      auto mixed = random;
      std::fill(mixed.begin() + edge, mixed.end() - edge, 19);
      roundtrip(mixed);
    }
  }
  for (unsigned i = 0; i < 500; ++i) {
    std::vector<uint8_t> mixed(128 + rng() % 4096);
    for (size_t at = 0; at < mixed.size();) {
      size_t count = std::min(size_t(1 + rng() % 140), mixed.size() - at);
      uint8_t value = uint8_t(rng());
      for (size_t j = 0; j < count; ++j) mixed[at++] = i & 1 ? value : uint8_t(rng());
    }
    roundtrip(mixed);
  }
  static starsoil::State state;
  for (auto world : {starsoil::WorldSize::Small,starsoil::WorldSize::Medium,starsoil::WorldSize::Large,starsoil::WorldSize::Huge}) {
    starsoil::new_game(state, 20261103, world);
    std::vector<uint8_t> raw(starsoil::SaveBytes);
    size_t size = starsoil::encode(state, raw.data(), raw.size()); raw.resize(size);
    auto buffer = raw; buffer.resize(starsoil_save::buffer_capacity(size));
    size_t compressed = starsoil_save::pack(buffer.data(), size, buffer.size());
    printf("world %ux%u: %zu -> %zu bytes\n",state.width,state.height,size,compressed);
    roundtrip(raw, true);
  }
  puts("PASS compressed/legacy round trips, bounded overlap, short/invalid streams and four real world sizes");
}
