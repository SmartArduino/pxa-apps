// SPDX-License-Identifier: MIT
// Test-only SDL callback tap. Capture actual mixed output without changing the
// simulator, SDK, application or the device's audio provider.
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <SDL.h>
#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static SDL_AudioCallback original_callback;
static void* original_context;
static int capture_fd=-1;
static void capture(void* unused,Uint8* stream,int length){
    (void)unused;original_callback(original_context,stream,length);
    size_t offset=0;
    while(capture_fd>=0&&offset<(size_t)length){
        ssize_t count=write(capture_fd,stream+offset,(size_t)length-offset);
        if(count<0&&errno==EINTR)continue;
        if(count<=0)break;
        offset+=(size_t)count;
    }
}
SDL_AudioDeviceID SDL_OpenAudioDevice(const char* device,int recording,
    const SDL_AudioSpec* requested,SDL_AudioSpec* obtained,int changes){
    typedef SDL_AudioDeviceID (*Open)(const char*,int,const SDL_AudioSpec*,SDL_AudioSpec*,int);
    Open open_device=(Open)dlsym(RTLD_NEXT,"SDL_OpenAudioDevice");
    if(!open_device)return 0;
    const char* path=getenv("PXA_TEST_AUDIO_PCM");
    if(recording||!requested||!requested->callback||!path||!*path)
        return open_device(device,recording,requested,obtained,changes);
    SDL_AudioSpec copy=*requested;
    original_callback=requested->callback;original_context=requested->userdata;
    copy.callback=capture;copy.userdata=NULL;
    capture_fd=open(path,O_CREAT|O_TRUNC|O_WRONLY,0600);
    SDL_AudioDeviceID result=open_device(device,recording,&copy,obtained,changes);
    char metadata[4096];
    if(result&&snprintf(metadata,sizeof metadata,"%s.json",path)<(int)sizeof metadata){
        FILE* file=fopen(metadata,"w");
        const SDL_AudioSpec* format=obtained?obtained:requested;
        if(file){fprintf(file,"{\"rate\":%d,\"channels\":%u,\"format\":%u}\n",format->freq,format->channels,format->format);fclose(file);}
    }
    if(!result&&capture_fd>=0){close(capture_fd);capture_fd=-1;}
    return result;
}
