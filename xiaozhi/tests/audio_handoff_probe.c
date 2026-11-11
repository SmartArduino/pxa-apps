/* SPDX-License-Identifier: Apache-2.0
 * App integration probe: compare sink-accepted PCM against submitted samples
 * when automatic listening reopens the microphone. No timing approximation.
 */
#undef NDEBUG
#define _XOPEN_SOURCE 700
#define _POSIX_C_SOURCE 200809L
#include "pxa/audio_mixer.h"
static pxa_status_t probe_pcm_write(pxa_audio_mixer_voice_t *, const uint8_t *, size_t);
static void probe_mixer_flush(pxa_audio_mixer_voice_t *);
static size_t probe_mixer_render(pxa_audio_mixer_t *, int16_t *, size_t);
static pxa_status_t probe_buffered(pxa_audio_mixer_voice_t *);
#define pxa_audio_mixer_write probe_pcm_write
#define pxa_audio_mixer_flush probe_mixer_flush
#define pxa_audio_mixer_render probe_mixer_render
#define pxa_audio_mixer_buffered probe_buffered
#define PXSYS_PRODUCT_RUNNER_LIBRARY 1
#include PXA_PRODUCT_RUNNER
#undef pxa_audio_mixer_write
#undef pxa_audio_mixer_flush
#undef pxa_audio_mixer_render
#undef pxa_audio_mixer_buffered
#include <assert.h>
static uint64_t pcm_hash = UINT64_C(14695981039346656037);
static uint64_t pcm_samples;
static pxa_status_t probe_buffered(pxa_audio_mixer_voice_t *v) {
  return getenv("XIAOZHI_NO_BUFFERED") ? PXA_STATUS_UNSUPPORTED : pxa_audio_mixer_buffered(v);
}
static unsigned explicit_flushes;
static uint64_t pending_silence, resumed_silence;
static int started;
static size_t probe_mixer_render(pxa_audio_mixer_t *mixer, int16_t *out, size_t count) {
  uint64_t before=0, after=0;
  for(unsigned i=0;i<PXA_AUDIO_MIXER_VOICES;++i) before+=mixer->voices[i].state.accepted_samples;
  size_t extent=pxa_audio_mixer_render(mixer,out,count);
  for(unsigned i=0;i<PXA_AUDIO_MIXER_VOICES;++i) after+=mixer->voices[i].state.accepted_samples;
  if(after>before) {
    if(started) resumed_silence+=pending_silence;
    pending_silence=count-extent; started=1;
  } else if(started) pending_silence+=count;
  return extent;
}
static void probe_mixer_flush(pxa_audio_mixer_voice_t *voice) {
  /* Closing a rejected, empty buffered route precedes the fallback open. */
  if(voice->state.submitted_samples) ++explicit_flushes;
  pxa_audio_mixer_flush(voice);
}
static pxa_status_t probe_pcm_write(pxa_audio_mixer_voice_t *voice, const uint8_t *pcm, size_t bytes) {
  pxa_status_t status = pxa_audio_mixer_write(voice, pcm, bytes);
  if (!status) {
    for (size_t i=0; i<bytes; ++i)
      pcm_hash=(pcm_hash^pcm[i])*UINT64_C(1099511628211);
    pcm_samples += bytes/2;
  }
  return status;
}
static struct {
  product_host_t *host;
  int checked;
} test;
static void bind_probe(void *context, void (*focus)(void *, bool),
                       void (*exit_app)(void *), void *runner) {
  (void)context;
  (void)focus;
  (void)exit_app;
  test.host = runner;
}
static void pump_probe(void *context) {
  (void)context;
  product_host_t *host = test.host;
  if(host && getenv("XIAOZHI_EXPECT_CONTINUOUS") && !test.checked) {
    static uint64_t last_stall;
    SDL_LockAudioDevice(host->audio_device);
    uint64_t accepted=0;
    for(unsigned i=0;i<PXA_AUDIO_MIXER_VOICES;++i) accepted+=host->audio_mixer.voices[i].state.accepted_samples;
    SDL_UnlockAudioDevice(host->audio_device);
    uint64_t now=SDL_GetTicks64();
    if(accepted>=5120 && now-last_stall>=450) { last_stall=now; SDL_Delay(220); }
  }
  if (!host || test.checked || host->capture_sequence < 2 || !host->capture)
    return;
  if (getenv("XIAOZHI_EXPECT_MUTED")) {
    for (unsigned i = 0; i < PXA_AUDIO_MIXER_VOICES; ++i)
      assert(!host->audio_sessions[i] && "Muted mode opened a playback session");
    fprintf(stderr, "XIAOZHI_MUTED_HANDOFF playback_sessions=0\n");
    test.checked = 1;
    return;
  }
  SDL_LockAudioDevice(host->audio_device);
  assert(!explicit_flushes && "Natural reply completion must drain, not flush, physical audio");
  for (unsigned i = 0; i < PXA_AUDIO_MIXER_VOICES; ++i) {
    if (!host->audio_sessions[i])
      continue;
    const pxa_audio_state_t state = host->audio_mixer.voices[i].state;
    if (!state.submitted_samples)
      continue;
    fprintf(stderr,
            "XIAOZHI_AUDIO_HANDOFF submitted=%llu accepted=%llu queued=%u pcm_samples=%llu pcm_hash=%016llx\n",
            (unsigned long long)state.submitted_samples,
            (unsigned long long)state.accepted_samples, state.queued_samples,
            (unsigned long long)pcm_samples, (unsigned long long)pcm_hash);
    assert(state.submitted_samples == state.accepted_samples &&
           "Automatic listening flushed the unplayed TTS tail");
    assert(!state.queued_samples);
    if(getenv("XIAOZHI_NO_BUFFERED")) {
      assert(!host->audio_mixer.voices[i].buffered && host->audio_mixer.voices[i].queue_limit==4);
      fprintf(stderr,"XIAOZHI_PCM_FALLBACK queue_limit=4\n");
    }
    if(getenv("XIAOZHI_EXPECT_CONTINUOUS")) {
      fprintf(stderr,"XIAOZHI_CONTINUITY resumed_silence_samples=%llu buffered=%u queue_limit=%u\n",(unsigned long long)resumed_silence,host->audio_mixer.voices[i].buffered,host->audio_mixer.voices[i].queue_limit);
      assert(!resumed_silence && "TTS starved between accepted PCM segments");
    }
    test.checked = 1;
  }
  SDL_UnlockAudioDevice(host->audio_device);
}
int main(int argc, char **argv) {
  options_t options = {0};
  options.width = 320;
  options.height = 240;
  options.density_dpi = 160;
  options.locale = "zh-CN";
  for (int i = 1; i < argc; ++i) {
    if (i + 1 >= argc)
      return 2;
    const char *option = argv[i++], *value = argv[i];
    if (!strcmp(option, "--package"))
      options.package_path = value;
    else if (!strcmp(option, "--publisher-key"))
      options.publisher_key = value;
    else if (!strcmp(option, "--state-root"))
      options.state_root = value;
    else if (!strcmp(option, "--locale"))
      options.locale = value;
    else if (!strcmp(option, "--width"))
      options.width = (uint32_t)atoi(value);
    else if (!strcmp(option, "--height"))
      options.height = (uint32_t)atoi(value);
    else if (!strcmp(option, "--pxadb-control-socket"))
      options.pxadb_control_socket = value;
    else
      return 2;
  }
  if (!options.package_path || !options.publisher_key)
    return 2;
  int result = run_product_simulator(&options, NULL, NULL, pump_probe, NULL,
                                     NULL, NULL, bind_probe, NULL, NULL, NULL);
  if (!result && !test.checked)
    return 3;
  return result;
}
