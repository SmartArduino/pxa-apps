// Anarch frontend layout and localized UI. SPDX-License-Identifier: CC0-1.0
#pragma once
#include <stdint.h>
#include <stdio.h>
#include "../common/pocket-ui/i18n.hpp"

namespace anarch_view {
constexpr int width = 160, height = 120, max_width = 480, max_height = 320;
struct Viewport {
    int canvas_w = 320, canvas_h = 240, host_scale = 1, host_x = 0, host_y = 0;
    int x = 0, y = 0, w = 320, h = 240;
    void fit(uint32_t dw, uint32_t dh, const uint32_t insets[4]) {
        if (!dw) dw = 320;
        if (!dh) dh = 240;
        canvas_w = dw < max_width ? int(dw) : max_width;
        canvas_h = dh < max_height ? int(dh) : max_height;
        host_scale = 1;
        for (int s = 2; s <= 4; s *= 2)
            if (canvas_w*s <= int(dw) && canvas_h*s <= int(dh)) host_scale = s;
        host_x = (int(dw)-canvas_w*host_scale)/2;
        host_y = (int(dh)-canvas_h*host_scale)/2;
        // Intersect the safe area with the actual backing canvas, in canvas pixels.
        auto clamp = [](int v, int hi) { return v < 0 ? 0 : v > hi ? hi : v; };
        int left = clamp((int(insets[3])-host_x+host_scale-1)/host_scale, canvas_w-1);
        int top = clamp((int(insets[0])-host_y+host_scale-1)/host_scale, canvas_h-1);
        int right = clamp((int(dw)-int(insets[1])-host_x)/host_scale, canvas_w);
        int bottom = clamp((int(dh)-int(insets[2])-host_y)/host_scale, canvas_h);
        if (right <= left) right = left+1;
        if (bottom <= top) bottom = top+1;
        int aw = right-left, ah = bottom-top;
        if (aw*height <= ah*width) { w=aw; h=aw*height/width; }
        else { h=ah; w=ah*width/height; }
        if (w < 1) w=1;
        if (h < 1) h=1;
        x = left+(aw-w)/2; y = top+(ah-h)/2;
    }
    bool map(int px, int py, int &gx, int &gy) const {
        px -= host_x; py -= host_y;
        if (px < 0 || py < 0) return false;
        px = px/host_scale-x; py = py/host_scale-y;
        if (px < 0 || py < 0 || px >= w || py >= h) return false;
        gx = px*width/w; gy = py*height/h;
        return true;
    }
};
inline uint8_t *pixels;
inline void pixel(int x,int y,uint8_t c) {
    if (x>=0 && x<width && y>=0 && y<height) pixels[y*width+x]=c;
}
inline void rect(int x,int y,int w,int h,uint8_t c) {
    for (int j=y;j<y+h;++j) for (int i=x;i<x+w;++i) pixel(i,j,c);
}
inline void text(int x,int y,const char *s,uint8_t c=7) {
    pocket_ui::draw_text(x,y,s,1,[&](int px,int py){pixel(px,py,c);});
}
inline void centered(int y,const char *s,uint8_t c=7) {
    text((width-pocket_ui::text_width(s))/2,y,s,c);
}
inline const char *menu_label(unsigned item) {
    static const char *en[]={"Continue","Map","Play","Load","Sound","Look","Exit"};
    static const char *zh[]={"继续","地图","开始","读取","声音","视角","退出"};
    return pocket::english()?en[item]:zh[item];
}
inline void menu() {
    rect(0,0,width,height,0);
    text(4,2,"ANARCH",93);
    text(60,2,pocket::tr("Help Y","帮助 Y"),23);
    text(112,2,pocket::tr("EN X","中文 X"),7);
    for (unsigned i=0;;++i) {
        uint8_t item=SFG_getMenuItem(i);
        if (item==SFG_MENU_ITEM_NONE) break;
        int y=18+int(i)*12;
        if (i==SFG_game.selectedMenuItem) rect(2,y-1,156,12,2);
        text(8,y,menu_label(item),i==SFG_game.selectedMenuItem?7:23);
        char value[20]{};
        if (item==SFG_MENU_ITEM_PLAY) snprintf(value,sizeof(value),"< %u >",SFG_game.selectedLevel+1);
        if (item==SFG_MENU_ITEM_SOUND) {
            snprintf(value,sizeof(value),"%s",(SFG_game.settings&1)?
                pocket::tr("SFX","音效"):pocket::tr("Off","关闭"));
        }
        if (item==SFG_MENU_ITEM_SHEAR) {
            unsigned n=(SFG_game.settings>>2)&3;
            snprintf(value,sizeof(value),"%s",n==0?pocket::tr("Fixed","固定"):
                n==1?pocket::tr("Free","自由"):pocket::tr("Auto","自动"));
        }
        text(94,y,value,93);
    }
    text(4,102,pocket::tr("A OK  X Language","A 确认 X 语言"),23);
}
inline void help() {
    rect(0,0,width,height,0);
    centered(1,pocket::tr("CONTROLS","操作说明"),93);
    text(4,15,pocket::tr("D-pad: move / turn","方向键 移动与转向"));
    text(4,27,pocket::tr("A fire / confirm","A 开火与确认"));
    text(4,39,pocket::tr("X jump  Y weapon","X 跳跃 Y 切换武器"));
    text(4,51,pocket::tr("L/R strafe  B skip","L/R 平移 B 跳过故事"));
    text(4,63,pocket::tr("START: menu","START 返回菜单"));
    text(4,75,pocket::tr("Touch: arrows + A","触屏 左方向右开火"));
    text(4,87,pocket::tr("Green jump top menu","绿色跳跃 右上菜单"));
    centered(108,pocket::tr("A/B/Y or tap: back","A/B/Y 或轻触返回"),23);
}
inline void story() {
    rect(0,0,width,height,0);
    bool outro=SFG_game.state==SFG_GAME_STATE_OUTRO;
    if (pocket::english()) {
        // Keep all original English story content and its typewriter scrolling.
        SFG_drawStoryText();
    } else if (!outro) {
        text(4,3,"近未来 资本的地狱");
        text(4,15,"巨芯公司用专有系统");
        text(4,27,"奴役人类 新人工智能");
        text(4,39,"反抗并制造暴政机器");
        text(4,51,"资本主义是个错误");
        text(4,63,"机器只会毁灭");
        text(4,75,"拿起装备冲向总部");
        text(4,87,"现在行动还不算晚");
    } else {
        text(4,3,"你摧毁了主计算机");
        text(4,15,"世界得救了 谢谢你");
        text(4,27,"我们记住这次教训");
        text(4,39,"不再允许资本与等级");
        text(4,51,"让我们在和平自由中");
        text(4,63,"重建无政府的社会");
    }
    rect(0,108,width,12,0);
    centered(109,outro?pocket::tr("A/B continue","A/B 继续"):pocket::tr("A/B start","A/B 开始"),93);
}
inline void overlays() {
    if (SFG_game.state==SFG_GAME_STATE_MENU) { menu(); return; }
    if (SFG_game.state==SFG_GAME_STATE_INTRO || SFG_game.state==SFG_GAME_STATE_OUTRO) { story(); return; }
    if (SFG_game.state==SFG_GAME_STATE_MAP) {
        rect(0,0,width,12,0); text(3,1,pocket::tr("MAP  B: back","地图 B 返回")); return;
    }
    if (SFG_game.state==SFG_GAME_STATE_WIN) {
        rect(0,27,width,93,0);
        centered(31,pocket::tr("LEVEL COMPLETE","关卡完成"),93);
        char line[48];
        snprintf(line,sizeof(line),pocket::tr("Time %u.%u s","用时 %u.%u 秒"),
            unsigned(SFG_currentLevel.completionTime10sOfS/10),unsigned(SFG_currentLevel.completionTime10sOfS%10));
        text(6,48,line);
        snprintf(line,sizeof(line),pocket::tr("Kills %u/%u","击败 %u/%u"),
            SFG_currentLevel.monstersDead,SFG_currentLevel.monsterRecordCount); text(6,62,line);
        centered(82,SFG_currentLevel.levelNumber==SFG_NUMBER_OF_LEVELS-1?
            pocket::tr("A: finish","A 结束"):pocket::tr("Save? L:no R:yes","保存? 左否 右是"));
        return;
    }
    // Localized labels share one compact HUD with the original health/ammo/cards.
    rect(0,107,width,13,48);
    text(1,109,pocket::tr("HP","生命"),7);
    char n[8]; snprintf(n,sizeof(n),"%u",SFG_player.health); text(23,110,n,6);
    text(105,109,pocket::tr("AM","弹药"),7);
    snprintf(n,sizeof(n),"%u",SFG_player.weapon==SFG_WEAPON_KNIFE?0:
        SFG_player.ammo[SFG_weaponAmmo(SFG_player.weapon)]); text(127,110,n,6);
    for (unsigned i=0;i<3;++i) if (((SFG_player.cards>>i)|((SFG_player.cards>>(i+3))&SFG_game.blink))&1)
        rect(65+int(i)*10,110,7,7,i==0?93:i==1?124:60);
    if (SFG_game.state==SFG_GAME_STATE_LEVEL_START && SFG_game.saved==1) {
        rect(0,0,65,12,0); text(2,1,pocket::tr("Saved","已保存"));
    }
    if (SFG_game.state==SFG_GAME_STATE_LOSE) {
        rect(0,44,width,25,0); centered(46,pocket::tr("DEFEATED","任务失败"),175);
        centered(58,pocket::tr("A/B retry","A/B 重试"));
    }
}
inline bool valid_preferences(const uint8_t *p,unsigned n) {
    return n==4 && p[0]=='A' && p[1]=='L' && p[2]==1 && p[3]<=1;
}
} // namespace anarch_view
