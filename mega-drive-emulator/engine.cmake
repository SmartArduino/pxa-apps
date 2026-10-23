set(EMU_SYSTEM md)
set(EMU_PICO "${CMAKE_CURRENT_SOURCE_DIR}/vendor/picodrive")
set(EMU_ENGINE_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/core.c" "${CMAKE_CURRENT_SOURCE_DIR}/rom_pager.c" "${CMAKE_CURRENT_SOURCE_DIR}/platform.c"
    "${EMU_PICO}/pico/pico.c"
    "${EMU_PICO}/pico/cart.c"
    "${EMU_PICO}/pico/memory.c"
    "${EMU_PICO}/pico/sek.c"
    "${EMU_PICO}/pico/z80if.c"
    "${EMU_PICO}/pico/videoport.c"
    "${EMU_PICO}/pico/draw2.c"
    "${EMU_PICO}/pico/draw.c"
    "${EMU_PICO}/pico/mode4.c"
    "${EMU_PICO}/pico/misc.c"
    "${EMU_PICO}/pico/state.c"
    "${EMU_PICO}/pico/eeprom.c"
    "${EMU_PICO}/pico/cd/mcd.c"
    "${EMU_PICO}/pico/cd/memory.c"
    "${EMU_PICO}/pico/cd/sek.c"
    "${EMU_PICO}/pico/cd/cdc.c"
    "${EMU_PICO}/pico/cd/cdd.c"
    "${EMU_PICO}/pico/cd/gfx.c"
    "${EMU_PICO}/pico/cd/gfx_dma.c"
    "${EMU_PICO}/pico/cd/misc.c"
    "${EMU_PICO}/pico/cd/pcm.c"
    "${EMU_PICO}/pico/cd/megasd.c"
    "${EMU_PICO}/pico/pico/pico.c"
    "${EMU_PICO}/pico/pico/memory.c"
    "${EMU_PICO}/pico/pico/xpcm.c"
    "${EMU_PICO}/pico/carthw/carthw.c"
    "${EMU_PICO}/pico/carthw/eeprom_spi.c"
    "${EMU_PICO}/pico/carthw/svp/svp.c"
    "${EMU_PICO}/pico/carthw/svp/memory.c"
    "${EMU_PICO}/pico/carthw/svp/ssp16.c"
    "${EMU_PICO}/pico/sound/sound.c"
    "${EMU_PICO}/pico/sound/resampler.c"
    "${EMU_PICO}/pico/sound/sn76496.c"
    "${EMU_PICO}/pico/sound/ym2612.c"
    "${EMU_PICO}/pico/sound/ym2413.c"
    "${EMU_PICO}/pico/sound/mix.c"
    "${EMU_PICO}/cpu/fame/famec.c"
    "${EMU_PICO}/cpu/cz80/cz80.c"
    "${EMU_PICO}/zlib/crc32.c"
)
set(EMU_ENGINE_INCLUDES "${EMU_PICO}" "${EMU_PICO}/zlib")
set(EMU_ENGINE_DEFINITIONS EMU_F68K _USE_CZ80 NO_32X NO_SMS PICO_NO_DYNAREC)
option(EMU_MD_PROFILE "Sample inclusive PicoDrive phase costs (diagnostics only)" OFF)
if(EMU_MD_PROFILE)
    list(APPEND EMU_ENGINE_DEFINITIONS PXA_MD_PROFILE)
    list(APPEND EMU_ENGINE_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/md_profile.c")
endif()

# PicoDrive models the MD DAC range rather than stretching it to full white.
set(EMU_TEST_BLUE 0x001d)

set_source_files_properties("${EMU_PICO}/pico/cart.c" "${EMU_PICO}/pico/cd/cdd.c" PROPERTIES COMPILE_DEFINITIONS PICO_MEMORY_CARTRIDGE)

# FAME generates thousands of opcode handlers. O0 keeps sanitizer compilation
# bounded; production and ordinary throughput builds retain O3.
if(EMU_NATIVE_TEST AND CMAKE_C_FLAGS MATCHES "-fsanitize=")
    set_source_files_properties("${EMU_PICO}/cpu/fame/famec.c" PROPERTIES COMPILE_OPTIONS "-O0")
endif()

set_source_files_properties("${EMU_PICO}/pico/state.c" PROPERTIES COMPILE_DEFINITIONS PICO_MEMORY_STATE)

# The state serializer must copy hot globals by typed fields, including fields
# added upstream. Generate the schema from the pinned vendor declarations.
find_package(Python3 REQUIRED COMPONENTS Interpreter)
set(EMU_STATE_FIELDS "${CMAKE_CURRENT_BINARY_DIR}/pxa_md_state_fields.h")
add_custom_command(OUTPUT "${EMU_STATE_FIELDS}"
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/generate_state_fields.py"
        "${EMU_PICO}/pico/pico_int.h" "${EMU_STATE_FIELDS}"
    DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/tests/generate_state_fields.py" "${EMU_PICO}/pico/pico_int.h")
list(APPEND EMU_ENGINE_SOURCES "${EMU_STATE_FIELDS}")
set_source_files_properties("${CMAKE_CURRENT_SOURCE_DIR}/core.c" "${EMU_PICO}/pico/state.c"
    PROPERTIES INCLUDE_DIRECTORIES "${CMAKE_CURRENT_BINARY_DIR}")
