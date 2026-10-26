static const unsigned char Player[] PROGMEM = {
0x78, 0xBC, 0x6A, 0x31, 0x31, 0x6A, 0xBC, 0x78, 
};

static const unsigned char destroyed[] PROGMEM = {
0x78, 0xB0, 0x60, 0x30, 0x30, 0x68, 0xB8, 0x70, 
};

void SpawnPlayerLasers(byte Xposit) {
      byte I = 0;
      while ((I < MaxPlayerLasers) && PlayerLaserY[I]) {
        I++;
      }
      if (!(I >= MaxPlayerLasers)) {
        PlayerLaserX[I] = Xposit;
        PlayerLaserY[I] = 50;
      }
}

void DeletePlayerLaser(byte PlayerLaserID) {
        
        Render(PlayerLaserX[PlayerLaserID], PlayerLaserY[PlayerLaserID], 1, BLACK);
        PlayerLaserX[PlayerLaserID] = 0;
        PlayerLaserY[PlayerLaserID] = 0;
  
}

void EndGame() {
            clearDisplay;
            ResetVariables();
            StartMove = 0;
            GameType = 10;
            AppearingEnemyType = random(1, 5);
            Menu = 1;
            TitleScreenMove = 1;
            IsItCustomGame = 0;
}

port::Task SpawnLaserIfCommanded() {
    if ((AButton || TitleScreenMove) && !ShootDelay) {
      if (MegaShot) {
        MegaShotCharge++;
        SOUND(1, MegaShotCharge * 6 + 140, 1);
        if (MegaShotCharge == 35) {
          SOLIDRECT(0, 0, 128, 64, 1);
          Display;
          for (byte l = 0; l < MaxPlayerLasers; l++) {
            SpawnPlayerLasers(PlayerX - 27 + l * 6);
          }
          MegaShotCharge = 255;
          MegaShot = 0;
          ShootDelay = 18;
          co_await port::Delay{(uint32_t)(50), __LINE__};
          clearDisplay;
        }
      }
      else {
        SpawnPlayerLasers(PlayerX + 3);
        SOUND(1, 1480, 1);
        ShootDelay = 18;
      }
    }
    if (MegaShotCharge && !AButton) {
      if (!ShootDelay) {
        SpawnPlayerLasers(PlayerX + 3);
        SOUND(1, 1480, 1);
        ShootDelay = 18;
      }
      MegaShotCharge = 0;
    }

  co_return;
}


port::Task RunPlayerCode() {
  Render(PlayerX, 55, 0, BLACK);
  if (!Death && !TitleScreenMove) {
    if (RightButton && PlayerX < 120) {
      PlayerX+=2;
    }
    if (LeftButton && PlayerX > 0) {
      PlayerX-=2;
    }
    co_await SpawnLaserIfCommanded();
  }
  Render(PlayerX, 55, 0, WHITE);

  if (TitleScreenMove) {
// PXA: mutable local state lifted to resettable sketch globals.
// PXA: mutable local state lifted to resettable sketch globals.
    Render(PlayerX, 55, 0, BLACK);
    if (MainScreenMove) {
      PlayerX--;
    }
    AutoInterval--;
    if (!AutoInterval) {
      AutoInterval = random(5, 41);
      AutoBackForth = random(-1, 2);
    }
    if (!Death) {
      PlayerX += AutoBackForth * 2;
      Render(PlayerX, 55, 0, WHITE);
      co_await SpawnLaserIfCommanded();
    }
  }
  
  for (byte I = 0; I < MaxPlayerLasers; I++) {
    if (PlayerLaserY[I]) {
      Render(PlayerLaserX[I], PlayerLaserY[I], 1, BLACK);
      PlayerLaserY[I] -= 2;
      if (MainScreenMove) {
        PlayerLaserX[I]--;
      }
      //Laser will eventually hit Y position 0 (when it goes beyond 255), and that will deactivate the laser.
      if (PlayerLaserY[I]) {
        Render(PlayerLaserX[I], PlayerLaserY[I], 1, WHITE);
      }

      if ((PlayerLaserY[I] < 17) && (PlayerLaserX[I] >= (BonusAlienX - 16)) && (PlayerLaserX[I] <= BonusAlienX)) {
        if (BonusTwoHits) {
          ExplosionSound = 7;
          BonusTwoHits = 0;
          InvulBonus = 25;
        }
        else {
          if (!(InvulBonus > 20)) {
            ExplosionSound = 17;
            Render(BonusAlienX, 1, 2, BLACK);
            Score += random(1, 11) * 10;
            Blast(BonusAlienX - 9, 20);
            
            PowerUpX = BonusAlienX - 12;
            PowerUpY = 11;
            BonusAlienX = 144;
          }
          else {
            SpawnLaser(PlayerLaserX[I], 16, 0);
          }
        }
        DeletePlayerLaser(I);
      }

      if ((PowerUpType == 1) && BonusAlienX) {
        if ((PlayerLaserY[I] < 22) && (PlayerLaserX[I] >= (BonusAlienX - 14)) && (PlayerLaserX[I] <= (BonusAlienX - 2))) {
          DeletePlayerLaser(I);
        }
      }
    }
  }
  
  if (ShootDelay) {
    ShootDelay--;
  }

      if (Death) {
        //ard.tunes.noTone();
        SOUND(1, Death, 1);
        Death -= 4;
        Render(PlayerX, 55, 0, BLACK);
        Render(PlayerX, 55, 4, WHITE);
        if (Death == 196) {
          Blast(PlayerX, 55);                   //Get ready for game over by deleting progress! Player could just switch off Arduboy and turn back on
          if ((Lives == 1) && !IsItCustomGame)  //to resume gameplay without losing progress, even though they lost and would have got a game over in
            EraseSaveProgress();                //the last game.
          
        }
        if (Death < 1) {
          Death = 0;
          if (!TitleScreenMove) {
            co_await port::Delay{(uint32_t)(500), __LINE__};
            if (!(IsItCustomGame && Lives == 10))
              Lives--;
          }
          if (!Lives) {
            TEXT(F("Game Over"), 10, 30, 2, 1);
            SOLIDRECT(33, 47, 99, 55, 0);
            TEXT(F("Score: "), 33, 47, 1, 1);
            ard.print(Score);
            
            Display;
            co_await GameOverMelody();
            co_await port::Delay{(uint32_t)(2000), __LINE__};
            EndGame();
          }
          else {
              Invulnerable = 40;
              Flash = 0;
          }
        }
      }

      if (MegaShot) {
        if (!random(10)) {
          InvulSparkle();
        }
      }
      if (MegaShotCharge > 244) {
        SOUND(1, (MegaShotCharge - 245) * 70, 1);
        MegaShotCharge--;
        if (MegaShotCharge < 245) {
          MegaShotCharge = 0;
        }
      }


      if (Invulnerable) {
        Flash++;
        if (Flash > 2) {
          if (Invulnerable > 40) {
              if (!TitleScreenMove && MELODY(F("^ ^ ^ !^ ^ !^!^ "), 3))
                MusicPlace = 0;
          //noTone(AudioPin);
          if (random(2)) {
            InvulSparkle();
          }
          }
          Flash = 0;
        }

        if (!Flash || (Flash == 2)) {
          Render(PlayerX, 55, 0, BLACK);
        }
        
        Invulnerable--;
        
      }

  co_return;
}

void InvulSparkle() {
  SpawnParticle(1, PlayerX + random(9), 55 + random(7), 0);
}


