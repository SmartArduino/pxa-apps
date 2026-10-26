/* Test-only original C oracle; never compiled into the Guest. */
#include "../common/retro-games/puzzles/vendor/palisade.c"
#include <stdio.h>
static void ints(const int *v,int n){printf("[");for(int i=0;i<n;i++)printf("%s%d",i?",":"",v[i]);printf("]");}
static void dump(const game_state *s){int w=s->shared->params.w,h=s->shared->params.h;printf("{\"borders\":[");for(int i=0;i<w*h;i++)printf("%s%d",i?",":"",s->borders[i]);printf("],\"status\":%d}",game_status(s));}
static char *click(game_state **s,game_ui *ui,game_drawstate *ds,int x,int y,int button){
 char *answer=NULL;
 for(int phase=0;phase<2;phase++){
  char *move=interpret_move(*s,ui,ds,x,y,button+(phase?6:0));
  if(move && move!=MOVE_UNUSED && move!=MOVE_UI_UPDATE && move!=MOVE_NO_EFFECT){
   assert(!answer);answer=dupstr(move);game_state *next=execute_move(*s,move);assert(next);free_game(*s);*s=next;sfree(move);
  }
 }
 assert(answer);return answer;
}
int main(int argc,char **argv){
 assert(argc==3);char *id=dupstr(argv[1]),*desc=strchr(id,':');assert(desc);*desc++=0;
 game_params *p=default_params();decode_params(p,id);assert(!validate_params(p,false));assert(!validate_desc(p,desc));
 game_state *s=new_game(NULL,p,desc);game_drawstate *ds=game_new_drawstate(NULL,s);game_set_size(NULL,ds,p,atoi(argv[2]));game_ui *ui=new_ui(s);
 int w=s->shared->params.w,h=s->shared->params.h;
 int lx=MARGIN+TILESIZE-2,ly=MARGIN+TILESIZE/2,rx=lx,ry=ly+TILESIZE;
 printf("{\"width\":%d,\"height\":%d,\"left\":[%d,%d],\"right\":[%d,%d],\"initial\":",w,h,lx,ly,rx,ry);dump(s);
 assert(execute_move(s,"BAD") == NULL);
 char *m1=click(&s,ui,ds,lx,ly,LEFT_BUTTON);printf(",\"move1\":\"%s\",\"after1\":",m1);dump(s);assert((s->borders[0]&BORDER_R) && (s->borders[1]&BORDER_L));
 char *m2=click(&s,ui,ds,rx,ry,RIGHT_BUTTON);printf(",\"move2\":\"%s\",\"after2\":",m2);dump(s);assert((s->borders[w]&DISABLED(BORDER_R)) && (s->borders[w+1]&DISABLED(BORDER_L)));
 printf(",\"invalid_move_rejected\":true,\"rule_checks\":true}\n");
 sfree(m1);sfree(m2);free_ui(ui);game_free_drawstate(NULL,ds);free_game(s);free_params(p);sfree(id);return 0;
}
