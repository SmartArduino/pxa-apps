/*
    Planetary Intruders
    by Téo Taylor
    A remix of 'Space Invaders'.

    If you'd like to modify the game yourself, here's what you need to know:
    ✧ The sketch (when compiled) states that it takes about 1,820 bytes of RAM, but static local variables
      in the program use up around an additional 16 bytes, and local variables, particularly used in strings,
      require about 50 bytes of RAM, so in total you should leave at least 66 bytes of available RAM so that
      errors do not occur in the program.

    ✧ If you are having RAM issues in the program (not enough RAM), try reducing the value of MaxEnemies,
      MaxPlayerLasers, MaxEnemyLasers, MaxParticles to save RAM. Don't worry; if the program exceeds the maximum
      number of Enemies/Lasers/Particles, the entity will not be spawned, thus the program will not crash. However,
      this may affect gameplay (e.g. less enemies will be spawned than intended).

    ✧ But... why am I saying all this anyway?? I think it's pretty unlikely that you'll run out of RAM. 740
      available bytes is quite a lot for an Arduboy.

    ✧ I don't mind if you reupload this game with your changes in it, but please put credits as comments in this
      sketch and in the game. Please don't remove any parts of the 'Info' section in this game.

    ✧ I am not that experienced at coding for Arduboy, so if you find something uneffective, like code that runs slowly,
      or something that uses up more flash than it should, please let me know on the Arduboy website! Especially if it's
      flash memory. Even a few bytes counts!


    Thank you for reading this. I hope you enjoy this game!

*/

#include <EEPROM.h>
#include <Arduboy2.h>
#include <ArduboyTones.h>


//Feel free to change the value of those, but not to something too low.
//Barely anything will spawn if it's only at something like 2.
#define MaxEnemies 50
#define MaxPlayerLasers 10
#define MaxEnemyLasers 25

//I was originally going to add features to the game where it made the player's hitbox bigger/smaller.
//But because of the lack of flash memory, I was never able to add it. So sad...
#define PlayerHitbox 8

//I can't be bothered to type up those button press detection lines over and over, so it's defined as a shorter line of text.
#define UpButton ard.pressed(UP_BUTTON)
#define DownButton ard.pressed(DOWN_BUTTON)
#define LeftButton ard.pressed(LEFT_BUTTON)
#define RightButton ard.pressed(RIGHT_BUTTON)
#define BButton ard.pressed(A_BUTTON)        //'B' and 'A' buttons are switched round. 
#define AButton ard.pressed(B_BUTTON)

#define Display ard.display()                 //'Display' has to start with capital 'D' because a lower-case 'd' would not compile.
#define clearDisplay ard.display(CLEAR_BUFFER)


    Arduboy2 ard;
    ArduboyTones musicAndSound(ard.audio.enabled);

    //This game was originally going to be called "SkyAttackers", because aliens fly down from the
    //sky on planet Earth. However... Sky Attackers... sounds more like a game where you avoid getting
    //your sandwich stolen by seagulls. Plus friends told me just to go for something that means the
    //same as "Space Invaders". So I looked on an online thesaurus for words that meant the same as
    //'space' and 'invaders'. And that was the story of how I got the totally creative and original
    //name for this game!
    const char NameOfThisGame[] = "Planetary Intruders";


    //The UFO variables.
    byte UFOPoints;      //How much points the UFO gained from aliens. If it's over 200, the UFO will move all around shooting at a fast pace.
    int UFOHP;
    byte UFOX;
    byte UFOY;
    boolean UFOHasBombs; //If this is true, the UFO will shoot down bombs instead of lasers, set to true by obtaining a Bomber. What a nightmare!
    byte UFOAliensAppear = 0;
    

    boolean AudioOn = 1;
    boolean MusicOn = 1;

    boolean IsItCustomGame = 0;   //If IsItCustomGame is true, the game does what you'd expect: end the game if the player wins, don't display lives, ect.
    boolean CustomGameFinish = 0;
    
    
    byte AnimationTick = 0;  //AnimationTick constantly counts up, and when it reaches 30, it resets to zero. Used for the animation of the aliens
                             //as well as half-a-second timings.
                             

    byte BarrierHP[4];

    byte GameType = 10;      //Titlescreen GameType is 10, just because. Can't have it on any value like 1, 2, or 3 while on the titlescreen.
    
    byte EnemyX[MaxEnemies];
    byte EnemyY[MaxEnemies];
    byte EnemyDirection[MaxEnemies];
    byte EnemyType[MaxEnemies];
    byte EnemyMoveInterval[MaxEnemies];
    byte EnemyShootInterval[MaxEnemies];  //Enemy Move and Shoot Interval is a timer for each alien that counts down. When it reaches zero, the timer resets
                                          //the alien changes direction/shoots a laser.
    byte MoveSlow = 0;   //A boolean which flickers from 0 to 1 to 0 ect. every frame to determine slow speeds, e.g. move 15 pixels in 30 frames.

    //Appearing enemy variables. These are used for the animation of the aliens moving into the screen from the edge.
    byte AppearingEnemyType;
    int AppearingEnemyX;
    int AppearingEnemyY;

    //Used for the little flashing warning sign in UFO game type.
    boolean IncomingLeftOrRight = 1;
    byte IncomingY = 20;

    
    byte LaserX[MaxEnemyLasers];
    byte LaserY[MaxEnemyLasers];
    byte LaserBomb[MaxEnemyLasers]; //LaserBomb should actually be 'boolean' type, but because of the flash memory usage in coverting
                                    //'boolean' to 'byte', I sacrificed some RAM to save flash.

    byte Death = 0;          //A variable which counts down, creating the death sound and being a timer to how long the player is in 'death' state.
    byte Invulnerable = 0;   //Counts down, when not zero (true), the player is invulnerable.
    byte MusicPlace = 0;     //A counter for keeping track of the char place in a string for music in the game.
    byte MusicInterval = 5;  //A very short timer which makes each not have a small delay. Without this variable, one note would play every frame in the music,
                             //making the music WAYYY too fast.
    byte Flash = 0;          //A quick short countdown timer for flashing the player while invincible.
    boolean MegaShot = 0;    //The Mega Shot power up! This is set to true upon collecting the Mega Shot power up.
    byte MegaShotCharge = 0; //A timer for counting up how long the player holds down the shoot button for the Mega Shot power up.
    byte Freeze = 0;         //A timer for counting down how long the enemies are frozen for after the player has collected the 'freeze' power up.

    byte BonusAlienX = 0;
    byte PowerUpX;
    byte PowerUpY = 0;
    byte PowerUpType = 1;    //Set to FixBarrier power up by default. Because random() is timing-based (I think), there's no point in initializing it to a random value.
    unsigned int BonusAlienInterval = 120;   //A countdown timer for determining how long it is until the bonus alien appears.
    byte InvulBonus = 1;             //Used when the bonus alien holds the invinciblility power up. When above 20, lasers that hit the alien are reflected.
    boolean BonusTwoHits = 0;        //Used when the bonus alien holds the 1UP power up. Gives the alien two hitpoints.
    
    byte StartMove = 0;   //A variable for game type 1, where the aliens move together in one big block.
    
    byte EnemyCount = 0;     //Determines how many aliens are remaining. When an alien is destroyed, this decreases by one.
    byte Level = 1;          //The higher this is, the harder the game gets.
    byte CurrentEnemies = 0; //Used in game type 2, to keep track of how many aliens are to come down from above.
    byte EnemiesToSpawn = 0;

    byte Lives = 3;
    int32_t Score = 0;

    byte PlayerLaserX[MaxPlayerLasers];
    byte PlayerLaserY[MaxPlayerLasers];
    byte PlayerX = 60;
    byte ShootDelay = 0;

    byte MainScreenMove;       //Another 'TitleScreenMove' function that flickers to 1 to 0 when 'TitleScreenMove' is true.
    byte TitleScreenMove = 1;  //If this variable is true, then it means a game is not on and the user is on the title screen.
    byte BackgroundOffset = 0;
    byte Menu = 1;
    byte Select = 0;  //Menu and Select are used in the title screen.
    boolean ButtonHold = 0;  //This is used to wait until the player lets go of the button.

    boolean ShowBackground = 1;  //If this it true, the background is rendered. Otherwise, it is not.

    byte NewType[3];  //An array for storing the three types of aliens spawned on game type 1, used in saving the game.

    byte ExplosionSound = 0;  //When set to a value, it counts down while playing a pitch based on the value, sounding like an explosion.



    

void ResetVariables() {
  PlayerX = 60; Lives = 3; Level = 1; CurrentEnemies = 0; EnemyCount = 0; Score = 0; BonusAlienX = 0; BonusAlienInterval = 120; Death = 0; Invulnerable = 0;
  BackgroundOffset = 0; AppearingEnemyX = 0; AppearingEnemyY = 0; AppearingEnemyType = 0; IncomingY = 20; StartMove = 1; GameType = 1; MegaShot = 0;
  for (byte J = 0; J < MaxEnemies; J++) {
    EnemyType[J] = 0;
  }
  for (byte J = 0; J < MaxPlayerLasers; J++) {
    PlayerLaserY[J] = 0;
  }
  for (byte J = 0; J < MaxEnemyLasers; J++) {
    LaserY[J] = 0;
  }
  for (byte J = 0; J < 4; J++) {
    BarrierHP[J] = 0;
  }
}




    


port::Task setup() {
  ard.begin();
  ard.setFrameRate(30);
  ard.audio.on();
  clearDisplay;
  if (BButton) {
    AudioOn = 0;
    MusicOn = 0;
  }

  if (!(EEPROM.read(34) == 186)) {   //The secret value to verify that the data in the EEPROM is valid is 186. Just a random number I came up with.
    EEPROM.update(34, 186);          //Chances are, data left by the last game on an Arduboy before this game is not specifically 186.
    EraseSaveProgress();             //If the value is not 186, erase the junk data on EEPROM and set address 34 to the verify value, 186.
  }
    
  DrawBackground();
  Render(PlayerX, 55, 0, WHITE);

  TEXT(NameOfThisGame, 5, 32, 1, 1);
  Display;
  co_await GameOverMelody();
  SOLIDRECT(1, 30, 128, 46, BLACK);
  Display;
  AppearingEnemyType = 1;

  co_return;
}

port::Task loop() {


  if (TitleScreenMove) {
    if ((AnimationTick == 15) && (EnemyCount < 10)) {
      co_await SpawnEnemy(130, 20, random(1, 5), 0);
    }
    PlayMusic(3);
  }

  if (ExplosionSound) {
    SOUND(1, ExplosionSound * 14, 1);
    ExplosionSound--;
  }

  DrawBackground();
  SlowControl();
  co_await RunPlayerCode();
  co_await RunEnemyCode();
  co_await RunBonusAlienCode();
  EnemysLaser();
  if (GameType == 3) {
    co_await RunUFOCode();
  }
  DrawBarriers();
  RunParticleCode();
  co_await DisplayInfo(WHITE);
  //RunSoundTimers();
  
  Display;
  while (!(ard.nextFrame())) { co_await port::Delay{1, __LINE__}; }

  ClearEnemiesOnScreen();
  co_await DisplayInfo(BLACK);

  AnimationTick++;
  if (AnimationTick > 30) {
    AnimationTick = 0;
  }

  co_return;
}
