#pragma once
struct OlLevelInfo {const char *file,*name;unsigned kind;};
static constexpr OlLevelInfo OL_LEVELS[]={
#define OL_LEVEL(id,file,name,kind) {file,name,kind},
#include "levels.def"
#undef OL_LEVEL
};
static constexpr unsigned OL_LEVEL_COUNT=sizeof(OL_LEVELS)/sizeof(OL_LEVELS[0]);
static constexpr unsigned OL_PLAYABLE_IDS[]={1,2,3,4,5,7,8,9,10,11,13,14,15,16,18,20};
static constexpr unsigned OL_PLAYABLE_COUNT=sizeof(OL_PLAYABLE_IDS)/sizeof(OL_PLAYABLE_IDS[0]);
static inline bool ol_playable_level(unsigned id) {return id<OL_LEVEL_COUNT && (OL_LEVELS[id].kind==1 || OL_LEVELS[id].kind==2);}
