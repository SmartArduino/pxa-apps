#include "common.h"

/*  Defines  */

#define EEPROM_ADDR_BASE    656
#define EEPROM_ADDR_MIGRATE 688
#define EEPROM_SIGNATURE    0x124E424FUL // "OBN\x12"

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
int8_t      padX, padY, padRepeatCount;
uint32_t    lastScore;
bool        isInvalid, isRecordDirty;
bool        isBMode;

/*  Local Functions  */

static uint16_t calcCheckSum();

void     eepSeek(int addr);
uint8_t  eepRead8(void);
uint16_t eepRead16(void);
uint32_t eepRead32(void);
void     eepReadBlock(void *p, size_t n);
void     eepWrite8(uint8_t val);
void     eepWrite16(uint16_t val);
void     eepWrite32(uint32_t val);
void     eepWriteBlock(const void *p, size_t n);

/*  Local Variables  */

PROGMEM static const byte soundTick[] = {
    0x90, 69, 0, 10, 0x80, 0xF0 // arduboy.tone2(440, 10);
};

PROGMEM static const byte soundClick[] = {
    0x90, 74, 0, 20, 0x80, 0xF0 // arduboy.tone2(587, 20);
};

static RECORD_STATE_T   recordState = RECORD_NOT_READ;
static int16_t          eepAddr;

// Exact AVR26-byte record; source bitfields occupy18..19, frames20..23, count24..25.
static void recordEncode(uint8_t b[26]){ObonoWriter w(b);for(auto v:record.hiscoreRaw)w.u8(v);w.u16(record.maxHoles|(record.maxContinuous<<10));w.u32(record.playFrames);w.u16(record.playCount);}
static void recordDecode(const uint8_t b[26]){memcpy(record.hiscoreRaw,b,18);uint16_t v=b[18]|((uint16_t)b[19]<<8);record.maxHoles=v&1023;record.maxContinuous=v>>10;record.playFrames=0;for(int i=0;i<4;i++)record.playFrames|=(uint32_t)b[20+i]<<(8*i);record.playCount=b[24]|((uint16_t)b[25]<<8);}

/*---------------------------------------------------------------------------*/
/*                             Common Functions                              */
/*---------------------------------------------------------------------------*/

void readRecord(void)
{
    bool isVerified = false;
    bool isMigrated = false;
    for (int i = 0; !isVerified && i < 2; i++) {
        eepSeek((i == 0) ? EEPROM_ADDR_BASE : EEPROM_ADDR_MIGRATE);
        if (eepRead32() == EEPROM_SIGNATURE) {
            uint8_t encoded[26];eepReadBlock(encoded,26);recordDecode(encoded);
            isVerified = (eepRead16() == calcCheckSum());
            isMigrated = (isVerified && i == 1);
        }
    }

    if (isVerified) {
        recordState = (isMigrated) ? RECORD_INITIAL : RECORD_STORED;
        isRecordDirty = isMigrated;
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
    uint8_t encoded[26];recordEncode(encoded);eepWriteBlock(encoded,26);
    eepWrite16(calcCheckSum());
    arduboy.audio.saveOnOff(); // Save Sound ON/OFF
    recordState = RECORD_STORED;
    isRecordDirty = false;
    dprintln(F("Write record to EEPROM"));
}

static uint16_t calcCheckSum()
{
    uint16_t checkSum = (EEPROM_SIGNATURE & 0xFFFF) + (EEPROM_SIGNATURE >> 16) * 3;
    uint8_t encoded[26];recordEncode(encoded);
    for (int i = 0; i < 13; i++) {
        uint16_t v=encoded[2*i]|((uint16_t)encoded[2*i+1]<<8);checkSum+=(uint16_t)(v*(i*2+5));
    }
    return checkSum;
}

void clearRecord(void)
{
    eepSeek(EEPROM_ADDR_BASE);
    for (int i = 0; i < (26 + 6) / 4; i++) {
        eepWrite32(0);
    }
    recordState = RECORD_INITIAL;
    dprintln(F("Clear EEPROM"));
}

bool enterScore(uint32_t score, bool isB)
{
    int r;
    for (r = 3; r > 0; r--) {
        uint32_t hiscore = getHiscore(r - 1, isB);
        if (hiscore >= score) break;
        if (r <= 2) setHiscore(r, isB, hiscore);
    }
    if (r < 3) {
        setHiscore(r, isB, score);
    }
    lastScore = score;
    return (r == 0);
}

uint32_t getHiscore(uint8_t r, bool isB)
{
    uint8_t *p = record.hiscoreRaw + (isB * 3 + r) * 3;
    return (uint32_t)*p | (uint32_t)*(p + 1) << 8 | (uint32_t)*(p + 2) << 16;
}

void setHiscore(uint8_t r, bool isB, uint32_t score)
{
    uint8_t *p = record.hiscoreRaw + (isB * 3 + r) * 3;
    *p++ = score & 0xff;
    *p++ = score >> 8 & 0xff;
    *p++ = score >> 16 & 0xff;
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

int8_t drawNumberR(int16_t x, int16_t y, int32_t value)
{
    int8_t ret = 0;
    do {
        arduboy.setCursor(x, y);
        arduboy.print((char)('0' + value % 10));
        value /= 10;
        x -= 6;
        ret++;
    } while (value > 0);
    return ret;
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

void obono_reset_common(){memset(&record,0,sizeof(record));padX=padY=padRepeatCount=0;lastScore=0;isInvalid=isRecordDirty=isBMode=false;recordState=RECORD_NOT_READ;eepAddr=0;}
void obono_pack_common(ObonoWriter&w){uint8_t b[26];recordEncode(b);for(auto v:b)w.u8(v);w.u8(padX);w.u8(padY);w.u8(padRepeatCount);w.u32(lastScore);w.u8(isInvalid);w.u8(isRecordDirty);w.u8(isBMode);w.u8(recordState);w.u16(eepAddr);}
void obono_register_common(){obono_audio_register(0,soundTick,sizeof(soundTick));obono_audio_register(1,soundClick,sizeof(soundClick));}
