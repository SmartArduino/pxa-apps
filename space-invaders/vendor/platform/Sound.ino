


boolean MELODY(String Original, byte Duration) {
  SOUND(2, ReturnTone(Original.charAt(MusicPlace++)), Duration);
  return (MusicPlace >= (Original.length()));
}

port::Task GameOverMelody() {
  for (MusicPlace = 0; MusicPlace < 5; MusicPlace) {
    MELODY(F("^^(^A"), 5);
    co_await port::Delay{(uint32_t)(400), __LINE__};
  }

  co_return;
}

//Notes from the D-minor scale (and a few non-scale notes for the melody of the barriers being restored).
//Chars are converted to notes, used by the MELODY() function.
//Only notes from the D-minor scale is used because the music consists of these notes and having a collection of every note for a few
//octaves would take up more flash.
unsigned int ReturnTone(char InputChar) {
  volatile int TheNote = 0;
  switch (InputChar) {
    case 'r':
      TheNote = 147;
      break;
    case 'u':
      TheNote = 175;
      break;
    case 'y':
      TheNote = 220;
      break;
    case ']':
      TheNote = 294;
      break;
    case ';':
      TheNote = 311;
      break;
    case '#':
      TheNote = 349;
      break;
    case '!':
      TheNote = 440;
      break;
    case '$':
      TheNote = 523;
      break;
    case '^':
      TheNote = 587;
      break;
    case '(':
      TheNote = 698;
      break;
    case 'A':
      TheNote = 784;
      break;
    case 'C':
      TheNote = 880;
      break;
    case 'H':
      TheNote = 1175;
      break;
    case ')':
      TheNote = 740;
      break;
    case '*':
      TheNote = 659;
      break;
  }
  return TheNote;
}

void SOUND(byte Type, unsigned int Frequency, byte Duration) {
  if ((AudioOn && (Type == 1) && !TitleScreenMove) || (MusicOn && (Type == 2))) {
    musicAndSound.tone(Frequency, Duration * 33);
  }
}

void PlayMusic(byte track) {
  if (MusicOn) {
  if (!Death && (Invulnerable < 30)) {
// PXA: mutable local state lifted to resettable sketch globals.
// PXA: mutable local state lifted to resettable sketch globals.
  //static boolean Repeat3 = 1;
  MusicInterval--;
  if (!MegaShot && !Freeze) {
  if (track == 1) {
    if (!MusicInterval) {
      MusicInterval = 5;

      //I thought this code was a flash-saver. Guess I was wrong...
      /*
    if (Repeat3) {
      if (!Repeat2) {
        MELODY(F("r]   "), MusicPlace, 3);
      }
      else {
        if (Repeat1 == 3) {
          MELODY(F("r;#  "), MusicPlace, 3);
        }
        else {
          MELODY(F("r;   "), MusicPlace, 3);
        }
      }
      MusicPlace++;
      if (MusicPlace >= 4) {
        Repeat1++;
        if (Repeat1 >= 4) {
          Repeat1 = 0;
          Repeat2++;
          if (Repeat2 >= 2) {
            Repeat2 = 0;
            if (EnemyCount >= 9) {
              Repeat3 = 0;
            }
          }
        }
        MusicPlace = 0;
      }
      }
      */
      if (Repeat2) {
        if (MELODY(F("r]  r]  r;  r;  "), 3)) { //r]  r]  r]  r]  r;  r;  r;  r;#  
          MusicPlace = 0;
          if (EnemyCount >= 9) {
            Repeat2 = 0;
          }
        }
      }
      else {
        if (MELODY(F("]^$^ !$^( (^ !^$"), 3)) { //"]^$^ !$^( (^ !^$^( ^! "
          MusicPlace = 0;
          if (EnemyCount < 9) {
            Repeat2 = 1;
          }
        }
      }
    }
  }

  if (track == 4) {
    if (!MusicInterval) {
      MusicInterval = 4;
      if (StartMove) {
        if (MELODY(F("^ ! ; ] "), 2)) {
          MusicPlace = 0;
        }
      }
      else {
        if (EnemyCount > 7 || !Repeat1) {
          if (MELODY(F("]^!#]#]!^^^^(^(A"), 4)) {
            MusicPlace = 0;
            if ((EnemyCount < 8) && !Repeat1) {
              Repeat1 = 1;
            }
            else {
              Repeat1 = 0;
            }
          }
        }
        else {
          if (MELODY(F("] ] ]  y]y"), 4)) {
            MusicPlace = 0;
          }
        }
      }
      
    }
  }

  if (track == 2) {
    if (!MusicInterval) {
      MusicInterval = 5;
      if (UFOPoints > 100) {
        MusicInterval = 4;
      }
      byte MusSpeed = MusicInterval;
      if (!Repeat1) {
        if (MELODY(F("]#!^(CHC(^!#"), MusSpeed)) {
          Repeat2++;
          MusicPlace = 0;
        }
      }
      else {
        if (UFOPoints >= 200) {
          if (MELODY(F("^ ^ ( ^A A A( ^!"), 4)) {
            MusicPlace = 0;
            Repeat2++;
          }
        }
        else {
          if (MELODY(F("^ ^ !^ ^( ^( A"), MusSpeed)) {
            MusicPlace = 0;
            Repeat2++;
          }
        }
      }
      if (Repeat2 >= 2) {
        Repeat1++;
        if (Repeat1 >= 2) {
          Repeat1 = 0;
        }
        Repeat2 = 0;
      }
    }
    
  }
  }
  else {
    if (!MusicInterval) {
      MusicInterval = 3;
      if (MELODY(F("] ^ ^  ]^#"), 3)) {
        MusicPlace = 0;
      }
    }
  }
  }
  else if (track == 3) {
    MusicInterval--;
  }
  if (track == 3) {
    if (!MusicInterval) {
      if (MELODY(F("r ] ]  r]u"), 3)) {
        MusicPlace = 0;
      }
      MusicInterval = 5;
    }
  }
  }
}


