#include <Tinyfont.h>
#include <Arduboy2.h>

Arduboy2 arduboy;
Tinyfont tinyfont = Tinyfont(arduboy.sBuffer, Arduboy2::width(), Arduboy2::height());



byte screen = 2;
int shipX[] = {0, 2, 4, 6, 8, 4, 0, -4, -8, -6, -4, -2, 0};
int shipY[] = {-11, -6, -8, 0, 4, 7, 5, 7, 4, 0, -8, -6, -11};
float obstacleDepth = 0;
byte obstacleType = 0;
int roto = 0;
int pitch = 76;
byte xo = 64;
byte yo = 48;
float leftDepth[5] = {1, 13.8, 26.6, 39.4, 52.2};
byte lives = 3;
byte score = 0;
byte temp = 0;

void setup() {
  arduboy.begin();
  arduboy.setFrameRate(60);
  arduboy.initRandomSeed();
  arduboy.clear();
}

void loop() {
  if (!(arduboy.nextFrame())) {
    return;
  }
  
  arduboy.pollButtons();
  arduboy.clear();
  
  if (screen == 0) {
    spawnObstacle();
    drawRoom();
    drawObstacle();
    drawVectorRot(xo, yo, shipX, shipY, 13, 300, pitch, roto);
    moveShip();
    collideWall();
    destroyObstacle();
    drawLives();
    killPlayer();
    if (arduboy.justPressed(A_BUTTON)) {
      temp++;
    }
  } else if (screen == 1) {
    arduboy.setCursor(35, 0);
    arduboy.print("Game Over");
    tinyfont.setCursor(45, 13);
    tinyfont.print("SCORE: ");
    tinyfont.print(score);
    drawVectorRot(xo, yo, shipX, shipY, (arduboy.frameCount%180)/13, 300, 76, arduboy.frameCount%360);
    if (arduboy.frameCount % 60 < 30) {
      arduboy.setCursor(0, 23);
      arduboy.print("-PRESS A TO CONTINUE-");
    }
    if (arduboy.justPressed(A_BUTTON)) {
      resetGame();
    }
  } else if (screen == 2) {
    arduboy.setCursor(20, 0);
    arduboy.print("ObstacleFlight");
    drawVectorRot(xo, yo - 15, shipX, shipY, 13, 300, 76, arduboy.frameCount % 360);
    tinyfont.setCursor(35, 50);
    if (arduboy.frameCount % 120 < 60) {
      tinyfont.print("-A TO START-");
    }
    if (arduboy.justPressed(A_BUTTON)) {
      screen = 0;
    }
  }
  
  arduboy.display();
}

void drawVector(int xc, int yc, int x[], int y[],byte len, int scale) {
  for (int i = 0; i < len-2; i++) {
    arduboy.drawLine(xc + (x[i] * scale/100), yc + (y[i] * scale/100), xc + (x[i+1] * scale/100), yc + (y[i+1] * scale/100), WHITE);
  }
}

byte getDistance(int x1, int y1, int x2, int y2) {
  int dx = max(x1, x2) - min(x1, x2);
  int dy = max(y1, y2) - min(y1, y2);

  return sqrt(sq(dx) + sq(dy));
}

int getAngle(int x1, int y1, int x2, int y2) {
  int angle = (atan2(y1 - y2, x1 - x2) * 180 / 3.14159265359f);
  angle = (((-(angle + 180)) % 360)+ 360)% 360;
  return angle;
}

void drawVectorRot(int xc, int yc, int x[], int y[],byte len, int xscale, int yscale, int rot) {
  for (int i = 0; i <= len-2; i++) {
    int leni = getDistance(0, 0, x[i], y[i]);
    int diri = getAngle(0, 0, x[i], y[i]);

    int lenii = getDistance(0, 0, x[i+1], y[i+1]);
    int dirii = getAngle(0, 0, x[i+1], y[i+1]);

    drawLineInv(xc + lendirX(leni * xscale/100, diri + rot), yc + lendirY(leni * yscale/100, diri + rot), xc + lendirX(lenii * xscale/100, dirii + rot), yc + lendirY(lenii * yscale/100, dirii + rot));
  }
}

void moveShip() {
    if (arduboy.pressed(LEFT_BUTTON)) {
      if (roto < 32) {
        roto += 2;
      }
      if (xo > 0) {
        xo -= 2;
      }
    } else {
      if (roto > 0) {
        roto -= 4;
      }
      //if (xo < 64) {
      //  xo++;
      //}
    }

    if (arduboy.pressed(RIGHT_BUTTON)) {
      if (roto > -32) {
        roto -= 2;
      }
      if (xo < 127) {
        xo += 2;
      }
    } else {
      if (roto < 0) {
        roto += 4;
      }
      //if (xo > 64) {
      //  xo--;
      //}
    }

    if (arduboy.pressed(UP_BUTTON)) {
      if (pitch < 172) {
        pitch += 6;
      }
      if (yo > 0) {
        yo -= 2;
      }
    } else {
      if (pitch > 76) {
        if (pitch - 12 > 76) {
          pitch -= 12;
        } else {
          pitch = 76;
        }
      }
      //if (yo < 48) {
      //  yo++;
      //}
    }

    if (arduboy.pressed(DOWN_BUTTON)) {
      if (pitch > -52) {
        pitch -= 8;
      }
      if (yo < 58) {
        yo += 2;
      }
    } else {
      if (pitch < 76) {
        if (pitch + 16 < 76) {
          pitch += 16;
        } else {
          pitch = 76;
        }
      }
      //if (yo > 48) {
      //  yo--;
      //}
    }
}

void drawRoom() {
  for (int i = 0; i < 5; i++) {
    arduboy.drawFastVLine(64 - leftDepth[i], 0, 64, WHITE);
    arduboy.drawFastVLine(64 + leftDepth[i], 0, 64, WHITE);
    leftDepth[i] += leftDepth[i]/(20 - score/5);
    if (leftDepth[i] > 64) {
      leftDepth[i] = 1;
    }
  }

  arduboy.fillTriangle(0, 0, 128, 0, 64, 32, BLACK);
  arduboy.fillTriangle(0, 64, 128, 64, 64, 32, BLACK);
  
  arduboy.drawLine(0, 0, 128, 64, WHITE);
  arduboy.drawLine(0, 64, 128, 0, WHITE);
}

void spawnObstacle() {
  if (obstacleType == 0) {
    obstacleType = random(1, 13);
    obstacleDepth = 1;
  }
}

void drawObstacle() {
  if (obstacleType != 0) {
    if (obstacleType == 1) {
      arduboy.fillRect(64 - obstacleDepth, 32 - obstacleDepth / 2, obstacleDepth, obstacleDepth / 2, WHITE);
    } else if (obstacleType == 2) {
      arduboy.fillRect(64 - obstacleDepth, 32, obstacleDepth, obstacleDepth / 2, WHITE);
    } else if (obstacleType == 3) {
      arduboy.fillRect(64, 32 - obstacleDepth / 2, obstacleDepth, obstacleDepth / 2, WHITE);
    } else if (obstacleType == 4) {
      arduboy.fillRect(64, 32, obstacleDepth, obstacleDepth / 2, WHITE);
    } else if (obstacleType == 5) {
      arduboy.fillRect(64 - obstacleDepth, 32 - obstacleDepth / 2, obstacleDepth, obstacleDepth / 2, WHITE);
      arduboy.fillRect(64 - obstacleDepth, 32, obstacleDepth, obstacleDepth / 2, WHITE);
    } else if (obstacleType == 6) {
      arduboy.fillRect(64 - obstacleDepth, 32, obstacleDepth, obstacleDepth / 2, WHITE);
      arduboy.fillRect(64, 32, obstacleDepth, obstacleDepth / 2, WHITE);
    } else if (obstacleType == 7) {
      arduboy.fillRect(64, 32 - obstacleDepth / 2, obstacleDepth, obstacleDepth / 2, WHITE);
      arduboy.fillRect(64, 32, obstacleDepth, obstacleDepth / 2, WHITE);
    } else if (obstacleType == 8) {
      arduboy.fillRect(64, 32 - obstacleDepth / 2, obstacleDepth, obstacleDepth / 2, WHITE);
      arduboy.fillRect(64 - obstacleDepth, 32 - obstacleDepth / 2, obstacleDepth, obstacleDepth / 2, WHITE);
    } else if (obstacleType == 9) {
      arduboy.fillRect(64 - obstacleDepth, 32 - obstacleDepth / 2, obstacleDepth, obstacleDepth / 2, WHITE);
      arduboy.fillRect(64 - obstacleDepth, 32, obstacleDepth, obstacleDepth / 2, WHITE);
      arduboy.fillRect(64, 32 - obstacleDepth / 2, obstacleDepth, obstacleDepth / 2, WHITE);
    } else if (obstacleType == 10) {
      arduboy.fillRect(64 - obstacleDepth, 32, obstacleDepth, obstacleDepth / 2, WHITE);
      arduboy.fillRect(64, 32 - obstacleDepth / 2, obstacleDepth, obstacleDepth / 2, WHITE);
      arduboy.fillRect(64, 32, obstacleDepth, obstacleDepth / 2, WHITE);
    } else if (obstacleType == 11) {
      arduboy.fillRect(64, 32 - obstacleDepth / 2, obstacleDepth, obstacleDepth / 2, WHITE);
      arduboy.fillRect(64, 32, obstacleDepth, obstacleDepth / 2, WHITE);
      arduboy.fillRect(64 - obstacleDepth, 32 - obstacleDepth / 2, obstacleDepth, obstacleDepth / 2, WHITE);
    } else if (obstacleType == 12) {
      arduboy.fillRect(64, 32, obstacleDepth, obstacleDepth / 2, WHITE);
      arduboy.fillRect(64 - obstacleDepth, 32 - obstacleDepth / 2, obstacleDepth, obstacleDepth / 2, WHITE);
      arduboy.fillRect(64 - obstacleDepth, 32, obstacleDepth, obstacleDepth / 2, WHITE);
    }
    //arduboy.fillRect(64 - obstacleDepth, 32 - obstacleDepth/2, obstacleDepth * 2, obstacleDepth, WHITE);
    obstacleDepth += obstacleDepth/(20 - score/5);
  }
}

void collideWall() {
  if (obstacleType != 0 and obstacleDepth > 63) {
    if (obstacleType == 1 and xo < 65 and yo < 33) {
      lives--;
      obstacleType = 0;
    }
    if (obstacleType == 2 and xo < 65 and yo > 31) {
      lives--;
      obstacleType = 0;
    }
    if (obstacleType == 3 and xo > 62 and yo < 33) {
      lives--;
      obstacleType = 0;
    }
    if (obstacleType == 4 and xo > 62 and yo > 31) {
      lives--;
      obstacleType = 0;
    }
    if (obstacleType == 5 and xo < 65) {
      lives--;
      obstacleType = 0;
    }
    if (obstacleType == 6 and yo > 31) {
      lives--;
      obstacleType = 0;
    }
    if (obstacleType == 7 and xo > 62) {
      lives--;
      obstacleType = 0;
    }
    if (obstacleType == 8 and yo < 33) {
      lives--;
      obstacleType = 0;
    }
    if (obstacleType == 9 and !(xo > 62 and yo > 31)) {
      lives--;
      obstacleType = 0;
    }
    if (obstacleType == 10 and !(xo < 65 and yo < 33)) {
      lives--;
      obstacleType = 0;
    }
    if (obstacleType == 11 and !(xo < 65 and yo > 31)) {
      lives--;
      obstacleType = 0;
    }
    if (obstacleType == 12 and !(xo > 52 and yo < 33)) {
      lives--;
      obstacleType = 0;
    }
  }
}

void drawLives() {
  tinyfont.setCursor(20, 0);
  tinyfont.print("LIVES:");
  tinyfont.print(lives);
  tinyfont.setCursor(70, 0);
  tinyfont.print("SCORE:");
  tinyfont.print(score);
}

void destroyObstacle() {
  if (obstacleType != 0 and obstacleDepth > 64) {
    obstacleType = 0;
    score++;
  }
}

void killPlayer() {
  if (lives < 1) {
    xo = 64;
    yo = 48;
    screen = 1;
  }
}

void resetGame() {
  screen = 0;
  obstacleDepth = 0;
  obstacleType = 0;
  roto = 0;
  pitch = 76;
  xo = 64;
  yo = 48;
  leftDepth[0] = 1;
  leftDepth[1] = 13.8;
  leftDepth[2] = 26.6;
  leftDepth[3] = 39.4;
  leftDepth[4] = 52.2;
  lives = 3;
  score = 0;
}

int lendirX(int len, int dir) {
  dir = ((dir % 360) + 360) % 360;
  dir = dir - 360;
  
  if (dir < 90) {
    return (cos(dir * (3.14159265359f / 180.0f)) * len);
  }
  if (dir > 90 and dir < 180) {
    return -(cos(dir * (3.14159265359f / 180.0f)) * len);
  }
  if (dir > 180 and dir < 270) {
    return -(cos(dir * (3.14159265359f / 180.0f)) * len);
  }
  if (dir > 270) {
    return (cos(dir * (3.14159265359f / 180.0f)) * len);
  }
  if (dir == 0 or dir == 360) {
    return len;
  }
  if (dir == 90 or dir == 270) {
    return 0;
  }
  if (dir == 180) {
    return - len;
  }
}

int lendirY(int len, int dir) {
  dir = ((dir % 360) + 360) % 360;
  dir = dir - 360;

  if (dir < 90) {
    return -(sin(dir * (3.14159265359f / 180.0f)) * len);
  }
  if (dir > 90 and dir < 180) {
    return -(sin(dir * (3.14159265359f / 180.0f)) * len);
  }
  if (dir > 180 and dir < 270) {
    return (sin(dir * (3.14159265359f / 180.0f)) * len);
  }
  if (dir > 270) {
    return (sin(dir * (3.14159265359f / 180.0f)) * len);
  }
  if (dir == 0 or dir == 360 or dir == 180) {
    return 0;
  }
  if (dir == 90) {
    return - len;
  }
  if (dir == 270) {
    return len;
  }
}

void drawLineInv (int16_t x0, int16_t y0, int16_t x1, int16_t y1)
{
  // bresenham's algorithm - thx wikpedia
  bool steep = abs(y1 - y0) > abs(x1 - x0);
  if (steep) {
    arduboy.swap(x0, y0);
    arduboy.swap(x1, y1);
  }

  if (x0 > x1) {
    arduboy.swap(x0, x1);
    arduboy.swap(y0, y1);
  }

  int16_t dx, dy;
  dx = x1 - x0;
  dy = abs(y1 - y0);

  int16_t err = dx / 2;
  int8_t ystep;

  if (y0 < y1)
  {
    ystep = 1;
  }
  else
  {
    ystep = -1;
  }

  for (; x0 <= x1; x0++)
  {
    if (steep)
    {
      if (arduboy.getPixel(y0, x0) == WHITE) {
        arduboy.drawPixel(y0, x0, BLACK);
      } else {
        arduboy.drawPixel(y0, x0, WHITE);
      }
    }
    else
    {
      if (arduboy.getPixel(x0, y0) == WHITE) {
        arduboy.drawPixel(x0, y0, BLACK);
      } else {
        arduboy.drawPixel(x0, y0, WHITE);
      }
    }

    err -= dy;
    if (err < 0)
    {
      y0 += ystep;
      err += dx;
    }
  }
}
