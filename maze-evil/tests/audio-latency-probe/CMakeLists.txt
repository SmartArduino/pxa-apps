cmake_minimum_required(VERSION 3.20)
project(pxa_audio_latency_probe CXX)

list(APPEND CMAKE_MODULE_PATH "${PXA_CMAKE_MODULE_DIR}")
include(PxaGuest)

pxa_add_app(pxa_main COMPONENT_ID main SOURCES main.cpp)
