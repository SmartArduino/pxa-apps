#pragma once
#include <stddef.h>
struct Stream;
bool olClassicBorrow(const void *pointer,size_t bytes);
void olClassicOpen(Stream *stream);
bool olClassicExists(const char *name);
void olClassicTrack(int track,bool background);
void olClassicBadData();
bool olClassicValidateSlot(const void *data,size_t bytes,const void *level);
int olClassicFirstLevel();
void olClassicTitleBackground(unsigned alpha);
