#include "common.h"

/*  Defines  */

#define EEPROM_ADDR_BASE    896
#define EEPROM_SIGNATURE    0x054E424FUL // "OBN\x05"

#define PAD_REPEAT_DELAY    (FPS / 4)
#define PAD_REPEAT_INTERVAL (FPS / 12)

enum RECORD_STATE_T {
    RECORD_NOT_READ = 0,
    RECORD_INITIAL,
    RECORD_STORED,
};

/*  Global Variables  */

MyArduboy   arduboy;
RECORD_T    record;
bool        isRecordDirty;
int8_t      padX, padY, padRepeatCount;
GAME_MODE_T gameMode = GAME_MODE_ENDLESS;
bool        isInvalid;

PROGMEM const uint8_t imgLabelScore[19] = { // 19x5
    0x16, 0x15, 0x0D, 0x00, 0x1E, 0x11, 0x11, 0x00, 0x1E, 0x11, 0x0F, 0x00, 0x1F, 0x05, 0x1B, 0x00, 0x1F, 0x15, 0x11
};

PROGMEM const uint8_t imgLabelChain[17] = { // 17x5
    0x1E, 0x11, 0x11, 0x00, 0x1F, 0x04, 0x1F, 0x00, 0x1E, 0x09, 0x1F, 0x00, 0x1F, 0x00, 0x1E, 0x01, 0x1F
};

PROGMEM const uint8_t imgLabelLevel[19] = { // 17x5
    0x1F, 0x10, 0x10, 0x00, 0x1F, 0x15, 0x11, 0x00, 0x1F, 0x10, 0x0F, 0x00, 0x1F, 0x15, 0x11, 0x00, 0x1F, 0x10, 0x10
};

PROGMEM const uint8_t imgLabelTime[13] = { // 13x5
    0x01, 0x1F, 0x01, 0x00, 0x1F, 0x00, 0x1F, 0x03, 0x1F, 0x00, 0x1F, 0x15, 0x11
};

PROGMEM const uint8_t imgLabelInfo[13] = { // 13x5
    0x1F, 0x00, 0x1E, 0x01, 0x1F, 0x00, 0x1F, 0x05, 0x01, 0x00, 0x1E, 0x11, 0x0F
};

PROGMEM const uint8_t imgLabelStep[15] = { // 15x5
    0x16, 0x15, 0x0D, 0x00, 0x01, 0x1F, 0x01, 0x00, 0x1F, 0x15, 0x11, 0x00, 0x1F, 0x05, 0x03
};

PROGMEM const uint8_t imgLabelIssue[17] = { // 17x5
    0x1F, 0x00, 0x16, 0x15, 0x0D, 0x00, 0x16, 0x15, 0x0D, 0x00, 0x1F, 0x10, 0x0F, 0x00, 0x1F, 0x15, 0x11
};

/*  Local Functions  */

static uint16_t calcCheckSum();

void eepSeek(int addr);
uint8_t eepRead8(void);
uint16_t eepRead16(void);
uint32_t eepRead32(void);
void eepReadBlock(void *p, size_t n);
void eepWrite8(uint8_t val);
void eepWrite16(uint16_t val);
void eepWrite32(uint32_t val);
void eepWriteBlock(const void *p, size_t n);

/*  Local Variables  */

PROGMEM static const byte soundTick[] = {
    0x90, 69, 0, 10, 0x80, 0xF0
};

PROGMEM static const byte soundClick[] = {
    0x90, 74, 0, 20, 0x80, 0xF0
};

static RECORD_STATE_T   recordState = RECORD_NOT_READ;
static int16_t          eepAddr;

// AVR record layout: exactly 22 little-endian bytes, independent of host padding.
static void recordEncode(uint8_t out[22]){ObonoWriter w(out);w.u8(record.endlessHiscore);w.u8(record.endlessHiscore>>8);w.u8(record.endlessHiscore>>16);w.u8(record.endlessMaxLevel);w.u8(record.limitedHiscore);w.u8(record.limitedHiscore>>8);w.u8(record.limitedHiscore>>16);w.u8(record.limitedMaxChain);w.u32(record.playFrames);w.u32(record.erasedDice);for(auto v:record.puzzleClearFlag)w.u8(v);w.u8(record.puzzleClearCount);}
static void recordDecode(const uint8_t p[22]){record.endlessHiscore=(uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16);record.endlessMaxLevel=p[3];record.limitedHiscore=(uint32_t)p[4]|((uint32_t)p[5]<<8)|((uint32_t)p[6]<<16);record.limitedMaxChain=p[7];record.playFrames=record.erasedDice=0;for(int i=0;i<4;i++){record.playFrames|=(uint32_t)p[8+i]<<(8*i);record.erasedDice|=(uint32_t)p[12+i]<<(8*i);}for(int i=0;i<5;i++)record.puzzleClearFlag[i]=p[16+i];record.puzzleClearCount=p[21];}

/*---------------------------------------------------------------------------*/
/*                             Common Functions                              */
/*---------------------------------------------------------------------------*/

void readRecord(void)
{
    bool isVerified = false;
    eepSeek(EEPROM_ADDR_BASE);
    if (eepRead32() == EEPROM_SIGNATURE) {
        uint8_t encoded[22];eepReadBlock(encoded,22);recordDecode(encoded);
        isVerified = (eepRead16() == calcCheckSum());
    }

    if (isVerified) {
        recordState = RECORD_STORED;
        isRecordDirty = false;
        dprintln(F("Read record from EEPROM"));
    } else {
        memset(&record, 0, sizeof(record));
        recordState = RECORD_INITIAL;
        isRecordDirty = true;
    }
    setSound(arduboy.isAudioEnabled()); // Load Sound ON/OFF
}

void writeRecord(void)
{
    if (!isRecordDirty) return;
    if (recordState == RECORD_INITIAL) {
        eepSeek(EEPROM_ADDR_BASE);
        eepWrite32(EEPROM_SIGNATURE);
    } else {
        eepSeek(EEPROM_ADDR_BASE + 4);
    }
    uint8_t encoded[22];recordEncode(encoded);eepWriteBlock(encoded,22);
    eepWrite16(calcCheckSum());
    arduboy.audio.saveOnOff(); // Save Sound ON/OFF
    recordState = RECORD_STORED;
    isRecordDirty = false;
    dprintln(F("Write record to EEPROM"));
}

static uint16_t calcCheckSum()
{
    uint16_t checkSum = (EEPROM_SIGNATURE & 0xFFFF) + (EEPROM_SIGNATURE >> 16) * 3;
    uint8_t encoded[22];recordEncode(encoded);
    for (int i=0;i<11;i++){uint16_t v=(uint16_t)encoded[2*i]|((uint16_t)encoded[2*i+1]<<8);checkSum+=(uint16_t)(v*(i*2+5));}
    return checkSum;
}

void clearRecord(void)
{
    eepSeek(EEPROM_ADDR_BASE);
    for (int i = 0; i < (22 + 6) / 4; i++) {
        eepWrite32(0);
    }
    recordState = RECORD_INITIAL;
    dprintln(F("Clear EEPROM"));
}

void handleDPad(void)
{
    padX = padY = 0;
    if (arduboy.buttonPressed(LEFT_BUTTON | RIGHT_BUTTON | UP_BUTTON | DOWN_BUTTON)) {
        if (++padRepeatCount >= (PAD_REPEAT_DELAY + PAD_REPEAT_INTERVAL)) {
            padRepeatCount = PAD_REPEAT_DELAY;
        }
        if (padRepeatCount == 1 || padRepeatCount == PAD_REPEAT_DELAY) {
            if (arduboy.buttonPressed(LEFT_BUTTON))  padX--;
            if (arduboy.buttonPressed(RIGHT_BUTTON)) padX++;
            if (arduboy.buttonPressed(UP_BUTTON))    padY--;
            if (arduboy.buttonPressed(DOWN_BUTTON))  padY++;
        }
    } else {
        padRepeatCount = 0;
    }
}

void drawNumber(int16_t x, int16_t y, int32_t value)
{
    arduboy.setCursor(x, y);
    arduboy.print(value);
}

void drawTime(int16_t x, int16_t y, uint32_t frames)
{
    uint16_t h = frames / (FPS * 3600UL);
    uint8_t m = frames / (FPS * 60) % 60;
    uint8_t s = frames / FPS % 60;
    arduboy.setCursor(x, y);
    if (h == 0 && m == 0) {
        if (s < 10) arduboy.print('0');
        arduboy.print(s);
        arduboy.print('.');
        arduboy.print(frames / (FPS / 10) % 10);
    } else {
        if (h > 0) {
            arduboy.print(h);
            arduboy.print(':');
            if (m < 10) arduboy.print('0');
        }
        arduboy.print(m);
        arduboy.print(':');
        if (s < 10) arduboy.print('0');
        arduboy.print(s);
    }
}

/*---------------------------------------------------------------------------*/
/*                              Sound Functions                              */
/*---------------------------------------------------------------------------*/

void setSound(bool on)
{
    arduboy.setAudioEnabled(on);
    dprint(F("audioEnabled="));
    dprintln(on);
}

void playSoundTick(void)
{
    arduboy.playScore2(soundTick, 255);
}

void playSoundClick(void)
{
    arduboy.playScore2(soundClick, 255);
}

/*---------------------------------------------------------------------------*/
/*                             EEPROM Functions                              */
/*---------------------------------------------------------------------------*/

void eepSeek(int addr)
{
    eepAddr = max(addr, EEPROM_STORAGE_SPACE_START);
}

uint8_t eepRead8(void)
{
    eeprom_busy_wait();
    return eeprom_read_byte((const uint8_t *)(uintptr_t)eepAddr++);
}

uint16_t eepRead16(void)
{
    eeprom_busy_wait();
    uint16_t ret = eeprom_read_word((const uint16_t *)(uintptr_t)eepAddr);
    eepAddr += 2;
    return ret;
}

uint32_t eepRead32(void)
{
    eeprom_busy_wait();
    uint32_t ret = eeprom_read_dword((const uint32_t *)(uintptr_t)eepAddr);
    eepAddr += 4;
    return ret;
}

void eepReadBlock(void *p, size_t n)
{
    eeprom_busy_wait();
    eeprom_read_block(p, (const void *)(uintptr_t)eepAddr, n);
    eepAddr += n;
}

void eepWrite8(uint8_t val)
{
    eeprom_busy_wait();
    eeprom_write_byte((uint8_t *)(uintptr_t)eepAddr, val);
    eepAddr++;
}

void eepWrite16(uint16_t val)
{
    eeprom_busy_wait();
    eeprom_write_word((uint16_t *)(uintptr_t)eepAddr, val);
    eepAddr += 2;
}

void eepWrite32(uint32_t val)
{
    eeprom_busy_wait();
    eeprom_write_dword((uint32_t *)(uintptr_t)eepAddr, val);
    eepAddr += 4;
}

void eepWriteBlock(const void *p, size_t n)
{
    eeprom_busy_wait();
    eeprom_write_block(p, (void *)(uintptr_t)eepAddr, n);
    eepAddr += n;
}

void obono_reset_common(){memset(&record,0,sizeof(record));isRecordDirty=0;padX=padY=padRepeatCount=0;gameMode=GAME_MODE_ENDLESS;isInvalid=0;recordState=RECORD_NOT_READ;eepAddr=0;}
void obono_pack_common(ObonoWriter&w){uint8_t b[22];recordEncode(b);for(auto v:b)w.u8(v);w.u8(isRecordDirty);w.u8(padX);w.u8(padY);w.u8(padRepeatCount);w.u8(gameMode);w.u8(isInvalid);w.u8(recordState);w.u16(eepAddr);}
void obono_register_common(){obono_audio_register(16,soundTick,sizeof(soundTick));obono_audio_register(17,soundClick,sizeof(soundClick));}
