// MIT. Adapted from Seth A. Robinson / nootropic design Highscores.cpp.
// The original 50-byte EEPROM layout and descending top-10 order are retained.
#include "Highscores.h"
#include "Utils.h"
#include "Player.h"
extern Player player;
char initials[3];
static uint8_t entryIndex, entryFile, entrySlot, scoresPage;
static uint32_t entryScore;
static bool entering;
void ResetHighScoresUI() {
    entryIndex = entryFile = entrySlot = scoresPage = 0;
    entryScore = 0; entering = false;
    memset(initials, ' ', sizeof(initials));
}
static uint16_t scoreAt(unsigned address) {
    uint8_t hi = EEPROM.read(address), lo = EEPROM.read(address+1);
    return hi == 255 && lo == 255 ? 0 : (uint16_t)((hi << 8) | lo);
}
bool enterHighScore(byte file) {
    for (uint8_t i=0; i<10; ++i) {
        if (player.m_score > scoreAt(file*50+i*5)) {
            entering=true; entryFile=file; entrySlot=i; entryIndex=0;
            entryScore=player.m_score; memset(initials,' ',3); return true;
        }
    }
    return false;
}
static void saveEntry() {
    uint16_t score=(uint16_t)min(entryScore,65535u);
    char letters[3]; memcpy(letters,initials,3);
    for (uint8_t slot=entrySlot; slot<10; ++slot) {
        unsigned address=entryFile*50+slot*5;
        uint16_t oldScore=scoreAt(address);
        char oldLetters[3];
        for (unsigned c=0;c<3;++c) oldLetters[c]=(char)EEPROM.read(address+2+c);
        EEPROM.write(address,(uint8_t)(score>>8)); EEPROM.write(address+1,(uint8_t)score);
        for (unsigned c=0;c<3;++c) EEPROM.write(address+2+c,(uint8_t)letters[c]);
        score=oldScore; memcpy(letters,oldLetters,3);
    }
    entering=false; scoresPage=0; g_mode=MODE_HIGHSCORES;
}
void UpdateHighScoreEntry() {
    if (!entering) {g_mode=MODE_HIGHSCORES;return;}
    static const char alphabet[]=" ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789!?";
    if (arduboy.justPressed(LEFT_BUTTON)||arduboy.justPressed(B_BUTTON)) {
        if(entryIndex) --entryIndex;
    }
    if (arduboy.justPressed(RIGHT_BUTTON) && entryIndex<2) ++entryIndex;
    int direction=(arduboy.justPressed(DOWN_BUTTON)?1:0)-(arduboy.justPressed(UP_BUTTON)?1:0);
    if(direction) {
        const char *found=strchr(alphabet,initials[entryIndex]);
        int index=found?(int)(found-alphabet):0;
        int count=sizeof(alphabet)-1;
        initials[entryIndex]=alphabet[(index+direction+count)%count];
    }
    if(arduboy.justPressed(A_BUTTON)) {
        if(entryIndex<2) ++entryIndex; else { saveEntry(); return; }
    }
    arduboy.clear(); text.setCursor(16,0); text.print("HIGH SCORE");
    text.setCursor(88,0); text.print(entryScore);
    for(unsigned i=0;i<3;++i) {
        text.setCursor(56+i*8,20); text.print(initials[i]);
        arduboy.drawLine(56+i*8,27,62+i*8,27,1);
    }
    arduboy.drawLine(56+entryIndex*8,28,62+entryIndex*8,28,1);
    text.setCursor(8,42); text.print("UD:LETTER LR:POS");
    text.setCursor(8,52); text.print("A:NEXT / SAVE");
    arduboy.display();
}
void displayHighScores(byte file) {
    if (arduboy.justPressed(LEFT_BUTTON)||arduboy.justPressed(RIGHT_BUTTON)) scoresPage^=1;
    if (arduboy.justPressed(A_BUTTON)||arduboy.justPressed(B_BUTTON)) {g_mode=MODE_MENU;return;}
    arduboy.clear(); text.setCursor(8,0); text.print("HIGH SCORES "); text.print(scoresPage+1);
    for(unsigned row=0;row<5;++row) {
        unsigned slot=scoresPage*5+row,address=file*50+slot*5;
        uint16_t score=scoreAt(address);
        text.setCursor(8,10+row*8); text.print(slot+1); text.setCursor(30,10+row*8);
        if(score) {for(unsigned c=0;c<3;++c) text.print((char)EEPROM.read(address+2+c));text.print(' ');text.print(score);}
    }
    text.setCursor(8,54);text.print("LR:PAGE A/B:BACK");arduboy.display();
}
