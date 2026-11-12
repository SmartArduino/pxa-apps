// SPDX-License-Identifier: MIT
#pragma once
#include <stddef.h>
#include <stdint.h>
namespace save_store {
constexpr size_t CAPACITY = 2048, CHUNK = 800;
constexpr uint64_t TOKEN = 0x55430001;
using Submit = int (*)(bool, const char *, const uint8_t *, size_t, uint64_t);
using Deliver = void (*)(bool, uint64_t, int, const uint8_t *, size_t);
// Each generation commits its descriptor only after every chunk succeeds.
// On startup both descriptors are read, and checksums select the latest intact
// save.
class Journal {
  struct Head {
    uint32_t generation = 0, hash = 0;
    uint16_t size = 0;
    bool valid = false;
  };
  enum Phase { Idle, HeadA, HeadB, ReadParts, Legacy, WriteParts, WriteHead };
  Head heads_[2]{};
  uint8_t data_[CAPACITY]{}, encoded_[20]{};
  Phase phase_ = Idle;
  int slot_ = 0, active_ = -1, part_ = 0;
  uint64_t outer_ = 0;
  Submit submit_ = nullptr;
  Deliver deliver_ = nullptr;
  bool fallback_ = false;
  int request();
  void finish(int status);
  void choose();
  void try_other(int status);

public:
  void bind(Submit submit, Deliver deliver) {
    submit_ = submit;
    deliver_ = deliver;
  }
  int read(uint64_t token);
  int write(uint64_t token, const uint8_t *data, size_t size);
  bool event(bool writing, uint64_t token, int status, const uint8_t *data,
             size_t size);
};
extern Journal journal;
} // namespace save_store
