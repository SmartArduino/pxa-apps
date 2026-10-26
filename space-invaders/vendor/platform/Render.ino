static const unsigned char UFO[] PROGMEM = {
0x10, 0x28, 0xA8, 0x54, 0x54, 0x46, 0x45, 0x55, 0x55, 0x45, 0x46, 0x54, 0x54, 0xA8, 0x28, 0x10
};

static const unsigned char UFODamage[] PROGMEM = {
0x10, 0x38, 0xB8, 0x6C, 0x6C, 0x7E, 0x7D, 0x6D, 0x6D, 0x7D, 0x7E, 0x6C, 0x6C, 0xB8, 0x38, 0x10
};

static const unsigned char IncomingLeft[] PROGMEM = {
0x18, 0x24, 0x42, 0x81, 0x00, 0x0E, 0xBF, 0x0E, 
};

static const unsigned char IncomingRight[] PROGMEM = {
0x0E, 0xBF, 0x0E, 0x00, 0x81, 0x42, 0x24, 0x18, 
};

//I find it WAY easier to type those quick lines of code instead of ard.drawLine ect. Too much work!
//I am not lazy! Totally not lazy!!

void LINE(byte LStartX, byte LStartY, byte LEndX, byte LEndY, byte COLOUR) {
  ard.drawLine(LStartX, LStartY, LEndX, LEndY, COLOUR);
}

void RECT(byte RStartX, byte RStartY, byte REndX, byte REndY, byte BW) {
  REndX -= RStartX;
  REndY -= RStartY;
  ard.drawRect(RStartX, RStartY, REndX, REndY, BW);
}

void SOLIDRECT(int RStartX, int RStartY, int REndX, int REndY, byte BW) {
  REndX -= RStartX;
  REndY -= RStartY;
  ard.fillRect(RStartX, RStartY, REndX, REndY, BW);
}

void TEXT(String Text, byte TXpos, byte TYpos, byte Tsize, boolean BW) {
  ard.setTextSize(Tsize);
  ard.setCursor(TXpos, TYpos);
  SOLIDRECT(TXpos, TYpos, (TXpos + (Text.length() * Tsize * 6)), TYpos + Tsize * 8, 0);
  if (BW) {
    ard.print(Text);
  }
}

void RenderPowerUp(int PXpos, byte PYpos, byte PBW) {
    SOLIDRECT(PXpos, PYpos, PXpos + 8, PYpos + 8, BLACK);
    switch (PowerUpType) {
      case 1:
      ard.drawBitmap(PXpos, PYpos, FixbarrierPower, 8, 8, PBW);
      break;
      case 2:
      ard.drawBitmap(PXpos, PYpos, UpPower, 8, 8, PBW);
      break;
      case 3:
      ard.drawBitmap(PXpos, PYpos, InvulPower, 8, 8, PBW);
      break;
      case 4:
      ard.drawBitmap(PXpos, PYpos, MegashotPower, 8, 8, PBW);
      break;
      case 5:
      ard.drawBitmap(PXpos, PYpos, StopPower, 8, 8, PBW);
      break;
    }
}

    

//Putting any function that draws something on the display into here saves a LOT of flash memory, particularly for drawBitmap functions!
void Render(byte Xpos, byte Ypos, byte type, byte BW) {
  if ((type == 11) || (type == 13)) {
    if ((Xpos * 24 - BackgroundOffset) <= -24) {
      Xpos += 7;
    }
  }
  switch (type) {
    case 0:
        ard.drawBitmap(Xpos, Ypos, Player, 8, 8, BW);
    break;
    case 1:
        RECT(Xpos, Ypos, (Xpos + 2), (Ypos + 5), BW);
    break;
    case 2:
    SOLIDRECT(Xpos - 16, Ypos, Xpos, Ypos + 16, BLACK);
    if (InvulBonus <= 20) {
      ard.drawBitmap(Xpos - 16, Ypos, BonusAlien, 16, 16, BW);
    }
    else {
      ard.drawBitmap(Xpos - 16, Ypos, InvulBonusAlien, 16, 16, BW);
        if ((PowerUpType == 3) && (random(10) == 9) && (Xpos > 13)) {
          SpawnParticle(1, Xpos - 13, Ypos + random(3, 16), 0);
        }
    }
    if (PowerUpType == 1)
      RECT(Xpos - 14, Ypos + 19, Xpos - 2, Ypos + 22, BW);
    RenderPowerUp(Xpos - 12, Ypos + 11, BW);
    
    break;
    case 3:
    RenderPowerUp(Xpos, Ypos, BW);
    break;
    case 4:
    ard.drawBitmap(Xpos, Ypos, destroyed, 8, 8, BW);
    break;
    case 5:
    ard.drawBitmap(Xpos - 12, Ypos - 8, Bom, 24, 8, BW);
    break;
    case 6:
    RECT(Xpos - 1, Ypos + 2, Xpos + 2, Ypos + 5, BW);
    ard.drawPixel(Xpos, Ypos + 1, BW);
    ard.drawPixel(Xpos + 1, Ypos, BW);
    if (MoveSlow) {
      ard.drawPixel(Xpos, Ypos + 3, BW);
    }
    break;
    case 7:
    if (BW) {
      SOLIDRECT(Xpos, Ypos, Xpos + 16, Ypos + 8, BLACK);
    }
      ard.drawBitmap(Xpos, Ypos, UFO, 16, 8, BW);
    break;
    case 8:
    if (BW) {
      SOLIDRECT(Xpos, Ypos, Xpos + 16, Ypos + 8, BLACK);
    }
    ard.drawBitmap(Xpos, Ypos, UFODamage, 16, 8, BW);
    break;
    case 9:
    SOLIDRECT(Xpos - 1, Ypos - 1, Xpos + 9, Ypos + 9, BLACK);
    ard.drawBitmap(Xpos, Ypos, IncomingLeft, 8, 8, BW);
    break;
    case 10:
    SOLIDRECT(Xpos - 1, Ypos - 1, Xpos + 9, Ypos + 9, BLACK);
    ard.drawBitmap(Xpos, Ypos, IncomingRight, 8, 8, BW);
    break;
    case 11:
    ard.drawBitmap(Xpos * 24 - BackgroundOffset, BackgroundHeight, CityBackground, 24, 24, BW);
    break;
    case 12:
    {
      byte BarrierPlace = (Xpos * 32 + 16);
      RECT(BarrierPlace - BarrierHP[Xpos], 52, BarrierPlace + BarrierHP[Xpos], 55, BW);
    }
    break;
    case 13:
    ard.drawBitmap(Xpos * 24 - BackgroundOffset, BackgroundHeight, CityBackground2, 24, 24, BW);
    break;
    case 14:
    SOLIDRECT(Xpos, Ypos - 8, Xpos + 16, Ypos, BLACK);
    if (BW) {
      ard.drawBitmap(Xpos, Ypos - 8, UFO, 16, 8, 1);
    }
    break;
    
    
    
    
  }
  
}

void RenderEnemy(byte EnemyID, byte WB, boolean mode) {
  int Xpos = EnemyX[EnemyID];
  int Ypos = EnemyY[EnemyID];
  byte type = EnemyType[EnemyID];
  if (mode) {
    type = AppearingEnemyType;
    Xpos = AppearingEnemyX;
    Ypos = AppearingEnemyY;
  }
  /*
  if (type == 2) {
    ard.drawBitmap(Xpos, Ypos, RangerclearDisplay, 8, 8, BLACK);
  }
  if (type == 3) {
    ard.drawBitmap(Xpos, Ypos, DevilclearDisplay, 8, 8, BLACK);
  }
  */
  SOLIDRECT(Xpos, Ypos, Xpos + 8, Ypos + 8, BLACK);
  if (AnimationTick <= 15) {
    switch (type) {
      case 1:
      ard.drawBitmap(Xpos, Ypos, JellyA, 8, 8, WB);
      break;
      case 2:
      ard.drawBitmap(Xpos, Ypos, RangerA, 8, 8, WB);
      break;
      case 3:
      ard.drawBitmap(Xpos, Ypos, DevilA, 8, 8, WB);
      break;
      case 4:
      ard.drawBitmap(Xpos, Ypos, BomberA, 8, 8, WB);
      break;
    }
  }
  else {
    switch (type) {
      case 1:
      ard.drawBitmap(Xpos, Ypos, JellyB, 8, 8, WB);
      break;
      case 2:
      ard.drawBitmap(Xpos, Ypos, RangerB, 8, 8, WB);
      break;
      case 3:
      ard.drawBitmap(Xpos, Ypos, DevilB, 8, 8, WB);
      break;
      case 4:
      ard.drawBitmap(Xpos, Ypos, BomberB, 8, 8, WB);
      break;
    }
  }
}

