# Independent extension foundation. The delivered 31d puzzle.cmake is unchanged.
list(APPEND CMAKE_MODULE_PATH "${PXA_CMAKE_MODULE_DIR}")
include("${CMAKE_CURRENT_LIST_DIR}/../Build.cmake")
include_directories("${CMAKE_CURRENT_LIST_DIR}/..")
set(PZ_ROOT "${CMAKE_CURRENT_LIST_DIR}")
if(NOT PXA_PUZZLE MATCHES "^(cube|loopy)$")
  message(FATAL_ERROR "Unknown extension puzzle backend")
endif()
set(PZ_CORE midend drawing misc malloc random tree234 dsf findloop sort combi version)
if(PXA_PUZZLE STREQUAL "loopy")
  list(APPEND PZ_CORE grid loopgen penrose penrose-legacy hat spectre)
endif()
set(PZ_SOURCES)
foreach(PZ_UNIT IN LISTS PZ_CORE)
  list(APPEND PZ_SOURCES "${PZ_ROOT}/vendor/${PZ_UNIT}.c")
endforeach()
pxa_retro_component(pxa_main COMPONENT_ID main
  SOURCES "${PZ_ROOT}/frontend.c" "${PZ_ROOT}/compat.c" "${PZ_ROOT}/vendor/${PXA_PUZZLE}.c" ${PZ_SOURCES}
  INCLUDE_DIRS "${PZ_ROOT}" "${PZ_ROOT}/vendor")
target_compile_definitions(pxa_main PRIVATE NO_TGMATH_H)
target_compile_options(pxa_main PRIVATE -ffunction-sections -fdata-sections)
target_link_options(pxa_main PRIVATE -Wl,--gc-sections -Wl,-z,stack-size=524288
  -Wl,--export=malloc -Wl,--export=free -Wl,--wrap=getenv -Wl,--wrap=__assert_fail -Wl,--wrap=abort -Wl,--wrap=exit
  -Wl,--wrap=__wasi_fd_close -Wl,--wrap=__wasi_fd_seek -Wl,--wrap=__wasi_fd_write)
