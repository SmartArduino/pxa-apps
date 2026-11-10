cmake_minimum_required(VERSION 3.20)
project(pxa_fat_fish LANGUAGES C CXX)
include("${CMAKE_CURRENT_LIST_DIR}/../common/cpp/Application.cmake")
pxa_add_callback_component(pxa_main ALLOCATOR COMPONENT_ID main
    SOURCES main.cpp chat.cpp voice.cpp ../common/media/decoder.cpp animation.cpp codec.cpp zlib_memory.c
    vendor/zlib/inflate.c vendor/zlib/inffast.c vendor/zlib/inftrees.c
    vendor/zlib/adler32.c vendor/zlib/crc32.c ../common/pocket-play/runtime.cpp)
target_compile_definitions(pxa_main PRIVATE
    Z_SOLO
    PXA_POCKET_STUDIO=1 PXA_POCKET_FILL=1 PXA_POCKET_SERVICES=1
    PXA_POCKET_SILENT=1 PXA_POCKET_CUSTOM_MENU=1
    PXA_POCKET_THEME_TEXT=1 PXA_POCKET_FAST_START=1)
target_link_options(pxa_main PRIVATE -Wl,-z,stack-size=65536)
