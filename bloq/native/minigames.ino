/**
  Simple minigames for SAF.

  by drummyfish, released under CC0 1.0, public domain
*/

//#define SAF_SETTING_FORCE_1BIT 1

#define SAF_SETTING_BACKGROUND_COLOR 0xe0

#define SAF_PROGRAM_NAME "Minigames"

#define SAF_SETTING_FASTER_1BIT 2
#define SAF_SETTING_ENABLE_SOUND 1 
#define SAF_SETTING_ENABLE_SAVES 1
#define SAF_SETTING_BACKGROUND_COLOR 0
//#define SAF_SETTING_FORCE_1BIT 1

#include "saf.h"

#define GAMES 5 ///< number of minigames

static const char *gameNames[GAMES] =
  {
    "SNEK",
    "MINE",
    "BLOQ",
    "2048",
    "RUNR"
  };

#define MEMORY_SIZE 512
#define MEMORY_VARIABLE_AREA ((32 * 29) / 2) // = 464

#define BUTTON_HOLD_PERIOD 18

void (*stepFunction) (void);

void menuStep(void);

void quitGame(void)
{
  stepFunction = &menuStep;
  SAF_playSound(SAF_SOUND_CLICK);
}

/** Memory that's used by the games: the memory is shared between games and at
  any time at most one game is using the memory. This helps save RAM on
  platforms that don't have much of it. The memory has two parts:

  - array area (first MEMORY_VARIABLE_AREA bytes): used for 2D array data, the
    size e.g. allows to store 32 * 29 half-byte values.
  - variable area: game's global variables should be mapped into this area */
uint8_t memory[MEMORY_SIZE];

#define VAR(type, index) (*((type *) (memory + MEMORY_VARIABLE_AREA + index)))

void clearMemory()
{
  uint8_t *m = memory;

  for (uint16_t i = 0; i < MEMORY_SIZE; ++i, ++m)
    *m = 0;
}

void setMemoryHalfByte(uint16_t index, uint8_t data)
{
  uint8_t *m = memory + index / 2;
  *m = (index % 2) ? ((*m & 0xf0) | data) : ((*m & 0x0f) | (data << 4));
}

uint8_t getMemoryHalfByte(uint16_t index)
{
  return (index % 2) ? (memory[index / 2] & 0x0f) : (memory[index / 2] >> 4);
}

uint8_t buttonPressedOrHeld(uint8_t key)
{
  uint8_t b = SAF_buttonPressed(key);
  return (b == 1) || (b >= BUTTON_HOLD_PERIOD);
}

void drawTextRightAlign(int8_t x, int8_t y, const char *text, uint8_t color,
  uint8_t size)
{
  uint8_t l = 0;

  while (text[l] != 0)
    l++;

  x = x - l * 5 * size + 1;

  SAF_drawText(text,x,y,color,size);
}

void blinkHighScore()
{
  if (SAF_frame() & 0x10)
  {
    SAF_drawRect(5,20,54,16,SAF_COLOR_GRAY_DARK,1);
    SAF_drawText("HISCORE!",11,25,SAF_COLOR_GREEN,1);
  }
}

void saveHiScore(uint8_t index, uint16_t score)
{
  SAF_save(index * 2,score & 0x00ff);
  SAF_save(index * 2 + 1,score / 256);
}

uint16_t getHiScore(uint8_t index)
{
  return SAF_load(index * 2) + (((uint16_t) SAF_load(index * 2 + 1)) * 256);
}

// SNAKE -----------------------------------------------------------------------

#define SNAKE_BOARD_W 32
#define SNAKE_BOARD_H 29
#define SNAKE_START_LEN 6
#define SNAKE_MOVE_SPEED 6

#define SNAKE_COLOR1 SAF_COLOR_BLACK
#define SNAKE_COLOR2 SAF_COLOR_RED

#define SNAKE_SQUARE_EMPTY 0
#define SNAKE_SQUARE_SU    1 // snake segment pointing upwards to next segment
#define SNAKE_SQUARE_SR    2
#define SNAKE_SQUARE_SD    3
#define SNAKE_SQUARE_SL    4
#define SNAKE_SAVE_SLOT    0

// variables:
#define SNAKE_TAIL_SQUARE VAR(int16_t,0)
#define SNAKE_HEAD_SQUARE VAR(int16_t,2)
#define SNAKE_FOOD_SQUARE VAR(int16_t,4)
#define SNAKE_MOVE_COUNT VAR(uint8_t,6) // countdown to next snake move
#define SNAKE_DIRECTION VAR(uint8_t,7)
#define SNAKE_FOOD_STEPS VAR(uint8_t,8)
#define SNAKE_SCORE VAR(uint16_t,9)
#define SNAKE_GAME_STATE VAR(uint8_t,11) // 0: playing, 1: end, 2: end, hiscore

void snakeSpawnFood()
{
  int16_t square = ((((uint16_t) SAF_random()) << 8) | SAF_random()) %
    (SNAKE_BOARD_W * SNAKE_BOARD_H);

  while (getMemoryHalfByte(square) != SNAKE_SQUARE_EMPTY)
    square = ((square != 0) ? square : (SNAKE_BOARD_W * SNAKE_BOARD_H)) - 1;

  SNAKE_FOOD_SQUARE = square;
}

void snakeInit()
{
  clearMemory();

  // TODO: init snake to random colors?

  SNAKE_TAIL_SQUARE = (SNAKE_BOARD_H / 2) * SNAKE_BOARD_W + SNAKE_BOARD_W / 2
    - SNAKE_START_LEN / 2;

  SNAKE_GAME_STATE = 0;
  SNAKE_HEAD_SQUARE = SNAKE_TAIL_SQUARE + SNAKE_START_LEN - 1;
  SNAKE_DIRECTION = SNAKE_SQUARE_SR;
  SNAKE_MOVE_COUNT = SNAKE_MOVE_SPEED;
  SNAKE_FOOD_STEPS = 0;
  SNAKE_SCORE = 0;

  for (uint8_t i = 0; i < SNAKE_START_LEN; ++i)
    setMemoryHalfByte(SNAKE_TAIL_SQUARE + i,SNAKE_SQUARE_SR);

  snakeSpawnFood();
}

int16_t snakeNextSquare(int16_t square)
{
  switch (getMemoryHalfByte(square))
  {
    case SNAKE_SQUARE_SU: return square - 32; break;
    case SNAKE_SQUARE_SR: return square + 1;  break;
    case SNAKE_SQUARE_SD: return square + 32; break;
    case SNAKE_SQUARE_SL: return square - 1;  break;
    default: return square; break;
  }
}

void snakeDraw()
{
  SAF_clearScreen(SAF_COLOR_WHITE);

  SAF_drawRect(0,SAF_SCREEN_HEIGHT,SAF_SCREEN_WIDTH,-6,SAF_COLOR_GRAY_DARK,1);

  char scoreText[6];

  SAF_drawText(SAF_intToStr(SNAKE_SCORE,scoreText),2,SAF_SCREEN_HEIGHT - 5,
    SAF_COLOR_WHITE,1);

  int16_t square = SNAKE_TAIL_SQUARE;
  uint8_t val = getMemoryHalfByte(square);

#if SAF_PLATFORM_COLOR_COUNT > 2
  uint8_t pattern = 1; // pattern of next and prev square, for coloring
#endif

  while (1) // draw from tail to head
  {
    uint8_t combinedPattern = 0;

#if SAF_PLATFORM_COLOR_COUNT > 2
    switch (val)
    {
      case SNAKE_SQUARE_SU: combinedPattern = 1 | pattern; pattern = 4; break;
      case SNAKE_SQUARE_SR: combinedPattern = 2 | pattern; pattern = 8; break;
      case SNAKE_SQUARE_SD: combinedPattern = 4 | pattern; pattern = 1; break;
      case SNAKE_SQUARE_SL: combinedPattern = 8 | pattern; pattern = 2; break;
      default: break;
    }
#endif

    uint8_t x = (square % 32) * 2;
    uint8_t y = (square / 32) * 2;

#if SAF_PLATFORM_COLOR_COUNT > 2
    SAF_drawPixel(x,y,((combinedPattern & (1 | 8) ) || (combinedPattern == (2 | 4))) ? SNAKE_COLOR2 : SNAKE_COLOR1);
    SAF_drawPixel(x + 1,y,(combinedPattern & 2) ? SNAKE_COLOR2 : SNAKE_COLOR1);
    SAF_drawPixel(x,y + 1,(combinedPattern & 4) ? SNAKE_COLOR2 : SNAKE_COLOR1);
    SAF_drawPixel(x + 1,y + 1,SNAKE_COLOR1);
#else
    SAF_drawPixel(x,y,SAF_COLOR_BLACK);
    SAF_drawPixel(x + 1,y,SAF_COLOR_BLACK);
    SAF_drawPixel(x,y + 1,SAF_COLOR_BLACK);
    SAF_drawPixel(x + 1,y + 1,SAF_COLOR_BLACK);
#endif

    if (square == SNAKE_HEAD_SQUARE)
      break;

    square = snakeNextSquare(square);

#if SAF_PLATFORM_COLOR_COUNT > 2
    val = getMemoryHalfByte(square);
#endif
  }

  // draw food:
  uint8_t x = (SNAKE_FOOD_SQUARE % 32) * 2;
  uint8_t y = (SNAKE_FOOD_SQUARE / 32) * 2;

  SAF_drawPixel(x,y,SAF_COLOR_BLUE);
  SAF_drawPixel(x + 1,y,SAF_COLOR_BLUE);
  SAF_drawPixel(x,y + 1,SAF_COLOR_BLUE);
  SAF_drawPixel(x + 1,y + 1,SAF_COLOR_BLUE);

#if SAF_PLATFORM_COLOR_COUNT <= 2
  SAF_drawRect(x - 1,y - 1,4,4,SAF_COLOR_WHITE,0);
#endif

  if (SNAKE_GAME_STATE == 2)
    blinkHighScore();
}

void snakeStep()
{
  if (SAF_buttonPressed(SAF_BUTTON_B) >= BUTTON_HOLD_PERIOD)
    quitGame();

  if (SNAKE_GAME_STATE != 0)
  {
    if (SAF_buttonJustPressed(SAF_BUTTON_A))
      snakeInit();

    snakeDraw();

    return;
  }

  SNAKE_MOVE_COUNT--;

  uint8_t timePressed = 0;

  // we register the button pressed for shortest time (allows best control):

  #define checkButton(b,d)\
    if (SAF_buttonPressed(b) && \
      (timePressed == 0 || SAF_buttonPressed(b) < timePressed)) {\
      timePressed = SAF_buttonPressed(b);\
      SNAKE_DIRECTION = d;}

  checkButton(SAF_BUTTON_UP,SNAKE_SQUARE_SU)
  checkButton(SAF_BUTTON_RIGHT,SNAKE_SQUARE_SR)
  checkButton(SAF_BUTTON_DOWN,SNAKE_SQUARE_SD)
  checkButton(SAF_BUTTON_LEFT,SNAKE_SQUARE_SL)

  if (SNAKE_MOVE_COUNT <=
    (SAF_buttonPressed(SAF_BUTTON_A) ? 0 : (SNAKE_MOVE_SPEED * 3) / 4))
  {
    if (SNAKE_FOOD_STEPS < 255)
      SNAKE_FOOD_STEPS++;

    uint8_t headVal = getMemoryHalfByte(SNAKE_HEAD_SQUARE);

    if (SNAKE_DIRECTION != headVal)
    {
      uint8_t opposite = 0;

      switch (headVal)
      {
        case SNAKE_SQUARE_SU: opposite = SNAKE_SQUARE_SD; break; 
        case SNAKE_SQUARE_SR: opposite = SNAKE_SQUARE_SL; break; 
        case SNAKE_SQUARE_SD: opposite = SNAKE_SQUARE_SU; break; 
        case SNAKE_SQUARE_SL: opposite = SNAKE_SQUARE_SR; break; 
        default: break;
      }

      // disallow turning 180 degrees (instadeath):
      if (SNAKE_DIRECTION != opposite)
      {
        headVal = SNAKE_DIRECTION;
        setMemoryHalfByte(SNAKE_HEAD_SQUARE,headVal);
      }
    }

    int16_t nextSquare = snakeNextSquare(SNAKE_HEAD_SQUARE);

    // check collision:

    uint8_t collides = 0;

    int8_t diff = (nextSquare % 32) - (SNAKE_HEAD_SQUARE % 32);

    if ( // collision with map borders
        (diff != 0 && diff != 1 && diff != -1) ||
        ((nextSquare < 0) || (nextSquare >= (SNAKE_BOARD_W * SNAKE_BOARD_H)))
      )
      collides = 1;

    if (!collides)
    {
      uint8_t squareVal = getMemoryHalfByte(nextSquare);

      if (squareVal != SNAKE_SQUARE_EMPTY)
        collides = 1; // collision with self
    }

    if (collides)
    {
      SAF_playSound(SAF_SOUND_BOOM);
      SNAKE_GAME_STATE = 1;

      if (SNAKE_SCORE >= getHiScore(SNAKE_SAVE_SLOT))
      {
        saveHiScore(SNAKE_SAVE_SLOT,SNAKE_SCORE);

        SNAKE_GAME_STATE = 2;
      }

      return;
    }

    setMemoryHalfByte(nextSquare,headVal);
    SNAKE_HEAD_SQUARE = nextSquare;

    if (nextSquare == SNAKE_FOOD_SQUARE) // takes food?
    {
      SNAKE_SCORE += 20 - SNAKE_FOOD_STEPS / 25;
      SNAKE_FOOD_STEPS = 0;
      SAF_playSound(SAF_SOUND_CLICK);
      snakeSpawnFood();
    }
    else
    { 
      nextSquare = snakeNextSquare(SNAKE_TAIL_SQUARE);
      setMemoryHalfByte(SNAKE_TAIL_SQUARE,SNAKE_SQUARE_EMPTY);

      SNAKE_TAIL_SQUARE = nextSquare; 
    }

    SNAKE_MOVE_COUNT = SNAKE_MOVE_SPEED;
  }  

  snakeDraw();
}

// MINESWEEPER -----------------------------------------------------------------

#define MINES_BOARD_W 12
#define MINES_BOARD_H 11
#define MINES_SQUARE_SIZE 5
#define MINES_MINE_COUNT 22
#define MINES_OFFSET_X 2
#define MINES_OFFSET_Y 8
#define MINES_SAVE_SLOT 1

#define MINES_SQUARE_COVERED_MARKED 0x0f
#define MINES_SQUARE_COVERED        0x0e
#define MINES_SQUARE_MINE_MARKED    0x0d
#define MINES_SQUARE_MINE           0x0c   

#define MINES_SELECTED_SQUARE VAR(uint16_t,0)
#define MINES_GAME_STATE VAR(uint8_t,2) /* 0: start, 1: play, 2: lost, 3: won, 
                                           4: won, hiscore */
#define MINES_SQUARES_LEFT VAR(uint8_t,3)
#define MINES_MS_ELAPSED VAR(uint32_t,4)

void minesInit()
{
  MINES_GAME_STATE = 0;

  MINES_SELECTED_SQUARE = (MINES_BOARD_W * MINES_BOARD_H) / 2;

  for (uint16_t i = 0; i < MINES_BOARD_W * MINES_BOARD_H; ++i)
    setMemoryHalfByte(i,MINES_SQUARE_COVERED);

  MINES_SQUARES_LEFT = MINES_BOARD_W * MINES_BOARD_H - MINES_MINE_COUNT;
  MINES_MS_ELAPSED = 0;
}

void minesPlaceMines(uint16_t clickSquare)
{
  for (uint8_t i = 0; i < MINES_MINE_COUNT; ++i)
  {
    uint16_t square = SAF_random() % (MINES_BOARD_W * MINES_BOARD_H);

    while (getMemoryHalfByte(square) == MINES_SQUARE_MINE || 
      square == clickSquare)
      square = ((square != 0) ? square : (MINES_BOARD_W * MINES_BOARD_H)) - 1;

    setMemoryHalfByte(square,MINES_SQUARE_MINE);
  }
}

void minesDraw()
{
  uint8_t state = MINES_GAME_STATE;
  uint8_t color = SAF_COLOR_WHITE;

#if SAF_PLATFORM_COLOR_COUNT > 2
  if (state == 2)
    color = SAF_COLOR_RED;
  else if (state == 3 || state == 4)
    color = SAF_COLOR_GREEN;
#endif

  SAF_clearScreen(color);

  uint16_t square = 0;

  // bar:
  SAF_drawRect(0,0,SAF_SCREEN_WIDTH,MINES_OFFSET_Y - 2,SAF_COLOR_GRAY_DARK,1);

  char text[5];

  SAF_drawText(SAF_intToStr(MINES_MS_ELAPSED / 1000,text),26,1,
    SAF_COLOR_WHITE,1);

  SAF_drawText(SAF_intToStr(MINES_SQUARES_LEFT,text),3,1,SAF_COLOR_WHITE,1);

  SAF_drawText("E",58,1,SAF_COLOR_WHITE,1);

  // background rect:
  SAF_drawRect(MINES_OFFSET_X,MINES_OFFSET_Y,
    MINES_BOARD_W * MINES_SQUARE_SIZE,
    MINES_BOARD_H * MINES_SQUARE_SIZE,
#if SAF_PLATFORM_COLOR_COUNT <= 2
    SAF_COLOR_BLACK,
#else
    SAF_COLOR_BROWN,
#endif
    1);

  // squares:
  for (uint8_t y = MINES_OFFSET_Y; 
       y < (MINES_BOARD_H * MINES_SQUARE_SIZE) + MINES_OFFSET_Y;
       y += MINES_SQUARE_SIZE)
    for (uint8_t x = MINES_OFFSET_X;
      x < (MINES_BOARD_W * MINES_SQUARE_SIZE) + MINES_OFFSET_X;
      x += MINES_SQUARE_SIZE)
    {
      uint8_t squareVal = getMemoryHalfByte(square);

      uint8_t marked = squareVal == MINES_SQUARE_COVERED_MARKED ||
                       squareVal == MINES_SQUARE_MINE_MARKED;

#if SAF_PLATFORM_COLOR_COUNT > 2
      color = marked ? SAF_COLOR_RED :
        ((squareVal < MINES_SQUARE_MINE) ? SAF_COLOR_WHITE : SAF_COLOR_GRAY);
#else
      color = (squareVal >= MINES_SQUARE_MINE) ?
        SAF_COLOR_WHITE : SAF_COLOR_BLACK;
#endif

uint8_t s = squareVal < MINES_SQUARE_MINE ? MINES_SQUARE_SIZE : (MINES_SQUARE_SIZE - 1);

      SAF_drawRect(x,y,s,s,color,1);

#if SAF_PLATFORM_COLOR_COUNT <= 2
      if (marked)
        SAF_drawText("*",x,y,SAF_COLOR_BLACK,1);
#endif

      if (squareVal < MINES_SQUARE_MINE && squareVal > 0)
      {
#if SAF_PLATFORM_COLOR_COUNT > 2
        switch (squareVal)
        {
          case 1: color = SAF_COLOR_BLUE; break;
          case 2: color = SAF_COLOR_GREEN; break;
          case 3: color = SAF_COLOR_RED; break;
          case 4: color = SAF_COLOR_BLUE_DARK; break;
          case 5: color = SAF_COLOR_GRAY_DARK; break;
          case 6: color = SAF_COLOR_RED_DARK; break;
          case 7: color = SAF_COLOR_YELLOW; break;
          case 8: color = SAF_COLOR_BROWN; break;
          default: break;
        }
#else
        color = SAF_COLOR_WHITE;
#endif

        char numText[2] = "0";
        numText[0] += squareVal;
        SAF_drawText(numText,x,y,color,1);
      }
      else if (state == 2 && squareVal == MINES_SQUARE_MINE)
      {
        char t[2] = "0";
        SAF_drawText(t,x,y,
#if SAF_PLATFORM_COLOR_COUNT > 2
        SAF_COLOR_RED,
#else
        SAF_COLOR_BLACK,
#endif 
        1);
      }

      square++;
    }

  uint16_t selectedSquare = MINES_SELECTED_SQUARE;

  uint8_t x = MINES_OFFSET_X + (selectedSquare % MINES_BOARD_W) * MINES_SQUARE_SIZE;
  uint8_t y = MINES_OFFSET_Y + (selectedSquare / MINES_BOARD_W) * MINES_SQUARE_SIZE;

#if SAF_PLATFORM_COLOR_COUNT <= 2
  SAF_drawRect(x - 1,y - 1,MINES_SQUARE_SIZE + 1,MINES_SQUARE_SIZE + 1,SAF_COLOR_WHITE,0);
  SAF_drawRect(x - 2,y - 2,MINES_SQUARE_SIZE + 3,MINES_SQUARE_SIZE + 3,SAF_COLOR_BLACK,0);
#else
  SAF_drawRect(x - 2,y - 2,MINES_SQUARE_SIZE + 3,MINES_SQUARE_SIZE + 3,SAF_COLOR_BLACK,0);
#endif

  if (state == 4)
    blinkHighScore();
}

void minesReveal(uint16_t square)
{
  uint8_t squareVal = getMemoryHalfByte(square);

  if (squareVal != MINES_SQUARE_COVERED && 
      squareVal != MINES_SQUARE_COVERED_MARKED)
    return;

  MINES_SQUARES_LEFT--;

  uint8_t mineCount = 0;

  int8_t offsets[8] = { 
    -1 * MINES_BOARD_W - 1,
    -1 * MINES_BOARD_W, 
    -1 * MINES_BOARD_W + 1,
    -1, 1,
    MINES_BOARD_W - 1,
    MINES_BOARD_W,
    MINES_BOARD_W + 1};

  uint8_t checkSquares = 0xff;

  uint8_t coordinate = square % MINES_BOARD_W; // x

  if (coordinate == 0)
    checkSquares &= 0x6b;
  else if (coordinate == MINES_BOARD_W - 1)
    checkSquares &= 0xd6;

  coordinate = square / MINES_BOARD_W; // y

  if (coordinate == 0)
    checkSquares &= 0x1f;
  else if (coordinate == MINES_BOARD_H - 1)
    checkSquares &= 0xf8;

  uint8_t checkSquaresbackup = checkSquares;

  for (uint8_t i = 0; i < 8; ++i)
  {
    if (checkSquares & 0x80)
    {
      squareVal = getMemoryHalfByte(square + offsets[i]);
   
      if (squareVal == MINES_SQUARE_MINE || squareVal == MINES_SQUARE_MINE_MARKED)
        mineCount++;
    }

    checkSquares <<= 1;
  }

  setMemoryHalfByte(square,mineCount);

  if (mineCount == 0)
    for (uint8_t i = 0; i < 8; ++i)
    {
      if (checkSquaresbackup & 0x80)
        minesReveal(square + offsets[i]);

      checkSquaresbackup <<= 1;
    }

  #undef checkSquare
}

void minesStep(void)
{
  if (SAF_buttonPressed(SAF_BUTTON_B) >= BUTTON_HOLD_PERIOD)
    quitGame();

  if (MINES_GAME_STATE == 0 || MINES_GAME_STATE == 1)
  {
    if (MINES_SQUARES_LEFT == 0) // win
    {
      SAF_playSound(SAF_SOUND_BEEP);

      MINES_GAME_STATE = 3;

      uint16_t score = getHiScore(MINES_SAVE_SLOT);

      if (score == 0 || MINES_MS_ELAPSED / 1000 < score)
      {
        saveHiScore(MINES_SAVE_SLOT,MINES_MS_ELAPSED / 1000);
        MINES_GAME_STATE = 4;
      }
 
      return;
    }
 
    MINES_MS_ELAPSED += SAF_MS_PER_FRAME;

    uint8_t x = MINES_SELECTED_SQUARE % MINES_BOARD_W;
    uint8_t y = MINES_SELECTED_SQUARE / MINES_BOARD_W;

    if (buttonPressedOrHeld(SAF_BUTTON_RIGHT) && x < MINES_BOARD_W - 1)
      MINES_SELECTED_SQUARE++;
    else if (buttonPressedOrHeld(SAF_BUTTON_LEFT) && x > 0)
      MINES_SELECTED_SQUARE--;

    if (buttonPressedOrHeld(SAF_BUTTON_UP) && y > 0)
      MINES_SELECTED_SQUARE -= MINES_BOARD_W;
    else if (buttonPressedOrHeld(SAF_BUTTON_DOWN) && y < MINES_BOARD_H - 1)
      MINES_SELECTED_SQUARE += MINES_BOARD_W;

    uint8_t squareVal = getMemoryHalfByte(MINES_SELECTED_SQUARE);

    // mark/unmark:
    if (SAF_buttonJustPressed(SAF_BUTTON_B) && squareVal >= MINES_SQUARE_MINE)
      setMemoryHalfByte(MINES_SELECTED_SQUARE,squareVal ^ 0x01);

    // reveal:
    if (SAF_buttonJustPressed(SAF_BUTTON_A))
    {
      if (squareVal == MINES_SQUARE_COVERED)
      {
        if (MINES_GAME_STATE == 0)
        {
          minesPlaceMines(MINES_SELECTED_SQUARE);
          MINES_GAME_STATE = 1;
        }

        minesReveal(MINES_SELECTED_SQUARE);
      }
      else if (squareVal == MINES_SQUARE_MINE) // loss
      {
        MINES_GAME_STATE = 2;

        SAF_playSound(SAF_SOUND_BOOM);
      }
    }
  }
  else // won or lost, waiting for keypress
    if (SAF_buttonJustPressed(SAF_BUTTON_A) || 
        SAF_buttonJustPressed(SAF_BUTTON_B))
      minesInit();

  minesDraw();
}

// BLOCKS ----------------------------------------------------------------------

/* square format is following:

   MSB 76543210 LSB

   012:  block type/color, 0 = empty square
   3456: for rotation, says the position of the block within the tetromino 
   7:    1 for an active (falling) block, 0 otherwise 

   the square with value 0xff is a flashing square to be removed */

#define BLOCKS_BOARD_W 10
#define BLOCKS_BOARD_H 15
#define BLOCKS_BOARD_SQUARES (BLOCKS_BOARD_W * BLOCKS_BOARD_H)
#define BLOCKS_SQUARE_SIZE 4
#define BLOCKS_BLOCK_TYPES 8
#define BLOCKS_LINE_SCORE 10
#define BLOCKS_LAND_SCORE 1
#define BLOCKS_LEVEL_DURATION (SAF_FPS * 60)
#define BLOCKS_SPEED_INCREASE 3
#define BLOCKS_START_SPEED 25
#define BLOCKS_SAVE_SLOT 2

#define BLOCKS_OFFSET_X \
  (SAF_SCREEN_WIDTH - BLOCKS_BOARD_W * BLOCKS_SQUARE_SIZE)

#define BLOCKS_OFFSET_Y 2

#define BLOCKS_STATE VAR(uint8_t,0)
#define BLOCKS_SPEED VAR(uint8_t,1)
#define BLOCKS_NEXT_MOVE VAR(uint8_t,2)
#define BLOCKS_WAIT_TIMER VAR(uint8_t,3)
#define BLOCKS_LEVEL VAR(uint8_t,4)
#define BLOCKS_SCORE VAR(uint16_t,5)
#define BLOCKS_NEXT_LEVEL_IN VAR(uint32_t,7)

uint8_t blocksSquareIsSolid(uint8_t square)
{
  uint8_t val = memory[square];
  return (val != 0) && (val & 0x80) == 0;
}

uint8_t blocksSpawnBlock(uint8_t type)
{
  if (type == 0)
    type = 7;

  uint8_t s[4] = {4,5,14,15}; // start with square tetromino
  uint8_t v[4] = {6,6,6,6};   // all center squares, unrotatable

  /* the v array holds the position of the tetromino like this:

       0
      123
     45678
      9ab
       c */

  switch (type) // modify to a specific shape
  {
    case 1: // reverse L
      s[0] = 3; s[1] = 13; 
      v[0] = 1; v[1] = 5; v[3] = 7;  
      break; 

    case 2: // L
      s[0] = 13;
      v[0] = 5; v[1] = 3; v[3] = 7;
      break; 

    case 3: // S
      s[0] = 6;  
      v[0] = 3; v[1] = 2; v[2] = 5; v[3] = 6;
      break; 

    case 4: // Z
      s[2] = 16;
      v[0] = 1; v[1] = 2; v[2] = 7; v[3] = 6;
      break;

    case 5: // upside-down T
      s[0] = 16;
      v[0] = 7; v[1] = 2; v[2] = 5; v[3] = 6; 
      break; 

    case 6: // I
      s[2] = 3; s[3] = 6; 
      v[1] = 7; v[2] = 5; v[3] = 8;
      break; 

    default: break;
  }

  type |= 0x80;

  uint8_t result = 1;

  for (uint8_t i = 0; i < 4; ++i)
  {
    uint8_t square = s[i];

    if (memory[square] != 0)
      result = 0;

    memory[square] = type | ((v[i] << 3));
  }

  return result;
}

void blocksInit(void)
{
  clearMemory();

  BLOCKS_SPEED = BLOCKS_START_SPEED;
  BLOCKS_NEXT_MOVE = BLOCKS_START_SPEED;
  BLOCKS_WAIT_TIMER = 0;
  BLOCKS_LEVEL = 0;
  BLOCKS_SCORE = 0;
  BLOCKS_NEXT_LEVEL_IN = 0;
  BLOCKS_STATE = 0;

  blocksSpawnBlock(SAF_random() % BLOCKS_BLOCK_TYPES);
}

void blocksRotate(uint8_t left)
{
  const int8_t rotationMap[13 * 2] =
    /* old    new  square offset */
    {/* 0 */  8,   2 * BLOCKS_BOARD_W + 2,
     /* 1 */  3,   2,
     /* 2 */  7,   BLOCKS_BOARD_W + 1,
     /* 3 */  11,  2 * BLOCKS_BOARD_W,
     /* 4 */  0,   -2 * BLOCKS_BOARD_W + 2,
     /* 5 */  2,   -1 * BLOCKS_BOARD_W + 1,
     /* 6 */  6,   0,
     /* 7 */  10,  BLOCKS_BOARD_W - 1,
     /* 8 */  12,  2 * BLOCKS_BOARD_W - 2,
     /* 9 */  1,   -2 * BLOCKS_BOARD_W,
     /* 10*/  5,   -1 * BLOCKS_BOARD_W - 1,
     /* 11*/  9,   -2,
     /* 12*/  4,   -2 * BLOCKS_BOARD_W - 2};

  uint8_t blocksProcessed = 0;
  uint8_t newPositions[4];
  uint8_t newValues[4];

  for (uint8_t i = 0; i < BLOCKS_BOARD_SQUARES; ++i)
  {
    uint8_t square = memory[i];

    if (square & 0x80)
    {

      uint8_t index = 2 * ((square >> 3) & 0x0f);

      uint8_t newPos = i + rotationMap[index + 1];

      if (left)
      {
        // rotate two more times

        index = rotationMap[index] * 2;
        newPos += rotationMap[index + 1];

        index = rotationMap[index] * 2;
        newPos += rotationMap[index + 1];
      }

      int xDiff = (newPos % BLOCKS_BOARD_W) - (i % BLOCKS_BOARD_W);

      xDiff = xDiff >= 0 ? xDiff : (-1 * xDiff);

      if ((xDiff > 2) || // left/right outside?
          (newPos >= BLOCKS_BOARD_SQUARES) || // top/bottom outside?
          blocksSquareIsSolid(newPos))
        return; // can't rotate

      newValues[blocksProcessed] = (square & 0x87) | (rotationMap[index] << 3);
      newPositions[blocksProcessed] = newPos;

      blocksProcessed++;

      if (blocksProcessed >= 4)
        break;
    }
  }

  for (uint8_t i = 0; i < BLOCKS_BOARD_SQUARES; ++i)
    if (memory[i] & 0x80)
      memory[i] = 0;

  for (uint8_t i = 0; i < 4; ++i)
    memory[newPositions[i]] = newValues[i];
}

void blocksDraw(void)
{
  SAF_clearScreen(BLOCKS_STATE ? SAF_COLOR_RED_DARK : SAF_COLOR_GRAY_DARK);

  SAF_drawRect(
#if SAF_PLATFORM_COLOR_COUNT > 2
    BLOCKS_OFFSET_X,BLOCKS_OFFSET_Y,
    BLOCKS_BOARD_W * BLOCKS_SQUARE_SIZE, BLOCKS_BOARD_H * BLOCKS_SQUARE_SIZE,
#else
    BLOCKS_OFFSET_X - 1,BLOCKS_OFFSET_Y- 1,BLOCKS_BOARD_W * BLOCKS_SQUARE_SIZE 
    + 2, BLOCKS_BOARD_H * BLOCKS_SQUARE_SIZE + 2,
#endif
    SAF_COLOR_WHITE,1);

  char text[16] = "L ";

  SAF_intToStr(BLOCKS_LEVEL,text + 1);
  SAF_drawText(text,2,3,SAF_COLOR_WHITE,1);
  SAF_intToStr(BLOCKS_SCORE,text);
  SAF_drawText(text,2,10,SAF_COLOR_WHITE,1);

  uint8_t x = 0, y = 0;

  for (uint8_t i = 0; i < BLOCKS_BOARD_SQUARES; ++i)
  {
    uint8_t square = memory[i]; 

    if (square)
    {
      if (square != 0xff)
        square &= 0x07;

      uint8_t color = 0;

#if SAF_PLATFORM_COLOR_COUNT > 2
      switch (square)
      {
        case 1: color = SAF_COLOR_RED; break;
        case 2: color = SAF_COLOR_GREEN; break;
        case 3: color = SAF_COLOR_BROWN; break;
        case 4: color = SAF_COLOR_YELLOW; break;
        case 5: color = SAF_COLOR_ORANGE; break;
        case 6: color = SAF_COLOR_BLUE; break;
        case 7: color = SAF_COLOR_GREEN_DARK; break;
        case 0xff: color = ((SAF_frame() >> 2) & 0x01) ? SAF_COLOR_BLACK : SAF_COLOR_WHITE; break;
        default: break;
      }
#else
      color = (square != 0xff) ? SAF_COLOR_BLACK :
        ((SAF_frame() >> 2) & 0x01) ? SAF_COLOR_BLACK : SAF_COLOR_WHITE;
#endif

      uint8_t
        drawX = BLOCKS_OFFSET_X + x * BLOCKS_SQUARE_SIZE,
        drawY = BLOCKS_OFFSET_Y + y * BLOCKS_SQUARE_SIZE;

      SAF_drawRect(drawX,drawY,BLOCKS_SQUARE_SIZE,BLOCKS_SQUARE_SIZE,color,1);
    }

    x++;

    if (x >= 10)
    {
      x = 0;
      y++;
    }
  }

  if (BLOCKS_STATE && BLOCKS_SCORE == getHiScore(BLOCKS_SAVE_SLOT))
    blinkHighScore();
}

uint8_t blocksFallStep(void)
{
  uint8_t canFall = 1;

  for (uint8_t i = 0; i < BLOCKS_BOARD_SQUARES; ++i)
    if (memory[i] & 0x80)
    {
      if (i / BLOCKS_BOARD_W == BLOCKS_BOARD_H - 1)
      {
        canFall = 0;
        break;
      }

      if (blocksSquareIsSolid(i + BLOCKS_BOARD_W))
      {
        canFall = 0;
        break;
      }
    }

  if (canFall)
  {
    for (uint8_t i = BLOCKS_BOARD_W * (BLOCKS_BOARD_H - 1) - 1; i != 255; --i)
      if (memory[i] & 0x80)
      {
        memory[i + BLOCKS_BOARD_W] = memory[i];
        memory[i] = 0;
      }
  }
 
  return canFall; 
}

void blocksMoveHorizontally(uint8_t left)
{
  uint8_t limitCol = left ? 0 : (BLOCKS_BOARD_W - 1);
  int8_t increment = left ? -1 : 1;

  for (uint8_t i = 0; i < BLOCKS_BOARD_SQUARES; ++i)
    if ((memory[i] & 0x80) && 
        (
          (i % BLOCKS_BOARD_W == limitCol) ||
          blocksSquareIsSolid(i + increment)
        ))
      return;

  uint8_t i0 = 0, i1 = BLOCKS_BOARD_SQUARES;

  if (!left)
  {
    i0 = BLOCKS_BOARD_SQUARES - 1;
    i1 = 255;
  }

  for (uint8_t i = i0; i != i1; i -= increment)
    if (memory[i] & 0x80)
    {
      memory[i + increment] = memory[i];
      memory[i] = 0;
    }
}

void blocksRemoveLines(void)
{
  for (uint8_t i = 0; i < BLOCKS_BOARD_SQUARES; ++i)
    if (memory[i] == 0xff)
    {
      BLOCKS_SCORE += BLOCKS_LINE_SCORE;

      for (uint8_t j = i + BLOCKS_BOARD_W - 1; j >= BLOCKS_BOARD_W; --j)
        memory[j] = memory[j - BLOCKS_BOARD_W];

      for (uint8_t j = 0; j < BLOCKS_BOARD_W; ++j)
        memory[j] = 0;
    } 
}

void blocksStep(void)
{
  if (SAF_buttonPressed(SAF_BUTTON_B) >= BUTTON_HOLD_PERIOD)
    quitGame();

  blocksDraw();

  if (BLOCKS_STATE == 1)
  {
    if (SAF_buttonJustPressed(SAF_BUTTON_A) ||
      SAF_buttonJustPressed(SAF_BUTTON_B))
    {
      blocksInit();
      BLOCKS_STATE = 0;
    }

    return;
  }

  if (BLOCKS_NEXT_LEVEL_IN == 0)
  {
    BLOCKS_LEVEL += 1;
    BLOCKS_NEXT_LEVEL_IN = BLOCKS_LEVEL_DURATION;

    if (BLOCKS_SPEED > BLOCKS_SPEED_INCREASE)
      BLOCKS_SPEED -= BLOCKS_SPEED_INCREASE;
  }

  BLOCKS_NEXT_LEVEL_IN -= 1;

  if (BLOCKS_WAIT_TIMER > 0)
  {
    BLOCKS_WAIT_TIMER -= 1;

    if (BLOCKS_WAIT_TIMER == 1)
      blocksRemoveLines();

    return;
  }

  BLOCKS_NEXT_MOVE = BLOCKS_NEXT_MOVE - 1;

  if (SAF_buttonJustPressed(SAF_BUTTON_A))
  {
    // drop the block:

    while (blocksFallStep());

    SAF_playSound(SAF_SOUND_BUMP);

    BLOCKS_NEXT_MOVE = 0;
  }

  if (buttonPressedOrHeld(SAF_BUTTON_LEFT))
    blocksMoveHorizontally(1);
  else if (buttonPressedOrHeld(SAF_BUTTON_RIGHT))
    blocksMoveHorizontally(0);

  if (SAF_buttonJustPressed(SAF_BUTTON_UP))
    blocksRotate(0);
  else if (SAF_buttonJustPressed(SAF_BUTTON_DOWN))
    blocksRotate(1);

  if (BLOCKS_NEXT_MOVE == 0)
  {
    if (!blocksFallStep())
    {
      for (uint8_t i = 0; i < BLOCKS_BOARD_SQUARES; ++i)
        memory[i] &= 0x07;

      // scan for completed lines:

      uint8_t col = 0;
      uint8_t count = 0;
      uint8_t lineCompleted = 0;

      for (uint8_t i = 0; i < BLOCKS_BOARD_SQUARES; ++i)
      {
        if (memory[i] != 0)
          count++;

        col++;

        if (col >= BLOCKS_BOARD_W)
        {
          if (count >= BLOCKS_BOARD_W)
          {
            lineCompleted = 1;

            for (uint8_t j = i - BLOCKS_BOARD_W + 1; j <= i; ++j)
              memory[j] = 0xff; 
          }

          col = 0;
          count = 0;
        }
      }

      if (lineCompleted)
      {
        BLOCKS_WAIT_TIMER = 20;
        SAF_playSound(SAF_SOUND_BEEP);
      }

      BLOCKS_SCORE += 1;

      if (!blocksSpawnBlock(SAF_random() % BLOCKS_BLOCK_TYPES))
      {
        BLOCKS_STATE = 1; // game over

        SAF_playSound(SAF_SOUND_BOOM);

        if (BLOCKS_SCORE >= getHiScore(BLOCKS_SAVE_SLOT))
          saveHiScore(BLOCKS_SAVE_SLOT,BLOCKS_SCORE);
      }
    }

    BLOCKS_NEXT_MOVE = BLOCKS_SPEED;
  }
}

// 2048 ------------------------------------------------------------------------

/* The first 16 bytes of the board hold the power values of the squares, and
   another 16 byte block follows which contains info for animation: each square
   has the previous square value in the lower 4 bits and the travel distance in
   the upper 4 bits. */

#define G2048_SQUARE_SIZE 11
#define G2048_BOARD_SIZE ((G2048_SQUARE_SIZE + 1) * 4 + 1)
#define G2048_SQUARES 16
#define G2048_OFFSET_X ((SAF_SCREEN_WIDTH - G2048_BOARD_SIZE) / 2)
#define G2048_OFFSET_Y 2
#define G2048_SAVE_SLOT 3

#define G2048_ANIMATION_FRAMES 8

#define G2048_ANIMATION_COUNTDOWN VAR(uint8_t,0)
#define G2048_ANIMATION_DIRECTION VAR(uint8_t,1)
#define G2048_WON VAR(uint8_t,2)
#define G2048_MS_ELAPSED VAR(uint32_t,3)
#define G2048_HIGHSCORE_COUNTDOWN VAR(uint8_t,7)

void g2048Spawn(void)
{
  uint8_t square = SAF_random() % G2048_SQUARES;

  for (uint8_t i = 0; i < G2048_SQUARES; ++i)
  {
    uint8_t s = (square + i) % G2048_SQUARES;

    if (memory[s] == 0)
    {
      memory[s] = (SAF_random() < 200) ? 1 : 2;
      break;
    }
  }
}

void g2048Init(void)
{
  G2048_ANIMATION_COUNTDOWN = 0;
  G2048_ANIMATION_DIRECTION = 0;
  G2048_WON = 0;
  G2048_MS_ELAPSED = 0;
  G2048_HIGHSCORE_COUNTDOWN = 0;

  for (uint8_t i = 0; i < G2048_SQUARES; ++i)
    memory[i] = 0;

  g2048Spawn();
}

void g2048Draw(void)
{
#if SAF_PLATFORM_COLOR_COUNT > 2
  SAF_clearScreen(SAF_COLOR_GRAY);
#else
  SAF_clearScreen(SAF_COLOR_BLACK);
#endif

  SAF_drawRect(G2048_OFFSET_X,G2048_OFFSET_Y,
    G2048_BOARD_SIZE,G2048_BOARD_SIZE,SAF_COLOR_WHITE,1);

  uint8_t square = 0;

  uint16_t animationNominator = G2048_SQUARE_SIZE * 
    (G2048_ANIMATION_FRAMES - G2048_ANIMATION_COUNTDOWN + 1);

  for (uint8_t y = 0; y < 4; ++y)
    for (uint8_t x = 0; x < 4; ++x)
    {
      uint8_t v = memory[
        G2048_ANIMATION_COUNTDOWN == 0 ? square : (G2048_SQUARES + square)];

      if (v == 0)
      {
        square++;
        continue;
      }
      
      int8_t animX = 0, animY = 0;

      if (G2048_ANIMATION_COUNTDOWN != 0)
      {
        uint16_t offset = v >> 4;

        offset = (offset * animationNominator) / G2048_ANIMATION_FRAMES;

        switch (G2048_ANIMATION_DIRECTION)
        {
          case 0: animY = -1 * offset; break;
          case 1: animX = offset; break;
          case 2: animY = offset; break;
          case 3: animX = -1 * offset; break;
          default: break;
        }

        v &= 0x0f;
      }

      uint8_t drawX = G2048_OFFSET_X + 1 + x * (G2048_SQUARE_SIZE + 1) + animX,
              drawY = G2048_OFFSET_Y + 1 + y * (G2048_SQUARE_SIZE + 1) + animY;

      uint8_t color = SAF_COLOR_BLACK,
              color2 = SAF_COLOR_GRAY_DARK;

#if SAF_PLATFORM_COLOR_COUNT == 2
      color2 = SAF_COLOR_WHITE;
#else
      switch (v)
      {
        case 1:  color = SAF_COLOR_RGB(31,222,79); break;
        case 2:  color = SAF_COLOR_RGB(122,120,235); break;
        case 3:  color = SAF_COLOR_RGB(235,123,89); break;
        case 4:  color = SAF_COLOR_RGB(179,107,77); break;
        case 5:  color = SAF_COLOR_RGB(237,162,36); break;
        case 6:  color = SAF_COLOR_RGB(209,17,42); break;
        case 7:  color = SAF_COLOR_RGB(13,189,148); break;
        case 8:  color = SAF_COLOR_RGB(207,112,190); break;
        case 9:  color = SAF_COLOR_RGB(156,62,115); 
                 color2 = SAF_COLOR_WHITE; break;
        case 10: color = SAF_COLOR_RGB(80,156,36);
                 color2 = SAF_COLOR_WHITE; break;
        case 11: color = SAF_COLOR_RGB(224,126,20); break;
        default: color = SAF_COLOR_RGB(122,15,5); 
                 color2 = SAF_COLOR_GRAY; break;
      }
#endif

      SAF_drawRect(drawX,drawY,
        G2048_SQUARE_SIZE,G2048_SQUARE_SIZE,color,1);

      char t[3] = "  ";
      uint8_t plusX = 4;

      if (v < 10)
        t[0] = '0' + v;
      else
      {
        t[0] = '1';
        t[1] = '0' + v - 10;
        plusX = 0;
      }

      SAF_drawText(t,drawX + plusX,drawY + (G2048_SQUARE_SIZE - 4) / 2,
        color2,1);

      square++;
    }

  SAF_drawRect(
    0,SAF_SCREEN_HEIGHT - 6,SAF_SCREEN_WIDTH,6,SAF_COLOR_GRAY_DARK,1);

  char timeText[8];

  SAF_drawText(SAF_intToStr(G2048_MS_ELAPSED / 1000,timeText),2,
    SAF_SCREEN_HEIGHT - 5,SAF_COLOR_WHITE,1);

  if (G2048_WON)
    SAF_drawText("WIN",SAF_SCREEN_WIDTH - 18,SAF_SCREEN_HEIGHT - 5,
      SAF_COLOR_YELLOW,1);

  if (G2048_HIGHSCORE_COUNTDOWN)
    blinkHighScore();
}

int8_t g2048NextSquare(int8_t square, uint8_t direction)
{
  switch (direction)
  {
    case 0: return (square >= 4) ? square - 4 : -1; break; // up
    case 1: return (square % 4 != 3) ? square + 1 : -1; break; // right
    case 2: return (square < 12) ? square + 4 : -1; break; // down
    case 3: return (square % 4 != 0) ? square - 1 : -1; break; // left
    default: return -1;
  }
}

uint8_t g2048Shift(uint8_t direction)
{
  uint8_t shifted = 0;

  int8_t square = (direction == 1 || direction == 2) ? G2048_SQUARES - 1 : 0;
  int8_t nextLineOffset = 0;
  uint8_t oppositeDirection = (direction + 2) % 4;

  // make a copy of the board that will be used for animation:

  for (uint8_t i = 0; i < G2048_SQUARES; ++i)
    memory[G2048_SQUARES + i] = memory[i];

  switch (direction)
  {
    case 0: nextLineOffset = -4 * 3 + 1; break;
    case 1: nextLineOffset = -1; break;
    case 2: nextLineOffset = 4 * 3 - 1; break;
    case 3: nextLineOffset = 1; break;
    default: break;
  }

  for (uint8_t i = 0; i < 4; ++i) // for each "line"
  {
    while (1) // for each square in the "line"
    {
      int8_t nextSquare = square;

      uint8_t *animSquare = memory + G2048_SQUARES + square;
      uint8_t distance = 0;

      uint8_t squareVal = memory[square];

      if (squareVal != 0)
      {
        while (1) // slide the square
        {
          int8_t previouSquare = nextSquare;

          nextSquare = g2048NextSquare(nextSquare,direction);

          if (nextSquare < 0)
            break; // end of board

          uint8_t squareVal2 = memory[nextSquare];

          if (squareVal2 == squareVal)
          {
            memory[nextSquare] = squareVal + 1;

            if (squareVal == 10)
            {
              G2048_WON = 1;

              uint16_t score = getHiScore(G2048_SAVE_SLOT);

              if (score == 0 || G2048_MS_ELAPSED / 1000 < score)
              {
                saveHiScore(G2048_SAVE_SLOT,G2048_MS_ELAPSED / 1000);

                G2048_HIGHSCORE_COUNTDOWN = 75;
              }
            }

            memory[previouSquare] = 0;
            shifted = 1;
            distance++;
            break;
          }

          if (squareVal2 == 0)
          {
            memory[nextSquare] = squareVal;
            memory[previouSquare] = 0;
            shifted = 1;
            distance++;
          }
          else
            break;
        }

        *animSquare |= (distance << 4);
      }

      nextSquare = g2048NextSquare(square,oppositeDirection);

      if (nextSquare < 0)
      {
        square += nextLineOffset;
        break;
      }

      square = nextSquare;
    }
  }

  if (shifted)
  {
    G2048_ANIMATION_COUNTDOWN = G2048_ANIMATION_FRAMES;
    G2048_ANIMATION_DIRECTION = direction;
  }

  return shifted; 
}

void g2048Step(void)
{
  if (SAF_buttonPressed(SAF_BUTTON_B) >= BUTTON_HOLD_PERIOD)
    quitGame();

  G2048_MS_ELAPSED += SAF_MS_PER_FRAME;

  if (G2048_HIGHSCORE_COUNTDOWN > 0)
    G2048_HIGHSCORE_COUNTDOWN -= 1;

  if (G2048_ANIMATION_COUNTDOWN == 0)
  {
    int8_t direction = -1;

    if (SAF_buttonJustPressed(SAF_BUTTON_UP))
      direction = 0;
    else if (SAF_buttonJustPressed(SAF_BUTTON_RIGHT))
      direction = 1;
    else if (SAF_buttonJustPressed(SAF_BUTTON_DOWN))
      direction = 2;
    else if (SAF_buttonJustPressed(SAF_BUTTON_LEFT))
      direction = 3;

    if (direction >= 0)
      if (g2048Shift(direction))
        g2048Spawn();
  }
  else
  {
    G2048_ANIMATION_COUNTDOWN -= 1;
  }

  g2048Draw();
}

// RUNNER ----------------------------------------------------------------------

/* Memory is a list of obstacles, terminated by 0. Each obstacle takes two
  bytes: 1st is type, 2nd is x position. */

#define RUNNER_OBSTACLE_NONE 0
#define RUNNER_OBSTACLE_WALL 1
#define RUNNER_OBSTACLE_CEILING 2
#define RUNNER_OBSTACLE_WALL_TALL 3
#define RUNNER_OBSTACLE_WALL_LONG 4
#define RUNNER_OBSTACLE_MOVING 5

#define RUNNER_REAL_OBSTACLES 5

#define RUNNER_OBSTACLE_BONUS 6 // item: extra points
#define RUNNER_OBSTACLE_SHIELD 7 // item: protection

#define RUNNER_SCORE_OBSTACLE 2
#define RUNNER_SCORE_BONUS 5

#define RUNNER_PLAYER_WIDTH 5
#define RUNNER_PLAYER_HEIGHT 8
#define RUNNER_PLAYER_X_POSITION 16
#define RUNNER_GROUND_POS 40
#define RUNNER_RENDER_X_OFFSET 8 // so that obstacles go whole behind L border
#define RUNNER_PLAYER_PHASE_INCREASE 7
#define RUNNER_SAVE_SLOT 4

#define RUNNER_POSITION VAR(uint16_t,0)
#define RUNNER_PLAYER_PHASE VAR(uint8_t,2)
#define RUNNER_NEXT_OBSTACLE_IN VAR(uint8_t,3)
#define RUNNER_SHIELD_COUNTDOWN VAR(uint8_t,4)
#define RUNNER_STATE VAR(uint8_t,5) // 0: play, 1: lost, 2: lost, high score
#define RUNNER_SCORE VAR(uint16_t,6)

uint8_t runnerLevel(void)
{
  return (RUNNER_POSITION >> 8) & 0xff;
}

void runnerAddObstacle(uint8_t type)
{
  uint8_t i = 0;

  while (memory[i] != RUNNER_OBSTACLE_NONE)
    i += 2;

  memory[i] = type;

  i++;
  memory[i] = SAF_SCREEN_WIDTH + RUNNER_RENDER_X_OFFSET;

  i++;
  memory[i] = RUNNER_OBSTACLE_NONE; // terminate

  uint8_t level = runnerLevel();

  if (level > 10)
    level = 10;

  int8_t minDistance = 25 - level;

  uint8_t additionalDistance = SAF_random() % (18 - level);

  RUNNER_NEXT_OBSTACLE_IN = minDistance + additionalDistance;
}

uint8_t runnerObstacleIsTakeable(uint8_t type)
{
  return type == RUNNER_OBSTACLE_BONUS || type == RUNNER_OBSTACLE_SHIELD;
}

void runnerGetObstacleSize(uint8_t type, uint8_t position,
  uint8_t *width, uint8_t *height, uint8_t *elevation)
{
  *width = RUNNER_PLAYER_WIDTH;
  *height = (RUNNER_PLAYER_HEIGHT * 3) / 4;
  *elevation = 0;

  switch (type)
  {
    case RUNNER_OBSTACLE_WALL_TALL:
      *width -= 1; *height = RUNNER_PLAYER_HEIGHT;
      break;

    case RUNNER_OBSTACLE_WALL_LONG:
      *width = (*width * 7) / 4; *height /= 2;
      break;

    case RUNNER_OBSTACLE_CEILING:
      *elevation = RUNNER_PLAYER_HEIGHT - 2;
      break;

    case RUNNER_OBSTACLE_MOVING:
      *width -= 2;
      *elevation =
        (SAF_cos((RUNNER_POSITION * 4 - position * 2) & 0xff) + 128) / 32; 
       break;

    case RUNNER_OBSTACLE_BONUS:
    case RUNNER_OBSTACLE_SHIELD:
      *width = 4; *height = 4; *elevation = (RUNNER_PLAYER_HEIGHT * 10) / 4;
      break;

    default: break;
  }
}

void runnerInit(void)
{
  RUNNER_POSITION = 0;
  RUNNER_PLAYER_PHASE = 0;
  RUNNER_NEXT_OBSTACLE_IN = 0;
  RUNNER_SHIELD_COUNTDOWN = 0;
  RUNNER_STATE = 0;
  RUNNER_SCORE = 0;

  memory[0] = RUNNER_OBSTACLE_NONE;
}

uint8_t runnerPlayerHeight(void)
{
  return SAF_sin(RUNNER_PLAYER_PHASE) / 8;
}

uint8_t runnerPlayerTallness(void)
{
  return RUNNER_PLAYER_PHASE != 255 ? RUNNER_PLAYER_HEIGHT :
    (RUNNER_PLAYER_HEIGHT / 2);
}

void runnerRemoveObstacle(uint8_t index)
{
  while (memory[index] != RUNNER_OBSTACLE_NONE)
  {
    memory[index] = memory[index + 2];
    memory[index + 1] = memory[index + 3];
    index += 2;
  }
}

void runnerResolveCollisions(void)
{
  uint8_t i = 0;

  uint8_t playerFrom = runnerPlayerHeight(),
          playerTo = playerFrom + runnerPlayerTallness();

  while (memory[i] != RUNNER_OBSTACLE_NONE)
  {
    uint8_t w, h, e;
    uint8_t pos = memory[i + 1];

    uint8_t obstacle = memory[i];

    runnerGetObstacleSize(obstacle,pos,&w,&h,&e);

    if (pos >= RUNNER_PLAYER_X_POSITION + RUNNER_PLAYER_WIDTH)
      break;

    uint8_t obstacleTo = e + h;

    if (pos + w > RUNNER_PLAYER_X_POSITION &&
      ((playerFrom >= e && playerFrom < obstacleTo) ||
       (playerTo >= e && playerTo < obstacleTo)))
    {
      if (runnerObstacleIsTakeable(memory[i]))
        runnerRemoveObstacle(i);

      if (obstacle == RUNNER_OBSTACLE_SHIELD)
      {
        RUNNER_SHIELD_COUNTDOWN = 255;
        SAF_playSound(SAF_SOUND_CLICK);
      }
      else if (obstacle == RUNNER_OBSTACLE_BONUS)
      {
        RUNNER_SCORE += RUNNER_SCORE_BONUS; 
        SAF_playSound(SAF_SOUND_CLICK);
      }
      else
      {
        if (RUNNER_SHIELD_COUNTDOWN == 0)
        {
          RUNNER_STATE = 1;
      
          SAF_playSound(SAF_SOUND_BOOM);

          uint16_t score = getHiScore(RUNNER_SAVE_SLOT);

          if (RUNNER_SCORE > score)
          {
            saveHiScore(RUNNER_SAVE_SLOT,RUNNER_SCORE);
            RUNNER_STATE = 2;
          } 
        }
        else
        { 
          runnerRemoveObstacle(i);
          RUNNER_SHIELD_COUNTDOWN = 0;
        }
      }

      return;
    }

    i += 2;
  }
}

void runnerDraw(void)
{
  SAF_drawRect(0,0,SAF_SCREEN_WIDTH,RUNNER_GROUND_POS,SAF_COLOR_WHITE,1);

  uint8_t groundPos = RUNNER_POSITION % 32;

  for (uint8_t i = 0; i < SAF_SCREEN_WIDTH; ++i) // draw ground
  {
    for (uint8_t j = RUNNER_GROUND_POS; j < SAF_SCREEN_HEIGHT; ++j)
      SAF_drawPixel(i,j,
      #if SAF_PLATFORM_COLOR_COUNT > 2
        groundPos & 0x10 ? SAF_COLOR_GRAY : SAF_COLOR_GRAY_DARK
      #else
        (i % 2 == j % 2) ? SAF_COLOR_BLACK : SAF_COLOR_WHITE 
      #endif
      );

    groundPos++;
  }

  uint8_t height = runnerPlayerTallness(),
          color = (RUNNER_STATE == 0) ? SAF_COLOR_BLACK : 
            (SAF_frame() & 0x2 ? SAF_COLOR_BLACK : SAF_COLOR_WHITE),
          dx = RUNNER_PLAYER_X_POSITION - RUNNER_RENDER_X_OFFSET,
          dy = RUNNER_GROUND_POS - height - runnerPlayerHeight();

  // draw the player:

  SAF_drawRect(dx,dy,RUNNER_PLAYER_WIDTH,height,color,1);

  if (RUNNER_SHIELD_COUNTDOWN != 0)
    SAF_drawCircle(dx + RUNNER_PLAYER_WIDTH / 2,dy + RUNNER_PLAYER_HEIGHT / 2,7,
#if SAF_PLATFORM_COLOR_COUNT > 2
      SAF_COLOR_BLUE,
#else
      SAF_COLOR_BLACK,
#endif
      0);

  uint8_t i = 0;

  while (1) // draw obstacles
  {
    uint8_t obstacle = memory[i];

    if (obstacle == RUNNER_OBSTACLE_NONE)
      break;

    uint8_t w, h, e;

    runnerGetObstacleSize(obstacle,memory[i + 1],&w,&h,&e);

    dx = memory[i + 1] - RUNNER_RENDER_X_OFFSET;
    dy = RUNNER_GROUND_POS - h - e;

    if (runnerObstacleIsTakeable(obstacle))
      SAF_drawCircle(dx,dy,w / 2,
#if SAF_PLATFORM_COLOR_COUNT > 2
        obstacle == RUNNER_OBSTACLE_BONUS ?
        SAF_COLOR_YELLOW : SAF_COLOR_BLUE,1
#else
        SAF_COLOR_BLACK, obstacle == RUNNER_OBSTACLE_BONUS
#endif
        );
    else
      SAF_drawRect(dx,dy,w,h,
#if SAF_PLATFORM_COLOR_COUNT > 2
      SAF_COLOR_RED_DARK,
#else
      SAF_COLOR_BLACK,
#endif
      1);
    
    i += 2;
  }  

  char text[12];

  SAF_intToStr(RUNNER_POSITION / 8,text);

  uint8_t p = 0;

  while (text[p] != 0)
    p++;

  text[p] = 'M';
  text[p + 1] = 0;

  SAF_drawText(text,2,2,
#if SAF_PLATFORM_COLOR_COUNT > 2
    SAF_COLOR_GRAY
#else
    SAF_COLOR_BLACK
#endif
    ,1);

  drawTextRightAlign(SAF_SCREEN_WIDTH - 2,2,SAF_intToStr(RUNNER_SCORE,text),
    SAF_COLOR_BLACK,1);

  if (RUNNER_STATE == 2)
    blinkHighScore();
}

void runnerScroll(void)
{
  RUNNER_POSITION += 1;

  uint8_t i = 0;

  while (memory[i] != RUNNER_OBSTACLE_NONE)
  {
    memory[i + 1] -= 1;
    i += 2;
  }

  uint8_t addScore = 0;

  if (memory[0] != RUNNER_OBSTACLE_NONE && memory[1] == 0)
  {
    addScore = memory[0] != RUNNER_OBSTACLE_SHIELD &&
      memory[0] != RUNNER_OBSTACLE_BONUS;

    // remove the obstacle once it's at position 0

    runnerRemoveObstacle(0);
  }

  if (addScore)
    RUNNER_SCORE += RUNNER_SCORE_OBSTACLE;
}

void runnerStep(void)
{
  runnerDraw();

  if (SAF_buttonPressed(SAF_BUTTON_B) >= BUTTON_HOLD_PERIOD)
    quitGame();

  if (RUNNER_STATE != 0)
  {
    if (SAF_buttonJustPressed(SAF_BUTTON_A) ||
      SAF_buttonJustPressed(SAF_BUTTON_B))
      runnerInit();

    return;
  }

  uint8_t duck = SAF_buttonPressed(SAF_BUTTON_DOWN);

  if (RUNNER_PLAYER_PHASE == 0)
  {  
    if (SAF_buttonJustPressed(SAF_BUTTON_A) || SAF_buttonPressed(SAF_BUTTON_UP))
    {
      // jump:
      RUNNER_PLAYER_PHASE = RUNNER_PLAYER_PHASE_INCREASE;
      SAF_playSound(SAF_SOUND_BUMP);
    }
    else if (duck)
      RUNNER_PLAYER_PHASE = 255;
  }
  else if (RUNNER_PLAYER_PHASE != 255)
  {
    RUNNER_PLAYER_PHASE += RUNNER_PLAYER_PHASE_INCREASE;

    if (RUNNER_PLAYER_PHASE > 128)
      RUNNER_PLAYER_PHASE = 0;
  }
  else
    RUNNER_PLAYER_PHASE = duck ? 255 : 0;

  if (RUNNER_SHIELD_COUNTDOWN != 0)
    RUNNER_SHIELD_COUNTDOWN -= 1;

  runnerScroll();

  runnerResolveCollisions();

  if (RUNNER_NEXT_OBSTACLE_IN == 0)
  {
    uint8_t level = runnerLevel() + 1;
  
    if (level > RUNNER_REAL_OBSTACLES)
      level = RUNNER_REAL_OBSTACLES; 
  
    uint8_t r = SAF_random();

    if (r > 240)
    {
      runnerAddObstacle(r > 245 ? RUNNER_OBSTACLE_BONUS
        : RUNNER_OBSTACLE_SHIELD);
    }
    else
      runnerAddObstacle(SAF_random() % level + 1);
  }
  else
    RUNNER_NEXT_OBSTACLE_IN -= 1;
}

// -----------------------------------------------------------------------------

uint8_t menuItem = 0;
uint8_t firstClick = 0;

void menuStep(void)
{
  SAF_clearScreen(SAF_COLOR_WHITE);  
  SAF_drawCircle(13,47,17,SAF_COLOR_GRAY_DARK,1);

  SAF_drawRect(0,29,SAF_SCREEN_WIDTH,6,SAF_COLOR_GREEN,1);

#if SAF_PLATFORM_COLOR_COUNT > 2
  SAF_drawText(gameNames[menuItem],12,25,SAF_COLOR_GRAY,2);
#endif

  SAF_drawText(gameNames[menuItem],11,24,SAF_COLOR_BLACK,2);

  uint16_t score = getHiScore(menuItem);

  char scoreText[6] = "XXX";

  if (score != 0)
    SAF_intToStr(score,scoreText);

  SAF_drawText(scoreText,score < 10000 ? 6 : 1,45,SAF_COLOR_WHITE,1);

  if (buttonPressedOrHeld(SAF_BUTTON_RIGHT))
  {
    menuItem = (menuItem + 1) % GAMES;
    SAF_playSound(SAF_SOUND_CLICK);
  }
  else if (buttonPressedOrHeld(SAF_BUTTON_LEFT))
  {
    menuItem = (menuItem > 0) ? menuItem - 1 : (GAMES - 1);
    SAF_playSound(SAF_SOUND_CLICK);
  }

  if (SAF_buttonPressed(SAF_BUTTON_A))
  {
    if (!firstClick)
    {
      SAF_randomSeed(SAF_frame()); // create a somewhat random initial seed
      firstClick = 1;
    }

    switch (menuItem)
    {
      case 0: snakeInit(); stepFunction = &snakeStep; break;
      case 1: minesInit(); stepFunction = &minesStep; break;
      case 2: blocksInit(); stepFunction = &blocksStep; break;
      case 3: g2048Init(); stepFunction = &g2048Step; break;
      case 4: runnerInit(); stepFunction = &runnerStep; break;
      default: break;
    }

    SAF_playSound(SAF_SOUND_CLICK);
  }
}

void SAF_init(void)
{
  stepFunction = &menuStep;
}

uint8_t SAF_loop(void)
{
  stepFunction();
  return 1;
}
