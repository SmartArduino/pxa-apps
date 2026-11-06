// SPDX-License-Identifier: MIT
// Included in runtime.cpp. Files are the catalogue: no separate index can get
// out of sync with a committed save. Slot zero preserves the pre-menu filename.
pocket::WorldInfo world_catalog[pocket::WorldSlots];
enum class WorldOperation { None, Open, Create, Remove };
WorldOperation world_operation = WorldOperation::None;
int active_world = -1, requested_world = -1, scan_world = 0, worlds_error = 0;
uint32_t requested_seed;
uint8_t requested_size;
bool opening_world, removing_world;
size_t pending_size;
bool saving_packed, saving_packing;
starsoil_save::IncrementalPack save_packer;
uint64_t save_pack_frame = UINT64_MAX;
bool saving_commit_ready;
bool saving_async_supported = true, saving_async_waiting;
uint64_t saving_session;
size_t saving_chunk;
uint64_t save_last_active, save_defer_began;
size_t saving_at, saving_hash_at;
uint64_t saving_handle, saving_next_step;
uint32_t saving_hash = starsoil_save::InitialHash;
uint32_t save_step_max_us, save_snapshot_max_us, save_blocks;
uint32_t save_open_max_us, save_write_max_us, save_close_max_us, save_commit_max_us;
pocket::WorldInfo saving_info;
int persist_status;
char saving_path[48], saving_temp[48];

void world_path(int slot, char *out, bool temporary = false) {
  if (slot == 0)
    snprintf(out, 48, "starsoil-world-v1.%s", temporary ? "tmp" : "bin");
  else
    snprintf(out, 48, "starsoil-world-%d.%s", slot + 1,
             temporary ? "tmp" : "bin");
}
void storage_error(int reason) {
  worlds_error = reason;
  world_operation = WorldOperation::None;
  opening_world = removing_world = false;
  pocket::redraw();
}
bool read_world_file(const pxa_fs_open_result_t &r) {
  loaded_size = 0;
  if (r.status)
    return false;
  bool ok = true;
  while (loaded_size < sizeof(loaded)) {
    uint32_t want =
        uint32_t(std::min(sizeof(loaded) - loaded_size, size_t(2048)));
    int32_t n = pxa_fs_read(r.handle, loaded + loaded_size, want);
    if (n < 0) {
      ok = false;
      break;
    }
    if (!n)
      break;
    loaded_size += uint32_t(n);
  }
  uint8_t extra;
  if (pxa_fs_read(r.handle, &extra, 1) != 0)
    ok = false;
  if (pxa_fs_close(r.handle))
    ok = false;
  return ok && starsoil_save::unpack(loaded, loaded_size, sizeof(loaded), SAVE_CAPACITY);
}
void scan_next_world() {
  while (scan_world < pocket::WorldSlots) {
    char path[48];
    world_path(scan_world, path);
    if (!pxa_fs_request_open(LOAD, path, uint32_t(strlen(path)),
                             PXA_FS_OPEN_READ))
      return;
    // An unreadable slot stays reserved; creating a world must not overwrite
    // it.
    world_catalog[scan_world++].occupied = true;
  }
  loaded_size = 0;
  load_ready = true;
}
void abort_async_save() {
  if(saving_session) {
    uint32_t n=0;
    if(pxa_fs_build_atomic_operation(packet,sizeof(packet),PXA_FS_ATOMIC_ABORT,
        SAVE_ATOMIC_ABORT,saving_session,nullptr,0,&n))(void)pxa_control_packet(packet,n);
    saving_session=0;
  }
  saving_async_waiting=false;
}
int32_t open_save_file() {
  return pxa_fs_request_open(SAVE,saving_temp,strlen(saving_temp),
      PXA_FS_OPEN_WRITE|PXA_FS_OPEN_CREATE|PXA_FS_OPEN_TRUNCATE);
}
void save_failed() {
  saving_packing = false;
  abort_async_save();
  saving_commit_ready = false;
  if (saving_handle) {
    uint64_t handle = saving_handle;
    saving_handle = 0;
    (void)pxa_fs_close(handle);
  }
  save_pending = false;
  save_dirty = true;
  save_retry = now_us() + 2000000;
  persist_status = -1;
  pxa_log_write(3, "Starsoil: save failed; committed world retained, retrying");
}
bool begin_snapshot_write() {
  uint64_t open_began = now_us();
  int32_t status=PXA_STATUS_UNSUPPORTED;
  if(saving_async_supported) {
    uint32_t n=0;
    if(pxa_fs_build_atomic_begin(packet,sizeof(packet),SAVE_ATOMIC_BEGIN,
        saving_temp,strlen(saving_temp),saving_path,strlen(saving_path),pending_size,&n))
      status=pxa_control_packet(packet,n);
    else status=PXA_STATUS_INTERNAL;
    if(status==PXA_STATUS_UNSUPPORTED)saving_async_supported=false;
    else if(!status)saving_async_waiting=true;
  }
  if(!saving_async_supported)status=open_save_file();
  save_open_max_us = std::max(save_open_max_us, uint32_t(now_us() - open_began));
  if (status) {
    save_failed();
    return false;
  }
  return true;
}
void send_save(bool force = false) {
  uint64_t now = now_us();
  if (running && game::input_active()) save_last_active = now;
  if (!save_dirty) save_defer_began = 0;
  else if (!save_defer_began) save_defer_began = now;
  // The old world was already committed before these IO phases began. Pause
  // and lifecycle save requests must not restart a write during open/remove.
  if (opening_world || removing_world) {
    save_dirty = false;
    save_defer_began = 0;
    return;
  }
  if (active_world < 0) {
    save_dirty = false;
    save_defer_began = 0;
    return;
  }
  if (!running || !save_dirty || save_pending || now < save_retry ||
      (!force && now < save_due))
    return;
  // An 8-second automatic checkpoint may wait for a short quiet moment.
  // Continuous input is bounded to another 22 seconds; lifecycle/world
  // barriers bypass this gate and always flush the latest state.
  if (!force && now - save_defer_began < 22000000 &&
      now - save_last_active < 600000) return;
  uint64_t began = now_us();
  pending_size = game::serialize(pending_save, sizeof(pending_save), false);
  saving_packing = false;
  if (pending_size && pending_size <= SAVE_CAPACITY) {
    if (pending_size > 65536) {
      save_packer.begin(pending_save, pending_size, sizeof(pending_save));
      saving_packing = !save_packer.ready();
      save_pack_frame = UINT64_MAX;
    } else pending_size = starsoil_save::pack(pending_save, pending_size, sizeof(pending_save));
  }
  saving_packed = !saving_packing && starsoil_save::packed(pending_save, pending_size);
  save_snapshot_max_us = std::max(save_snapshot_max_us, uint32_t(now_us() - began));
  if (!pending_size) {
    save_dirty = false;
    return;
  }
  if (pending_size > sizeof(pending_save))
    __builtin_trap();
  if (pending_size < 4) { save_failed(); return; }
  game::describe_world(saving_info);
  world_path(active_world, saving_path);
  world_path(active_world, saving_temp, true);
  if (!saving_packing && !begin_snapshot_write()) return;
  save_pending = true;
  save_dirty = false;
  save_defer_began = 0;
  save_due = 0;
  persist_status = 1;
}
void service_save(bool urgent = false) {
  if(saving_packing) {
    if(!urgent && save_pack_frame==frame_id)return;
    save_pack_frame=frame_id;
    uint64_t began=now_us();
    if(save_packer.step(65536)) {
      saving_packing=false;pending_size=save_packer.result();
      saving_packed=starsoil_save::packed(pending_save,pending_size);
      (void)begin_snapshot_write();
    }
    save_step_max_us=std::max(save_step_max_us,uint32_t(now_us()-began));
    return;
  }

  if(saving_session) {
    if(saving_async_waiting || (!urgent && now_us()<saving_next_step))return;
    uint64_t began=now_us();uint32_t n=0;
    bool writing=saving_at<pending_size;
    saving_chunk=writing?std::min(pending_size-saving_at,size_t(PXA_FS_ATOMIC_MAX_CHUNK)):0;
    size_t hashed_end=std::min(saving_at+saving_chunk,pending_size-4);
    if(writing && !saving_packed && hashed_end>saving_hash_at) {
      saving_hash=starsoil_save::checksum(saving_hash,pending_save+saving_hash_at,hashed_end-saving_hash_at);
      saving_hash_at=hashed_end;
    }
    if(writing && !saving_packed && saving_at+saving_chunk>pending_size-4)
      for(unsigned i=0;i<4;++i)pending_save[pending_size-4+i]=uint8_t(saving_hash>>(8*i));
    if(!pxa_fs_build_atomic_operation(packet,sizeof(packet),
        writing?PXA_FS_ATOMIC_WRITE:PXA_FS_ATOMIC_COMMIT,
        writing?SAVE_ATOMIC_WRITE:SAVE_ATOMIC_COMMIT,saving_session,
        writing?pending_save+saving_at:nullptr,saving_chunk,&n) || pxa_control_packet(packet,n))save_failed();
    else saving_async_waiting=true;
    save_step_max_us=std::max(save_step_max_us,uint32_t(now_us()-began));
    saving_next_step=began+20000;
    return;
  }

  if ((!saving_handle && !saving_commit_ready) || (!urgent && now_us() < saving_next_step)) return;
  uint64_t began = now_us();
  if (saving_handle && saving_at < pending_size) {
    size_t want = std::min(pending_size - saving_at, size_t(4096));
    size_t hashed_end = std::min(saving_at + want, pending_size - 4);
    if (!saving_packed && hashed_end > saving_hash_at) {
      saving_hash = starsoil_save::checksum(saving_hash, pending_save + saving_hash_at,
                                            hashed_end - saving_hash_at);
      saving_hash_at = hashed_end;
    }
    if (!saving_packed && saving_at + want > pending_size - 4)
      for (unsigned i = 0; i < 4; ++i)
        pending_save[pending_size - 4 + i] = uint8_t(saving_hash >> (8 * i));
    int32_t n = pxa_fs_write(saving_handle, pending_save + saving_at, uint32_t(want));
    save_write_max_us = std::max(save_write_max_us, uint32_t(now_us() - began));
    ++save_blocks;
    if (n <= 0 || size_t(n) > want) save_failed();
    else saving_at += size_t(n);
  } else if (saving_handle) {
    // Data write, close/flush, and atomic replace each get their own frame.
    uint64_t handle = saving_handle;
    saving_handle = 0;
    bool ok = pxa_fs_close(handle) == 0;
    save_close_max_us = std::max(save_close_max_us, uint32_t(now_us() - began));
    if (ok) saving_commit_ready = true;
    else save_failed();
  } else {
    saving_commit_ready = false;
    uint32_t n = 0;
    if (!pxa_fs_build_replace(packet, sizeof(packet), SAVE_COMMIT, saving_temp,
          strlen(saving_temp), saving_path, strlen(saving_path), &n) ||
        pxa_control_packet(packet, n)) save_failed();
    save_commit_max_us = std::max(save_commit_max_us, uint32_t(now_us() - began));
  }
  save_step_max_us = std::max(save_step_max_us, uint32_t(now_us() - began));
  saving_next_step = began + 20000;
}
void finish_world_delete() {
  world_catalog[requested_world] = {};
  if (active_world == requested_world) {
    active_world = -1;
    save_dirty = false;
    persist_status = 0;
    game::world_deleted();
  }
  removing_world = false;
  world_operation = WorldOperation::None;
  pocket::redraw();
}
void service_worlds() {
  if (world_operation == WorldOperation::None || opening_world ||
      removing_world || exit_requested)
    return;
  // Complete the current world's immutable write before changing its path or
  // replacing State. A queued create/open/delete cannot steal a pending save.
  if (save_pending || save_dirty) {
    if (persist_status < 0 && !save_pending) {
      storage_error(2);
      return;
    }
    send_save(true);
    return;
  }
  char path[48];
  world_path(requested_world, path);
  if (world_operation == WorldOperation::Create) {
    if (world_catalog[requested_world].occupied) {
      storage_error(3);
      return;
    }
    active_world = requested_world;
    persist_status = 0;
    game::create_world(requested_seed, requested_size);
    blocked_buttons |= buttons;
    buttons = 0;
    memset(repeats, 0, sizeof(repeats));
    world_catalog[active_world].occupied = true;
    game::describe_world(world_catalog[active_world]);
    world_operation = WorldOperation::None;
    pocket::save();
    send_save(true);
  } else if (world_operation == WorldOperation::Open) {
    if (pxa_fs_request_open(WORLD_OPEN, path, uint32_t(strlen(path)),
                            PXA_FS_OPEN_READ)) {
      storage_error(2);
      return;
    }
    opening_world = true;
  } else {
    if (pxa_fs_request_path(PXA_FS_REMOVE, WORLD_REMOVE, path,
                            uint32_t(strlen(path)))) {
      storage_error(2);
      return;
    }
    removing_world = true;
  }
}
int32_t handle_world_storage(const pxa_event_t &e) {
  if (e.service != PXA_FS_SERVICE)
    return PXA_EVENT_UNHANDLED;
  if (e.token == LOAD || e.token == WORLD_OPEN) {
    pxa_fs_open_result_t r;
    if (!pxa_fs_parse_open(&e, e.token, &r))
      return PXA_STATUS_INTERNAL;
    bool ok = read_world_file(r);
    if (e.token == LOAD) {
      auto &info = world_catalog[scan_world];
      info.occupied = r.status != PXA_STATUS_NOT_FOUND;
      info.valid = ok && game::inspect_world(loaded, loaded_size, info);
      ++scan_world;
      scan_next_world();
    } else {
      opening_world = false;
      if (exit_requested) {
        world_operation = WorldOperation::None;
        return PXA_EVENT_HANDLED;
      }
      if (!ok) {
        storage_error(2);
        return PXA_EVENT_HANDLED;
      }
      if (!game::load(loaded, loaded_size)) {
        world_catalog[requested_world].valid = false;
        storage_error(1);
        return PXA_EVENT_HANDLED;
      }
      active_world = requested_world;
      game::inspect_world(loaded, loaded_size, world_catalog[active_world]);
      persist_status = 0;
      save_dirty = false;
      save_defer_began = 0;
      world_operation = WorldOperation::None;
      blocked_buttons |= buttons;
      buttons = 0;
      memset(repeats, 0, sizeof(repeats));
      pocket::redraw();
    }
    return PXA_EVENT_HANDLED;
  }
  if(e.token==SAVE_ATOMIC_ABORT)return PXA_EVENT_HANDLED;
  if(e.token==SAVE_ATOMIC_BEGIN || e.token==SAVE_ATOMIC_WRITE || e.token==SAVE_ATOMIC_COMMIT) {
    uint16_t opcode=e.token==SAVE_ATOMIC_BEGIN?PXA_FS_ATOMIC_BEGIN:
        e.token==SAVE_ATOMIC_WRITE?PXA_FS_ATOMIC_WRITE:PXA_FS_ATOMIC_COMMIT;
    pxa_fs_atomic_result_t r;
    if(!pxa_fs_parse_atomic(&e,e.token,opcode,&r))return PXA_STATUS_INTERNAL;
    saving_async_waiting=false;
    if(e.token==SAVE_ATOMIC_BEGIN && r.status==PXA_STATUS_UNSUPPORTED) {
      saving_async_supported=false;
      if(open_save_file())save_failed();
      return PXA_EVENT_HANDLED;
    }
    if(r.status){save_failed();return PXA_EVENT_HANDLED;}
    if(e.token==SAVE_ATOMIC_BEGIN) {
      saving_session=r.session;saving_at=saving_hash_at=0;
      saving_hash=starsoil_save::InitialHash;saving_next_step=0;
      pxa_log_write(2,"Starsoil: async save started");
    } else if(e.token==SAVE_ATOMIC_WRITE) {
      if(r.written!=saving_chunk){save_failed();return PXA_EVENT_HANDLED;}
      saving_at+=r.written;++save_blocks;
    } else {
      saving_session=0;save_pending=false;persist_status=2;
      world_catalog[active_world]=saving_info;
      pxa_log_write(2,"Starsoil: world committed (async)");pocket::redraw();
    }
    return PXA_EVENT_HANDLED;
  }
  if (e.token == SAVE) {
    pxa_fs_open_result_t r;
    if (!pxa_fs_parse_open(&e, SAVE, &r))
      return PXA_STATUS_INTERNAL;
    if (r.status) {
      save_failed();
      return PXA_EVENT_HANDLED;
    }
    saving_handle = r.handle;
    saving_commit_ready = false;
    saving_at = saving_hash_at = 0;
    saving_hash = starsoil_save::InitialHash;
    saving_next_step = 0;
    return PXA_EVENT_HANDLED;
  }
  if (e.token == SAVE_COMMIT) {
    int32_t status;
    if (!pxa_fs_parse_status(&e, SAVE_COMMIT, PXA_FS_REPLACE, &status))
      return PXA_STATUS_INTERNAL;
    if (status)
      save_failed();
    else {
      save_pending = false;
      persist_status = 2;
      world_catalog[active_world] = saving_info;
      pxa_log_write(2, "Starsoil: world committed");
      pocket::redraw();
    }
    return PXA_EVENT_HANDLED;
  }
  if (e.token == WORLD_REMOVE || e.token == WORLD_REMOVE_TEMP) {
    int32_t status;
    if (!pxa_fs_parse_status(&e, e.token, PXA_FS_REMOVE, &status))
      return PXA_STATUS_INTERNAL;
    if (e.token == WORLD_REMOVE_TEMP)
      finish_world_delete();
    else if (status && status != PXA_STATUS_NOT_FOUND)
      storage_error(2);
    else {
      char path[48];
      world_path(requested_world, path, true);
      if (pxa_fs_request_path(PXA_FS_REMOVE, WORLD_REMOVE_TEMP, path,
                              uint32_t(strlen(path))))
        finish_world_delete();
    }
    return PXA_EVENT_HANDLED;
  }
  return PXA_EVENT_UNHANDLED;
}
