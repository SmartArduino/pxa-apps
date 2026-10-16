cmake_minimum_required(VERSION 3.20)
project(pxa_pinyin_ime_lite LANGUAGES C CXX)
list(APPEND CMAKE_MODULE_PATH "${PXA_CMAKE_MODULE_DIR}")
include(PxaGuest)
pxa_add_app(pxa_main COMPONENT_ID main SOURCES main.cpp)
# Same editing protocol and T9, a different compile-time lookup engine. The
# full decoder, dictionary I/O and its initial-memory setting are not linked.
pxa_add_component(pxa_provider CPP COMPONENT_ID provider SOURCES provider.cpp
    ../pinyin-ime/core/ime_t9.c ../pinyin-ime/core/ime_trie.c ../pinyin-ime/generated/ime_t9_syllables.c)
target_include_directories(pxa_provider PRIVATE . ../pinyin-ime)
