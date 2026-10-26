
port::Task UFOAppear() {
  unsigned int WoozieSound = 900;  //A woozy up down sound.
  boolean WoozieUpDown = 0;
  for (byte AppearingPos = 0; AppearingPos < 25; AppearingPos++) { //Make the UFO hover down while playing a woozy sound.
    co_await DisplayInfo(WHITE);
    DrawBackground();
    Render(56, AppearingPos, 14, WHITE);
    Display;
    for (byte k = 0; k < 100; k++) {
      if (AudioOn) {
        musicAndSound.tone(WoozieSound, 10);  //This sound effect used to sound really cool in a previous version, but due to library
      }                                       //change, it is now just a woozy sound effect. Boohoo...
      if (WoozieUpDown) {
        WoozieSound += 4;
      }
      else {
        WoozieSound -= 4;
      }
      co_await port::Delay{(uint32_t)(1), __LINE__};
    }
    if (WoozieUpDown) {
      WoozieUpDown = 0;
    }
    else {
      WoozieUpDown = 1;
    }
    Render(56, AppearingPos, 14, BLACK);
  }

  //Set up the UFO.
  UFOPoints = 0;
  UFOHP = Level * 8;
  UFOHasBombs = 0;
  EnemyCount = 9;
  UFOX = 56;
  UFOY = 16;
  co_await port::Delay{(uint32_t)(600), __LINE__};

  co_return;
}

port::Task RunUFOCode() {
// PXA: mutable local state lifted to resettable sketch globals.
// PXA: mutable local state lifted to resettable sketch globals.
// PXA: mutable local state lifted to resettable sketch globals.            //Create static local variables.
// PXA: mutable local state lifted to resettable sketch globals.
// PXA: mutable local state lifted to resettable sketch globals.
    if (UFOHP) {
      PlayMusic(2);          //Play the music.
    }
    Render(UFOX, UFOY, 7, BLACK);

  if (!Freeze) {
    if (UFOPoints < 200) {
      UFOX += UFOHoverMovement;
      if (((UFOX > 60) || (UFOX < 52)) && !UFODefeat) { //If UFO hasn't obtained enough aliens, just hover like normal.
        UFOHoverMovement *= -1;
      }
    }
    else {
          UFOX += UFOHoverMovement;  //Otherwise move all around the place, particularly to where the player is.
      if (UFOHP) {
          UFOX += UFOHoverMovement;
          if (UFOX < 4) {
            UFOX += 3;
          }
          if (UFOX > 108) {
            UFOX -= 3;
          }
          if ((UFOX + 4) > PlayerX) {
            UFOX--;
          }
          if ((UFOX + 4) < PlayerX) {
            UFOX++;
          }
        if (UFOY < UFOGlideTo) {
          UFOY++;
        }
        if (UFOY > UFOGlideTo) {
          UFOY--;
        }
        if (UFOY == UFOGlideTo) {
          UFOGlideTo = random(35);
          UFOHoverMovement *= -1;
        }
      }
    }
    UFOShoot -= 2;   //Countdown UFOShoot.
    if ((UFOShoot <= UFOPoints) && UFOHP) {  //If UFOShoot reaches the value of UFOPoints, set UFOShoot 226 and fire a laser (or bomb).
      UFOShoot = 226;                        //That means the more points the UFO has, the shorter the UFOShoot timer will be, thus the
      SpawnLaser(UFOX + 8, 21, UFOHasBombs); //faster the UFO will shoot lasers.
      if ((UFOPoints >= 200) && (random(5) == 4)) {  //If UFOPoints is over 200, occasionally shoot two extra lasers.
        SpawnLaser(UFOX + 2, 21, 0);
        SpawnLaser(UFOX + 14, 21, 0);
      }
    }
  }
    
    
    if (UFOHIT) {
      Render(UFOX, UFOY, 8, WHITE);   //Flash if hit by laser.
      UFOHIT--;
    }
    else {
      Render(UFOX, UFOY, 7, WHITE);   //Otherwise, render normally.
    }
    if (!UFOHP && UFODefeat <= 10) {
      Render(UFOX, UFOY, 7, BLACK);   //Make the UFO disappear when defeated and the defeat animation is over.
    }

    if (MegaShotCharge == 254) {      //If the player uses the mega shot power up, the UFO takes damage based on its HP.
      UFOHIT = 10;
      UFOHP -= Level * 2;
      if (UFOHP <= 0) {    //And if the UFO is defeated by mega shot, trigger the death animation.
        Freeze = 0;
        UFODefeat = 100;
        UFOHP = 0;
      }
    }
    


    
    for (byte I = 0; I < MaxPlayerLasers; I++) {   //Detect if a laser has hit the UFO.
      if ((PlayerLaserY[I] < UFOY + 8) && (PlayerLaserY[I] > UFOY) && (PlayerLaserX[I] >= UFOX) && (PlayerLaserX[I] <= (UFOX + 16)) && UFOHP) {
        SOUND(1, 277, 1);
        if (random(0, 201 + UFOPoints) <= 200) {
          UFOHP--;
          if (!UFOHP) {
            Freeze = 0;
            UFODefeat = 100;
          }
        }
        UFOHIT = 10;
        DeletePlayerLaser(I);
      }
    }

    //UFO death animation.
    if (!UFOHP) {
      UFOHoverMovement *= -1; //Doing this operation will make the UFO shake.
      UFODefeat--;
      if ((UFODefeat == 93) || (UFODefeat == 67) || (UFODefeat == 50) || (UFODefeat == 30)) {
        ExplosionSound = random(10, 21);  //Do 4 blasts...
        Blast(UFOX + random(17), 20);
      }
      if (UFODefeat == 10) { //...and then, one final blast as the UFO is destroyed and gone.
        ExplosionSound = 15;
        Blast(UFOX + 8, 20);
        Score += Level * 80 + UFOPoints;
        UFOPoints = 0;
      }
      if (!UFODefeat) { //once the animation is finished, move on to the next level.
        co_await NewLevel(0);
      }
    }

  co_return;
}

