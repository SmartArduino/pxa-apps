// SPDX-License-Identifier: MIT
#include "save_store.hpp"
#include <algorithm>
#include <stdio.h>
#include <string.h>
namespace save_store {
Journal journal;
static uint32_t hash(const uint8_t *p, size_t n) {
  uint32_t h = 2166136261u;
  for (size_t i = 0; i < n; i++)
    h = (h ^ p[i]) * 16777619u;
  return h;
}
static uint32_t get32(const uint8_t *p) {
  return uint32_t(p[0]) | (uint32_t(p[1]) << 8) | (uint32_t(p[2]) << 16) |
         (uint32_t(p[3]) << 24);
}
static void put32(uint8_t *p, uint32_t n) {
  for (int i = 0; i < 4; i++)
    p[i] = uint8_t(n >> (8 * i));
}
int Journal::request() {
  char key[40];
  bool writing = phase_ == WriteParts || phase_ == WriteHead;
  if (phase_ == Legacy)
    return submit_(false, "pxa-unciv.save.v1", nullptr, 0, TOKEN);
  if (phase_ == HeadA || phase_ == HeadB) {
    snprintf(key, sizeof(key), "unciv.save.%c", phase_ == HeadA ? 'a' : 'b');
    return submit_(false, key, nullptr, 0, TOKEN);
  }
  if (phase_ == WriteHead) {
    snprintf(key, sizeof(key), "unciv.save.%c", 'a' + slot_);
    return submit_(true, key, encoded_, sizeof(encoded_), TOKEN);
  }
  snprintf(key, sizeof(key), "unciv.save.%c.%d", 'a' + slot_, part_);
  size_t count = std::min(CHUNK, size_t(heads_[slot_].size) - part_ * CHUNK);
  return submit_(writing, key, writing ? data_ + part_ * CHUNK : nullptr,
                 writing ? count : 0, TOKEN);
}
void Journal::finish(int status) {
  bool writing = phase_ == WriteHead || phase_ == WriteParts;
  phase_ = Idle;
  deliver_(writing, outer_, status, status ? nullptr : data_,
           status ? 0 : heads_[slot_].size);
}
int Journal::read(uint64_t token) {
  if (phase_ != Idle)
    return -6;
  outer_ = token;
  active_ = -1;
  heads_[0] = heads_[1] = Head{};
  phase_ = HeadA;
  int status = request();
  if (status)
    phase_ = Idle;
  return status;
}
void Journal::choose() {
  slot_ = heads_[1].valid && (!heads_[0].valid ||
                              heads_[1].generation > heads_[0].generation)
              ? 1
              : 0;
  fallback_ = false;
  part_ = 0;
  phase_ = heads_[slot_].valid ? ReadParts : Legacy;
  int status = request();
  if (status)
    finish(status);
}
void Journal::try_other(int status) {
  heads_[slot_].valid = false;
  int other = 1 - slot_;
  if (!fallback_ && heads_[other].valid) {
    fallback_ = true;
    slot_ = other;
    part_ = 0;
    phase_ = ReadParts;
    int error = request();
    if (error)
      finish(error);
  } else
    finish(status);
}
int Journal::write(uint64_t token, const uint8_t *data, size_t size) {
  if (phase_ != Idle)
    return -6;
  if (!data || !size || size > CAPACITY)
    return -1;
  uint32_t generation = std::max(heads_[0].generation, heads_[1].generation);
  if (generation == UINT32_MAX)
    return -9;
  slot_ = active_ == 0 ? 1 : 0;
  Head h{generation + 1, hash(data, size), uint16_t(size), true};
  heads_[slot_] = h;
  memcpy(data_, data, size);
  memcpy(encoded_, "UCJ1", 4);
  put32(encoded_ + 4, h.generation);
  encoded_[8] = uint8_t(size);
  encoded_[9] = uint8_t(size >> 8);
  encoded_[10] = uint8_t((size + CHUNK - 1) / CHUNK);
  encoded_[11] = uint8_t(slot_);
  put32(encoded_ + 12, h.hash);
  put32(encoded_ + 16, hash(encoded_, 16));
  outer_ = token;
  part_ = 0;
  phase_ = WriteParts;
  int status = request();
  if (status) {
    heads_[slot_].valid = false;
    phase_ = Idle;
  }
  return status;
}
bool Journal::event(bool writing, uint64_t token, int status,
                    const uint8_t *bytes, size_t size) {
  if (token != TOKEN)
    return false;
  if (phase_ == Idle)
    return true;
  bool expected = phase_ == WriteParts || phase_ == WriteHead;
  if (writing != expected) {
    finish(-15);
    return true;
  }
  if (phase_ == HeadA || phase_ == HeadB) {
    int which = phase_ == HeadA ? 0 : 1;
    auto &h = heads_[which];
    if (!status && size == 20 && !memcmp(bytes, "UCJ1", 4) &&
        get32(bytes + 16) == hash(bytes, 16)) {
      h.generation = get32(bytes + 4);
      h.size = uint16_t(bytes[8] | (bytes[9] << 8));
      h.hash = get32(bytes + 12);
      h.valid = h.generation && h.size > 0 && h.size <= CAPACITY &&
                bytes[10] == (h.size + CHUNK - 1) / CHUNK && bytes[11] == which;
    }
    if (phase_ == HeadA) {
      phase_ = HeadB;
      int error = request();
      if (error)
        finish(error);
    } else
      choose();
    return true;
  }
  if (phase_ == Legacy) {
    if (!status && size <= CAPACITY && size) {
      memcpy(data_, bytes, size);
      heads_[slot_].size = size;
    } else if (!status)
      status = -15;
    finish(status);
    return true;
  }
  if (phase_ == ReadParts) {
    size_t count = std::min(CHUNK, size_t(heads_[slot_].size) - part_ * CHUNK);
    if (status || !bytes || size != count) {
      try_other(status ? status : -15);
      return true;
    }
    memcpy(data_ + part_ * CHUNK, bytes, size);
    part_++;
    if (size_t(part_) * CHUNK >= heads_[slot_].size) {
      if (hash(data_, heads_[slot_].size) != heads_[slot_].hash) {
        try_other(-15);
        return true;
      }
      active_ = slot_;
      finish(0);
    } else {
      int error = request();
      if (error)
        try_other(error);
    }
    return true;
  }
  if (status) {
    heads_[slot_].valid = false;
    finish(status);
    return true;
  }
  if (phase_ == WriteParts) {
    part_++;
    if (size_t(part_) * CHUNK >= heads_[slot_].size)
      phase_ = WriteHead;
    int error = request();
    if (error) {
      heads_[slot_].valid = false;
      finish(error);
    }
  } else {
    active_ = slot_;
    finish(0);
  }
  return true;
}
} // namespace save_store
