cmake_minimum_required(VERSION 3.20)
project(pxa_openlara C CXX)
list(APPEND CMAKE_MODULE_PATH "${PXA_CMAKE_MODULE_DIR}")
include(PxaGuest)
# Every target shares a small icon/notice-only asset directory.
file(GLOB media_asset_dirs "${CMAKE_CURRENT_SOURCE_DIR}/assets*/game")
if(media_asset_dirs)
    message(FATAL_ERROR "OpenLara media must be outside package assets. Use .game-data/openlara/files.")
endif()
set(OL_CLASSIC 1)
set(OL_OPTIMIZED 1)
set(OL_AUDIO_IDLE_SKIP 1)
set(OL_RENDER_PROFILE 0)
set(OL_SHADED_PALETTE_CACHE 0)
foreach(definition IN LISTS PXA_APP_DEFINITIONS)
    if(definition MATCHES "^OL_SHADED_PALETTE_CACHE=([01])$")
        set(OL_SHADED_PALETTE_CACHE "${CMAKE_MATCH_1}")
    endif()
    if(definition MATCHES "^OL_CLASSIC=([01])$")
        set(OL_CLASSIC "${CMAKE_MATCH_1}")
    endif()
    if(definition MATCHES "^OL_OPTIMIZED=([01])$")
        set(OL_OPTIMIZED "${CMAKE_MATCH_1}")
    endif()
    if(definition MATCHES "^OL_AUDIO_IDLE_SKIP=([01])$")
        set(OL_AUDIO_IDLE_SKIP "${CMAKE_MATCH_1}")
    endif()
    if(definition MATCHES "^OL_RENDER_PROFILE=([01])$")
        set(OL_RENDER_PROFILE "${CMAKE_MATCH_1}")
    endif()
endforeach()
if(OL_CLASSIC)
    pxa_add_component(pxa_main COMPONENT_ID main NO_TRANSLATIONS
        SOURCES main.cpp classic-engine.cpp classic-raster.cpp level-catalog.cpp storage.cpp audio.cpp music-stream.cpp title-pcx.cpp
            vendor/classic/libs/tinf/tinflate.c
        INCLUDE_DIRS . vendor/classic DEFINITIONS __PXA_CLASSIC__=1 OL_CLASSIC_MEMORY=1)
    # This translation unit also contains geometry, clipping and scanline setup.
    # Optimize those paths without changing floating-point arithmetic rules.
    set_source_files_properties(classic-engine.cpp PROPERTIES COMPILE_OPTIONS "-O2;-std=c++11")
    set_source_files_properties(classic-raster.cpp PROPERTIES COMPILE_OPTIONS "-O3")
    target_compile_options(pxa_main PRIVATE -Wno-invalid-source-encoding)
else()
    pxa_add_component(pxa_main COMPONENT_ID main NO_TRANSLATIONS NO_ALLOCATOR
        SOURCES main.cpp engine.cpp storage.cpp audio.cpp music-stream.cpp vendor/sound.cpp vendor/fixed/common.cpp vendor/renderer.cpp
        INCLUDE_DIRS . vendor vendor/fixed DEFINITIONS __PXA__=1)
endif()
target_compile_options(pxa_main PRIVATE $<$<COMPILE_LANGUAGE:CXX>:-std=c++2c;-fno-exceptions;-fno-rtti>
    -fno-strict-aliasing -fwrapv -Wno-narrowing)
target_link_options(pxa_main PRIVATE -Wl,-z,stack-size=65536)
target_compile_definitions(pxa_main PRIVATE OL_CLASSIC=${OL_CLASSIC} OL_OPTIMIZED=${OL_OPTIMIZED} OL_AUDIO_IDLE_SKIP=${OL_AUDIO_IDLE_SKIP} OL_RENDER_PROFILE=${OL_RENDER_PROFILE} OL_SHADED_PALETTE_CACHE=${OL_SHADED_PALETTE_CACHE})
if(OL_OPTIMIZED AND NOT OL_CLASSIC)
    # Keep the large virtual gameplay dispatch out of the instruction cache
    # working set; the transform and scanline translation units retain O3.
    set_source_files_properties(engine.cpp PROPERTIES COMPILE_OPTIONS -Os)
endif()
