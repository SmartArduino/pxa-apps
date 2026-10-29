/* SPDX-License-Identifier: Apache-2.0
 * App integration probe: compare sink-accepted PCM against submitted samples
 * when automatic listening reopens the microphone. No timing approximation.
 */
#undef NDEBUG
#define _XOPEN_SOURCE 700
#define _POSIX_C_SOURCE 200809L
#include "pxa/audio_mixer.h"
static pxa_status_t probe_pcm_write(pxa_audio_mixer_voice_t *, const uint8_t *, size_t);
#define pxa_audio_mixer_write probe_pcm_write
#define PXSYS_PRODUCT_RUNNER_LIBRARY 1
#include PXA_PRODUCT_RUNNER
#undef pxa_audio_mixer_write
#include <assert.h>
static uint64_t pcm_hash = UINT64_C(14695981039346656037);
static uint64_t pcm_samples;
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
