/* SPDX-License-Identifier: MIT. Test-only original Undead interpreter and solver. */
#include <stdarg.h>
#include <stdint.h>
#include "../common/retro-games/puzzles/vendor/undead.c"
void fatal(const char *format,...) { va_list a;va_start(a,format);vfprintf(stderr,format,a);va_end(a);fputc('\n',stderr);exit(2); }
void get_random_seed(void **seed,int *size) {*seed=dupstr("native-research-only");*size=20;}
static bool real(char *move) {return move && move!=MOVE_UI_UPDATE && move!=MOVE_NO_EFFECT && move!=MOVE_UNUSED;}
static uint32_t hash_state(const game_state *s) {
 uint32_t h=2166136261u;
 for(int i=0;i<s->common->num_total;i++) {h=(h^(unsigned)s->guess[i])*16777619u;h=(h^s->pencils[i])*16777619u;}
 for(int i=0;i<2*s->common->num_paths;i++)h=(h^s->hints_done[i])*16777619u;
 return h;
}
static int errors(const bool *values,int n) {int result=0;for(int i=0;i<n;i++)result+=values[i];return result;}
static void point(const game_state *s,game_drawstate *ds,int monster,int xy[2]) {
 for(int gy=1;gy<=s->common->params.h;gy++)for(int gx=1;gx<=s->common->params.w;gx++)
  if(s->common->xinfo[gx+gy*(s->common->params.w+2)]==monster) {
   xy[0]=BORDER+gx*TILESIZE+TILESIZE/2+1;xy[1]=BORDER+(gy+1)*TILESIZE+TILESIZE/2+2;return;
  }
 fatal("Original monster point not found");
}
static void emit(const game_state *before,const game_state *after,const int select[2],const int input[2],int mode,int key,const char *move,bool top) {
 printf("{\"select\":[%d,%d],\"input\":[%d,%d],\"mode\":%d,\"key_button\":%d,\"key_index\":%d,\"top_count_touch\":%s,\"move\":",select[0],select[1],input[0],input[1],mode,key,key=='G'?0:key=='V'?1:key=='Z'?2:key=='\b'?3:-1,top?"true":"false");
 if(move)printf("\"%s\"",move);else printf("null");
 printf(",\"before_hash\":%u,\"after_hash\":%u,\"status\":%d,\"solved\":%s,\"cheated\":%s,\"cell_errors\":%d,\"hint_errors\":%d,\"count_errors\":%d}",hash_state(before),hash_state(after),game_status(after),after->solved?"true":"false",after->cheated?"true":"false",errors(after->cell_errors,after->common->wh),errors(after->hint_errors,2*after->common->num_paths),errors(after->count_errors,3));
}
static game_state *key_touch(game_state *state,game_ui *ui,game_drawstate *ds,int r,int key,bool pencil,bool top,bool output) {
 int xy[2],input[2]={0,0};point(state,ds,r,xy);
 char *m=interpret_move(state,ui,ds,xy[0],xy[1],pencil?RIGHT_BUTTON:LEFT_BUTTON);
 if(real(m))fatal("Cell select unexpectedly changed state");
 if(top) {int c=key=='G'?0:key=='V'?1:2;input[0]=COUNT_X(c)+COUNT_W/2;input[1]=COUNT_Y+COUNT_H/2;m=interpret_move(state,ui,ds,input[0],input[1],LEFT_BUTTON);}
 else m=interpret_move(state,ui,ds,0,0,key);
 game_state *next=real(m)?execute_move(state,m):dup_game(state);if(!next)fatal("Original interpreter move rejected");
 if(output)emit(state,next,xy,input,pencil?1:0,key,real(m)?m:NULL,top);
 if(real(m))sfree(m);return next;
}
static int first_mutable(const game_state *s) {for(int r=0;r<s->common->num_total;r++)if(!s->common->fixed[r])return r;fatal("No mutable monster slot");return -1;}
static void require_honest(const game_state *s) {if(s->cheated)fatal("Ordinary input marked cheated");}
/* Capture actual original drawing text and inspect all original monster sprites. */
void frontend_default_colour(frontend *f,float *out) {(void)f;out[0]=out[1]=out[2]=0.9F;}
void activate_timer(frontend *f) {(void)f;}
void deactivate_timer(frontend *f) {(void)f;}
static int text_count;
static void record_text(drawing *d,int x,int y,int type,int size,int align,int colour,const char *str) {
 (void)d;(void)type;printf("%s[%d,%d,%d,%d,%d,\"",text_count++?",":"",x,y,size,align,colour);
 for(const unsigned char *p=(const unsigned char*)str;*p;p++) {if(*p=='"'||*p=='\\')putchar('\\');putchar(*p>=32&&*p<127?*p:'?');}printf("\"]");
}
static void no_rect(drawing*d,int x,int y,int w,int h,int c) {(void)d;(void)x;(void)y;(void)w;(void)h;(void)c;}
static void no_line(drawing*d,int x,int y,int w,int h,int c) {no_rect(d,x,y,w,h,c);}
static void no_circle(drawing*d,int x,int y,int r,int f,int o) {(void)d;(void)x;(void)y;(void)r;(void)f;(void)o;}
static void no_poly(drawing*d,const int*p,int n,int f,int o) {(void)d;(void)p;(void)n;(void)f;(void)o;}
static void no_clip(drawing*d,int x,int y,int w,int h) {(void)d;(void)x;(void)y;(void)w;(void)h;}
static void no_void(drawing*d) {(void)d;}
static void no_status(drawing*d,const char*s) {(void)d;(void)s;}
static char *ascii_fallback(drawing*d,const char*const*s,int n) {(void)d;for(int i=0;i<n;i++) {bool ok=true;for(const unsigned char*p=(const unsigned char*)s[i];*p;p++)if(*p>=127)ok=false;if(ok)return dupstr(s[i]);}return dupstr(n?s[n-1]:"");}
static const drawing_api original_drawing={.version=1,.draw_text=record_text,.draw_rect=no_rect,.draw_line=no_line,.draw_circle=no_circle,.draw_polygon=no_poly,.draw_update=no_clip,.clip=no_clip,.unclip=no_void,.start_draw=no_void,.end_draw=no_void,.status_bar=no_status,.text_fallback=ascii_fallback};
static unsigned colour565(float *palette,int i) {unsigned r=(unsigned)(palette[3*i]*255+.5F),g=(unsigned)(palette[3*i+1]*255+.5F),b=(unsigned)(palette[3*i+2]*255+.5F);return((r>>3)<<11)|((g>>2)<<5)|(b>>3);}
static void print_render(game_state *state,game_ui *ui,int tile) {
 int colours;float *palette=game_colours(NULL,&colours);drawing *dr=drawing_new(&original_drawing,NULL,NULL);game_drawstate *ds=game_new_drawstate(dr,state);game_set_size(dr,ds,&state->common->params,tile);
 printf(",\"render\":{\"palette\":[");for(int i=0;i<colours;i++)printf("%s%u",i?",":"",colour565(palette,i));printf("],\"texts\":[");text_count=0;game_redraw(dr,ds,NULL,state,0,ui,0,0);printf("],\"sprites\":[");
 int count=0;for(int c=0;c<3;c++)printf("%s[%d,%d,%d,%d]",count++?",":"",COUNT_X(c)+COUNT_MSIZE/2,COUNT_Y+COUNT_H/2,COUNT_MSIZE,c==0?COL_GHOST:c==1?COL_VAMPIRE:COL_ZOMBIE);
 for(int r=0;r<state->common->num_total;r++)if(state->guess[r]==1||state->guess[r]==2||state->guess[r]==4) {int xy[2];point(state,ds,r,xy);printf(",[%d,%d,%d,%d]",xy[0]-1,xy[1]-2,tile,state->guess[r]==1?COL_GHOST:state->guess[r]==2?COL_VAMPIRE:COL_ZOMBIE);}
 printf("]}");game_free_drawstate(dr,ds);drawing_free(dr);sfree(palette);
}

int main(int argc,char **argv) {
 if(argc<4)fatal("Usage: oracle game-id|--generate tilesize info|ghost|vampire|zombie|clear|pencilG|pencilV|pencilZ|topGhost|topVampire|topZombie|mirror|clue|errors|solution [prior-move... or generation-seed]");
 bool generated=!strcmp(argv[1],"--generate");game_params *p=default_params();char *id,*desc;
 if(generated) {const char *seed=argc>4?argv[4]:"native-fixture";random_state *rs=random_new(seed,strlen(seed));char *aux=NULL;desc=new_game_desc(p,rs,&aux,false);random_free(rs);sfree(aux);char *shortparams=encode_params(p,false);id=snewn(strlen(shortparams)+strlen(desc)+2,char);sprintf(id,"%s:%s",shortparams,desc);sfree(desc);sfree(shortparams);}
 else id=dupstr(argv[1]);
 char *idcopy=dupstr(id);desc=strchr(idcopy,':');if(!desc)fatal("Invalid original ID");*desc++=0;decode_params(p,idcopy);
 if(validate_params(p,true)||validate_desc(p,desc))fatal("Invalid original parameters or description");char *canonical=encode_params(p,false);if(strcmp(canonical,idcopy))fatal("Noncanonical short parameters");sfree(canonical);
 game_state *opening=new_game(NULL,p,desc),*state=dup_game(opening);int ts=atoi(argv[2]);if(ts<10||ts>64)fatal("Invalid test tilesize");
 if(!generated)for(int i=4;i<argc;i++) {if(strchr(argv[i],'S'))fatal("Solver injection forbidden for prior moves");game_state *next=execute_move(state,argv[i]);if(!next)fatal("Invalid original prior move");free_game(state);state=next;require_honest(state);}
 game_drawstate draw={0};game_drawstate *ds=&draw;draw.tilesize=ts;draw.w=p->w;draw.h=p->h;draw.count_style=COUNT_STYLE_DEFAULT;calculate_count_layout(ds);game_ui *ui=new_ui(state);
 const char *action=argv[3];int bw,bh;game_compute_size(p,ts,ui,&bw,&bh);
 printf("{\"id\":\"%s\",\"width\":%d,\"height\":%d,\"difficulty\":%d,\"tilesize\":%d,\"board_size\":[%d,%d],\"monster_counts\":[%d,%d,%d],\"monsters\":%d,\"input_keys\":[71,86,90,8],\"action\":\"%s\",",id,p->w,p->h,p->diff,ts,bw,bh,state->common->num_ghosts,state->common->num_vampires,state->common->num_zombies,state->common->num_total,action);
 if(!strcmp(action,"info")) {
  printf("\"cells\":[");int c=0;for(int gy=1;gy<=p->h;gy++)for(int gx=1;gx<=p->w;gx++) {int index=gx+gy*(p->w+2);if(c++)printf(",");printf("{\"grid\":[%d,%d],\"point\":[%d,%d],\"monster\":%d,\"fixed\":%s}",gx,gy,BORDER+gx*TILESIZE+TILESIZE/2+1,BORDER+(gy+1)*TILESIZE+TILESIZE/2+2,state->common->xinfo[index],state->common->xinfo[index]>=0&&state->common->fixed[state->common->xinfo[index]]?"true":"false");}printf("]");
 }else if(!strcmp(action,"mirror")) {
  int xy[2]={0,0};for(int gy=1;gy<=p->h&&!xy[0];gy++)for(int gx=1;gx<=p->w;gx++)if(state->common->xinfo[gx+gy*(p->w+2)]==-1) {xy[0]=BORDER+gx*TILESIZE+TILESIZE/2+1;xy[1]=BORDER+(gy+1)*TILESIZE+TILESIZE/2+2;break;}
  if(!xy[0])fatal("No original mirror fixture");char *m=interpret_move(state,ui,ds,xy[0],xy[1],LEFT_BUTTON);if(real(m))fatal("Mirror select changed original state");m=interpret_move(state,ui,ds,0,0,'G');if(real(m))fatal("Mirror key changed original state");int zero[2]={0,0};printf("\"prediction\":");emit(state,state,xy,zero,0,'G',NULL,false);
 }else if(!strcmp(action,"clue")) {
  int xy[2]={BORDER+TILESIZE+TILESIZE/2+1,BORDER+TILESIZE+TILESIZE/2+2};char *m=interpret_move(state,ui,ds,xy[0],xy[1],LEFT_BUTTON);if(!real(m)||m[0]!='D')fatal("Original clue touch rejected");game_state *next=execute_move(state,m);if(!next)fatal("Original clue move rejected");printf("\"prediction\":");emit(state,next,xy,xy,0,0,m,false);sfree(m);free_game(state);state=next;
 }else if(!strcmp(action,"solution")||!strcmp(action,"errors")) {
  const char *error=NULL;char *solution=solve_game(opening,opening,NULL,&error);if(!solution||solution[0]!='S')fatal("Original solver failed: %s",error?error:"unknown");game_state *target=execute_move(opening,solution);sfree(solution);if(!target||!target->cheated)fatal("Invalid native-only solver target");
  bool solving=!strcmp(action,"solution");int counts[3]={state->common->num_ghosts,state->common->num_vampires,state->common->num_zombies};int lowest=0;for(int i=1;i<3;i++)if(counts[i]<counts[lowest])lowest=i;int count=0;printf("\"touches\":[");
  for(int r=0;r<state->common->num_total;r++)if(!state->common->fixed[r] && (!solving||state->guess[r]!=target->guess[r])) {
   int wanted=solving?target->guess[r]:1<<lowest;int key=wanted==1?'G':wanted==2?'V':wanted==4?'Z':0;if(!key)fatal("Native solver target not filled");if(count++)printf(",");game_state *next=key_touch(state,ui,ds,r,key,false,false,true);if(hash_state(next)==hash_state(state))fatal("Original ordinary key did not change state");require_honest(next);free_game(state);state=next;
  }
  printf("],\"ordinary_moves\":%d,\"target_solver_is_native_only\":true,\"target_cheated\":true,\"status\":%d,\"solved\":%s,\"cheated\":%s,\"count_errors\":%d,\"hint_errors\":%d",count,game_status(state),state->solved?"true":"false",state->cheated?"true":"false",errors(state->count_errors,3),errors(state->hint_errors,2*state->common->num_paths));
  if(solving) {if(!state->solved||state->cheated||game_status(state)!=1||!check_numbers_draw(state,state->guess))fatal("Ordinary monster keys did not honestly solve");for(int i=0;i<state->common->num_paths;i++)if(!check_path_solution(state,i))fatal("Original ray clue mismatch");if(errors(state->count_errors,3)||errors(state->hint_errors,2*state->common->num_paths))fatal("Honest solution has original errors");}
  else if(state->solved||!errors(state->count_errors,3))fatal("Original count-error fixture failed");free_game(target);
 }else {
  bool pencil=!strncmp(action,"pencil",6),top=!strncmp(action,"top",3);int key=!strcmp(action,"clear")?'\b':!strcmp(action,"ghost")||!strcmp(action,"pencilG")||!strcmp(action,"topGhost")?'G':!strcmp(action,"vampire")||!strcmp(action,"pencilV")||!strcmp(action,"topVampire")?'V':!strcmp(action,"zombie")||!strcmp(action,"pencilZ")||!strcmp(action,"topZombie")?'Z':0;if(!key)fatal("Unknown native action");int r=first_mutable(state);if(pencil&&state->guess[r]!=7)fatal("Pencil requires original blank cell");printf("\"prediction\":");game_state *next=key_touch(state,ui,ds,r,key,pencil,top,true);require_honest(next);free_game(state);state=next;
 }
 print_render(state,ui,ts);printf("}\n");free_ui(ui);free_game(state);free_game(opening);free_params(p);sfree(id);sfree(idcopy);return 0;
}
