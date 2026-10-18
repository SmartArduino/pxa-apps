cmake_minimum_required(VERSION 3.20)
project(pxa_duke3d_sd C CXX)
add_subdirectory(../duke3d duke-engine)
target_compile_definitions(pxa_main PRIVATE PORT_EXTERNAL_GAME_DATA=1
    PORT_VFLASH_CACHE_PAGES=48 PORT_VFLASH_COOPERATE=1 PORT_VFLASH_PAGED_NVM=1)
