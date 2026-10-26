/* SPDX-License-Identifier: MIT. Native-only research; never a PXA Guest. */
#include "../common/retro-games/puzzles/vendor/towers.c"

char *__wrap_getenv(const char *name) {(void)name;return NULL;}
static int polygons,triangles,number_texts,clue_texts,pencil_texts,clips;
void __wrap_draw_polygon(drawing *dr,const int *coords,int n,int fill,int outline) {
    (void)dr;(void)coords;(void)fill;(void)outline;assert(n==3 || n==4);polygons++;triangles+=(n==3);
}
void __wrap_draw_text(drawing *dr,int x,int y,int fonttype,int fontsize,int align,int colour,const char *text) {
    (void)dr;(void)x;(void)y;(void)fonttype;(void)align;assert(text[0]>='1' && text[0]<='5' && !text[1]);
    if(fontsize==18)number_texts++;else if(fontsize==14)clue_texts++;else if(colour==COL_PENCIL)pencil_texts++;
}
void __wrap_clip(drawing *dr,int x,int y,int w,int h) {(void)dr;(void)x;(void)y;assert(w==37 && h==37);clips++;}

static void point(const game_state *s,const game_drawstate *ds,int x,int y,int *px,int *py) {
    int h=s->grid[y*s->par.w+x];
    *px=COORD(x)+TILESIZE/2+X_3D_DISP(h,s->par.w);
    *py=COORD(y)+TILESIZE/2-Y_3D_DISP(h,s->par.w);
}
static void select_cell(const game_state *s,game_ui *ui,const game_drawstate *ds,int x,int y,int button) {
    int px,py;point(s,ds,x,y,&px,&py);
    char *move=interpret_move(s,ui,ds,px,py,button);
    assert(move==MOVE_UI_UPDATE && ui->hx==x && ui->hy==y && ui->hshow);
    assert(ui->hpencil==(button==RIGHT_BUTTON) && !ui->hcursor);
    assert(!interpret_move(s,ui,ds,px,py,button+3));
    assert(!interpret_move(s,ui,ds,px,py,button+6));
}
static void apply(game_state **s,game_ui *ui,char *move) {
    assert(move && move!=MOVE_UI_UPDATE && move!=MOVE_NO_EFFECT && move!=MOVE_UNUSED && move[0]!='S');
    game_state *next=execute_move(*s,move);assert(next && !next->cheated);
    game_changed_state(ui,*s,next);free_game(*s);*s=next;sfree(move);
}
static char *input_number(game_state **s,game_ui *ui,const game_drawstate *ds,int x,int y,int button,int key) {
    select_cell(*s,ui,ds,x,y,button);
    char *move=interpret_move(*s,ui,ds,0,0,key);
    assert(move && move!=MOVE_UI_UPDATE && !ui->hshow);
    char *copy=dupstr(move);apply(s,ui,move);return copy;
}
static void enter(game_state **s,game_ui *ui,const game_drawstate *ds,int x,int y,int button,int key) {
    sfree(input_number(s,ui,ds,x,y,button,key));
}
static game_state *base(const game_params *p,const char *desc) {
    assert(!validate_desc(p,desc));return new_game(NULL,p,desc);
}
static int visibility(const game_state *s,int clue) {
    int start,step,best=0,n=0;STARTSTEP(start,step,clue,s->par.w);
    for(int j=0;j<s->par.w;j++) if(s->grid[start+j*step]>best) {best=s->grid[start+j*step];n++;}
    return n;
}
static void reset(game_state **s,game_ui **ui,const game_params *p,const char *desc) {
    free_ui(*ui);free_game(*s);*s=base(p,desc);*ui=new_ui(*s);
}

static void one_sample(int sample) {
    game_params *p=default_params();assert(p->w==5 && p->diff==DIFF_EASY);
    char *params=encode_params(p,true);assert(!strcmp(params,"5de"));
    int nkeys;key_label *keys=game_request_keys(p,&nkeys);assert(nkeys==6);
    for(int i=0;i<5;i++)assert(keys[i].button=='1'+i);assert(keys[5].button=='\b');sfree(keys);
    char seed[60];sprintf(seed,"towers-native-research-%d",sample);
    random_state *rs=random_new(seed,strlen(seed));char *aux=NULL;
    char *desc=new_game_desc(p,rs,&aux,false);assert(!validate_desc(p,desc));random_free(rs);
    game_state *s=base(p,desc);game_ui *ui=new_ui(s);assert(ui->three_d && !ui->pencil_keep_highlight && !ui->hshow);
    for(int i=0;i<25;i++)assert(!s->grid[i] && !s->clues->immutable[i]);
    for(int i=0;i<20;i++)assert(s->clues->clues[i]>0 && s->clues->clues[i]<=5);
    game_drawstate *ds=game_new_drawstate(NULL,s);game_set_size(NULL,ds,p,37);
    int bw,bh;game_compute_size(p,37,ui,&bw,&bh);assert(bw==267 && bh==267);
    game_compute_size(p,38,ui,&bw,&bh);assert(bh==274);
    const char *error=NULL;char *solution=solve_game(s,s,NULL,&error);
    assert(solution && solution[0]=='S' && strlen(solution)==26 && !error);
    for(int i=0;i<25;i++)assert(solution[i+1]>='1' && solution[i+1]<='5');
    assert(!execute_move(s,"BAD") && !execute_move(s,"R-1,0,1") && !execute_move(s,"R0,0,6") && !execute_move(s,"D-1,-1"));

    /* Original ink, mouse-highlight clearing, outside cancellation and BS. */
    char *m=input_number(&s,ui,ds,0,0,LEFT_BUTTON,'5');assert(!strcmp(m,"R0,0,5"));sfree(m);assert(s->grid[0]==5 && !s->pencil[0]);
    select_cell(s,ui,ds,0,0,LEFT_BUTTON);assert(!interpret_move(s,ui,ds,0,0,'6') && ui->hshow);
    assert(interpret_move(s,ui,ds,-200,-200,LEFT_BUTTON)==MOVE_UI_UPDATE && !ui->hshow);
    assert(!interpret_move(s,ui,ds,0,0,'1'));
    m=input_number(&s,ui,ds,0,0,LEFT_BUTTON,'\b');assert(!strcmp(m,"R0,0,0"));sfree(m);assert(!s->grid[0]);
    enter(&s,ui,ds,0,0,RIGHT_BUTTON,'1');assert(s->pencil[0]==(1<<1) && !s->grid[0]);
    enter(&s,ui,ds,0,0,RIGHT_BUTTON,'2');assert(s->pencil[0]==((1<<1)|(1<<2)));
    enter(&s,ui,ds,0,0,RIGHT_BUTTON,'1');assert(s->pencil[0]==(1<<2));
    m=input_number(&s,ui,ds,0,0,RIGHT_BUTTON,'\b');assert(!strcmp(m,"R0,0,0"));sfree(m);assert(!s->grid[0] && !s->pencil[0]);

    /* All four original outer clue markers toggle twice and preserve the grid. */
    for(int side=0;side<4;side++) {
        int i=5*side,x,y;CLUEPOS(x,y,i,5);int px=COORD(x)+TILESIZE/2,py=COORD(y)+TILESIZE/2;
        for(int twice=0;twice<2;twice++) {
            m=interpret_move(s,ui,ds,px,py,LEFT_BUTTON);assert(m && m[0]=='D');apply(&s,ui,m);
            assert(s->clues_done[i]==(twice==0));
        }
    }
    for(int i=0;i<25;i++)assert(!s->grid[i]);

    /* Duplicate Latin values produce the original cell error mask. */
    enter(&s,ui,ds,0,0,LEFT_BUTTON,'1');enter(&s,ui,ds,1,0,LEFT_BUTTON,'1');
    bool err[49];assert(check_errors(s,err) && err[1*7+1] && err[1*7+2] && !s->completed);
    reset(&s,&ui,p,desc);

    /* Partial visible-tower mismatch is knowable; D marking does not suppress it. */
    int start,step,c=s->clues->clues[0];STARTSTEP(start,step,0,5);
    enter(&s,ui,ds,start%5,start/5,LEFT_BUTTON,c==1?'1':'5');
    assert(check_errors(s,err) && err[1]);
    int cx,cy;CLUEPOS(cx,cy,0,5);
    m=interpret_move(s,ui,ds,COORD(cx)+TILESIZE/2,COORD(cy)+TILESIZE/2,LEFT_BUTTON);apply(&s,ui,m);
    assert(s->clues_done[0] && check_errors(s,err) && err[1]);
    reset(&s,&ui,p,desc);

    /* A full Latin square still fails if the original four-side clues disagree. */
    for(int i=0;i<25;i++)enter(&s,ui,ds,i%5,i/5,LEFT_BUTTON,'1'+((solution[i+1]-'1'+1)%5));
    assert(check_errors(s,err) && !s->completed && !s->cheated);
    for(int y=0;y<5;y++)for(int x=0;x<5;x++)assert(!err[(y+1)*7+(x+1)]);
    int clue_errors=0;for(int i=0;i<20;i++){int x,y;CLUEPOS(x,y,i,5);clue_errors+=err[(y+1)*7+x+1];}assert(clue_errors>0);
    reset(&s,&ui,p,desc);

    /* Original decoder accepts a consistent immutable-given fixture. */
    char *given=dupstr(desc);char *comma=strchr(given,',');if(comma)*comma=0;
    char *gdesc=snewn(strlen(given)+8,char);sprintf(gdesc,"%s,%cx",given,solution[1]);sfree(given);
    assert(!validate_desc(p,gdesc));game_state *g=base(p,gdesc);game_ui *gu=new_ui(g);
    assert(g->clues->immutable[0]==solution[1]-'0');
    int gx,gy;point(g,ds,0,0,&gx,&gy);
    assert(interpret_move(g,gu,ds,gx,gy,LEFT_BUTTON)==MOVE_UI_UPDATE && !gu->hshow);
    assert(!interpret_move(g,gu,ds,0,0,'\b') && !execute_move(g,"R0,0,0") && !execute_move(g,"P0,0,1"));
    free_ui(gu);free_game(g);

    printf("{\"sample\":%d,\"params\":\"%s\",\"id\":\"%s:%s\",\"keys\":[49,50,51,52,53,8],\"three_d\":true,\"tile\":37,\"board\":[267,267],\"ordinary_solution\":[",sample,params,params,desc);
    game_state *before_last=NULL;
    for(int i=0;i<25;i++) {
        if(i==24)before_last=dup_game(s);
        int px,py;point(s,ds,i%5,i/5,&px,&py);
        m=input_number(&s,ui,ds,i%5,i/5,LEFT_BUTTON,solution[i+1]);
        printf("%s{\"cell\":[%d,%d],\"point\":[%d,%d],\"key\":%d,\"move\":\"%s\"}",i?",":"",i%5,i/5,px,py,solution[i+1],m);sfree(m);
    }
    assert(before_last && !before_last->completed && s->completed && !s->cheated && game_status(s)==1 && !check_errors(s,err));
    assert(game_flash_length(before_last,s,1,ui)==0.4F && game_anim_length(before_last,s,1,ui)==0.0F);
    printf("],\"grid\":[");for(int i=0;i<25;i++)printf("%s%d",i?",":"",s->grid[i]);
    printf("],\"clues\":[");for(int i=0;i<20;i++){assert(visibility(s,i)==s->clues->clues[i]);printf("%s{\"expected\":%d,\"visible\":%d}",i?",":"",s->clues->clues[i],visibility(s,i));}
    printf("],\"immutable_fixture\":\"%s:%s\",\"protruding_tops\":[",params,gdesc);
    int probed=0;
    for(int i=0;i<25;i++)if(s->grid[i]==5) {
        int x=i%5,y=i/5,px=COORD(x)+TILESIZE/2,py=COORD(y)-Y_3D_DISP(5,5)+1;
        game_ui *probe=new_ui(s);
        assert(FROMCOORD(py)==y-1);
        assert(interpret_move(s,probe,ds,px,py,LEFT_BUTTON)==MOVE_UI_UPDATE && probe->hx==x && probe->hy==y && probe->hshow);
        printf("%s{\"cell\":[%d,%d],\"point\":[%d,%d],\"flat_grid_row\":%d,\"native_selected_row\":%d}",probed?",":"",x,y,px,py,FROMCOORD(py),probe->hy);probed++;free_ui(probe);
    }
    assert(probed==5);
    polygons=triangles=number_texts=clue_texts=pencil_texts=clips=0;
    game_redraw(NULL,ds,NULL,s,1,ui,0,0);
    assert(polygons>75 && number_texts>=25 && clue_texts>=20 && clips>0);
    printf("],\"draw_probe\":{\"polygons\":%d,\"triangles\":%d,\"ink_text_size_18\":%d,\"clue_text_size_14\":%d,\"clip_calls\":%d},\"latin_valid_wrong_visibility_rejected\":true,\"partial_visibility_error_and_done_marker_checked\":true,\"duplicate_latin_error_checked\":true,\"given_immutable\":true,\"pencil_toggle_and_backspace\":true,\"outside_selection_cancels\":true,\"all_four_clue_marker_sides\":true,\"invalid_and_drag_release_rejected\":true,\"status\":1,\"completed\":true,\"cheated\":false,\"solver_S_executed\":false,\"flash_seconds\":0.4,\"animation_seconds\":0.0,\"passed\":true}",polygons,triangles,number_texts,clue_texts,clips);
    free_game(before_last);sfree(gdesc);sfree(solution);sfree(aux);sfree(desc);sfree(params);free_ui(ui);game_free_drawstate(NULL,ds);free_game(s);free_params(p);
}
int main(void) {
    printf("{\"native_only\":true,\"no_pxa_module_build_or_gui\":true,\"samples\":[");
    for(int i=0;i<8;i++){if(i)putchar(',');one_sample(i);}puts("],\"passed\":true}");return 0;
}
