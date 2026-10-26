/* SPDX-License-Identifier: MIT. Test-only original Bridges interpreter. */
#include <stdarg.h>
#include <stdint.h>
#include "../common/retro-games/puzzles/vendor/bridges.c"
void fatal(const char *format,...) {va_list a;va_start(a,format);vfprintf(stderr,format,a);va_end(a);exit(2);}
void get_random_seed(void **seed,int *size) {*seed=dupstr("test-only");*size=9;}
static bool real(char *m) {return m&&m!=MOVE_UI_UPDATE&&m!=MOVE_NO_EFFECT&&m!=MOVE_UNUSED;}
static uint32_t hash_state(const game_state *s) {
 uint32_t h=2166136261u;for(int i=0;i<s->w*s->h;i++) {
  h=(h^(uint32_t)s->grid[i])*16777619u;h=(h^(unsigned char)s->lines[i])*16777619u;
 }return h;
}
static char *gesture(const game_state *state,int ts,int x1,int y1,int x2,int y2,bool right) {
 game_ui *ui=new_ui(state);ui->cur_visible=false;
 game_drawstate drawstate={0},*ds=&drawstate;ds->tilesize=ts;
 int button=right?RIGHT_BUTTON:LEFT_BUTTON;
 char *ret=NULL,*m;int points[3][2]={{COORD(x1)+ts/2,COORD(y1)+ts/2},{COORD(x2)+ts/2,COORD(y2)+ts/2},{COORD(x2)+ts/2,COORD(y2)+ts/2}};
 for(int i=0;i<3;i++) {m=interpret_move(state,ui,ds,points[i][0],points[i][1],button+3*i);if(real(m)) {if(ret)fatal("Unexpected multiple state moves");ret=m;}}
 free_ui(ui);return ret;
}
int main(int argc,char **argv) {
 if(argc<4)fatal("Usage: oracle game-id tilesize left|right|mark [prior-move...]");
 char *id=dupstr(argv[1]),*desc=strchr(id,':');if(!desc)fatal("Invalid ID");*desc++=0;
 game_params *p=default_params();decode_params(p,id);const char *error=validate_params(p,true);
 if(error)fatal("Invalid parameters");if(validate_desc(p,desc))fatal("Invalid description");
 game_state *opening=new_game(NULL,p,desc),*state=dup_game(opening);int ts=atoi(argv[2]);
 int x1=-1,y1=-1,x2=-1,y2=-1;
 for(int i=0;i<opening->n_islands&&x1<0;i++)for(int j=i+1;j<opening->n_islands;j++) {
  struct island *a=&opening->islands[i],*b=&opening->islands[j];
  if(a->x!=b->x&&a->y!=b->y)continue;
  char *m=gesture(opening,ts,a->x,a->y,b->x,b->y,false);
  if(!m)continue;game_state *trial=execute_move(opening,m);
  if(trial&&hash_state(trial)!=hash_state(opening)) {x1=a->x;y1=a->y;x2=b->x;y2=b->y;}
  if(trial)free_game(trial);sfree(m);if(x1>=0)break;
 }
 if(x1<0)fatal("No playable bridge found");
 for(int i=4;i<argc;i++) {game_state *next=execute_move(state,argv[i]);if(!next)fatal("Invalid prior move");free_game(state);state=next;}
 bool mark=!strcmp(argv[3],"mark"),right=!strcmp(argv[3],"right");
 if(right) {
  x1=-1;
  for(int i=0;i<state->n_islands&&x1<0;i++)for(int j=i+1;j<state->n_islands;j++) {
   struct island *a=&state->islands[i],*b=&state->islands[j];
   if(a->x!=b->x&&a->y!=b->y)continue;
   char *candidate=gesture(state,ts,a->x,a->y,b->x,b->y,true);
   if(!candidate)continue;game_state *trial=execute_move(state,candidate);
   if(trial&&hash_state(trial)!=hash_state(state)) {x1=a->x;y1=a->y;x2=b->x;y2=b->y;}
   if(trial)free_game(trial);sfree(candidate);if(x1>=0)break;
  }
  if(x1<0)fatal("No legal empty connection for original no-line marker");
 }
 char *move=gesture(state,ts,x1,y1,mark?x1:x2,mark?y1:y2,right);
 if(!move)fatal("Expected original move");game_state *after=execute_move(state,move);if(!after)fatal("Invalid result");
 int sum_before=0,sum_after=0;for(int i=0;i<state->w*state->h;i++) {sum_before+=(unsigned char)state->lines[i];sum_after+=(unsigned char)after->lines[i];}
 printf("{\"width\":%d,\"height\":%d,\"islands\":%d,\"max_bridges\":%d,\"source\":[%d,%d],\"destination\":[%d,%d],\"start\":[%d,%d],\"end\":[%d,%d],\"gesture\":\"%s\",\"mode\":%d,\"move\":\"%s\",\"before_hash\":%u,\"after_hash\":%u,\"lines_before\":%d,\"lines_after\":%d}\n",
 state->w,state->h,state->n_islands,state->maxb,x1,y1,x2,y2,(x1+1)*ts,(y1+1)*ts,(mark?x1+1:x2+1)*ts,(mark?y1+1:y2+1)*ts,mark?"tap":"drag",right?1:0,move,hash_state(state),hash_state(after),sum_before,sum_after);
 free_game(after);sfree(move);free_game(state);free_game(opening);free_params(p);sfree(id);return 0;
}
