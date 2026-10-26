/* SPDX-License-Identifier: MIT
 * Test-only boundary: include the unchanged original backend and call it.
 * This executable is never included in the PXA Guest. */
#include <stdarg.h>
#include <assert.h>
#include "../common/retro-games/puzzles/vendor/flood.c"

void fatal(const char *format, ...) {
    va_list args;va_start(args,format);vfprintf(stderr,format,args);va_end(args);
    fputc('\n',stderr);exit(2);
}
static game_state *from_id(const char *id) {
    char *copy=dupstr(id),*colon=strchr(copy,':');
    if(!colon)fatal("Missing original game description");
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
    game_state *state=from_id("2x2c3m0:0122,2");
    assert(state->moves==0 && game_status(state)==0);
    assert(execute_move(state,"M0")==NULL);
    state=step(state,"M1");
    assert(state->moves==1 && !state->complete && game_status(state)==0);
    assert(memcmp(state->grid,"\1\1\2\2",4)==0);
    state=step(state,"M2");
    assert(state->moves==2 && state->complete && game_status(state)==1);
    assert(execute_move(state,"M1")==NULL);free_game(state);
    state=from_id("2x2c3m0:0122,1");state=step(state,"M1");
    assert(state->moves==1 && !state->complete && game_status(state)==-1);
    state=step(state,"M2");
    assert(state->moves==2 && state->complete && game_status(state)==-1);
    free_game(state);puts("PASS: original initial/noop, flood/moves, victory, complete-block, defeat/late-completion");
}
int main(int argc,char **argv) {
    if(argc==2 && !strcmp(argv[1],"--self-test")){self_test();return 0;}
    if(argc==3 && !strcmp(argv[1],"--solution")) {
        game_state *state=from_id(argv[2]);const char *error=NULL;
        char *solution=solve_game(state,state,NULL,&error);
        if(!solution)fatal("Original solver: %s",error?error:"no solution");
        puts(solution);sfree(solution);free_game(state);return 0;
    }
    if(argc<2)fatal("Usage: native-oracle game-id [Mcolour ...]");
    game_state *state=from_id(argv[1]);
    for(int i=2;i<argc;i++)state=step(state,argv[i]);
    printf("{\"width\":%d,\"height\":%d,\"moves\":%d,\"limit\":%d,\"complete\":%s,\"status\":%d,\"grid\":[",
           state->w,state->h,state->moves,state->movelimit,state->complete?"true":"false",game_status(state));
    for(int i=0;i<state->w*state->h;i++)printf("%s%d",i?",":"",(unsigned char)state->grid[i]);
    puts("]}");free_game(state);return 0;
}
