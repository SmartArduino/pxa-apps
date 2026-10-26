/* SPDX-License-Identifier: MIT. Test-only original Rect interpreter. */
#include <stdarg.h>
#include <stdint.h>
#include "../common/retro-games/puzzles/vendor/rect.c"
void fatal(const char *format,...) {va_list a;va_start(a,format);vfprintf(stderr,format,a);va_end(a);exit(2);}
void get_random_seed(void **seed,int *size) {*seed=dupstr("test-only");*size=9;}
static bool real(char *m) {return m&&m!=MOVE_UI_UPDATE&&m!=MOVE_NO_EFFECT&&m!=MOVE_UNUSED;}
static uint32_t hash_state(const game_state *s) {
 uint32_t h=2166136261u;for(int i=0;i<s->w*s->h;i++) {h=(h^s->hedge[i])*16777619u;h=(h^s->vedge[i])*16777619u;}return h;
}
int main(int argc,char **argv) {
 if(argc<4)fatal("Usage: oracle game-id tilesize left|right [prior-move...]");
 char *id=dupstr(argv[1]),*desc=strchr(id,':');if(!desc)fatal("Invalid ID");*desc++=0;
 game_params *p=default_params();decode_params(p,id);if(validate_params(p,true)||validate_desc(p,desc))fatal("Invalid game ID");
 game_state *state=new_game(NULL,p,desc);int ts=atoi(argv[2]);
 for(int i=4;i<argc;i++) {game_state *next=execute_move(state,argv[i]);if(!next)fatal("Invalid prior move");free_game(state);state=next;}
 bool right=!strcmp(argv[3],"right");game_ui *ui=new_ui(state);game_drawstate drawstate={0},*ds=&drawstate;ds->tilesize=ts;
 /* Original RIGHT clears interior edges, leaving the dragged outline intact.
  * Include the previously drawn 2x2 rectangle inside a 3x3 erase region. */
 int extent=right?2:1;
 int start[2]={COORD(0)+ts/2,COORD(0)+ts/2},end[2]={COORD(extent)+ts/2,COORD(extent)+ts/2};
 int points[3][2]={{start[0],start[1]},{end[0],end[1]},{end[0],end[1]}};
 int button=right?RIGHT_BUTTON:LEFT_BUTTON;char *move=NULL;
 for(int i=0;i<3;i++) {char *m=interpret_move(state,ui,ds,points[i][0],points[i][1],button+3*i);if(real(m)) {if(move)fatal("Multiple state moves");move=m;}}
 if(!move)fatal("No original rectangle move");game_state *after=execute_move(state,move);if(!after||hash_state(after)==hash_state(state))fatal("No rectangle state change");
 printf("{\"width\":%d,\"height\":%d,\"start\":[%d,%d],\"end\":[%d,%d],\"gesture\":\"drag\",\"mode\":%d,\"move\":\"%s\",\"before_hash\":%u,\"after_hash\":%u}\n",state->w,state->h,start[0],start[1],end[0],end[1],right?1:0,move,hash_state(state),hash_state(after));
 free_ui(ui);free_game(after);sfree(move);free_game(state);free_params(p);sfree(id);return 0;
}
