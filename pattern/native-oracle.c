/* SPDX-License-Identifier: MIT
 * Test-only calls into unchanged official Pattern. No replacement game rules.
 */
#include <stdarg.h>
#include "../common/retro-games/puzzles/vendor/pattern.c"
void fatal(const char *format,...) {
 va_list args;va_start(args,format);vfprintf(stderr,format,args);va_end(args);fputc('\n',stderr);exit(2);
}
void frontend_default_colour(frontend *fe,float *c){(void)fe;c[0]=c[1]=c[2]=0.9F;}
static game_state *from_id(const char *id){
 char *copy=dupstr(id),*desc=strchr(copy,':');if(!desc)fatal("Missing description");*desc++=0;
 game_params *params=default_params();decode_params(params,copy);
 const char *error=validate_params(params,true);if(!error)error=validate_desc(params,desc);
 if(error)fatal("Original validation: %s",error);
 game_state *s=new_game(NULL,params,desc);free_params(params);sfree(copy);return s;
}
static char *pointer(const game_state *s,int tile,int x,int y,int end_x,int end_y,int mode){
 const int buttons[]={LEFT_BUTTON,RIGHT_BUTTON,MIDDLE_BUTTON};if(mode<0||mode>2)fatal("Invalid mode");
 game_drawstate ds={0};ds.tilesize=tile;game_ui *ui=new_ui(s);char *move=NULL;
 for(int phase=0;phase<3;phase++){
  char *ret=interpret_move(s,ui,&ds,phase?end_x:x,phase?end_y:y,buttons[mode]+phase*3);
  if(ret && ret!=MOVE_UI_UPDATE && ret!=MOVE_NO_EFFECT && ret!=MOVE_UNUSED){if(move)sfree(move);move=ret;}
 }
 free_ui(ui);return move;
}
static game_state *step(game_state *s,const char *move){
 game_state *next=execute_move(s,move);if(!next)fatal("Original rejected %s",move);free_game(s);return next;
}
static char *solution(game_state *s){
 const char *error=NULL;char *ret=solve_game(s,s,NULL,&error);
 if(error||!ret||ret[0]!='S'||strlen(ret)!=(size_t)(s->common->w*s->common->h+1))fatal("Original solver: %s",error?error:"invalid output");
 return ret;
}
static void output(game_state *s,const char *move,int tile){
 game_drawstate drawstate={0},*ds=&drawstate;ds->tilesize=tile;
 printf("{\"width\":%d,\"height\":%d,\"status\":%d,\"completed\":%s,\"cheated\":%s,\"origin_x\":%d,\"origin_y\":%d,\"move\":\"%s\",\"grid\":[",
 s->common->w,s->common->h,game_status(s),s->completed?"true":"false",s->cheated?"true":"false",TOCOORD(s->common->w,0),TOCOORD(s->common->h,0),move?move:"");
 for(int i=0;i<s->common->w*s->common->h;i++)printf("%s%d",i?",":"",s->grid[i]);printf("],\"immutable\":[");
 for(int i=0;i<s->common->w*s->common->h;i++)printf("%s%d",i?",":"",s->common->immutable[i]);
 printf("],\"clues\":[");
 for(int i=0;i<s->common->w+s->common->h;i++){
  printf("%s[",i?",":"");for(int j=0;j<s->common->rowlen[i];j++)printf("%s%d",j?",":"",s->common->rowdata[i*s->common->rowsize+j]);printf("]");
 }
 int count;float *colours=game_colours(NULL,&count);printf("],\"palette_rgb565\":[");
 for(int i=0;i<count;i++){
  unsigned r=(unsigned)(min(max(colours[i*3],0),1)*255+0.5F),g=(unsigned)(min(max(colours[i*3+1],0),1)*255+0.5F),b=(unsigned)(min(max(colours[i*3+2],0),1)*255+0.5F);
  printf("%s%u",i?",":"",((r>>3)<<11)|((g>>2)<<5)|(b>>3));
 }
 sfree(colours);puts("]}");
}
static void self_test(void){
 game_params *params=default_params();assert(params->w==15&&params->h==15);
 decode_params(params,"0x5");assert(validate_params(params,true));
 decode_params(params,"5x5");assert(!validate_params(params,true));
 for(int sample=0;sample<8;sample++){
  char seed[80];snprintf(seed,sizeof(seed),"official Pattern custom5x5 QA %d",sample);
  random_state *rs=random_new(seed,strlen(seed));char *aux=NULL;char *desc=new_game_desc(params,rs,&aux,false);
  assert(!validate_desc(params,desc));game_state *s=new_game(NULL,params,desc);assert(!s->completed&&!s->cheated);
  assert(!execute_move(s,"INVALID"));assert(!execute_move(s,"F5,0,1,1"));
  char *fill=pointer(s,24,114,114,114,114,0);assert(fill&&!strcmp(fill,"F0,0,1,1"));s=step(s,fill);sfree(fill);assert(s->grid[0]==GRID_FULL);
  char *clear=pointer(s,24,114,114,114,114,2);assert(clear&&!strcmp(clear,"U0,0,1,1"));s=step(s,clear);sfree(clear);assert(s->grid[0]==GRID_UNKNOWN);
  char *sol=solution(s);
  for(int i=0;i<25;i++){char move[64];snprintf(move,sizeof(move),"%c%d,%d,1,1",sol[i+1]=='1'?'F':'E',i%5,i/5);s=step(s,move);}
  assert(game_status(s)==1&&s->completed&&!s->cheated);
  for(int i=0;i<25;i++)assert(s->grid[i]==(sol[i+1]=='1'?GRID_FULL:GRID_EMPTY));
  sfree(sol);free_game(s);sfree(desc);sfree(aux);free_params(params);random_free(rs);params=default_params();decode_params(params,"5x5");
 }
 free_params(params);
 puts("PASS: original default15 unchanged, validated custom5, 8 generated clue-solvable puzzles, illegal bounds rejection, original pointer fill/clear, all25 ordinary F/E cells completed=true cheated=false");
}
int main(int argc,char **argv){
 if(argc==2&&!strcmp(argv[1],"--self-test")){self_test();return 0;}
 if(argc<2)fatal("Usage: native-oracle game-id [moves] [--click tile x y mode | --drag tile x y endx endy mode | --solution]");
 game_state *s=from_id(argv[1]);char *clicked=NULL;int tile=24;
 for(int i=2;i<argc;i++){
  if(!strcmp(argv[i],"--click")||!strcmp(argv[i],"--drag")){
   int drag=!strcmp(argv[i],"--drag");if(i+(drag?6:4)>=argc)fatal("Pointer needs coordinates");
   tile=atoi(argv[++i]);int x=atoi(argv[++i]),y=atoi(argv[++i]),endx=x,endy=y;
   if(drag){endx=atoi(argv[++i]);endy=atoi(argv[++i]);}int mode=atoi(argv[++i]);
   sfree(clicked);clicked=pointer(s,tile,x,y,endx,endy,mode);if(clicked)s=step(s,clicked);
  }else if(!strcmp(argv[i],"--tile")){if(i+1>=argc)fatal("Missing tile");tile=atoi(argv[++i]);}
  else if(!strcmp(argv[i],"--solution")){
   char *sol=solution(s);printf("{\"solution_grid\":[");for(int j=1;sol[j];j++)printf("%s%d",j>1?",":"",sol[j]=='1'?1:0);puts("],\"method\":\"original solve_game with NULL auxiliary, native only; no Guest S move\"}");
   sfree(sol);sfree(clicked);free_game(s);return 0;
  }else s=step(s,argv[i]);
 }
 output(s,clicked,tile);sfree(clicked);free_game(s);return 0;
}
