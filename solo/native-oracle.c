/* SPDX-License-Identifier: MIT. Native-only original Solo input research. */
#include <stdarg.h>
#include "../common/retro-games/puzzles/vendor/solo.c"
void fatal(const char*f,...){va_list a;va_start(a,f);vfprintf(stderr,f,a);va_end(a);fputc('\n',stderr);exit(2);}
static bool actual(char*m){return m && m!=MOVE_UI_UPDATE && m!=MOVE_NO_EFFECT && m!=MOVE_UNUSED;}
static char* select_cell(const game_state*s,game_ui*u,game_drawstate*ds,int index,int button){
 int x=index%s->cr,y=index/s->cr;return interpret_move(s,u,ds,BORDER+x*TILE_SIZE+TILE_SIZE/2,BORDER+y*TILE_SIZE+TILE_SIZE/2,button);
}
static game_state* key_move(game_state*s,game_ui*u,game_drawstate*d,int key,char type,int index,int value){
 char*m=interpret_move(s,u,d,0,0,key);assert(actual(m));char expected[80];snprintf(expected,sizeof(expected),"%c%d,%d,%d",type,index%s->cr,index/s->cr,value);assert(!strcmp(m,expected));game_state*t=execute_move(s,m);assert(t);sfree(m);free_game(s);return t;
}
static void run_fixture(game_params*p,int tile,const char*seed){
 assert(!validate_params(p,true));int order=p->c*p->r,nkeys,w,h;key_label*keys=game_request_keys(p,&nkeys);assert(nkeys==order+1);for(int i=0;i<order;i++)assert(keys[i].button=='1'+i);assert(keys[order].button=='\b');sfree(keys);
 random_state*rs=random_new(seed,strlen(seed));char*aux=NULL;char*desc=new_game_desc(p,rs,&aux,false);assert(!validate_desc(p,desc));game_state*opening=new_game(NULL,p,desc);assert(!opening->completed&&!opening->cheated);game_ui*u=new_ui(opening);assert(!u->pencil_keep_highlight);game_drawstate d={0};game_set_size(NULL,&d,p,tile);game_drawstate*ds=&d;game_compute_size(p,tile,u,&w,&h);
 int empty=-1,given=-1;for(int i=0;i<order*order;i++){if(!opening->immutable[i]&&empty<0)empty=i;if(opening->immutable[i]&&given<0)given=i;}assert(empty>=0&&given>=0);
 assert(interpret_move(opening,u,ds,0,0,'1')==NULL);
 assert(select_cell(opening,u,ds,empty,LEFT_BUTTON)==MOVE_UI_UPDATE && u->hshow && !u->hpencil);
 assert(select_cell(opening,u,ds,empty,LEFT_BUTTON)==MOVE_UI_UPDATE && !u->hshow);
 select_cell(opening,u,ds,empty,LEFT_BUTTON);assert(interpret_move(opening,u,ds,-100,-100,LEFT_BUTTON)==MOVE_UI_UPDATE && !u->hshow);assert(interpret_move(opening,u,ds,0,0,'1')==NULL);
 select_cell(opening,u,ds,given,LEFT_BUTTON);assert(!u->hshow&&interpret_move(opening,u,ds,0,0,'1')==NULL);
 game_state*s=dup_game(opening);select_cell(s,u,ds,empty,RIGHT_BUTTON);assert(u->hshow&&u->hpencil);s=key_move(s,u,ds,'1','P',empty,1);assert(s->pencil[empty*order]&&!s->grid[empty]&&!u->hshow);
 select_cell(s,u,ds,empty,RIGHT_BUTTON);s=key_move(s,u,ds,'1','P',empty,1);assert(!s->pencil[empty*order]);
 select_cell(s,u,ds,empty,RIGHT_BUTTON);s=key_move(s,u,ds,'2','P',empty,2);assert(s->pencil[empty*order+1]);select_cell(s,u,ds,empty,LEFT_BUTTON);s=key_move(s,u,ds,'\b','R',empty,0);assert(!s->grid[empty]&&!s->pencil[empty*order+1]&&!u->hshow);
 select_cell(s,u,ds,empty,LEFT_BUTTON);s=key_move(s,u,ds,'1','R',empty,1);assert(s->grid[empty]==1&&!u->hshow);select_cell(s,u,ds,empty,RIGHT_BUTTON);assert(!u->hshow);select_cell(s,u,ds,empty,LEFT_BUTTON);s=key_move(s,u,ds,'\b','R',empty,0);assert(!s->grid[empty]);free_game(s);
 const char*err=NULL;char*solution=solve_game(opening,opening,NULL,&err);assert(solution&&!err&&solution[0]=='S');game_state*target=execute_move(opening,solution);assert(target&&target->completed&&target->cheated);s=dup_game(opening);int entries=0;float final_flash=0;
 for(int i=0;i<order*order;i++)if(!s->immutable[i]){select_cell(s,u,ds,i,LEFT_BUTTON);assert(u->hshow&&!u->hpencil);char*m=interpret_move(s,u,ds,0,0,'0'+target->grid[i]);assert(actual(m)&&m[0]=='R'&&!u->hshow);game_state*t=execute_move(s,m);assert(t&&!t->cheated);final_flash=game_flash_length(s,t,1,u);sfree(m);free_game(s);s=t;entries++;}
 assert(s->completed&&!s->cheated&&game_status(s)==1&&check_valid(order,s->blocks,s->kblocks,s->kgrid,s->xtype,s->grid)&&final_flash==FLASH_TIME&&game_anim_length(opening,s,1,u)==0);
 printf("PASS original Solo: order=%d tile=%d board=%dx%d keys=%d; immutable/no-selection/outside/toggle rejection, pencil toggle+clear, ink+erase and filled Right rejection; %d ordinary R entries completed with cheated=false and original flash %.2fs\n",order,tile,w,h,nkeys,entries,(double)final_flash);
 free_game(s);free_game(target);sfree(solution);free_ui(u);free_game(opening);sfree(aux);sfree(desc);random_free(rs);
}
static void self_test(void){game_params*p=default_params();assert(p->c==3&&p->r==3);run_fixture(p,27,"solo-default-native");free_params(p);char*name;assert(game_fetch_preset(0,&name,&p)&&p->c==2&&p->r==2);run_fixture(p,48,"solo-preset-native");sfree(name);free_params(p);}
void frontend_default_colour(frontend*f,float*out){(void)f;out[0]=out[1]=out[2]=.9F;}
void activate_timer(frontend*f){(void)f;}void deactivate_timer(frontend*f){(void)f;}
void get_random_seed(void**s,int*n){*n=8;*s=dupstr("testseed");}
static game_params*op;static game_state*os;static game_ui*ou;static game_drawstate*ods;static char*last_move;static int move_count,oracle_tile;
static void load(const char*id,int tile){char*b=dupstr(id),*desc=strchr(b,':');if(!desc)fatal("Missing ID");*desc++=0;op=default_params();decode_params(op,b);const char*e=validate_params(op,true);if(!e)e=validate_desc(op,desc);if(e)fatal("Original validation: %s",e);oracle_tile=tile;os=new_game(NULL,op,desc);ou=new_ui(os);ods=game_new_drawstate(NULL,os);game_set_size(NULL,ods,op,tile);sfree(b);}
static void clean(void){sfree(last_move);game_free_drawstate(NULL,ods);free_ui(ou);free_game(os);free_params(op);}
static void input(int x,int y,int button){char*m=interpret_move(os,ou,ods,x,y,button);if(actual(m)){game_state*t=execute_move(os,m);assert(t);game_changed_state(ou,os,t);free_game(os);os=t;sfree(last_move);last_move=dupstr(m);move_count++;sfree(m);}}
static void quoted(const char*s){putchar('"');for(const unsigned char*p=(const unsigned char*)s;*p;p++){assert(*p>=32&&*p<127);if(*p=='"'||*p=='\\')putchar('\\');putchar(*p);}putchar('"');}
static unsigned c565(float*p,int c){unsigned r=(unsigned)(p[3*c]*255+.5F),g=(unsigned)(p[3*c+1]*255+.5F),b=(unsigned)(p[3*c+2]*255+.5F);return((r>>3)<<11)|((g>>2)<<5)|(b>>3);}
struct text_record{int x,y,size,align,colour;char label[80];};static struct text_record texts[512];static int ntexts,nops;
static void op_start(const char*kind){if(nops++)putchar(',');putchar('[');quoted(kind);}
static void record_text(drawing*d,int x,int y,int type,int size,int align,int colour,const char*s){(void)d;(void)type;assert(ntexts<512&&strlen(s)<80);texts[ntexts++]=(struct text_record){x,y,size,align,colour,""};strcpy(texts[ntexts-1].label,s);op_start("text");printf(",%d,%d,%d,%d,%d,",x,y,size,align,colour);quoted(s);putchar(']');}
static void record_rect(drawing*d,int x,int y,int w,int h,int c){(void)d;op_start("rect");printf(",%d,%d,%d,%d,%d]",x,y,w,h,c);}
static void record_line(drawing*d,int x,int y,int x2,int y2,int c){(void)d;op_start("line");printf(",%d,%d,%d,%d,%d]",x,y,x2,y2,c);}
static void record_polygon(drawing*d,const int*p,int n,int fill,int outline){(void)d;op_start("poly");printf(",%d,%d,[",fill,outline);for(int i=0;i<2*n;i++)printf("%s%d",i?",":"",p[i]);printf("]]");}
static void record_clip(drawing*d,int x,int y,int w,int h){(void)d;op_start("clip");printf(",%d,%d,%d,%d]",x,y,w,h);}
static void record_unclip(drawing*d){(void)d;op_start("unclip");putchar(']');}
static void ignore_update(drawing*d,int x,int y,int w,int h){(void)d;(void)x;(void)y;(void)w;(void)h;}
static void ignore_draw(drawing*d){(void)d;}static void ignore_status(drawing*d,const char*s){(void)d;(void)s;}
static const drawing_api native_drawing={.version=1,.draw_text=record_text,.draw_rect=record_rect,.draw_line=record_line,.draw_polygon=record_polygon,.draw_update=ignore_update,.clip=record_clip,.unclip=record_unclip,.start_draw=ignore_draw,.end_draw=ignore_draw,.status_bar=ignore_status};
static void output(void){int nc,nkeys,w,h;float*palette=game_colours(NULL,&nc);key_label*keys=game_request_keys(op,&nkeys);game_compute_size(op,oracle_tile,ou,&w,&h);
 printf("{\"width\":%d,\"height\":%d,\"w\":%d,\"h\":%d,\"status\":%d,\"cheated\":%d,\"completed\":%d,\"move_count\":%d,\"last_move\":",w,h,os->cr,os->cr,game_status(os),os->cheated,os->completed,move_count);quoted(last_move?last_move:"");printf(",\"ui\":{\"hx\":%d,\"hy\":%d,\"hshow\":%d,\"hpencil\":%d,\"keep_pencil\":%d},\"keys\":[",ou->hx,ou->hy,ou->hshow,ou->hpencil,ou->pencil_keep_highlight);for(int i=0;i<nkeys;i++)printf("%s%d",i?",":"",keys[i].button);
 printf("],\"palette\":[");for(int i=0;i<nc;i++)printf("%s%u",i?",":"",c565(palette,i));printf("],\"cells\":[");for(int i=0;i<os->cr*os->cr;i++)printf("%s%d",i?",":"",os->grid[i]);printf("],\"clues\":[");for(int i=0;i<os->cr*os->cr;i++)printf("%s%d",i?",":"",os->immutable[i]?os->grid[i]:0);printf("],\"pencils\":[");for(int i=0;i<os->cr*os->cr;i++){int bits=0;for(int j=0;j<os->cr;j++)bits|=(int)os->pencil[i*os->cr+j]<<j;printf("%s%d",i?",":"",bits);}printf("],\"blocks\":[");for(int i=0;i<os->cr*os->cr;i++)printf("%s%d",i?",":"",os->blocks->whichblock[i]);printf("],\"ops\":[");drawing*dr=drawing_new(&native_drawing,NULL,NULL);game_free_drawstate(NULL,ods);ods=game_new_drawstate(dr,os);game_set_size(dr,ods,op,oracle_tile);nops=ntexts=0;game_redraw(dr,ods,NULL,os,0,ou,0,0);
 printf("],\"highlights\":[");for(int i=0;i<os->cr*os->cr;i++)printf("%s%d",i?",":"",ods->hl[i]);printf("],\"texts\":[");for(int i=0;i<ntexts;i++){struct text_record*t=&texts[i];printf("%s[%d,%d,%d,%d,%d,",i?",":"",t->x,t->y,t->size,t->align,t->colour);quoted(t->label);putchar(']');}puts("]}");drawing_free(dr);sfree(palette);free_keys(keys,nkeys);}
static void target(void){const char*e=NULL;char*solution=solve_game(os,os,NULL,&e);if(!solution||e)fatal("Original solve_game: %s",e?e:"NULL");game_state*t=execute_move(os,solution);assert(t&&t->completed&&t->cheated);int count=os->cr*os->cr;printf("{\"target\":[");for(int i=0;i<count;i++)printf("%s%d",i?",":"",t->grid[i]);for(int i=0;i<count;i++)if(!os->immutable[i]){select_cell(os,ou,ods,i,LEFT_BUTTON);input(0,0,'0'+t->grid[i]);}assert(os->completed&&!os->cheated&&check_valid(os->cr,os->blocks,os->kblocks,os->kgrid,os->xtype,os->grid));printf("],\"ordinary_status\":%d,\"ordinary_cheated\":%d,\"ordinary_entries\":%d,\"original_flash_seconds\":%.2f}\n",game_status(os),os->cheated,move_count,(double)FLASH_TIME);free_game(t);sfree(solution);}
int main(int argc,char**argv){if(argc==2&&!strcmp(argv[1],"--self-test")){self_test();return 0;}if(argc==4&&!strcmp(argv[1],"--target")){load(argv[2],atoi(argv[3]));target();clean();return 0;}if(argc<3)fatal("Usage: oracle ID tile [x,y,original-button ...]");load(argv[1],atoi(argv[2]));for(int i=3;i<argc;i++){int x,y,b;char tail;if(sscanf(argv[i],"%d,%d,%d%c",&x,&y,&b,&tail)!=3)fatal("Bad native input");input(x,y,b);}output();clean();return 0;}
