// SPDX-License-Identifier: MIT
// Drive the production dispatcher with deliberately delayed filesystem replies.
// The simulator separately exercises the real FS and complete world codec.
#include "../runtime.hpp"
#include <algorithm>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace {
using namespace pocket;
constexpr int PXA_FS_SERVICE = 5, PXA_FS_OPEN_READ = 1,
              PXA_FS_OPEN_WRITE = 2, PXA_FS_OPEN_CREATE = 4,
              PXA_FS_OPEN_TRUNCATE = 16, PXA_FS_REMOVE = 3,
              PXA_FS_REPLACE = 8, PXA_STATUS_NOT_FOUND = -4,
              PXA_STATUS_INTERNAL = -1, PXA_EVENT_HANDLED = 0,
              PXA_EVENT_UNHANDLED = 1;
enum { LOAD = 1, SAVE, SAVE_COMMIT, WORLD_OPEN, WORLD_REMOVE, WORLD_REMOVE_TEMP };
struct pxa_event_t { int service = PXA_FS_SERVICE; uint64_t token; int32_t status = 0; };
struct pxa_fs_open_result_t { int32_t status; uint64_t handle; };
struct Request { uint64_t token; std::string path; };
std::vector<Request> requests;
std::vector<uint8_t> disk_read;
size_t read_cursor;
uint8_t current_seed = 7;
int created = 0, deleted = 0;
uint8_t loaded[SAVE_CAPACITY], pending_save[SAVE_CAPACITY], packet[1024];
size_t loaded_size;
bool load_ready, running = true, save_dirty, save_pending, exit_requested;
uint64_t save_retry, save_due, repeats[4];
uint32_t buttons, blocked_buttons;
uint64_t now_us() { return 5000000; }
int32_t pxa_fs_request_open(uint64_t token, const char *path, uint32_t, int) {
  requests.push_back({token, path}); return 0;
}
int32_t pxa_fs_request_path(int, uint64_t token, const char *path, uint32_t) {
  requests.push_back({token, path}); return 0;
}
int32_t pxa_fs_read(uint64_t, uint8_t *out, uint32_t capacity) {
  size_t n = std::min(size_t(capacity), disk_read.size() - read_cursor);
  std::copy_n(disk_read.data() + read_cursor, n, out);
  read_cursor += n; return int32_t(n);
}
int32_t pxa_fs_write(uint64_t, uint8_t *, uint32_t size) { return int32_t(size); }
int32_t pxa_fs_close(uint64_t) { return 0; }
bool pxa_fs_parse_open(const pxa_event_t *e, uint64_t, pxa_fs_open_result_t *r) {
  *r = {e->status, 1}; return true;
}
bool pxa_fs_parse_status(const pxa_event_t *e, uint64_t, int, int32_t *status) {
  *status = e->status; return true;
}
bool pxa_fs_build_replace(uint8_t *, size_t, uint64_t, const char *, size_t,
                           const char *, size_t, uint32_t *written) {
  *written = 1; return true;
}
int32_t pxa_control_packet(uint8_t *, uint32_t) { return 0; }
void pxa_log_write(int, const char *) {}
#include "../world_storage.inc"
void reply(uint64_t token, int32_t status = 0) {
  pxa_event_t event{PXA_FS_SERVICE, token, status};
  assert(handle_world_storage(event) == PXA_EVENT_HANDLED);
}
void finish_save() { reply(SAVE); reply(SAVE_COMMIT); }
} // namespace

namespace pocket {
void redraw() {}
void save() { save_dirty = true; save_due = 0; }
} // namespace pocket
namespace game {
size_t serialize(uint8_t *out, size_t) { out[0] = current_seed; return 1; }
bool inspect_world(const uint8_t *data, size_t n, pocket::WorldInfo &info) {
  if (!n) return false;
  info.seed = data[0]; info.valid = true; return true;
}
bool load(const uint8_t *data, size_t n) {
  if (!n) return false;
  current_seed = data[0]; return true;
}
void create_world(uint32_t seed, uint8_t) { current_seed = uint8_t(seed); ++created; }
void world_deleted() { ++deleted; current_seed = 0; }
} // namespace game

int main() {
  active_world = 0;
  world_catalog[0].occupied = world_catalog[1].occupied = true;
  requested_world = 1;
  world_operation = WorldOperation::Open;
  save_dirty = true;
  service_worlds();
  assert(save_pending && current_seed == 7 && active_world == 0);
  assert(requests.back().path == "starsoil-world-v1.tmp");
  service_worlds();
  assert(!opening_world && pending_save[0] == 7);
  finish_save();
  service_worlds();
  assert(opening_world && requests.back().token == WORLD_OPEN);
  size_t count = requests.size();
  pocket::save(); send_save(true); // Pause/background arrives during open.
  assert(!save_pending && !save_dirty && requests.size() == count);
  disk_read = {9}; read_cursor = 0;
  reply(WORLD_OPEN);
  assert(current_seed == 9 && active_world == 1 && !opening_world);
  assert(world_catalog[0].seed == 7 && world_catalog[1].seed == 9);
  puts("PASS delayed world load waits for old snapshot; lifecycle save cannot cross the transition");

  requested_world = 1;
  world_operation = WorldOperation::Remove;
  service_worlds();
  assert(removing_world && requests.back().path == "starsoil-world-2.bin");
  count = requests.size();
  pocket::save(); send_save(true);
  assert(!save_pending && requests.size() == count);
  reply(WORLD_REMOVE);
  assert(requests.back().path == "starsoil-world-2.tmp");
  count = requests.size();
  pocket::save(); send_save(true); // Another lifecycle save during cleanup.
  assert(requests.size() == count && !save_pending);
  reply(WORLD_REMOVE_TEMP, PXA_STATUS_NOT_FOUND);
  assert(active_world == -1 && deleted == 1 && !world_catalog[1].occupied);
  pocket::save(); send_save(true);
  assert(requests.size() == count && !save_dirty);
  puts("PASS active-world delete and temp cleanup suppress lifecycle saves; removed world cannot resurrect");

  active_world = 0; current_seed = 7;
  requested_world = 1; requested_seed = 42; requested_size = 0;
  world_operation = WorldOperation::Create;
  save_dirty = true;
  service_worlds();
  assert(save_pending && pending_save[0] == 7 && created == 0);
  service_worlds();
  assert(current_seed == 7 && created == 0);
  finish_save(); service_worlds();
  assert(created == 1 && current_seed == 42 && active_world == 1);
  assert(save_pending && pending_save[0] == 42);
  finish_save();
  assert(world_catalog[0].seed == 7 && world_catalog[1].seed == 42);
  puts("PASS queued creation cannot replace pending old-world bytes or its save path");

  requested_world = 0; world_operation = WorldOperation::Open;
  save_dirty = true;
  service_worlds();
  reply(SAVE, -99);
  service_worlds();
  assert(worlds_error == 2 && world_operation == WorldOperation::None);
  assert(current_seed == 42 && active_world == 1 && save_dirty);
  puts("PASS save failure cancels switching, retains active state and keeps retry data");
}
