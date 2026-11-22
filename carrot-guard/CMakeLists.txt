cmake_minimum_required(VERSION 3.20)
project(carrot_guard LANGUAGES CXX)
find_package(Pxa CONFIG REQUIRED)
pxa_add_component(pxa_main CPP NO_TRANSLATIONS COMPONENT_ID main
    SOURCES main.cpp core.cpp pixel_bank.cpp pixel_inflate.cpp)
target_compile_definitions(pxa_main PRIVATE PXA_GUEST_CPP_TRANSPORT=1)
target_link_options(pxa_main PRIVATE -Wl,-z,stack-size=65536)
