cmake_minimum_required(VERSION 3.20)
project(tide_pact CXX)

list(APPEND CMAKE_MODULE_PATH "${PXA_CMAKE_MODULE_DIR}")
include(PxaGuest)
find_package(Python3 REQUIRED COMPONENTS Interpreter)

set(asset_files)
foreach(asset chart relief chart_materials chart_actions journal_icons ship_portraits helm_icons skirmish fishery enclave harbor title_compact storm portraits pilot_ships pilot_marks broadside glyphs glyphs_compact glyphs_native ui_wide wordmark skin controls tabs goods specialty_icons navigation shore_troops boarding cadiz_duel duel_fx discoveries officers voyage_icons driftwood towns town_icons town_walk port_people port_ui dispatch_icons weather_icons outskirts inland calicut levada marsh nature flags shore_tasks
              expedition_estuary expedition_forest expedition_wadi expedition_vardo expedition_malabar expedition_sabaki weather_motion expedition_arguin
              expedition_vardo_thaw expedition_malabar_dry field_animals field_traveler field_marks)
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
pxa_add_app(pxa_main COMPONENT_ID main SOURCES main.cpp game.cpp sea_skirmish.cpp sea_fishery.cpp sea_law.cpp settlement.cpp pilotage.cpp land_battle.cpp "${asset_source}")
target_include_directories(pxa_main PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
