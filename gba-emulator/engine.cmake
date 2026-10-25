set(EMU_SYSTEM gba)
set(GPSP "${CMAKE_CURRENT_SOURCE_DIR}/vendor/gpsp")
set(EMU_ENGINE_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/core.c"
    "${GPSP}/cpu.cc" "${GPSP}/video.cc" "${GPSP}/main.c"
    "${GPSP}/gba_memory.c" "${GPSP}/savestate.c" "${GPSP}/input.c"
    "${GPSP}/sound.c" "${GPSP}/cheats.c" "${GPSP}/gbp.c"
    "${GPSP}/serial.c" "${GPSP}/rfu.c" "${GPSP}/serial_proto.c")
set(EMU_ENGINE_INCLUDES "${CMAKE_CURRENT_SOURCE_DIR}/port" "${GPSP}")
set(EMU_ENGINE_DEFINITIONS INLINE=inline ROM_BUFFER_SIZE=2 PXA_GPSP_BOUNDED_CACHE=1)
# Read on every configure so the shared build cache cannot retain profiling.
if(EMU_NATIVE_TEST)
    list(APPEND EMU_ENGINE_DEFINITIONS PXA_GBA_IDLE_TEST=1)
    option(GBA_PROFILE "Measure gpSP phases and instruction regions" OFF)
    option(GBA_PROFILE_PHASES "Measure phases without changing CPU execution paths" OFF)
    option(GBA_BENCHMARK "Fixed-input, full-render frame-block measurements" OFF)
else()
    set(GBA_PROFILE "$ENV{PXA_GBA_PROFILE}")
    set(GBA_PROFILE_PHASES "$ENV{PXA_GBA_PROFILE_PHASES}")
    set(GBA_BENCHMARK "$ENV{PXA_GBA_BENCHMARK}")
endif()
if(GBA_BENCHMARK)
    list(APPEND EMU_ENGINE_DEFINITIONS PXA_GBA_BENCHMARK=1)
endif()
if(GBA_PROFILE)
    list(APPEND EMU_ENGINE_DEFINITIONS PXA_GBA_PROFILE=1)
endif()
if(GBA_PROFILE OR GBA_PROFILE_PHASES)
    list(APPEND EMU_ENGINE_DEFINITIONS PXA_EMU_PROFILE_PHASES=1)
    if(EMU_NATIVE_TEST)
        list(APPEND EMU_ENGINE_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/../common/emulator/emu_profile.c")
    endif()
endif()
find_package(Python3 REQUIRED COMPONENTS Interpreter)
set(GBA_BENCHMARK_INPUT "$ENV{PXA_GBA_BENCHMARK_INPUT}")
if(GBA_BENCHMARK AND GBA_BENCHMARK_INPUT)
    execute_process(COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tools/compile_bench_input.py"
        "${GBA_BENCHMARK_INPUT}" "${CMAKE_CURRENT_BINARY_DIR}/gba_bench_input.h"
        COMMAND_ERROR_IS_FATAL ANY)
    list(APPEND EMU_ENGINE_INCLUDES "${CMAKE_CURRENT_BINARY_DIR}")
    list(APPEND EMU_ENGINE_DEFINITIONS PXA_GBA_BENCHMARK_INPUT=1)
endif()
set(GPSP_BIOS_C "${CMAKE_CURRENT_BINARY_DIR}/open_gba_bios.c")
add_custom_command(OUTPUT "${GPSP_BIOS_C}"
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tools/embed_bios.py"
        "${GPSP}/bios/open_gba_bios.bin" "${GPSP_BIOS_C}"
    DEPENDS "${GPSP}/bios/open_gba_bios.bin" "${CMAKE_CURRENT_SOURCE_DIR}/tools/embed_bios.py")
list(APPEND EMU_ENGINE_SOURCES "${GPSP_BIOS_C}")
