// SPDX-License-Identifier: MIT
#include "../model.hpp"
#include "../sensors.hpp"
#include <cassert>
#include <deque>
#include <set>
#include <vector>
struct Call {
  uint16_t service, opcode;
  uint64_t token;
  std::vector<uint8_t> payload;
};
static std::deque<Call> calls;
static std::set<uint64_t> closed;
static bool block_close = false, block_submit = false;
static unsigned cancels = 0;
extern "C" int32_t pxa_call(uint32_t route, uint64_t token, const uint8_t *data,
                            uint32_t size) {
  uint16_t service = route & 65535, op = route >> 16;
  if (service == PXA_CORE_SERVICE && op == PXA_CORE_CLOSE_HANDLE) {
    if (block_close)
      return PXA_STATUS_WOULD_BLOCK;
    assert(closed.insert(pxa_load_u64(data)).second);
    return 0;
  }
  if (service == PXA_CORE_SERVICE && op == PXA_CORE_CANCEL_REQUEST) {
    ++cancels;
    return 0;
  }
  if (service == PXA_CLOCK_SERVICE || service == PXA_LOG_SERVICE)
    return 0;
  if (block_submit)
    return PXA_STATUS_WOULD_BLOCK;
  calls.push_back({service, op, token, {data, data + size}});
  return 0;
}
using Bytes = std::vector<uint8_t>;
static Bytes integer(uint64_t value, unsigned size) {
  Bytes b(size);
  for (unsigned i = 0; i < size; ++i)
    b[i] = value >> (8 * i);
  return b;
}
static void record(Bytes &b, unsigned tag, const Bytes &value) {
  uint8_t raw[256];
  size_t n = 0;
  assert(pxa_wire_record_encode(raw, sizeof(raw), tag, value.data(),
                                value.size(), &n));
  b.insert(b.end(), raw, raw + n);
}
static void descriptor(Bytes &b, unsigned id, const char *name, unsigned unit) {
  Bytes d;
  record(d, 1, integer(id, 2));
  record(d, 2, Bytes(name, name + strlen(name)));
  record(d, 3, integer(unit, 2));
  record(d, 4, integer(3, 1));
  record(d, 5, integer(100, 4));
  record(d, 6, integer(1000, 4));
  record(b, 1, d);
}
struct Fixture {
  compass::Model d;
  compass_sensor::Service<compass::Model> s{d, "Compass", "ready", 100};
  uint64_t now = 1000000;
  Fixture() {
    calls.clear();
    closed.clear();
    block_close = block_submit = false;
    cancels = 0;
  }
  void update(bool fg = true) { s.update(fg, now += 1000); }
  void reply(Bytes b, bool fg = true) {
    assert(!calls.empty());
    auto c = calls.front();
    calls.pop_front();
    pxa_event_t e{c.service, c.opcode, c.token, b.data(), uint32_t(b.size())};
    assert(s.event(e, fg, now += 1000));
  }
  void list(bool magnetic = true, bool gravity = true) {
    update();
    assert(calls.front().opcode == PXA_SENSOR_LIST);
    Bytes b(4);
    if (magnetic)
      descriptor(b, 4, "magnetic-field.0", PXA_SENSOR_UNIT_NANO_TESLA);
    if (gravity)
      descriptor(b, 1, "motion.acceleration",
                 PXA_SENSOR_UNIT_MICRO_METERS_PER_SECOND_SQUARED);
    reply(b);
  }
  void grant(uint64_t h) {
    update();
    Bytes b(4);
    auto v = integer(h, 8);
    b.insert(b.end(), v.begin(), v.end());
    reply(b);
  }
  void connect() {
    list();
    grant(0x100000001);
    grant(0x100000002);
    grant(0x200000001);
    grant(0x200000002);
    assert(d.status == compass::Status::Streaming && d.gyro);
  }
};
int main() {
  {
    Fixture f;
    f.connect();
    block_close = true;
    f.update(false);
    assert(!f.d.gyro && !f.d.tilt.calibrated);
    f.update();
    assert(calls.empty());
    block_close = false;
    f.update();
    assert(closed.size() == 4 && calls.size() == 1);
  }
  {
    Fixture f;
    f.list(false);
    assert(f.d.status == compass::Status::Unavailable && calls.empty());
  }
  {
    Fixture f;
    f.list(true, false);
    assert(f.d.status == compass::Status::NoGravity && calls.empty());
  }
  {
    Fixture f;
    f.list();
    f.update();
    f.reply(integer(PXA_STATUS_DENIED, 4));
    assert(f.d.status == compass::Status::Denied);
    f.update();
    assert(calls.empty());
    ++f.d.retry;
    f.update();
    assert(calls.size() == 1);
  }
  {
    Fixture f;
    f.list();
    f.update();
    assert(calls.front().service == PXA_PERMISSION_SERVICE);
    f.update(false);
    assert(cancels == 1);
    Bytes b(4);
    auto h = integer(0x300000001, 8);
    b.insert(b.end(), h.begin(), h.end());
    f.reply(b, false);
    assert(closed.count(0x300000001));
    f.update();
    assert(calls.size() == 1);
  }
  {
    Fixture f;
    f.connect();
    Bytes b;
    record(b, 1, Bytes{'s', 'e', 'n', 's', 'o', 'r', '.', 'r', 'e', 'a', 'd'});
    const char *scope = "motion.acceleration";
    record(b, 2, Bytes(scope, scope + strlen(scope)));
    pxa_event_t e{PXA_PERMISSION_SERVICE, PXA_PERMISSION_REVOKED, 0, b.data(),
                  uint32_t(b.size())};
    assert(f.s.event(e, true, f.now));
    assert(f.d.status == compass::Status::Denied && !f.d.gyro &&
           closed.size() == 4);
  }
  {
    Fixture f;
    block_submit = true;
    f.update();
    assert(calls.empty());
    block_submit = false;
    f.update();
    assert(calls.size() == 1);
    f.now += 2100000;
    f.update();
    assert(f.d.status == compass::Status::Error);
    f.update();
    assert(cancels == 1);
  }
  puts("Compass service: discovery, missing channels, permission "
       "denial/retry/revocation, cancellation/late completion, close "
       "backpressure, background release and request timeout passed");
}
