/* SPDX-License-Identifier: MIT
 * Test-only native calls into the unchanged official backend. No game rules. */
#include <stdarg.h>
#include "../common/retro-games/puzzles/vendor/sixteen.c"
#define QA_SIXTEEN 1
void fatal(const char *format,...) {
 va_list args;va_start(args,format);vfprintf(stderr,format,args);va_end(args);fputc('\n',stderr);exit(2);
}
void frontend_default_colour(frontend *fe,float *c){(void)fe;c[0]=c[1]=c[2]=0.9F;}
static int qa_w(const game_state *s){
#if defined(QA_UNRULY)
 return s->w2;
#elif defined(QA_PATTERN)
 return s->common->w;
#elif defined(QA_NETSLIDE)
 return s->width;
#else
 return s->w;
#endif
}
static int qa_h(const game_state *s){
#if defined(QA_UNRULY)
 return s->h2;
#elif defined(QA_PATTERN)
 return s->common->h;
#elif defined(QA_NETSLIDE)
 return s->height;
#else
 return s->h;
#endif
}
static int qa_cell(const game_state *s,int i){
#if defined(QA_SIXTEEN) || defined(QA_NETSLIDE)
 return s->tiles[i];
#else
 return s->grid[i];
#endif
}
static int qa_moves(const game_state *s){
#if defined(QA_SIXTEEN) || defined(QA_TWIDDLE)
 return s->movecount;
#elif defined(QA_NETSLIDE)
 return s->move_count;
#else
 (void)s;return -1;
#endif
}
static int qa_immutable(const game_state *s,int i){
#if defined(QA_PATTERN) || defined(QA_UNRULY)
 return s->common->immutable[i];
#else
 (void)s;(void)i;return 0;
#endif
}
static int qa_origin(const game_state *s,int tile,int vertical){
 game_drawstate ads={0},*ds=&ads;ds->tilesize=tile;
#if defined(QA_PATTERN)
 return TOCOORD(vertical?qa_h(s):qa_w(s),0);
#elif defined(QA_NETSLIDE)
 (void)s;(void)vertical;return BORDER+WINDOW_OFFSET;
#else
 (void)s;(void)vertical;return COORD(0);
#endif
}
static game_state *from_id(const char *id){
 char *copy=dupstr(id),*desc=strchr(copy,':');if(!desc)fatal("Missing description");*desc++=0;
 game_params *params=default_params();decode_params(params,copy);
 const char *error=validate_params(params,true);if(!error)error=validate_desc(params,desc);
 if(error)fatal("Original validation: %s",error);
 game_state *s=new_game(NULL,params,desc);free_params(params);sfree(copy);return s;
}
static char *click(const game_state *s,int tile,int x,int y,int mode){
 const int buttons[]={LEFT_BUTTON,RIGHT_BUTTON,MIDDLE_BUTTON};if(mode<0||mode>2)fatal("Invalid mode");
 game_drawstate ds={0};ds.tilesize=tile;game_ui *ui=new_ui(s);char *move=NULL;
 for(int phase=0;phase<3;phase++){
  char *ret=interpret_move(s,ui,&ds,x,y,buttons[mode]+phase*3);
  if(ret && ret!=MOVE_UI_UPDATE && ret!=MOVE_NO_EFFECT && ret!=MOVE_UNUSED){if(move)sfree(move);move=ret;}
 }
 free_ui(ui);return move;
}
static game_state *step(game_state *s,const char *move){
 game_state *next=execute_move(s,move);if(!next)fatal("Original rejected %s",move);free_game(s);return next;
}
static void output(game_state *s,const char *move,int tile){
 printf("{\"width\":%d,\"height\":%d,\"moves\":%d,\"status\":%d,\"origin_x\":%d,\"origin_y\":%d,\"move\":\"%s\",\"grid\":[",
  qa_w(s),qa_h(s),qa_moves(s),game_status(s),qa_origin(s,tile,0),qa_origin(s,tile,1),move?move:"");
 for(int i=0;i<qa_w(s)*qa_h(s);i++)printf("%s%d",i?",":"",qa_cell(s,i));printf("],\"immutable\":[");
 for(int i=0;i<qa_w(s)*qa_h(s);i++)printf("%s%d",i?",":"",qa_immutable(s,i));
 int count;float *colours=game_colours(NULL,&count);printf("],\"palette_rgb565\":[");
 for(int i=0;i<count;i++){
  unsigned r=(unsigned)(min(max(colours[i*3],0),1)*255+0.5F),g=(unsigned)(min(max(colours[i*3+1],0),1)*255+0.5F),b=(unsigned)(min(max(colours[i*3+2],0),1)*255+0.5F);
  printf("%s%u",i?",":"",((r>>3)<<11)|((g>>2)<<5)|(b>>3));
 }
 sfree(colours);
#ifdef QA_NETSLIDE
 unsigned char *active=compute_active(s,-1,-1);printf("],\"active\":[");
 for(int i=0;i<qa_w(s)*qa_h(s);i++)printf("%s%d",i?",":"",active[i]);sfree(active);
#endif
 puts("]}");
}
static void self_test(void){
 game_params *params=default_params();random_state *rs=random_new("official B-group QA",19);char *aux=NULL;
 char *desc=new_game_desc(params,rs,&aux,false);assert(validate_desc(params,desc)==NULL);
 game_state *s=new_game(NULL,params,desc);assert(game_status(s)==0);assert(execute_move(s,"INVALID")==NULL);
 char move[100];
#if defined(QA_SIXTEEN)
 strcpy(move,"R0,-1");
#elif defined(QA_TWIDDLE)
 strcpy(move,"M0,0,1");
#elif defined(QA_NETSLIDE)
 strcpy(move,"R0,1");
#elif defined(QA_UNRULY)
 int index=0;while(index<qa_w(s)*qa_h(s)&&qa_immutable(s,index))index++;assert(index<qa_w(s)*qa_h(s));
 snprintf(move,sizeof(move),"P1,%d,%d",index%qa_w(s),index/qa_w(s));
#else
 strcpy(move,"F0,0,1,1");
#endif
 game_state *changed=execute_move(s,move);assert(changed);
 int differences=0;for(int i=0;i<qa_w(s)*qa_h(s);i++)differences+=qa_cell(s,i)!=qa_cell(changed,i);assert(differences>0);
 if(qa_moves(s)>=0)assert(qa_moves(changed)==qa_moves(s)+1);
#if defined(QA_SIXTEEN) || defined(QA_TWIDDLE) || defined(QA_NETSLIDE)
#ifdef QA_TWIDDLE
 strcpy(move,"M0,0,-1");
#elif defined(QA_SIXTEEN)
 strcpy(move,"R0,1");
#else
 strcpy(move,"R0,-1");
#endif
 game_state *restored=execute_move(changed,move);assert(restored);
 for(int i=0;i<qa_w(s)*qa_h(s);i++)assert(qa_cell(s,i)==qa_cell(restored,i));assert(qa_moves(restored)==2);free_game(restored);
#endif
 free_game(changed);free_game(s);sfree(desc);sfree(aux);free_params(params);random_free(rs);
 puts("PASS: original generation/validation, illegal rejection, legal state change/count, inverse where applicable");
}
int main(int argc,char **argv){
 if(argc==2&&!strcmp(argv[1],"--self-test")){self_test();return 0;}
 if(argc<2)fatal("Usage: native-oracle game-id [moves ...] [--click tile x y mode]");
 game_state *s=from_id(argv[1]);char *clicked=NULL;int tile=48;
 for(int i=2;i<argc;i++){
  if(!strcmp(argv[i],"--click")){
   if(i+4>=argc)fatal("Click requires tile x y mode");tile=atoi(argv[++i]);int x=atoi(argv[++i]),y=atoi(argv[++i]),mode=atoi(argv[++i]);
   clicked=click(s,tile,x,y,mode);if(clicked)s=step(s,clicked);
  }else if(!strcmp(argv[i],"--tile")){if(i+1>=argc)fatal("Missing tile");tile=atoi(argv[++i]);}
  else s=step(s,argv[i]);
 }
 output(s,clicked,tile);sfree(clicked);free_game(s);return 0;
}
