#include "common.h"

/*  Defines  */

#define callInitFunc(idx) moduleTable[idx].initFunc()
#define callUpdateFunc(idx) moduleTable[idx].updateFunc()
#define callDrawFunc(idx) moduleTable[idx].drawFunc()

/*  Typedefs  */

typedef struct
{
    void(*initFunc)(void);
    MODE_T(*updateFunc)(void);
    void(*drawFunc)(void);
} MODULE_FUNCS;

/*  Local Variables  */

PROGMEM static const MODULE_FUNCS moduleTable[] = {
    { initLogo,     updateLogo,     drawLogo },
    { initTitle,    updateTitle,    drawTitle },
    { initGame,     updateGame,     drawGame },
};

static MODE_T mode;

/*  For Debugging  */

#ifdef DEBUG
bool    dbgPrintEnabled = true;
char    dbgRecvChar = '\0';

static void dbgCheckSerialRecv(void)
{
    int recv;
    while ((recv = Serial.read()) != -1) {
        switch (recv) {
        case 'd':
            dbgPrintEnabled = !dbgPrintEnabled;
            Serial.print("Debug output ");
            Serial.println(dbgPrintEnabled ? "ON" : "OFF");
            break;
        case 'r':
            clearRecord();
            break;
        }
        if (recv >= ' ' && recv <= '~') {
            dbgRecvChar = recv;
        }
    }
}
#endif

/*---------------------------------------------------------------------------*/

void setup()
{
#ifdef DEBUG
    Serial.begin(115200);
#endif
    ab.beginNoLogo();
    ab.setFrameRate(FPS);
    //ab.setTextColors(WHITE, WHITE);
    dprintln(F("Start " APP_TITLE " Version " APP_VERSION));
    ab.initAudio(1);
    readRecord();
    mode = (record.simpleMode) ? MODE_TITLE : MODE_LOGO;
    callInitFunc(mode);
}

void loop()
{
#ifdef DEBUG
    dbgCheckSerialRecv();
#endif
    if (!(ab.nextFrame())) return;
    MODE_T nextMode = callUpdateFunc(mode);
    callDrawFunc(mode);
#ifdef DEBUG
    dbgRecvChar = '\0';
#endif
    ab.display();
    if (mode != nextMode) {
        mode = nextMode;
        dprint(F("mode="));
        dprintln(mode);
        callInitFunc(mode);
    }
}
