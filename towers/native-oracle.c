/* SPDX-License-Identifier: MIT. Test-only original Towers replay and geometry. */
#include "../common/retro-games/puzzles/vendor/towers.c"
#include <stdarg.h>
void fatal(const char *fmt,...){va_list a;va_start(a,fmt);vfprintf(stderr,fmt,a);va_end(a);fputc('\n',stderr);exit(2);}
char *__wrap_getenv(const char *name){(void)name;return NULL;}
void frontend_default_colour(frontend *fe,float *out){(void)fe;out[0]=out[1]=out[2]=0.9F;}
void activate_timer(frontend *fe){(void)fe;}
void deactivate_timer(frontend *fe){(void)fe;}
void get_random_seed(void **s,int *n){*n=8;*s=dupstr("testseed");}
static game_params *p;static game_state *s;static game_ui *ui;static game_drawstate *ds;
static char *last;static int bounds[4],bw,bh;
typedef struct {int x,y,size,align,colour,clip[4];char label[40];} Text;
typedef struct {int n,coords[8],fill,outline,clip[4];} Poly;
static Text texts[2048];static Poly polygons[4096];static int nt,np;
static void text(drawing *d,int x,int y,int type,int size,int align,int colour,const char *str){
    (void)d;(void)type;assert(nt<2048 && strlen(str)<40);Text *t=&texts[nt++];t->x=x;t->y=y;t->size=size;t->align=align;t->colour=colour;
    memcpy(t->clip,bounds,sizeof(bounds));strcpy(t->label,str);
}
static void poly(drawing *d,const int *points,int n,int fill,int outline){
    (void)d;assert(np<4096 && n>=3 && n<=4);Poly *q=&polygons[np++];q->n=n;q->fill=fill;q->outline=outline;memcpy(q->coords,points,2*n*sizeof(int));memcpy(q->clip,bounds,sizeof(bounds));
}
static void rectangle(drawing*d,int x,int y,int w,int h,int c){(void)d;(void)x;(void)y;(void)w;(void)h;(void)c;}
static void line(drawing*d,int x,int y,int a,int b,int c){rectangle(d,x,y,a,b,c);}
static void clipit(drawing*d,int x,int y,int w,int h){(void)d;bounds[0]=max(0,x);bounds[1]=max(0,y);bounds[2]=min(bw,x+w);bounds[3]=min(bh,y+h);}
static void no_unclip(drawing*d){(void)d;bounds[0]=bounds[1]=0;bounds[2]=bw;bounds[3]=bh;}
static void update(drawing*d,int x,int y,int w,int h){(void)d;(void)x;(void)y;(void)w;(void)h;}
static void status(drawing*d,const char *str){(void)d;(void)str;}
static const drawing_api api={.version=1,.draw_text=text,.draw_rect=rectangle,.draw_line=line,.draw_polygon=poly,.draw_update=update,.clip=clipit,.unclip=no_unclip,.start_draw=no_unclip,.end_draw=no_unclip,.status_bar=status};
static void point(int index,int *x,int *y){int h=s->grid[index];*x=COORD(index%p->w)+TILESIZE/2+X_3D_DISP(h,p->w);*y=COORD(index/p->w)+TILESIZE/2-Y_3D_DISP(h,p->w);}
static void input(int x,int y,int button){
    char *move=interpret_move(s,ui,ds,x,y,button);
    if(move && move!=MOVE_UI_UPDATE && move!=MOVE_UNUSED && move!=MOVE_NO_EFFECT){
        assert(move[0]!='S');game_state *next=execute_move(s,move);assert(next && !next->cheated);game_changed_state(ui,s,next);free_game(s);s=next;sfree(last);last=dupstr(move);sfree(move);
    }
}
static unsigned rgb565(float *palette,int c){unsigned r=(unsigned)(palette[3*c]*255+.5F),g=(unsigned)(palette[3*c+1]*255+.5F),b=(unsigned)(palette[3*c+2]*255+.5F);return((r>>3)<<11)|((g>>2)<<5)|(b>>3);}
static void print_array(int *v,int n){putchar('[');for(int i=0;i<n;i++)printf("%s%d",i?",":"",v[i]);putchar(']');}
static void output(void){
    int nkeys,nc;key_label *keys=game_request_keys(p,&nkeys);float *palette=game_colours(NULL,&nc);
    game_compute_size(p,ds->tilesize,ui,&bw,&bh);no_unclip(NULL);nt=np=0;
    drawing *dr=drawing_new(&api,NULL,NULL);int tile=ds->tilesize;game_free_drawstate(NULL,ds);ds=game_new_drawstate(dr,s);game_set_size(dr,ds,p,tile);game_redraw(dr,ds,NULL,s,1,ui,0,0);
    bool errors[(p->w+2)*(p->w+2)];check_errors(s,errors);
    printf("{\"width\":%d,\"height\":%d,\"w\":%d,\"h\":%d,\"status\":%d,\"cheated\":%s,\"last_move\":\"%s\",\"three_d\":%s,\"selection\":{\"x\":%d,\"y\":%d,\"show\":%s,\"pencil\":%s},\"keys\":[",bw,bh,p->w,p->w,game_status(s),s->cheated?"true":"false",last?last:"",ui->three_d?"true":"false",ui->hx,ui->hy,ui->hshow?"true":"false",ui->hpencil?"true":"false");
    for(int i=0;i<nkeys;i++)printf("%s%d",i?",":"",keys[i].button);
    printf("],\"palette\":[");for(int i=0;i<nc;i++)printf("%s%u",i?",":"",rgb565(palette,i));
    printf("],\"cells\":[");for(int i=0;i<p->w*p->w;i++)printf("%s%d",i?",":"",s->grid[i]);
    printf("],\"pencils\":[");for(int i=0;i<p->w*p->w;i++)printf("%s%d",i?",":"",s->pencil[i]);
    printf("],\"immutable\":[");for(int i=0;i<p->w*p->w;i++)printf("%s%d",i?",":"",s->clues->immutable[i]);
    printf("],\"clues\":[");for(int i=0;i<4*p->w;i++)printf("%s%d",i?",":"",s->clues->clues[i]);
    printf("],\"clues_done\":[");for(int i=0;i<4*p->w;i++)printf("%s%s",i?",":"",s->clues_done[i]?"true":"false");
    printf("],\"errors\":[");for(int i=0;i<(p->w+2)*(p->w+2);i++)printf("%s%s",i?",":"",errors[i]?"true":"false");
    printf("],\"centres\":[");for(int i=0;i<p->w*p->w;i++){int x,y;point(i,&x,&y);printf("%s[%d,%d]",i?",":"",x,y);}
    printf("],\"clue_points\":[");for(int i=0;i<4*p->w;i++){int x,y;CLUEPOS(x,y,i,p->w);printf("%s[%d,%d]",i?",":"",COORD(x)+TILESIZE/2,COORD(y)+TILESIZE/2);}
    printf("],\"raised_tops\":[");int count=0;for(int i=0;i<p->w*p->w;i++)if(s->grid[i]==p->w){int x=COORD(i%p->w)+TILESIZE/2,y=COORD(i/p->w)-Y_3D_DISP(p->w,p->w)+1;game_ui *probe=new_ui(s);assert(interpret_move(s,probe,ds,x,y,LEFT_BUTTON)==MOVE_UI_UPDATE && probe->hx==i%p->w && probe->hy==i/p->w);printf("%s{\"index\":%d,\"point\":[%d,%d],\"flat_row\":%d,\"native_row\":%d}",count++?",":"",i,x,y,FROMCOORD(y),probe->hy);free_ui(probe);}
    printf("],\"texts\":[");for(int i=0;i<nt;i++){Text *t=&texts[i];printf("%s{\"draw\":[%d,%d,%d,%d,%d,\"%s\"],\"clip\":",i?",":"",t->x,t->y,t->size,t->align,t->colour,t->label);print_array(t->clip,4);putchar('}');}
    printf("],\"polygons\":[");for(int i=0;i<np;i++){Poly *q=&polygons[i];printf("%s{\"points\":",i?",":"");print_array(q->coords,2*q->n);printf(",\"fill\":%d,\"outline\":%d,\"clip\":",q->fill,q->outline);print_array(q->clip,4);putchar('}');}
    puts("]}");sfree(palette);free_keys(keys,nkeys);drawing_free(dr);
}
int main(int argc,char **argv){
    bool target=argc==4 && !strcmp(argv[1],"--target");assert(argc>=3);
    char *id=dupstr(argv[target?2:1]),*desc=strchr(id,':');assert(desc);*desc++=0;p=default_params();decode_params(p,id);assert(!validate_params(p,true) && !validate_desc(p,desc));
    s=new_game(NULL,p,desc);ui=new_ui(s);ds=game_new_drawstate(NULL,s);game_set_size(NULL,ds,p,atoi(argv[target?3:2]));
    if(target){
        const char *error=NULL;char *solution=solve_game(s,s,NULL,&error);assert(solution && !error && solution[0]=='S' && strlen(solution)==(size_t)(p->w*p->w+1));
        game_state *before=NULL;
        for(int i=0;i<p->w*p->w;i++)if(!s->clues->immutable[i]){int x,y;point(i,&x,&y);if(before)free_game(before);before=dup_game(s);input(x,y,LEFT_BUTTON);input(x,y,LEFT_BUTTON+3);input(x,y,LEFT_BUTTON+6);input(0,0,solution[i+1]);}
        assert(before && !before->completed && s->completed && !s->cheated && !check_errors(s,NULL));
        printf("{\"target\":[");for(int i=0;i<p->w*p->w;i++)printf("%s%d",i?",":"",solution[i+1]-'0');
        printf("],\"ordinary_status\":%d,\"ordinary_cheated\":false,\"before_last_status\":%d,\"original_flash_seconds\":%.1f,\"solver_S_executed\":false}\n",game_status(s),game_status(before),(double)game_flash_length(before,s,1,ui));free_game(before);sfree(solution);
    }else{for(int i=3;i<argc;i++){int x,y,b;char tail;assert(sscanf(argv[i],"%d,%d,%d%c",&x,&y,&b,&tail)==3);input(x,y,b);}output();}
    sfree(last);free_ui(ui);game_free_drawstate(NULL,ds);free_game(s);free_params(p);sfree(id);return 0;
}
