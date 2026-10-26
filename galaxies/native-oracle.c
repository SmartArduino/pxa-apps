/* SPDX-License-Identifier: MIT. Test-only unchanged Galaxies interpreter. */
#include <stdarg.h>
#include <stdint.h>
#include "../common/retro-games/puzzles/vendor/galaxies.c"
void fatal(const char *format,...) {va_list a;va_start(a,format);vfprintf(stderr,format,a);va_end(a);exit(2);}
void get_random_seed(void **seed,int *size) {*seed=dupstr("test-only");*size=9;}
static bool real(char *m) {return m&&m!=MOVE_UI_UPDATE&&m!=MOVE_NO_EFFECT&&m!=MOVE_UNUSED;}
static uint32_t hash_state(const game_state *s) {
 uint32_t h=2166136261u;for(int i=0;i<s->sx*s->sy;i++) {
  h=(h^s->grid[i].flags)*16777619u;
  if(s->grid[i].flags&F_TILE_ASSOC) {h=(h^s->grid[i].dotx)*16777619u;h=(h^s->grid[i].doty)*16777619u;}
 }return h;
}
static char *gesture(const game_state *state,int ts,int x1,int y1,int x2,int y2,bool right) {
 game_ui *ui=new_ui(state);game_drawstate drawstate={0},*ds=&drawstate;ds->tilesize=ts;
 char *ret=NULL,*m;int button=right?RIGHT_BUTTON:LEFT_BUTTON;
 int points[3][2]={{SCOORD(x1),SCOORD(y1)},{SCOORD(x2),SCOORD(y2)},{SCOORD(x2),SCOORD(y2)}};
 for(int i=0;i<3;i++) {m=interpret_move(state,ui,ds,points[i][0],points[i][1],button+3*i);if(real(m)) {if(ret)fatal("Multiple state moves");ret=m;}}
 free_ui(ui);return ret;
}
int main(int argc,char **argv) {
 if(argc<4)fatal("Usage: oracle game-id tilesize left|right [prior-move...]");
 char *id=dupstr(argv[1]),*desc=strchr(id,':');if(!desc)fatal("Invalid ID");*desc++=0;
 game_params *p=default_params();decode_params(p,id);if(validate_params(p,true)||validate_desc(p,desc))fatal("Invalid game ID");
 game_state *opening=new_game(NULL,p,desc),*state=dup_game(opening);int ts=atoi(argv[2]);
 for(int i=4;i<argc;i++) {game_state *next=execute_move(state,argv[i]);if(!next)fatal("Invalid prior move");free_game(state);state=next;}
 bool right=!strcmp(argv[3],"right");int x1=-1,y1=-1,x2=-1,y2=-1;char *move=NULL;game_state *after=NULL;
 if(!right) {
  for(int y=1;y<state->sy-1&&x1<0;y++)for(int x=1;x<state->sx-1;x++)if(edge_placement_legal(state,x,y)) {
   move=gesture(state,ts,x,y,x,y,false);if(!move)continue;after=execute_move(state,move);
   if(after&&hash_state(after)!=hash_state(state)) {x1=x2=x;y1=y2=y;break;}
   if(after)free_game(after);sfree(move);move=NULL;
  }
 }else {
  for(int d=0;d<state->ndots&&x1<0;d++)for(int y=1;y<state->sy-1&&x1<0;y+=2)for(int x=1;x<state->sx-1;x+=2) {
   move=gesture(state,ts,state->dots[d]->x,state->dots[d]->y,x,y,true);if(!move)continue;after=execute_move(state,move);
   if(after&&hash_state(after)!=hash_state(state)) {x1=state->dots[d]->x;y1=state->dots[d]->y;x2=x;y2=y;break;}
   if(after)free_game(after);sfree(move);move=NULL;
  }
 }
 if(x1<0||!move||!after)fatal("No legal changing original gesture");
 game_drawstate drawstate={0},*ds=&drawstate;ds->tilesize=ts;
 int assoc_before=0,assoc_after=0;for(int i=0;i<state->sx*state->sy;i++) {assoc_before+=!!(state->grid[i].flags&F_TILE_ASSOC);assoc_after+=!!(after->grid[i].flags&F_TILE_ASSOC);}
 printf("{\"width\":%d,\"height\":%d,\"dots\":%d,\"source\":[%d,%d],\"destination\":[%d,%d],\"start\":[%d,%d],\"end\":[%d,%d],\"gesture\":\"%s\",\"mode\":%d,\"move\":\"%s\",\"before_hash\":%u,\"after_hash\":%u,\"associations_before\":%d,\"associations_after\":%d}\n",state->w,state->h,state->ndots,x1,y1,x2,y2,SCOORD(x1),SCOORD(y1),SCOORD(x2),SCOORD(y2),right?"drag":"tap",right?1:0,move,hash_state(state),hash_state(after),assoc_before,assoc_after);
 free_game(after);sfree(move);free_game(state);free_game(opening);free_params(p);sfree(id);return 0;
}
