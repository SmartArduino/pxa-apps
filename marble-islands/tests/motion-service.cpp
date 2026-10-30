// SPDX-License-Identifier: CC0-1.0
#include "../motion-service.hpp"
#include <assert.h>
#include <stdio.h>
#include <vector>
using namespace marble_motion;
struct Request {
  uint16_t service, opcode;
  uint64_t token;
  std::vector<uint8_t> payload;
};
static std::vector<Request> requests;
static std::vector<uint64_t> closed;
static unsigned blocked;
extern "C" int32_t pxa_call(uint32_t route, uint64_t token, const uint8_t *p,
                            uint32_t n) {
  uint16_t service = uint16_t(route), op = uint16_t(route >> 16);
  if (service == PXA_CORE_SERVICE && op == PXA_CORE_CLOSE_HANDLE) {
    if (blocked) {
      --blocked;
      return PXA_STATUS_WOULD_BLOCK;
    }
    closed.push_back(pxa_load_u64(p));
  }
  requests.push_back({service, op, token, {p, p + n}});
  return 0;
}
extern "C" int32_t pxa_io(uint64_t, uint32_t, uint8_t *, uint32_t) {
  return PXA_STATUS_WOULD_BLOCK;
}
static Request last() {
  for (auto i = requests.rbegin(); i != requests.rend(); ++i)
    if (i->token)
      return *i;
  assert(false);
  return {};
}
static void record(std::vector<uint8_t> &out, uint16_t tag, const void *p,
                   size_t n) {
  uint8_t data[512];
  size_t used;
  assert(pxa_wire_record_encode(data, sizeof(data), tag,
                                static_cast<const uint8_t *>(p), n, &used));
  out.insert(out.end(), data, data + used);
}
static void number(std::vector<uint8_t> &out, uint16_t tag, uint64_t n,
                   unsigned bytes) {
  uint8_t b[8];
  pxa_store_u64(b, n);
  record(out, tag, b, bytes);
}
static void descriptor(std::vector<uint8_t> &out, unsigned id, const char *name,
                       unsigned unit, unsigned dimensions = 3) {
  std::vector<uint8_t> body;
  number(body, 1, id, 2);
  record(body, 2, name, strlen(name));
  number(body, 3, unit, 2);
  number(body, 4, dimensions, 1);
  number(body, 5, 50, 4);
  number(body, 6, 3000, 4);
  record(out, 1, body.data(), body.size());
}
static void reply(Service &s, Device &d, Request request,
                  std::vector<uint8_t> p, bool foreground = true) {
  pxa_event_t e{};
  e.service = request.service;
  e.opcode = request.opcode;
  e.token = request.token;
  e.payload = p.data();
  e.payload_size = p.size();
  assert(s.event(e, foreground, d.now + 1000));
  s.update(foreground, d.now + 1000);
}
static void status(Service &s, Device &d, int code = 0, uint64_t handle = 0) {
  std::vector<uint8_t> b(handle ? 12 : 4);
  pxa_store_u32(b.data(), uint32_t(code));
  if (handle)
    pxa_store_u64(b.data() + 4, handle);
  reply(s, d, last(), b);
}
static constexpr uint64_t AP = UINT64_C(0x100000001),
                          GP = UINT64_C(0x100000002),
                          AS = UINT64_C(0x200000001),
                          GS = UINT64_C(0x200000002);
static void list(Service &s, Device &d, bool gyro = true,
                 bool acceleration = true) {
  auto r = last();
  assert(r.service == PXA_SENSOR_SERVICE && r.opcode == PXA_SENSOR_LIST);
  std::vector<uint8_t> b(4);
  if (acceleration)
    descriptor(b, 7, "motion.acceleration", 2);
  if (gyro)
    descriptor(b, 11, "motion.angular-velocity", 3);
  descriptor(b, 19, "ambient.temperature", 1, 1);
  reply(s, d, r, b);
}
static void connect(Service &s, Device &d, bool gyro = true,
                    bool deny_gyro = false) {
  d.request(true);
  s.update(true, 1000000);
  list(s, d, gyro);
  assert(d.status == Status::Permission);
  status(s, d, 0, AP);
  if (gyro)
    status(s, d, deny_gyro ? PXA_STATUS_DENIED : 0,
           deny_gyro ? 0 : GP);
  assert(last().service == PXA_SENSOR_SERVICE &&
         last().opcode == PXA_SENSOR_SUBSCRIBE);
  status(s, d, 0, AS);
  if (gyro && !deny_gyro)
    status(s, d, 0, GS);
  assert(d.status == Status::Streaming && d.gyro == (gyro && !deny_gyro));
}
static void sample(Service &s, Device &d, uint64_t handle, uint64_t time,
                   const int32_t *values, unsigned dim = 3) {
  std::vector<uint8_t> b;
  number(b, 4, handle, 8);
  number(b, 2, time, 8);
  number(b, 3, 1, 2);
  uint8_t v[12];
  for (unsigned i = 0; i < dim; ++i)
    pxa_store_u32(v + 4 * i, uint32_t(values[i]));
  record(b, 4, v, dim * 4);
  pxa_event_t e{};
  e.service = PXA_SENSOR_SERVICE;
  e.opcode = PXA_SENSOR_SAMPLE;
  e.payload = b.data();
  e.payload_size = b.size();
  assert(s.event(e, true, d.now));
}
static bool did_close(uint64_t h) {
  for (auto v : closed)
    if (h == v)
      return true;
  return false;
}
int main() {
  const int32_t flat[] = {0, 0, 9806650};
  {
    Device d;
    Service s(d);
    connect(s, d);
    for (int i = 0; i < 20; ++i) {
      d.now += 50000;
      sample(s, d, AS, d.now, flat);
    }
    assert(d.ready());
    uint64_t received = d.tilt.received;
    sample(s, d, GS, d.now + 5000000, flat);
    sample(s, d, AS, d.now + 50000, flat, 2);
    sample(s, d, UINT64_C(0x200000099), d.now + 50000, flat);
    assert(d.tilt.received == received);
    blocked = 2;
    s.update(false, d.now + 1000);
    assert(!d.ready() && !d.tilt.received);
    s.update(false, d.now + 1000);
    assert(did_close(AP) && did_close(GP) && did_close(AS) && did_close(GS));
  }
  {
    Device d;
    Service s(d);
    connect(s, d, true, true);
    assert(!d.gyro);
    s.update(false, d.now + 1000);
  }
  {
    Device d;
    Service s(d);
    connect(s, d, false);
    assert(!d.gyro);
    s.update(false, d.now + 1000);
  }
  {
    Device d;
    Service s(d);
    d.request(true);
    s.update(true, 1000000);
    list(s, d, false, false);
    assert(d.status == Status::Unavailable);
    size_t before = requests.size();
    s.update(true, d.now + 5000000);
    assert(requests.size() == before);
  }
  {
    Device d;
    Service s(d);
    d.request(true);
    s.update(true, 1000000);
    list(s, d);
    s.update(true, d.now + 10000000);
    assert(d.status == Status::Permission);
    status(s, d, PXA_STATUS_DENIED);
    assert(d.status == Status::Denied);
    size_t before = requests.size();
    s.update(false, d.now + 1000);
    s.update(true, d.now + 1000);
    assert(requests.size() == before);
    ++d.retry;
    s.update(true, d.now + 1000);
    assert(last().service == PXA_SENSOR_SERVICE);
  }
  // Late permission/subscription completions after pause are closed, not
  // revived.
  {
    Device d;
    Service s(d);
    d.request(true);
    s.update(true, 1000000);
    list(s, d, false);
    auto pending = last();
    s.update(false, d.now + 1000);
    std::vector<uint8_t> b(12);
    pxa_store_u64(b.data() + 4, AP);
    reply(s, d, pending, b, false);
    assert(did_close(AP) && !d.ready());
  }
  {
    Device d;
    Service s(d);
    d.request(true);
    s.update(true, 1000000);
    list(s, d, false);
    status(s, d, 0, AP);
    auto pending = last();
    s.update(false, d.now + 1000);
    std::vector<uint8_t> b(12);
    pxa_store_u64(b.data() + 4, AS);
    reply(s, d, pending, b, false);
    assert(did_close(AS) && !d.ready());
  }
  // Gyroscope revocation keeps gravity control; acceleration revocation stops
  // it.
  {
    Device d;
    Service s(d);
    connect(s, d);
    for (const char *scope :
         {"motion.angular-velocity", "motion.acceleration"}) {
      std::vector<uint8_t> b;
      record(b, 1, "sensor.read", 11);
      record(b, 2, scope, strlen(scope));
      pxa_event_t e{};
      e.service = PXA_PERMISSION_SERVICE;
      e.opcode = PXA_PERMISSION_REVOKED;
      e.payload = b.data();
      e.payload_size = b.size();
      assert(s.event(e, true, d.now + 1000));
      if (scope[7] == 'a' && strlen(scope) > 20)
        assert(!d.gyro);
    }
    assert(d.status == Status::Denied && !d.ready());
  }
  puts("PASS motion service: discovery, both permissions, gyro/gravity "
       "fallback, denial/retry, calibration, bad samples, close backpressure, "
       "pause and late replies, permission revocation");
}
