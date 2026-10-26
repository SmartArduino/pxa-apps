#define MaxParticles 10

byte ParticleX[MaxParticles];
byte ParticleY[MaxParticles];
unsigned char ParticleMoveX[MaxParticles];
unsigned char ParticleMoveY[MaxParticles];
byte ParticleType[MaxParticles];

void RunParticleCode() {
  for (byte t = 0; t < MaxParticles; t++) {
    if (ParticleY[t]) {
      if (!ParticleType[t]) {
          ard.drawPixel(ParticleX[t], ParticleY[t], BLACK);
          if (MainScreenMove) {
            ParticleX[t]--;
          }
          ParticleX[t] += ParticleMoveX[t];
          ParticleY[t] += ParticleMoveY[t];
          ParticleMoveY[t]++;
          if ((ParticleMoveY[t] == 5) || (ParticleY[t] <= 2)) {
            ParticleY[t] = 0;
          }
          else {
            ard.drawPixel(ParticleX[t], ParticleY[t], WHITE);
          }
      }
      if (ParticleType[t] == 1) {
        byte LX = ParticleX[t];
        byte LY = ParticleY[t];
        LINE(LX, LY + 2, LX, LY - 2, 0);
        LINE(LX + 2, LY, LX - 2, LY, 0);
        ParticleMoveY[t]--;
        if (!ParticleMoveY[t]) {
          ParticleY[t] = 0;
        }
        else {
          if (MainScreenMove) {
            ParticleX[t]--;
          }
          
          LINE(LX, LY + 2, LX, LY - 2, 1);
          LINE(LX + 2, LY, LX - 2, LY, 1);
        }
      }
      
      //Smoke particles are unused since the original background code isn't used.
      //No point in keeping it, it just unnecessarily takes up flash memory.
      /*
      if (ParticleType[t] == 2) {
        ard.drawPixel(ParticleX[t], ParticleY[t], BLACK);
        if ((AnimationTick == 9) || (AnimationTick == 24)) {
          ParticleY[t]--;
          ParticleMoveY[t]++;
        }
        if (ParticleMoveY[t] > 10) {
          ParticleX[t] = 0;
          ParticleY[t] = 0;
        }
        else {
          ard.drawPixel(ParticleX[t], ParticleY[t], WHITE);
        }
      }
      */
    }
  }
}

void SpawnParticle(byte tYpe, byte Xpos, byte Ypos, unsigned char MoveX) {
  byte I = 0;
  while ((I < MaxParticles) && ParticleY[I]) { //Note to self: Game will crash if program tries to read a variable in an array that is bigger than its initial size.
    I++;                                       //That's why the game kept crashing when many aliens were killed at once. Too many burst particles.
  }
  if (I >= MaxParticles) {
    return;
  }
  ParticleType[I] = tYpe;
  ParticleX[I] = Xpos;
  ParticleY[I] = Ypos;
  ParticleMoveX[I] = MoveX;
  if (tYpe) {
    ParticleMoveY[I] = 25;
  }
  else {
    ParticleMoveY[I] = -3;
  }
  /*
  else if (tYpe == 2) {
    ParticleMoveY[I] = 0;
  }
  */
  
}

void Blast(byte Xpos, byte Ypos) {
  /*
  SpawnParticle(0, Xpos + 4, Ypos + 3, -2);
  SpawnParticle(0, Xpos + 5, Ypos + 1, -1);
  SpawnParticle(0, Xpos + 3, Ypos + 1, 1);
  SpawnParticle(0, Xpos + 6, Ypos + 3, 2);
  */
  for (byte i = 2; i < 7; i++) {
    SpawnParticle(0, Xpos + i, Ypos + 3, i - 4);
  }
}
