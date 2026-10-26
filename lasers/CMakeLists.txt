cmake_minimum_required(VERSION 3.20)
project(pxa_lasers LANGUAGES C CXX)
set(ARDU_GAME lasers)
set(ARDU_OWNER_EXTRA_SOURCES
 "${CMAKE_CURRENT_SOURCE_DIR}/owner/owner_compat.cpp"
 "${CMAKE_CURRENT_SOURCE_DIR}/vendor/platform/ArduboyPlaytune.cpp"
 "${CMAKE_CURRENT_SOURCE_DIR}/vendor/platform/MyArduboy.cpp"
 "${CMAKE_CURRENT_SOURCE_DIR}/vendor/platform/common.cpp"
 "${CMAKE_CURRENT_SOURCE_DIR}/vendor/platform/menu.cpp"
 "${CMAKE_CURRENT_SOURCE_DIR}/vendor/platform/game.cpp"
 "${CMAKE_CURRENT_SOURCE_DIR}/vendor/platform/title.cpp"
 "${CMAKE_CURRENT_SOURCE_DIR}/vendor/platform/logo.cpp"
 "${CMAKE_CURRENT_SOURCE_DIR}/vendor/platform/warn.cpp"
)
set(ARDU_OWNER_INCLUDE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/owner" "${CMAKE_CURRENT_SOURCE_DIR}/vendor/platform")
set(ARDU_OWNER_COMPILE_DEFINITIONS USE_ARDUBOY2_LIB)
include(${CMAKE_CURRENT_LIST_DIR}/game.cmake)
