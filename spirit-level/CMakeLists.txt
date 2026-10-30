cmake_minimum_required(VERSION 3.20)
project(pxa_spirit_level LANGUAGES C CXX)
include("${CMAKE_CURRENT_LIST_DIR}/../common/cpp/Application.cmake")
pxa_add_callback_component(pxa_main ALLOCATOR COMPONENT_ID main SOURCES
    main.cpp model.cpp services.cpp ../common/pocket-play/runtime.cpp)
target_link_options(pxa_main PRIVATE -Wl,-z,stack-size=65536)
target_compile_definitions(pxa_main PRIVATE PXA_POCKET_STUDIO=1 PXA_POCKET_FILL=1 PXA_POCKET_RESPONSIVE=1 PXA_POCKET_SERVICES=1 PXA_POCKET_UTILITY=1 PXA_POCKET_SILENT=1)
