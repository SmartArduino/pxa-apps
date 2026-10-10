#include "render.h"
#include <stddef.h>
#include "strings.h"
void pd_c_language(int language){pd_strings_set_language(language);}
size_t pd_c_game_size(void){return sizeof(pd_game_t);}
size_t pd_c_layout_size(void){return sizeof(pd_layout_t);}
int pd_c_render(const void* game,const void* layout,unsigned char* draw,unsigned long long id,unsigned now){return pd_render_present(77,0xffffffff,game,layout,draw,49152,id,now);}
