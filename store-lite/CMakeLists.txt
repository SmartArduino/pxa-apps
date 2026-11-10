cmake_minimum_required(VERSION 3.20)
project(pxa_store_lite LANGUAGES C CXX)
set(PXA_COROUTINE_SLOT_BYTES 4096)
list(APPEND CMAKE_MODULE_PATH "${PXA_CMAKE_MODULE_DIR}")
include(PxaGuest)
pxa_add_app(pxa_main COMPONENT_ID main SOURCES main.cpp ../store/store_client.cpp ../store/store_json.cpp ../store/store_account.cpp ../store/store_media_url.cpp)
target_compile_definitions(pxa_main PRIVATE PXA_GUEST_CPP_TRANSPORT=1 STORE_MAX_APPS=6)
target_compile_options(pxa_main PRIVATE -Oz)
# Optimize the app and its private SDK runtime together, so unused protocol
# paths and duplicate helpers are removed before AOT generation.
set_property(TARGET pxa_main pxa_guest_cpp PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
target_compile_options(pxa_guest_cpp PRIVATE -Oz)
target_link_options(pxa_main PRIVATE -Wl,-z,stack-size=65536)
