/**
  Simple chess for SAF, using smallchesslib.

  by drummyfish, released under CC0 1.0, public domain
*/


#define SAF_PLATFORM_ESPBOY 1

#define SIMPLE 0         // simpler version with 1b graphics
#define AI_COUNTDOWN 20  /* If -1, AI against AI will require pressing A to make
                            another move. If >= 0, AI vs AI will wait this
                            number of frames before making another move. This is
                            because AI takes long to compute moves and delay
                            SAF frames greatly, freezing rendering. */
#define MAX_AI_LEVEL 5

#if defined(SAF_PLATFORM_ARDUBOY)
  #define SIMPLE 1
  #define MAX_AI_LEVEL 2
  #define AI_COUNTDOWN -1
#elif defined(SAF_PLATFORM_POKITTO) || defined(SAF_PLATFORM_GAMEBUINO_META) || defined(SAF_PLATFORM_RINGO) || defined(SAF_PLATFORM_ESPBOY) || defined(SAF_PLATFORM_NIBBLE)
  #define MAX_AI_LEVEL 5
  #define AI_COUNTDOWN 20
  #define SCL_RECORD_MAX_LENGTH 128
#endif

#if defined(SAF_PLATFORM_NIBBLE) || defined(SAF_PLATFORM_ESPBOY)
  #define SCL_CALL_WDT_RESET 1
#endif

#define SAF_SETTING_ENABLE_SAVES 0

#define SAF_PROGRAM_NAME "Chess"

#define SAF_SETTING_FASTER_1BIT 2
#define SAF_SETTING_ENABLE_SOUND 1 
#define SAF_SETTING_ENABLE_SAVES 0

#if SIMPLE
  #define SCL_RECORD_MAX_LENGTH 1
#endif

#include "saf.h"
#include "smallchesslib.h"

#define STATE_MENU 0
#define STATE_PLAYING 1
#define STATE_END 255

#define MENU_ITEMS 5
#define GAME_TYPES 4

#define SQUARE_LIGHT 0xb6
#define SQUARE_DARK  0x92
#define SQUARE_PIECE 0x7a
#define SQUARE_MOVE  0xd1
#define CURSOR_COLOR 0xa1

#if SIMPLE
  #undef CURSOR_COLOR
  #define CURSOR_COLOR SAF_COLOR_BLACK
#endif

uint8_t state = STATE_MENU;
uint8_t menuItem = 0;
uint8_t playerW = 0;
uint8_t playerB = 0;
uint8_t variant = 0;
uint8_t soundOn = 1;
uint8_t visualSelectedSquare = 0;
uint8_t boardFlipped = 0;
int8_t previousMove[2];
int8_t selectedPiece = -1;

int8_t aiCountDown = AI_COUNTDOWN;

SCL_Game game;
SCL_SquareSet allowedSquares;

#define STR(s) s

#if !SIMPLE
SCL_Board variantStartState;
#endif

const
#if SAF_PLATFORM_HARWARD
PROGMEM
#endif
uint8_t images[] =
  {
    // PIECES (0):
    0xff,0xfc,0xed,0xb5,0xe0,0x00, // 0:  white pawn
    0x01,0xe3,0x2d,0xb5,0xe0,0x00, // 1:  white rook
    0xc6,0xe7,0x82,0xda,0xe8,0x00, // 2:  white knight
    0xce,0xd7,0x8c,0xb5,0xe0,0x00, // 3:  white bishop
    0xb5,0x27,0xad,0xb5,0xe0,0x00, // 4:  white queen
    0xfe,0x17,0x9e,0xb6,0xd0,0x00, // 5:  white king
    0xff,0xfc,0xe1,0x84,0x00,0x00, // 6:  black pawn
    0x48,0x00,0x21,0x86,0x10,0x00, // 7:  black rook
    0xce,0x10,0x41,0xc6,0x00,0x00, // 8:  black knight
    0xce,0x10,0x21,0xce,0x10,0x00, // 9:  black bishop
    0xb4,0x00,0x21,0x84,0x00,0x00, // 10: black queen
    0xfe,0x10,0x00,0x86,0x10,0x00, // 11: black king
    // ICONS (12):
    0xcf,0x30,0x12,0xce,0x13,0x00, // 12: human
    0xfc,0x05,0x0a,0x50,0xa0,0x00, // 13: game
    0x84,0xcf,0x39,0xcf,0xfc,0xc0, // 14: question mark
    0xdf,0x3c,0x75,0xdc,0x71,0xc0, // 15: note
    0xdf,0x30,0x40,0x07,0x3d,0xc0, // 16: arrow

#if !SIMPLE
    // MASKS (17):
    0x00,0x03,0x1e,0x7b,0xff,0xc0,
    0xff,0xff,0xde,0x7b,0xff,0xc0,
    0x39,0xff,0xff,0x3d,0xf7,0xc0,
    0x31,0xef,0xff,0x7b,0xff,0xc0,
    0x4b,0xff,0xde,0x7b,0xff,0xc0,
    0x01,0xef,0xff,0x79,0xef,0xc0,
    0x00,0x03,0x1e,0x7b,0xff,0xc0,
    0xb7,0xff,0xde,0x79,0xef,0xc0,
    0x31,0xef,0xbe,0x39,0xff,0xc0,
    0x31,0xef,0xde,0x31,0xef,0xc0,
    0x4b,0xff,0xde,0x7b,0xff,0xc0,
    0x01,0xef,0xff,0x79,0xef,0xc0
#endif
  };

uint8_t image[8];

#if !SIMPLE
uint8_t imageMask[8];
#endif

void drawImage(uint8_t index, uint8_t x, uint8_t y)
{
  const uint8_t *byte = images + index * 6;

  for (uint8_t i = 2; i < 8; ++i, ++byte)
#if SAF_PLATFORM_HARWARD
    image[i] = pgm_read_byte(byte);
#else
    image[i] = *byte;
#endif

  uint8_t mask = 0xff;

#if !SIMPLE
  if (index <= 11)
  {
    byte = images + (index + 17) * 6;
  
    for (uint8_t i = 2; i < 8; ++i, ++byte)
#if SAF_PLATFORM_HARWARD
      imageMask[i] = pgm_read_byte(byte);
#else
      imageMask[i] = *byte;
#endif

    byte = imageMask;
  }
  else
    byte = 0;
#else
  byte = 0;
#endif

  SAF_drawImage1Bit(image,x,y,byte,SAF_COLOR_WHITE,SAF_COLOR_BLACK,SAF_TRANSFORM_NONE);
}

void drawBoard(void)
{
#if SIMPLE
  SAF_clearScreen(SAF_COLOR_WHITE);
#endif

  const char *c = game.board;

  uint8_t x = 0, y = boardFlipped ? 0 : 64 - 8;

  uint8_t square = 0;

  for (uint8_t i = 0; i < 8; ++i)
  {
    x = boardFlipped ? 64 - 8 : 0;
  
    for (uint8_t j = 0; j < 8; ++j)
    {
      char s = *c;

      uint8_t squareColor = SQUARE_LIGHT;

#if SIMPLE
      SAF_drawPixel(x + 4, y + 4,SAF_COLOR_BLACK);
#else
      if (square == selectedPiece || SCL_squareSetContains(allowedSquares,square))
        squareColor = SQUARE_PIECE;
      else if (square == previousMove[0] || square == previousMove[1])
        squareColor = SQUARE_MOVE;
      else if (i % 2 == j % 2)
        squareColor = SQUARE_DARK;

      SAF_drawRect(x,y,8,8,squareColor,1);
#endif

      if (s != '.')
      {
        uint8_t image = 0;

        switch (s)
        {
          case 'R': image = 1; break;
          case 'N': image = 2; break;
          case 'B': image = 3; break;
          case 'Q': image = 4; break;
          case 'K': image = 5; break;
          case 'p': image = 6; break;
          case 'r': image = 7; break;
          case 'n': image = 8; break;
          case 'b': image = 9; break;
          case 'q': image = 10; break;
          case 'k': image = 11; break;
          default: break;
        }

#if SIMPLE
      if (square != selectedPiece || SAF_frame() & 0x04)
#endif
        drawImage(image,x + 1,y);
      }

      c++;

      x += boardFlipped ? -8 : 8;

      square++;
    }

    y += boardFlipped ? 8 : -8;
  }

  if (state == STATE_PLAYING &&(
    (!playerW && SCL_boardWhitesTurn(game.board)) ||
    (!playerB && !SCL_boardWhitesTurn(game.board))))
  {
    // cursor:

    x = (visualSelectedSquare % 8) * 8;
    y = (visualSelectedSquare / 8) * 8;

    SAF_drawRect(x,y,8,8,CURSOR_COLOR,0);
  }

  if (game.state != SCL_GAME_STATE_PLAYING && (SAF_frame() & 0x08))
  {
    SAF_drawRect(32 - 10,32 - 10,20,20,SAF_COLOR_WHITE,1);

#if SIMPLE
    SAF_drawRect(32 - 11,32 - 11,22,22,SAF_COLOR_BLACK,0);
#endif

    char resultStr[4];

    resultStr[0] = '1';
    resultStr[1] = '-';
    resultStr[2] = '1';
    resultStr[3] = 0;

    if (game.state == SCL_GAME_STATE_WHITE_WIN)
      resultStr[2] = '0';
    else if (game.state == SCL_GAME_STATE_BLACK_WIN)
      resultStr[0] = '0';
    else
    {
      resultStr[1] = '/';
      resultStr[2] = '2';
    }

    SAF_drawText(resultStr,24,30,SAF_COLOR_BLACK,1);
  }
}

void drawMenu(void)
{
  SAF_clearScreen(SAF_COLOR_WHITE);

  for (uint8_t j = 0; j < 12; ++j)
    for (uint8_t i = 0; i < SAF_SCREEN_WIDTH; ++i)
      if ((j / 4) % 2 == (i / 4) % 2)
        SAF_drawPixel(i,j,SAF_COLOR_BLACK);
 
#define Y0 15
#define Y 9
#define X 13 
#define X2 24

  uint8_t x = X, y = Y0;

  drawImage(2,x,y);

  x = X2;

  if (playerW == 0)
    drawImage(12,x,y);
  else
    for (uint8_t i = 0; i < playerW; ++i)
      drawImage(0,x + i * 8,y);

  x = X;
  y += Y;

  drawImage(8,x,y);

  x = X2;

  if (playerB == 0)
    drawImage(12,x,y);
  else
    for (uint8_t i = 0; i < playerB; ++i)
      drawImage(6,x + i * 8,y);

  x = X;
  y += Y;

  drawImage(13,x,y);

  x = X2;

  uint8_t variantImgaes[] = {1,14,0,2};
  drawImage(variantImgaes[variant],x,y);

  x = X;
  y += Y;

  drawImage(15,x,y);

  if (!soundOn)
    for (uint8_t i = 0; i < 5; ++i)
      SAF_drawPixel(x + i,y + 2 + i,SAF_COLOR_BLACK);

  y += Y;
  drawImage(16,x,y);

  y = menuItem * Y + 1 + Y0;
  x -= 6;

  SAF_drawPixel(x,y,SAF_COLOR_BLACK);
  x++; y++;
  SAF_drawPixel(x,y,SAF_COLOR_BLACK);
  x++; y++;
  SAF_drawPixel(x,y,SAF_COLOR_BLACK);
  x--; y++;
  SAF_drawPixel(x,y,SAF_COLOR_BLACK);
  x--; y++;
  SAF_drawPixel(x,y,SAF_COLOR_BLACK);

#undef X
#undef Y
#undef Y0
#undef X2
}

uint8_t button(uint8_t button)
{
  return SAF_buttonPressed(button) == 1 || SAF_buttonPressed(button) > 12;
}

void handleMenuItemPlayer(uint8_t *player)
{
  if (button(SAF_BUTTON_RIGHT))
    *player += *player < MAX_AI_LEVEL ? 1 : 0;
  else if (button(SAF_BUTTON_LEFT))
    *player -= *player > 0 ? 1 : 0;
}

void unselectPiece(void)
{
  selectedPiece = -1;
  SCL_squareSetClear(allowedSquares);
}

void trySelect(uint8_t square)
{
  char c = game.board[square];
 
  if (c != '.' && SCL_pieceIsWhite(c) == SCL_boardWhitesTurn(game.board))
  {
    selectedPiece = square;
    SCL_boardGetMoves(game.board,square,allowedSquares);
  }
  else
    unselectPiece();
}

void initGame(void)
{
#if SIMPLE
  SCL_gameInit(&game,NULL);
#else
  switch (variant)
  {
    case 1:
      SCL_boardInit960(variantStartState,SAF_frame() % 960);
      break;

    case 2:
      SCL_boardFromFEN(variantStartState,SCL_FEN_HORDE);
      variantStartState[59] = 'k';
      break;

    case 3:
      SCL_boardFromFEN(variantStartState,SCL_FEN_KNIGHTS);
      break;

    default:
      break;
  }

  SCL_gameInit(&game,variant == 0 ? NULL : variantStartState);
#endif

  SCL_randomSimpleSeed(SAF_frame() % 256);
  previousMove[0] = -1;
  previousMove[1] = -1;
  visualSelectedSquare = SCL_BOARD_SQUARES / 2;
  unselectPiece();
}

void getAIMove(uint8_t *s0, uint8_t *s1, char *p)
{
  uint8_t level = SCL_boardWhitesTurn(game.board) ? playerW : playerB;

  uint8_t depth = level;
  uint8_t extraDepth = level;
  uint8_t randomness = game.ply < 2 ? 1 : 0;

  switch (level)
  {
    case 1: depth = 1; extraDepth = 1; break;
    case 2: depth = 2; extraDepth = 1; break;
    case 3: depth = 2; extraDepth = 2; break;
    case 4: depth = 3; extraDepth = 2; break;
    case 5: depth = 3; extraDepth = 3; break;
    default: break;
  }

  SCL_getAIMove(
    game.board,depth,extraDepth,0,SCL_boardEvaluateStatic,SCL_randomSimple,
    randomness,s0,s1,p);
}

void orientBoard(void)
{
  boardFlipped = !playerB && 
    (
      playerW ||
      (state == STATE_PLAYING && !SCL_boardWhitesTurn(game.board))
    );  
}

void playSound(uint8_t sound)
{
#ifndef SAF_PLATFORM_ARDUBOY
  // sounds are messed up on arduboy due to AI delaying frames
  if (soundOn)
    SAF_playSound(sound);
#endif
}

void SAF_init(void)
{
  image[0] = 6;
  image[1] = 7;

#if !SIMPLE
  imageMask[0] = 6;
  imageMask[1] = 7;
#endif

  initGame();
}

uint8_t SAF_loop(void)
{
  orientBoard();

  switch (state)
  {
    case STATE_MENU:
    {
      if (button(SAF_BUTTON_UP))
        menuItem -= menuItem > 0 ? 1 : 0;
      else if (button(SAF_BUTTON_DOWN))
        menuItem += (menuItem < MENU_ITEMS - 1) ? 1 : 0;

      switch (menuItem)
      {
        case 0: handleMenuItemPlayer(&playerW); break;
        case 1: handleMenuItemPlayer(&playerB); break;

#if !SIMPLE
        case 2: 
          if (button(SAF_BUTTON_RIGHT))
            variant = (variant + 1) % GAME_TYPES;
          else if (button(SAF_BUTTON_LEFT))
            variant = variant > 0 ? (variant - 1) : (GAME_TYPES - 1);

          break;
#endif

        case 3:
          if (button(SAF_BUTTON_RIGHT) || button(SAF_BUTTON_LEFT) ||
            button(SAF_BUTTON_A))
            soundOn = !soundOn;

          break;

        case 4:
          if (SAF_buttonJustPressed(SAF_BUTTON_A))
          {
            initGame();
            state = STATE_PLAYING;
          }

          break;

        default: break;
      }

      if (SAF_buttonJustPressed(SAF_BUTTON_B))
        state = STATE_PLAYING;

      drawMenu();

      break;
    }

    case STATE_PLAYING:
    {
      uint8_t moveFrom = 0, moveTo = 0;
      char movePromote = 'q';

      if (aiCountDown < 0)
      {
        if (SAF_buttonJustPressed(SAF_BUTTON_A))
          aiCountDown = 0;
      }
      else if (aiCountDown > 0)
        aiCountDown--;

      if ((playerW && SCL_boardWhitesTurn(game.board)) ||
        (playerB && !SCL_boardWhitesTurn(game.board)))
      {
        if (playerW == 0 || playerB == 0 || aiCountDown == 0)
        {
          getAIMove(&moveFrom,&moveTo,&movePromote);
          aiCountDown = AI_COUNTDOWN;
        }
      }
      else
      {
        if (button(SAF_BUTTON_RIGHT))
          visualSelectedSquare += visualSelectedSquare % 8 < 7 ? 1 : 0;
        else if (button(SAF_BUTTON_LEFT))
          visualSelectedSquare -= visualSelectedSquare % 8 > 0 ? 1 : 0;
        else if (button(SAF_BUTTON_UP))
          visualSelectedSquare -= visualSelectedSquare > 7 ? 8 : 0;
        else if (button(SAF_BUTTON_DOWN))
          visualSelectedSquare += visualSelectedSquare < 56 ? 8 : 0;

        if (SAF_buttonJustPressed(SAF_BUTTON_A))
        {
          uint8_t selectedSquare = boardFlipped ? 
            ((visualSelectedSquare / 8) * 8 + 7 - visualSelectedSquare % 8) : 
            ((7 - visualSelectedSquare / 8) * 8 + visualSelectedSquare % 8);

          if (selectedPiece < 0)
            trySelect(selectedSquare);
          else
          {
            if (SCL_squareSetContains(allowedSquares,selectedSquare))
            {
              moveFrom = selectedPiece;
              moveTo = selectedSquare;
              unselectPiece();
            }
            else if (selectedSquare == selectedPiece)
              unselectPiece();
            else
              trySelect(selectedSquare);
          }
        }
        else if (SAF_buttonJustPressed(SAF_BUTTON_C))
        {
          SCL_gameUndoMove(&game);

          if (playerW || playerB) // against AI actually undo 2 plys
            SCL_gameUndoMove(&game);

          previousMove[0] = -1;
          previousMove[1] = -1;
          unselectPiece();
        }
      }

      if (moveFrom != 0 || moveTo != 0)
      {
        uint8_t capture = game.board[moveTo] != '.';

        SCL_gameMakeMove(&game,moveFrom,moveTo,movePromote);

        previousMove[0] = moveFrom;
        previousMove[1] = moveTo;

        if (game.state != SCL_GAME_STATE_PLAYING)
        {
          state = STATE_END;
          playSound(SAF_SOUND_BOOM);
          previousMove[0] = -1;
          previousMove[1] = -1;
          unselectPiece();
        }
        else
          playSound(capture ? SAF_SOUND_CLICK : SAF_SOUND_BUMP);
      }

      if (SAF_buttonJustPressed(SAF_BUTTON_B))
        state = STATE_MENU;

      drawBoard();
      break;
    }

    case STATE_END:
    {
      if (SAF_buttonJustPressed(SAF_BUTTON_B))
      {
        initGame();
        state = STATE_MENU;
      }
#if !SIMPLE
      else if (SAF_frame() % 10 == 0)
      {
        game.ply = (game.ply + 1) % (SCL_recordLength(game.record) + 5);

        if (variant == 0)
          SCL_boardInit(game.board);
        else
          SCL_boardCopy(variantStartState,game.board);

        uint16_t i = 0;

        while (i < game.ply && i < SCL_recordLength(game.record))
        {
          uint8_t s0, s1;
          char p;

          SCL_recordGetMove(game.record,i,&s0,&s1,&p);
          SCL_boardMakeMove(game.board,s0,s1,p);
   
          i++;
        }
      }
#endif

      drawBoard();
      break;
    }
  }

  return 1;
}
