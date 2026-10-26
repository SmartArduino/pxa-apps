/* SPDX-License-Identifier: MIT
 * Test-only original-C oracle, never shipped in the Guest. */
#include <stdarg.h>
#include "../common/retro-games/puzzles/vendor/lightup.c"
void fatal(const char *format, ...) {va_list a;va_start(a,format);vfprintf(stderr,format,a);va_end(a);fputc('\n',stderr);exit(2);}
void frontend_default_colour(frontend *f,float *out){(void)f;out[0]=out[1]=out[2]=0.9F;}
static game_params *params;
static game_state *from_id(const char *id){char *copy=dupstr(id),*p=strchr(copy,':');if(!p)fatal("Missing ID");*p++=0;
 params=default_params();decode_params(params,copy);const char *e=validate_params(params,true);if(!e)e=validate_desc(params,p);
 if(e)fatal("Original validation: %s",e);game_state *s=new_game(NULL,params,p);sfree(copy);return s;}
static game_state *step(game_state *s,const char *m){game_state *n=execute_move(s,m);if(!n)fatal("Original rejection: %s",m);free_game(s);return n;}
static unsigned colour565(float *palette,int c){unsigned r=(unsigned)(palette[3*c]*255+.5F),g=(unsigned)(palette[3*c+1]*255+.5F),b=(unsigned)(palette[3*c+2]*255+.5F);return ((r>>3)<<11)|((g>>2)<<5)|(b>>3);}
static void output(game_state *s){game_ui *ui=new_ui(s);game_drawstate *ds=game_new_drawstate(NULL,s);int count;float *p=game_colours(NULL,&count);
 printf("{\"w\":%d,\"h\":%d,\"status\":%d,\"nlights\":%d,\"cells\":[",s->w,s->h,game_status(s),s->nlights);
 for(int i=0;i<s->w*s->h;i++){unsigned f=tile_flags(ds,s,ui,i%s->w,i/s->w,false);int bg=(f&DF_BLACK)?COL_BLACK:(f&DF_LIT)?COL_LIT:COL_BACKGROUND;
 int center=(f&DF_LIGHT)?((f&DF_OVERLAP)?COL_ERROR:COL_LIGHT):(f&DF_IMPOSSIBLE)&&(!(f&DF_LIT)||ui->draw_blobs_when_lit)?COL_BLACK:bg;
 printf("%s[%u,%d,%u,%u]",i?",":"",s->flags[i],s->lights[i],colour565(p,bg),colour565(p,center));}
 puts("]}");sfree(p);game_free_drawstate(NULL,ds);free_ui(ui);}
static void self_test(void){game_state *s=from_id("3x3:i");assert(!grid_correct(s)&&s->nlights==0);
 assert(execute_move(s,"L-1,0")==NULL&&execute_move(s,"L3,0")==NULL&&execute_move(s,"invalid")==NULL);
 s=step(s,"L0,0");int lit=0;for(int i=0;i<9;i++)lit+=s->lights[i]>0;assert(lit==5&&s->nlights==1&&(s->flags[0]&F_LIGHT));
 s=step(s,"L1,1;L2,2");assert(s->nlights==3&&grid_correct(s)&&game_status(s)==1&&!s->used_solve);
 free_game(s);free_params(params);s=from_id("3x3:i");s=step(s,"I0,0");assert((s->flags[0]&F_IMPOSSIBLE)&&s->nlights==0);
 game_ui *ui=new_ui(s);game_drawstate *ds=game_new_drawstate(NULL,s);game_set_size(NULL,ds,params,32);
 assert(interpret_move(s,ui,ds,32,32,LEFT_BUTTON)==MOVE_NO_EFFECT);
 char *m=interpret_move(s,ui,ds,32,32,RIGHT_BUTTON);assert(!strcmp(m,"I0,0"));sfree(m);
 game_free_drawstate(NULL,ds);free_ui(ui);s=step(s,"I0,0;L0,0;L2,0");assert(s->lights[0]==2&&s->lights[2]==2&&!grid_correct(s));
 free_game(s);free_params(params);puts("PASS: original ray illumination, five-cell coverage, three-bulb uncheated victory, overlapping-bulb rejection, impossible mark and Left/Right exclusion, invalid bounds/syntax");}
int main(int argc,char **argv){if(argc==2&&!strcmp(argv[1],"--self-test")){self_test();return 0;}
 if(argc>=7&&!strcmp(argv[1],"--input")){game_state *s=from_id(argv[2]);for(int i=7;i<argc;i++)s=step(s,argv[i]);game_ui *ui=new_ui(s);game_drawstate *ds=game_new_drawstate(NULL,s);
 game_set_size(NULL,ds,params,atoi(argv[3]));char *m=interpret_move(s,ui,ds,atoi(argv[4]),atoi(argv[5]),!strcmp(argv[6],"R")?RIGHT_BUTTON:LEFT_BUTTON);
 if(!m||m==MOVE_UI_UPDATE||m==MOVE_NO_EFFECT||m==MOVE_UNUSED)fatal("Original input has no move");puts(m);sfree(m);game_free_drawstate(NULL,ds);free_ui(ui);free_game(s);free_params(params);return 0;}
 if(argc<2)fatal("Usage: native-oracle ID [moves]");game_state *s=from_id(argv[1]);for(int i=2;i<argc;i++)s=step(s,argv[i]);output(s);free_game(s);free_params(params);return 0;}
