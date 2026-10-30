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
  return ok;
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
void save_failed() {
  save_pending = false;
  save_dirty = true;
  save_retry = now_us() + 2000000;
  persist_status = -1;
  pxa_log_write(3, "Starsoil: save failed; committed world retained, retrying");
}
void send_save(bool force = false) {
  uint64_t now = now_us();
  // The old world was already committed before these IO phases began. Pause
  // and lifecycle save requests must not restart a write during open/remove.
  if (opening_world || removing_world) {
    save_dirty = false;
    return;
  }
  if (active_world < 0) {
    save_dirty = false;
    return;
  }
  if (!running || !save_dirty || save_pending || now < save_retry ||
      (!force && now < save_due))
    return;
  pending_size = game::serialize(pending_save, sizeof(pending_save));
  if (!pending_size) {
    save_dirty = false;
    return;
  }
  if (pending_size > sizeof(pending_save))
    __builtin_trap();
  world_path(active_world, saving_path);
  world_path(active_world, saving_temp, true);
  if (pxa_fs_request_open(SAVE, saving_temp, uint32_t(strlen(saving_temp)),
                          PXA_FS_OPEN_WRITE | PXA_FS_OPEN_CREATE |
                              PXA_FS_OPEN_TRUNCATE)) {
    save_failed();
    return;
  }
  save_pending = true;
  save_dirty = false;
  save_due = 0;
  persist_status = 1;
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
    size_t created_size = game::serialize(loaded, sizeof(loaded));
    world_catalog[active_world].valid =
        game::inspect_world(loaded, created_size, world_catalog[active_world]);
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
      world_operation = WorldOperation::None;
      blocked_buttons |= buttons;
      buttons = 0;
      memset(repeats, 0, sizeof(repeats));
      pocket::redraw();
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
    size_t at = 0;
    bool ok = true;
    while (at < pending_size) {
      int32_t n =
          pxa_fs_write(r.handle, pending_save + at,
                       uint32_t(std::min(pending_size - at, size_t(2048))));
      if (n <= 0) {
        ok = false;
        break;
      }
      at += uint32_t(n);
    }
    if (pxa_fs_close(r.handle))
      ok = false;
    uint32_t n = 0;
    if (!ok ||
        !pxa_fs_build_replace(packet, sizeof(packet), SAVE_COMMIT, saving_temp,
                              strlen(saving_temp), saving_path,
                              strlen(saving_path), &n) ||
        pxa_control_packet(packet, n))
      save_failed();
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
      auto &info = world_catalog[active_world];
      info.occupied = true;
      info.valid = game::inspect_world(pending_save, pending_size, info);
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
