/* Test-only original C oracle; never compiled into the Guest. */
#include "../common/retro-games/puzzles/vendor/tents.c"
#include <stdio.h>
static void ints(const int *v,int n){printf("[");for(int i=0;i<n;i++)printf("%s%d",i?",":"",v[i]);printf("]");}
static void dump(const game_state *s){printf("{\"grid\":[");for(int i=0;i<s->p.w*s->p.h;i++)printf("%s%d",i?",":"",s->grid[i]);printf("],\"status\":%d}",game_status(s));}
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
 int w=s->p.w,h=s->p.h;
 int a=0;while(a<w*h && s->grid[a]!=BLANK)a++;assert(a<w*h);int b=a+1;while(b<w*h && s->grid[b]!=BLANK)b++;assert(b<w*h);int lx=COORD(a%w)+TILESIZE/2,ly=COORD(a/w)+TILESIZE/2,rx=COORD(b%w)+TILESIZE/2,ry=COORD(b/w)+TILESIZE/2;
 printf("{\"width\":%d,\"height\":%d,\"left\":[%d,%d],\"right\":[%d,%d],\"initial\":",w,h,lx,ly,rx,ry);dump(s);
 assert(execute_move(s,"BAD") == NULL);
 char *m1=click(&s,ui,ds,lx,ly,LEFT_BUTTON);printf(",\"move1\":\"%s\",\"after1\":",m1);dump(s);assert(s->grid[a]==TENT);
 char *m2=click(&s,ui,ds,rx,ry,RIGHT_BUTTON);printf(",\"move2\":\"%s\",\"after2\":",m2);dump(s);assert(s->grid[a]==TENT && s->grid[b]==NONTENT);
 printf(",\"invalid_move_rejected\":true,\"rule_checks\":true}\n");
 sfree(m1);sfree(m2);free_ui(ui);game_free_drawstate(NULL,ds);free_game(s);free_params(p);sfree(id);return 0;
}
