set(EMU_SYSTEM snes)
set(EMU_SNES_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/vendor/snes9x/src")
file(GLOB EMU_SNES_SOURCES CONFIGURE_DEPENDS "${EMU_SNES_ROOT}/*.c")
list(FILTER EMU_SNES_SOURCES EXCLUDE REGEX "/(apu_blargg|snapshot|cpuexec)\\.c$")
set(EMU_ENGINE_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/core.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/rom_pager.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/apu_sync.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/snes_profile.c" ${EMU_SNES_SOURCES})
set(EMU_ENGINE_INCLUDES "${CMAKE_CURRENT_SOURCE_DIR}/vendor/snes9x" "${EMU_SNES_ROOT}")
set(EMU_ENGINE_DEFINITIONS RIGHTSHIFT_IS_SAR NO_ZERO_LUT)
# cpuops.c includes the execution loop so instruction bodies share its AOT
# memory state. Generate enum IDs and switch cases from the same mode tables.
find_package(Python3 REQUIRED COMPONENTS Interpreter)
set(EMU_OPCODE_DISPATCH "${CMAKE_CURRENT_BINARY_DIR}/pxa_snes_opcodes.inc")
add_custom_command(OUTPUT "${EMU_OPCODE_DISPATCH}"
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/generate_opcode_dispatch.py"
        "${EMU_SNES_ROOT}/cpuops.c" "${EMU_OPCODE_DISPATCH}"
    DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/tests/generate_opcode_dispatch.py" "${EMU_SNES_ROOT}/cpuops.c"
    VERBATIM)
list(APPEND EMU_ENGINE_SOURCES "${EMU_OPCODE_DISPATCH}")
list(APPEND EMU_ENGINE_INCLUDES "${CMAKE_CURRENT_BINARY_DIR}")
option(EMU_SNES_EVENT_SYNC "Batch SPC work at shared-state boundaries" ON)
option(EMU_SNES_PROFILE "Instrument SNES phases (diagnostics only, not FPS measurements)" OFF)
if(EMU_SNES_EVENT_SYNC)
    list(APPEND EMU_ENGINE_DEFINITIONS PXA_SNES_EVENT_SYNC)
endif()
if(EMU_SNES_PROFILE)
    list(APPEND EMU_ENGINE_DEFINITIONS PXA_SNES_PROFILE)
endif()
