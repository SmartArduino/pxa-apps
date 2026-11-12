// SPDX-License-Identifier: GPL-3.0-only
// Included outside the platform's anonymous namespace, shared by both renderers.
namespace md_audio {
void scene(Scene next, unsigned mode) {
  mode = std::min(2u, mode);
  bool changing = (music_scene == Scene::Menu) != (next == Scene::Menu);
  if (changing) {
    music_changing = true;
    music_interval = foreground_us() + 180000000;
    music_wave_due = 0;
  }
  music_scene = next; music_mode = mode;
  if (mode == 2) music_stop();
}
void wave() { music_wave_due = foreground_us() + 8000000 + uint64_t(audio_random(7001)) * 1000; }
void stop(unsigned track) { stop_track(track); }
void effect(md::Sound id, unsigned track, unsigned volume) {
  if (track >= SoundTracks || unsigned(id) >= recording_count || !volume ||
      !effects_gain || paused || background) return;
  requested_sounds[track] = {unsigned(id), std::min(100u, volume), true, false, foreground_us() + 500000};
}
void loop(md::Sound id, unsigned track, unsigned volume) {
  if (track >= SoundTracks) return;
  if (!volume || !effects_gain || paused || background) { stop_track(track); return; }
  auto &playing = sound_tracks[track];
  volume = std::min(100u, volume);
  if (playing.loop && playing.id == unsigned(id)) {
    playing.end = foreground_us() + 500000;
    if (playing.gain != volume && sound_handle && pxa_audio_control_sound(sound_handle,
        uint8_t(track), PXA_AUDIO_ASSET_SET_GAIN, recording_gain(effects_gain * volume / 100)) >= 0)
      playing.gain = volume;
    return;
  }
  requested_sounds[track] = {unsigned(id), volume, true, true, foreground_us() + 500000};
}
}
namespace milky {
void sounds(const char *const *paths, unsigned count) {
  recording_paths = paths; recording_count = std::min(64u, count); prewarm_index = 0;
}
void play(unsigned id, unsigned track, bool loop) {
  if (loop) md_audio::loop(md::Sound(id), track, 100);
  else md_audio::effect(md::Sound(id), track, 100);
}
void stop(unsigned track) { stop_track(track); }
void volume(unsigned effects, unsigned music) {
  effects_gain = std::min(100u, effects); music_gain = std::min(100u, music);
  if (!effects_gain) for (unsigned i = 0; i < SoundTracks; ++i) stop_track(i);
  if (!music_gain) music_stop();
}
}
