//This code also includes code for power ups.

static const unsigned char BonusAlien[] PROGMEM = {
0xC0, 0x20, 0x50, 0xD0, 0x48, 0x48, 0x28, 0xE8, 0x28, 0x4A, 0x4C, 0xD4, 0x5E, 0x2F, 0xD7, 0x12,
0x03, 0x1E, 0x24, 0x45, 0x08, 0x09, 0x0A, 0x0A, 0x0A, 0x08, 0x08, 0x05, 0x44, 0x3E, 0x01, 0x00
};

static const unsigned char InvulBonusAlien[] PROGMEM = {
0xC0, 0xE0, 0xB0, 0x30, 0xB8, 0xB8, 0xD8, 0x18, 0xD8, 0xBA, 0xBC, 0x34, 0xBA, 0xE9, 0xD5, 0x12,
0x03, 0x1F, 0x27, 0x46, 0x0F, 0x0E, 0x0D, 0x0D, 0x0D, 0x0F, 0x0F, 0x06, 0x47, 0x3F, 0x01, 0x00
};

static const unsigned char InvulPower[] PROGMEM = {
0x7E, 0x91, 0x99, 0xBF, 0xFD, 0x99, 0x89, 0x7E, 
};

static const unsigned char FixbarrierPower[] PROGMEM = {
0x7E, 0x99, 0x99, 0x99, 0x99, 0x99, 0x99, 0x7E, 
};

static const unsigned char UpPower[] PROGMEM = {
0x7E, 0x81, 0xC5, 0xFF, 0xFF, 0xC1, 0x81, 0x7E, 
};

static const unsigned char MegashotPower[] PROGMEM = {
0x7E, 0x81, 0xDD, 0x81, 0x81, 0xDD, 0x81, 0x7E, 
};

static const unsigned char StopPower[] PROGMEM = {
0x3C, 0x42, 0xBD, 0xBD, 0xBD, 0xBD, 0x42, 0x3C, 
};



port::Task RunBonusAlienCode() {
    BonusAlienInterval--;
    if (!BonusAlienInterval) {
      BonusAlienX = 1;            //Make the bonus alien appear in the area.
      PowerUpType = random(1, 6);
      if (PowerUpType == 2) {
        BonusTwoHits = 1;    //If the bonus alien is holding the 1UP power up, give it two hitpoints.
      }
      else {
        BonusTwoHits = 0;
      }
      if (PowerUpType == 3) {
        InvulBonus = 30;
      }

      
      if (EnemyCount < 5) {
        BonusAlienX = 144;   //If there is less than 5 aliens, Bonus Alien will not appear.
      }                      //The player could farm the Bonus Alien for power ups with one small alien wondering around if this code wasn't there.
    }
      
  if (BonusAlienX) {
      Render(BonusAlienX, 1, 2, BLACK);
      BonusAlienX++;
      if (PowerUpType == 5) {
        BonusAlienX++;       //A freeze power up makes the bonus alien go faster. Totally makes sense...
      }
      if (MainScreenMove) {
        BonusAlienX--;       //If the bonus alien is on the title screen, move to the left.
      }
      Render(BonusAlienX, 1, 2, WHITE);  //Now that the bonus alien's position is set, render the alien.

      if ((AnimationTick == 15) || ((!AnimationTick) && (PowerUpType == 4))) {
        SpawnLaser(BonusAlienX - 8, 16, 0);                  //Spawn lasers, and if holding the mega shot power up, shoot at twice the speed.
      }
      
      if (BonusAlienX >= 144) {          //If bonus alien makes it to the other side of the screen, or its position is set 144, then disappear.
        BonusAlienX = 0;
        BonusAlienInterval = random(150, 320);
      }
      if (InvulBonus) {
        InvulBonus--;      //Countdown bonus alien invincibility.
      }
      if (!InvulBonus && (PowerUpType == 3)) {
        InvulBonus = random(27, 51);     //Reset invincible timer if holding the invincibility power up.
      }
      
      }

  //If power up is dropped...
  if (PowerUpY) {
          Render(PowerUpX, PowerUpY++, 3, BLACK);  //Render the power up black and make it fall.
          if (MainScreenMove) {
            PowerUpX--;       //Make it move to the left if it's on the title screen.
          }
          Render(PowerUpX, PowerUpY, 3, WHITE);
        if (PowerUpY > 54) {
          SOUND(1, 46, 3);
          Render(PowerUpX, PowerUpY, 3, BLACK);   //If power up hits the ground, delete it! What a pain...
          PowerUpY = 0;
        }
        if ((PowerUpY > 46) && ((PowerUpX + 8) >= PlayerX) && (PowerUpX <= (PlayerX + 8))) {  //Player and power up collision.
          
  if (!TitleScreenMove) {
    SOUND(1, 2349, 2);
    co_await port::Delay{(uint32_t)(66), __LINE__};             //Play a collect sound effect.
    SOUND(1, 1760, 6);
    co_await port::Delay{(uint32_t)(200), __LINE__};
  }
    switch (PowerUpType) {
    case 1:
    co_await BuildBarriers();      //Power up type 1: Fix barrier power up.
    break;
    case 2:
    Lives++;   //Power up type 2: 1UP.
               //Originally, collecting the power up would play the short 1UP melody from Super Mario Bros,
               //but because only notes from the D-minor scale can be used, this melody had to be scrapped.
      /*
    if (!TitleScreenMove) {
    for (byte e = 0; e < 6; e++) {
      MELODY(F("*AJFHM"), e, 5);
      delay(140);
    }
    }
    */
    break;
    case 3:
    Invulnerable = 255;   //Power up type 3: Invincibility.
    MusicPlace = 0;
    Death = 0;       //I got SO mad at my own game when I was JUST about to collect the power up, but hit a laser first, that I decided to cancel out death!
    break;
    case 4:
    MegaShot = 1;    //Power up type 4: Mega shot.
    break;
    case 5:
    Freeze = 195;    //Power up type 5: Freeze enemies.
    break;
  }
          Render(PowerUpX, PowerUpY, 3, BLACK);   //Delete the power up.
          PowerUpY = 0;
        }
  }

  co_return;
}

