/* SPDX-License-Identifier: MIT -- test-only original C oracle; never shipped. */
#include <stdarg.h>
#include "../common/retro-games/puzzles/vendor/blackbox.c"
void fatal(const char *format,...){va_list a;va_start(a,format);vfprintf(stderr,format,a);va_end(a);fputc('\n',stderr);exit(2);}
void frontend_default_colour(frontend *f,float *out){(void)f;out[0]=out[1]=out[2]=0.9F;}
static game_params *params;
static game_state *from_id(const char *id){char *copy=dupstr(id),*p=strchr(copy,':');if(!p)fatal("Missing ID");*p++=0;params=default_params();decode_params(params,copy);
 const char *e=validate_params(params,true);if(!e)e=validate_desc(params,p);if(e)fatal("Original validation: %s",e);game_state *s=new_game(NULL,params,p);sfree(copy);return s;}
static game_state *step(game_state *s,const char *m){game_state *n=execute_move(s,m);if(!n)fatal("Original rejection: %s",m);free_game(s);return n;}
static unsigned colour565(float *p,int c){unsigned r=(unsigned)(p[3*c]*255+.5F),g=(unsigned)(p[3*c+1]*255+.5F),b=(unsigned)(p[3*c+2]*255+.5F);return ((r>>3)<<11)|((g>>2)<<5)|(b>>3);}
static void output(game_state *s){int count;float *p=game_colours(NULL,&count);printf("{\"w\":%d,\"h\":%d,\"nballs\":%d,\"nguesses\":%d,\"status\":%d,\"reveal\":%d,\"cells\":[",s->w,s->h,s->nballs,s->nguesses,game_status(s),s->reveal);
 for(int y=1,i=0;y<=s->h;y++)for(int x=1;x<=s->w;x++,i++){unsigned f=GRID(s,x,y);int c=(f&BALL_GUESS)?COL_BALL:(f&BALL_LOCK)?COL_LOCK:COL_COVER;printf("%s[%u,%u]",i?",":"",f,colour565(p,c));}
 printf("],\"exits\":[");for(int i=0;i<s->nlasers;i++)printf("%s%u",i?",":"",s->exits[i]);puts("]}");sfree(p);}
static void self_test(void){game_params *p=default_params();p->w=p->h=3;p->minballs=p->maxballs=1;const char seed[]="native-ray-fixture";random_state *rs=random_new(seed,sizeof(seed));char *aux=NULL,*desc=new_game_desc(p,rs,&aux,false),*encoded=encode_params(p,false);
 char *id=snewn(strlen(encoded)+strlen(desc)+2,char);sprintf(id,"%s:%s",encoded,desc);game_state *s=from_id(id);
 for(int y=1;y<=3;y++)for(int x=1;x<=3;x++)GRID(s,x,y)=0;GRID(s,2,1)=BALL_CORRECT;
 assert(laser_exit(s,1)==LASER_HIT&&laser_exit(s,0)==LASER_REFLECT&&laser_exit(s,2)==LASER_REFLECT);
 int exit=laser_exit(s,5);assert(exit>=0&&exit<s->nlasers);s=step(s,"F1");assert(s->exits[1]==LASER_HIT&&execute_move(s,"F1")==NULL);
 s=step(s,"F0");assert(s->exits[0]==LASER_REFLECT);assert(execute_move(s,"T0,1")==NULL&&execute_move(s,"F12")==NULL&&execute_move(s,"invalid")==NULL);
 s=step(s,"LB2,2");game_ui *ui=new_ui(s);game_drawstate *ds=game_new_drawstate(NULL,s);game_set_size(NULL,ds,params,24);
 assert(interpret_move(s,ui,ds,72,72,LEFT_BUTTON)==NULL);char *m=interpret_move(s,ui,ds,72,72,RIGHT_BUTTON);assert(!strcmp(m,"LB2,2"));sfree(m);game_free_drawstate(NULL,ds);free_ui(ui);
 s=step(s,"T2,1");assert(s->nguesses==1);s=step(s,"R");assert(game_status(s)==1&&s->reveal&&s->nwrong==0&&s->nmissed==0);
 free_game(s);free_params(params);free_params(p);random_free(rs);sfree(desc);sfree(aux);sfree(encoded);sfree(id);
 puts("PASS: original direct hit, immediate reflection, traversing ray, ray reuse rejection, ball guess, lock/Left exclusion, invalid bounds/syntax, exact one-ball uncheated reveal victory");}
int main(int argc,char **argv){if(argc==2&&!strcmp(argv[1],"--self-test")){self_test();return 0;}
 if(argc>=7&&!strcmp(argv[1],"--input")){game_state *s=from_id(argv[2]);for(int i=7;i<argc;i++)s=step(s,argv[i]);game_ui *ui=new_ui(s);game_drawstate *ds=game_new_drawstate(NULL,s);game_set_size(NULL,ds,params,atoi(argv[3]));
 char *m=interpret_move(s,ui,ds,atoi(argv[4]),atoi(argv[5]),!strcmp(argv[6],"R")?RIGHT_BUTTON:LEFT_BUTTON);if(!m||m==MOVE_UI_UPDATE||m==MOVE_NO_EFFECT||m==MOVE_UNUSED)fatal("Original input has no move");puts(m);sfree(m);game_free_drawstate(NULL,ds);free_ui(ui);free_game(s);free_params(params);return 0;}
 if(argc<2)fatal("Usage: native-oracle ID [moves]");game_state *s=from_id(argv[1]);for(int i=2;i<argc;i++)s=step(s,argv[i]);output(s);free_game(s);free_params(params);return 0;}
