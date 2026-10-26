/* SPDX-License-Identifier: MIT
 * Test-only wrapper around the unchanged official Untangle backend.
 * This native executable is never included in the PXA Guest. */
#include <stdarg.h>
#include "../common/retro-games/puzzles/vendor/untangle.c"

void fatal(const char *format, ...) {
    va_list args;va_start(args,format);vfprintf(stderr,format,args);va_end(args);
    fputc('\n',stderr);exit(2);
}
void frontend_default_colour(frontend *f,float *out) {
    (void)f;out[0]=out[1]=out[2]=0.9F;
}
static game_state *from_id(const char *id) {
    char *copy=dupstr(id),*colon=strchr(copy,':');
    if(!colon)fatal("Missing game description");
    *colon++=0;game_params *params=default_params();decode_params(params,copy);
    const char *error=validate_params(params,true);
    if(!error)error=validate_desc(params,colon);
    if(error)fatal("Original validation: %s",error);
    game_state *state=new_game(NULL,params,colon);free_params(params);sfree(copy);
    return state;
}
static game_state *step(game_state *state,const char *move) {
    game_state *next=execute_move(state,move);
    if(!next)fatal("Original backend rejected %s",move);
    free_game(state);return next;
}
static void self_test(void) {
    game_state *state=from_id("4:0-2,1-3");
    assert(state->params.n==4 && state->w==3 && game_status(state)==0);
    assert(execute_move(state,"P4:0,0/1")==NULL);
    assert(execute_move(state,"P0:0,0/0")==NULL);
    assert(execute_move(state,"malformed")==NULL);
    state=step(state,"P0:0,0/1;P1:2,0/1;P2:2,2/1;P3:0,2/1");
    assert(game_status(state)==0 && state->crosses[0] && state->crosses[1]);
    state=step(state,"P2:0,1/1");
    assert(state->pts[2].x==0 && state->pts[2].y==1 && game_status(state)==1);
    assert(!state->crosses[0] && !state->crosses[1] && !state->cheated);
    free_game(state);
    state=from_id("4:0-2,1-3");
    game_ui *ui=new_ui(state);game_drawstate *ds=game_new_drawstate(NULL,state);
    game_set_size(NULL,ds,&state->params,50);
    int x=state->pts[0].x*50/state->pts[0].d;
    int y=state->pts[0].y*50/state->pts[0].d;
    assert(interpret_move(state,ui,ds,x,y,LEFT_BUTTON)==MOVE_UI_UPDATE && ui->dragpoint==0);
    assert(interpret_move(state,ui,ds,60,60,LEFT_DRAG)==MOVE_UI_UPDATE);
    char *move=interpret_move(state,ui,ds,60,60,LEFT_RELEASE);
    assert(!strcmp(move,"P0:60,60/50") && ui->dragpoint==-1);
    sfree(move);
    assert(interpret_move(state,ui,ds,x,y,LEFT_BUTTON)==MOVE_UI_UPDATE);
    assert(interpret_move(state,ui,ds,-100,-100,LEFT_DRAG)==MOVE_UI_UPDATE);
    assert(interpret_move(state,ui,ds,-100,-100,LEFT_RELEASE)==MOVE_UI_UPDATE && ui->dragpoint==-1);
    assert(interpret_move(state,ui,ds,75,75,LEFT_BUTTON)==MOVE_NO_EFFECT);
    game_free_drawstate(NULL,ds);free_ui(ui);free_game(state);
    puts("PASS: original circle/crossings, invalid index/denominator/syntax, point move, crossing-free victory, DOWN/DRAG/RELEASE, outside cancel, empty hit");
}
static void output(const game_state *state) {
    int count;float *palette=game_colours(NULL,&count),*c=palette+3*COL_POINT;
    unsigned r=(unsigned)(c[0]*255+0.5F),g=(unsigned)(c[1]*255+0.5F),b=(unsigned)(c[2]*255+0.5F);
    unsigned rgb565=((r>>3)<<11)|((g>>2)<<5)|(b>>3);
    int crossed=0;for(int i=0;i<count234(state->graph->edges);i++)crossed+=!!state->crosses[i];
    printf("{\"n\":%d,\"width\":%d,\"height\":%d,\"status\":%d,\"crossed_edges\":%d,\"point_rgb565\":%u,\"points\":[",
        state->params.n,state->w,state->h,game_status(state),crossed,rgb565);
    for(int i=0;i<state->params.n;i++)printf("%s[%ld,%ld,%ld]",i?",":"",state->pts[i].x,state->pts[i].y,state->pts[i].d);
    printf("],\"edges\":[");
    for(int i=0;i<count234(state->graph->edges);i++) {
        edge *e=index234(state->graph->edges,i);printf("%s[%d,%d]",i?",":"",e->a,e->b);
    }
    puts("]}");sfree(palette);
}
int main(int argc,char **argv) {
    if(argc==2 && !strcmp(argv[1],"--self-test")){self_test();return 0;}
    if(argc==8 && !strcmp(argv[1],"--input")) {
        game_state *state=from_id(argv[2]);game_ui *ui=new_ui(state);
        game_drawstate *ds=game_new_drawstate(NULL,state);game_set_size(NULL,ds,&state->params,atoi(argv[3]));
        int sx=atoi(argv[4]),sy=atoi(argv[5]),tx=atoi(argv[6]),ty=atoi(argv[7]);
        char *down=interpret_move(state,ui,ds,sx,sy,LEFT_BUTTON);
        if(down!=MOVE_UI_UPDATE)fatal("Original input did not select vertex");
        interpret_move(state,ui,ds,tx,ty,LEFT_DRAG);
        char *move=interpret_move(state,ui,ds,tx,ty,LEFT_RELEASE);
        if(move==MOVE_UI_UPDATE || move==MOVE_NO_EFFECT || move==MOVE_UNUSED)fatal("Original input cancelled");
        puts(move);sfree(move);game_free_drawstate(NULL,ds);free_ui(ui);free_game(state);return 0;
    }
    if(argc<2)fatal("Usage: native-oracle ID [original P moves]");
    game_state *state=from_id(argv[1]);for(int i=2;i<argc;i++)state=step(state,argv[i]);
    output(state);free_game(state);return 0;
}
