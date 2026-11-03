/* SPDX-License-Identifier: MIT. Input selects actions; all rules stay in OPR. */
#include "ui.h"
#include <string.h>
static void message(opr_ui *u, const char *s) {
    unsigned i=0; while (s[i] && i+1<sizeof(u->message)) { u->message[i]=s[i]; i++; } u->message[i]=0;
}
static void start(opr_ui *u, int cls) {
    u->selected_class=cls; opr_session_start(&u->session, cls, u->seed); u->scene=OPR_UI_PLAY;
    message(u,"EXPLORE THE FOG AND FIND STAIRS");
}
static void pause_set(opr_ui *u, int pause) {
    opr_session_pause(&u->session,pause); u->scene=pause?OPR_UI_PAUSE:OPR_UI_PLAY; u->controller=0;
}
void opr_ui_init(opr_ui *u) { memset(u,0,sizeof(*u)); u->seed=1; u->scene=OPR_UI_SELECT; message(u,"CHOOSE A CLASS TO START A FIVE FLOOR RUN"); }
void opr_ui_background(opr_ui *u,int b) { u->background=!!b; u->controller=0; }
void opr_ui_back(opr_ui *u) {
    if (u->scene==OPR_UI_HELP) { u->scene=u->help_return; return; }
    if (u->scene==OPR_UI_PLAY) pause_set(u,1);
    else if (u->scene==OPR_UI_PAUSE) pause_set(u,0);
    else if (u->session.active) { u->seed=u->session.seed; pause_set(u,0); }
}
void opr_ui_action(opr_ui *u,opr_action a) {
    if (u->scene!=OPR_UI_PLAY || u->background) return;
    OPR_Player before=u->session.game.player;
    int combat=u->session.game.inCombat, count=u->session.game.entityCount;
    if (!opr_session_action(&u->session,a)) { message(u,"NO TURN - WALL OR UNAVAILABLE ACTION"); return; }
    const OPR_GameState *g=&u->session.game;
    if(g->victory) message(u,"BOSS DEFEATED - DUNGEON CLEARED");
    else if(g->gameOver) message(u,"YOU FELL - REPLAY OR CHOOSE A NEW RUN");
    else if(g->player.floor!=before.floor) message(u,"DESCENDED - HEALTH PARTLY RESTORED");
    else if(g->player.level!=before.level) message(u,"LEVEL UP - HEALTH RESTORED");
    else if(!combat&&g->inCombat) message(u,"ENEMY ENGAGED - ATTACK POTION OR FLEE");
    else if(combat&&!g->inCombat) message(u,count>g->entityCount?"ENEMY DEFEATED - EXPERIENCE GAINED":"YOU ESCAPED THE COMBAT");
    else if(a==OPR_ACT_POTION) message(u,"POTION USED - THE ENEMY COUNTERATTACKS");
    else if(a==OPR_ACT_FLEE) message(u,"ESCAPE FAILED - THE ENEMY STRUCK");
    else if(combat) message(u,"YOU ATTACKED - THE ENEMY COUNTERATTACKS");
    else if(g->player.weaponAtk!=before.weaponAtk) message(u,"CHEST - WEAPON UPGRADED");
    else if(g->player.armorDef!=before.armorDef) message(u,"CHEST - ARMOR UPGRADED");
    else if(g->player.atk!=before.atk||g->player.def!=before.def) message(u,"SHRINE - HEALTH RESTORED AND STATS RAISED");
    else if(g->player.potions>before.potions) message(u,"POTIONS COLLECTED");
    else if(g->player.xp>before.xp) message(u,"CHEST - EXPERIENCE GAINED");
    else message(u,"EXPLORE THE FOG AND FIND STAIRS");
}
void opr_ui_pointer(opr_ui *u,int x,int y) {
    if(u->background) return;
    if(u->scene==OPR_UI_HELP) { if(y>=350) u->scene=u->help_return; return; }
    if(u->scene==OPR_UI_SELECT) {
        if(x>=40&&x<360&&y>=130&&y<274) { int cls=(y-130)/50; if(cls<3&&(y-130)%50<44) start(u,cls); }
        else if(x>=40&&x<195&&y>=290&&y<328) { uint32_t s=u->seed+UINT32_C(0x9e3779b9); s^=s<<13;s^=s>>17;s^=s<<5;u->seed=s?s:1; }
        else if(x>=205&&x<360&&y>=290&&y<328) { u->seed=1; }
        else if(x>=40&&x<195&&y>=342&&y<382&&u->session.active) { u->seed=u->session.seed; u->selected_class=u->session.game.player.classId; pause_set(u,0); }
        else if(x>=205&&x<360&&y>=342&&y<382) { u->help_return=OPR_UI_SELECT;u->scene=OPR_UI_HELP; }
        return;
    }
    if(u->scene==OPR_UI_PAUSE) {
        if(x>=80&&x<320&&y>=155&&y<199) pause_set(u,0);
        else if(x>=80&&x<320&&y>=215&&y<259) { u->scene=OPR_UI_SELECT; message(u,"SELECT A CLASS TO REPLACE THIS RUN"); }
        else if(x>=80&&x<320&&y>=275&&y<319) { u->help_return=OPR_UI_PAUSE;u->scene=OPR_UI_HELP; }
        return;
    }
    if(y>=8&&y<38) {
        if(x>=8&&x<92) { opr_session_pause(&u->session,1); u->scene=OPR_UI_SELECT; u->controller=0; }
        else if(x>=306&&x<392) pause_set(u,1);
        return;
    }
    if(u->session.game.gameOver||u->session.game.victory) {
        if(x>=80&&x<320&&y>=215&&y<257) start(u,u->session.game.player.classId);
        return;
    }
    if(x>=309&&x<355&&y>=119&&y<159) opr_ui_action(u,OPR_ACT_UP);
    else if(x>=268&&x<309&&y>=164&&y<204) opr_ui_action(u,OPR_ACT_LEFT);
    else if(x>=355&&x<396&&y>=164&&y<204) opr_ui_action(u,OPR_ACT_RIGHT);
    else if(x>=309&&x<355&&y>=209&&y<249) opr_ui_action(u,OPR_ACT_DOWN);
    else if(x>=268&&x<396&&y>=270&&y<308) opr_ui_action(u,OPR_ACT_ATTACK);
    else if(x>=268&&x<396&&y>=314&&y<352) opr_ui_action(u,OPR_ACT_POTION);
    else if(x>=268&&x<396&&y>=358&&y<396) opr_ui_action(u,OPR_ACT_FLEE);
}
void opr_ui_controller(opr_ui *u,uint32_t buttons) {
    uint32_t edges=buttons&~u->controller; u->controller=buttons;
    if(u->background) return;
    if(edges&64) { opr_ui_back(u); return; }
    if(u->scene==OPR_UI_SELECT) {
        if(edges&1) u->selected_class=(u->selected_class+2)%3;
        else if(edges&2) u->selected_class=(u->selected_class+1)%3;
        else if(edges&16) start(u,u->selected_class);
        return;
    }
    if(u->scene!=OPR_UI_PLAY) return;
    if(edges&1) opr_ui_action(u,OPR_ACT_UP);
    else if(edges&2) opr_ui_action(u,OPR_ACT_DOWN);
    else if(edges&4) opr_ui_action(u,OPR_ACT_LEFT);
    else if(edges&8) opr_ui_action(u,OPR_ACT_RIGHT);
    else if(edges&16) opr_ui_action(u,OPR_ACT_ATTACK);
    else if(edges&32) opr_ui_action(u,OPR_ACT_POTION);
    else if(edges&128) opr_ui_action(u,OPR_ACT_FLEE);
}
