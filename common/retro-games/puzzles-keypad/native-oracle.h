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
static int oracle_tile;
static void load_id(const char *id,int tile){
    char *copy=dupstr(id),*desc=strchr(copy,':');if(!desc)fatal("Missing ID");*desc++=0;
    op=default_params();decode_params(op,copy);const char *e=validate_params(op,true);if(!e)e=validate_desc(op,desc);if(e)fatal("Original validation: %s",e);
    oracle_tile=tile;os=new_game(NULL,op,desc);ou=new_ui(os);ods=game_new_drawstate(NULL,os);game_set_size(NULL,ods,op,tile);sfree(copy);
}
static void clean(void){sfree(lastmove);lastmove=NULL;game_free_drawstate(NULL,ods);free_ui(ou);free_game(os);free_params(op);}
static void input(int x,int y,int button){
    char *m=interpret_move(os,ou,ods,x,y,button);
    if(m&&m!=MOVE_UI_UPDATE&&m!=MOVE_NO_EFFECT&&m!=MOVE_UNUSED){
        game_state *next=execute_move(os,m);if(!next)fatal("Original rejected interpreted move %s",m);
        game_changed_state(ou,os,next);free_game(os);os=next;sfree(lastmove);lastmove=dupstr(m);sfree(m);
    }
}
static unsigned c565(float *p,int c){unsigned r=(unsigned)(p[3*c]*255+.5F),g=(unsigned)(p[3*c+1]*255+.5F),b=(unsigned)(p[3*c+2]*255+.5F);return((r>>3)<<11)|((g>>2)<<5)|(b>>3);}
static int text_count;
static void record_text(drawing *d,int x,int y,int type,int size,int align,int colour,const char *str){
    (void)d;(void)type;printf("%s[%d,%d,%d,%d,%d,\"",text_count++?",":"",x,y,size,align,colour);
    for(const unsigned char *p=(const unsigned char*)str;*p;p++){if(*p=='"'||*p=='\\')putchar('\\');if(*p>=32&&*p<127)putchar(*p);else putchar('?');}printf("\"]");
}
static void no_rect(drawing*d,int x,int y,int w,int h,int c){(void)d;(void)x;(void)y;(void)w;(void)h;(void)c;}
static void no_line(drawing*d,int x,int y,int w,int h,int c){no_rect(d,x,y,w,h,c);}
static void no_poly(drawing*d,const int*p,int n,int f,int o){(void)d;(void)p;(void)n;(void)f;(void)o;}
static void no_clip(drawing*d,int x,int y,int w,int h){(void)d;(void)x;(void)y;(void)w;(void)h;}
static void no_unclip(drawing*d){(void)d;}
static void no_status(drawing*d,const char*s){(void)d;(void)s;}
static char *ascii_fallback(drawing*d,const char*const*s,int n){(void)d;for(int i=0;i<n;i++){bool ok=true;for(const unsigned char*p=(const unsigned char*)s[i];*p;p++)if(*p>=127)ok=false;if(ok)return dupstr(s[i]);}return dupstr(n?s[n-1]:"");}
static const drawing_api oracle_drawing={.version=1,.draw_text=record_text,.draw_rect=no_rect,.draw_line=no_line,.draw_polygon=no_poly,.draw_update=no_clip,.clip=no_clip,.unclip=no_unclip,.start_draw=no_unclip,.end_draw=no_unclip,.status_bar=no_status,.text_fallback=ascii_fallback};
static void output(void){
    int nc,nkeys,w,h;float *palette=game_colours(NULL,&nc);key_label *keys=game_request_keys(op,&nkeys);
    game_compute_size(op,ods->tilesize,ou,&w,&h);
    printf("{\"width\":%d,\"height\":%d,\"status\":%d,\"cheated\":%d,\"last_move\":\"%s\",\"keys\":[",w,h,game_status(os),os->cheated,lastmove?lastmove:"");
    for(int i=0;i<nkeys;i++)printf("%s%d",i?",":"",keys[i].button);
    printf("],\"palette\":[");for(int i=0;i<nc;i++)printf("%s%u",i?",":"",c565(palette,i));
    printf("],\"cells\":[");
#ifdef KP_FILLING
    printf("%d",os->board[0]);for(int i=1;i<op->w*op->h;i++)printf(",%d",os->board[i]);
    printf("],\"w\":%d,\"h\":%d,\"clues\":[",op->w,op->h);for(int i=0;i<op->w*op->h;i++)printf("%s%d",i?",":"",os->shared->clues[i]);
#else
    printf("%d",os->grid[0]);for(int i=1;i<op->w*op->w;i++)printf(",%d",os->grid[i]);
    printf("],\"w\":%d,\"h\":%d,\"pencils\":[",op->w,op->w);for(int i=0;i<op->w*op->w;i++)printf("%s%d",i?",":"",os->pencil[i]);
#endif
    printf("],\"texts\":[");drawing *dr=drawing_new(&oracle_drawing,NULL,NULL);text_count=0;
    /* Fresh drawing cache ensures every official clue, entry and pencil glyph
       is recorded, including unchanged cells. UI still reflects replayed input. */
    game_free_drawstate(NULL,ods);ods=game_new_drawstate(dr,os);game_set_size(dr,ods,op,oracle_tile);
    game_redraw(dr,ods,NULL,os,0,ou,0,0);puts("]}");
    sfree(palette);free_keys(keys,nkeys);drawing_free(dr);
}
static void self_test(void){
    game_params *p=default_params();random_state *r=random_new("keypad-rule-test",16);char *aux=NULL,*desc=new_game_desc(p,r,&aux,false),*encoded=encode_params(p,false),*id=snewn(strlen(encoded)+strlen(desc)+2,char);
    sprintf(id,"%s:%s",encoded,desc);load_id(id,24);int n;key_label *keys=game_request_keys(op,&n);
    assert(n>0&&n<=10&&keys[n-1].button=='\b');for(int i=0;i<n-1;i++)assert(keys[i].button=='1'+i);free_keys(keys,n);
    int index=0;
#ifdef KP_FILLING
    while(os->shared->clues[index])index++;int w=op->w;int original=os->board[index];
#else
    int w=op->w;int original=os->grid[index];
#endif
    input((index%w+1)*24,(index/w+1)*24,LEFT_BUTTON);input(0,0,'1');assert(lastmove);
#ifdef KP_FILLING
    assert(os->board[index]==1);
#else
    assert(os->grid[index]==1);assert(!ou->hshow);
#endif
    input((index%w+1)*24,(index/w+1)*24,LEFT_BUTTON);input(0,0,'\b');
#ifdef KP_FILLING
    assert(os->board[index]==original);
    assert(execute_move(os,"999999_1")==NULL&&execute_move(os,"bad")==NULL);
#else
    assert(os->grid[index]==original);input(24,24,RIGHT_BUTTON);input(0,0,'2');assert(os->pencil[0]==(1<<2)&&!ou->hshow);input(24,24,LEFT_BUTTON);input(0,0,'\b');assert(os->pencil[0]==0);
    assert(execute_move(os,"R999,0,1")==NULL&&execute_move(os,"bad")==NULL);
#endif
    const char *error=NULL;char *solution=solve_game(os,os,aux,&error);assert(solution&&!error);
    int count;
#ifdef KP_FILLING
    count=op->w*op->h;
#else
    count=op->w*op->w;
#endif
    for(int i=0;i<count;i++){
        char move[64];
#ifdef KP_FILLING
        if(os->shared->clues[i])continue;
        sprintf(move,"%d_%d",i,solution[i+1]-'0');
#else
        sprintf(move,"R%d,%d,%d",i%op->w,i/op->w,solution[i+1]-'0');
#endif
        game_state *next=execute_move(os,move);assert(next);free_game(os);os=next;
    }
    assert(game_status(os)==1&&!os->cheated);clean();sfree(solution);sfree(id);sfree(desc);sfree(encoded);sfree(aux);free_params(p);random_free(r);
    puts("PASS: original default generator/validation, original requested integer keys, pointer selection, digit entry, Backspace clear, invalid syntax/bounds, normal-move uncheated completed solution; Keen also original right-pencil and highlight lifetime");
}
static void target(void){
    const char *e=NULL;char *solution=solve_game(os,os,NULL,&e);if(!solution||e)fatal("Original solver: %s",e?e:"no solution");
    int count;
#ifdef KP_FILLING
    count=op->w*op->h;
#else
    count=op->w*op->w;
#endif
    printf("{\"target\":[");for(int i=0;i<count;i++){
        int value=solution[i+1]-'0';assert(value>0&&value<=9);printf("%s%d",i?",":"",value);char move[64];
#ifdef KP_FILLING
        if(os->shared->clues[i]){assert(os->shared->clues[i]==value);continue;}sprintf(move,"%d_%d",i,value);
#else
        sprintf(move,"R%d,%d,%d",i%op->w,i/op->w,value);
#endif
        game_state *next=execute_move(os,move);assert(next);free_game(os);os=next;
    }
    assert(game_status(os)==1&&!os->cheated);
    printf("],\"ordinary_status\":%d,\"ordinary_cheated\":%d}\n",game_status(os),os->cheated);sfree(solution);
}
int main(int argc,char **argv){
    if(argc==2&&!strcmp(argv[1],"--self-test")){self_test();return 0;}
    if(argc==4&&!strcmp(argv[1],"--target")){load_id(argv[2],atoi(argv[3]));target();clean();return 0;}
    if(argc<3)fatal("Usage: oracle ID tile [x,y,integer-button ...]");load_id(argv[1],atoi(argv[2]));
    for(int i=3;i<argc;i++){int x,y,b;char tail;if(sscanf(argv[i],"%d,%d,%d%c",&x,&y,&b,&tail)!=3)fatal("Invalid QA input");input(x,y,b);}
    output();clean();return 0;
}
