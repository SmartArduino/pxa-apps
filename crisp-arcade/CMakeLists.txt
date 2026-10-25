cmake_minimum_required(VERSION 3.20)
project(pxa_crisp_arcade LANGUAGES C CXX)
include("${CMAKE_CURRENT_LIST_DIR}/../common/cpp/Application.cmake")
set(CRISP_LIB vendor/src/lib)
pxa_add_callback_component(pxa_main COMPONENT_ID main SOURCES
    main.cpp backend.cpp core_bridge.c registration.c
    ${CRISP_LIB}/menu.c ${CRISP_LIB}/particle.c ${CRISP_LIB}/random.c
    ${CRISP_LIB}/sound.c ${CRISP_LIB}/textPattern.c ${CRISP_LIB}/vector.c
    vendor/src/games/gameCastN.c vendor/src/games/gameGrapplingH.c
    vendor/src/games/gameReflector.c vendor/src/games/gameColorRoll.c
    ../common/pocket-play/runtime.cpp)
target_include_directories(pxa_main PRIVATE ${CMAKE_CURRENT_LIST_DIR}/${CRISP_LIB})
# Upstream deliberately hashes into signed 32-bit integers using wraparound.
target_compile_options(pxa_main PRIVATE -fwrapv)
target_link_options(pxa_main PRIVATE -Wl,-z,stack-size=65536)
