cmake_minimum_required(VERSION 3.20)
project(pxa_openlara C CXX)
list(APPEND CMAKE_MODULE_PATH "${PXA_CMAKE_MODULE_DIR}")
include(PxaGuest)
pxa_add_component(pxa_main COMPONENT_ID main NO_TRANSLATIONS NO_ALLOCATOR
    SOURCES main.cpp engine.cpp storage.cpp audio.cpp vendor/sound.cpp vendor/fixed/common.cpp vendor/renderer.cpp
    INCLUDE_DIRS . vendor vendor/fixed DEFINITIONS __PXA__=1)
target_compile_options(pxa_main PRIVATE -std=c++2c -fno-exceptions -fno-rtti
    -fno-strict-aliasing -fwrapv -Wno-narrowing)
target_link_options(pxa_main PRIVATE -Wl,-z,stack-size=65536)
set(OL_OPTIMIZED 1)
set(OL_AUDIO_IDLE_SKIP 1)
foreach(definition IN LISTS PXA_APP_DEFINITIONS)
    if(definition MATCHES "^OL_OPTIMIZED=([01])$")
        set(OL_OPTIMIZED "${CMAKE_MATCH_1}")
    endif()
    if(definition MATCHES "^OL_AUDIO_IDLE_SKIP=([01])$")
        set(OL_AUDIO_IDLE_SKIP "${CMAKE_MATCH_1}")
    endif()
endforeach()
target_compile_definitions(pxa_main PRIVATE OL_OPTIMIZED=${OL_OPTIMIZED} OL_AUDIO_IDLE_SKIP=${OL_AUDIO_IDLE_SKIP})
if(OL_OPTIMIZED)
    # Keep the large virtual gameplay dispatch out of the instruction cache
    # working set; the transform and scanline translation units retain O3.
    set_source_files_properties(engine.cpp PROPERTIES COMPILE_OPTIONS -Os)
endif()
