cmake_minimum_required(VERSION 3.20)
project(tide_pact CXX)

list(APPEND CMAKE_MODULE_PATH "${PXA_CMAKE_MODULE_DIR}")
include(PxaGuest)
find_package(Python3 REQUIRED COMPONENTS Interpreter)

set(asset_files)
foreach(asset chart harbor title_compact storm portraits caravel broadside glyphs glyphs_compact wordmark skin controls tabs goods navigation ports boarding cadiz_duel duel_fx discoveries officers voyage_icons driftwood towns town_icons town_walk port_people port_ui dispatch_icons weather_icons sea_stories)
    list(APPEND asset_files "${CMAKE_CURRENT_SOURCE_DIR}/art/${asset}.pxi")
endforeach()
set(asset_source "${CMAKE_CURRENT_BINARY_DIR}/embedded_assets.cpp")
add_custom_command(
    OUTPUT "${asset_source}"
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tools/embed_assets.py"
            "${asset_source}" "${CMAKE_CURRENT_SOURCE_DIR}/art"
    DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/tools/embed_assets.py" ${asset_files}
    VERBATIM
)
pxa_add_app(pxa_main COMPONENT_ID main SOURCES main.cpp game.cpp "${asset_source}")
target_include_directories(pxa_main PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
