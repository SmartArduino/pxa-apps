/* SPDX-License-Identifier: MIT. Test-only calls; original rules unchanged. */
#include "../common/retro-games/puzzles/vendor/unequal.c"
/* SPDX-License-Identifier: MIT
 * Test-only oracle: all input interpretation, state and drawing commands come
 * from the unmodified official backend included by the module wrapper.
 */
#include <stdarg.h>
void fatal(const char *format,...){va_list a;va_start(a,format);vfprintf(stderr,format,a);va_end(a);fputc('\n',stderr);exit(2);}
void frontend_default_colour(frontend *f,float *out){(void)f;out[0]=out[1]=out[2]=0.9F;}
void activate_timer(frontend *f){(void)f;}
void deactivate_timer(frontend *f){(void)f;}
void get_random_seed(void **s,int *n){*n=8;*s=dupstr("testseed");}
static game_params *op;
static game_state *os;
static game_ui *ou;
static game_drawstate *ods;
static char *lastmove;
static game_state *previous;
static int oracle_tile;
static void load_id(const char *id,int tile){
    char *copy=dupstr(id),*desc=strchr(copy,':');if(!desc)fatal("Missing ID");*desc++=0;
    op=default_params();decode_params(op,copy);const char *e=validate_params(op,true);if(!e)e=validate_desc(op,desc);if(e)fatal("Original validation: %s",e);
    oracle_tile=tile;os=new_game(NULL,op,desc);ou=new_ui(os);ods=game_new_drawstate(NULL,os);game_set_size(NULL,ods,op,tile);sfree(copy);
}
static void clean(void){if(previous){free_game(previous);previous=NULL;}sfree(lastmove);lastmove=NULL;game_free_drawstate(NULL,ods);free_ui(ou);free_game(os);free_params(op);}
static void input(int x,int y,int button){
    char *m=interpret_move(os,ou,ods,x,y,button);
    if(m&&m!=MOVE_UI_UPDATE&&m!=MOVE_NO_EFFECT&&m!=MOVE_UNUSED){
        game_state *next=execute_move(os,m);if(!next)fatal("Original rejected interpreted move %s",m);
        game_changed_state(ou,os,next);if(previous)free_game(previous);previous=dup_game(os);free_game(os);os=next;sfree(lastmove);lastmove=dupstr(m);sfree(m);
    }
}
static unsigned c565(float *p,int c){unsigned r=(unsigned)(p[3*c]*255+.5F),g=(unsigned)(p[3*c+1]*255+.5F),b=(unsigned)(p[3*c+2]*255+.5F);return((r>>3)<<11)|((g>>2)<<5)|(b>>3);}
static int text_count;
static int poly_count,polys[256][13];
static int rect_count,rects[1024][5];
static void record_text(drawing *d,int x,int y,int type,int size,int align,int colour,const char *str){
    (void)d;(void)type;printf("%s[%d,%d,%d,%d,%d,\"",text_count++?",":"",x,y,size,align,colour);
    for(const unsigned char *p=(const unsigned char*)str;*p;p++){if(*p=='"'||*p=='\\')putchar('\\');if(*p>=32&&*p<127)putchar(*p);else putchar('?');}printf("\"]");
}
static void no_rect(drawing*d,int x,int y,int w,int h,int c){(void)d;assert(rect_count<1024);int *r=rects[rect_count++];r[0]=x;r[1]=y;r[2]=w;r[3]=h;r[4]=c;}
static void no_line(drawing*d,int x,int y,int w,int h,int c){(void)d;(void)x;(void)y;(void)w;(void)h;(void)c;}
static void no_poly(drawing*d,const int*p,int n,int f,int o){
 (void)d;(void)o;if(n==6){assert(poly_count<256);polys[poly_count][0]=f;for(int i=0;i<12;i++)polys[poly_count][i+1]=p[i];poly_count++;}
}
static void no_clip(drawing*d,int x,int y,int w,int h){(void)d;(void)x;(void)y;(void)w;(void)h;}
static void no_unclip(drawing*d){(void)d;}
static void no_status(drawing*d,const char*s){(void)d;(void)s;}
static char *ascii_fallback(drawing*d,const char*const*s,int n){(void)d;for(int i=0;i<n;i++){bool ok=true;for(const unsigned char*p=(const unsigned char*)s[i];*p;p++)if(*p>=127)ok=false;if(ok)return dupstr(s[i]);}return dupstr(n?s[n-1]:"");}
static const drawing_api oracle_drawing={.version=1,.draw_text=record_text,.draw_rect=no_rect,.draw_line=no_line,.draw_polygon=no_poly,.draw_update=no_clip,.clip=no_clip,.unclip=no_unclip,.start_draw=no_unclip,.end_draw=no_unclip,.status_bar=no_status,.text_fallback=ascii_fallback};
static unsigned hintmask(int index){unsigned mask=0;for(int n=0;n<op->order;n++)if(HINT(os,index%op->order,index/op->order,n))mask|=1U<<(n+1);return mask;}
static void output(void){
    int nc,nkeys,w,h;float *palette=game_colours(NULL,&nc);key_label *keys=game_request_keys(op,&nkeys);
    game_compute_size(op,ods->tilesize,ou,&w,&h);
    printf("{\"width\":%d,\"height\":%d,\"status\":%d,\"cheated\":%d,\"last_move\":\"%s\",\"keys\":[",w,h,game_status(os),os->cheated,lastmove?lastmove:"");
    for(int i=0;i<nkeys;i++)printf("%s%d",i?",":"",keys[i].button);
    printf("],\"palette\":[");for(int i=0;i<nc;i++)printf("%s%u",i?",":"",c565(palette,i));
    printf("],\"cells\":[");
    printf("%d",os->nums[0]);for(int i=1;i<op->order*op->order;i++)printf(",%d",os->nums[i]);
    printf("],\"w\":%d,\"h\":%d,\"pencils\":[",op->order,op->order);for(int i=0;i<op->order*op->order;i++)printf("%s%d",i?",":"",hintmask(i));

    printf("],\"texts\":[");drawing *dr=drawing_new(&oracle_drawing,NULL,NULL);text_count=0;poly_count=0;rect_count=0;
    /* Fresh drawing cache ensures every official clue, entry and pencil glyph
       is recorded, including unchanged cells. UI still reflects replayed input. */
    game_free_drawstate(NULL,ods);ods=game_new_drawstate(dr,os);game_set_size(dr,ods,op,oracle_tile);
    game_redraw(dr,ods,NULL,os,0,ou,0,0);
 printf("],\"flags\":[");for(int i=0;i<16;i++)printf("%s%u",i?",":"",os->flags[i]);
 printf("],\"immutable\":[");for(int i=0;i<16;i++)printf("%s%d",i?",":"",!!(os->flags[i]&F_IMMUTABLE));
 printf("],\"inequality_polygons\":[");for(int i=0;i<poly_count;i++){printf("%s[",i?",":"");for(int j=0;j<13;j++)printf("%s%d",j?",":"",polys[i][j]);printf("]");}
 printf("],\"rectangles\":[");for(int i=0;i<rect_count;i++){printf("%s[",i?",":"");for(int j=0;j<5;j++)printf("%s%d",j?",":"",rects[i][j]);printf("]");}
 printf("],\"flash_seconds\":%.2f",previous?game_flash_length(previous,os,1,ou):0.0F);
 printf(",\"ui\":{\"hx\":%d,\"hy\":%d,\"show\":%d,\"pencil\":%d}}\n",ou->hx,ou->hy,ou->hshow,ou->hpencil);
    sfree(palette);free_keys(keys,nkeys);drawing_free(dr);
}
static int real_move(char *move){return move&&move!=MOVE_UI_UPDATE&&move!=MOVE_NO_EFFECT&&move!=MOVE_UNUSED;}
static char *select_key(game_state *s,int index,int button,int key){
 game_drawstate ds={0};ds.tilesize=32;ds.order=4;game_ui *ui=new_ui(s);
 char *select=interpret_move(s,ui,&ds,16+(index%4)*48+16,16+(index/4)*48+16,button);assert(!real_move(select));
 char *move=interpret_move(s,ui,&ds,0,0,key);free_ui(ui);return real_move(move)?move:NULL;
}
static game_state *step(game_state *s,char *m){assert(m);game_state *next=execute_move(s,m);assert(next);sfree(m);free_game(s);return next;}
static void self_test(void){
 game_params *p=default_params();assert(p->order==4&&p->diff==DIFF_EASY&&p->mode==MODE_UNEQUAL);assert(!validate_params(p,true));
 int w,h;game_compute_size(p,32,NULL,&w,&h);assert(w==h);char *encoded=encode_params(p,true);
 printf("{\"native_only\":true,\"params\":\"%s\",\"order\":4,\"mode\":\"Unequal\",\"difficulty\":\"Easy\",\"preferred_tile\":32,\"original_board_at_tile32\":[%d,%d],\"cases\":[",encoded,w,h);
 int immutable_tests=0;
 for(int sample=0;sample<8;sample++){
  char seed[80];snprintf(seed,sizeof(seed),"original Unequal native default4 research %d",sample);random_state *rs=random_new(seed,strlen(seed));char *aux=NULL;
  char *desc=new_game_desc(p,rs,&aux,false);assert(!validate_desc(p,desc));game_state *s=new_game(NULL,p,desc);assert(s->order==4&&!s->completed&&!s->cheated);
  assert(!execute_move(s,"INVALID"));assert(!execute_move(s,"R4,0,1"));assert(!execute_move(s,"R0,0,5"));
  int mutable=-1,immutable=-1,givens=0,inequalities=0;
  for(int i=0;i<16;i++){if(s->flags[i]&F_IMMUTABLE){givens++;immutable=i;}else if(mutable<0)mutable=i;if(s->flags[i]&(F_ADJ_UP|F_ADJ_RIGHT|F_ADJ_DOWN|F_ADJ_LEFT))inequalities++;}
  assert(mutable>=0&&inequalities>0);
  if(immutable>=0){char *m=select_key(s,immutable,LEFT_BUTTON,'1');assert(!m);immutable_tests++;}
  char *m=select_key(s,mutable,RIGHT_BUTTON,'1');assert(m&&m[0]=='P');s=step(s,m);assert(HINT(s,mutable%4,mutable/4,0)==1&&s->nums[mutable]==0);
  m=select_key(s,mutable,RIGHT_BUTTON,'1');assert(m&&m[0]=='P');s=step(s,m);assert(HINT(s,mutable%4,mutable/4,0)==0);
  m=select_key(s,mutable,LEFT_BUTTON,'2');assert(m&&m[0]=='R');s=step(s,m);assert(s->nums[mutable]==2);
  m=select_key(s,mutable,LEFT_BUTTON,' ');assert(m&&m[0]=='R');s=step(s,m);assert(s->nums[mutable]==0);
  const char *error=NULL;char *sol=solve_game(s,s,NULL,&error);assert(sol&&!error&&sol[0]=='S'&&strlen(sol)==17);
  game_state *wrong=dup_game(s);
  for(int i=0;i<16;i++)if(!(wrong->flags[i]&F_IMMUTABLE)){char move[64];snprintf(move,sizeof(move),"R%d,%d,1",i%4,i/4);wrong=step(wrong,dupstr(move));}
  assert(check_complete(wrong->nums,wrong,true)<0&&!wrong->completed);free_game(wrong);
  game_state *opposite=dup_game(s);int adj_errors=0;
  for(int i=0;i<16;i++)opposite->nums[i]=5-c2n(sol[i+1],4);
  for(int y=0;y<4;y++)for(int x=0;x<4;x++){
   assert(check_num_error(opposite->nums,opposite,x,y,false));
   if(!check_num_adj(opposite->nums,opposite,x,y,false))adj_errors++;
  }
  assert(adj_errors>0&&check_complete(opposite->nums,opposite,true)<0);free_game(opposite);
  int ordinary=0;
  for(int i=0;i<16;i++){
   if(s->flags[i]&F_IMMUTABLE){assert(s->nums[i]==c2n(sol[i+1],4));continue;}
   m=select_key(s,i,LEFT_BUTTON,sol[i+1]);assert(m&&m[0]=='R');s=step(s,m);ordinary++;
  }
  assert(check_complete(s->nums,s,true)>0&&game_status(s)==1&&s->completed&&!s->cheated);
  for(int i=0;i<16;i++)assert(s->nums[i]==c2n(sol[i+1],4));
  printf("%s{\"sample\":%d,\"generated_valid\":true,\"given_cells\":%d,\"inequality_flag_cells\":%d,\"pencil_toggle\":true,\"normal_digit_clear\":true,\"illegal_rejected\":true,\"wrong_latin_rejected\":true,\"valid_latin_opposite_inequalities_rejected\":true,\"ordinary_original_key_moves\":%d,\"completed\":true,\"cheated\":false}",sample?",":"",sample,givens,inequalities,ordinary);
  sfree(sol);free_game(s);sfree(desc);sfree(aux);random_free(rs);
 }
 assert(immutable_tests>0);printf("],\"immutable_input_tests\":%d,\"passed\":true,\"gui\":false,\"pxa_build\":false}\n",immutable_tests);sfree(encoded);free_params(p);return;
}

static void target(void){
 const char *error=NULL;char *solution=solve_game(os,os,NULL,&error);if(!solution||error)fatal("Original solver: %s",error?error:"no solution");
 assert(solution[0]=='S'&&strlen(solution)==17);printf("{\"target\":[");
 for(int i=0;i<16;i++){
  int value=solution[i+1]-'0';assert(value>=1&&value<=4);printf("%s%d",i?",":"",value);
  if(os->flags[i]&F_IMMUTABLE){assert(os->nums[i]==value);continue;}
  char move[64];sprintf(move,"R%d,%d,%d",i%4,i/4,value);game_state *next=execute_move(os,move);assert(next);free_game(os);os=next;
 }
 assert(game_status(os)==1&&os->completed&&!os->cheated);printf("],\"ordinary_status\":1,\"ordinary_cheated\":0}\n");sfree(solution);
}
int main(int argc,char **argv){
 if(argc==2&&!strcmp(argv[1],"--self-test")){self_test();puts("PASS: 8 original default4de native cases, Latin/inequality rejection, immutable selection, pencil/clear, ordinary completed uncheated");return 0;}
 if(argc==4&&!strcmp(argv[1],"--target")){load_id(argv[2],atoi(argv[3]));target();clean();return 0;}
 if(argc<3)fatal("Usage: native-oracle ID tile [x,y,integer-button ...]");load_id(argv[1],atoi(argv[2]));
 for(int i=3;i<argc;i++){int x,y,b;char tail;if(sscanf(argv[i],"%d,%d,%d%c",&x,&y,&b,&tail)!=3)fatal("Invalid original input sequence");input(x,y,b);}
 output();clean();return 0;
}
