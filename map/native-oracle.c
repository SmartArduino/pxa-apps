/* SPDX-License-Identifier: MIT. Test-only original Map interpreter/solver. */
#include <stdarg.h>
#include <stdint.h>
#include "../common/retro-games/puzzles/vendor/map.c"
void fatal(const char *format,...) {va_list a;va_start(a,format);vfprintf(stderr,format,a);va_end(a);exit(2);}
void get_random_seed(void **seed,int *size) {*seed=dupstr("test-only");*size=9;}
static bool real(char *m) {return m&&m!=MOVE_UI_UPDATE&&m!=MOVE_NO_EFFECT&&m!=MOVE_UNUSED;}
static uint32_t hash_state(const game_state *s) {uint32_t h=2166136261u;for(int i=0;i<s->p.n;i++) {h=(h^s->colouring[i])*16777619u;h=(h^s->pencil[i])*16777619u;}return h;}
static void region_point(const game_state *state,int ts,int r,int point[2]) {
 game_drawstate drawstate={0},*ds=&drawstate;ds->tilesize=ts;
 for(int y=0;y<state->p.h;y++)for(int x=0;x<state->p.w;x++)for(int yy=1;yy<=3;yy++)for(int xx=1;xx<=3;xx++) {
  int px=COORD(x)+ts*xx/4,py=COORD(y)+ts*yy/4;
  if(region_from_coords(state,ds,px,py)==r) {point[0]=px;point[1]=py;return;}
 }fatal("No original pixel for region %d",r);
}
static char *gesture(const game_state *state,int ts,const int start[2],const int end[2],bool right) {
 game_ui *ui=new_ui(state);game_drawstate drawstate={0};drawstate.tilesize=ts;
 char *ret=NULL,*m;int button=right?RIGHT_BUTTON:LEFT_BUTTON;
 int points[3][2]={{start[0],start[1]},{end[0],end[1]},{end[0],end[1]}};
 for(int i=0;i<3;i++) {m=interpret_move(state,ui,&drawstate,points[i][0],points[i][1],button+3*i);if(real(m)) {if(ret)fatal("Multiple state moves");ret=m;}}
 free_ui(ui);return ret;
}
static void print_move(const game_state *before,const game_state *after,const int start[2],const int end[2],int r,bool right,const char *move) {
 printf("{\"width\":%d,\"height\":%d,\"regions\":%d,\"region\":%d,\"start\":[%d,%d],\"end\":[%d,%d],\"gesture\":\"drag\",\"mode\":%d,\"move\":",before->p.w,before->p.h,before->p.n,r,start[0],start[1],end[0],end[1],right?1:0);
 if(move)printf("\"%s\"",move);else printf("null");
 printf(",\"before_hash\":%u,\"after_hash\":%u,\"colour_before\":%d,\"colour_after\":%d,\"pencil_before\":%d,\"pencil_after\":%d,\"completed\":%s}",hash_state(before),hash_state(after),before->colouring[r],after->colouring[r],before->pencil[r],after->pencil[r],after->completed?"true":"false");
}
int main(int argc,char **argv) {
 if(argc<4)fatal("Usage: oracle game-id tilesize left|right|clear|immutable|solution [prior-move...]");
 char *id=dupstr(argv[1]),*desc=strchr(id,':');if(!desc)fatal("Invalid ID");*desc++=0;
 game_params *p=default_params();decode_params(p,id);if(validate_params(p,true)||validate_desc(p,desc))fatal("Invalid ID");
 char *canonical=encode_params(p,false);if(strcmp(canonical,id))fatal("Noncanonical original short parameters");sfree(canonical);
 game_state *opening=new_game(NULL,p,desc),*state=dup_game(opening);int ts=atoi(argv[2]);const char *error=NULL;
 char *solve=solve_game(opening,opening,NULL,&error);if(!solve)fatal("Original solver failed");game_state *target=execute_move(opening,solve);if(!target||!target->completed)fatal("Invalid solver target");sfree(solve);
 for(int i=4;i<argc;i++) {game_state *next=execute_move(state,argv[i]);if(!next)fatal("Invalid prior move");free_game(state);state=next;}
 bool solution=!strcmp(argv[3],"solution"),right=!strcmp(argv[3],"right"),clear=!strcmp(argv[3],"clear"),immutable=!strcmp(argv[3],"immutable");
 if(solution)printf("{\"width\":%d,\"height\":%d,\"regions\":%d,\"original_solver_completed\":true,\"touches\":[",state->p.w,state->p.h,state->p.n);
 int count=0;
 do {
  int r=-1,source=-1,start[2]={0,0},end[2];
  for(int i=0;i<state->p.n;i++) {
   if(immutable) {if(state->map->immutable[i]) {r=i;break;}}
   else if(!state->map->immutable[i]) {
    if(solution?state->colouring[i]!=target->colouring[i]:clear?state->colouring[i]>=0:state->colouring[i]<0) {r=i;break;}
   }
  }
  if(r<0)break;region_point(state,ts,r,end);
  if(!clear&&!immutable) {
   int colour=right?opening->colouring[0]:target->colouring[r];
   if(right)for(int i=0;i<opening->p.n;i++)if(opening->colouring[i]>=0) {colour=opening->colouring[i];break;}
   for(int i=0;i<state->p.n;i++)if(state->map->immutable[i]&&state->colouring[i]==colour) {source=i;break;}
   if(source<0)fatal("No original given region for colour %d",colour);region_point(state,ts,source,start);
  }
  char *move=gesture(state,ts,start,end,right);
  if(immutable) {if(move)fatal("Original immutable region changed");print_move(state,state,start,end,r,false,NULL);count++;break;}
  if(!move)fatal("Original interpreter rejected colour/pencil gesture");
  game_state *next=execute_move(state,move);if(!next||hash_state(state)==hash_state(next))fatal("No original state change");
  if(solution&&count)printf(",");print_move(state,next,start,end,r,right,move);count++;
  sfree(move);free_game(state);state=next;
 }while(solution);
 if(!count)fatal("No changing target gesture");
 if(solution) {
  if(!state->completed||state->cheated)fatal("Real-touch plan did not complete");
  for(int i=0;i<state->map->ngraph;i++)if(state->colouring[state->map->graph[i]/state->p.n]==state->colouring[state->map->graph[i]%state->p.n])fatal("Adjacent colours collide");
  printf("],\"generated_touch_completed\":true,\"original_game_status\":%d,\"adjacency_entries\":%d,\"moves\":%d,\"final_colours\":[",game_status(state),state->map->ngraph,count);for(int i=0;i<state->p.n;i++)printf("%s%d",i?",":"",state->colouring[i]);printf("]}");
 }
 printf("\n");free_game(state);free_game(target);free_game(opening);free_params(p);sfree(id);return 0;
}
