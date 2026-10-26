void IOIndicator(byte pos, boolean OnOff) {  //Display a little square which is filled in for true, and just lines for false.
  if (OnOff) {
    SOLIDRECT(90, pos, 98, pos + 8, WHITE);
  }
  else {
    SOLIDRECT(90, pos, 98, pos + 8, BLACK);
    RECT(90, pos, 98, pos + 8, WHITE);
  }
}

port::Task PauseGame() {                 //Draw a black rectangle and play the pause tone.
    SOLIDRECT(31, 18, 103, 32, 0);
    for (MusicPlace = 0; MusicPlace < 4; MusicPlace) {
      MELODY(F("HCHC"), 3);
      co_await port::Delay{(uint32_t)(99), __LINE__};
    }
    while (BButton) { co_await port::Delay{1, __LINE__}; }
  

  co_return;
}

char ShowScore[12]; // full signed 32-bit score and terminator

void DisplayValue(byte YPOS, byte inputValue) {
  itoa(inputValue, ShowScore, 10);
  TEXT(ShowScore, 92, YPOS, 1, 1);
}

port::Task PrepareNewGame() {
            while (BButton) { co_await port::Delay{1, __LINE__}; }
            Menu = 0;
            TitleScreenMove = 0;
            ResetVariables();
            StartMove = 0;
            DrawBackground();
            Display;

  co_return;
}


port::Task DisplayInfo(boolean BlackWhite) {
  if (!TitleScreenMove) {
  if (!IsItCustomGame) {
  for (byte m = 0; m < (Lives - 1); m++) {
    Render(m * 9 + 1, 1, 0, BlackWhite);
  }
  }
  
  itoa(Score, ShowScore, 10);
  TEXT(ShowScore, 40, 1, 1, BlackWhite);

  if (BButton && BlackWhite) {
    co_await PauseGame();
    TEXT(F("Paused"), 31, 18, 2, 1);
    Display;
    while (!BButton) { co_await port::Delay{1, __LINE__}; }
    co_await PauseGame();
  }
  if (GameType == 3) {
    if (UFOHP <= 0) {
      BlackWhite = 0;
    }
    TEXT(F("UFO"), 97, 1, 1, BlackWhite);
    SOLIDRECT(116, 3, 123, 5, BLACK);
    if (UFOHP > 0) {
      byte HPBar = 0;
      byte smallcounter = 0;
      for (int l = 0; l <= UFOHP; l++) {
        smallcounter++;
        if (smallcounter == Level) {
          smallcounter = 0;
          HPBar++;
        }
      }
      
      RECT(115, 2, 124, 6, BlackWhite);
      SOLIDRECT(116, 3, 116 + HPBar, 5, BlackWhite);
    }
  }
  }
  else {
    byte MaxSelect = 0; // Info has no selectable rows

    if (Menu != 4) {
      AppearingEnemyX = 29;
      if ((Menu == 3) || (Menu == 5))
        AppearingEnemyX = 9;

      if (Menu == 5) {
        AppearingEnemyY = Select * 8 + 1;
      }
      else {
        AppearingEnemyY = Select * 9 + 18;
      }
      RenderEnemy(0, WHITE, 1);
    }
    
    if (Menu == 1) {
      MaxSelect = 2;
      
      TEXT(F("Play"), 40, 18, 1, 1);
      TEXT(F("Settings"), 40, 27, 1, 1);
      TEXT(F("Info"), 40, 36, 1, 1);
    }
    if (Menu == 4) {
      TEXT(NameOfThisGame, 1, 14, 1, 1);
      TEXT(F("by Teo Taylor"), 1, 30, 1, WHITE);
      TEXT(F("Thanks for playing!"), 1, 39, 1, WHITE);
    }
    if (Menu == 2) {
      MaxSelect = 2;
      
      TEXT(F("New game"), 40, 18, 1, 1);
      TEXT(F("Continue"), 40, 27, 1, 1);
      if (!EEPROM.read(16)) {
        LINE(38, 31, 88, 31, 1);
      }
      TEXT(F("Custom game"), 40, 36, 1, 1);
    }
    if (Menu == 3) {
      MaxSelect = 3;
      
      TEXT(F("Background"), 20, 18, 1, 1); IOIndicator(18, ShowBackground);
      TEXT(F("Sound"), 20, 27, 1, 1); IOIndicator(27, AudioOn);
      TEXT(F("Music"), 20, 36, 1, 1); IOIndicator(36, MusicOn);
      TEXT(F("OK"), 20, 45, 1, 1);
      
    }
// PXA: mutable local state lifted to resettable sketch globals.
// PXA: mutable local state lifted to resettable sketch globals.
// PXA: mutable local state lifted to resettable sketch globals.
// PXA: mutable local state lifted to resettable sketch globals.
// PXA: mutable local state lifted to resettable sketch globals.
// PXA: mutable local state lifted to resettable sketch globals.
      //static boolean SetupPowerUpSetting = 0;
    if (Menu == 5) {
      /*
      if (!SetupPowerUpSetting) {
        SetupPowerUpSetting = 1;
        for (byte y = 0; y < 5; y++) {
          AdaptedPowerUp[y] = 0;
        }
      }
      */
      
      MaxSelect = 6;

      TEXT(F("Level:"), 20, 1, 1, 1);
      DisplayValue(1, SetLevel);
      
      TEXT(F("Game type:"), 20, 9, 1, 1);
      DisplayValue(9, SetGameType);

      
      DisplayValue(17, SetType1);
      
      if (SetGameType == 1) {
          TEXT(F("Row 1:"), 20, 17, 1, 1);
          TEXT(F("Row 2:"), 20, 25, 1, 1);
          TEXT(F("Row 3:"), 20, 33, 1, 1);
          DisplayValue(25, SetType2);
          DisplayValue(33, SetType3);
      }
      else {
        TEXT(F("Alien type:"), 20, 17, 1, 1);
      }
      
      TEXT(F("Power up"), 20, 41, 1, 1);
      Render(71, 41, 3, WHITE);
      IOIndicator(41, AdaptedPowerUp[PowerUpType]);
      
      TEXT(F("START"), 20, 49, 1, 1);
      
    }
    
    if (!ButtonHold) {
      if (UpButton && Select) {
        SOUND(2, 1480, 1);
        RenderEnemy(0, BLACK, 1);
        Select--;
        if ((Menu == 5) && (Select == 4) && (SetGameType != 1))
          Select = 2;
      }
      if (DownButton && (Select < MaxSelect)) {
        SOUND(2, 1480, 1);
        RenderEnemy(0, BLACK, 1);
        Select++;
        if ((Menu == 5) && (Select == 3) && (SetGameType != 1))
          Select = 5;
      }
      if (AButton) {
        clearDisplay;
        SOUND(2, 659, 1);
        Menu = 1;
        Select = 0;
      }
      if (Menu == 5) {
        if (Select == 5) {
          if (LeftButton) {
            SOUND(2, 1480, 1);
            PowerUpType--;
            if (!PowerUpType)
              PowerUpType = 5;
          }
          if (RightButton) {
            SOUND(2, 1480, 1);
            PowerUpType++;
            if (PowerUpType > 5)
              PowerUpType = 1;
          }
        }
        if (!Select) {
          if (LeftButton) {
            SOUND(2, 1480, 1);
            SetLevel--;
          }
          if (RightButton) {
            SOUND(2, 1480, 1);
            SetLevel++;
          }
          if (!SetLevel)
            SetLevel++;
        }
      }

      
      if (BButton) {
        SOUND(2, 1480, 1);
        clearDisplay;
        if (Menu == 1) {
          switch (Select) {
            case 0:
            Menu = 2;
            Select = 0;
            break;
            case 1:
            Menu = 3;
            Select = 0;
            break;
            case 2:
            Menu = 4;
            break;
          }
          goto OptionSelected;
        }
        if (Menu == 4) {
          Menu = 1;
          goto OptionSelected;
        }
        if (Menu == 2) {
          if (!Select) {
            co_await PrepareNewGame();
            GameType = 1;
            StartMove = 1;
            
            byte SetTheEnemyType = 3;
            for (byte j = 0; j < 3; j++) {
              for (byte l = 0; l < 9; l++) {
                co_await SpawnEnemy(1 + l * 9, 1 + j * 9, SetTheEnemyType, 1);
              }
              NewType[j] = SetTheEnemyType--;
            }
            
    
    
    
            co_await port::Delay{(uint32_t)(500), __LINE__};
            co_await BuildBarriers();
          }
          if (Select == 1) {
            if (EEPROM.read(16)) {
            co_await PrepareNewGame();
            
            GameType = EEPROM.read(28);
            Level = EEPROM.read(27);
            Invulnerable = EEPROM.read(22);
            PlayerX = EEPROM.read(17);
            Lives = EEPROM.read(16);
            for (byte i = 0; i < 4; i++) {
              BarrierHP[i] = EEPROM.read(23 + i);
            }
            Freeze = EEPROM.read(32);
            MegaShot = EEPROM.read(33);
            EEPROM.get(18, Score);

            NewType[0] = EEPROM.read(29);
            NewType[1] = EEPROM.read(30);
            NewType[2] = EEPROM.read(31);
    
            co_await NewLevel(1);
            Render(PlayerX, 55, 0, WHITE);
            EraseSaveProgress();
            }
            else {
              SOUND(2, 46, 2);
            }
          }
          if (Select == 2) {
            Menu = 5;
            Select = 0;
          }

          goto OptionSelected;
        }
        if (Menu == 3) {
          switch (Select) {
            case 0:
            if (ShowBackground) {
              ShowBackground = 0;
            }
            else {
              ShowBackground = 1;
            }
            break;
            case 1:
            if (AudioOn) {
              AudioOn = 0;
            }
            else {
              AudioOn = 1;
            }
            break;
            case 2:
            if (MusicOn) {
              MusicOn = 0;
            }
            else {
              MusicOn = 1;
            }
            break;
            case 3:
            Menu = 1;
            Select = 0;
            break;
          }
          goto OptionSelected;
        }
        if (Menu == 5) {
          if (!Select) {
            SetLevel++;
            if (!SetLevel)
              SetLevel++;
          }
          if (Select == 1) {
            SetGameType++;
            if (SetGameType >= 4) {
              SetGameType = 1;
            }
          }
          if (Select == 2) {
            SetType1++;
            if (SetType1 >= 5) {
              SetType1 = 1;
            }
          }
          if (Select == 3) {
            SetType2++;
            if (SetType2 >= 5) {
              SetType2 = 1;
            }
          }
          if (Select == 4) {
            SetType3++;
            if (SetType3 >= 5) {
              SetType3 = 1;
            }
          }
          if (Select == 5) {
            if (AdaptedPowerUp[PowerUpType]) {
              AdaptedPowerUp[PowerUpType] = 0;
            }
            else {
              AdaptedPowerUp[PowerUpType] = 1;
            }
          }
          if (Select == 6) {
            co_await PrepareNewGame();
            Level = SetLevel - 1;
            NewType[0] = SetType1;
            NewType[1] = SetType2;
            NewType[2] = SetType3;
            GameType = SetGameType;
            Lives = 1;
            IsItCustomGame = 1;
            CustomGameFinish = 0;
            Select = 0;
            co_await NewLevel(1);

            if (AdaptedPowerUp[5])
              Freeze = 255;
            if (AdaptedPowerUp[4])
              MegaShot = 1;
            if (AdaptedPowerUp[3])
              Invulnerable = 255;
            if (AdaptedPowerUp[2])
              Lives = 10;
            if (AdaptedPowerUp[1]) {
              co_await port::Delay{(uint32_t)(500), __LINE__};
              co_await BuildBarriers();
            }
            
            
          }
            
          goto OptionSelected;
        }
      }
    }
    OptionSelected:
    
    if (UpButton || DownButton || LeftButton || RightButton || AButton || BButton) {
      ButtonHold = 1;
    }
    else {
      if (ButtonHold) {
        ButtonHold = 0;
      }
    }
    }
  //Debug info to display on the screen for when I need to see a value of a variable to fix something.
  /*
  char ShowDebug[5];
  itoa(EnemyCount, ShowDebug, 10);
  TEXT(ShowDebug, 75, 1, 1, BlackWhite);
  */
  

  co_return;
}


