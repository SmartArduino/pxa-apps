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
              PXA_STATUS_INTERNAL = -1, PXA_STATUS_UNSUPPORTED = -3, PXA_EVENT_HANDLED = 0,
              PXA_EVENT_UNHANDLED = 1;
enum { LOAD = 1, SAVE, SAVE_COMMIT, WORLD_OPEN, WORLD_REMOVE, WORLD_REMOVE_TEMP, SAVE_ATOMIC_BEGIN, SAVE_ATOMIC_WRITE, SAVE_ATOMIC_COMMIT, SAVE_ATOMIC_ABORT };
constexpr int PXA_FS_ATOMIC_BEGIN=20,PXA_FS_ATOMIC_WRITE=21,PXA_FS_ATOMIC_COMMIT=22,PXA_FS_ATOMIC_ABORT=23,PXA_FS_ATOMIC_MAX_CHUNK=2048;
struct pxa_fs_atomic_result_t { int32_t status; uint64_t session; uint32_t written; };
bool async_host;int built_opcode;std::vector<uint8_t> async_bytes;
uint64_t async_session=0x100000001;uint32_t async_written;
struct pxa_event_t { int service = PXA_FS_SERVICE; uint64_t token; int32_t status = 0; };
struct pxa_fs_open_result_t { int32_t status; uint64_t handle; };
struct Request { uint64_t token; std::string path; };
std::vector<Request> requests;
std::vector<uint8_t> disk_read;
size_t read_cursor;
uint8_t current_seed = 7;
bool compressible_fixture;
bool input_busy;
int close_calls, commit_calls;
bool fail_close;
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
uint64_t simulated_now = 5000000, frame_id;
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
int32_t pxa_fs_close(uint64_t) { ++close_calls; simulated_now += 900000; return fail_close ? -1 : 0; }
bool pxa_fs_parse_open(const pxa_event_t *e, uint64_t, pxa_fs_open_result_t *r) {
  *r = {e->status, 1}; return true;
}
bool pxa_fs_parse_status(const pxa_event_t *e, uint64_t, int, int32_t *status) {
  *status = e->status; return true;
}
bool pxa_fs_build_replace(uint8_t *, size_t, uint64_t, const char *, size_t,
                           const char *, size_t, uint32_t *written) {
  built_opcode=PXA_FS_REPLACE;*written = 1; return true;
}
bool pxa_fs_build_atomic_begin(uint8_t *,size_t,uint64_t,const char *,size_t,const char *,size_t,uint64_t,uint32_t *n) {built_opcode=PXA_FS_ATOMIC_BEGIN;*n=1;return true;}
bool pxa_fs_build_atomic_operation(uint8_t *,size_t,uint16_t op,uint64_t,uint64_t,const uint8_t *data,size_t n,uint32_t *out) {
  built_opcode=op;*out=1;if(op==PXA_FS_ATOMIC_WRITE)async_bytes.insert(async_bytes.end(),data,data+n);return true;
}
bool pxa_fs_parse_atomic(const pxa_event_t *e,uint64_t,uint16_t,pxa_fs_atomic_result_t *r) { *r={e->status,async_session,async_written};return true; }
int32_t pxa_control_packet(uint8_t *, uint32_t) { if(built_opcode>=20)return async_host?0:PXA_STATUS_UNSUPPORTED;
  ++commit_calls; simulated_now += 400000; return 0; }
void pxa_log_write(int, const char *) {}
#include "../world_storage.inc"
void reply(uint64_t token, int32_t status = 0) {
  if(token==SAVE || token==SAVE_ATOMIC_BEGIN)while(saving_packing)service_save(true);
  pxa_event_t event{PXA_FS_SERVICE, token, status};
  assert(handle_world_storage(event) == PXA_EVENT_HANDLED);
}
void finish_write() {
  while (saving_handle || saving_commit_ready) { simulated_now += 20000; service_save(); }
}
void finish_save() { reply(SAVE); finish_write(); reply(SAVE_COMMIT); }
} // namespace

namespace pocket {
void redraw() {}
void save() { save_dirty = true; save_due = 0; }
} // namespace pocket
namespace game {
bool input_active() { return input_busy; }
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
  while (saving_handle || saving_commit_ready) {
    simulated_now += 20000;
    int calls_before = write_calls;uint64_t began = simulated_now;
    service_save();max_step=std::max(max_step,simulated_now-began);++steps;
    assert(write_calls-calls_before <= 1);
  }
  assert(written.size()==snapshot_size && written[0]==71 && max_step==900000 && steps>250);
  uint32_t hash=2166136261u;
  for(size_t i=0;i<written.size()-4;++i)hash=(hash^written[i])*16777619u;
  for(unsigned i=0;i<4;++i)assert(written[written.size()-4+i]==uint8_t(hash>>(8*i)));
  reply(SAVE_COMMIT);assert(world_catalog[active_world].seed==71);
  printf("PASS %zu-byte save: short writes, immutable bytes and incremental checksum; emulated 8ms writes / 900ms close / 400ms replace, maximum save step %llu us across %u steps\n",
    snapshot_size,(unsigned long long)max_step,steps);
  save_dirty=true;save_due=0;fail_write=2;send_save();reply(SAVE);
  simulated_now+=20000;service_save();simulated_now+=20000;service_save();
  assert(!saving_handle && !save_pending && save_dirty && persist_status<0 &&
    world_catalog[active_world].seed==71);
  puts("PASS interrupted write closes its handle, keeps the committed catalogue and schedules retry");

  compressible_fixture=true;save_retry=save_due=0;save_dirty=true;fail_write=0;
  size_t opens_before_pack=requests.size();
  send_save();assert(saving_packing && requests.size()==opens_before_pack);
  for(unsigned i=0;i<100;++i)service_save();
  assert(saving_packing && requests.size()==opens_before_pack);
  while(saving_packing){++frame_id;service_save();}
  assert(requests.size()==opens_before_pack+1);
  assert(saving_packed && pending_size<10000);
  finish_save();assert(starsoil_save::packed(written.data(),written.size()));
  disk_read=written;read_cursor=0;
  assert(read_world_file({0,1}) && loaded_size==snapshot_size);
  assert(std::memcmp(loaded,"SSR1",4)==0 && loaded[4]==19);
  assert(starsoil_save::load32(loaded+loaded_size-4)==
      starsoil_save::checksum(starsoil_save::InitialHash,loaded,loaded_size-4));
  puts("PASS production dispatcher compresses, preserves short-write snapshots and expands a checksummed save on load");
  save_dirty=true;save_due=save_retry=0;save_defer_began=0;input_busy=true;
  size_t before_requests=requests.size();send_save();
  assert(!save_pending && requests.size()==before_requests);
  uint64_t deferred_at=save_defer_began;
  simulated_now+=21000000;send_save();assert(!save_pending);
  simulated_now=deferred_at+22000000;send_save();assert(save_pending);
  input_busy=false;finish_save();
  save_dirty=true;save_due=0;input_busy=true;send_save();assert(!save_pending);
  input_busy=false;simulated_now+=599999;send_save();assert(!save_pending);
  simulated_now+=1;send_save();assert(save_pending);finish_save();
  input_busy=true;save_dirty=true;save_due=0;send_save(true);assert(save_pending);
  reply(SAVE);
  while(saving_at<pending_size)service_save(true);
  int commits_before=commit_calls;
  service_save(true);assert(!saving_handle && saving_commit_ready && commit_calls==commits_before);
  service_save(true);assert(!saving_commit_ready && commit_calls==commits_before+1);
  reply(SAVE_COMMIT);input_busy=false;
  puts("PASS idle save grace, bounded continuous input, forced lifecycle save and separate close/commit steps");

  save_dirty=true;save_due=save_retry=0;send_save(true);reply(SAVE);
  while(saving_at<pending_size)service_save(true);
  int prior_commits=commit_calls;auto prior_catalogue=world_catalog[active_world];
  fail_close=true;service_save(true);fail_close=false;
  assert(save_dirty && !save_pending && !saving_commit_ready &&
      commit_calls==prior_commits && world_catalog[active_world].seed==prior_catalogue.seed);
  simulated_now=save_retry;send_save(true);finish_save();
  assert(persist_status==2 && !save_dirty);
  puts("PASS failed close never publishes a replacement; the previous catalogue survives and retry commits normally");

  active_world=-1;save_dirty=true;save_due=0;send_save();
  assert(!save_dirty && save_defer_began==0);
  active_world=0;save_dirty=true;save_due=0;save_last_active=simulated_now;
  input_busy=false;send_save();assert(!save_pending);
  simulated_now+=600000;send_save();assert(save_pending);finish_save();
  save_defer_began=123;save_dirty=false;send_save();assert(!save_defer_began);
  puts("PASS quick input events receive idle grace; title/load/clean states cannot inherit a stale save deadline");

  async_host=saving_async_supported=true;save_dirty=true;save_due=save_retry=0;
  snapshot_size=5000;compressible_fixture=false;current_seed=89;async_bytes.clear();
  int sync_writes_before=write_calls,sync_closes_before=close_calls,sync_commits_before=commit_calls;
  uint64_t owner_began=simulated_now;
  send_save(true);assert(save_pending && saving_async_waiting && !saving_session);
  service_save(true);assert(async_bytes.empty());reply(SAVE_ATOMIC_BEGIN);
  assert(saving_session==async_session && !saving_async_waiting);
  while(saving_at<pending_size) {
    size_t old_at=saving_at;service_save(true);
    assert(saving_async_waiting && saving_at==old_at);
    size_t queued=async_bytes.size();service_save(true);assert(async_bytes.size()==queued);
    async_written=uint32_t(saving_chunk);reply(SAVE_ATOMIC_WRITE);
    assert(!saving_async_waiting && saving_at>old_at);
  }
  service_save(true);assert(save_pending && saving_async_waiting && built_opcode==PXA_FS_ATOMIC_COMMIT);
  assert(world_catalog[active_world].seed!=89);reply(SAVE_ATOMIC_COMMIT);
  assert(!save_pending && !saving_session && world_catalog[active_world].seed==89);
  assert(simulated_now==owner_began && write_calls==sync_writes_before && close_calls==sync_closes_before && commit_calls==sync_commits_before);
  assert(async_bytes.size()==snapshot_size && async_bytes[0]==89);
  uint32_t async_hash=starsoil_save::checksum(starsoil_save::InitialHash,async_bytes.data(),async_bytes.size()-4);
  assert(starsoil_save::load32(async_bytes.data()+async_bytes.size()-4)==async_hash);
  current_seed=90;save_dirty=true;send_save(true);reply(SAVE_ATOMIC_BEGIN);service_save(true);
  reply(SAVE_ATOMIC_WRITE,-99);
  assert(save_dirty && !save_pending && !saving_session && world_catalog[active_world].seed==89);
  puts("PASS async saves wait for acknowledgements, preserve immutable/checksummed chunks, never call synchronous write/close/replace, publish after commit and retain old state on failure");

}
