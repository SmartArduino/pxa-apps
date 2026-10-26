cmake_minimum_required(VERSION 3.21)
project(pxa_randocity LANGUAGES C CXX)
set(ARDU_GAME randocity)
set(ARDU_OWNER_INCLUDE_DIRS "${CMAKE_CURRENT_LIST_DIR}/vendor/platform")
set(ARDU_OWNER_EXTRA_SOURCES
  "${CMAKE_CURRENT_LIST_DIR}/frontend.cpp"
  "${CMAKE_CURRENT_LIST_DIR}/vendor/platform/FatFont.cpp"
  "${CMAKE_CURRENT_LIST_DIR}/vendor/platform/lfsr.cpp"
  "${CMAKE_CURRENT_LIST_DIR}/vendor/platform/World.cpp"
  "${CMAKE_CURRENT_LIST_DIR}/vendor/platform/Timer.cpp"
  "${CMAKE_CURRENT_LIST_DIR}/vendor/platform/Records.cpp"
  "${CMAKE_CURRENT_LIST_DIR}/vendor/platform/ArduboyTones.cpp")
include("${CMAKE_CURRENT_LIST_DIR}/../common/retro-games/arduboy-batch-20261010/game.cmake")
set_source_files_properties("${CMAKE_CURRENT_LIST_DIR}/../common/retro-games/arduboy-batch-20261010/frontend.cpp" PROPERTIES HEADER_FILE_ONLY TRUE)
