/* SPDX-License-Identifier: MIT. Test-only original Signpost interpreter/solver. */
#include <stdarg.h>
#include <stdint.h>
#include "../common/retro-games/puzzles/vendor/signpost.c"
void fatal(const char *format,...) {va_list a;va_start(a,format);vfprintf(stderr,format,a);va_end(a);exit(2);}
void get_random_seed(void **seed,int *size) {*seed=dupstr("test-only");*size=9;}
static bool real(char *m) {return m&&m!=MOVE_UI_UPDATE&&m!=MOVE_NO_EFFECT&&m!=MOVE_UNUSED;}
static uint32_t hash_state(const game_state *s) {
 uint32_t h=2166136261u;for(int i=0;i<s->n;i++) {h=(h^s->nums[i])*16777619u;h=(h^s->next[i])*16777619u;h=(h^s->prev[i])*16777619u;}return h;
}
static char *gesture(const game_state *state,int ts,int x1,int y1,int x2,int y2,bool right) {
 game_ui *ui=new_ui(state);game_drawstate drawstate={0},*ds=&drawstate;ds->tilesize=ts;
 char *ret=NULL,*m;int button=right?RIGHT_BUTTON:LEFT_BUTTON;
 int points[3][2]={{COORD(x1)+ts/2,COORD(y1)+ts/2},{COORD(x2)+ts/2,COORD(y2)+ts/2},{COORD(x2)+ts/2,COORD(y2)+ts/2}};
 for(int i=0;i<3;i++) {m=interpret_move(state,ui,ds,points[i][0],points[i][1],button+3*i);if(real(m)) {if(ret)fatal("Multiple state moves");ret=m;}}
 free_ui(ui);return ret;
}
static void print_move(const game_state *before,const game_state *after,int ts,int x1,int y1,int x2,int y2,bool right,const char *move) {
 game_drawstate drawstate={0},*ds=&drawstate;ds->tilesize=ts;
 printf("{\"width\":%d,\"height\":%d,\"start\":[%d,%d],\"end\":[%d,%d],\"gesture\":\"drag\",\"mode\":%d,\"move\":\"%s\",\"before_hash\":%u,\"after_hash\":%u,\"completed\":%s}",before->w,before->h,COORD(x1)+ts/2,COORD(y1)+ts/2,COORD(x2)+ts/2,COORD(y2)+ts/2,right?1:0,move,hash_state(before),hash_state(after),after->completed?"true":"false");
}
int main(int argc,char **argv) {
 if(argc<4)fatal("Usage: oracle game-id tilesize left|right|clear|solution [prior-move...]");
 char *id=dupstr(argv[1]),*desc=strchr(id,':');if(!desc)fatal("Invalid ID");*desc++=0;
 game_params *p=default_params();decode_params(p,id);if(validate_params(p,true)||validate_desc(p,desc))fatal("Invalid ID");
 game_state *opening=new_game(NULL,p,desc),*state=dup_game(opening),*target=dup_game(opening);int ts=atoi(argv[2]);
 if(solve_state(target)<=0||!check_completion(target,false))fatal("Original solver did not complete");
 for(int i=4;i<argc;i++) {game_state *next=execute_move(state,argv[i]);if(!next)fatal("Invalid prior move");free_game(state);state=next;}
 bool solution=!strcmp(argv[3],"solution"),clear=!strcmp(argv[3],"clear"),right=!strcmp(argv[3],"right");
 if(solution)printf("{\"width\":%d,\"height\":%d,\"cells\":%d,\"original_solver_completed\":true,\"touches\":[",state->w,state->h,state->n);
 int count=0;
 do {
  int from=-1,to=-1,x1,y1,x2,y2;
  if(clear) {
   for(int i=0;i<state->n;i++)if((state->next[i]>=0||state->prev[i]>=0)&&!(state->nums[i]==state->n&&(state->flags[i]&FLAG_IMMUTABLE))) {from=i;break;}
   if(from<0)fatal("No linked cell to clear");x1=from%state->w;y1=from/state->w;x2=state->w;y2=y1;
  }else {
   for(int i=0;i<state->n;i++)if(target->next[i]>=0&&state->next[i]!=target->next[i]) {from=i;to=target->next[i];break;}
   if(from<0)break;
   if(solution)right=(count%2)==1;
   x1=(right?to:from)%state->w;y1=(right?to:from)/state->w;x2=(right?from:to)%state->w;y2=(right?from:to)/state->w;
  }
  char *move=gesture(state,ts,x1,y1,x2,y2,right);if(!move)fatal("Original interpreter rejected target gesture");
  game_state *next=execute_move(state,move);if(!next||hash_state(state)==hash_state(next))fatal("No original state change");
  if(solution&&count)printf(",");print_move(state,next,ts,x1,y1,x2,y2,right,move);count++;
  sfree(move);free_game(state);state=next;
 }while(solution);
 if(!count)fatal("No changing target gesture");
 if(solution) {if(!state->completed||state->used_solve)fatal("Touch sequence did not complete honestly");printf("],\"generated_touch_completed\":true,\"original_game_status\":%d,\"moves\":%d,\"final_numbers\":[",game_status(state),count);for(int i=0;i<state->n;i++)printf("%s%d",i?",":"",state->nums[i]);printf("]}");}
 printf("\n");free_game(state);free_game(target);free_game(opening);free_params(p);sfree(id);return 0;
}
