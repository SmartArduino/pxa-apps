#define JellyAttackSpeed random(147, 228) - Level
#define RangerAttackSpeed random(11, 25)
#define DevilAttackSpeed random(27, 108)
#define BomberAttackSpeed random(96, 188) - Level


static const unsigned char JellyA[] PROGMEM = {
0x1C, 0x7A, 0x9F, 0x5F, 0x1F, 0x7B, 0x9E, 0x5C, 
};

static const unsigned char JellyB[] PROGMEM = {
0x1C, 0xFA, 0x9E, 0x1E, 0x1E, 0xFA, 0x9E, 0x1C, 
};

static const unsigned char RangerA[] PROGMEM = {
0x8C, 0x52, 0x29, 0x25, 0x25, 0x29, 0x52, 0x8C, 
};

static const unsigned char RangerB[] PROGMEM = {
0x0C, 0x52, 0xA9, 0x29, 0x29, 0xA9, 0x52, 0x0C, 
};

static const unsigned char DevilA[] PROGMEM = {
0xB6, 0x4F, 0x9C, 0x88, 0x88, 0x9C, 0x4F, 0x36, 
};

static const unsigned char DevilB[] PROGMEM = {
0x36, 0x4F, 0xAC, 0x88, 0x88, 0xAC, 0x4F, 0xB6, 
};

static const unsigned char BomberA[] PROGMEM = {
0x7E, 0x9F, 0x9B, 0x1F, 0x1F, 0x9B, 0x9F, 0x7E, 
};


static const unsigned char BomberB[] PROGMEM = {
0xFE, 0x9F, 0x17, 0x3F, 0x3F, 0x17, 0x9F, 0xFE, 
};

static const unsigned char Bom[] PROGMEM = {
0x20, 0x63, 0x55, 0x9A, 0x92, 0x04, 0xFC, 0xA8, 0x54, 0x03, 0xFA, 0x8C, 0xF8, 0x04, 0xFA, 0x11,
0x22, 0x1C, 0xF8, 0x04, 0x82, 0x99, 0x67, 0x20, 
};

//Unused bitmaps which are silhouette of Devils and Rangers.
/*

static const unsigned char DevilclearDisplay[] PROGMEM = {
0x36, 0xFF, 0xFC, 0x78, 0x78, 0xFC, 0xFF, 0x36, 
};

static const unsigned char RangerclearDisplay[] PROGMEM = {
0x8C, 0x5E, 0xBF, 0x3F, 0x3F, 0xBF, 0x5E, 0x8C, 
};


*/

port::Task SpawnEnemy(byte PlaceX, byte PlaceY, byte Type, boolean IsThereDelay) {
  //if IsThereDelay is true, there will be a quick beep sound and a short delay. Used in spawning a block of aliens.
  byte O = 0;
  while ((O < MaxEnemies) && EnemyType[O]) {
    O++;
  }
  if (O >= MaxEnemies) {
    co_return;
  }
  EnemyCount++;
  EnemyX[O] = PlaceX;
  EnemyY[O] = PlaceY;
    //Alien types:
    //1- Jelly
    //2- Ranger
    //3- Devil
    //4- Bomber
  EnemyType[O] = Type;
  EnemyDirection[O] = random(8);
  //Directions:
  //0- Up
  //1- Up Right
  //2- Right
  //3- Down Right
  //4- Down
  //5- Down Left
  //6- Left
  //7- Up Left

  //On UFO game type, spawn aliens that shoot lasers shortly after spawning.
  if (GameType == 3) {
    EnemyShootInterval[O] = random(40);
  }
  else {
  //Prepare the ShootInterval for enemies. For Jellies in alien blocks, make their ShootInterval faster so that they shoot displays of lasers while in a big block.
  switch (EnemyType[O]) {
    case 1:
    if (StartMove) {
      EnemyShootInterval[O] = random(80);
    }
    else {
      EnemyShootInterval[O] = JellyAttackSpeed;
    }
    break;
    case 2:
    EnemyShootInterval[O] = RangerAttackSpeed;
    break;
    case 3:
    EnemyShootInterval[O] = DevilAttackSpeed;
    break;
    case 4:
    EnemyShootInterval[O] = BomberAttackSpeed;
    break;
  }
  }
              //Prepare MoveInterval.
              if (EnemyType[O] < 3) {
                EnemyMoveInterval[O] = random(5, 41);
              }
              if (EnemyType[O] == 3) {
                EnemyMoveInterval[O] = random(14, 34);
              }
              if (EnemyType[O] == 4) {
                EnemyMoveInterval[O] = random(26, 71);
              }
  
  if (IsThereDelay) {
    SOUND(1, 234 + O * 24, 1);
    RenderEnemy(O, WHITE, 0);
    Display;
    co_await port::Delay{(uint32_t)(70), __LINE__};
  }

  co_return;
}


void SpawnLaser(byte Xposition, byte Yposition, boolean BombOrNot) {
  byte O = 0;
  while ((O < MaxEnemyLasers) && LaserY[O]) {
    O++;     //Look for an available space in LaserY
  }
  if (O >= MaxEnemyLasers) {
    return;  //If there is no space for more lasers, don't spawn a laser.
  }
  //musicAndSound(4000, 20);            Annoying high-pitched sound.
  LaserX[O] = Xposition;
  LaserY[O] = Yposition;
  LaserBomb[O] = BombOrNot; //If the LaserBomb is true, the laser will be a bomb instead.
}

//A function that is run in the loop() script to make SlowMove and MainScreenMove flicker to 0 to 1.
//MoveSlow is used to make Bombers slow (move 1 pixel every 2 frames) and MainScreenMove, when active, makes everything move from
//right to left in the title screen.
void SlowControl() {
  if (MoveSlow) {
    MoveSlow = 0;
  }
  else {
    MoveSlow = 1;
  }

  
  if (TitleScreenMove) {
    MainScreenMove = MoveSlow;
  }
  else {
    MainScreenMove = 0;
  }
}


//Controls all the enemys' lasers.
void EnemysLaser() {
      for (byte n = 0; n < MaxEnemyLasers; n++) { //Go through every laser currently active.
        if (LaserY[n]) {
          if (LaserBomb[n]) {
            if (LaserY[n] < 100) { //If the bomb is not in an explosion state, make the bomb fall like any other laser.
              Render(LaserX[n], LaserY[n], 6, 0);
              LaserY[n]++;
            }
          }
          else {
            LINE(LaserX[n], LaserY[n] + 3, LaserX[n], LaserY[n], 0); //Make the laser fall.
            //SOLIDRECT(LaserX[n] - 1, LaserY[n] + 3, LaserX[n] + 1, LaserY[n] - 1, 0); <-- Makes lasers stand out against the background but was too laggy and didn't work.
            LaserY[n]++;
          }
          if (MainScreenMove) { //If lasers are on title screen, move to the left.
            LaserX[n]--;
            if (!LaserX[n]) { //If the laser is at the edge of the screen, delete the laser.
              if (LaserBomb[n]) {
                Render(1, 63, 5, BLACK); //If the laser is a bomb, erase the explosion.
              }
              LaserY[n] = 0;
            }
          }
          
          if ((LaserY[n] > 47) && !(WithinBarrier(LaserX[n]) == 5)) { //Lasers and barriers.
            if (!LaserBomb[n]) { //If the laser is not a bomb, just delete the laser.
              DestroyBarrier(LaserX[n]); //Damage the barrier.
              LaserY[n] = 0;
            }
            else {
              if (LaserY[n] < 100) {
                DestroyBarrier(LaserX[n]); //Damage the barrier.
                LaserY[n] = 230;   //Turn bomb into an explosion.
                ExplosionSound = 20; //Explosion sound effect
              }
            }
            
          }
          
          if (LaserY[n] > 59) {
            if (LaserBomb[n]) {
              if (LaserY[n] < 100) {
              Explosion(LaserX[n]);  //BOOM! Bomb hits the ground and explodes!
              LaserY[n] = 140;
              }
            }
            else {
              LaserY[n] = 0;   //If it's not a bomb, just delete the laser.
            }
          }
          else {
            //If player is not already destroyed and laser is collided with player...
            if (!Death && (LaserY[n] > 50) && (LaserX[n] >= PlayerX) && (LaserX[n] <= (PlayerX + PlayerHitbox))) {
              if (!LaserBomb[n]) {
              if (Invulnerable > 40) {
                SpawnPlayerLasers(LaserX[n]);       //Reflect laser by spawning a player laser in place of alien's laser.
              }
              else if (!Invulnerable) {
                Death = 200;                     //Destroy player.
              }
              LaserY[n] = 0;           //Delete laser.
              }
              else {
                if (LaserY[n] < 100) { //If the laser is a bomb, don't just destroy the player, blow 'em up!
                  Explosion(LaserX[n]);
                  LaserY[n] = 130;
                }
              }
            }
            else {                 //If laser hasn't collided with anything and nothing has happened, just render the laser.
              if (LaserBomb[n]) {
                if (LaserY[n] < 100) {
                  Render(LaserX[n], LaserY[n], 6, 1);
                }
              }
              else if (LaserY[n]) {
                LINE(LaserX[n], LaserY[n] + 3, LaserX[n], LaserY[n], 1);
              }
            }
          }

          //This segment of code makes explosions happen! (if the bomb is in an explosion state, meaning LaserY is bigger than 100 or 200)
          if (LaserBomb[n] && LaserY[n] > 100) {
            byte ATY;            //AtYposition. Tells where the explosion is going to be (on the ground or on one of the barriers.)
            if (LaserY[n] > 200) {
              ATY = 53;         //> 200 means on a barrier.
            }
            else {
              ATY = 63;         //> 100 but < 200 means on the ground.
            }
            if (MainScreenMove) {
              Render(LaserX[n] + 1, ATY, 5, BLACK); //If the explosion is on the title screen, clear the previous explosion so it doesn't 'melt' on the screen.
            }
            Render(LaserX[n], ATY, 5, WHITE); //Render the explosion.
            LaserY[n]--;
            if ((LaserY[n] == 200) || (LaserY[n] == 100)) { //If the explosion is complete, delete the laser (or rather, the explosion).
              Render(LaserX[n], ATY, 5, BLACK);
              LaserY[n] = 0;
            }
          }
        }
      }
}

void MoveEnemy(byte EnemyID) {
  //If the player has not collected the freeze power up, do the following:
  if (!Freeze) {
    //If the game type is 3 (UFO game type), then move towards UFO.
  if (GameType == 3) {
    if (EnemyY[EnemyID] > UFOY) {
      EnemyY[EnemyID]--;
    }
    if (EnemyY[EnemyID] < UFOY) {
      EnemyY[EnemyID]++;
    }
    if (EnemyX[EnemyID] < (UFOX + 4)) {
      EnemyX[EnemyID]++;
    }
    if (EnemyX[EnemyID] > (UFOX + 4)) {
      EnemyX[EnemyID]--;
    }
  }
  else { //If the game type is something different, then move normally, based on direction.
    byte Direction = EnemyDirection[EnemyID];
    if ((Direction == 0) || (Direction == 1) || (Direction == 7)) {
      EnemyY[EnemyID]--;
    }
    if ((Direction >= 1) && (Direction <= 3)) {
      EnemyX[EnemyID]++;
    }
    if ((Direction >= 3) && (Direction <= 5)) {
      EnemyY[EnemyID]++;
    }
    if ((Direction >= 5) && (Direction <= 7)) {
      EnemyX[EnemyID]--;
    }
  }
            
            //Test if enemy is out of the area.
            boolean OutRight = (EnemyX[EnemyID] >= 118);
            boolean OutLeft = (EnemyX[EnemyID] <= 3);
            boolean OutUp = (EnemyY[EnemyID] <= 3);
            boolean OutDown = (EnemyY[EnemyID] >= 34);

            if (TitleScreenMove) {
              OutRight = 0;
              OutLeft = 0;
            }
            //If out of the area, change direction to something else and nudge back into the area.
            if (OutRight || OutLeft || OutUp || OutDown) {
              EnemyMoveInterval[EnemyID] = 0;
              if (OutRight) {
                EnemyX[EnemyID]--;
              }
              if (OutLeft) {
                EnemyX[EnemyID]++;
              }
              if (OutUp) {
                EnemyY[EnemyID]++;
              }
              if (OutDown) {
                EnemyY[EnemyID]--;
              }
            }
  }
}

port::Task RunEnemyCode() {
  for (byte s = 0; s < MaxEnemies; s++) {
        if (EnemyType[s]) {
          //When game type is 1 (Big block of aliens), StartMove will be active.
          if (StartMove) {
            if (!Freeze) {
            if ((StartMove <= 45) || ((StartMove > 95) && (StartMove <= 140)) || ((StartMove > 168) && (StartMove <= 180)) || ((StartMove > 205) && (StartMove <= 217)))
              EnemyX[s]++; //Go Right
            if (((StartMove > 67) && (StartMove <= 73)) || ((StartMove > 162) && (StartMove <= 168)))
              EnemyY[s]++; //go down a bit
            if (((StartMove > 73) && (StartMove <= 95)) || ((StartMove > 140) && (StartMove <= 162)) || ((StartMove > 180) && (StartMove <= 205)) || ((StartMove > 45) && (StartMove <= 67)))
              EnemyX[s]--; //go left
            if ((StartMove > 217) && (StartMove <= 229))
              EnemyY[s]--; //and now move up.
            
            
          
            //If the enemy type is Jelly, shoot lasers. Any other enemy will not be able to shoot lasers while StartMove is active.
            if (EnemyType[s] == 1) {
              EnemyShootInterval[s]--;
              if (!EnemyShootInterval[s]) {
                SpawnLaser(EnemyX[s] + 4, EnemyY[s] + 4, 0);
                EnemyShootInterval[s] = JellyAttackSpeed;
              }
              }
            }
          }
          else { //Code for when StartMove is not active.
            if (MainScreenMove) {
              EnemyX[s]--; //Move Enemies to the left if they're on the title screen.
            }
            if (!Freeze)
              EnemyMoveInterval[s]--; //Countdown EnemyMoveInterval (unless frozen).
            if (EnemyType[s] < 3) {
              MoveEnemy(s);       //Jellys and Rangers move at a medium speed.
            }
            if (EnemyType[s] == 3) {
              MoveEnemy(s);
              if (!((GameType == 3) && (EnemyX[s] > (UFOX - 26)) && (EnemyX[s] < (UFOX + 34))))
                MoveEnemy(s);     //Because devils move fast, add another MoveEnemy function to double their speed (as long as they aren't too close to the UFO).
            }
            if (EnemyType[s] == 4) {
              if (MoveSlow) {
                MoveEnemy(s);   //Bombers move slow, so only move once every two frames.
              }
            }

            if (!Freeze) {
            if (!EnemyMoveInterval[s]) {   //Once EnemyMoveInterval is up, reset EnemyMoveInterval and change direction.
              EnemyDirection[s] = random(8);
              if (EnemyType[s] < 3) {
                EnemyMoveInterval[s] = random(41);
              }
              if (EnemyType[s] == 3) {
                EnemyMoveInterval[s] = random(24);
              }
              if (EnemyType[s] == 4) {
                EnemyMoveInterval[s] = random(26, 71);
              }
            }
            }

            if (!Freeze) {
            EnemyShootInterval[s]--;  //Countdown EnemyShootInterval.
            if (!EnemyShootInterval[s]) { //Once EnemyShootInterval is up, shoot a laser (or bomb if type is Bomber) and reset EnemyShootInterval.
              if (EnemyType[s] == 1) {
                SpawnLaser(EnemyX[s] + 4, EnemyY[s] + 4, 0);
                EnemyShootInterval[s] = JellyAttackSpeed;
              }
              if (EnemyType[s] == 2) {
                if ((EnemyX[s] <= (PlayerX + 8)) && (EnemyX[s] >= (PlayerX - 8))) {
                  SpawnLaser(EnemyX[s] + 4, EnemyY[s] + 4, 0);
                }
                EnemyShootInterval[s] = RangerAttackSpeed;
              }
              if (EnemyType[s] == 3) {
                SpawnLaser(EnemyX[s] + 4, EnemyY[s] + 4, 0);
                EnemyShootInterval[s] = DevilAttackSpeed;
              }
              if (EnemyType[s] == 4) {
                SpawnLaser(EnemyX[s] + 4, EnemyY[s] + 3, 1);
                EnemyShootInterval[s] = BomberAttackSpeed;
              }
            }
            }
  
          }

          if (GameType == 3) { //If game type if 3 (UFO game type), test for collision with the UFO:
            if ((EnemyX[s] >= UFOX) && (EnemyX[s] <= (UFOX + 8)) && (EnemyY[s] >= UFOY) && (EnemyY[s] <= (UFOY + 6))) {
              //If alien is colliding with UFO, delete the alien and increase the UFO score.
              EnemyCount--;
              UFOPoints += EnemyType[s] * 10;
              if (UFOPoints > 200) {
                UFOPoints = 200;
              }
              if (EnemyType[s] == 4) {
                UFOHasBombs = true;   //Better not let any Bombers get to the UFO! That's a guarenteed Game Over if they make it!
              }
              EnemyType[s] = 0;
              SOUND(1, UFOPoints * 2 + 70, 3);
            }
          }
          //This code was originally going to make aliens have a random chance of getting destroyed, reguardless of getting hit by a laser or not,
          //when the player shot a large wave of lasers. However, this has caused some bugs, plus I later didn't like the idea.
          /*
        if (MegaShotCharge == 254) {
          if (random(10) == 9) {
            Blast(EnemyX[s], EnemyY[s]);
            RenderEnemy(s, BLACK, 0);
            Score += EnemyType[s] * 10;
            EnemyX[s] = 0;
            EnemyY[s] = 0;
            EnemyType[s] = 0;
            EnemyDirection[s] = 0;
            EnemyCount--;
            CurrentEnemies--;
          }
        }
        */

          
          RenderEnemy(s, WHITE, 0); //Render the enemy.
        for (byte I = 0; I < MaxPlayerLasers; I++) {   //Check if all the current player lasers are colliding with the enemy.
          if ((PlayerLaserY[I] < EnemyY[s] + 8) && (PlayerLaserY[I] > EnemyY[s]) && (PlayerLaserX[I] >= EnemyX[s]) && (PlayerLaserX[I] <= (EnemyX[s] + 8))) {
            //If the enemy gets hit by a laser, add score and destroy alien.
            SOUND(1, 277, 1);
            RenderEnemy(s, BLACK, 0);
            Score += EnemyType[s] * 10;
            Blast(EnemyX[s], EnemyY[s]);
            EnemyType[s] = 0;
            
            EnemyCount--;
            CurrentEnemies--;
            DeletePlayerLaser(I);
          }
        }
        
        if ((EnemyX[s] > 247) && TitleScreenMove) { //If enemy is on the edge of screen in title screen (EnemyX resets to 255 if on edge), delete the enemy.
          RenderEnemy(s, BLACK, 0);
          EnemyType[s] = 0;
          EnemyCount--;
        }
        
        }
      }

      if (StartMove && !Freeze) {
        StartMove++; //Increase StartMove if it's active (and if player hasn't collected Freeze power up).
      }
      if (StartMove > 229) {
        StartMove = 0; //Once all the aliens are in the middle, they all split up and StartMove disables.
      }

      //Make a new level if there are no enemies remaining.
      if (!EnemyCount && (GameType == 1)) {
        co_await NewLevel(0);
      }
      if (!CurrentEnemies && (GameType == 2)) {
        co_await NewLevel(0);
      }

      //Spawn an enemy from the side of the screen (if game type is 3) or from the top of the screen (if game type is 2).
      if (AppearingEnemyX || AppearingEnemyY) {
        if (GameType == 2) {
        RenderEnemy(0, BLACK, 1);
        AppearingEnemyY++;
        RenderEnemy(0, WHITE, 1);
        if (AppearingEnemyY > 4) {
          co_await SpawnEnemy(AppearingEnemyX, AppearingEnemyY, AppearingEnemyType, 0);
          AppearingEnemyX = 0;
          AppearingEnemyY = 0;
        }
        }
        else if (GameType == 3) {
          RenderEnemy(0, BLACK, 1);
          if (AppearingEnemyX < 5) {
            AppearingEnemyX++;
            RenderEnemy(0, WHITE, 1);
          }
          else if (AppearingEnemyX > 117) {
            AppearingEnemyX--;
            RenderEnemy(0, WHITE, 1);
          }
          else {
            co_await SpawnEnemy(AppearingEnemyX, AppearingEnemyY, AppearingEnemyType, 0);
            AppearingEnemyX = 0;
            AppearingEnemyY = 0;
            IncomingY = random(4, 34);
            IncomingLeftOrRight = random(2);
          }
        }
      }
    if (GameType == 2) {
        PlayMusic(1);
      if (!Freeze) {
        if ((AnimationTick == 15) && EnemiesToSpawn) {
          AppearingEnemyX = random(4, 117);
          AppearingEnemyY = -8;
          EnemiesToSpawn--;
        }
      }
    }

    if (GameType == 1) {
      PlayMusic(4);
    }
    if (!Freeze) {
      if ((GameType == 3) && (UFOPoints < 200)) {
        if (UFOAliensAppear) {
          UFOAliensAppear--;
          if (UFOAliensAppear < 25) {
            if (IncomingLeftOrRight) {
              Render(2, IncomingY, 9, MoveSlow);
            }
            else {
              Render(114, IncomingY, 10, MoveSlow);
            }
          }
        }
        else {
          if (IncomingLeftOrRight) {
            Render(2, IncomingY, 9, 0);
          }
          else {
            Render(114, IncomingY, 10, 0);
          }
          AppearingEnemyType = RandomAlienType();
          if (IncomingLeftOrRight) {
            AppearingEnemyX = -10;
          }
          else {
            AppearingEnemyX = 130;
          }
          AppearingEnemyY = IncomingY; 
          UFOAliensAppear = random(66, 171);
          
        }
      }
    }
    

      if (Freeze) {
        Freeze--;
      }

  co_return;
}

void ClearEnemiesOnScreen() {
  for (byte s = 0; s < MaxEnemies; s++) {
    if (EnemyType[s]) {
      RenderEnemy(s, BLACK, 0);
    }
  }
}

port::Task NewLevel(boolean LoadFromSave) {
  EnemyCount = 0;
  Level++;

if (IsItCustomGame) {
  if (CustomGameFinish) {
    co_await GameOverMelody();
    co_await port::Delay{(uint32_t)(100), __LINE__};
    EndGame();
    co_return;
  }
  else {
    CustomGameFinish = 1;
  }
}
  
  if (!LoadFromSave)
    GameType = random(1, 4);
  
  if (GameType == 1) {
    StartMove = 1;
    byte AlienT;
    for (byte j = 0; j < 3; j++) {
      if (LoadFromSave) {
        AlienT = NewType[j];
      }
      else {
        AlienT = RandomAlienType();
        NewType[j] = AlienT;
      }
      for (byte l = 0; l < 9; l++) {
        co_await SpawnEnemy(1 + l * 9, 1 + j * 9, AlienT, 1);
      }
    }
  }
  if (GameType == 2) {
    if (LoadFromSave) {
      AppearingEnemyType = NewType[0];
    }
    else {
      AppearingEnemyType = RandomAlienType();
    }
    CurrentEnemies = random(35, 51);
    EnemiesToSpawn = CurrentEnemies;
  }
  if (GameType == 3) {
    co_await UFOAppear();
  }
  if (!IsItCustomGame)
    SaveProgress();

  co_return;
}

byte RandomAlienType() {
  byte AlienRandomizer = random(1, Level + 6);
  if ((AlienRandomizer <= 4) && (AlienRandomizer >= 1)) {
    return 1;
  }
  if ((AlienRandomizer >= 5) && (AlienRandomizer <= 7)) {
    return 2;
  }
  if ((AlienRandomizer >= 8) && (AlienRandomizer <= 11)) {
    return 3;
  }
  if (AlienRandomizer > 11) {
    return 4;
  }
  return 1; // byte conversion may wrap at very high levels
}

void Explosion(byte xpos) {
  ExplosionSound = 20; //Explosion sound effect
  if (((PlayerX + 8) >= (xpos - 10)) && (PlayerX <= (xpos + 10)) && !Invulnerable && !Death) {
        Death = 200;
  }
}



