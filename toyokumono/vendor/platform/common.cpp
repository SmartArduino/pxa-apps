#include "common.h"
#include "owner_compat.h"

/*  Defines  */

#define EEPROM_ADDR_BASE    568
#define EEPROM_SIGNATURE    0x184E424FUL // "OBN\x18"

#define PAD_REPEAT_DELAY    (FPS / 4)
#define PAD_REPEAT_INTERVAL (FPS / 12)

enum RECORD_STATE_T : uint8_t {
    RECORD_NOT_READ = 0,
    RECORD_INITIAL,
    RECORD_STORED,
};

/*  Global Variables  */

MyArduboy2  ab;
RECORD_T    record;
FLYING_T    dots[DOTS_MAX];
uint16_t    lastScore;
uint8_t     counter, dotIndex;
int8_t      padX, padY, padRepeatCount;
bool        isInvalid, isRecordDirty, isTitleAnimation, isInstruction;

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

static RECORD_STATE_T   recordState = RECORD_NOT_READ;
static int16_t          eepAddr;

/*---------------------------------------------------------------------------*/
/*                             Common Functions                              */
/*---------------------------------------------------------------------------*/

void readRecord(void)
{
    bool isVerified = false;
    eepSeek(EEPROM_ADDR_BASE);
    if (eepRead32() == EEPROM_SIGNATURE) {
        eepReadBlock(&record, sizeof(record));
        isVerified = (eepRead16() == calcCheckSum());
    }

    if (isVerified) {
        recordState = RECORD_STORED;
        isRecordDirty = false;
    } else {
        memset(&record, 0, sizeof(record));
        recordState = RECORD_INITIAL;
        isRecordDirty = true;
    }
    setSound(ab.isAudioEnabled()); // Load Sound ON/OFF
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
    eepWriteBlock(&record, sizeof(record));
    eepWrite16(calcCheckSum());
    ab.audio.saveOnOff(); // Save Sound ON/OFF
    recordState = RECORD_STORED;
    isRecordDirty = false;
}

static uint16_t calcCheckSum()
{
    uint16_t checkSum = (EEPROM_SIGNATURE & 0xFFFF) + (EEPROM_SIGNATURE >> 16) * 3;
    const uint8_t *p = (const uint8_t *) &record;
    for (int i = 0; i < sizeof(record) / 2; i++) {
        checkSum += (uint16_t)(p[i*2] | (uint16_t)p[i*2+1]<<8) * (i * 2 + 5);
    }
    return checkSum;
}

void clearRecord(void)
{
    eepSeek(EEPROM_ADDR_BASE);
    for (int i = 0; i < (sizeof(record) + 6) / 4; i++) {
        eepWrite32(0);
    }
    recordState = RECORD_INITIAL;
}

bool enterScore(uint16_t score)
{
    int r = 10;
    uint16_t h;
    while (r > 0 && (h = record.hiscore[r - 1]) < score) {
        if (--r < 9) record.hiscore[r + 1] = h;
    }
    if (r < 10) record.hiscore[r] = score;
    lastScore = score;
    return (r == 0);
}

void handleDPad(void)
{
    padX = padY = 0;
    if (ab.buttonPressed(LEFT_BUTTON | RIGHT_BUTTON | UP_BUTTON | DOWN_BUTTON)) {
        if (++padRepeatCount >= (PAD_REPEAT_DELAY + PAD_REPEAT_INTERVAL)) {
            padRepeatCount = PAD_REPEAT_DELAY;
        }
        if (padRepeatCount == 1 || padRepeatCount == PAD_REPEAT_DELAY) {
            if (ab.buttonPressed(LEFT_BUTTON))  padX--;
            if (ab.buttonPressed(RIGHT_BUTTON)) padX++;
            if (ab.buttonPressed(UP_BUTTON))    padY--;
            if (ab.buttonPressed(DOWN_BUTTON))  padY++;
        }
    } else {
        padRepeatCount = 0;
    }
}

void drawTime(int16_t x, int16_t y, uint32_t frames)
{
    uint16_t h = frames / (FPS * 3600UL);
    uint8_t m = frames / (FPS * 60) % 60;
    uint8_t s = frames / FPS % 60;
    ab.setCursor(x, y);
    if (h == 0 && m == 0) {
        if (s < 10) ab.print('0');
        ab.print(s);
        ab.print('.');
        ab.print(frames / (FPS / 10) % 10);
    } else {
        if (h > 0) {
            ab.print(h);
            ab.print(':');
            if (m < 10) ab.print('0');
        }
        ab.print(m);
        ab.print(':');
        if (s < 10) ab.print('0');
        ab.print(s);
    }
}

void drawText(const char *p, int16_t y)
{
    while (pgm_read_byte(p) != '\e') {
        uint8_t len = strnlen_P(p, 21);
        ab.printEx(64 - len * 3, y, (const __FlashStringHelper *) p);
        p += len + 1;
        y += (len == 0) ? 2 : 6;
    }
}

void drawDots(void)
{
    for (FLYING_T *p = dots; p < &dots[DOTS_MAX]; p++) {
        if (p->y <= 0 || (p->y >> DECIMAL_BITS) >= HEIGHT) continue;
        ab.drawPixel(p->x >> DECIMAL_BITS, p->y >> DECIMAL_BITS, WHITE);
    }
}

/*---------------------------------------------------------------------------*/
/*                              Sound Functions                              */
/*---------------------------------------------------------------------------*/

void setSound(bool on)
{
    ab.setAudioEnabled(on);
}

void playSoundTick(void)
{
    ab.playTone(440, 10);
}

void playSoundClick(void)
{
    ab.playTone(587, 20);
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

void toy_reset_common(){memset(&record,0,sizeof(record));memset(dots,0,sizeof(dots));lastScore=counter=dotIndex=0;padX=padY=padRepeatCount=0;isInvalid=isRecordDirty=isTitleAnimation=isInstruction=false;recordState=RECORD_NOT_READ;eepAddr=0;}
void toy_pack_flying(ObonoWriter&w,const FLYING_T&p){w.u16(p.x);w.u16(p.y);w.u8(p.vx);w.u8(p.vy);}
void toy_pack_common(ObonoWriter&w){for(unsigned i=0;i<10;i++)w.u16(record.hiscore[i]);w.u32(record.playFrames);w.u16(record.playCount);for(auto&p:dots)toy_pack_flying(w,p);w.u16(lastScore);w.u8(counter);w.u8(dotIndex);w.u8(padX);w.u8(padY);w.u8(padRepeatCount);w.u8(isInvalid);w.u8(isRecordDirty);w.u8(isTitleAnimation);w.u8(isInstruction);w.u8(recordState);w.u16(eepAddr);}
