/* SPDX-License-Identifier: MIT. Test-only; original backend and pointer rules. */
#include "../common/retro-games/puzzles/vendor/loopy.c"
#include <stdio.h>

/* Match the frozen Guest's platform environment boundary, not game rules. */
char *__wrap_getenv(const char *name) { (void)name; return NULL; }

static void midpoint(const game_state *s, const game_drawstate *ds, int index,
                     int *x, int *y) {
    grid_edge *e = s->game_grid->edges[index];
    int x1, y1, x2, y2;
    grid_to_screen(ds, s->game_grid, e->dot1->x, e->dot1->y, &x1, &y1);
    grid_to_screen(ds, s->game_grid, e->dot2->x, e->dot2->y, &x2, &y2);
    *x = (x1 + x2) / 2; *y = (y1 + y2) / 2;
}

static char *tap(game_state **state, game_ui *ui, game_drawstate *ds,
                 int index, int button) {
    int x, y; midpoint(*state, ds, index, &x, &y);
    char *move = interpret_move(*state, ui, ds, x, y, button);
    assert(move && move != MOVE_UNUSED && move != MOVE_UI_UPDATE && move != MOVE_NO_EFFECT);
    assert(move[0] != 'S' && atoi(move) == index);
    game_state *next = execute_move(*state, move); assert(next);
    assert(!interpret_move(next, ui, ds, x, y, button + 3));
    assert(!interpret_move(next, ui, ds, x, y, button + 6));
    free_game(*state); *state = next;
    return move;
}

static void edge_tap(game_state **s, game_ui *ui, game_drawstate *ds,
                     int index, int button) {
    char *move = tap(s, ui, ds, index, button); sfree(move);
}

static int errors(const game_state *s) {
    int count = 0;
    for (int i = 0; i < s->game_grid->num_edges; i++) count += s->line_errors[i];
    return count;
}

static void dump(const game_state *s) {
    printf("{\"lines\":[");
    for (int i = 0; i < s->game_grid->num_edges; i++)
        printf("%s%d", i ? "," : "", s->lines[i]);
    printf("],\"line_errors\":%d,\"exactly_one_loop\":%s,\"status\":%d,\"cheated\":%s}",
           errors(s), s->exactly_one_loop ? "true" : "false", game_status(s),
           s->cheated ? "true" : "false");
}

static game_state *fixture(game_params *p, const char *description) {
    assert(!validate_params(p, true) && !validate_desc(p, description));
    game_state *s = new_game(NULL, p, description);
    assert(!s->solved && !s->cheated && !s->exactly_one_loop);
    return s;
}

static void self_test(void) {
    game_params *p = default_params();
    char *encoded = encode_params(p, true);
    assert(!strcmp(encoded, "10x10t0de")); sfree(encoded);
    int bw, bh; game_compute_size(p, 24, NULL, &bw, &bh);
    assert(bw == 265 && bh == 265);
    game_compute_size(p, 25, NULL, &bw, &bh); assert(bh == 275);
    game_state *s = fixture(p, "zzzv");
    assert(s->game_grid->num_faces == 100 && s->game_grid->num_edges == 220 && s->game_grid->num_dots == 121);
    game_ui *ui = new_ui(s); assert(ui->autofollow == AF_OFF && ui->draw_faint_lines);
    game_drawstate *ds = game_new_drawstate(NULL, s); game_set_size(NULL, ds, p, 24);
    assert(!execute_move(s, "BAD") && !execute_move(s, "999y") && !execute_move(s, "-1y"));
    assert(!interpret_move(s, ui, ds, -200, -200, LEFT_BUTTON));
    for (int i = 0; i < 220; i++) {
        int x, y; midpoint(s, ds, i, &x, &y);
        char *move = interpret_move(s, ui, ds, x, y, LEFT_BUTTON);
        assert(move && atoi(move) == i && move[strlen(move)-1] == 'y'); sfree(move);
        grid_edge *e = s->game_grid->edges[i];
        for (int sign = -1; sign <= 1; sign += 2) {
            int tx=x, ty=y;
            if (e->dot1->x == e->dot2->x) tx += sign * 6; else ty += sign * 6;
            move = interpret_move(s, ui, ds, tx, ty, LEFT_BUTTON);
            assert(move && atoi(move) == i); sfree(move);
        }
    }
    edge_tap(&s, ui, ds, 0, LEFT_BUTTON); assert(s->lines[0] == LINE_YES);
    edge_tap(&s, ui, ds, 0, LEFT_BUTTON); assert(s->lines[0] == LINE_UNKNOWN);
    edge_tap(&s, ui, ds, 0, RIGHT_BUTTON); assert(s->lines[0] == LINE_NO);
    edge_tap(&s, ui, ds, 0, LEFT_BUTTON); assert(s->lines[0] == LINE_UNKNOWN);
    edge_tap(&s, ui, ds, 0, RIGHT_BUTTON); assert(s->lines[0] == LINE_NO);
    edge_tap(&s, ui, ds, 0, MIDDLE_BUTTON); assert(s->lines[0] == LINE_UNKNOWN);
    edge_tap(&s, ui, ds, 0, LEFT_BUTTON); assert(s->lines[0] == LINE_YES);
    edge_tap(&s, ui, ds, 0, RIGHT_BUTTON); assert(s->lines[0] == LINE_UNKNOWN);
    edge_tap(&s, ui, ds, 0, LEFT_BUTTON); assert(s->lines[0] == LINE_YES);
    edge_tap(&s, ui, ds, 0, MIDDLE_BUTTON); assert(s->lines[0] == LINE_UNKNOWN);
    assert(!s->solved && !s->cheated);
    free_ui(ui); game_free_drawstate(NULL, ds); free_game(s);

    /* Legal minimum square board; completion through ordinary pointer moves. */
    p->w = p->h = 3;
    s = fixture(p, "212101212"); ui = new_ui(s); ds = game_new_drawstate(NULL, s);
    game_set_size(NULL, ds, p, 24);
    int perimeter = 0, excluded = 0;
    for (int i = 0; i < s->game_grid->num_edges; i++) {
        grid_edge *e = s->game_grid->edges[i];
        if (e->face1 && e->face2) { edge_tap(&s, ui, ds, i, RIGHT_BUTTON); excluded++; }
    }
    for (int i = 0; i < s->game_grid->num_edges; i++) {
        grid_edge *e = s->game_grid->edges[i];
        if (!e->face1 || !e->face2) { edge_tap(&s, ui, ds, i, LEFT_BUTTON); perimeter++; }
    }
    assert(perimeter == 12 && excluded == 12 && s->exactly_one_loop && s->solved && !s->cheated && game_status(s) == 1 && errors(s) == 0);
    for (int i = 0; i < 9; i++) assert(face_order(s, i, LINE_YES) == s->clues[i]);
    free_ui(ui); game_free_drawstate(NULL, ds); free_game(s);

    /* One geometrical loop is insufficient if its clue is wrong. */
    s = fixture(p, "212101212"); ui = new_ui(s); ds = game_new_drawstate(NULL, s);
    game_set_size(NULL, ds, p, 24);
    for (int j = 0; j < 4; j++) { int edge=s->game_grid->faces[4]->edges[j]->index; edge_tap(&s, ui, ds, edge, LEFT_BUTTON); }
    assert(s->exactly_one_loop && !s->solved && !s->cheated && face_order(s, 4, LINE_YES) == 4 && s->clues[4] == 0);
    game_redraw(NULL, ds, NULL, s, 1, ui, 0, 0); assert(ds->clue_error[4]);
    free_ui(ui); game_free_drawstate(NULL, ds); free_game(s);

    /* Two disjoint loops never pass. Keep two paths until both closures. */
    s = fixture(p, "i"); ui = new_ui(s); ds = game_new_drawstate(NULL, s); game_set_size(NULL, ds, p, 24);
    for (int f = 0; f <= 8; f += 8)
        for (int j = 0; j < 3; j++) { int edge=s->game_grid->faces[f]->edges[j]->index; edge_tap(&s, ui, ds, edge, LEFT_BUTTON); assert(!s->solved); }
    for (int f = 0; f <= 8; f += 8) { int edge=s->game_grid->faces[f]->edges[3]->index; edge_tap(&s, ui, ds, edge, LEFT_BUTTON); assert(!s->solved); }
    assert(!s->exactly_one_loop && game_status(s) == 0 && errors(s) == 4);
    free_ui(ui); game_free_drawstate(NULL, ds); free_game(s);

    s = fixture(p, "i"); ui = new_ui(s); ds = game_new_drawstate(NULL, s); game_set_size(NULL, ds, p, 24);
    grid_dot *junction=NULL;
    for (int i=0; i<s->game_grid->num_dots; i++) if (s->game_grid->dots[i]->order == 4) { junction=s->game_grid->dots[i]; break; }
    assert(junction);
    for (int j=0; j<3; j++) { int edge=junction->edges[j]->index; edge_tap(&s, ui, ds, edge, LEFT_BUTTON); }
    assert(!s->solved && !s->exactly_one_loop && errors(s) == 3);
    free_ui(ui); game_free_drawstate(NULL, ds); free_game(s); free_params(p);
    puts("{\"passed\":true,\"default_params\":\"10x10t0de\",\"tile\":24,\"board\":[265,265],\"faces\":100,\"edges\":220,\"dots\":121,\"all_edge_midpoints_and_six_pixel_offsets\":true,\"left_right_middle_cycles\":true,\"drag_release_no_move\":true,\"invalid_and_outside_rejected\":true,\"uncheated_pointer_completion\":{\"fixture\":\"3x3t0de:212101212\",\"yes_edges\":12,\"no_edges\":12,\"status\":1,\"cheated\":false},\"wrong_clue_rejected_and_drawn_as_error\":true,\"two_disjoint_loops_rejected\":true,\"three_way_vertex_rejected\":true}");
}

static void solution_plan(game_params *p, game_state *s, game_ui *ui,
                          game_drawstate *ds) {
    const char *error=NULL;
    char *encoded=solve_game(s,s,NULL,&error); assert(encoded && encoded[0]=='S');
    int n=s->game_grid->num_edges, count=0;
    int *target=snewn(n,int);
    for (int i=0;i<n;i++) target[i]=LINE_UNKNOWN;
    const char *c=encoded+1;
    while (*c) {
        char *end; int edge=(int)strtol(c,&end,10);assert(end!=c && edge>=0 && edge<n);
        assert(target[edge]==LINE_UNKNOWN && (*end=='y' || *end=='n'));
        target[edge]=(*end=='y')?LINE_YES:LINE_NO;count++;c=end+1;
    }
    assert(count==n); /* Never apply the solver's S command to either runtime. */
    printf("{\"test_only_solver\":true,\"solver_command_sent_to_guest\":false,\"edges\":%d,\"steps\":[",n);
    int step=0, yes=0, no=0; game_state *before_last=NULL;
    for (int pass=0;pass<2;pass++) for (int edge=0;edge<n;edge++) {
        if (target[edge] != (pass?LINE_YES:LINE_NO)) continue;
        assert(!s->solved && !s->cheated);
        int x,y;midpoint(s,ds,edge,&x,&y);
        if (step==n-1) before_last=dup_game(s);
        char *move=tap(&s,ui,ds,edge,pass?LEFT_BUTTON:RIGHT_BUTTON);
        printf("%s{\"edge\":%d,\"point\":[%d,%d],\"mode\":%d,\"move\":\"%s\"}",
               step?",":"",edge,x,y,pass?0:1,move);
        sfree(move);step++;if(pass)yes++;else no++;
    }
    assert(step==n && before_last && !before_last->solved);
    assert(s->solved && s->exactly_one_loop && !s->cheated && errors(s)==0);
    assert(game_flash_length(before_last,s,1,ui)==FLASH_TIME);
    printf("],\"yes_edges\":%d,\"no_edges\":%d,\"before_closure\":",yes,no);dump(before_last);
    printf(",\"complete\":");dump(s);
    printf(",\"faces\":[");
    for (int f=0;f<s->game_grid->num_faces;f++) {
        int actual=face_order(s,f,LINE_YES);
        assert(s->clues[f]<0 || actual==s->clues[f]);
        printf("%s{\"clue\":%d,\"yes_count\":%d}",f?",":"",s->clues[f],actual);
    }
    printf("],\"vertex_degrees\":[");
    for (int d=0;d<s->game_grid->num_dots;d++) {
        grid_dot *dot=s->game_grid->dots[d];int degree=0;
        for (int e=0;e<dot->order;e++) degree+=(s->lines[dot->edges[e]->index]==LINE_YES);
        assert(degree==0 || degree==2);printf("%s%d",d?",":"",degree);
    }
    printf("],\"expected_original_flash_seconds\":%.1f,\"ordinary_pointer_completion\":true}\n",(double)FLASH_TIME);
    sfree(target);sfree(encoded);free_game(before_last);free_ui(ui);game_free_drawstate(NULL,ds);free_game(s);
}

int main(int argc, char **argv) {
    if (argc == 2 && !strcmp(argv[1], "--self-test")) { self_test(); return 0; }
    bool solution=(argc==4 && !strcmp(argv[1],"--solution"));
    assert(argc == 3 || solution);
    char *id=dupstr(argv[solution?2:1]), *desc=strchr(id, ':'); assert(desc); *desc++=0;
    game_params *p=default_params(); decode_params(p, id); assert(!validate_params(p, true) && !validate_desc(p, desc));
    game_state *s=new_game(NULL, p, desc); game_ui *ui=new_ui(s);
    game_drawstate *ds=game_new_drawstate(NULL, s); game_set_size(NULL, ds, p, atoi(argv[solution?3:2]));
    if (solution) {solution_plan(p,s,ui,ds);free_params(p);sfree(id);return 0;}
    int left=0, right=-1, lx,ly,rx,ry,bw,bh; grid_edge *a=s->game_grid->edges[left];
    for (int i=1; i<s->game_grid->num_edges; i++) {
        grid_edge *b=s->game_grid->edges[i];
        if (a->dot1!=b->dot1 && a->dot1!=b->dot2 && a->dot2!=b->dot1 && a->dot2!=b->dot2) { right=i; break; }
    }
    assert(right>=0); midpoint(s,ds,left,&lx,&ly); midpoint(s,ds,right,&rx,&ry);
    game_compute_size(p, ds->tilesize, ui, &bw, &bh);
    printf("{\"width\":%d,\"height\":%d,\"board\":[%d,%d],\"edges\":%d,\"left_edge\":%d,\"right_edge\":%d,\"left\":[%d,%d],\"right\":[%d,%d],\"middle\":[%d,%d],\"initial\":", p->w,p->h,bw,bh,s->game_grid->num_edges,left,right,lx,ly,rx,ry,rx,ry); dump(s);
    assert(!execute_move(s,"BAD"));
    char *m1=tap(&s,ui,ds,left,LEFT_BUTTON); assert(s->lines[left]==LINE_YES && errors(s)==0);
    printf(",\"move1\":\"%s\",\"after1\":",m1); dump(s);
    char *m2=tap(&s,ui,ds,right,RIGHT_BUTTON); assert(s->lines[right]==LINE_NO && s->lines[left]==LINE_YES && errors(s)==0);
    printf(",\"move2\":\"%s\",\"after2\":",m2); dump(s);
    char *m3=tap(&s,ui,ds,right,MIDDLE_BUTTON); assert(s->lines[right]==LINE_UNKNOWN && s->lines[left]==LINE_YES && !s->cheated);
    printf(",\"move3\":\"%s\",\"after3\":",m3); dump(s);
    printf(",\"invalid_move_rejected\":true,\"rule_checks\":true}\n");
    sfree(m1);sfree(m2);sfree(m3);free_ui(ui);game_free_drawstate(NULL,ds);free_game(s);free_params(p);sfree(id);
    return 0;
}
