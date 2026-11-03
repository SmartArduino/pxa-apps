# Project source files

# NOTE: The header files are included here for automated source && header
# file lists when generating IDE project files, i.e.: Xcode or MSVCPP.

set( STATES_DIR "${SRC_DIR}/states" )

# Where to find internal development header files
include_directories( ${SRC_DIR} )

# Where to find external development header files
include_directories( ${NOMLIB_INCLUDE_DIR} )
include_directories( ${NOMLIB_EXTERNAL_INCLUDE_DIRS} )

# Core game logic
set(  TTCARDS_SOURCE
      ${SRC_DIR}/config.hpp
      ${SRC_DIR}/version.hpp
      ${SRC_DIR}/main.cpp
      ${SRC_DIR}/GameConfig.cpp
      ${SRC_DIR}/GameConfig.hpp
      ${SRC_DIR}/Board.cpp
      ${SRC_DIR}/Board.hpp
      ${SRC_DIR}/BoardTile.cpp
      ${SRC_DIR}/BoardTile.hpp
      ${SRC_DIR}/Card.cpp
      ${SRC_DIR}/Card.hpp
      ${SRC_DIR}/CardCollection.cpp
      ${SRC_DIR}/CardCollection.hpp
      ${SRC_DIR}/CardHand.cpp
      ${SRC_DIR}/CardHand.hpp
      ${SRC_DIR}/CardRules.cpp
      ${SRC_DIR}/CardRules.hpp
      ${SRC_DIR}/CardResourceLoader.cpp
      ${SRC_DIR}/CardResourceLoader.hpp
      ${SRC_DIR}/CardRenderer.cpp
      ${SRC_DIR}/CardRenderer.hpp
      ${SRC_DIR}/IPlayer.cpp
      ${SRC_DIR}/IPlayer.hpp
      ${SRC_DIR}/HumanPlayer.cpp
      ${SRC_DIR}/HumanPlayer.hpp
      ${SRC_DIR}/CPU_Player.cpp
      ${SRC_DIR}/CPU_Player.hpp
      ${SRC_DIR}/Game.cpp
      ${SRC_DIR}/Game.hpp
      ${SRC_DIR}/CardsPageDataSource.cpp
      ${SRC_DIR}/CardsPageDataSource.hpp
      ${SRC_DIR}/helpers.cpp
      ${SRC_DIR}/helpers.hpp )

# Game states logic
set(  TTCARDS_SOURCE_STATES
      ${STATES_DIR}/DialogCursor.cpp
      ${STATES_DIR}/DialogCursor.hpp
      ${STATES_DIR}/ConfirmationDialogState.cpp
      ${STATES_DIR}/ConfirmationDialogState.hpp
      ${STATES_DIR}/GameOverStateCursor.cpp
      ${STATES_DIR}/GameOverStateCursor.hpp
      ${STATES_DIR}/GameOverState.cpp
      ${STATES_DIR}/GameOverState.hpp
      ${STATES_DIR}/PauseState.cpp
      ${STATES_DIR}/PauseState.hpp
      ${STATES_DIR}/CardsMenuState.cpp
      ${STATES_DIR}/CardsMenuState.hpp
      ${STATES_DIR}/PlayState.cpp
      ${STATES_DIR}/PlayState.hpp

      ${STATES_DIR}/MainMenuState.cpp
      ${STATES_DIR}/MainMenuState.hpp )

set(  TTCARDS_SOURCE
      ${TTCARDS_SOURCE}
      ${TTCARDS_SOURCE_STATES} )

# Auto-generate a header file for tracking the project software version
# NOTE: These rely on PROJECT_VERSION_MAJOR, PROJECT_VERSION_MINOR and
# PROJECT_VERSION_PATCH being defined.
configure_file( "${CMAKE_TEMPLATE_PATH}/version.hpp.in"
                "${PROJECT_SOURCE_DIR}/${SRC_DIR}/version.hpp" )
