/******************************************************************************

  TTcards - Triple Triad remake

Copyright (c) 2013, 2014 Jeffrey Carpenter <i8degrees@gmail.com>
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this
   list of conditions and the following disclaimer.
2. Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

******************************************************************************/
#include "Game.hpp"

// Private headers
#include "types.hpp"
#include "version.hpp"

#include <Rocket/Debugger.h>
#include <nomlib/serializers.hpp>

// Forward declarations
#include "CardCollection.hpp"
#include "CardResourceLoader.hpp"
#include "Board.hpp"
#include "helpers.hpp"
#include "states/GameOverState.hpp"
#include "states/ConfirmationDialogState.hpp"
#include "states/PauseState.hpp"
#include "states/CardsMenuState.hpp"
#include "states/PlayState.hpp"
#include "states/MainMenuState.hpp"

//#include "states/States.hpp" // StateFactory

using namespace nom;

namespace tt {

Game::Game(nom::int32 argc, char* argv[]) :
  // SDLApp( OSX_DISABLE_MINIMIZE_ON_LOSS_FOCUS | OSX_DISABLE_FULLSCREEN_SPACES | INIT_ENGINE_FONTS ),
  SDLApp( OSX_DISABLE_MINIMIZE_ON_LOSS_FOCUS | OSX_DISABLE_FULLSCREEN_SPACES ),
  audio_dev_(nullptr),
  listener_(nullptr),
  cursor_move(nullptr),
  cursor_cancel(nullptr),
  cursor_wrong(nullptr),
  card_place(nullptr),
  card_flip(nullptr),
  load_game_sfx(nullptr),
  save_game_sfx(nullptr),
  theme_track_(nullptr),
  winning_track(nullptr),
  config_(nullptr),
  res_cfg_(nullptr),
  debug_game_(false),
  game(this)
{
  NOM_LOG_TRACE(TTCARDS_LOG_CATEGORY_TRACE);

  std::ostringstream ttcards_version;

  ttcards_version << APP_NAME << " " << "v" << TTCARDS_VERSION_MAJOR << "."
      << TTCARDS_VERSION_MINOR << "." << TTCARDS_VERSION_PATCH << "-d"
      << std::endl;

  //this->state_factory = new States();

  nom::File dir;

  if( nom::init_third_party(0) == false )
  {
    nom::DialogMessageBox(  "Critical Error",
                            "Could not load third party libraries" );
    exit(NOM_EXIT_FAILURE);
  }
  atexit(nom::quit); // Clean up memory associated with nomlib

  nom::init_rand( time(0) );

  #if ! defined( NDEBUG )  // Debug target build
    NOM_LOG_INFO( TTCARDS, "DEBUG build" );

    // Enable info log level and greater for our engine (deprecated)
    nom::SDL2Logger::set_logging_priority( NOM, nom::LogPriority::NOM_LOG_PRIORITY_DEBUG );

    nom::SDL2Logger::set_logging_priority( NOM_LOG_CATEGORY_APPLICATION, nom::LogPriority::NOM_LOG_PRIORITY_INFO );

    // Enable logging of all messages in the game
    nom::SDL2Logger::set_logging_priority( TTCARDS, nom::LogPriority::NOM_LOG_PRIORITY_VERBOSE );

    nom::SDL2Logger::set_logging_priority(  TTCARDS_LOG_CATEGORY_APPLICATION,
                                            LogPriority::NOM_LOG_PRIORITY_DEBUG );

    nom::SDL2Logger::set_logging_priority( TTCARDS_LOG_CATEGORY_INPUT, nom::LogPriority::NOM_LOG_PRIORITY_DEBUG );
    nom::SDL2Logger::set_logging_priority( TTCARDS_LOG_CATEGORY_TEST, nom::LogPriority::NOM_LOG_PRIORITY_INFO );

    // nom::SDL2Logger::set_logging_priority(  TTCARDS_LOG_CATEGORY_EVENTS,
                                            // nom::NOM_LOG_PRIORITY_DEBUG );

    // GameConfig debugging log levels;
    // NOM_LOG_PRIORITY_INFO will log only the file path of the configuration
    // file each time it is loaded
    // NOM_LOG_PRIORITY_DEBUG will log just the output data stream
    // NOM_LOG_PRIORITY_VERBOSE will log both the input and output data streams

    nom::SDL2Logger::set_logging_priority(  TTCARDS_LOG_CATEGORY_CFG,
                                            NOM_LOG_PRIORITY_INFO );

    // Enable logging of game state function traces
    nom::SDL2Logger::set_logging_priority( TTCARDS_LOG_CATEGORY_TRACE_STATES, nom::LogPriority::NOM_LOG_PRIORITY_DEBUG );

    nom::SDL2Logger::set_logging_priority( TTCARDS_LOG_CATEGORY_GUI, nom::LogPriority::NOM_LOG_PRIORITY_DEBUG );

    // Game states debugging (engine scope)
    nom::SDL2Logger::set_logging_priority( NOM_LOG_CATEGORY_SYSTEM, nom::LogPriority::NOM_LOG_PRIORITY_DEBUG );

    // Game states debugging (game scope)
    // nom::SDL2Logger::set_logging_priority( TTCARDS_LOG_CATEGORY_CARDS_MENU_STATE, nom::LogPriority::NOM_LOG_PRIORITY_DEBUG );
    nom::SDL2Logger::set_logging_priority( TTCARDS_LOG_CATEGORY_GAME_OVER_STATE, nom::LogPriority::NOM_LOG_PRIORITY_VERBOSE );

    nom::SDL2Logger::set_logging_priority( TTCARDS_LOG_CATEGORY_CPU_PLAYER, nom::LogPriority::NOM_LOG_PRIORITY_DEBUG );

    nom::SDL2Logger::set_logging_priority(  TTCARDS_LOG_CATEGORY_INIT_GAME_STATE,
                                            NOM_LOG_PRIORITY_DEBUG );

    // Extended diagnostics of the rendering subsystem; i.e.: allocation
    // counters, etc.
    // nom::SDL2Logger::set_logging_priority(NOM_LOG_CATEGORY_RENDER, nom::NOM_LOG_PRIORITY_DEBUG);

    // Animation subsystem
    // nom::SDL2Logger::set_logging_priority(NOM_LOG_CATEGORY_ACTION, nom::NOM_LOG_PRIORITY_DEBUG);
    // nom::SDL2Logger::set_logging_priority(NOM_LOG_CATEGORY_ACTION_QUEUE, nom::NOM_LOG_PRIORITY_DEBUG);
    // nom::SDL2Logger::set_logging_priority(NOM_LOG_CATEGORY_ACTION_PLAYER, nom::NOM_LOG_PRIORITY_DEBUG);

    // Extended call stack diagnostics (i.e.: number of initialized Card objects)
    // nom::SDL2Logger::set_logging_priority( TTCARDS_LOG_CATEGORY_TRACE, nom::LogPriority::NOM_LOG_PRIORITY_VERBOSE );

    // Enable event handler queue debugging statistics
    nom::set_hint(NOM_EVENT_QUEUE_STATISTICS, "1");

  #else // NDEBUG -- release target build

    NOM_LOG_INFO( TTCARDS, "RELEASE build" );

    // Disable logging of all messages but critical log level (these are fatal)
    // under the scope of our engine, nomlib.
    nom::SDL2Logger::set_logging_priority( NOM, nom::LogPriority::NOM_LOG_PRIORITY_CRITICAL );

    // Disable logging of all messages but critical log level (these are fatal)
    // under the scope of the game.
    nom::SDL2Logger::set_logging_priority( TTCARDS, nom::LogPriority::NOM_LOG_PRIORITY_CRITICAL );

  #endif

  NOM_LOG_TRACE( TTCARDS_LOG_CATEGORY_TRACE );

  // Command line arguments
  if ( argc > 1 )
  {
    for ( nom::int32 opt = 1; opt < argc; opt++ )
    {
      // Save cards collection at user defined path
      if ( std::string(argv[opt]) == "--export" )
      {
        CardCollection cards;

        if ( argv[opt + 1] == nullptr )
        {
          NOM_LOG_ERR ( TTCARDS, "Missing parameter <input_filename> for export argument." );
          exit ( NOM_EXIT_FAILURE );
        }
        else if ( dir.exists ( argv[opt + 1] ) == false )
        {
          NOM_LOG_ERR ( TTCARDS, "File path for <input_filename> does not exist at: " + std::string(argv[opt + 1]) );
          exit ( NOM_EXIT_FAILURE );
        }
        else if ( cards.load( argv[opt + 1] ) == false )
        {
          NOM_LOG_ERR ( TTCARDS, "Could not load game cards collection at: " + std::string(argv[opt + 1]) );
          exit ( NOM_EXIT_FAILURE );
        }

        if ( argv[opt + 2] == nullptr )
        {
          NOM_LOG_ERR ( TTCARDS, "Missing parameter <output_filename> for export argument." );
          exit ( NOM_EXIT_FAILURE );
        }
      #if ! defined( NOM_DEBUG ) // Allow overwriting of file I/O if in dev mode
        else if ( dir.exists ( argv[opt + 2] ) == true )
        {
          NOM_LOG_ERR ( TTCARDS, "File path for <output_filename> already exists at: " + std::string(argv[opt + 2]) );
          exit ( NOM_EXIT_FAILURE );
        }
      #endif
        else if ( cards.save( argv[opt + 2] ) == false )
        {
          NOM_LOG_ERR ( TTCARDS, "Could not save the game cards collection at: " + std::string(argv[opt + 2]) );
          exit ( NOM_EXIT_FAILURE );
        }
        else
        {
          NOM_LOG_INFO ( TTCARDS, "Game cards successfully saved at: " + std::string(argv[opt + 2]) );
          exit ( NOM_EXIT_SUCCESS );
        }
      } else if( std::string(argv[opt]) == "-v" || std::string(argv[opt]) == "--version" ) {
        std::cout << ttcards_version.str() << std::endl;
        // TODO: Copyright info
        std::exit(NOM_EXIT_SUCCESS);
      } else if( std::string(argv[opt]) == "-h" || std::string(argv[opt]) == "--help" ) {

        std::cout << APP_NAME << " usage options:" << std::endl;
        std::cout << "[ --export <input_filename> <output_filename> ]" << std::endl;
        std::cout << "[ --config <output_filename> ]" << std::endl;
        std::cout << "[ -h, --help ]" << std::endl;
        std::cout << "[ -v, --version ]" << std::endl;
        std::exit(NOM_EXIT_SUCCESS);
      } // end help option
    } // end for argv[opt] loop
  } // end argc > 1

  working_directory = dir.resource_path();

  // Change the working directory to whatever working_directory has been set to
  //
  // Note that it is important that we do not mess with the working directory
  // path until after our command line arguments have been processed, so that we
  // do not unintentionally mess up relative paths!
  dir.set_path (working_directory);
}

Game::~Game()
{
  NOM_LOG_TRACE( TTCARDS_LOG_CATEGORY_TRACE );

  this->gui_window_.shutdown();
  nom::shutdown_librocket();
}

bool Game::on_init()
{
  nom::Path p;
  nom::File fp;
  std::string fs_root;
  auto& paths = this->game->paths_;

  int render_driver = -1;
  uint32 window_flags = SDL_WINDOW_OPENGL;
  uint32 render_flags = SDL_RENDERER_ACCELERATED;
  bool vsync_hint_result = false;

  // ...Initialize file-system paths...

#if defined(NOM_PLATFORM_OSX)
    fs_root = fp.resource_path() + "/";
#elif defined(NOM_PLATFORM_WINDOWS)
    fs_root = this->working_directory + "\\";
#else
    // Has not been tested!
    fs_root = this->working_directory + "/Resources/";
#endif

  // NOTE: This is used by nom::BMFont interface and libRocket; this sets
  // the game's root directory path by which other file system paths can use as
  // a prefix when dealing with forming a complete file-system path to load from
  // and save to.
  nom::set_file_root(fs_root);

  if( this->init_config_paths() == false ) {
    NOM_LOG_CRIT( TTCARDS_LOG_CATEGORY_APPLICATION,
                  "Could not locate the game configuration files." );
    return false;
  }

  auto cfg_parser = nom::make_unique_json_deserializer();
  if( cfg_parser == nullptr ) {
    NOM_LOG_CRIT( TTCARDS_LOG_CATEGORY_APPLICATION,
                  "Could not load input file: failure to allocate memory!" );
    return false;
  }

  // ...General game logic configuration file...

  this->game->config_ = nom::make_unique<GameConfig>();
  NOM_ASSERT(this->game->config_ != nullptr);
  if( this->game->config_->load_file( paths["ROOT_CONFIG_PATH"],
      cfg_parser.get() ) == false )
  {
    NOM_LOG_CRIT( TTCARDS_LOG_CATEGORY_APPLICATION,
                  "Could not load resource configuration file:",
                  paths["ROOT_CONFIG_PATH"] );
    return false;
  }

  // ...Game assets configuration file for locating resource paths...

#if defined(SCALE_FACTOR) && SCALE_FACTOR == 1
  this->game->res_cfg_ = nom::make_unique<GameConfig>();
  NOM_ASSERT(this->game->res_cfg_ != nullptr);
  if( this->game->res_cfg_->load_file(  paths["LOW_RES_CONFIG_PATH"],
      cfg_parser.get() ) == false )
  {
    NOM_LOG_CRIT( TTCARDS_LOG_CATEGORY_APPLICATION,
                  "Could not load resource configuration file:",
                  paths["LOW_RES_CONFIG_PATH"] );
    return false;
  }
#else
  this->game->res_cfg_ = nom::make_unique<GameConfig>();
  NOM_ASSERT(this->game->res_cfg_ != nullptr);
  if( this->game->res_cfg_->load_file(  paths["HI_RES_CONFIG_PATH"],
      cfg_parser.get() ) == false )
  {
    NOM_LOG_CRIT( TTCARDS_LOG_CATEGORY_APPLICATION,
                  "Could not load resource configuration file:",
                  paths["HI_RES_CONFIG_PATH"] );
    return false;
  }
#endif

  // IMPORTANT: The file-system root prefix must be set before we call the
  // following!
  if( this->init_game_paths( this->game->config_.get() ) == false ) {
    NOM_LOG_CRIT( NOM_LOG_CATEGORY_APPLICATION,
                  "Failed to initialize game paths" );
    return false;
  }

  this->game->debug_game_ =
    this->game->config_->get_bool("DEBUG_GAME", false);

  const std::string RENDER_SCALE_QUALITY =
    this->config_->get_string("RENDER_SCALE_QUALITY", "nearest");
  if( nom::set_hint(  SDL_HINT_RENDER_SCALE_QUALITY,
                      RENDER_SCALE_QUALITY) == false )
  {
    NOM_LOG_INFO( TTCARDS_LOG_CATEGORY_APPLICATION,
                  "Could not set the renderer's scale quality to",
                  RENDER_SCALE_QUALITY );
  }

  const bool ENABLE_VSYNC =
    this->config_->get_bool32("ENABLE_VSYNC", true);
  const std::string ENABLE_VSYNC_STR =
    std::to_string(ENABLE_VSYNC);

  vsync_hint_result =
    nom::set_hint(SDL_HINT_RENDER_VSYNC, ENABLE_VSYNC_STR);
  if( vsync_hint_result == false ) {
    NOM_LOG_WARN( TTCARDS_LOG_CATEGORY_APPLICATION,
                  "Could not enable VSYNC." );
  }

  // We only can support a OpenGL capable rendering driver at the moment; see
  // nom::RocketSDL2RenderInterface.
  render_driver = nom::available_render_driver("opengl");
  if( render_driver == -1 )
  {
    NOM_LOG_CRIT( NOM_LOG_CATEGORY_APPLICATION,
                  "Could not find an OpenGL rendering driver." );
    return false;
  }

  const int VIDEO_DISPLAY_INDEX =
    this->config_->get_int("VIDEO_DISPLAY_INDEX", 0);
  auto window_ret =
    this->window.create(  APP_NAME, RenderWindow::WINDOW_POS_CENTERED,
                          VIDEO_DISPLAY_INDEX, SCREEN_RESOLUTION, window_flags,
                          render_driver, render_flags );
  if( window_ret == false ) {
    nom::DialogMessageBox(  "Critical Error",
                            "Could not initialize rendering context and window." );
    return false;
  }

  const std::string ICON_FILENAME =
    this->config_->get_string("APP_ICON", "icon.png");
  this->window.set_window_icon(ICON_FILENAME);

  // Try to set a sensible (optimal) frame rate based on the display
  // capabilities
  if( ENABLE_VSYNC == true && vsync_hint_result == true ) {

    // Disable frame rate governor; the effective frame rate will be
    // determined by the updating frequency of the display at its native
    // refresh rate -- as per what the underlying rendering driver chooses to
    // use.
    this->max_frame_interval_ = 0;
  } else {
    // ...Not using VSYNC...

    if( this->config_->find("MAX_FRAME_RATE") == false ) {

      auto display_refresh_rate =
        this->window.refresh_rate();
      if( display_refresh_rate > 0 ) {
        // Use the auto-detected refresh rate value
        this->max_frame_interval_ = display_refresh_rate;
      } else {
        NOM_LOG_WARN( TTCARDS_LOG_CATEGORY_APPLICATION,
                      "Could not auto-detect display refresh rate." );
        // Unable to detect the refresh rate; the effective frame rate will be
        // whatever the CPU && GPU is capable of.
        this->max_frame_interval_ = 0;
      }
    } else if( this->config_->find("MAX_FRAME_RATE") == true ) {
      // ...Run at a custom frame rate specified by the end-user...

      if( this->config_->get_int("MAX_FRAME_RATE") < 0 ) {
        NOM_LOG_WARN( TTCARDS_LOG_CATEGORY_APPLICATION,
                      "Could not use the custom frame rate; this value must"
                      "be equal to or greater than zero." );
        // Unable to detect the refresh rate; the effective frame rate will be
        // whatever the CPU && GPU is capable of.
        this->max_frame_interval_ = 0;
      } else {
        this->max_frame_interval_ = this->config_->get_int("MAX_FRAME_RATE");
      }
    }
  }

  this->min_frame_interval_ = this->config_->get_int("MIN_FRAME_RATE", 5);

  if( nom::RocketSDL2RenderInterface::gl_init(  this->window.size().w,
                                                this->window.size().h ) == false )
  {
    nom::DialogMessageBox( "Critical Error", "Could not initialize OpenGL." );
    return false;
  }

  // Allow for automatic rescaling of the output window based on the aspect
  // ratio (i.e.: handle fullscreen resizing); this will use letterboxing
  // when the aspect ratio is greater than what is available, or side-bars
  // when the aspect ratio is less than.
  this->window.set_logical_size( GAME_RESOLUTION.w, GAME_RESOLUTION.h );

  #if defined(SCALE_FACTOR) && SCALE_FACTOR == 1
    this->window.set_scale( nom::Point2f(2,2) );
  #else
    this->window.set_scale( nom::Point2f(1,1) );
  #endif

  // ...Game loading notification...
  this->window.set_window_title("Loading " + APP_WINDOW_TITLE + "...");
  this->window.fill(Color4i::Black);
  this->window.update();

  // Initialize file-system root for libRocket
  Rocket::Core::FileInterface* fs = nullptr;

  // Used by both internal and external libRocket interfaces; decorators,
  // libRocket-rendered fonts and so forth
  fs = new nom::RocketFileInterface( fs_root.c_str() );

  Rocket::Core::SystemInterface* sys =
    new RocketSDL2SystemInterface();

  if( nom::init_librocket( fs, sys ) == false )
  {
    nom::DialogMessageBox( "Critical Error", "Could not initialize libRocket." );
    return false;
  }

  Rocket::Core::RenderInterface* renderer =
    new nom::RocketSDL2RenderInterface( &this->window );

  #if ! defined( NDEBUG )
    this->gui_window_.enable_debugger();
  #endif

  if( this->gui_window_.create_context( "main",
      this->window.size(), renderer ) == false )
  {
    nom::DialogMessageBox( "Critical Error", "Could not initialize GUI context." );
    return false;
  }

  // Resize libRocket's visual debugger window to fit within our game's tiny
  // resolution
  #if ! defined( NDEBUG )

    // Visual debugger is too large when we are dealing with the original game
    // resolution
    #if defined(SCALE_FACTOR) && SCALE_FACTOR == 1
      // 32 is the height of the visual debugger's menu; as per
      // Rocket/Debugger/MenuSource.h
      Size2i debugger_dims( 150, (this->gui_window_.size().h / 2) + 32 );
      this->gui_window_.set_debugger_size(debugger_dims);
    #endif

  #endif

  const auto GUI_TITLE_FONT =
    this->game->res_cfg_->get_string("GUI_TITLE_FONT");
  if( this->gui_window_.load_font(GUI_TITLE_FONT) == false ) {
    NOM_LOG_CRIT( TTCARDS_LOG_CATEGORY_APPLICATION,
                  "Could not load resource file:", GUI_TITLE_FONT );
    return false;
  }

  const auto GUI_TEXT_FONT =
    this->game->res_cfg_->get_string("GUI_TEXT_FONT");
  if( this->gui_window_.load_font(GUI_TEXT_FONT) == false ) {
    NOM_LOG_CRIT( TTCARDS_LOG_CATEGORY_APPLICATION,
                  "Could not load resource file:", GUI_TEXT_FONT );
    return false;
  }

  // ...Initialize the font for the main menu states...
  if( this->game->menu_font_.load(GUI_TEXT_FONT) == false ) {
    NOM_LOG_CRIT( TTCARDS_LOG_CATEGORY_APPLICATION,
                  "Could not load resource file:", GUI_TEXT_FONT );
    return false;
  }

  Rocket::Core::DecoratorInstancer* decorator0 = new nom::DecoratorInstancerFinalFantasyFrame();
  Rocket::Core::Factory::RegisterDecoratorInstancer("final-fantasy-theme", decorator0 );
  decorator0->RemoveReference();

  DecoratorInstancerSprite* decorator1 = new nom::DecoratorInstancerSprite();
  Rocket::Core::Factory::RegisterDecoratorInstancer("sprite-sheet", decorator1 );
  decorator1->RemoveReference();

  const auto WINDOW_FULLSCREEN =
    this->game->config_->get_bool("WINDOW_FULLSCREEN", false);
  if( WINDOW_FULLSCREEN == true ) {
    this->window.toggle_fullscreen();
  }

  this->set_state_machine( new nom::StateMachine() );

  // Commence the loading of game resources

  const auto CARD_FONTFACE =
    this->game->res_cfg_->get_string("CARD_FONTFACE");
  if( this->game->card_font_.load(CARD_FONTFACE) == false ) {
    NOM_LOG_ERR(  TTCARDS_LOG_CATEGORY_APPLICATION,
                  "Could not load resource from file:",
                  CARD_FONTFACE );
    return false;
  }

  const auto BOARD_BACKGROUND =
    this->game->res_cfg_->get_string("BOARD_BACKGROUND");
  if( this->background.load(BOARD_BACKGROUND, false,
      nom::Texture::Access::Static) == false ) {
    NOM_LOG_INFO( TTCARDS_LOG_CATEGORY_APPLICATION,
                  "Could not load resource from file:",
                  BOARD_BACKGROUND );
  return false;
  }

  auto SCORE_FONTFACE =
    this->game->res_cfg_->get_string("SCORE_FONTFACE");
  if( this->scoreboard_font.load(SCORE_FONTFACE) == false ) {
    NOM_LOG_ERR(  TTCARDS_LOG_CATEGORY_APPLICATION,
                  "Could not load resource from file:", SCORE_FONTFACE );
    return false;
  }

  auto GAMEOVER_FONTFACE =
    this->game->res_cfg_->get_string("GAMEOVER_FONTFACE");

  if( this->gameover_font.load(GAMEOVER_FONTFACE) == false ) {
    NOM_LOG_ERR(  TTCARDS_LOG_CATEGORY_APPLICATION,
                  "Could not load resource from file:",
                  GAMEOVER_FONTFACE );
    return false;
  }

  // Set both player's scoreboard fonts
  for( nom::uint32 idx = 0; idx < TOTAL_PLAYERS; ++idx ) {
    this->scoreboard_text[idx].set_font(&this->game->scoreboard_font);
  }

  // Initialize game over text
  this->game->gameover_text.set_font(&this->game->gameover_font);

  // ...Initialize the text for the main menu states...
  this->game->menu_text_.set_font(&this->game->menu_font_);

  const auto GAMEOVER_BACKGROUND =
    this->game->res_cfg_->get_string("GAMEOVER_BACKGROUND");
  if( this->gameover_background.load(GAMEOVER_BACKGROUND, false,
      nom::Texture::Access::Static) == false ) {
    NOM_LOG_ERR(  TTCARDS_LOG_CATEGORY_APPLICATION,
                  "Could not load resource from file:",
                  GAMEOVER_BACKGROUND );
    return false;
  }

  // Initialize interface cursor

  nom::SpriteSheet cursor_frames;
  const auto INTERFACE_CURSOR_ATLAS =
    this->game->res_cfg_->get_string("INTERFACE_CURSOR_ATLAS");

  const auto INTERFACE_CURSOR =
    this->game->res_cfg_->get_string("INTERFACE_CURSOR");

  if( cursor_frames.load_file(INTERFACE_CURSOR_ATLAS) == false ) {
    NOM_LOG_ERR(  TTCARDS_LOG_CATEGORY_APPLICATION,
                  "Could not load sprite sheet from file:",
                  INTERFACE_CURSOR_ATLAS );
    return false;
  }

  if( this->cursor_tex_.load(INTERFACE_CURSOR) == false ) {

    NOM_LOG_ERR(  TTCARDS_LOG_CATEGORY_APPLICATION,
                  "Could not load resource from file:",
                  INTERFACE_CURSOR );
    return false;
  }

  IntRect cursor_bounds(IntRect::zero);

  //

  cursor_bounds = cursor_frames.dimensions(SHEET_CURSOR_NONE);
  this->game->left_cursor_frames_.append_frame(cursor_bounds);

  cursor_bounds = cursor_frames.dimensions(SHEET_CURSOR_LEFT);
  this->game->left_cursor_frames_.append_frame(cursor_bounds);

  //

  cursor_bounds = cursor_frames.dimensions(SHEET_CURSOR_NONE);
  this->game->right_cursor_frames_.append_frame(cursor_bounds);

  cursor_bounds = cursor_frames.dimensions(SHEET_CURSOR_RIGHT);
  this->game->right_cursor_frames_.append_frame(cursor_bounds);

  //

  this->game->cursor_ =
    std::make_shared<SpriteBatch>();
  NOM_ASSERT(this->game->cursor_ != nullptr);
  this->game->cursor_->set_texture(this->game->cursor_tex_);
  this->game->cursor_->set_sprite_sheet(this->game->right_cursor_frames_);
  this->game->cursor_->set_frame(INTERFACE_CURSOR_HIDDEN);

  // blinking cursor animation
  const real32 CURSOR_BLINK_INTERVAL =
    this->game->config_->get_real32("CURSOR_BLINK_INTERVAL", 0.100f);

  auto cursor_action =
    nom::create_action<SpriteBatchAction>(
      this->game->cursor_, CURSOR_BLINK_INTERVAL);
  NOM_ASSERT(cursor_action != nullptr);
  cursor_action->set_name("cursor_action");

  this->game->blinking_cursor_action_ =
    nom::create_action<RepeatForeverAction>(cursor_action);
  NOM_ASSERT(this->game->blinking_cursor_action_ != nullptr);
  this->game->blinking_cursor_action_->set_name("blinking_cursor_action");

  // ...Cards database (decks)...

  this->game->cards_db_[PlayerIndex::PLAYER_1].reset( new CardCollection() );
  this->game->cards_db_[PlayerIndex::PLAYER_2].reset( new CardCollection() );

  if( this->game->cards_db_[PlayerIndex::PLAYER_1] == nullptr ) {
    NOM_LOG_ERR(  TTCARDS_LOG_CATEGORY_APPLICATION,
                  "Could not initialize the player's card database:",
                  "memory allocation failure." );
    return false;
  }

  if( this->game->cards_db_[PlayerIndex::PLAYER_2] == nullptr ) {
    NOM_LOG_ERR(  TTCARDS_LOG_CATEGORY_APPLICATION,
                  "Could not initialize the opponent's card database:",
                  "memory allocation failure." );
    return false;
  }

  this->game->card_res_.reset( new CardResourceLoader() );
  NOM_ASSERT(this->game->card_res_ != nullptr);
  if( this->card_res_->init(  this->game->card_font_,
                              this->game->res_cfg_.get(),
                              paths["EXTRA_CARD_TEXTURE_DIR"] ) == false )
  {
    NOM_LOG_ERR(  TTCARDS_LOG_CATEGORY_APPLICATION,
                  "Could not bootstrap card resources." );
    return false;
  }

  // IMPORTANT: Initializing this more than once leads to a crash within
  // libRocket.
  this->game->cards_page_model_.reset( new CardsPageDataSource("cards_db") );

  nom::SpriteSheet triad_frames;
  nom::Texture* triad_tex = new nom::Texture();
  NOM_ASSERT(triad_tex != nullptr);

  auto TRIAD_SPINNER_ATLAS =
    this->game->res_cfg_->get_string("TRIAD_SPINNER_ATLAS" );
  auto TRIAD_SPINNER =
    this->game->res_cfg_->get_string("TRIAD_SPINNER" );

  if( triad_frames.load_file(TRIAD_SPINNER_ATLAS) == false ) {
    NOM_LOG_ERR(  TTCARDS_LOG_CATEGORY_APPLICATION,
                  "Could not load sprite sheet from file:",
                  TRIAD_SPINNER_ATLAS );
    return false;
  }

  if( triad_tex->load(TRIAD_SPINNER) == false ) {
    NOM_LOG_ERR(  TTCARDS_LOG_CATEGORY_APPLICATION,
                  "Could not load texture from file:", TRIAD_SPINNER );
    return false;
  }

  auto triad_start_frame =
    this->game->config_->get_int("TRIAD_START_FRAME");
  if( triad_start_frame < 0 ) {
    triad_start_frame = 0;
  }

  this->game->triad_ =
    std::make_shared<SpriteBatch>();
  NOM_ASSERT(this->game->triad_ != nullptr);

  this->game->triad_->set_texture(triad_tex);
  this->game->triad_->set_sprite_sheet(triad_frames);
  this->game->triad_->set_frame(triad_start_frame);

  // actions init

  // blinking cursor animation
  const real32 TRIAD_FRAME_INTERVAL =
    this->game->config_->get_real32("TRIAD_FRAME_INTERVAL", 0.100f);

  auto triad_action =
    nom::create_action<SpriteBatchAction>(
      this->game->triad_, TRIAD_FRAME_INTERVAL);
  NOM_ASSERT(triad_action != nullptr);

  this->game->triad_action_ =
    nom::create_action<RepeatForeverAction>(triad_action);
  NOM_ASSERT(this->game->triad_action_ != nullptr);
  this->game->triad_action_->set_name("triad_action");

  // ...win, lose, draw text actions...

  this->game->won_text_sprite_ =
    tt::generate_text_sprite(this->game->gameover_text, "You Win!");
  NOM_ASSERT(this->game->won_text_sprite_ != nullptr);
  NOM_ASSERT(this->game->won_text_sprite_->valid() == true);
  this->game->won_text_sprite_->set_alpha(Color4i::ALPHA_TRANSPARENT);
  nom::set_alignment( this->game->won_text_sprite_.get(), Point2i::zero,
                      GAME_RESOLUTION, Anchor::MiddleCenter );

  this->game->lost_text_sprite_ =
    tt::generate_text_sprite(this->game->gameover_text, "You Lose...");
  NOM_ASSERT(this->game->lost_text_sprite_ != nullptr);
  NOM_ASSERT(this->game->lost_text_sprite_->valid() == true);
  this->game->lost_text_sprite_->set_alpha(Color4i::ALPHA_TRANSPARENT);
  nom::set_alignment( this->game->lost_text_sprite_.get(), Point2i::zero,
                      GAME_RESOLUTION, Anchor::MiddleCenter );

  this->game->tied_text_sprite_ =
    tt::generate_text_sprite(this->game->gameover_text, "Draw");
  NOM_ASSERT(this->game->tied_text_sprite_ != nullptr);
  NOM_ASSERT(this->game->tied_text_sprite_->valid() == true);
  this->game->tied_text_sprite_->set_alpha(Color4i::ALPHA_TRANSPARENT);
  nom::set_alignment( this->game->tied_text_sprite_.get(), Point2i::zero,
                      GAME_RESOLUTION, Anchor::MiddleCenter );

  this->game->combo_text_sprite_ =
    tt::generate_text_sprite(this->game->gameover_text, "Combo!");
  NOM_ASSERT(this->game->combo_text_sprite_ != nullptr);
  NOM_ASSERT(this->game->combo_text_sprite_->valid() == true);
  nom::set_alignment( this->game->combo_text_sprite_.get(), Point2i::zero,
                      GAME_RESOLUTION, Anchor::MiddleRight );

  this->game->same_text_sprite_ =
    tt::generate_text_sprite(this->game->gameover_text, "Same!");
  NOM_ASSERT(this->game->same_text_sprite_ != nullptr);
  NOM_ASSERT(this->game->same_text_sprite_->valid() == true);
  nom::set_alignment( this->game->same_text_sprite_.get(), Point2i::zero,
                      GAME_RESOLUTION, Anchor::MiddleRight );

  // ...Initialize default game window fade out animation...

  this->game->fade_window_sprite_ = std::make_shared<Sprite>();
  if( this->game->fade_window_sprite_ != nullptr ) {
    this->game->fade_window_sprite_->init_with_color( Color4i::Black,
                                                      SCREEN_RESOLUTION );
    // this->game->fade_window_sprite_->set_color_blend_mode(BlendMode::BLEND_MODE_BLEND);
    // this->game->fade_window_sprite_->set_alpha(Color4i::ALPHA_TRANSPARENT);
    // this->game->fade_window_sprite_->set_position(Point2i::zero);
  }

  // Initialize audio subsystem...
  if( this->game->config_->get_bool("AUDIO_SFX", true) ||
      this->game->config_->get_bool("AUDIO_TRACKS", true) ) {

    #if defined(NOM_USE_OPENAL)
      this->audio_dev_.reset( new nom::AudioDevice() );
      this->listener_.reset( new nom::Listener() );

      for( auto idx = 0; idx != NUM_SOUND_BUFFERS; ++idx ) {
        this->sound_buffers[idx].reset( new nom::SoundBuffer() );
      }
    #endif // defined NOM_USE_OPENAL
  }
  else {
    this->audio_dev_.reset( new nom::NullAudioDevice() );
    this->listener_.reset( new nom::NullListener() );

    for( auto idx = 0; idx != NUM_SOUND_BUFFERS; ++idx ) {
      this->sound_buffers[idx].reset( new nom::NullSoundBuffer() );
    }
  }

  // Load audio resources
  if( this->game->config_->get_bool("AUDIO_SFX", true) ) {

    if ( this->sound_buffers[0]->load( this->config_->get_string("CURSOR_MOVE") ) == false )
    {
      NOM_LOG_INFO ( TTCARDS, "Could not load resource file: " + this->config_->get_string("CURSOR_MOVE") );
    }

    if ( this->sound_buffers[1]->load( this->config_->get_string("CURSOR_CANCEL") ) == false )
    {
      NOM_LOG_INFO ( TTCARDS, "Could not load resource file: " + this->config_->get_string("CURSOR_CANCEL") );
    }

    if ( this->sound_buffers[2]->load( this->config_->get_string("CURSOR_WRONG") ) == false )
    {
      NOM_LOG_INFO ( TTCARDS, "Could not load resource file: " + this->config_->get_string("CURSOR_WRONG") );
    }

    if ( this->sound_buffers[3]->load( this->config_->get_string("CARD_PLACE") ) == false )
    {
      NOM_LOG_INFO ( TTCARDS, "Could not load resource file: " + this->config_->get_string("CARD_PLACE") );
    }

    if ( this->sound_buffers[4]->load( this->config_->get_string("CARD_FLIP") ) == false )
    {
      NOM_LOG_INFO ( TTCARDS, "Could not load resource file: " + this->config_->get_string("CARD_FLIP") );
    }

    if ( this->sound_buffers[5]->load( this->config_->get_string("SFX_LOAD_GAME") ) == false )
    {
      NOM_LOG_INFO ( TTCARDS, "Could not load resource file: " + this->config_->get_string("SFX_LOAD_GAME") );
    }

    if ( this->sound_buffers[6]->load( this->config_->get_string("SFX_SAVE_GAME") ) == false )
    {
      NOM_LOG_INFO ( TTCARDS, "Could not load resource file: " + this->config_->get_string("SFX_SAVE_GAME") );
    }

#if defined(NOM_USE_OPENAL)
    this->cursor_move.reset( new nom::Sound() );
    this->cursor_cancel.reset( new nom::Sound() );
    this->cursor_wrong.reset( new nom::Sound() );
    this->card_place.reset( new nom::Sound() );
    this->card_flip.reset( new nom::Sound() );
    this->load_game_sfx.reset( new nom::Sound() );
    this->save_game_sfx.reset( new nom::Sound() );
#endif // defined NOM_USE_OPENAL

    this->cursor_move->setBuffer( *this->sound_buffers[0] );
    this->cursor_cancel->setBuffer( *this->sound_buffers[1] );
    this->cursor_wrong->setBuffer( *this->sound_buffers[2] );
    this->card_place->setBuffer( *this->sound_buffers[3] );
    this->card_flip->setBuffer( *this->sound_buffers[4] );
    this->load_game_sfx->setBuffer( *this->sound_buffers[5] );
    this->save_game_sfx->setBuffer( *this->sound_buffers[6] );
  } else {
    this->cursor_move.reset( new nom::NullSound() );
    this->cursor_cancel.reset( new nom::NullSound() );
    this->cursor_wrong.reset( new nom::NullSound() );
    this->card_place.reset( new nom::NullSound() );
    this->card_flip.reset( new nom::NullSound() );
    this->load_game_sfx.reset( new nom::NullSound() );
    this->save_game_sfx.reset( new nom::NullSound() );
  }

  if( this->game->config_->get_bool("AUDIO_TRACKS", true) ) {

    const std::string MUSIC_THEME_TRACK =
      this->config_->get_string("MUSIC_THEME_TRACK");
    const std::string MUSIC_WIN_TRACK =
      this->config_->get_string("MUSIC_WIN_TRACK");

    if( this->sound_buffers[7]->load(MUSIC_THEME_TRACK) == false ) {
      NOM_LOG_INFO( TTCARDS_LOG_CATEGORY_APPLICATION,
                    "Could not load resource file: ", MUSIC_THEME_TRACK );
    }

    if( this->sound_buffers[8]->load(MUSIC_WIN_TRACK) == false ) {
      NOM_LOG_INFO( TTCARDS_LOG_CATEGORY_APPLICATION,
                    "Could not load resource file: ", MUSIC_WIN_TRACK );
    }

    #if defined(NOM_USE_OPENAL)
      this->theme_track_.reset( new nom::Music() );
      this->winning_track.reset( new nom::Music() );
    #endif // defined NOM_USE_OPENAL

    this->theme_track_->setBuffer( *this->sound_buffers[7] );
    this->winning_track->setBuffer( *this->sound_buffers[8] );
  } // end if AUDIO_TRACKS
  else {
    this->theme_track_.reset( new nom::NullMusic() );
    this->winning_track.reset( new nom::NullMusic() );
  }

  // Set default audio volume level
  const auto AUDIO_VOLUME =
    this->game->config_->get_real32("AUDIO_VOLUME", 50.0f);
  this->game->set_volume(AUDIO_VOLUME);

  // ...Initialize our global input action bindings...

  // TODO: Most all of the input action bindings here should be re-initialized
  // at the start of a new game!

  this->game->input_mapper.clear();
  nom::InputActionMapper state;

  auto quit_game( [=](const nom::Event& evt) {
    this->on_game_quit(evt);
  });

  auto toggle_fullscreen( [=](const nom::Event& evt) {
    this->window.toggle_fullscreen();
  });

  auto fps_counter( [=](const nom::Event& evt) {
    this->game->toggle_fps();
  });

  auto pause_music( [=](const nom::Event& evt) {
    this->game->pause_music();
  });

  auto mute_volume( [=](const nom::Event& evt) {
    this->game->mute_volume();
  });

  auto increase_volume( [=](const nom::Event& evt) {

    const real32 GAIN_STEP = 5.0f;
    const real32 MAX_VOLUME_LEVEL = nom::Listener::max_volume();
    auto curr_volume = this->game->listener_->volume();

    if( curr_volume < (MAX_VOLUME_LEVEL - 1.0f) ) {
      auto gain =
        nom::round_float<real32>(curr_volume + GAIN_STEP);
      this->game->set_volume(gain);
    }
  });

  auto decrease_volume( [=](const nom::Event& evt) {

    const real32 GAIN_STEP = 5.0f;
    const real32 MIN_VOLUME_LEVEL = nom::Listener::min_volume();
    auto curr_volume = this->game->listener_->volume();

    if( curr_volume > MIN_VOLUME_LEVEL ) {
      auto gain =
        nom::round_float<real32>(curr_volume - GAIN_STEP);
      this->game->set_volume(gain);
    }
  });

  auto save_screenshot( [=](const nom::Event& evt) {
    this->game->save_screenshot();
  });

  auto reload_config( [=](const nom::Event& evt) {
    this->game->reload_config();
  });

  auto platform_key_mod = KMOD_LCTRL;

  if( this->game->debug_game_ == true ) {
    auto jumpto_gameover_state( [=](const nom::Event& evt) {
      auto player1_win = new nom::uint32(GameOverType::Won);
      this->set_state(Game::State::GameOver, player1_win);
    });

    auto dump_board( [=](const nom::Event& evt) {
      this->game->dump_board();
    });

    auto dump_player1_hand( [=](const nom::Event& evt) {
      this->game->dump_hand(PlayerIndex::PLAYER_1);
    });

    auto dump_player2_hand( [=](const nom::Event& evt) {
      this->game->dump_hand(PlayerIndex::PLAYER_2);
    });

    auto dump_player1_collection( [=](const nom::Event& evt) {
      this->game->dump_collection(PlayerIndex::PLAYER_1);
    });

    auto dump_player2_collection( [=](const nom::Event& evt) {
      this->game->dump_collection(PlayerIndex::PLAYER_2);
    });

    state.insert( "jumpto_gameover_state",
                  nom::KeyboardAction(SDLK_0), jumpto_gameover_state );

    state.insert( "dump_board",
                  nom::KeyboardAction(SDLK_BACKSPACE, platform_key_mod),
                  dump_board );

    state.insert( "dump_player1_hand",
                  nom::KeyboardAction(SDLK_LEFTBRACKET), dump_player1_hand );

    state.insert( "dump_player2_hand",
                  nom::KeyboardAction(SDLK_RIGHTBRACKET), dump_player2_hand );

    state.insert( "dump_player1_collection",
                  nom::KeyboardAction(SDLK_LEFTBRACKET, platform_key_mod),
                  dump_player1_collection );

    state.insert( "dump_player2_collection",
                  nom::KeyboardAction(SDLK_RIGHTBRACKET, platform_key_mod),
                  dump_player2_collection );
  } // end if DEBUG_GAME

  state.insert("quit_game", nom::KeyboardAction(SDLK_q), quit_game);
  state.insert( "toggle_fullscreen", nom::KeyboardAction(SDLK_f, platform_key_mod),
                toggle_fullscreen );
  state.insert( "fps_counter", nom::KeyboardAction(SDLK_BACKSLASH),
                fps_counter );
  state.insert( "pause_music", nom::KeyboardAction(SDLK_m, KMOD_LSHIFT),
                pause_music );
  state.insert("mute_volume", nom::KeyboardAction(SDLK_m), mute_volume);
  state.insert("increase_volume", nom::KeyboardAction(SDLK_PERIOD),
               increase_volume);
  state.insert("decrease_volume", nom::KeyboardAction(SDLK_COMMA),
               decrease_volume);
  state.insert( "save_screenshot", nom::KeyboardAction(SDLK_F1),
                save_screenshot );
  state.insert("reload_config", nom::KeyboardAction(SDLK_r), reload_config);

  this->game->input_mapper.insert( "Game", state, true );

  if( this->evt_handler_.enable_game_controller_polling() == false ) {
    NOM_LOG_ERR(  TTCARDS_LOG_CATEGORY_APPLICATION,
                  "Could not initialize joystick subsystem: ",
                  nom::error() );
  }

  SDLApp::set_event_handler(this->evt_handler_);
  this->input_mapper.set_event_handler(this->evt_handler_);
  this->game->gui_window_.set_event_handler(this->evt_handler_);

  this->game->set_state(Game::State::MainMenu);

  // Core game initialization is finished!
  this->window.set_window_title(APP_WINDOW_TITLE);

  return true;
}

int Game::Run()
{
  real32 delta_time = 0.0f;
  real32 elapsed_frames = 0.0f;
  Timer game_time;
  std::stringstream fps_title_prefix;
  std::stringstream fps_title_suffix;

  game_time.start();
  this->fps_timer_.start();
  fps_title_prefix << APP_WINDOW_TITLE << " - ";

  nom::Event evt;
  while( this->running() == true ) {

    delta_time = game_time.ticks();

    while( this->evt_handler_.poll_event(evt) == true ) {
      // NOTE: Pending events will be handled by the event listeners that were
      // given an EventHandler object via ::set_event_handler
    }

    this->on_update(delta_time);
    this->game->actions_.update(delta_time);

    // Fix for GitHub Issue #9
    this->window.fill(nom::Color4i::Black);

    this->on_draw(this->window);

    TT_RENDER_ACTION( this->game->fade_window_sprite_,
                      "fade_window_action" );

    ++elapsed_frames;
    if( this->show_fps() == true ) {

      if( this->fps_timer_.ticks() >= 1000 ) {
        real32 frame_interval = elapsed_frames;

        fps_title_suffix.str("");
        fps_title_suffix.precision(1);
        fps_title_suffix  << fps_title_prefix.str()
                          << std::fixed << frame_interval << " " << "FPS";
        this->window.set_window_title( fps_title_suffix.str() );
        this->fps_timer_.restart();
        elapsed_frames = 0;
      }
    } else {
      // Use the default window title string
      this->window.set_window_title(APP_WINDOW_TITLE);
    }

    if( this->fps_timer_.started() == true ) {
      // Update and render at full speed
      tt::set_frame_interval(this->max_frame_interval_);
    } else if( this->fps_timer_.started() == false ) {
      // Game window is not active, so we can free up what is otherwise wasted
      // CPU cycles for end-user multi-tasking
      tt::set_frame_interval(this->min_frame_interval_);
    }

  } // end while game is running

  return NOM_EXIT_SUCCESS;
}

void Game::set_state(nom::uint32 id, nom::void_ptr data)
{
  switch(id)
  {
    default:
    {
      NOM_ASSERT_INVALID_PATH();
    } break;

    case Game::State::CardsMenu:
    {
      this->state()->set_state( CardsMenuStatePtr( new CardsMenuState( this->game ) ), data );
    } break;

    case Game::State::Play:
    {
      this->state()->set_state( PlayStatePtr( new PlayState( this->game ) ), data );
    } break;

    case Game::State::GameOver:
    {
      this->state()->set_state( GameOverStatePtr( new GameOverState( this->game, data ) ), data );
    } break;

    case Game::State::Pause:
    {
      this->state()->set_state( PauseStatePtr( new PauseState( this->game ) ), data );
    } break;

    case Game::State::ConfirmationDialog:
    {
      this->state()->set_state( ConfirmationDialogStatePtr( new ConfirmationDialogState( this->game ) ), data );
    } break;

    case Game::State::MainMenu:
    {
      auto state_ptr =
        MainMenuStatePtr( new MainMenuState(this->game) );

      this->state()->set_state( std::move(state_ptr), data );
    } break;
  }
}

bool Game::
init_game_rules(const GameConfig* config, tt::RegionRuleSet& region)
{
  if( config == nullptr ) {
    NOM_LOG_DEBUG(  TTCARDS_LOG_CATEGORY_APPLICATION,
                    "Could not initialize game rules; config was NULL." );
    return false;
  }

  tt::clear_card_rulesets(&region);

  tt::string_list region_cfg =
    config->get_array("REGION_RULESET");

  NOM_LOG_DEBUG(  TTCARDS_LOG_CATEGORY_INIT_GAME_STATE,
                  "Initializing game rules from existing configuration file..." );

  for( auto itr = region_cfg.begin(); itr != region_cfg.end(); ++itr ) {

    uint32 ruleset = CardRuleset::NO_RULESET;
    if( (*itr) == "Open" ) {

      NOM_LOG_DEBUG(  TTCARDS_LOG_CATEGORY_INIT_GAME_STATE,
                      "Setting up game rule OPEN_RULESET" );
      ruleset = CardRuleset::OPEN_RULESET;

    } else if( (*itr) == "Combo" ) {

      NOM_LOG_DEBUG(  TTCARDS_LOG_CATEGORY_INIT_GAME_STATE,
                      "Setting up game rule COMBO_RULESET" );
      ruleset = CardRuleset::COMBO_RULESET;

    } else if( (*itr) == "Elemental" ) {

      NOM_LOG_DEBUG(  TTCARDS_LOG_CATEGORY_INIT_GAME_STATE,
                      "Setting up game rule ELEMENTAL_RULESET" );
      ruleset = CardRuleset::ELEMENTAL_RULESET;

    } else if( (*itr) == "Same" ) {

      NOM_LOG_DEBUG(  TTCARDS_LOG_CATEGORY_INIT_GAME_STATE,
                      "Setting up game rule SAME_RULESET" );
      ruleset = CardRuleset::SAME_RULESET;
    }

    tt::append_card_ruleset(&region, ruleset);
  }

  return true;
}

void
Game::fade_window(  real32 duration, const nom::Color4i& color,
                    nom::uint8 alpha, const nom::Size2i& window_dims,
                    const std::function<void()>& on_completion_func )
{
  bool ret = false; // log action execution failures

  auto sp = this->game->fade_window_sprite_;

  if( sp != nullptr ) {

    auto window_color = sp->color();
    auto window_dims = sp->size();

    sp->init_with_color(color, window_dims);
    sp->set_position(Point2i::zero);
    sp->set_color(color);
    sp->set_color_blend_mode(BlendMode::BLEND_MODE_BLEND);
    sp->set_alpha(alpha);
  }

  auto fade_out_action =
    nom::create_action<FadeInAction>(sp, duration);
  if( fade_out_action != nullptr ) {
    fade_out_action->set_name("fade_window_action");
  }

  ret = this->game->actions_.run_action(fade_out_action, on_completion_func);

  if( ret == false ) {
    NOM_LOG_ERR( TTCARDS_LOG_CATEGORY_APPLICATION, "Animation failure: ",
                  fade_out_action->name() );

  }
}

void Game::on_game_quit(const nom::Event& evt)
{
  this->on_app_quit(evt);
}

std::shared_ptr<nom::IActionObject> Game::
create_flip_card_action(  const std::shared_ptr<nom::Sprite>& sp,
                          const nom::Point2i& card_pos )
{
  const real32 FLIP_CARD_FLASH_DURATION =
    this->game->config_->get_real32("FLIP_CARD_FLASH_DURATION", 0.100f);
  const Color4i FLIP_CARD_FLASH_COLOR = Color4i::White;

  this->game->actions_.cancel_action("flip_card_action");

  if( sp != nullptr ) {
    sp->set_position(card_pos);
    sp->set_alpha(Color4i::ALPHA_TRANSPARENT);
  }

  auto flip_card_action =
    nom::create_action<FadeInAction>(sp, FLIP_CARD_FLASH_DURATION);
  if( flip_card_action != nullptr ) {
    flip_card_action->set_name("flip_card_action");
  }

  return flip_card_action;
}

bool Game::save_deck(CardCollection* deck, const std::string& filename)
{
  nom::Value game;

  auto fp = nom::make_unique_json_serializer();
  if( fp == nullptr ) {
    NOM_LOG_ERR(  TTCARDS,
                  "Could not open input file: failure to allocate memory!" );
    return false;
  }

  if( deck != nullptr ) {
    // ...Arrays enclosed in an object...
    game["cards"] = tt::serialize_deck(deck);

    if( fp->save(game, filename) == false ) {
      NOM_LOG_ERR(  TTCARDS_LOG_CATEGORY_APPLICATION,
                    "Unable to save game at:", filename );
      return false;
    }
  }

  NOM_LOG_DEBUG(  TTCARDS_LOG_CATEGORY_APPLICATION,
                  "Game saved at:", filename );

  // Success!
  return true;
}

bool Game::load_new_deck(CardCollection* deck, const std::string& filename)
{
  Cards cards;
  nom::Value game;

  auto fp = nom::make_unique_json_deserializer();
  if( fp == nullptr ) {
    NOM_LOG_ERR(  TTCARDS_LOG_CATEGORY_APPLICATION,
                  "Could not open input file: failure to allocate memory!" );
    return false;
  }

  if( fp->load(filename, game) == false ) {
    NOM_LOG_ERR(  TTCARDS_LOG_CATEGORY_APPLICATION,
                  "Unable to load game from:", filename );
    return false;
  }

  NOM_LOG_DEBUG(  TTCARDS_LOG_CATEGORY_APPLICATION,
                  "Game loaded from:", filename );

  if( deck != nullptr ) {
    // ...Arrays enclosed in an object...
    cards = tt::deserialize_deck(game["basic"]);

    deck->clear();
    deck->append_cards(cards);
  }

  // Success!
  return true;
}

bool Game::load_deck(CardCollection* deck, const std::string& filename)
{
  Cards cards;
  nom::Value game;

  auto fp = nom::make_unique_json_deserializer();
  if( fp == nullptr ) {
    NOM_LOG_ERR(  TTCARDS_LOG_CATEGORY_APPLICATION,
                  "Could not open input file: failure to allocate memory!" );
    return false;
  }

  if( fp->load(filename, game) == false ) {
    NOM_LOG_ERR(  TTCARDS_LOG_CATEGORY_APPLICATION,
                  "Unable to load game from:", filename );
    return false;
  }

  NOM_LOG_DEBUG(  TTCARDS_LOG_CATEGORY_APPLICATION,
                  "Game loaded from:", filename );

  // ...Arrays enclosed in an object...
  if( deck != nullptr ) {
    cards = tt::deserialize_deck(game["cards"]);

    deck->clear();
    deck->append_cards(cards);
  }

  // Success!
  return true;
}

bool
Game::save_player_hand( Board* board, CardHand* p1_hand, CardHand* p2_hand,
                        bool game_state, const std::string& filename )
{
  nom::Value game;

  auto fp = nom::make_unique_json_serializer();
  if( fp == nullptr ) {
    NOM_LOG_ERR(  TTCARDS,
                  "Could not open input file: failure to allocate memory!" );
    return false;
  }

  // ...Arrays enclosed in an object...
  if( board != nullptr ) {
    game["board"] = tt::serialize_board(board);
  }

  if( p1_hand != nullptr ) {
    game["player"]["hand"] = tt::serialize_hand(p1_hand);
  }

  if( p2_hand != nullptr ) {
    game["opponent"]["hand"] = tt::serialize_hand(p2_hand);
  }

  if( game_state == true ) {
    game["state"]["cursor_pos"] = this->game->cursor_pos_;
  }

  if( fp->save(game, filename) == false ) {
    NOM_LOG_ERR(  TTCARDS_LOG_CATEGORY_APPLICATION,
                  "Unable to save game at:", filename );
    return false;
  }

  NOM_LOG_DEBUG(  TTCARDS_LOG_CATEGORY_APPLICATION,
                  "Game saved at:", filename );

  // Success!
  return true;
}

bool
Game::load_player_hand( Board* board, CardHand* p1_hand, CardHand* p2_hand,
                        bool game_state, const std::string& filename )
{
  Cards board_cards;
  Cards p1_cards, p2_cards;
  nom::Value game;

  auto fp = nom::make_unique_json_deserializer();
  if( fp == nullptr ) {
    NOM_LOG_ERR(  TTCARDS_LOG_CATEGORY_APPLICATION,
                  "Could not open input file: failure to allocate memory!" );
    return false;
  }

  if( fp->load(filename, game) == false ) {
    NOM_LOG_ERR(  TTCARDS_LOG_CATEGORY_APPLICATION,
                  "Unable to load game from:", filename );
    return false;
  }

  NOM_LOG_DEBUG(  TTCARDS_LOG_CATEGORY_APPLICATION,
                  "Game loaded from:", filename );

  // ...Arrays enclosed in an object...
  if( board != nullptr ) {
    board_cards = tt::deserialize_board(game["board"]);

    board->clear();
    board->update(board_cards);
  }

  if( p1_hand != nullptr ) {
    p1_cards =
      tt::deserialize_hand(p1_hand->player_id(), game["player"]["hand"]);
    p1_hand->clear();
    p1_hand->push_back(p1_cards);
  }

  if( p2_hand != nullptr ) {
    p2_cards =
      tt::deserialize_hand(p2_hand->player_id(), game["opponent"]["hand"]);
    p2_hand->clear();
    p2_hand->push_back(p2_cards);
  }

  if( game_state == true ) {
    this->game->cursor_pos_ = game["state"]["cursor_pos"].get_int();
  }

  // Success!
  return true;
}

bool Game::player_deck_exists() const
{
  File fp;
  bool result = false;

  const auto PLAYER_DECK_PATH =
    this->game->paths_["PLAYER_DECK_PATH"];
  result = fp.exists(PLAYER_DECK_PATH);

  return result;
}

bool Game::opponent_deck_exists() const
{
  File fp;
  bool result = false;

  const auto OPPONENT_DECK_PATH =
    this->game->paths_["OPPONENT_DECK_PATH"];
  result = fp.exists(OPPONENT_DECK_PATH);

  return result;
}

void Game::dump_board()
{
  if( this->game->board_ != nullptr ) {
    this->game->board_->dump_values();
  }
}

void Game::dump_hand(PlayerIndex player_index)
{
  auto& phand = this->game->hand[player_index];
  auto player_id = tt::player_id(player_index);

  NOM_LOG_INFO(TTCARDS_LOG_CATEGORY_APPLICATION, "Player", player_id, phand);
}

void Game::dump_collection(PlayerIndex player_index)
{
  auto db = this->game->cards_db_[player_index].get();
  if( db != nullptr ) {

    auto player_id = tt::player_id(player_index);

    NOM_LOG_INFO( TTCARDS_LOG_CATEGORY_APPLICATION,
                  "Player", player_id, "\n\tdeck:", db->size() );

    // IMPORTANT: We exceed the maximal logging output size of ~4KB imposed by
    // SDL's logging facilities if we do not break up the data dump of a
    // player's full card deck
    for( auto itr = db->begin(); itr != db->end(); ++itr ) {
      NOM_LOG_INFO(TTCARDS_LOG_CATEGORY_APPLICATION, *itr);
    }
  }
}

bool Game::init_config_paths()
{
  nom::File fp;
  nom::Path p;
  auto& paths = this->game->paths_;

  const std::string ROOT_CONFIG_FILENAME = "config_game.json";
  const std::string HI_RES_CONFIG_FILENAME = "config_assets-hi-res.json";
  const std::string LOW_RES_CONFIG_FILENAME = "config_assets-low-res.json";

  // NOTE: Root configuration files
  paths["CFG_DIR"] =
    fp.user_app_support_path() + p.native() + APP_NAME + p.native();

  if( fp.exists(paths["CFG_DIR"]) == false ) {
    return false;
  }

  paths["ROOT_CONFIG_PATH"] = paths["CFG_DIR"] + ROOT_CONFIG_FILENAME;
  paths["HI_RES_CONFIG_PATH"] = paths["CFG_DIR"] + HI_RES_CONFIG_FILENAME;
  paths["LOW_RES_CONFIG_PATH"] = paths["CFG_DIR"] + LOW_RES_CONFIG_FILENAME;

  return true;
}

bool Game::init_game_paths(GameConfig* cfg)
{
  nom::File fp;
  nom::Path p;
  auto& paths = this->game->paths_;

  NOM_ASSERT(cfg != nullptr);
  if( cfg == nullptr ) {
    return false;
  }

  const std::string DEBUG_DATA_DIR = "Debug";
  const std::string SAVE_SCREENSHOT_DIR = "Screenshots";

  const std::string SAVE_GAME_DIR =
    cfg->get_string("SAVE_GAME_DIR", "SavedGames");

  const std::string SAVE_GAME_DECK_DIR =
    cfg->get_string("SAVE_GAME_DECK_DIR", "Decks");

  const std::string CARDS_DB_FILENAME =
    cfg->get_string("CARDS_DB_FILENAME", "cards.json");

  const std::string SAVE_GAME_FILENAME =
    cfg->get_string("SAVE_GAME_FILENAME", "game.json");

  const std::string PLAYER_DECK_FILENAME =
    cfg->get_string("PLAYER_DECK_FILENAME", "player.json");

  const std::string OPPONENT_DECK_FILENAME =
    cfg->get_string("OPPONENT_DECK_FILENAME", "opponent.json");

  const std::string EXTRA_CARD_TEXTURE_DIR =
    cfg->get_string("EXTRA_CARD_TEXTURE_DIR", "Cards");

  // NOTE: Base file name and extensions
  paths["CARDS_DB_FILENAME"] = CARDS_DB_FILENAME;
  paths["SAVE_GAME_FILENAME"] = SAVE_GAME_FILENAME;
  paths["PLAYER_DECK_FILENAME"] = PLAYER_DECK_FILENAME;
  paths["OPPONENT_DECK_FILENAME"] = OPPONENT_DECK_FILENAME;

  // IMPORTANT: The following paths are expected to resolve to valid, absolute
  // file paths, with a trailing platform-dependent path delimiter.

  // Resources
  paths["DATA_DIR"] = nom::file_root();

  // in-game screen-shots storage
  paths["SAVE_SCREENSHOT_DIR"] =
    paths["CFG_DIR"] + SAVE_SCREENSHOT_DIR + p.native();

  paths["DEBUG_DATA_DIR"] = paths["DATA_DIR"] + DEBUG_DATA_DIR + p.native();
  paths["SAVE_GAME_DIR"] =
    paths["CFG_DIR"] + SAVE_GAME_DIR + p.native();
  paths["SAVE_DECK_DIR"] =
    paths["SAVE_GAME_DIR"] + SAVE_GAME_DECK_DIR + p.native();
  paths["EXTRA_CARD_TEXTURE_DIR"] = paths["CFG_DIR"] + EXTRA_CARD_TEXTURE_DIR +
    p.native();

  paths["CARDS_DB_PATH"] = paths["DATA_DIR"] + CARDS_DB_FILENAME;
  paths["SAVE_GAME_PATH"] = paths["SAVE_GAME_DIR"] + SAVE_GAME_FILENAME;
  paths["PLAYER_DECK_PATH"] = paths["SAVE_DECK_DIR"] + PLAYER_DECK_FILENAME;
  paths["OPPONENT_DECK_PATH"] = paths["SAVE_DECK_DIR"] + OPPONENT_DECK_FILENAME;

  // TODO: Add a command line switch to access a dump of these paths?
  std::string key;
  std::string path;
  for( auto itr = paths.begin(); itr != paths.end(); ++itr ) {
    key = itr->first;
    path = itr->second;

    NOM_LOG_DEBUG(  TTCARDS_LOG_CATEGORY_CFG,
                    "path key:", key, ":", path );
  }

  return true;
}

void Game::pause_music()
{
  this->game->theme_track_->togglePause();
  this->game->winning_track->togglePause();
}

void Game::mute_volume()
{
  auto current_volume = this->game->listener_->volume();

  if( current_volume >= 100.0f ) {
    this->game->set_volume(0.0f);
  } else if( current_volume <= 0.0f ) {
    this->game->set_volume(100.0f);
  }
}

void Game::set_volume(nom::real32 gain)
{
  this->game->listener_->set_volume(gain);

  NOM_LOG_INFO( TTCARDS_LOG_CATEGORY_APPLICATION, "volume:",
                this->game->listener_->volume() );
}

void Game::save_screenshot()
{
  nom::Path p;
  const std::string filename =
    this->game->paths_["SAVE_SCREENSHOT_DIR"] + "Screenshot.png";

  if( this->game->window.save_screenshot(filename) == false ) {
    NOM_LOG_ERR(  TTCARDS_LOG_CATEGORY_APPLICATION,
                  "Could not dump the screen shot to:", filename );
  }
}

void Game::reload_config()
{
  auto fp = nom::make_unique_json_deserializer();
  if( fp == nullptr ) {
    NOM_LOG_ERR(  TTCARDS,
                  "Could not load input file: failure to allocate memory!" );
    return;
  }

  if( this->game->config_->load_file( this->game->paths_["ROOT_CONFIG_PATH"],
      fp.get() ) == false )
  {
    NOM_LOG_ERR(  TTCARDS_LOG_CATEGORY_APPLICATION,
                  "Could not reload configuration file:",
                  this->game->paths_["ROOT_CONFIG_PATH"] );
    return;
  }

  NOM_ASSERT(this->state() != nullptr);

  this->game->set_state(Game::State::MainMenu);

  this->game->debug_game_ =
    this->game->config_->get_bool("DEBUG_GAME", false);
}

void Game::on_window_shown(const nom::Event& evt)
{
  NOM_LOG_TRACE_PRIO(TTCARDS_LOG_CATEGORY_EVENTS, NOM_LOG_PRIORITY_DEBUG);

  // NOTE: Update simulation at X frames per second; this uses however many CPU
  // cycles necessary to sustain the frame rate.
  this->set_show_fps(true);
  this->fps_timer_.start();
}

void Game::on_window_hidden(const nom::Event& evt)
{
  NOM_LOG_TRACE_PRIO(TTCARDS_LOG_CATEGORY_EVENTS, NOM_LOG_PRIORITY_DEBUG);

  // NOTE: Throttle simulation speed; use less CPU cycles for update
  this->set_show_fps(false);
  this->fps_timer_.stop();
}

void Game::on_window_minimized(const nom::Event& evt)
{
  NOM_LOG_TRACE_PRIO(TTCARDS_LOG_CATEGORY_EVENTS, NOM_LOG_PRIORITY_DEBUG);

  // NOTE: Throttle simulation speed; use less CPU cycles for update
  this->set_show_fps(false);
  this->fps_timer_.stop();
}

void Game::on_window_restored(const nom::Event& evt)
{
  NOM_LOG_TRACE_PRIO(TTCARDS_LOG_CATEGORY_EVENTS, NOM_LOG_PRIORITY_DEBUG);

  // NOTE: Update simulation at X frames per second; this uses however many CPU
  // cycles necessary to sustain the frame rate.
  this->set_show_fps(true);
  this->fps_timer_.start();
}

void Game::on_window_keyboard_focus(const nom::Event& evt)
{
  NOM_LOG_TRACE_PRIO(TTCARDS_LOG_CATEGORY_EVENTS, NOM_LOG_PRIORITY_DEBUG);

  // NOTE: Update simulation at X frames per second; this uses however many CPU
  // cycles necessary to sustain the frame rate.
  this->set_show_fps(true);
  if( this->fps_timer_.started() == false ) {
    this->fps_timer_.start();
  }
}

void Game::on_window_keyboard_focus_lost(const nom::Event& evt)
{
  NOM_LOG_TRACE_PRIO(TTCARDS_LOG_CATEGORY_EVENTS, NOM_LOG_PRIORITY_DEBUG);

  // NOTE: Throttle simulation speed; use less CPU cycles for update
  this->set_show_fps(false);
  if( this->fps_timer_.started() == true ) {
    this->fps_timer_.stop();
  }
}

void Game::on_window_mouse_focus(const nom::Event& evt)
{
  NOM_LOG_TRACE_PRIO(TTCARDS_LOG_CATEGORY_EVENTS, NOM_LOG_PRIORITY_DEBUG);

  // NOTE: Update simulation at X frames per second; this uses however many CPU
  // cycles necessary to sustain the frame rate.
  this->set_show_fps(true);
  if( this->fps_timer_.started() == false ) {
    this->fps_timer_.start();
  }
}

void Game::on_window_mouse_focus_lost(const nom::Event& evt)
{
  NOM_LOG_TRACE_PRIO(TTCARDS_LOG_CATEGORY_EVENTS, NOM_LOG_PRIORITY_DEBUG);

  // NOTE: Throttle simulation speed; use less CPU cycles for update
  this->set_show_fps(false);
  if( this->fps_timer_.started() == true ) {
    this->fps_timer_.stop();
  }
}

} // namespace tt
