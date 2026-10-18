/* PXA mixer; based on recrof/cnc_sound.cpp. GPL-3.0-or-later. */
#include "esp_heap_caps.h"
#include "port.h"
#include "soundio_imp.h"
#include <stdlib.h>
#include <string.h>
#define AUDIO_SAMPLE_RATE 16000
#define TRACKER_BUFFER_COUNT 4   // queued buffers per tracker
#define TRACKER_BUFFER_SIZE 8192 // soundio_common.cpp BUFFER_CHUNK_SIZE
#define FILL_BUFFERS_MAX 2       // free buffers reported at once (as OpenAL)
#define MAX_TRACKERS 8
#define MIX_FRAMES 256 // output frames per mixing pass (11.6 ms)
#define MIXER_TASK_PRIORITY (configMAX_PRIORITIES - 3)
#define MIXER_TASK_CORE 0
#define MASTER_SHIFT 1 // headroom: the output is attenuated by 6 dB

static const char *TAG = "sound";

struct SampleTrackerTypeImp {
  uint8_t *Buffers[TRACKER_BUFFER_COUNT];
  int Length[TRACKER_BUFFER_COUNT];
  int Head;          // buffer being played
  int Count;         // queued buffers
  uint32_t Position; // in the head buffer, in frames, 16.16 fixed point
  uint32_t Step;     // source frames per output frame, 16.16 fixed point
  int Bits;
  bool Stereo;
  int Rate;
  int Gain; // 0..256
  bool Started;
  bool Used;
};

static SampleTrackerTypeImp trackers[MAX_TRACKERS];

static bool txChannel;
static volatile bool paused;
static volatile uint32_t underruns;

static void setFormat(SampleTrackerTypeImp *st, int bits, bool stereo,
                      int rate) {
  st->Bits = bits;
  st->Stereo = stereo;
  st->Rate = rate;
  st->Step = (uint32_t)(((uint64_t)rate << 16) / AUDIO_SAMPLE_RATE);
}

static inline int frameSize(const SampleTrackerTypeImp *st) {
  return (st->Bits / 8) * (st->Stereo ? 2 : 1);
}

// Mixes one tracker into acc[] (stereo). Called with the lock held.
static void mixTracker(SampleTrackerTypeImp *st, int32_t *acc, int frames) {
  int fs = frameSize(st);
  for (int i = 0; i < frames && st->Count; i++) {
    uint32_t idx = st->Position >> 16;
    int bufFrames = st->Length[st->Head] / fs;
    if (idx >= (uint32_t)bufFrames) {
      // next buffer
      st->Position -= bufFrames << 16;
      st->Head = (st->Head + 1) % TRACKER_BUFFER_COUNT;
      st->Count--;
      i--;
      continue;
    }
    const uint8_t *p = st->Buffers[st->Head] + idx * fs;
    int l, r;
    if (st->Bits == 16) {
      l = (int16_t)(p[0] | (p[1] << 8));
      r = st->Stereo ? (int16_t)(p[2] | (p[3] << 8)) : l;
    } else {
      l = (p[0] - 128) << 8;
      r = st->Stereo ? (p[1] - 128) << 8 : l;
    }
    acc[2 * i] += l * st->Gain;
    acc[2 * i + 1] += r * st->Gain;
    st->Position += st->Step;
  }
}

bool SoundImp_Init(int, bool, int, bool) {
  txChannel = true;
  paused = false;
  return true;
}
extern "C" void port_game_audio() {
  static uint64_t last;
  uint64_t now = port_time_us();
  if (!last)
    last = now;
  unsigned frames = (now - last) * 16000 / 1000000;
  if (frames > 1600) {
    frames = 1600;
    last = now - 100000;
  }
  static int32_t acc[320 * 2];
  static int16_t out[320 * 2];
  while (frames >= 320) {
    memset(acc, 0, sizeof(acc));
    if (!paused)
      for (int i = 0; i < MAX_TRACKERS; i++)
        if (trackers[i].Used && trackers[i].Started)
          mixTracker(&trackers[i], acc, 320);
    for (int i = 0; i < 640; i++) {
      int v = acc[i] >> (8 + MASTER_SHIFT);
      out[i] = v > 32767 ? 32767 : v < -32768 ? -32768 : v;
    }
    port_pcm(out, 320, 16000, 2);
    frames -= 320;
    last += 20000;
  }
}
void SoundImp_Shutdown() { paused = true; }

void SoundImp_PauseSound() { paused = true; }

bool SoundImp_ResumeSound() {
  paused = false;
  return txChannel;
}

SampleTrackerTypeImp *SoundImp_Init_Sample(int bits_per_sample, bool stereo,
                                           int rate) {
  for (int t = 0; t < MAX_TRACKERS; t++) {
    SampleTrackerTypeImp *st = &trackers[t];
    if (!st->Used) {
      for (int i = 0; i < TRACKER_BUFFER_COUNT; i++) {
        st->Buffers[i] =
            (uint8_t *)heap_caps_malloc(TRACKER_BUFFER_SIZE, MALLOC_CAP_SPIRAM);
        if (!st->Buffers[i]) {
          return nullptr;
        }
      }
      setFormat(st, bits_per_sample, stereo, rate);
      st->Gain = 256;
      st->Head = st->Count = 0;
      st->Position = 0;
      st->Started = false;
      st->Used = true;
      return st;
    }
  }
  return nullptr;
}

void SoundImp_Shutdown_Sample(SampleTrackerTypeImp *st) {
  st->Used = false;
  for (int i = 0; i < TRACKER_BUFFER_COUNT; i++) {
    free(st->Buffers[i]);
    st->Buffers[i] = nullptr;
  }
}

void SoundImp_Set_Sample_Attributes(SampleTrackerTypeImp *st,
                                    int bits_per_sample, bool stereo,
                                    int rate) {
  setFormat(st, bits_per_sample, stereo, rate);
}

void SoundImp_Set_Sample_Volume(SampleTrackerTypeImp *st, unsigned int volume) {
  // volume: 0..255 * 255 (OpenAL gain = volume / 65536)
  st->Gain = volume >> 8;
}

void SoundImp_Buffer_Sample_Data(SampleTrackerTypeImp *st, const void *data,
                                 size_t datalen) {
  if (datalen > TRACKER_BUFFER_SIZE) {
    datalen = TRACKER_BUFFER_SIZE;
  }
  if (st->Count < TRACKER_BUFFER_COUNT) {
    int slot = (st->Head + st->Count) % TRACKER_BUFFER_COUNT;
    memcpy(st->Buffers[slot], data, datalen);
    st->Length[slot] = datalen;
    if (!st->Count) {
      st->Position = 0;
    }
    st->Count++;
  }
}

int SoundImp_Get_Sample_Free_Buffer_Count(SampleTrackerTypeImp *st) {
  // soundio_common.cpp fills this many chunks in one go; it assumes at most 2
  // (the OpenAL backend): a streamed song (only one compressed block queued
  // ahead) ends its chunk short when asked for more, which is taken for the end
  // of the song. Before the start, the queue is filled until no buffer is free:
  // 2 at most there too.
  if (!st->Started) {
    return st->Count < FILL_BUFFERS_MAX ? FILL_BUFFERS_MAX - st->Count : 0;
  }
  int free = TRACKER_BUFFER_COUNT - st->Count;
  return free < FILL_BUFFERS_MAX ? free : FILL_BUFFERS_MAX;
}

// Buffers queued (the movie player keeps two, cnc_vqaaudio.cpp)
int SoundImp_Get_Sample_Queued(SampleTrackerTypeImp *st) { return st->Count; }

void SoundImp_Start_Sample(SampleTrackerTypeImp *st) { st->Started = true; }

void SoundImp_Stop_Sample(SampleTrackerTypeImp *st) {
  st->Started = false;
  st->Count = 0;
  st->Head = 0;
  st->Position = 0;
}

bool SoundImp_Sample_Status(SampleTrackerTypeImp *st) {
  // playing while it has data queued (as an OpenAL source stops when its queue
  // runs dry)
  return st->Started && st->Count > 0;
}

extern "C" uint32_t soundGetUnderruns(void) { return underruns; }
