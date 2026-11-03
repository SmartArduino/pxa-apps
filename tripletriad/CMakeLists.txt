cmake_minimum_required(VERSION 3.20)
project(pxa_tripletriad C CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
get_filename_component(ROOT "${CMAKE_CURRENT_LIST_DIR}" ABSOLUTE)
include("${CMAKE_CURRENT_LIST_DIR}/upstream-core.cmake")
if(TRIAD_NATIVE_TESTS)
 add_executable(triad_test native-test.cpp match.cpp ui.cpp "${ROOT}/assets.cpp" ${TRIAD_CORE_SOURCES})
 target_include_directories(triad_test PRIVATE "${CMAKE_CURRENT_LIST_DIR}" "${CMAKE_CURRENT_LIST_DIR}/compat" "${ROOT}/engine")
 target_compile_options(triad_test PRIVATE -Wall -Wextra)
else()
 list(APPEND CMAKE_MODULE_PATH "${PXA_CMAKE_MODULE_DIR}")
 # This C++17 engine uses the public C Guest transport. It does not link the
 # optional C++26 SDK, whose feature check otherwise inherits engine C++17 mode.
 set(PXA_CPP_SDK_DIR "")
 include(PxaGuest)
 pxa_add_component(pxa_main COMPONENT_ID main
  SOURCES frontend.cpp match.cpp ui.cpp "${ROOT}/assets.cpp" ${TRIAD_CORE_SOURCES}
  INCLUDE_DIRS "${CMAKE_CURRENT_LIST_DIR}" "${CMAKE_CURRENT_LIST_DIR}/compat" "${ROOT}/engine" "${ROOT}/../common/retro-games")
 target_compile_options(pxa_main PRIVATE "$<$<COMPILE_LANGUAGE:CXX>:-fno-exceptions;-fno-rtti>")
 # With no WASI imports, WAMR calls exported __wasm_call_ctors rather than
 # _initialize. Exporting malloc/free also makes Host payload allocation use
 # the same libc allocator as the C++ engine instead of inserting a second heap.
 target_link_options(pxa_main PRIVATE -Wl,-z,stack-size=524288
  -Wl,--export=__wasm_call_ctors -Wl,--export=malloc -Wl,--export=free
  -Wl,--wrap=__assert_fail -Wl,--wrap=abort -Wl,--wrap=exit
  -Wl,--wrap=__abort_message -Wl,--wrap=__cxa_pure_virtual -Wl,--wrap=__cxa_deleted_virtual
  -Wl,--wrap=_ZNSt3__222__libcpp_verbose_abortEPKcz)
endif()
