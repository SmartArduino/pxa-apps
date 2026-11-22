cmake_minimum_required(VERSION 3.20)
project(windstone_town CXX)
list(APPEND CMAKE_MODULE_PATH "${PXA_CMAKE_MODULE_DIR}")
include(PxaGuest)
find_package(Python3 REQUIRED COMPONENTS Interpreter)
set(asset_source "${CMAKE_CURRENT_BINARY_DIR}/pixel_assets.cpp")
add_custom_command(
    OUTPUT "${asset_source}"
    COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/tools/embed.py"
            "${asset_source}" "${CMAKE_CURRENT_SOURCE_DIR}/art"
    DEPENDS tools/embed.py tools/palette.py art/title.pxi art/sprites.pxi art/font.pxi art/terrain.pxi art/plants.pxi
            art/actors.pxi art/monsters.pxi art/ui.pxi art/icons.pxi art/effects.pxi art/relics.pxi art/caves.pxi
            art/walls.pxi art/stairs.pxi art/backdrop.pxi art/tools.pxi art/markers.pxi art/waterfall.pxi art/glints.pxi
            art/expedition.pxi art/folio.pxi art/walking.pxi art/combat.pxi art/items.pxi
            art/combat_body.pxi art/held_weapons.pxi art/worn_gear.pxi
    VERBATIM
)
pxa_add_app(pxa_main COMPONENT_ID main SOURCES main.cpp game.cpp "${asset_source}")
