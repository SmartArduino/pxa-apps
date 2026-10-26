
#define BackgroundHeight 6


//This unused code makes the background look amaaaazing.
//Unfortunately, this code had to be scrapped because it made the game laggy and caused the game to crash for some reason.
//Plus it used a lot of flash memory.
/*
// PXA: mutable local state lifted to resettable sketch globals.

static const unsigned char DestroyedBuilding[] PROGMEM = {
0x1C, 0x60, 0x40, 0x80, 0x40, 0x80, 0x80, 0x00
};

void DrawThinTower(int XPOS, byte Height, boolean Blasted) {
  ard.fillRect(XPOS + 1, 36 - Height, 8, Height - 1, BLACK);
  ard.drawRect(XPOS, 37 - Height, 9, Height, WHITE);
  for (byte WindowDot = 37 - Height; WindowDot < 34; WindowDot += 2) {
    for (byte q = 2; q < 8; q += 2) {
      if ((LightsOn == 1) || (LightsOn == 4) || (LightsOn == 5) || (LightsOn == 6) || (LightsOn == 8) || (LightsOn == 10) || (LightsOn == 11)) {
        ard.drawPixel(XPOS + q, WindowDot, WHITE);
      }
      LightsOn++;
    }
    if (LightsOn > 14) {
      LightsOn = 0;
    }
  }
  if (Blasted) {
    ard.fillRect(XPOS + 2, 35 - Height, 8, 8, BLACK);
    ard.drawBitmap(XPOS + 2, 35 - Height, DestroyedBuilding, 8, 8, WHITE);
    if ((AnimationTick == 9) && random(2)) {
      SpawnParticle(2, XPOS + 2 + random(8), 41 - Height, 0);
    }
  }
}

void DrawBackground() {
  LINE(0, 38, 128, 38, BLACK);
  LightsOn = 0;
  DrawThinTower(-3, 17, 0);
  DrawThinTower(5, 14, 0);
  DrawThinTower(1, 7, 0);
  DrawThinTower(10, 18, 0);
  DrawThinTower(15, 24, 1);
  DrawThinTower(26, 9, 0);
  DrawThinTower(31, 14, 0);
  DrawThinTower(37, 6, 0);
  DrawThinTower(53, 12, 0);
  DrawThinTower(48, 27, 1);
  DrawThinTower(58, 14, 0);
  DrawThinTower(63, 9, 0);
  DrawThinTower(66, 14, 1);
  DrawThinTower(70, 7, 0);
  DrawThinTower(78, 7, 0);
  DrawThinTower(87, 14, 0);
  DrawThinTower(82, 23, 0);
  DrawThinTower(91, 20, 1);
  DrawThinTower(97, 13, 0);
  DrawThinTower(102, 19, 0);
  DrawThinTower(113, 15, 0);
  DrawThinTower(110, 21, 1);
  DrawThinTower(118, 8, 0);
  DrawThinTower(125, 24, 1);
}
*/

static const unsigned char CityBackground[] PROGMEM = {
0x20, 0x20, 0xA0, 0x20, 0xE0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x60, 0x98, 0x87, 0x98, 0x60,
0x80, 0x00, 0x00, 0x00, 0xE0, 0x20, 0xA0, 0x20, 0x8A, 0x00, 0x82, 0x00, 0xFF, 0x40, 0x40, 0x40,
0xC0, 0x00, 0xFF, 0x00, 0xAA, 0xAA, 0xAA, 0x00, 0xFF, 0x00, 0x00, 0x00, 0xFF, 0x00, 0xA8, 0x00,
0x28, 0x00, 0x0A, 0x00, 0xFF, 0x55, 0x55, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xAA, 0xFF, 0x01, 0x55,
0x01, 0x55, 0x01, 0xFF, 0xFF, 0x00, 0x08, 0x00
};

static const unsigned char CityBackground2[] PROGMEM = {
0x20, 0x20, 0xE0, 0x00, 0x00, 0x80, 0x80, 0x80, 0x80, 0xFC, 0x04, 0x44, 0x04, 0x54, 0x04, 0x54,
0x04, 0xFC, 0x00, 0x00, 0xE0, 0x20, 0x20, 0x20, 0x2A, 0x00, 0xFF, 0x10, 0xFF, 0x55, 0x55, 0x01,
0x51, 0x51, 0xFF, 0x15, 0x00, 0x51, 0x00, 0x14, 0x00, 0xFF, 0x00, 0x00, 0xFF, 0x00, 0x8A, 0x00,
0x22, 0x00, 0xFF, 0x00, 0xFF, 0x45, 0x45, 0x00, 0x55, 0x55, 0xFF, 0x44, 0x00, 0x54, 0x00, 0x11,
0x00, 0xFF, 0x55, 0x01, 0xFF, 0x00, 0x2A, 0x00
};

const boolean BackgroundImageType[8] = {0, 1, 1, 0, 0, 1, 0, 1};


void DrawBackground() {
// PXA: mutable local state lifted to resettable sketch globals.
  LINE(0, 63, 128, 63, 1);
  if (ShowBackground) {
    
  if (TitleScreenMove) {
    for (byte I = 0; I < 7; I++) {
      Render(I, 0, 11 + BackgroundImageType[I] * 2, 0);
    }
  }
  
  MoveNow++;
  if (TitleScreenMove && (MoveNow > 5)) {
    BackgroundOffset++;
    MoveNow = 0;
    if (BackgroundOffset >= 128) {
      BackgroundOffset = 0;
    }
  }
  
  for (byte I = 0; I < 7; I++) {
    Render(I, 0, 11 + BackgroundImageType[I] * 2, 1);
  }
  
  }
}

