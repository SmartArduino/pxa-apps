/*
Missile Command Clone
*/

#include <Arduboy2.h>

#define CURSOR_MOVE_SPEED 1
#define CURSOR_FLOOR 56
#define MISSILE_FLOOR 63
#define MAX_INCOMING 8
#define MAX_LIVE_INCOMING 6
#define MAX_OUTGOING 6
#define MAX_MISSILES 14
#define TURRET_0_START_INDEX 5
#define TURRET_0_STOP_INDEX 8
#define TURRET_1_START_INDEX 8
#define TURRET_1_STOP_INDEX 11
#define MISSILE_FRAME_DELAY 150
#define PLAYER_MISSILE_SPEED 0.8
#define MAX_INCOMING_SPEED 0.2
#define MIN_INCOMING_SPEED 0.1
#define MINIMUM_FRAME_SPAWN_DELAY 20
#define FREE_LIFE_SCORE_MODULO 100
#define CITY_TARGET_Y 59

#define MENU 0
#define PLAYING 1
#define GAMEOVER 2
#define PAUSED 3

struct Missile {
  public:
    byte startX;
    byte startY;
    byte target;
    byte stopX;
    byte stopY;
    float currentY;
    float currentX;
    float speedY;
    bool incoming;
    bool valid;
    byte explosionFrame;
    byte explosionRadius;
    bool scored;
};

struct Plane {
  public:
    byte startY;
    bool dir;
    float currentX;
    byte explosionFrame;
    byte explosionRadius;
    bool valid;
    bool scored;
};

const uint8_t PROGMEM planeBitmap1[] = {0x01, 0x00, 0x06, 0x40, 0x7F, 0xC0, 0xFF, 
0x80, 0x0E, 0x00, 0x03, 0x00, 0x00, 0x80, 
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
0x00};
const uint8_t PROGMEM planeBitmap0[] = {0x20, 0x00, 0x98, 0x00, 0xFF, 0x80, 0x7F, 
0xC0, 0x1C, 0x00, 0x30, 0x00, 0x40, 0x00, 
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
0x00};
const uint8_t PROGMEM turret[] = {0x00, 0x00, 0x00, 0x00, 0x02, 0x40, 0x03, 0xC0, 
0x07, 0xE0, 0x0F, 0xF0, 0x1F, 0xF8, 0x3F, 0xFC};

Arduboy2 arduboy;
byte gameState = MENU;
byte menuIndex = 0;
int highscore = 0;
bool targets[8] = {1,1,1,1,1,1,1,1};
int targetRubble[8] = {0,0,0,0,0,0,0,0};
int targetRubbleSecondLayer[8] = {0,0,0,0,0,0,0,0};
byte flashIndex[2] = {255, 255};
byte flashCounter[2] = {0,0};
byte targetXCoords[8] = {8, 24, 40, 56, 72, 88, 104, 120};
Plane plane;
Missile incomingMissiles[MAX_MISSILES];
int score = 0;
int nextDifficultyUpdateScore = 10;
int nextLivesUpdateScore = 100;
int nextPlaneUpdateScore = 25;
bool aFired = true;
bool bFired = true;
bool pendingLoss = false;
byte lossCounter = 0;
byte cursorX = 63;
byte cursorY = 26;
bool transitionLock = false;
byte missileSpawnFrameDelay = MISSILE_FRAME_DELAY;
bool pauseLock = false;

///////////////////////////////////////

void setup() {
  arduboy.begin();
  // arduboy.beginNoLogo();
  arduboy.setFrameRate(60);

  byte eepromValid = 0;
  EEPROM.get(400, eepromValid);
  if (eepromValid != 111){
    EEPROM.put(400, 111);
    EEPROM.put(420, 0);
    EEPROM.put(421, 0);
  } else {
    EEPROM.get(420, highscore);
  }
  
  initNullMissile(0);
  initNullMissile(1);
  initNullMissile(2);
  initNullMissile(3);
  initNullMissile(4);

  initNullMissile(5);
  initNullMissile(6);
  initNullMissile(7);

  initNullMissile(8);
  initNullMissile(9);
  initNullMissile(10);
}

void loop() {
  if (!(arduboy.nextFrame())) return;
  updateState();
  drawState();
}

//////////////////////////////////////

void updateState(){
  switch(gameState){
    case MENU:
      menuHandler();
      break;
    case PLAYING:
      updateCursor();
      updateMissiles();
      missileSpawner();
      updateDifficulty();
      updateLives();
      updatePlane();
      checkForLoss();
      break;
    case GAMEOVER:
      gameoverHandler();
      updateMissiles();
      updatePlane();
      break;
    case PAUSED:
      pauseHandler();
      break;
  }
}

void drawState(){
  arduboy.clear();
  switch(gameState){
    case MENU:
      drawMenu();
      drawCities();
      drawScore();
      break;
    case PLAYING:
      drawMissiles();
      drawCities();
      drawPlane();
      drawCursor();
      drawScore();
      break;
    case GAMEOVER:
      drawGameover();
      drawMissiles();
      drawCities();
      drawPlane();
      drawScore();
      break;
    case PAUSED:
      drawMissiles();
      drawCities();
      drawPlane();
//      drawCursor();
      drawScore();
      drawPaused();
      break;
  }
  arduboy.display();
}

///////////////////////////////////////
void updateCursor(){
  if (arduboy.pressed(A_BUTTON)
      && arduboy.pressed(B_BUTTON)
      && arduboy.pressed(UP_BUTTON)
      && !pendingLoss){
      pauseLock = true;
      gameState = PAUSED;
      return;     
   }
  
  // TODO cursor acceleration?
  if (arduboy.pressed(LEFT_BUTTON) && cursorX > 0){
    cursorX = cursorX - CURSOR_MOVE_SPEED;
  } else if (arduboy.pressed(RIGHT_BUTTON) && cursorX < 127) {
    cursorX = cursorX + CURSOR_MOVE_SPEED;
  }

  if (arduboy.pressed(UP_BUTTON) && cursorY > 0){
    cursorY = cursorY - CURSOR_MOVE_SPEED;
  } else if (arduboy.pressed(DOWN_BUTTON) && cursorY < CURSOR_FLOOR) {
    cursorY = cursorY + CURSOR_MOVE_SPEED;
  }

  if (arduboy.pressed(A_BUTTON)){
    if (aFired == false) {
      aFired = true;
      if (targets[1]){
        fireMissile(0);
      } else {
        fireMissile(1);
      }
    }
  } else {
    aFired = false;
  }

  if (arduboy.pressed(B_BUTTON)){
    if (bFired == false){
      bFired = true;
      if (targets[6]){
        fireMissile(1);
      } else {
        fireMissile(0);
      }
    }
  } else {
    bFired = false;
  }
}

void drawCursor(){
  if (!pendingLoss){
    arduboy.drawLine(cursorX-2, cursorY, cursorX+2, cursorY, WHITE);
    arduboy.drawLine(cursorX, cursorY-2, cursorX, cursorY+2, WHITE);
  }
}

void initBonusPlane(){
  Plane bonusPlane;
  bonusPlane.startY = random(10,41);
  bonusPlane.dir = random(0,2);
  if (bonusPlane.dir == 0){
    bonusPlane.currentX = -10;
  } else {
    bonusPlane.currentX = 138;
  }
  bonusPlane.explosionFrame = 0;
  bonusPlane.explosionRadius = 0;
  bonusPlane.valid = true;
  bonusPlane.scored = false;
  plane = bonusPlane;
}

void initIncomingMissile(byte index){
  Missile newMissile;
  newMissile.startX = random(128);
  newMissile.startY = 0;
  newMissile.target = random(0, 8);
  newMissile.stopX = targetXCoords[newMissile.target];
  if (targets[newMissile.target] == 0){
    newMissile.stopY = 63;
  } else{
    newMissile.stopY = CITY_TARGET_Y;
  }
  newMissile.speedY = MIN_INCOMING_SPEED + ((MAX_INCOMING_SPEED - MIN_INCOMING_SPEED) * ((float) random(20)/20.0));
  newMissile.currentY = newMissile.startY;
  newMissile.currentX = newMissile.startX;
  newMissile.incoming = true;
  newMissile.valid = true;
  newMissile.explosionFrame = 0;
  newMissile.explosionRadius = 0;
  newMissile.scored = 0;
  incomingMissiles[index] = newMissile; 
}

void missileSpawner(){
  if ((arduboy.everyXFrames(missileSpawnFrameDelay) == true) && !pendingLoss && (liveIncomingCount() < 6)){
    for (int i = 0; i < MAX_INCOMING; i++){
      if (incomingMissiles[i].valid == false){
        initIncomingMissile(i);
        break;
      }
    }
  }
}

byte liveIncomingCount(){
  byte count = 0;
  for (int i = 0; i < MAX_INCOMING; i++){
    if (incomingMissiles[i].valid && (incomingMissiles[i].explosionFrame == 0)){
      count++;
    }
  }
  return count;
}

void initNullMissile(byte index){
  Missile nullMissile;
  nullMissile.valid = false;
  incomingMissiles[index] = nullMissile;
}

void updatePlane(){
  if (plane.valid){
    if (plane.explosionFrame > 120){
      plane.valid = false;
    } else if (plane.explosionFrame > 0){
      plane.explosionFrame++;
    } else {
      if ((plane.dir == 0) && plane.currentX < 138){
        plane.currentX += 0.2;
      } else if ((plane.dir == 1) && plane.currentX > -10){
         plane.currentX -= 0.2;
      } else {
         plane.valid = false;
      }
    }

       for (int j = 0; j < MAX_MISSILES; j++) {
           if (incomingMissiles[j].valid == true && (incomingMissiles[j].explosionRadius > 0) && 
              ((plane.currentX - (float)incomingMissiles[j].stopX)*(plane.currentX - (float)incomingMissiles[j].stopX)
              + (plane.startY - (float)incomingMissiles[j].stopY)*(plane.startY - (float)incomingMissiles[j].stopY)
              <= ((incomingMissiles[j].explosionRadius+1)*(incomingMissiles[j].explosionRadius+1)))){
              if (plane.scored == false && (gameState == PLAYING)){
                score += 5;
                plane.scored = true;
                plane.explosionFrame = 1;
              }
              break;
           }
        }
    
  } else if ((score >= nextPlaneUpdateScore)){
    nextPlaneUpdateScore = nextScoreForModulo(25);
    
    initBonusPlane();
  }
}

int nextScoreForModulo(int modulo){
  for(int i =score+1; i<score+102;i++){
    if (i % modulo == 0){
      return i; 
    }
  }
}

void updateMissiles(){
  for (int i = 0; i < MAX_MISSILES; i++){
    if (incomingMissiles[i].valid == true) {

      // handle stoping missiles at destination, explosion incrementation and halt
      if (abs(incomingMissiles[i].stopY - incomingMissiles[i].currentY) < 0.5){
        // this block is awful
        if ((incomingMissiles[i].incoming == true) && incomingMissiles[i].explosionFrame == 0){ // if incoming missile at target, or exploded by collison
          if ((incomingMissiles[i].stopX == targetXCoords[incomingMissiles[i].target]) && (incomingMissiles[i].stopY == CITY_TARGET_Y) 
              && (flashIndex[0] != incomingMissiles[i].target) && (flashIndex[1] != incomingMissiles[i].target)){ // if exploded at target
            targets[incomingMissiles[i].target] = 0;
          }
          incomingMissiles[i].explosionFrame = 1;
        } else if ((incomingMissiles[i].explosionFrame < 255) && (incomingMissiles[i].incoming == false)){ // update outgoing missile explosion
          incomingMissiles[i].explosionFrame = incomingMissiles[i].explosionFrame + 1;
          incomingMissiles[i].explosionRadius = calculateOutgoingExplosionRadiusFromFrame(incomingMissiles[i].explosionFrame);
        } else if ((incomingMissiles[i].explosionFrame < 155) && (incomingMissiles[i].incoming == true)){ // update incoming missile explosion
          incomingMissiles[i].explosionFrame = incomingMissiles[i].explosionFrame + 1;
          incomingMissiles[i].explosionRadius = calculateIncomingExplosionRadiusFromFrame(incomingMissiles[i].explosionFrame);
        } else if ((incomingMissiles[i].explosionFrame >= 155) && (incomingMissiles[i].incoming == true) ){ // terminate incoming missile explosion
          initNullMissile(i);
        } else if (incomingMissiles[i].explosionFrame >= 255){ // terminate outgoing missile explosion
          initNullMissile(i);
        }
        continue;
      }

      // handle missile/explosion collision
      if (incomingMissiles[i].incoming == true){
        for (int j = 0; j < MAX_MISSILES; j++) {
           if (incomingMissiles[j].valid == true && (incomingMissiles[j].explosionRadius > 0) && 
              ((incomingMissiles[i].currentX - (float)incomingMissiles[j].stopX)*(incomingMissiles[i].currentX - (float)incomingMissiles[j].stopX)
              + (incomingMissiles[i].currentY - (float)incomingMissiles[j].stopY)*(incomingMissiles[i].currentY - (float)incomingMissiles[j].stopY)
              <= ((incomingMissiles[j].explosionRadius+1)*(incomingMissiles[j].explosionRadius+1)))){
              incomingMissiles[i].stopX = (byte)incomingMissiles[i].currentX;
              incomingMissiles[i].stopY = max(1, (byte)incomingMissiles[i].currentY);
              if (incomingMissiles[i].scored == false && (gameState == PLAYING)){
                score++;
                incomingMissiles[i].scored = true;
              }
              break;
           }
        }
      }

      // handle missile movement update
      // TODO: this scaling factor isn't quite right, but it's good enough for now
      float speedScalingFactor = min(1, abs((((float)incomingMissiles[i].startY - (float)incomingMissiles[i].stopY) / ((float)incomingMissiles[i].startX - (float)incomingMissiles[i].stopX))));
      if ((float)incomingMissiles[i].stopY > incomingMissiles[i].currentY) {
       incomingMissiles[i].currentY = incomingMissiles[i].currentY + speedScalingFactor * incomingMissiles[i].speedY;
      } else {
        incomingMissiles[i].currentY = incomingMissiles[i].currentY - speedScalingFactor * incomingMissiles[i].speedY;
      }
      incomingMissiles[i].currentX = ((incomingMissiles[i].currentY - (float)incomingMissiles[i].startY) / (((float)incomingMissiles[i].startY - (float)incomingMissiles[i].stopY) / ((float)incomingMissiles[i].startX - (float)incomingMissiles[i].stopX))) + (float)incomingMissiles[i].startX;
    }
  }
}

void drawMissiles(){
  for (int i = 0; i < MAX_MISSILES; i++){
    if (incomingMissiles[i].valid == true){
      if ((incomingMissiles[i].incoming == false) && (incomingMissiles[i].explosionFrame == 0)){
        arduboy.drawLine(incomingMissiles[i].stopX - 1, incomingMissiles[i].stopY - 1, incomingMissiles[i].stopX + 1, incomingMissiles[i].stopY + 1, WHITE);
        arduboy.drawLine(incomingMissiles[i].stopX - 1, incomingMissiles[i].stopY + 1, incomingMissiles[i].stopX + 1, incomingMissiles[i].stopY - 1, WHITE);
      }
      
      if ((incomingMissiles[i].explosionFrame == 0) && (incomingMissiles[i].scored == false)){
        arduboy.drawLine(incomingMissiles[i].startX, incomingMissiles[i].startY, incomingMissiles[i].currentX, incomingMissiles[i].currentY, WHITE);
        if (incomingMissiles[i].incoming == true) {
          if (arduboy.everyXFrames(2) == true){
              // put some shimmer point effect here
             //arduboy.drawLine(incomingMissiles[i].startX, incomingMissiles[i].startY, incomingMissiles[i].currentX, incomingMissiles[i].currentY, BLACK);
          }
        }
      } else{
        arduboy.fillCircle(incomingMissiles[i].stopX, incomingMissiles[i].stopY, incomingMissiles[i].explosionRadius, WHITE);
        if (arduboy.everyXFrames(3) == true){ // shimmer effect
          arduboy.fillCircle(incomingMissiles[i].stopX, incomingMissiles[i].stopY, incomingMissiles[i].explosionRadius, BLACK);
        }

        //if (incomingMissiles[i].incoming == false){ // don't flash for incoming missile explosions
          if (incomingMissiles[i].explosionFrame < 5){ // flash effects
            arduboy.invert(true);
          } else if ((incomingMissiles[i].explosionFrame > 5) && (incomingMissiles[i].explosionFrame < 7)){
            arduboy.invert(false);
          }
        //}
      }
    }
  }
}

void drawScore(){
  arduboy.setCursor(0,0);
  arduboy.setTextSize(1);
  if (gameState == MENU){
    arduboy.print(String(highscore));
  } else {
    arduboy.print(String(score));
  }

  if ((targets[1] || targets[6])){ //  && gameState == PLAYING
    for (byte k=0; k < cityCount() - activePlayerMissileCount(); k++){
      arduboy.fillRect(127-(k*3) - 3, 1, 2, 5, WHITE);
    }
  }
}

void drawPlane(){
  if (plane.valid){
    if (plane.explosionFrame > 90){
      byte explosionRadius = 0;
      if (plane.explosionFrame <= 100){
        explosionRadius = 3;
      } else if(plane.explosionFrame <= 110){
        explosionRadius = 2;
      } else if(plane.explosionFrame <= 120){
        explosionRadius = 1;
      }

      arduboy.fillCircle(plane.currentX, plane.startY, explosionRadius, WHITE);
      if (arduboy.everyXFrames(3) == true){ // shimmer effect
        arduboy.fillCircle(plane.currentX, plane.startY, explosionRadius, BLACK);
      }

      
    } else {
      if (plane.dir == 1){
        arduboy.drawSlowXYBitmap(plane.currentX-4, plane.startY-3, planeBitmap1, 16, 8, WHITE);
        if ((plane.explosionFrame > 0) && (arduboy.everyXFrames(4) || arduboy.everyXFrames(5))){
          arduboy.drawSlowXYBitmap(plane.currentX-4, plane.startY-3, planeBitmap1, 16, 8, BLACK);
        }
      } else{
        arduboy.drawSlowXYBitmap(plane.currentX-5, plane.startY-3, planeBitmap0, 16, 8, WHITE);
        if ((plane.explosionFrame > 0) && (arduboy.everyXFrames(4) || arduboy.everyXFrames(5))){
          arduboy.drawSlowXYBitmap(plane.currentX-4, plane.startY-3, planeBitmap0, 16, 8, BLACK);
        }
      }
    }
  }
}

void drawCity(byte i){
  byte mainColor = WHITE;
  byte secondColor = BLACK; 
  arduboy.fillRect(i*16 + 2, 57, 12, 12, mainColor);

  // dude why didn't I just make this a loop what the hell is wrong with me
  arduboy.drawPixel(i*16 + 3, 58, secondColor);
  arduboy.drawPixel(i*16 + 5, 58, secondColor);
  arduboy.drawPixel(i*16 + 7, 58, secondColor);
  arduboy.drawPixel(i*16 + 9, 58, secondColor);
  arduboy.drawPixel(i*16 + 11, 58, secondColor);
  arduboy.drawPixel(i*16 + 13, 58, secondColor);
  arduboy.drawPixel(i*16 + 15, 58, secondColor);

  arduboy.drawPixel(i*16 + 3, 60, secondColor);
  arduboy.drawPixel(i*16 + 5, 60, secondColor);
  arduboy.drawPixel(i*16 + 7, 60, secondColor);
  arduboy.drawPixel(i*16 + 9, 60, secondColor);
  arduboy.drawPixel(i*16 + 11, 60, secondColor);
  arduboy.drawPixel(i*16 + 13, 60, secondColor);
  arduboy.drawPixel(i*16 + 15, 60, secondColor);

  arduboy.drawPixel(i*16 + 3, 62, secondColor);
  arduboy.drawPixel(i*16 + 5, 62, secondColor);
  arduboy.drawPixel(i*16 + 7, 62, secondColor);
  arduboy.drawPixel(i*16 + 9, 62, secondColor);
  arduboy.drawPixel(i*16 + 11, 62, secondColor);
  arduboy.drawPixel(i*16 + 13, 62, secondColor);
  arduboy.drawPixel(i*16 + 15, 62, secondColor);
}

void drawCities(){
  for (int i = 0; i < 8; i++){
    byte mainColor = WHITE;
    byte secondColor = BLACK; 
    
    if ((i != 1) && (i != 6) && (targets[i] == true)){
      if ((flashIndex[0] == i) && (flashCounter[0] > 0)){
        if (((flashCounter[0] / 4) % 2) == 0){
          arduboy.fillRect(i*16 + 2, 57, 12, 12, secondColor);
        } else {
          drawCity(i);
        }

        if (flashCounter[0] < 255){
          flashCounter[0]++;
        } else{
          flashCounter[0] = 0;
          flashIndex[0] = 255;
        }
      } else {
        drawCity(i);
      }
    } else if (targets[i] == true){
      if ((flashIndex[1] == i) && (flashCounter[1] > 0)){
        if (((flashCounter[1] / 4) % 2) == 0){
          arduboy.drawSlowXYBitmap(i*16, 56, turret, 16, 8, BLACK);
        } else {
          arduboy.drawSlowXYBitmap(i*16, 56, turret, 16, 8, WHITE);
        }

        if (flashCounter[1] < 255){
          flashCounter[1]++;
        } else{
          flashCounter[1] = 0;
          flashIndex[1] = 255;
        }
      } else {
        arduboy.drawSlowXYBitmap(i*16, 56, turret, 16, 8, WHITE);
      }
    } else {
      randomRubble(i);
    }
  }
  arduboy.drawLine(0, 63, 127, 63, WHITE);
}

void randomRubble(byte targetPosition){
  for(int k=1; k< 15; k++){
    if ((targetRubble[targetPosition] >> k) & 1){
      arduboy.drawPixel(targetPosition*16+k,62, WHITE); 
    }

    if ((targetRubbleSecondLayer[targetPosition] >> k) & 1){
      arduboy.drawPixel(targetPosition*16+k,61, WHITE); 
    }
  }
}

void fireMissile(bool turret){
  byte startX = 0;
  if (turret == 0){
    startX = 24;
  } else {
    startX = 103;
  }
  Missile playerMissile;
  playerMissile.startX = startX;
  playerMissile.startY = 58;
  playerMissile.stopX = cursorX;
  playerMissile.stopY = cursorY;
  playerMissile.speedY = PLAYER_MISSILE_SPEED;
  playerMissile.currentX = playerMissile.startX;
  playerMissile.currentY = playerMissile.startY;
  playerMissile.incoming = false;
  playerMissile.valid  = true;
  playerMissile.explosionFrame = 0;
  playerMissile.explosionRadius = 0;

  for(int i=MAX_INCOMING; i<(MAX_INCOMING + cityCount()); i++){
    if (incomingMissiles[i].valid == false) {
      incomingMissiles[i] = playerMissile;
      break;
    }
  }
}

byte activePlayerMissileCount(){
  byte count = 0;
  for(int i=MAX_INCOMING; i<(MAX_INCOMING + cityCount()); i++){
    if (incomingMissiles[i].valid == true) {
      count++;
    }
  }
  return count;
}

void initRandomness(){
  arduboy.initRandomSeed();
  for(int i=0;i<8;i++){
    targetRubble[i] = random(0, 65535);
    targetRubbleSecondLayer[i] = random(0, 65535) & targetRubble[i];
  }
}

bool rightButtonMenuLock = false;
bool leftButtonMenuLock = false;
void menuHandler(){
  if (arduboy.pressed(A_BUTTON) || arduboy.pressed(B_BUTTON)){
    if (transitionLock == false){
      gameState = PLAYING;
      initRandomness();
    }
  } else {
    transitionLock = false;
  }
}

void clearHighscore(){
  EEPROM.put(420, 0);
  highscore = 0;
}

void updateDifficulty(){
  if (score >= nextDifficultyUpdateScore){
    if (missileSpawnFrameDelay >= 70){
      missileSpawnFrameDelay -= 5;
    } else if (missileSpawnFrameDelay >= 40){
      missileSpawnFrameDelay -= 2;
    }
    nextDifficultyUpdateScore = nextScoreForModulo(10);
    missileSpawnFrameDelay = max(MINIMUM_FRAME_SPAWN_DELAY, missileSpawnFrameDelay);
  }
  
}

void updateLives(){
  if (pendingLoss) {
    return;
  }

  if (score >= nextLivesUpdateScore){
    byte cityAdded = false;
    for(int k = 0; k < 8; k++){
      if((k != 1) && (k != 6) && (targets[k] == false) && !cityAdded){
        targets[k] = true;
        cityAdded = true;
        flashIndex[0] = k;
        flashCounter[0] = 1;
      } else if (((k == 1) || (k == 6)) && (targets[k] == false)){
        targets[k] = true;
        flashIndex[1] = k;
        flashCounter[1] = 1;
      }
    }
    nextLivesUpdateScore = nextScoreForModulo(FREE_LIFE_SCORE_MODULO);
  }
}

void resetGame(){
  for(int i = 0; i < 8; i++){
    targets[i] = 1;
  }
  for(int i = 0; i < MAX_MISSILES; i++){
    incomingMissiles[i].valid = false;
  }
  score = 0;
  aFired = true;
  bFired = true;
  pendingLoss = false;
  lossCounter = 0;
  cursorX = 63;
  cursorY = 26;
  missileSpawnFrameDelay = MISSILE_FRAME_DELAY;
  nextDifficultyUpdateScore = 10;
  nextLivesUpdateScore = 100;
  nextPlaneUpdateScore = 25;
  flashCounter[0] = 0;
  flashCounter[1] = 0;
  flashIndex[0] = 255;
  flashIndex[1] = 255;
}

void drawMenu(){
  arduboy.setCursor(6,19);
  arduboy.setTextSize(2);
  arduboy.print(F("NIGHT RAID"));

  byte menuDrawOffset = 29;
  
  if (menuIndex == 0 && arduboy.everyXFrames(2)){
    arduboy.drawRect(menuDrawOffset,36,69,11,WHITE);
  }
  arduboy.setTextSize(1);
  arduboy.setCursor(menuDrawOffset+2,38);
  arduboy.print(F("PRESS START"));
}

int calculateOutgoingExplosionRadiusFromFrame(byte frame){
  if (frame < 5){
    return 1;
  } else if (frame < 10){
    return 2;
  } else if (frame < 20){
    return 3;
  } else if (frame < 35){
    return 4;
  } else if (frame < 200){
    return 5;
  } else if (frame < 220){
    return 4;
  } else if (frame < 235){
    return 3;
  } else if (frame < 245){
    return 2;
  } else if (frame <= 255){
    return 1;
  } else {
    return 1;
  }
}

int calculateIncomingExplosionRadiusFromFrame(byte frame){
  if (frame < 5){
    return 1;
  } else if (frame < 10){
    return 2;
  } else if (frame < 20){
    return 3;
  } else if (frame < 120){
    return 4;
  } else if (frame < 135){
    return 3;
  } else if (frame < 145){
    return 2;
  } else if (frame <= 155){
    return 1;
  } else {
    return 1;
  }
}

int cityCount(){ 
  // i'm sorry for this heinous crime against loops everywhere
  return targets[0] + targets[2] + targets[3] + targets[4] + targets[5] + targets[7];
}

void drawGameover(){
  arduboy.setCursor(25,21);
  arduboy.setTextSize(2);
  arduboy.print(F("THE END"));
}

void drawPaused(){
  arduboy.setCursor(29,21);
  arduboy.setTextSize(2);
  arduboy.print(F("PAUSED"));
}

void gameoverHandler(){
  if (arduboy.pressed(A_BUTTON) || arduboy.pressed(B_BUTTON)){
    gameState = MENU;
    resetGame();
    transitionLock = true;
  }
}

int activeIncomingCount(){
  byte count = 0;
  for (int i = 0; i < MAX_INCOMING; i++){
    count += incomingMissiles[i].valid;
  }
  return count;
}

void updateHighscore(){
  if (score > highscore){
    EEPROM.put(420, score);
    highscore = score;
  }
}

void checkForLoss(){
  if (cityCount() == 0){
    pendingLoss = true;
  } else if ((targets[1] + targets[6]) == 0){
    pendingLoss = true;
  }

  if (pendingLoss == true){
    lossCounter++;
  }

  if (lossCounter >= 240){
    updateHighscore();
    gameState = GAMEOVER;
  }
}

void pauseHandler(){
  if (pauseLock && !arduboy.pressed(A_BUTTON) && !arduboy.pressed(B_BUTTON)) {
    pauseLock = false;
  }
  if (!pauseLock && (arduboy.pressed(A_BUTTON) || arduboy.pressed(B_BUTTON))){
    bFired = true;
    aFired = true;
    gameState = PLAYING;
  }
}

