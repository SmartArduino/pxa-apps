// SPDX-License-Identifier: MIT
#pragma once
#include "pxa.h"
#include "pxa_assets.h"
#include "pxa_audio.h"
#include "pxa_permission.h"
#include "soundtrack.hpp"
#include <cstring>

namespace carrot {
// Project-local audio lifecycle; only public SDK interfaces are used.
// Long music streams on the Host. Short sounds are prepared once, then mixed
// on separate tracks, so rendering speed never drives PCM delivery.
class AudioPlayer {
  static constexpr uint64_t PermissionToken = 701, OpenToken = 702,
                            GraphToken = 703, SoundToken = 800;
  enum Stage { Idle, Permission, Opening, Graph, Ready, Unavailable };
  Stage stage = Idle;
  uint64_t permission = 0, session = 0, instance = 0, now = 0, lastCombat = 0;
  uint64_t sounds[music::Count]{};
  unsigned loading = 0, loaded = 0;
  bool pending = false, enabled = true, foreground = true, playing = false,
       musicAttempted = false;
  void loadNext() {
    if (stage != Ready || pending || loading == music::Count)
      return;
    int32_t status =
        pxa_assets_load_sound(SoundToken + loading, music::Sounds[loading]);
    if (status == PXA_STATUS_OK)
      pending = true;
    else if (status != PXA_STATUS_WOULD_BLOCK && status != PXA_STATUS_BUSY)
      ++loading;
  }
  void sync() {
    if (stage != Ready)
      return;
    const bool wanted = enabled && foreground;
    if (!wanted) {
      if (playing)
        pxa_audio_control_music(session, PXA_AUDIO_ASSET_PAUSE, 0);
      playing = false;
      for (uint8_t track = 3; track < 6; ++track)
        pxa_audio_control_sound(session, track, PXA_AUDIO_ASSET_STOP, 0);
    } else if (!musicAttempted) {
      musicAttempted = true;
      const auto status =
          pxa_audio_play_music(session, music::Theme, 1, -12 * 256, &instance);
      playing = status >= 0 && instance;
      if (!playing)
        pxa_log_write(2, "carrot generated music unavailable");
    } else if (instance && !playing) {
      playing =
          pxa_audio_control_music(session, PXA_AUDIO_ASSET_RESUME, 0) >= 0;
    }
  }

public:
  void start() {
    uint8_t request[128];
    uint32_t bytes = 0;
    if (pxa_permission_build(request, sizeof(request), PXA_PERMISSION_ACQUIRE,
                             PermissionToken, "audio.playback", 14,
                             reinterpret_cast<const uint8_t *>("media"), 5,
                             &bytes) &&
        pxa_control_packet(request, bytes) == PXA_STATUS_OK)
      stage = Permission;
    else
      stage = Unavailable;
  }
  void setEnabled(bool value) {
    if (enabled != value) {
      enabled = value;
      sync();
    }
  }
  void setForeground(bool value) {
    if (foreground != value) {
      foreground = value;
      sync();
    }
  }
  bool handle(const pxa_event_t &e) {
    pxa_permission_acquire_result_t grant;
    if (pxa_permission_parse_acquire(&e, PermissionToken, &grant)) {
      if (grant.status == PXA_STATUS_OK) {
        permission = grant.handle;
        stage = pxa_audio_open_media(OpenToken, permission) == PXA_STATUS_OK
                    ? Opening
                    : Unavailable;
      } else
        stage = Unavailable;
      return true;
    }
    pxa_audio_open_result_t opened;
    if (pxa_audio_parse_open(&e, OpenToken, &opened)) {
      if (opened.status == PXA_STATUS_OK) {
        session = opened.handle;
        stage = pxa_audio_commit_gain(GraphToken, session, -6 * 256) ==
                        PXA_STATUS_OK
                    ? Graph
                    : Unavailable;
      } else
        stage = Unavailable;
      return true;
    }
    int32_t status;
    if (pxa_audio_parse_status(&e, GraphToken, PXA_AUDIO_COMMIT_GRAPH,
                               &status)) {
      stage = status == PXA_STATUS_OK ? Ready : Unavailable;
      if (stage == Ready) {
        sync();
        loadNext();
      }
      return true;
    }
    pxa_asset_result_t asset;
    if (pending && pxa_assets_parse_result(&e, SoundToken + loading,
                                           PXA_ASSETS_LOAD, &asset)) {
      pending = false;
      if (stage == Ready && asset.status == PXA_STATUS_OK &&
          asset.info.kind == PXA_ASSET_AUDIO &&
          asset.info.encoding == PXA_ASSET_ENCODING_PCM_S16LE_16K_MONO) {
        sounds[loading] = asset.handle;
        ++loaded;
      } else {
        if (asset.status == PXA_STATUS_OK && asset.handle)
          pxa_close_handle(asset.handle);
        pxa_log_write(2, "carrot generated sound unavailable");
      }
      ++loading;
      if (loaded == music::Count)
        pxa_log_write(1, "carrot 14 generated sounds ready");
      loadNext();
      return true;
    }
    pxa_audio_playback_event_t playback;
    if (pxa_audio_parse_playback(&e, &playback) &&
        playback.session == session && playback.instance == instance) {
      if (playback.state == PXA_AUDIO_PLAYBACK_READY)
        pxa_log_write(1, "carrot generated music decoder ready");
      else if (playback.state == PXA_AUDIO_PLAYBACK_ERROR) {
        playing = false;
        instance = 0;
        pxa_log_write(2, "carrot generated music decoder failed");
      }
      return true;
    }
    pxa_permission_revoked_t revoked;
    if (pxa_permission_parse_revoked(&e, &revoked) && revoked.name_size == 14 &&
        !std::memcmp(revoked.name, "audio.playback", 14)) {
      stop();
      return true;
    }
    return false;
  }
  void tick(uint64_t time) {
    now = time;
    loadNext();
  }
  void cue(music::Cue kind) {
    const unsigned index = unsigned(kind);
    if (stage != Ready || !enabled || !foreground || index >= music::Count ||
        !sounds[index])
      return;
    const uint8_t track = music::track(kind);
    if (track == 4) {
      if (lastCombat && now - lastCombat < 80000)
        return;
      lastCombat = now;
    }
    pxa_audio_play_sound_track(session, sounds[index], track, 0, -3 * 256);
  }
  void stop() {
    if (pending)
      pxa_cancel(SoundToken + loading);
    for (auto &sound : sounds) {
      if (sound)
        pxa_close_handle(sound);
      sound = 0;
    }
    if (session)
      pxa_close_handle(session);
    if (permission)
      pxa_close_handle(permission);
    session = permission = instance = 0;
    playing = pending = false;
    stage = Unavailable;
  }
};
} // namespace carrot
