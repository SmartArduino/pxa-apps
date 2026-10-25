cmake_minimum_required(VERSION 3.20)
project(pxa_store CXX)
include(../common/cpp/Application.cmake)
pxa_add_callback_component(pxa_main ALLOCATOR COMPONENT_ID main
    SOURCES main.cpp store_account.cpp store_client.cpp store_json.cpp search_image.cpp store_media.cpp store_media_url.cpp)
target_compile_definitions(pxa_main PRIVATE PXA_COROUTINE_SLOT_COUNT=6
    PXA_COROUTINE_SLOT_BYTES=1024 PXA_REQUEST_CAPACITY=8)
