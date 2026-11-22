// SPDX-License-Identifier: MIT
#include "../save_store.hpp"
#include <assert.h>
#include <deque>
#include <map>
#include <stdio.h>
#include <string>
#include <vector>
using Bytes = std::vector<uint8_t>;
struct Request {
  bool write;
  std::string key;
  Bytes value;
  uint64_t token;
};
std::map<std::string, Bytes> disk;
std::deque<Request> requests;
save_store::Journal engine;
bool delivered = false, was_write = false;
int delivered_status = 0;
Bytes delivered_value;
int submit(bool write, const char *key, const uint8_t *data, size_t n,
           uint64_t token) {
  assert(n <= 960);
  requests.push_back(
      {write, key, write ? Bytes(data, data + n) : Bytes{}, token});
  return 0;
}
void deliver(bool writing, uint64_t token, int status, const uint8_t *p,
             size_t n) {
  assert(token == 1 || token == 2);
  delivered = true;
  was_write = writing;
  delivered_status = status;
  delivered_value = status || !p ? Bytes{} : Bytes(p, p + n);
}
void reset() {
  requests.clear();
  engine = save_store::Journal{};
  engine.bind(submit, deliver);
  delivered = false;
}
void step(int failure = 0) {
  assert(!requests.empty());
  auto q = requests.front();
  requests.pop_front();
  if (q.write) {
    if (!failure)
      disk[q.key] = q.value;
    engine.event(true, q.token, failure, nullptr, 0);
  } else {
    auto i = disk.find(q.key);
    int status = failure ? failure : i == disk.end() ? -5 : 0;
    engine.event(false, q.token, status, status ? nullptr : i->second.data(),
                 status ? 0 : i->second.size());
  }
}
void drain() {
  for (int n = 0; !requests.empty(); n++) {
    assert(n < 30);
    step();
  }
}
Bytes load() {
  reset();
  assert(engine.read(1) == 0);
  drain();
  assert(delivered && !was_write && delivered_status == 0);
  return delivered_value;
}
void write(const Bytes &b) {
  delivered = false;
  assert(engine.write(2, b.data(), b.size()) == 0);
  drain();
  assert(delivered && was_write && delivered_status == 0);
}
int main() {
  Bytes a(1910, 0x31), b(1478, 0x72), c(1030, 0x19);
  reset();
  assert(engine.read(1) == 0);
  drain();
  assert(delivered_status == -5);
  write(a);
  assert(load() == a);
  auto original = disk;
  // Stop after each possible durable write in a new generation, then reboot.
  for (int cut = 0; cut <= 4; cut++) {
    disk = original;
    assert(load() == a);
    delivered = false;
    assert(engine.write(2, b.data(), b.size()) == 0);
    for (int i = 0; i < cut && !requests.empty(); i++)
      step();
    auto result = load();
    assert(result == (cut >= 3 ? b : a));
  }
  disk = original;
  assert(load() == a);
  write(b);
  auto two = disk;
  for (int cut = 0; cut <= 4; cut++) {
    disk = two;
    assert(load() == b);
    assert(engine.write(2, c.data(), c.size()) == 0);
    for (int i = 0; i < cut && !requests.empty(); i++)
      step();
    auto result = load();
    assert(result == (cut >= 3 ? c : b));
  }
  disk = two;
  disk["unciv.save.b.0"][6] ^= 1;
  assert(load() == a);
  disk = two;
  disk["unciv.save.b"][4] ^= 0x80;
  assert(load() == a);
  disk = two;
  disk["unciv.save.b.1"].resize(2);
  assert(load() == a);
  disk = two;
  assert(load() == b);
  delivered = false;
  assert(engine.write(2, c.data(), c.size()) == 0);
  step(-9);
  assert(delivered && was_write && delivered_status == -9);
  assert(load() == b);
  disk.clear();
  disk["pxa-unciv.save.v1"] = c;
  assert(load() == c);
  write(a);
  assert(load() == a);
  disk = two;
  assert(load() == b);
  assert(engine.write(2, a.data(), a.size()) == 0);
  assert(engine.write(2, b.data(), b.size()) == -6);
  drain();
  assert(load() == a);
  puts("KV journal: chunk limits, power loss, corruption, fallback, legacy and "
       "write failure passed");
}
