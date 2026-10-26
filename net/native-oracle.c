/* SPDX-License-Identifier: MIT. Test-only oracle, never linked into the Guest.
 * The included official Net backend remains unchanged. */
#include <stdarg.h>
#include "../common/retro-games/puzzles/vendor/net.c"

void fatal(const char *format, ...) {
    va_list args;va_start(args,format);vfprintf(stderr,format,args);va_end(args);
    fputc('\n',stderr);exit(2);
}
void get_random_seed(void **seed,int *size) {
    const char *value="Net native test oracle only";
    *seed=dupstr(value);*size=(int)strlen(value);
}
static void print_tiles(const game_state *state) {
    putchar('[');for(int i=0;i<state->width*state->height;i++)
        printf("%s%u",i?",":"",state->tiles[i]);putchar(']');
}
int main(int argc,char **argv) {
    if(argc<3)fatal("Usage: native-oracle game-id tilesize [x y LEFT|RIGHT|MIDDLE [prior-move...]]");
    char *id=dupstr(argv[1]),*description=strchr(id,':');
    if(!description)fatal("Missing game description");*description++=0;
    game_params *params=default_params();decode_params(params,id);
    const char *error=validate_params(params,true);
    if(error)fatal("Invalid parameters: %s",error);
    error=validate_desc(params,description);if(error)fatal("Invalid description: %s",error);
    game_state *opening=new_game(NULL,params,description);
    game_state *state=dup_game(opening);
    for(int i=6;i<argc;i++) {
        game_state *next=execute_move(state,argv[i]);if(!next)fatal("Invalid prior move");
        free_game(state);state=next;
    }
    game_ui *ui=new_ui(state);ui->cur_visible=false;
    game_drawstate drawstate={0},*ds=&drawstate;ds->tilesize=atoi(argv[2]);
    if(ds->tilesize<8||ds->tilesize>128)fatal("Invalid tile size");
    int x=-1,y=-1,button=LEFT_BUTTON;
    if(argc>=6) {
        x=atoi(argv[3]);y=atoi(argv[4]);
        if(!strcmp(argv[5],"RIGHT"))button=RIGHT_BUTTON;
        else if(!strcmp(argv[5],"MIDDLE"))button=MIDDLE_BUTTON;
        else if(strcmp(argv[5],"LEFT"))fatal("Invalid pointer button");
    } else {
        for(int i=0;i<state->width*state->height;i++)if(COUNT(state->tiles[i]&15)==1) {
            x=i%state->width;y=i/state->width;break;
        }
    }
    if(x<0||x>=state->width||y<0||y>=state->height)fatal("Invalid chosen tile");
    int px=WINDOW_OFFSET+LINE_THICK+x*ds->tilesize+ds->tilesize/2;
    int py=WINDOW_OFFSET+LINE_THICK+y*ds->tilesize+ds->tilesize/2;
    char *move=interpret_move(state,ui,ds,px,py,button);
    bool real_move=move&&move!=MOVE_UI_UPDATE&&move!=MOVE_NO_EFFECT&&move!=MOVE_UNUSED;
    game_state *after=real_move?execute_move(state,move):dup_game(state);
    if(!after)fatal("Oracle move execution failed");
    const char *solve_error=NULL;char *solution=solve_game(opening,opening,NULL,&solve_error);
    game_state *solved=solution?execute_move(opening,solution):NULL;
    if(!solved||!solved->completed)fatal("Generated puzzle not solvable by the original solver");
    unsigned char *active=compute_active(solved,solved->width/2,solved->height/2);
    int active_count=0,nonempty_count=0;
    for(int i=0;i<solved->width*solved->height;i++) {
        if(active[i])active_count++;
        if(solved->tiles[i]&15)nonempty_count++;
    }
    sfree(active);
    printf("{\"width\":%d,\"height\":%d,\"x\":%d,\"y\":%d,\"point\":[%d,%d],\"move\":",state->width,state->height,x,y,px,py);
    if(real_move)printf("\"%s\"",move);else printf("null");
    printf(",\"tile_before\":%u,\"tile_after\":%u,\"locked\":%s,\"completed_before\":%s,\"completed_after\":%s,\"tiles_before\":",
        tile(state,x,y),tile(after,x,y),tile(state,x,y)&LOCKED?"true":"false",
        state->completed?"true":"false",after->completed?"true":"false");
    print_tiles(state);printf(",\"tiles_after\":");print_tiles(after);
    printf(",\"solution\":\"%s\",\"solved_tiles\":",solution);print_tiles(solved);
    printf(",\"original_solver_completed\":true,\"solved_active_count\":%d,\"solved_nonempty_count\":%d}\n",active_count,nonempty_count);
    free_game(solved);sfree(solution);free_game(after);
    if(real_move)sfree(move);free_ui(ui);free_game(state);free_game(opening);free_params(params);sfree(id);
    return 0;
}
