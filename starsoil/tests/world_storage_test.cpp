// SPDX-License-Identifier: MIT
// Drive the production dispatcher with deliberately delayed filesystem replies.
// The simulator separately exercises the real FS and complete world codec.
#include "../runtime.hpp"
#include "../save_checksum.hpp"
#include "../save_codec.hpp"
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
bool compressible_fixture;
size_t snapshot_size = 5, write_limit = 2048;
int write_calls, fail_write;
std::vector<uint8_t> written;
int created = 0, deleted = 0;
uint8_t world_buffer[starsoil_save::buffer_capacity(SAVE_CAPACITY)], packet[1024];
auto &loaded = world_buffer;
auto &pending_save = world_buffer;
size_t loaded_size;
bool load_ready, running = true, save_dirty, save_pending, exit_requested;
uint64_t save_retry, save_due, repeats[4];
uint32_t buttons, blocked_buttons;
uint64_t simulated_now = 5000000;
uint64_t now_us() { return simulated_now; }
int32_t pxa_fs_request_open(uint64_t token, const char *path, uint32_t, int) {
  if (token == SAVE) { written.clear(); write_calls = 0; }
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
int32_t pxa_fs_write(uint64_t, uint8_t *bytes, uint32_t size) {
  ++write_calls;simulated_now += 8000;
  if (fail_write && write_calls == fail_write) return -1;
  size_t count = std::min(size_t(size), write_limit);
  written.insert(written.end(),bytes,bytes+count);
  return int32_t(count);
}
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
void finish_write() {
  while (saving_handle) { simulated_now += 20000; service_save(); }
}
void finish_save() { reply(SAVE); finish_write(); reply(SAVE_COMMIT); }
} // namespace

namespace pocket {
void redraw() {}
void save() { save_dirty = true; save_due = 0; }
} // namespace pocket
namespace game {
size_t serialize(uint8_t *out, size_t, bool) {
  std::fill(out,out+snapshot_size,uint8_t(19));out[0] = current_seed;
  if (compressible_fixture) std::memcpy(out,"SSR1",4);
  return snapshot_size;
}
void describe_world(pocket::WorldInfo &info) { info = {}; info.seed = current_seed; info.occupied = info.valid = true; }
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

  count = requests.size();
  simulated_now = save_retry - 1;send_save();
  assert(requests.size() == count && !save_pending && save_dirty);
  simulated_now = save_retry;send_save();
  assert(save_pending && pending_save[0] == 42);
  reply(SAVE);finish_write();reply(SAVE_COMMIT, -99); // e.g. an external reader locks rename.
  assert(persist_status < 0 && save_dirty && !save_pending && world_catalog[1].seed == 42);
  current_seed = 43;
  simulated_now = save_retry;send_save();
  assert(save_pending && pending_save[0] == 43);
  finish_save();
  assert(persist_status == 2 && !save_dirty && !save_pending && world_catalog[1].seed == 43);
  puts("PASS open/replace errors retain the last commit, obey retry delay, resnapshot the latest state and clear the error on successful retry");

  snapshot_size = SAVE_CAPACITY;write_limit = 137;
  current_seed = 71;save_dirty = true;save_due = save_retry = 0;
  send_save();reply(SAVE);
  assert(write_calls == 0); // The open callback must not drain the file.
  current_seed = 72; // Later game edits cannot change the immutable snapshot.
  uint64_t max_step = 0;unsigned steps = 0;
  while (saving_handle) {
    simulated_now += 20000;
    int calls_before = write_calls;uint64_t began = simulated_now;
    service_save();max_step=std::max(max_step,simulated_now-began);++steps;
    assert(write_calls-calls_before <= 1);
  }
  assert(written.size()==snapshot_size && written[0]==71 && max_step==8000 && steps>250);
  uint32_t hash=2166136261u;
  for(size_t i=0;i<written.size()-4;++i)hash=(hash^written[i])*16777619u;
  for(unsigned i=0;i<4;++i)assert(written[written.size()-4+i]==uint8_t(hash>>(8*i)));
  reply(SAVE_COMMIT);assert(world_catalog[active_world].seed==71);
  printf("PASS %zu-byte save: short writes, immutable bytes and incremental checksum; emulated 8ms filesystem calls, maximum save step %llu us across %u steps\n",
    snapshot_size,(unsigned long long)max_step,steps);
  save_dirty=true;save_due=0;fail_write=2;send_save();reply(SAVE);
  simulated_now+=20000;service_save();simulated_now+=20000;service_save();
  assert(!saving_handle && !save_pending && save_dirty && persist_status<0 &&
    world_catalog[active_world].seed==71);
  puts("PASS interrupted write closes its handle, keeps the committed catalogue and schedules retry");

  compressible_fixture=true;save_retry=save_due=0;save_dirty=true;fail_write=0;
  send_save();assert(saving_packed && pending_size<10000);
  finish_save();assert(starsoil_save::packed(written.data(),written.size()));
  disk_read=written;read_cursor=0;
  assert(read_world_file({0,1}) && loaded_size==snapshot_size);
  assert(std::memcmp(loaded,"SSR1",4)==0 && loaded[4]==19);
  assert(starsoil_save::load32(loaded+loaded_size-4)==
      starsoil_save::checksum(starsoil_save::InitialHash,loaded,loaded_size-4));
  puts("PASS production dispatcher compresses, preserves short-write snapshots and expands a checksummed save on load");
}
