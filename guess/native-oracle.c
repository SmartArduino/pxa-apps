/* SPDX-License-Identifier: MIT
 * Test-only access to the unchanged official Guess engine. Not a Guest input API. */
#include <stdarg.h>
#include "../common/retro-games/puzzles/vendor/guess.c"
struct blitter { int w,h; };
blitter *blitter_new(drawing *dr,int w,int h) {(void)dr;blitter*b=snew(blitter);b->w=w;b->h=h;return b;}
void blitter_free(drawing *dr,blitter*b) {(void)dr;sfree(b);}
void fatal(const char*f,...) {va_list a;va_start(a,f);vfprintf(stderr,f,a);va_end(a);fputc('\n',stderr);exit(2);}
void frontend_default_colour(frontend*f,float*c) {(void)f;c[0]=c[1]=c[2]=0.9F;}
static game_state *from_id(const char*id) {
 char*b=dupstr(id),*d=strchr(b,':');if(!d)fatal("Missing description");*d++=0;
 game_params*p=default_params();decode_params(p,b);const char*e=validate_params(p,true);
 if(!e)e=validate_desc(p,d);if(e)fatal("Original validation: %s",e);
 game_state*s=new_game(NULL,p,d);free_params(p);sfree(b);return s;
}
static bool is_move(char*m) {return m && m!=MOVE_UI_UPDATE && m!=MOVE_NO_EFFECT && m!=MOVE_UNUSED;}
static void ints(const int*a,int n) {putchar('[');for(int i=0;i<n;i++)printf("%s%d",i?",":"",a[i]);putchar(']');}
static void report(game_state*s,game_ui*u,game_drawstate*ds,char**emitted,int n) {
 printf("{\"npegs\":%d,\"ncolours\":%d,\"nguesses\":%d,\"next_go\":%d,\"status\":%d,\"solution\":",s->params.npegs,s->params.ncolours,s->params.nguesses,s->next_go,game_status(s));ints(s->solution->pegs,s->params.npegs);
 printf(",\"current\":");ints(u->curr_pegs->pegs,s->params.npegs);
 printf(",\"holds\":[");for(int i=0;i<s->params.npegs;i++)printf("%s%d",i?",":"",u->holds[i]);
 printf("],\"rows\":[");for(int i=0;i<s->params.nguesses;i++){printf("%s{\"pegs\":",i?",":"");ints(s->guesses[i]->pegs,s->params.npegs);printf(",\"feedback\":");ints(s->guesses[i]->feedback,s->params.npegs);putchar('}');}
 printf("],\"palette\":[");int nc;float*c=game_colours(NULL,&nc);for(int i=0;i<s->params.ncolours;i++){int r=(int)(c[(COL_1+i)*3]*255+.5),g=(int)(c[(COL_1+i)*3+1]*255+.5),b=(int)(c[(COL_1+i)*3+2]*255+.5);printf("%s[%d,%d,%d]",i?",":"",(r>>3)*255/31,(g>>2)*255/63,(b>>3)*255/31);}sfree(c);
 printf("],\"geometry\":{\"colx\":%d,\"coly\":%d,\"guessx\":%d,\"guessy\":%d,\"solnx\":%d,\"solny\":%d,\"pegrad\":%d,\"offset\":%d,\"hintx\":%d,\"hinty\":%d,\"hintw\":%d},\"moves\":[",ds->colx,ds->coly,ds->guessx,ds->guessy,ds->solnx,ds->solny,ds->pegrad,PEGOFF,HINT_X(0),HINT_Y(0),HINT_W);
 for(int i=0;i<n;i++)printf("%s\"%s\"",i?",":"",emitted[i]);puts("]}");
}
static void self_test(void) {
 game_params*p=default_params();random_state*r=random_new("guess-native-test",17);char*d=new_game_desc(p,r,NULL,false);game_state*s=new_game(NULL,p,d);
 assert(execute_move(s,"G0,0,0,0")==NULL);assert(execute_move(s,"malformed")==NULL);
 char win[128];snprintf(win,sizeof(win),"G%d_,%d,%d,%d",s->solution->pegs[0],s->solution->pegs[1],s->solution->pegs[2],s->solution->pegs[3]);
 game_state*t=execute_move(s,win);assert(t && game_status(t)==1 && t->holds[0]);assert(execute_move(t,win)==NULL);free_game(t);
 int wrong[4];for(int i=0;i<4;i++)wrong[i]=s->solution->pegs[i]%p->ncolours+1;
 char loss[128];snprintf(loss,sizeof(loss),"G%d,%d,%d,%d",wrong[0],wrong[1],wrong[2],wrong[3]);
 for(int i=0;i<10;i++){t=execute_move(s,loss);assert(t);free_game(s);s=t;assert(game_status(s)==(i==9?-1:0));}
 assert(execute_move(s,loss)==NULL);free_game(s);sfree(d);random_free(r);free_params(p);
 pegrow a=new_pegrow(4),b=new_pegrow(4);int av[4]={1,1,2,3},bv[4]={1,2,1,4};memcpy(a->pegs,av,sizeof av);memcpy(b->pegs,bv,sizeof bv);assert(mark_pegs(a,b,6)==1);assert(a->feedback[0]==1 && a->feedback[1]==2 && a->feedback[2]==2 && a->feedback[3]==0);free_pegrow(a);free_pegrow(b);
 puts("PASS: original duplicate-aware feedback, invalid colour/syntax, held guesses, four exact pegs win, ten wrong guesses lose, completed input rejected");
}
int main(int argc,char**argv) {
 if(argc==2 && !strcmp(argv[1],"--self-test")){self_test();return 0;}
 if(argc<3)fatal("Usage: oracle ID tile [D:x,y M:x,y U:x,y R:x,y ...]");
 game_state*s=from_id(argv[1]);game_ui*u=new_ui(s);game_drawstate*d=game_new_drawstate(NULL,s);game_set_size(NULL,d,&s->params,atoi(argv[2]));
 char**emitted=snewn(argc,char*);int n=0;
 for(int i=3;i<argc;i++){char phase;int x,y;if(sscanf(argv[i],"%c:%d,%d",&phase,&x,&y)!=3)fatal("Bad test event");int b=phase=='D'?LEFT_BUTTON:phase=='M'?LEFT_DRAG:phase=='U'?LEFT_RELEASE:phase=='R'?RIGHT_BUTTON:-1;if(b<0)fatal("Bad button");
 char*m=interpret_move(s,u,d,x,y,b);if(is_move(m)){game_state*t=execute_move(s,m);if(!t)fatal("Original rejected input move");game_changed_state(u,s,t);free_game(s);s=t;emitted[n++]=m;}}
 report(s,u,d,emitted,n);for(int i=0;i<n;i++)sfree(emitted[i]);sfree(emitted);game_free_drawstate(NULL,d);free_ui(u);free_game(s);return 0;
}
