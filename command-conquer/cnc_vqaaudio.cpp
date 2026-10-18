/**
 * @file cnc_vqaaudio.cpp
 * @brief Movie (VQA) sound of the ESP32-S3 port (replaces vqaaudio_openal.cpp).
 *
 * The VQA player decodes the movie sound into a ring of HMIBufSize blocks; the
 * blocks are queued, two at a time like the OpenAL version, on a sample tracker
 * of the mixer (cnc_sound.cpp). The movie clock is the system clock.
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 */
#include "esp_timer.h"
#include "soundio_imp.h"
#include "vqaaudio.h"
#include "vqafile.h"
#include "vqaloader.h"
#include "vqatask.h"
#include <string.h>

#define QUEUED_BLOCKS 2

// cnc_sound.cpp: number of buffers queued on a tracker
int SoundImp_Get_Sample_Queued(SampleTrackerTypeImp *st);

int AudioFlags;
static int TimerIntCount;
static int TimerMethod;
static int TickOffset;
static unsigned VQAAudioPaused;
static VQAHandle *AudioVQAHandle;
static SampleTrackerTypeImp *Tracker;

// Queues the next block if it is decoded (as Queue_Audio() of
// vqaaudio_openal.cpp)
static bool Queue_Audio(void) {
  VQAConfig *config = &AudioVQAHandle->Config;
  VQAAudio *audio = &AudioVQAHandle->VQABuf->Audio;

  SoundImp_Buffer_Sample_Data(Tracker, &audio->Buffer[audio->PlayPosition],
                              config->HMIBufSize);
  audio->field_B8 = audio->field_B0;
  audio->field_B0 += config->HMIBufSize;
  if (audio->field_B0 >= audio->BuffBytes) {
    audio->field_B0 = 0;
  }

  audio->field_14 = audio->field_10 + 1;
  if (audio->field_14 >= audio->NumAudBlocks) {
    audio->field_14 = 0;
  }
  if (audio->IsLoaded[audio->field_14] != 1) {
    if (VQAMovieDone) {
      ++audio->field_B4;
    }
    ++audio->NumSkipped;
    config->DrawFlags &= 0xFB;
    return false;
  }

  audio->IsLoaded[audio->field_10] = 0;
  audio->PlayPosition += config->HMIBufSize;
  ++audio->field_10;
  if (audio->PlayPosition >= config->AudioBufSize) {
    audio->PlayPosition = 0;
    audio->field_10 = 0;
  }
  ++audio->field_B4;
  return true;
}

static void VQA_AudioCallback(void) {
  if (!VQAAudioPaused && AudioVQAHandle && Tracker) {
    while (SoundImp_Get_Sample_Queued(Tracker) < QUEUED_BLOCKS) {
      if (!Queue_Audio()) {
        break;
      }
    }
    if (!SoundImp_Sample_Status(Tracker) &&
        SoundImp_Get_Sample_Queued(Tracker) > 0) {
      SoundImp_Start_Sample(Tracker);
    }
  }
}

static void Release_Tracker(void) {
  if (Tracker) {
    SoundImp_Stop_Sample(Tracker);
    SoundImp_Shutdown_Sample(Tracker);
    Tracker = nullptr;
  }
}

int VQA_StartTimerInt(VQAHandle *handle, int a2) {
  VQAAudio *audio = &handle->VQABuf->Audio;
  if (!(AudioFlags & VQA_AUDIO_FLAG_INTERRUPT_TIMER)) {
    AudioFlags |= VQA_AUDIO_FLAG_UNKNOWN016;
  }
  audio->Flags |= VQA_AUDIO_FLAG_UNKNOWN016;
  ++TimerIntCount;
  return 0;
}

void VQA_StopTimerInt(VQAHandle *handle) {
  if (TimerIntCount > 0) {
    --TimerIntCount;
  }
  AudioFlags &= ~VQA_AUDIO_FLAG_INTERRUPT_TIMER;
}

int VQA_OpenAudio(VQAHandle *handle, void *hwnd) {
  VQAConfig *config = &handle->Config;
  VQAHeader *header = &handle->Header;
  VQAAudio *audio = &handle->VQABuf->Audio;

  audio->field_10 = 0;
  if (config->AudioRate == -1) {
    config->AudioRate =
        header->FPS == config->FrameRate
            ? audio->SampleRate
            : config->FrameRate * audio->SampleRate / header->FPS;
  }
  audio->Flags |= VQA_AUDIO_FLAG_UNKNOWN001;
  AudioFlags |= VQA_AUDIO_FLAG_UNKNOWN001;
  return 0;
}

void VQA_CloseAudio(VQAHandle *handle) {
  VQAAudio *audio = &handle->VQABuf->Audio;
  VQA_StopAudio(handle);
  AudioFlags &= ~(VQA_AUDIO_FLAG_UNKNOWN004 | VQA_AUDIO_FLAG_UNKNOWN008);
  audio->Flags &= ~(VQA_AUDIO_FLAG_UNKNOWN004 | VQA_AUDIO_FLAG_UNKNOWN008);
  audio->Flags &= ~(VQA_AUDIO_FLAG_UNKNOWN001 | VQA_AUDIO_FLAG_UNKNOWN002);
  AudioFlags &= ~(VQA_AUDIO_FLAG_UNKNOWN001 | VQA_AUDIO_FLAG_UNKNOWN002 |
                  VQA_AUDIO_FLAG_AUDIO_DMA_TIMER);
}

int VQA_StartAudio(VQAHandle *handle) {
  VQAConfig *config = &handle->Config;
  VQAAudio *audio = &handle->VQABuf->Audio;

  AudioVQAHandle = handle;
  if (AudioFlags & VQA_AUDIO_FLAG_AUDIO_DMA_TIMER) {
    return -1;
  }
  Release_Tracker();
  Tracker = SoundImp_Init_Sample(audio->BitsPerSample, audio->Channels > 1,
                                 audio->SampleRate);
  if (Tracker == nullptr) {
    return -1;
  }
  SoundImp_Set_Sample_Volume(Tracker, config->Volume << 8);

  audio->BuffBytes = config->HMIBufSize * 4;
  audio->field_B0 = 0;
  audio->field_B4 = 0;
  for (int i = 0; i < QUEUED_BLOCKS; i++) {
    Queue_Audio();
  }
  SoundImp_Start_Sample(Tracker);

  audio->Flags |= VQA_AUDIO_FLAG_AUDIO_DMA_TIMER;
  AudioFlags |= VQA_AUDIO_FLAG_AUDIO_DMA_TIMER;
  return 0;
}

void VQA_StopAudio(VQAHandle *handle) {
  VQAAudio *audio = &handle->VQABuf->Audio;
  if (AudioFlags & VQA_AUDIO_FLAG_AUDIO_DMA_TIMER) {
    Release_Tracker();
    audio->Flags &= ~VQA_AUDIO_FLAG_AUDIO_DMA_TIMER;
    AudioFlags &= ~VQA_AUDIO_FLAG_AUDIO_DMA_TIMER;
  }
  AudioVQAHandle = nullptr;
}

void VQA_PauseAudio() {
  if (AudioVQAHandle && Tracker &&
      (AudioFlags & VQA_AUDIO_FLAG_AUDIO_DMA_TIMER) && !VQAAudioPaused) {
    VQAAudioPaused = VQA_GetTime(AudioVQAHandle);
    SoundImp_Set_Sample_Volume(Tracker, 0);
  }
}

void VQA_ResumeAudio() {
  if (AudioVQAHandle && Tracker &&
      (AudioFlags & VQA_AUDIO_FLAG_AUDIO_DMA_TIMER) && VQAAudioPaused) {
    SoundImp_Set_Sample_Volume(Tracker, AudioVQAHandle->Config.Volume << 8);
    TickOffset -= VQA_GetTime(AudioVQAHandle) - VQAAudioPaused;
    VQAAudioPaused = 0;
  }
}

// Moves the decoded sound into the ring of blocks (as vqaaudio_openal.cpp)
int VQA_CopyAudio(VQAHandle *handle) {
  VQAConfig *config = &handle->Config;
  VQAAudio *audio = &handle->VQABuf->Audio;

  VQA_AudioCallback();

  if ((config->OptionFlags & 1) && audio->Buffer != nullptr &&
      audio->TempBufSize > 0) {
    int current_block = audio->AudBufPos / config->HMIBufSize;
    int next_block =
        (audio->TempBufSize + audio->AudBufPos) / config->HMIBufSize;
    if ((unsigned)next_block >= audio->NumAudBlocks) {
      next_block -= audio->NumAudBlocks;
    }
    if (audio->IsLoaded[next_block] == 1) {
      return -10;
    }
    if (next_block < current_block) {
      int end_space = config->AudioBufSize - audio->AudBufPos;
      int remaining = audio->TempBufSize - end_space;
      memcpy(&audio->Buffer[audio->AudBufPos], audio->TempBuf, end_space);
      memcpy(audio->Buffer, &audio->TempBuf[end_space], remaining);
      audio->AudBufPos = remaining;
      audio->TempBufSize = 0;
      for (unsigned i = current_block; i < audio->NumAudBlocks; ++i) {
        audio->IsLoaded[i] = 1;
      }
      for (int i = 0; i < next_block; ++i) {
        audio->IsLoaded[i] = 1;
      }
    } else {
      memcpy(&audio->Buffer[audio->AudBufPos], audio->TempBuf,
             audio->TempBufSize);
      audio->AudBufPos += audio->TempBufSize;
      audio->TempBufSize = 0;
      for (int i = current_block; i < next_block; ++i) {
        audio->IsLoaded[i] = 1;
      }
    }
  }
  return 0;
}

void VQA_SetTimer(VQAHandle *handle, int time, int method) {
  if (method == -1) {
    if (AudioFlags & VQA_AUDIO_FLAG_AUDIO_DMA_TIMER) {
      method = VQA_AUDIO_TIMER_METHOD_DMA;
    } else if (AudioFlags &
               (VQA_AUDIO_FLAG_UNKNOWN016 | VQA_AUDIO_FLAG_UNKNOWN032)) {
      method = VQA_AUDIO_TIMER_METHOD_INTERRUPT;
    } else {
      method = VQA_AUDIO_TIMER_METHOD_DOS;
    }
  } else {
    if (!(AudioFlags & VQA_AUDIO_FLAG_AUDIO_DMA_TIMER) && method == 3) {
      method = VQA_AUDIO_TIMER_METHOD_INTERRUPT;
    }
    if (!(AudioFlags &
          (VQA_AUDIO_FLAG_UNKNOWN016 | VQA_AUDIO_FLAG_UNKNOWN032)) &&
        method == VQA_AUDIO_TIMER_METHOD_INTERRUPT) {
      method = VQA_AUDIO_TIMER_METHOD_DOS;
    }
  }
  TimerMethod = method;
  TickOffset = 0;
  TickOffset = time - VQA_GetTime(handle);
}

// Movie time in 1/60 s
unsigned VQA_GetTime(VQAHandle *handle) {
  return unsigned(TickOffset + (int)(esp_timer_get_time() * 60 / 1000000));
}

int VQA_TimerMethod() { return TimerMethod; }
