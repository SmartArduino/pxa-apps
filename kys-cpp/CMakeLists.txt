cmake_minimum_required(VERSION 3.20)
project(pxa_kys_cpp C CXX ASM)
set(PXA_RECROF_ENGINE_STACK_BYTES 262144)
set(PXA_RECROF_CALLBACK_STACK_BYTES 32768)
include(../common/sdl-games/Build.cmake)
file(GLOB game_sources CONFIGURE_DEPENDS vendor/src/*.cpp)
list(FILTER game_sources EXCLUDE REGEX "/(Audio|RunNodeFromJson|kys)\\.cpp$")
pxa_sdl_component(TITLE "kys-cpp" SOURCES ${game_sources}
  main.cpp backend_checks.cpp audio.cpp sqlite_vfs.c vendor/sqlite/sqlite3.c
  vendor/cifa/Cifa.cpp
  vendor/mlcc/strfunc.cpp vendor/mlcc/filefunc.cpp vendor/mlcc/PotConv.cpp
  vendor/mlcc/SimpleCC.cpp vendor/mlcc/SQLite3Wrapper.cpp vendor/mlcc/ZipFile2.cpp
  vendor/others/Hanz2Piny.cpp font.c
  INCLUDES compat vendor/src vendor/mlcc vendor/cifa vendor/others vendor/sqlite
  DEFINITIONS PORT_EXTERNAL_GAME_DATA=1 PORT_SDL_FPS=30
    SQLITE_OS_OTHER=1 SQLITE_THREADSAFE=0 SQLITE_OMIT_LOAD_EXTENSION=1
    SQLITE_TEMP_STORE=3 SQLITE_DEFAULT_CACHE_SIZE=-128 SQLITE_DEFAULT_MEMSTATUS=0
    SQLITE_OMIT_WAL=1 SQLITE_OMIT_DEPRECATED=1)
# SDL2's bundled permissive MP3 decoder, linked only into this app guest.
target_sources(SDL2_mixer PRIVATE "${PXA_SDL_DIR}/vendor/sdl-mixer/src/codecs/music_minimp3.c")
target_compile_definitions(SDL2_mixer PRIVATE MUSIC_MP3_MINIMP3=1)
target_compile_options(pxa_main PRIVATE "$<$<COMPILE_LANGUAGE:CXX>:-Wno-deprecated-declarations;-frtti>")

target_compile_features(pxa_main PRIVATE cxx_std_26)

# Keep the complete C++ engine within embedded code memory.
target_compile_options(pxa_main PRIVATE -Os)

